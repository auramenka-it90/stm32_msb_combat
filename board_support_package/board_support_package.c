/**
 ******************************************************************************
 * @file    board_support_package.c
 * @brief   Hardware Initialization, FPGA Bring-up, and Diagnostic Functions.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "board_support_package.h"
#include "pin_mgmt.h"
#include "stm32_2_fpga_spi_bridge.h"
#include "fpga_control.h"
#include "terminal.h"
#include "fcs.h"
#include "host.h"
#include "configuration.h"

/* Global diagnostic status register */
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;

/* Global FPGA SPI Bridge Handle */
FPGA_HandleTypeDef hfpga_bridge;

/* STM32 Pin Management Configuration */
Pin_Mgmt_Config_t pin = {
    .debug_enabled = 1
};

/* ========================================================================= */
/*       ADC INTERNAL TEMPERATURE SENSOR DEFINITIONS & CONSTANTS             */
/* ========================================================================= */
#define ADC_MAX_VALUE_12BIT      4095.0f

/* Параметры датчика из Datasheet STM32F411 */
#define TEMP_SENSOR_V25          0.76f     /* Напряжение датчика при 25 °C (В) */
#define TEMP_SENSOR_AVG_SLOPE    0.0025f   /* Чувствительность: 2.5 мВ / °C (В/°C) */
#define TEMP_SENSOR_REF_TEMP     25.0f     /* Опорная температура для V25 (°C) */

#define TEMP_ERROR_VALUE         (-999.0f) /* Значение при аппаратной ошибке АЦП */
#define TEMP_FILTER_MAX_SAMPLES  32U       /* Максимальный размер буфера усреднения */

/* Адрес калибровки VREFINT (снято на заводе при 3.3 В, 30 °C) */
#ifndef VREFINT_CAL_ADDR
#define VREFINT_CAL_ADDR         ((uint16_t*)0x1FFF7A2A)
#endif
#define VREFINT_CAL_VOLTAGE      3.3f      /* Опорное напряжение заводской калибровки */

/* Глобальные переменные (для совместимости с твоим кодом) */
float adc_voltage = 0.0f;
float cpu_temperature = 0.0f;
float real_vref= 0.0f;

/* ========================================================================= */
/*  SYSTEM HARDWARE INITIALIZATION & POST                                    */
/* ========================================================================= */

// Complete hardware bring-up sequence in strict order
uint32_t	init_hardware(void){
	test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;

	// 1. Read non-volatile device configuration from Flash
	if(!get_dev_cfg()){
		test_hardware_result |= _B_FAULT_CFG_;
	}

	// 2. Initialize GPIO subsystem and microsecond DWT timer
	if(PIN_MGMT_Init(&pin) != osOK){
		test_hardware_result |= _B_FAULT_PINS_;
	} else {
		DWT_Init();

		// 3. Release SPI bus into Hi-Z mode so Spartan-6 can boot from W25Q128
		SPI_Bus_Release_To_FPGA();

		// 4. Pulse PROG_B (PB15) and wait for DONE = 1 (PA1) with 500ms timeout
		if(FPGA_Reset_With_Check(10, 1500) != osOK){
			test_hardware_result |= _B_FAULT_FPGA_;
		} else {

			// 5. FPGA boot completed: Re-acquire SPI1 bus back to STM32 (AF Mode)
			SPI_Bus_Acquire_For_STM32();

			// 6. Bind SPI Bridge strictly to FPGA Chip Select PB0 (pin_fpga_cs)
			if(FPGA_Bridge_Init(&hfpga_bridge, &hspi1, &pin_fpga_cs) != FPGA_OK){
				test_hardware_result |= _B_FAULT_FPGA_;
			} else {

				// 7. Verify SPI link: Read hardcoded 0xDEAD signature from Device 1
				if(FPGA_Debug_Verify(&hfpga_bridge, 100) != FPGA_OK){
					test_hardware_result |= _B_FAULT_FPGA_;
				} else {

					// 8. Test SPI data path integrity via loopback register
					if(FPGA_Debug_Test_Echo(&hfpga_bridge, FPGA_DEBUG_ECHO_TEST_VAL, 100) != FPGA_OK){
						test_hardware_result |= _B_FAULT_FPGA_;
					} else {

						// 9. Configure FPGA application modules (100 Hz timer, latch enable)
						fcs_init_fpga_hardware();
						if(fcs_state.is_link_error){
							test_hardware_result |= _B_FAULT_FPGA_;
						} else {

							// 10. Initialize BC UART2 communication
							if(host_uart_init(&huart2) != true){
								test_hardware_result |= _B_FAULT_HOST_UART_;
							} else {
								host_uart_start_receive();
							}
						}
					}
				}
			}
		}
	}

	// 11. Initialize DSPA Terminal subsystem
	if(!terminal_init()){
		test_hardware_result |= _B_FAULT_TERMINAL_;
	}

	return test_hardware_result;
}

// Module status check helper
bool	test_status_hardware(uint32_t module){
	return !(get_status_hardware() & module);
}

// Returns current global diagnostic register
uint32_t	get_status_hardware(void){
	return test_hardware_result;
}

// Checks and clears sticky Software Reset flag
bool	get_rcc_csr(void){
	bool is_soft_reset = __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) ? true : false;
	if(is_soft_reset){
		__HAL_RCC_CLEAR_RESET_FLAGS();
	}
	return is_soft_reset;
}

// System reset wrapper
/*void	bsp_system_reset(void){
	HAL_NVIC_SystemReset();
}*/
void	bsp_system_reset(void){
	__disable_irq();
	__DSB();
	__ISB();
	NVIC_SystemReset();
	while(1){
		__NOP();
	}
}

// RS-485 Transmitter Enable wrapper with atomic 2us stabilization delay
void	ten(bool par){
	if(par){
		PIN_Set_F(&pin_usart1_kpa_te);
		delay_us(2); // Transceiver stabilization time
	} else {
		PIN_Reset_F(&pin_usart1_kpa_te);
	}
}

/* ========================================================================= */
/*                         ФУНКЦИИ ЧТЕНИЯ                                    */
/* ========================================================================= */

/**
 * @brief  Чтение температуры кристалла с опциональным усреднением
 * @param  filter_enable: 1 - включить скользящее среднее, 0 - без фильтрации
 * @param  filter_samples: количество выборок для усреднения (1..32)
 * @retval Температура в градусах Цельсия или TEMP_ERROR_VALUE при сбое
 */
float Read_Temperature_Enhanced(uint8_t filter_enable, uint8_t filter_samples){
    uint32_t vref_raw = 0;
    uint32_t temp_raw = 0;
    float current_temp;

    /* Буфер и переменные для скользящего среднего */
    static float filter_buffer[TEMP_FILTER_MAX_SAMPLES] = {0};
    static uint8_t filter_idx = 0;
    static uint8_t filter_count = 0;

    /* Ограничение параметров фильтра */
    if (filter_samples < 1U) filter_samples = 1U;
    if (filter_samples > TEMP_FILTER_MAX_SAMPLES) filter_samples = TEMP_FILTER_MAX_SAMPLES;

    /* ---------------------------------------------------------------------
     * 1. Запуск секвенсора: последовательно считываем Rank 1 и Rank 2
     * --------------------------------------------------------------------- */
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        cpu_temperature = TEMP_ERROR_VALUE;
        return cpu_temperature;
    }

    /* Ожидание и чтение Rank 1 (VREFINT) */
    if (HAL_ADC_PollForConversion(&hadc1, 5) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        cpu_temperature = TEMP_ERROR_VALUE;
        return cpu_temperature;
    }
    vref_raw = HAL_ADC_GetValue(&hadc1);

    /* Ожидание и чтение Rank 2 (TEMPSENSOR) */
    if (HAL_ADC_PollForConversion(&hadc1, 5) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        cpu_temperature = TEMP_ERROR_VALUE;
        return cpu_temperature;
    }
    temp_raw = HAL_ADC_GetValue(&hadc1);

    /* Останавливаем АЦП после завершения цепочки */
    HAL_ADC_Stop(&hadc1);

    /* Защита от деления на ноль */
    if (vref_raw == 0) {
        cpu_temperature = TEMP_ERROR_VALUE;
        return cpu_temperature;
    }

    /* ---------------------------------------------------------------------
     * 2. Расчет реального напряжения питания VDDA (VREF+)
     *    Формула: VDDA = 3.3V * VREFINT_CAL / VREFINT_DATA
     * --------------------------------------------------------------------- */
    real_vref = ((float)(*VREFINT_CAL_ADDR) * VREFINT_CAL_VOLTAGE) / (float)vref_raw;

    /* ---------------------------------------------------------------------
     * 3. Перевод сырых отсчетов датчика в Вольты с учетом реального VDDA
     * --------------------------------------------------------------------- */
    adc_voltage = ((float)temp_raw * real_vref) / ADC_MAX_VALUE_12BIT;

    /* ---------------------------------------------------------------------
     * 4. Расчет температуры по Datasheet STM32F411
     *    T = ((Vsense - V25) / Slope) + 25.0
     * --------------------------------------------------------------------- */
    current_temp = ((adc_voltage - TEMP_SENSOR_V25) / TEMP_SENSOR_AVG_SLOPE) + TEMP_SENSOR_REF_TEMP;

    /* ---------------------------------------------------------------------
     * 5. Фильтр скользящего среднего
     * --------------------------------------------------------------------- */
    if (filter_enable && (filter_samples > 1U)) {
        filter_buffer[filter_idx] = current_temp;
        filter_idx = (filter_idx + 1U) % filter_samples;

        if (filter_count < filter_samples) {
            filter_count++;
        }

        float sum = 0.0f;
        for (uint8_t i = 0; i < filter_count; i++) {
            sum += filter_buffer[i];
        }
        cpu_temperature = sum / (float)filter_count;
    } else {
        cpu_temperature = current_temp;
    }

    return cpu_temperature;
}

/**
 * @brief  Базовая функция-обертка
 * @retval Температура с усреднением по 8 сэмплам (800 мс при вызове раз в 100 мс)
 */
float Read_Temperature(void) {
	return Read_Temperature_Enhanced(1, 8);
}

/* ========================================================================= */
/*  DWT MICROSECOND DELAY GENERATOR                                          */
/* ========================================================================= */

/*
void	DWT_Init(void){
	volatile uint32_t *dwt_lar = (volatile uint32_t *)0xE0001FB0U;
	*dwt_lar = 0xC5ACCE55U;

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
*/

void	DWT_Init(void){
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void	DWT_DeInit(void){
	DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
	DWT->CYCCNT = 0;
	CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

void	delay_us(const uint32_t us){
	const uint32_t ticks_needed = us * (SystemCoreClock / 1000000U);
	const uint32_t tick_start   = DWT->CYCCNT;

	while((DWT->CYCCNT - tick_start) < ticks_needed){
		// Busy wait
	}
}

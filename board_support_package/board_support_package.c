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

/* Global Analog Telemetry */
float adc_voltage = 0.0f;
float cpu_temperature = 0.0f;

/* Factory Calibration Register Addresses for STM32F411 */
#define TS_CAL1_ADDR     ((volatile uint16_t*)0x1FFF7A2CU)
#define TS_CAL2_ADDR     ((volatile uint16_t*)0x1FFF7A30U)

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
		if(FPGA_Reset_With_Check(10, 500) != osOK){
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
void	bsp_system_reset(void){
	HAL_NVIC_SystemReset();
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
/*  ANALOG MEASUREMENTS (ADC INTERNAL TEMPERATURE)                           */
/* ========================================================================= */

// Fast internal CPU temperature calculation (8-sample hardware averaging)
static float	Read_Temperature_Enhanced(void){
	uint32_t adc_value = 0;
	float temp = 0.0f;

	// Wake up internal temperature sensor and Vrefint channels
	if((ADC->CCR & ADC_CCR_TSVREFE) == 0){
		ADC->CCR |= ADC_CCR_TSVREFE;
		delay_us(20);
	}

	// Take average of 8 consecutive conversions for noise suppression
	for(int i = 0; i < 8; i++){
		HAL_ADC_Start(&hadc1);
		if(HAL_ADC_PollForConversion(&hadc1, 4) == HAL_OK){
			adc_value += HAL_ADC_GetValue(&hadc1);
		}
		HAL_ADC_Stop(&hadc1);
	}
	adc_value /= 8;

	// Factory calibration registers
	uint16_t ts_cal1 = *TS_CAL1_ADDR;
	uint16_t ts_cal2 = *TS_CAL2_ADDR;

	if(ts_cal2 > ts_cal1 && ts_cal1 != 0xFFFF && ts_cal2 != 0xFFFF){
		temp = ((110.0f - 30.0f) / (float)(ts_cal2 - ts_cal1)) * (float)((int32_t)adc_value - ts_cal1) + 30.0f;
	} else {
		adc_voltage = (float)adc_value * 3.3f / 4095.0f;
		temp = 25.0f + ((adc_voltage - 0.76f) / 0.0025f);
	}
	return temp;
}

// Public wrapper (exactly like Bootloader)
float	Read_Temperature(void){
	return Read_Temperature_Enhanced();
}

/* ========================================================================= */
/*  DWT MICROSECOND DELAY GENERATOR                                          */
/* ========================================================================= */

void	DWT_Init(void){
	volatile uint32_t *dwt_lar = (volatile uint32_t *)0xE0001FB0U;
	*dwt_lar = 0xC5ACCE55U;

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

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
/* Global ADC values */
float adc_voltage = 0.0f;   /* Temperature sensor voltage, V */
float real_vref = 3.3f;     /* Measured VDDA via VREFINT, V */
float cpu_temperature = 0.0f;

/* Factory calibration addresses from HAL (stm32f4xx_ll_adc.h):
   VREFINT_CAL_ADDR      0x1FFF7A2A - VREFINT raw @30C, VDDA=3.3V
   TEMPSENSOR_CAL1_ADDR  0x1FFF7A2C - sensor raw @30C,  VDDA=3.3V (DS8623 Table 72)
   TEMPSENSOR_CAL2_ADDR  0x1FFF7A2E - sensor raw @110C, VDDA=3.3V (DS8623 Table 72) */

#define ADC_FULL_SCALE       4095.0f
#define TEMP_ERROR_VALUE     (-999.0f)
#define TEMP_AVG_PAIRS       8U       /* Number of (VREFINT, TEMP) pairs averaged per call */
#define TEMP_CONV_TIMEOUT_MS 10U

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
/*                         READ FUNCTIONS                                      */
/* ========================================================================= */

/**
 * @brief  Read die temperature using factory calibration and VREFINT.
 * @retval Temperature in deg C, or TEMP_ERROR_VALUE on hardware failure.
 */
static float Read_Temperature_Enhanced(void){
	static uint8_t ts_ready = 0U;
	static uint16_t cal1 = 0xFFFFU, cal2 = 0xFFFFU, vref_cal = 0xFFFFU;
	uint32_t vref_sum = 0, temp_sum = 0;
	uint32_t vref_avg, temp_avg;
	float v_c1, v_c2, temp;
	uint8_t good = 0U;
	uint8_t ok = 1U;
	uint8_t i;

	/* One-time init: enable internal channels (VREFINT startup up to 1 ms)
	   and latch factory calibration (immutable ROM values). */
	if(ts_ready == 0U){
		if((ADC->CCR & ADC_CCR_TSVREFE) == 0U){
			ADC->CCR |= ADC_CCR_TSVREFE;
			HAL_Delay(2);
		}
		cal1 = *TEMPSENSOR_CAL1_ADDR;
		cal2 = *TEMPSENSOR_CAL2_ADDR;
		vref_cal = *VREFINT_CAL_ADDR;
		ts_ready = 1U;
	}

	/* Sequencer: Rank1 = VREFINT, Rank2 = TEMP SENSOR, continuous scan */
	if(HAL_ADC_Start(&hadc1) != HAL_OK){
		ok = 0U;
	}

	/* Discard first pair (transient right after ADC start) */
	if(ok && ((HAL_ADC_PollForConversion(&hadc1, TEMP_CONV_TIMEOUT_MS) != HAL_OK) ||
	          (HAL_ADC_PollForConversion(&hadc1, TEMP_CONV_TIMEOUT_MS) != HAL_OK))){
		ok = 0U;
	}

	/* Average BOTH channels over TEMP_AVG_PAIRS pairs */
	for(i = 0U; (ok != 0U) && (i < TEMP_AVG_PAIRS); i++){
		if(HAL_ADC_PollForConversion(&hadc1, TEMP_CONV_TIMEOUT_MS) != HAL_OK){
			ok = 0U;
			break;
		}
		vref_sum += HAL_ADC_GetValue(&hadc1);   /* Rank1 = VREFINT */
		if(HAL_ADC_PollForConversion(&hadc1, TEMP_CONV_TIMEOUT_MS) != HAL_OK){
			ok = 0U;
			break;
		}
		temp_sum += HAL_ADC_GetValue(&hadc1);   /* Rank2 = TEMP SENSOR */
		good++;
	}
	HAL_ADC_Stop(&hadc1);

	vref_avg = (good != 0U) ? (vref_sum / good) : 0U;
	temp_avg = (good != 0U) ? (temp_sum / good) : 0U;

	if((ok == 0U) || (good == 0U) || (vref_avg == 0U)){
		cpu_temperature = TEMP_ERROR_VALUE;
		return cpu_temperature;
	}

	/* 1. Real VDDA from VREFINT channel (AN4073):
	   VDDA = V_CAL_VDDA * VREFINT_CAL / VREFINT_DATA.
	   VREFINT_CAL was acquired with VDDA = 3.3 V. */
	if((vref_cal != 0xFFFFU) && (vref_cal != 0x0000U)){
		real_vref = (3.3f * (float)vref_cal) / (float)vref_avg;
	} else {
		real_vref = 3.3f;                        /* Corrupted calibration -> nominal */
	}

	/* 2. Sensor voltage in volts (independent of actual supply) */
	adc_voltage = ((float)temp_avg * real_vref) / ADC_FULL_SCALE;

	/* 3. Temperature from per-chip calibration (sign-agnostic: slope derived
	   from the actual calibration points, works for either slope sign). */
	v_c1 = 3.3f * (float)cal1 / ADC_FULL_SCALE;  /* Sensor voltage at 30  C */
	v_c2 = 3.3f * (float)cal2 / ADC_FULL_SCALE;  /* Sensor voltage at 110 C */

	if((cal1 != 0xFFFFU) && (cal2 != 0xFFFFU) && (cal1 != cal2)){
		temp = 30.0f + ((adc_voltage - v_c1) * (110.0f - 30.0f) / (v_c2 - v_c1));
	} else {
		/* Corrupted factory calibration -> datasheet typical values:
		   V25 = 0.76 V, average slope 2.5 mV/degC (DS8623 Table 71) */
		temp = 25.0f + ((adc_voltage - 0.76f) / 0.0025f);
	}

	cpu_temperature = temp;
	return cpu_temperature;
}


/**
 * @brief  Simple wrapper for backward compatibility.
 */
float Read_Temperature(void){
	return Read_Temperature_Enhanced();
}

/* ========================================================================= */
/*  DWT MICROSECOND DELAY GENERATOR                                          */
/* ========================================================================= */

void	DWT_Init(void){
	// Trace block must be clocked before its registers accept writes
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

	// DWT->LAR (0xE0001FB0): unlocked on STM32F4 regardless, required on M7 parts
	volatile uint32_t *dwt_lar = (volatile uint32_t *)0xE0001FB0U;
	*dwt_lar = 0xC5ACCE55U;

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

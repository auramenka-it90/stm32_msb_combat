/**
 * ******************************************************************************
 * @file    board_support_package.c
 * @brief   Secure Hardware Initialization and Board Control Helper Functions.
 *          Fixes bus contention on SPI, sticky RCC flags, and struct errors.
 *          All comments in English.
 * ******************************************************************************
 */

#include "board_support_package.h"
#include "pin_mgmt.h"
#include "stm32_2_fpga_spi_bridge.h"
#include "fpga_control.h"
#include "terminal.h"
#include "fcs.h" 	/* Required to invoke fcs_init_fpga_hardware() and access fcs_state */
#include "host.h" 	/* ADDED: Access to UART DMA driver APIs */

/* Global hardware test result variable */
uint32_t test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;

/* Global FPGA communication bridge handle instantiation */
FPGA_HandleTypeDef hfpga_bridge;

/* STM32 pins configuration */
Pin_Mgmt_Config_t pin = {
    .debug_enabled = 1
};

/**
  * @brief  Securely initializes all hardware components in the proper sequence.
  *         Handles hardware reset, SPI bridge setup, and link integrity verification.
  * @retval Current hardware status register
  */
uint32_t init_hardware(void){

    /* CRITICAL: Reset the global status before starting test sequence to allow retries */
    test_hardware_result = _B_TEST_HARDWARE_SUCCESS_;

    /* 1. PINS CONFIGURATION */
    if (PIN_MGMT_Init(&pin) != osOK) {
        test_hardware_result |= _B_FAULT_PINS_;
    } else {
        DWT_Init(); /* Initialize microsecond delay generator */

        /* === CRITICAL HARDWARE PROTECTION: RELEASE SPI BUS TO FPGA (TRI-STATE / Hi-Z) === */
        /* Configures SCK, MISO, MOSI, and CSO to High-Impedance mode so FPGA can boot safely from Flash */
        SPI_Bus_Release_To_FPGA();

        /* Perform hardware-level reset and wait for FPGA boot completion signal (DONE pin) */
        if (FPGA_Reset_With_Check(10, 500) != osOK) {
            test_hardware_result |= _B_FAULT_FPGA_;
        } else {

            /* === FPGA BOOT COMPLETED: ACQUIRE SPI BUS BACK TO STM32 (AF MODE) === */
            /* Configures pins back to active SPI1 hardware alternate function and sets CS High */
            SPI_Bus_Acquire_For_STM32();

            /* 2. FPGA SOFTWARE BRIDGE INITIALIZATION */
            /* We pass the dedicated 'pin_spi_cso' from pin_mgmt as the active Chip Select descriptor */
            if (FPGA_Bridge_Init(&hfpga_bridge, &hspi1, &pin_spi_cso) != FPGA_OK) {
                test_hardware_result |= _B_FAULT_FPGA_;
            } else {
                /* 3. SPI LINK VERIFICATION */
                if (FPGA_Debug_Verify(&hfpga_bridge, 100) != FPGA_OK) {
                    test_hardware_result |= _B_FAULT_FPGA_;
                } else {
                    /* 4. BUS INTEGRITY TEST (ECHO REGISTER) */
                    if (FPGA_Debug_Test_Echo(&hfpga_bridge, FPGA_DEBUG_ECHO_TEST_VAL, 100) != FPGA_OK) {
                        test_hardware_result |= _B_FAULT_FPGA_;
                    } else {
                        /* 5. FPGA DETAILED MODULES INITIALIZATION (Debug, FCS, IC) */
                        fcs_init_fpga_hardware();
                        if (fcs_state.is_link_error) {
                            test_hardware_result |= _B_FAULT_FPGA_;
                        } else {
                            /* 6. BC UART2 COMMUNICATION INITIALIZATION */
                            if (host_uart_init(&huart2) != true) {
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


    if (!terminal_init()) {
        test_hardware_result |= _B_FAULT_TERMINAL_;
    }

    return test_hardware_result;
}

/**
  * @brief  Checks if a specific hardware module is operational.
  */
inline bool test_status_hardware(uint32_t module) {
    return !(get_status_hardware() & module);
}

/**
  * @brief  Gets the overall hardware diagnostic status.
  */
inline uint32_t get_status_hardware(void) {
    return test_hardware_result;
}

/**
  * @brief  Checks if a Software Reset occurred.
  *         Clears the sticky RCC reset flags to ensure future cold boots are read correctly.
  *         Call before HAL_Init.
  */
inline bool get_rcc_csr(void) {
    bool is_soft_reset = __HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) ? true : false;

    if (is_soft_reset) {
        /* Clear sticky reset flags, otherwise SFTRST remains 'true' on subsequent power-on resets */
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }

    return is_soft_reset;
}

/**
  * @brief  Performs a safe system reset.
  */
void bsp_system_reset(void) {
    HAL_NVIC_SystemReset();
}

/**
  * @brief  RS-485 Driver Transmitter Enable (TE) control wrapper.
  *         Uses ultra-fast atomic register access without mutex overhead.
  *         Provides exact 2us transceiver stabilization delay using DWT.
  * @param  par: true to enable transmitter, false to disable.
  */
void ten(bool par) {
    if (par) {
        /* Direct write to BSRR register - atomic, thread-safe, ultra-fast */
        PIN_Set_F(&pin_usart1_kpa_te);

        /* Datasheet required delay for RS-485 transceiver driver to stabilize */
        delay_us(2);
    } else {
        /* Disable transmitter immediately after transmission is complete */
        PIN_Reset_F(&pin_usart1_kpa_te);
    }
}

/* ========================================================================= */
/*  ADC INTERNAL TEMPERATURE SENSOR OPERATIONS                              */
/* ========================================================================= */

/* Global ADC values */
float adc_voltage = 0.0f;
float cpu_temperature = 0.0f;

#define TS_CAL1_ADDR     ((volatile uint16_t*)0x1FFF7A2CU)
#define TS_CAL2_ADDR     ((volatile uint16_t*)0x1FFF7A30U)

/**
  * @brief  Enhanced internal CPU temperature reading.
  *         Uses factory-calibrated values if valid, otherwise automatically
  *         falls back to your physically-tested and verified formula.
  * @param  filter_enable: 1 to enable moving average filter, 0 for raw reading
  * @param  filter_samples: Number of samples for moving average (1-32)
  * @retval Filtered temperature in degrees Celsius
  */
float Read_Temperature_Enhanced(uint8_t filter_enable, uint8_t filter_samples)
{
    HAL_StatusTypeDef status;
    uint32_t adc_value;
    static float filter_buffer[32] = {0};
    static uint8_t filter_idx = 0;
    static uint8_t filter_ready = 0;
    float sum = 0;
    uint8_t i, valid_samples;

    /* Clamp filter samples to valid range */
    if (filter_samples < 1) filter_samples = 1;
    if (filter_samples > 32) filter_samples = 32;

    /* 1. CRITICAL: Ensure the Temperature Sensor and VREFINT channels are enabled */
    if ((ADC->CCR & ADC_CCR_TSVREFE) == 0) {
        ADC->CCR |= ADC_CCR_TSVREFE;
        delay_us(20); /* Wait for the internal sensor to wake up */
    }

    /* 2. Start ADC conversion */
    HAL_ADC_Start(&hadc1);
    status = HAL_ADC_PollForConversion(&hadc1, 4);

    if (status != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        cpu_temperature = -999.0f;
        return cpu_temperature;
    }

    /* 3. Retrieve raw conversion value */
    adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* 4. Try high-accuracy calculation using factory calibration registers */
    uint16_t ts_cal1 = *TS_CAL1_ADDR;
    uint16_t ts_cal2 = *TS_CAL2_ADDR;

    /* Verify if factory calibration registers contain valid values */
    if (ts_cal2 > ts_cal1 && ts_cal1 != 0xFFFF && ts_cal2 != 0xFFFF)
    {
        /* High-accuracy calculation using factory calibration values */
        cpu_temperature = ((110.0f - 30.0f) / (float)(ts_cal2 - ts_cal1)) * (float)((int32_t)adc_value - ts_cal1) + 30.0f;
    }
    else
    {
        /* Safe fallback to your trusted, heat-gun-tested typical formula if calibration data is invalid */
        adc_voltage = (float)adc_value * 3.3f / 4095.0f;
        cpu_temperature = 25.0f + ((adc_voltage - 0.76f) / 0.0025f);
    }

    /* 5. Apply moving average filtering if enabled */
    if (filter_enable && filter_samples > 1) {
        /* Add new sample to circular buffer */
        filter_buffer[filter_idx] = cpu_temperature;
        filter_idx = (filter_idx + 1) % filter_samples;

        /* Set buffer ready flag on first full loop */
        if (!filter_ready && filter_idx == 0) {
            filter_ready = 1;
        }

        /* Determine number of valid samples to average */
        if (filter_ready) {
            valid_samples = filter_samples;
        } else {
            valid_samples = filter_idx;
        }

        /* Calculate current moving average */
        if (valid_samples > 0) {
            for (i = 0; i < valid_samples; i++) {
                sum += filter_buffer[i];
            }
            cpu_temperature = sum / (float)valid_samples;
        }
    }

    return cpu_temperature;
}

/**
 * @brief  Simple wrapper for backward compatibility.
 */
float Read_Temperature(void) {
    return Read_Temperature_Enhanced(1, 24);
}

/* ========================================================================= */
/*  DWT DELAY HARDWARE CONTROL                                               */
/* ========================================================================= */

/**
 * @brief  Initializes the Data Watchpoint and Trace (DWT) cycle counter.
 */
void DWT_Init(void) {
    volatile uint32_t *dwt_lar = (volatile uint32_t *)0xE0001FB0U;
    *dwt_lar = 0xC5ACCE55U;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  Deinitializes the DWT cycle counter.
 */
void DWT_DeInit(void) {
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
    CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

/**
 * @brief  Performs microsecond blocking delay.
 */
void delay_us(const uint32_t us) {
    const uint32_t ticks_needed = us * (SystemCoreClock / 1000000U);
    const uint32_t tick_start = DWT->CYCCNT;

    while ((DWT->CYCCNT - tick_start) < ticks_needed) {
        /* Busy wait */
    }
}

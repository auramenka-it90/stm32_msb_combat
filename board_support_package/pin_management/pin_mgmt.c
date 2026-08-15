/**
 ******************************************************************************
 * @file    pin_mgmt.c
 * @brief   Pin Management Library Implementation (MainAppl).
 *          Features Fast Atomic Registers (BSRR/IDR) and Thread-Safe I/O.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "pin_mgmt.h"

/* Global Pin Management Configuration */
Pin_Mgmt_Config_t bsp_pin_config = PIN_MGMT_CONFIG_DEFAULT;

static uint8_t      spi_bus_acquired = 0;
static osMutexId_t  gpio_mutex       = NULL;

/* ========================================================================= */
/*  PIN DESCRIPTORS                                                          */
/* ========================================================================= */

const Pin_Descriptor_t pin_led_green = PIN_DESC(LED_GREEN_GPIO_Port, LED_GREEN_Pin, 0, "LED_GREEN");
const Pin_Descriptor_t pin_clk25mhz  = PIN_DESC(CLK25MHZ_GPIO_Port, CLK25MHZ_Pin, 0, "CLK25MHZ");

/* Test Points for Logic Analyzer / Oscilloscope */
const Pin_Descriptor_t pin_tp1        = PIN_DESC(TP1_GPIO_Port, TP1_Pin, 0, "TP1");
const Pin_Descriptor_t pin_tp2        = PIN_DESC(TP2_GPIO_Port, TP2_Pin, 0, "TP2");
const Pin_Descriptor_t pin_tp3        = PIN_DESC(TP3_GPIO_Port, TP3_Pin, 0, "TP3");
const Pin_Descriptor_t pin_tp4        = PIN_DESC(TP4_GPIO_Port, TP4_Pin, 0, "TP4");

/* FPGA Boot Status Pin (PA1) */
const Pin_Descriptor_t pin_fpga_done  = PIN_DESC(DONE_GPIO_Port, DONE_Pin, 0, "FPGA_DONE");

/* UART2: Link to FPGA */
const Pin_Descriptor_t pin_usart2_fpga_tx = PIN_DESC(USART2_FPGA_TX_GPIO_Port, USART2_FPGA_TX_Pin, 0, "USART2_FPGA_TX");
const Pin_Descriptor_t pin_usart2_fpga_rx = PIN_DESC(USART2_FPGA_RX_GPIO_Port, USART2_FPGA_RX_Pin, 0, "USART2_FPGA_RX");

/* PA4: Flash W25Q128 Chip Select (Default 1, kept as Input in MainAppl) */
const Pin_Descriptor_t pin_flash_cs = PIN_DESC(SP1_FPGA_CSO_GPIO_Port, SP1_FPGA_CSO_Pin, 1, "FLASH_CS");
const Pin_Descriptor_t pin_spi_cso  = PIN_DESC(SP1_FPGA_CSO_GPIO_Port, SP1_FPGA_CSO_Pin, 1, "FLASH_CS");

/* PB0: FPGA Registers Chip Select (Active working CS for MainAppl) */
const Pin_Descriptor_t pin_fpga_cs            = PIN_DESC(STM32_2_FPGA_NSS_P_GPIO_Port, STM32_2_FPGA_NSS_P_Pin, 1, "FPGA_CS");
const Pin_Descriptor_t pin_stm32_2_fpga_nss_p = PIN_DESC(STM32_2_FPGA_NSS_P_GPIO_Port, STM32_2_FPGA_NSS_P_Pin, 1, "FPGA_CS");

/* SPI1 Data Bus */
const Pin_Descriptor_t pin_spi_sck   = PIN_DESC(SP1_FPGA_SCK_GPIO_Port, SP1_FPGA_SCK_Pin, 0, "SPI_SCK");
const Pin_Descriptor_t pin_spi_miso  = PIN_DESC(SPI1_FPGA_MISO_GPIO_Port, SPI1_FPGA_MISO_Pin, 0, "SPI_MISO");
const Pin_Descriptor_t pin_spi_mosi  = PIN_DESC(SPI1_FPGA_MOSI_GPIO_Port, SPI1_FPGA_MOSI_Pin, 0, "SPI_MOSI");

const Pin_Descriptor_t pin_fpga_2_stm32_misc1 = PIN_DESC(FPGA_2_STM32_MISC1_GPIO_Port, FPGA_2_STM32_MISC1_Pin, 0, "FPGA_MISC1");
const Pin_Descriptor_t pin_stm32_2_fpga_nss_d = PIN_DESC(STM32_2_FPGA_NSS_D_GPIO_Port, STM32_2_FPGA_NSS_D_Pin, 1, "STM32_NSS_D");
const Pin_Descriptor_t pin_interrupt_fpga      = PIN_DESC(INTERRUPT_FPGA_GPIO_Port, INTERRUPT_FPGA_Pin, 0, "INTERRUPT_FPGA");

const Pin_Descriptor_t pin_mr_fpga   = PIN_DESC(MR_FPGA_GPIO_Port, MR_FPGA_Pin, 0, "MR_FPGA");
/* PB15: PROG_B (Default 0: FPGA held in reset until explicit bring-up) */
const Pin_Descriptor_t pin_mr_prog   = PIN_DESC(MR_PROG_GPIO_Port, MR_PROG_Pin, 0, "MR_PROG");

/* UART6: Debug / ST-Link VCP */
const Pin_Descriptor_t pin_usart6_debug_tx = PIN_DESC(USART6_DEBUG_TX_GPIO_Port, USART6_DEBUG_TX_Pin, 0, "USART6_DEBUG_TX");
const Pin_Descriptor_t pin_usart6_debug_rx = PIN_DESC(USART6_DEBUG_RX_GPIO_Port, USART6_DEBUG_RX_Pin, 0, "USART6_DEBUG_RX");

/* UART1: KPA / RS-485 */
const Pin_Descriptor_t pin_usart1_kpa_te = PIN_DESC(USART1_KPA_TE_GPIO_Port, USART1_KPA_TE_Pin, 0, "USART1_KPA_TE");
const Pin_Descriptor_t pin_usart1_kpa_tx = PIN_DESC(USART1_KPA_TX_GPIO_Port, USART1_KPA_TX_Pin, 0, "USART1_KPA_TX");
const Pin_Descriptor_t pin_usart1_kpa_rx = PIN_DESC(USART1_KPA_RX_GPIO_Port, USART1_KPA_RX_Pin, 0, "USART1_KPA_RX");

/* ST-Link Hardware Detection Pin */
const Pin_Descriptor_t pin_stlink_detect = PIN_DESC(STLINK_GND_TEST_GPIO_Port, STLINK_GND_TEST_Pin, 1, "STLINK_DETECT");

/* ========================================================================= */
/*  PRIVATE FUNCTIONS                                                        */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Creates recursive GPIO mutex to protect shared bus accesses
//-----------------------------------------------------------------------------
static osStatus_t	_create_gpio_mutex(void){
	if(gpio_mutex == NULL){
		const osMutexAttr_t mutex_attr = {
			.name       = "PIN_MGMT_GPIO_Mutex",
			.attr_bits  = osMutexRecursive | osMutexPrioInherit,
			.cb_mem     = NULL,
			.cb_size    = 0U
		};
		gpio_mutex = osMutexNew(&mutex_attr);
		if(gpio_mutex == NULL) return osErrorResource;
	}
	return osOK;
}

//-----------------------------------------------------------------------------
// Deletes GPIO mutex
//-----------------------------------------------------------------------------
static osStatus_t	_delete_gpio_mutex(void){
	if(gpio_mutex != NULL){
		osMutexDelete(gpio_mutex);
		gpio_mutex = NULL;
	}
	return osOK;
}

//-----------------------------------------------------------------------------
// Configures SPI1 lines to AF5 and enables PB0 CS output (PA4 untouched)
//-----------------------------------------------------------------------------
static osStatus_t	_spi_bus_af_mode(void){
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* SCK (PA5), MISO (PA6), MOSI (PA7) -> AF5 */
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;

	GPIO_InitStruct.Pin = pin_spi_sck.pin;
	HAL_GPIO_Init(pin_spi_sck.port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = pin_spi_miso.pin;
	HAL_GPIO_Init(pin_spi_miso.port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = pin_spi_mosi.pin;
	HAL_GPIO_Init(pin_spi_mosi.port, &GPIO_InitStruct);

	/* FPGA CS (PB0) -> Output Push-Pull, Initial HIGH (1) */
	GPIO_InitStruct.Pin   = pin_fpga_cs.pin;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(pin_fpga_cs.port, &GPIO_InitStruct);
	PIN_Set_F(&pin_fpga_cs);

	return osOK;
}

//-----------------------------------------------------------------------------
// Puts SPI1 lines and PB0 into Analog mode (Hi-Z) during FPGA boot
//-----------------------------------------------------------------------------
static osStatus_t	_spi_bus_hiz_mode(void){
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;

	GPIO_InitStruct.Pin = pin_spi_sck.pin;
	HAL_GPIO_Init(pin_spi_sck.port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = pin_spi_miso.pin;
	HAL_GPIO_Init(pin_spi_miso.port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = pin_spi_mosi.pin;
	HAL_GPIO_Init(pin_spi_mosi.port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = pin_fpga_cs.pin;
	HAL_GPIO_Init(pin_fpga_cs.port, &GPIO_InitStruct);

	return osOK;
}

/* ========================================================================= */
/*  PUBLIC API IMPLEMENTATION                                                */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Initializes pin management subsystem and creates mutex
//-----------------------------------------------------------------------------
osStatus_t	PIN_MGMT_Init(Pin_Mgmt_Config_t *config){
	if(config != NULL){
		bsp_pin_config = *config;
	}
	spi_bus_acquired = 0;
	return _create_gpio_mutex();
}

//-----------------------------------------------------------------------------
// Deinitializes pin management subsystem
//-----------------------------------------------------------------------------
osStatus_t	PIN_MGMT_DeInit(void){
	_delete_gpio_mutex();
	return osOK;
}

//-----------------------------------------------------------------------------
// Standard mutex-protected pin set
//-----------------------------------------------------------------------------
osStatus_t	PIN_Set(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	osStatus_t status = PIN_GPIO_Mutex_Acquire(osWaitForever);
	if(status != osOK) return status;

	HAL_GPIO_WritePin(pin->port, pin->pin, GPIO_PIN_SET);
	PIN_GPIO_Mutex_Release();
	return osOK;
}

//-----------------------------------------------------------------------------
// Standard mutex-protected pin reset
//-----------------------------------------------------------------------------
osStatus_t	PIN_Reset(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	osStatus_t status = PIN_GPIO_Mutex_Acquire(osWaitForever);
	if(status != osOK) return status;

	HAL_GPIO_WritePin(pin->port, pin->pin, GPIO_PIN_RESET);
	PIN_GPIO_Mutex_Release();
	return osOK;
}

//-----------------------------------------------------------------------------
// Standard mutex-protected pin toggle
//-----------------------------------------------------------------------------
osStatus_t	PIN_Toggle(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	osStatus_t status = PIN_GPIO_Mutex_Acquire(osWaitForever);
	if(status != osOK) return status;

	HAL_GPIO_TogglePin(pin->port, pin->pin);
	PIN_GPIO_Mutex_Release();
	return osOK;
}

//-----------------------------------------------------------------------------
// Standard mutex-protected pin read
//-----------------------------------------------------------------------------
uint8_t		PIN_Read(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return 0;
	uint8_t state = 0;
	if(PIN_GPIO_Mutex_Acquire(osWaitForever) == osOK){
		state = (HAL_GPIO_ReadPin(pin->port, pin->pin) == GPIO_PIN_SET) ? 1 : 0;
		PIN_GPIO_Mutex_Release();
	}
	return state;
}

//-----------------------------------------------------------------------------
// Fast atomic pin set via BSRR register (No mutex overhead)
//-----------------------------------------------------------------------------
osStatus_t	PIN_Set_F(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	pin->port->BSRR = pin->pin;
	return osOK;
}

//-----------------------------------------------------------------------------
// Fast atomic pin reset via upper BSRR register (No mutex overhead)
//-----------------------------------------------------------------------------
osStatus_t	PIN_Reset_F(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	pin->port->BSRR = (uint32_t)pin->pin << 16U;
	return osOK;
}

//-----------------------------------------------------------------------------
// Fast atomic pin toggle via BSRR (Prevents RMW race condition on shared ports)
//-----------------------------------------------------------------------------
osStatus_t	PIN_Toggle_F(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return osErrorParameter;
	uint32_t odr = pin->port->ODR;
	if(odr & pin->pin){
		pin->port->BSRR = (uint32_t)pin->pin << 16U;
	} else {
		pin->port->BSRR = pin->pin;
	}
	return osOK;
}

//-----------------------------------------------------------------------------
// Fast direct non-blocking IDR register read
//-----------------------------------------------------------------------------
uint8_t		PIN_Read_F(const Pin_Descriptor_t *pin){
	if(!pin || !pin->port) return 0;
	return (pin->port->IDR & pin->pin) ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Acquires GPIO bus mutex
//-----------------------------------------------------------------------------
osStatus_t	PIN_GPIO_Mutex_Acquire(uint32_t timeout){
	if(gpio_mutex == NULL){
		osStatus_t status = _create_gpio_mutex();
		if(status != osOK) return status;
	}
	return osMutexAcquire(gpio_mutex, timeout);
}

//-----------------------------------------------------------------------------
// Releases GPIO bus mutex
//-----------------------------------------------------------------------------
osStatus_t	PIN_GPIO_Mutex_Release(void){
	if(gpio_mutex == NULL) return osErrorResource;
	return osMutexRelease(gpio_mutex);
}

//-----------------------------------------------------------------------------
// Checks if GPIO mutex is currently locked
//-----------------------------------------------------------------------------
uint8_t		PIN_GPIO_Mutex_Is_Locked(void){
	if(gpio_mutex == NULL) return 0;
	osStatus_t status = osMutexAcquire(gpio_mutex, 0);
	if(status == osOK){
		osMutexRelease(gpio_mutex);
		return 0;
	}
	return 1;
}

//-----------------------------------------------------------------------------
// Pulses PROG_B line to trigger FPGA reconfiguration
//-----------------------------------------------------------------------------
osStatus_t	FPGA_Reset_OS(uint32_t reset_time_ms){
	PIN_Reset_F(&pin_mr_prog);
	PIN_Delay(reset_time_ms);
	PIN_Set_F(&pin_mr_prog);
	return osOK;
}

//-----------------------------------------------------------------------------
// Polls DONE pin until HIGH with timeout
//-----------------------------------------------------------------------------
osStatus_t	FPGA_Wait_Ready(uint32_t timeout_ms){
	uint32_t start_tick = HAL_GetTick();
	while(!FPGA_Is_Ready()){
		if((HAL_GetTick() - start_tick) > timeout_ms){
			return osErrorTimeout;
		}
		PIN_Delay(10);
	}
	return osOK;
}

//-----------------------------------------------------------------------------
// Fast check of FPGA DONE status (1 = Ready, 0 = Unprogrammed/Booting)
//-----------------------------------------------------------------------------
uint8_t		FPGA_Is_Ready(void){
	return PIN_Read_F(&pin_fpga_done);
}

//-----------------------------------------------------------------------------
// Checks if FPGA is currently held in hardware reset
//-----------------------------------------------------------------------------
uint8_t		FPGA_Is_In_Reset(void){
	return (PIN_Read_F(&pin_mr_prog) == 0) ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Full hardware reset and wait sequence for FPGA
//-----------------------------------------------------------------------------
osStatus_t	FPGA_Reset_With_Check(uint32_t reset_time_ms, uint32_t timeout_ms){
	PIN_Reset_F(&pin_mr_prog);
	PIN_Delay(reset_time_ms);
	PIN_Set_F(&pin_mr_prog);
	return FPGA_Wait_Ready(timeout_ms);
}

//-----------------------------------------------------------------------------
// Configures physical mode of SPI pins
//-----------------------------------------------------------------------------
osStatus_t	SPI_Bus_Configure(SPI_Bus_Mode_t mode){
	switch(mode){
		case SPI_BUS_AF_MODE:  return _spi_bus_af_mode();
		case SPI_BUS_HIZ_MODE: return _spi_bus_hiz_mode();
		default:               return osErrorParameter;
	}
}

//-----------------------------------------------------------------------------
// Acquires SPI bus for STM32 (AF5 mode)
//-----------------------------------------------------------------------------
osStatus_t	SPI_Bus_Acquire_For_STM32(void){
	osStatus_t status = PIN_GPIO_Mutex_Acquire(osWaitForever);
	if(status != osOK) return status;

	if(spi_bus_acquired){
		PIN_GPIO_Mutex_Release();
		return osOK;
	}

	status = SPI_Bus_Configure(SPI_BUS_AF_MODE);
	if(status == osOK){
		spi_bus_acquired = 1;
	}
	PIN_GPIO_Mutex_Release();
	return status;
}

//-----------------------------------------------------------------------------
// Releases SPI bus into Hi-Z mode for FPGA boot
//-----------------------------------------------------------------------------
osStatus_t	SPI_Bus_Release_To_FPGA(void){
	osStatus_t status = PIN_GPIO_Mutex_Acquire(osWaitForever);
	if(status != osOK) return status;

	if(!spi_bus_acquired){
		PIN_GPIO_Mutex_Release();
		return osOK;
	}

	status = SPI_Bus_Configure(SPI_BUS_HIZ_MODE);
	if(status == osOK){
		spi_bus_acquired = 0;
	}
	PIN_GPIO_Mutex_Release();
	return status;
}

//-----------------------------------------------------------------------------
// Checks if SPI bus is currently acquired by STM32
//-----------------------------------------------------------------------------
uint8_t		SPI_Bus_Is_Acquired(void){
	uint8_t acquired = 0;
	if(PIN_GPIO_Mutex_Acquire(10) == osOK){
		acquired = spi_bus_acquired;
		PIN_GPIO_Mutex_Release();
	}
	return acquired;
}

//-----------------------------------------------------------------------------
// Fast atomic check if ST-Link hardware debugger is attached
//-----------------------------------------------------------------------------
uint8_t		STLINK_Is_Connected(void){
	return (PIN_Read_F(&pin_stlink_detect) == 0) ? 1 : 0;
}

//-----------------------------------------------------------------------------
// Universal delay (Non-blocking in RTOS, blocking in bare-metal)
//-----------------------------------------------------------------------------
osStatus_t	PIN_Delay(uint32_t ms){
	if(osKernelGetState() == osKernelRunning){
		return osDelay(ms);
	} else {
		HAL_Delay(ms);
		return osOK;
	}
}

//-----------------------------------------------------------------------------
// Blinks designated pin
//-----------------------------------------------------------------------------
osStatus_t	PIN_Blink(const Pin_Descriptor_t *pin, uint8_t count, uint32_t delay_ms){
	if(!pin) return osErrorParameter;
	for(uint8_t i = 0; i < count; i++){
		PIN_Set(pin);
		PIN_Delay(delay_ms);
		PIN_Reset(pin);
		PIN_Delay(delay_ms);
	}
	return osOK;
}

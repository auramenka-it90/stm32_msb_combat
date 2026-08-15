/**
 ******************************************************************************
 * @file    stm32_2_fpga_spi_bridge.c
 * @brief   High-Speed SPI Bridge for STM32 to FPGA Communication.
 *          Provides synchronous 16-bit register access (Polling mode).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "stm32_2_fpga_spi_bridge.h"

/* ========================================================================= */
/*  PRIVATE MACROS                                                           */
/* ========================================================================= */

#define FPGA_CS_LOW(h)   PIN_Reset_F((h)->nss_pin) /* Drives PB0 LOW  (Active)   */
#define FPGA_CS_HIGH(h)  PIN_Set_F((h)->nss_pin)   /* Drives PB0 HIGH (Inactive) */


/* ========================================================================= */
/*  PUBLIC API IMPLEMENTATION                                                */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Initializes SPI bridge, binds dedicated PB0 CS pin, and creates mutex
//-----------------------------------------------------------------------------
FPGA_Status_t	FPGA_Bridge_Init(FPGA_HandleTypeDef *hbridge, SPI_HandleTypeDef *hspi, const Pin_Descriptor_t *nss_pin){
	if(!hbridge || !hspi || !nss_pin){
		return FPGA_ERROR;
	}

	hbridge->hspi    = hspi;
	hbridge->nss_pin = nss_pin;

	// Ensure CS line is deselected initially
	FPGA_CS_HIGH(hbridge);

	// Recursive mutex prevents deadlocks during nested RMW operations
	const osMutexAttr_t mutex_attr = {
		.name       = "FpgaBridgeMutex",
		.attr_bits  = osMutexRecursive | osMutexPrioInherit,
		.cb_mem     = NULL,
		.cb_size    = 0U
	};

	hbridge->mutex_id = osMutexNew(&mutex_attr);
	if(hbridge->mutex_id == NULL){
		return FPGA_ERROR;
	}

	return FPGA_OK;
}

//-----------------------------------------------------------------------------
// Synchronous 16-bit Write: Word 1 = Cmd(MSB=1)+Addr, Word 2 = Data
//-----------------------------------------------------------------------------
FPGA_Status_t	FPGA_Write_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t data, uint32_t timeout_ms){
	HAL_StatusTypeDef hal_status;

	if(osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK){
		return FPGA_BUSY;
	}

	// Word 1: MSB=1 (Write Command), Address bits [9:0]
	hbridge->tx_buf[0] = (1U << 15) | (addr & 0x03FF);
	// Word 2: Data payload to write into FPGA register
	hbridge->tx_buf[1] = data;

	FPGA_CS_LOW(hbridge);

	// Transmit 2 words (32 clocks total on SPI configured in 16-bit mode)
	hal_status = HAL_SPI_TransmitReceive(hbridge->hspi, (uint8_t*)hbridge->tx_buf,
	                                     (uint8_t*)hbridge->rx_buf, 2, timeout_ms);

	FPGA_CS_HIGH(hbridge);

	osMutexRelease(hbridge->mutex_id);

	if(hal_status == HAL_TIMEOUT) return FPGA_TIMEOUT;
	if(hal_status != HAL_OK)      return FPGA_ERROR;

	return FPGA_OK;
}

//-----------------------------------------------------------------------------
// Synchronous 16-bit Read: Word 1 = Cmd(MSB=0)+Addr, Word 2 = Returns Data
//-----------------------------------------------------------------------------
FPGA_Status_t	FPGA_Read_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t *data, uint32_t timeout_ms){
	HAL_StatusTypeDef hal_status;

	if(!data){
		return FPGA_ERROR;
	}

	if(osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK){
		return FPGA_BUSY;
	}

	// Word 1: MSB=0 (Read Command), Address bits [9:0]
	hbridge->tx_buf[0] = (0U << 15) | (addr & 0x03FF);
	// Word 2: Dummy word to clock out the response from FPGA
	hbridge->tx_buf[1] = 0x0000;

	FPGA_CS_LOW(hbridge);

	hal_status = HAL_SPI_TransmitReceive(hbridge->hspi, (uint8_t*)hbridge->tx_buf,
	                                     (uint8_t*)hbridge->rx_buf, 2, timeout_ms);

	FPGA_CS_HIGH(hbridge);

	if(hal_status == HAL_OK){
		*data = hbridge->rx_buf[1]; // Captured FPGA response is in the 2nd slot
	}

	osMutexRelease(hbridge->mutex_id);

	if(hal_status == HAL_TIMEOUT) return FPGA_TIMEOUT;
	if(hal_status != HAL_OK)      return FPGA_ERROR;

	return FPGA_OK;
}

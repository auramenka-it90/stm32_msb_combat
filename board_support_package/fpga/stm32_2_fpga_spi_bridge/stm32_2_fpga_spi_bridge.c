/**
  ******************************************************************************
  * @file    stm32_2_fpga_spi_bridge.c
  * @brief   High-Speed Dedicated SPI Bridge for STM32 to FPGA Communication.
  *          Provides synchronous 16-bit register access (Polling mode).
  *          Fully integrated with 'pin_mgmt' library for atomic NSS/CS handling
  *          via direct register writes (BSRR).
  *          Thread-safe operations protected by RTOS CMSIS-OS2 Mutexes.
  *          All comments in English.
  ******************************************************************************
  */

#include "stm32_2_fpga_spi_bridge.h"

// --- Private Macros for NSS Control using Fast Pin Library Operations ---
#define FPGA_CS_LOW(h)  PIN_Reset_F((h)->nss_pin)
#define FPGA_CS_HIGH(h) PIN_Set_F((h)->nss_pin)

FPGA_Status_t FPGA_Bridge_Init(FPGA_HandleTypeDef *hbridge, SPI_HandleTypeDef *hspi, const Pin_Descriptor_t *nss_pin){
    if (hbridge == NULL || hspi == NULL || nss_pin == NULL) {
        return FPGA_ERROR;
    }

    hbridge->hspi    = hspi;
    hbridge->nss_pin = nss_pin;

    // Ensure NSS line is deselected initially using fast register access
    FPGA_CS_HIGH(hbridge);

    // Define attributes to initialize the mutex as recursive.
    // This prevents deadlocks when read-modify-write APIs (like LED set)
    // acquire the lock and internally call Read/Write functions that attempt
    // to acquire the same lock.
    const osMutexAttr_t mutex_attr = {
        .name = "FpgaBridgeMutex",
        .attr_bits = osMutexRecursive, // Enable recursive locking capability
        .cb_mem = NULL,
        .cb_size = 0U
    };

    hbridge->mutex_id = osMutexNew(&mutex_attr);
    if (hbridge->mutex_id == NULL) {
        return FPGA_ERROR;
    }

    return FPGA_OK;
}

FPGA_Status_t FPGA_Write_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t data, uint32_t timeout_ms){
    HAL_StatusTypeDef hal_status;
    FPGA_Status_t status = FPGA_OK;

    // Acquire bus mutex to prevent concurrency issues
    if (osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK) {
        return FPGA_BUSY;
    }

    // Prepare payload. MSB = 1 (Write command). Map address to bits [14:0].
    hbridge->tx_buf[0] = (1U << 15) | (addr & 0x03FF); // 10-bit address mask
    hbridge->tx_buf[1] = data;

    FPGA_CS_LOW(hbridge);

    // Size = 2 elements (since SPI is configured in 16-bit word length mode)
    hal_status = HAL_SPI_TransmitReceive(hbridge->hspi, (uint8_t*)hbridge->tx_buf,
                                         (uint8_t*)hbridge->rx_buf, 2, timeout_ms);

    FPGA_CS_HIGH(hbridge);

    osMutexRelease(hbridge->mutex_id);

    if (hal_status == HAL_TIMEOUT) return FPGA_TIMEOUT;
    if (hal_status != HAL_OK)      return FPGA_ERROR;

    return status;
}

FPGA_Status_t FPGA_Read_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t *data, uint32_t timeout_ms){
    HAL_StatusTypeDef hal_status;

    if (data == NULL) return FPGA_ERROR;

    if (osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK) {
        return FPGA_BUSY;
    }

    // Prepare payload. MSB = 0 (Read command). Map address to bits [14:0].
    hbridge->tx_buf[0] = (0U << 15) | (addr & 0x03FF);
    hbridge->tx_buf[1] = 0x0000; // Dummy word

    FPGA_CS_LOW(hbridge);

    hal_status = HAL_SPI_TransmitReceive(hbridge->hspi, (uint8_t*)hbridge->tx_buf,
                                         (uint8_t*)hbridge->rx_buf, 2, timeout_ms);

    FPGA_CS_HIGH(hbridge);

    if (hal_status == HAL_OK) {
        *data = hbridge->rx_buf[1]; // Captured read word is in the 2nd transaction slot
    }

    osMutexRelease(hbridge->mutex_id);

    if (hal_status == HAL_TIMEOUT) return FPGA_TIMEOUT;
    if (hal_status != HAL_OK)      return FPGA_ERROR;

    return FPGA_OK;
}

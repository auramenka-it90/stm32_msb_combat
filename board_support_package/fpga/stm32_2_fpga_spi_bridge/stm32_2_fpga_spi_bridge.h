/**
  ******************************************************************************
  * @file    stm32_2_fpga_spi_bridge.h
  * @brief   Header for STM32 to FPGA High-Speed SPI Bridge Module.
  *          Provides synchronous 16-bit register access (Polling mode).
  *          Fully integrated with 'pin_mgmt' library for atomic NSS/CS handling
  *          via direct register writes (BSRR).
  *          Thread-safe operations protected by RTOS CMSIS-OS2 Mutexes.
  *          All comments in English.
  ******************************************************************************
  */

#ifndef STM32_2_FPGA_SPI_BRIDGE_H
#define STM32_2_FPGA_SPI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "pin_mgmt.h" // Integrated to utilize Pin_Descriptor_t and fast register access

// Driver status enumeration
typedef enum {
    FPGA_OK      = 0,
    FPGA_ERROR   = 1,
    FPGA_TIMEOUT = 2,
    FPGA_BUSY    = 3
} FPGA_Status_t;

// FPGA Handle structure containing hardware references and thread-safety blocks
typedef struct {
    SPI_HandleTypeDef      *hspi;       // HAL SPI instance pointer
    const Pin_Descriptor_t *nss_pin;    // Pointer to Pin Management Descriptor for NSS
    osMutexId_t             mutex_id;   // Bus access mutex

    // Aligned DMA-safe buffers allocated inside handle to prevent stack corruption
    uint16_t           tx_buf[2] __attribute__((aligned(4)));
    uint16_t           rx_buf[2] __attribute__((aligned(4)));
} FPGA_HandleTypeDef;

/**
  * @brief  Initializes the FPGA bridge driver, binds the dedicated NSS pin, and creates RTOS primitives.
  * @param  hbridge: Pointer to driver handle.
  * @param  hspi: Pointer to pre-configured SPI handle (Must be in 16-bit Data Mode).
  * @param  nss_pin: Pointer to the dedicated NSS Pin Descriptor (e.g., &pin_stm32_2_fpga_nss_p).
  * @retval FPGA_Status_t status
  */
FPGA_Status_t FPGA_Bridge_Init(FPGA_HandleTypeDef *hbridge, SPI_HandleTypeDef *hspi, const Pin_Descriptor_t *nss_pin);

/**
  * @brief  Synchronously writes 16-bit word to FPGA register using Polling mode.
  */
FPGA_Status_t FPGA_Write_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t data, uint32_t timeout_ms);

/**
  * @brief  Synchronously reads 16-bit word from FPGA register using Polling mode.
  */
FPGA_Status_t FPGA_Read_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t *data, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* STM32_2_FPGA_SPI_BRIDGE_H */

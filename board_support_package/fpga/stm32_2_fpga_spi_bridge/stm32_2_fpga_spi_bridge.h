/**
 ******************************************************************************
 * @file    stm32_2_fpga_spi_bridge.h
 * @brief   Header for STM32 to FPGA High-Speed SPI Bridge Module.
 *          Provides synchronous 16-bit register access (Polling mode).
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
#include "pin_mgmt.h"

typedef enum {
    FPGA_OK      = 0,
    FPGA_ERROR   = 1,
    FPGA_TIMEOUT = 2,
    FPGA_BUSY    = 3
} FPGA_Status_t;

typedef struct {
    SPI_HandleTypeDef      *hspi;
    const Pin_Descriptor_t *nss_pin;    /* Bound to pin_fpga_cs (PB0) */
    osMutexId_t             mutex_id;

    /* 4-byte aligned buffers for SPI DMA/Polling */
    uint16_t           tx_buf[2] __attribute__((aligned(4)));
    uint16_t           rx_buf[2] __attribute__((aligned(4)));
} FPGA_HandleTypeDef;

/* APIs */
FPGA_Status_t	FPGA_Bridge_Init(FPGA_HandleTypeDef *hbridge, SPI_HandleTypeDef *hspi, const Pin_Descriptor_t *nss_pin);
FPGA_Status_t	FPGA_Write_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t data, uint32_t timeout_ms);
FPGA_Status_t	FPGA_Read_Poll(FPGA_HandleTypeDef *hbridge, uint16_t addr, uint16_t *data, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* STM32_2_FPGA_SPI_BRIDGE_H */

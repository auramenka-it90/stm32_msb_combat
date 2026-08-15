/**
 ******************************************************************************
 * @file    fpga_control.h
 * @brief   Header for High-Level FPGA Module Control API.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef FPGA_CONTROL_H
#define FPGA_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32_2_fpga_spi_bridge.h"
#include "fpga_reg_map.h"

/* Global FPGA SPI Bridge Handle */
extern FPGA_HandleTypeDef hfpga_bridge;

/* RS-485 Channel Selection Enum (Bit 3 of misc_reg) */
typedef enum {
    FPGA_UART_PORT_1 = 0, /* Channel 1: Transceiver DD19 (tx2, rx2, de2) */
    FPGA_UART_PORT_2 = 1  /* Channel 2: Transceiver DD20 (tx3, rx3, de3) */
} FPGA_Uart_Port_t;


/* ========================================================================= */
/*  DEBUG MODULE APIS (Device ID = 1)                                        */
/* ========================================================================= */

// Verifies SPI bus connection by checking hardcoded 0xDEAD constant
FPGA_Status_t	FPGA_Debug_Verify(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms);

// Tests SPI bus integrity by writing and reading back test pattern
FPGA_Status_t	FPGA_Debug_Test_Echo(FPGA_HandleTypeDef *hbridge, uint16_t test_val, uint32_t timeout_ms);

// Updates physical status LEDs (Bits [2:0])
FPGA_Status_t	FPGA_Debug_Set_LEDs(FPGA_HandleTypeDef *hbridge, uint8_t led_mask, uint32_t timeout_ms);

// Configures programmable tick timer divider in ms (Bits [11:4], 10 = 100 Hz)
FPGA_Status_t	FPGA_Debug_Set_Tick_Divider(FPGA_HandleTypeDef *hbridge, uint8_t divider, uint32_t timeout_ms);

// Selects active RS-485 channel: 0 = Port 1 (DD19), 1 = Port 2 (DD20)
FPGA_Status_t	FPGA_Debug_Set_UART_Mux(FPGA_HandleTypeDef *hbridge, FPGA_Uart_Port_t port, uint32_t timeout_ms);

// Direct boolean control of Red (VD37), Yellow (VD36), and Green (VD35) LEDs
FPGA_Status_t	FPGA_Debug_Write_LEDs(FPGA_HandleTypeDef *hbridge, bool r, bool y, bool g, uint32_t timeout_ms);

// Non-blocking running lights diagnostic sequence (shifts every 200 ms)
FPGA_Status_t	FPGA_Debug_Running_Lights(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms);


/* ========================================================================= */
/*  FCS MODULE APIS (Device ID = 2)                                          */
/* ========================================================================= */

// Reads all 31 discrete inputs (Status 1: DR+Ammo, Status 2: Flags+JK)
FPGA_Status_t	FPGA_FCS_Read_Inputs(FPGA_HandleTypeDef *hbridge, uint16_t *status_1, uint16_t *status_2, uint32_t timeout_ms);

// Configures Hard/Soft override mask and virtual simulation values
FPGA_Status_t	FPGA_FCS_Configure_Override(FPGA_HandleTypeDef *hbridge, uint16_t hard_soft_1, uint16_t hard_soft_2,
                                           uint16_t soft_val_1, uint16_t soft_val_2, uint32_t timeout_ms);

// Configures hardware input inversion masks on the PCB level
FPGA_Status_t	FPGA_FCS_Configure_Inversions(FPGA_HandleTypeDef *hbridge, uint16_t inv_1, uint8_t inv_2, uint32_t timeout_ms);

// Writes 8 discrete output signals (ENA_SHOOTING, GMEE, UOI, etc.)
FPGA_Status_t	FPGA_FCS_Set_Control_Outputs(FPGA_HandleTypeDef *hbridge, uint8_t fcs_control_val, uint32_t timeout_ms);

// Selects between Sample-and-Hold Latch (1) and Real-Time Combinatorial Bypass (0)
FPGA_Status_t	FPGA_FCS_Set_Latch_Enable(FPGA_HandleTypeDef *hbridge, bool enable, uint32_t timeout_ms);


/* ========================================================================= */
/*  INTERRUPT CONTROLLER APIS (Device ID = 3)                                */
/* ========================================================================= */

// Configures Interrupt Mask, Edge polarity, and Global Enable
FPGA_Status_t	FPGA_Int_Configure(FPGA_HandleTypeDef *hbridge, uint16_t mask, uint16_t edge_sel,
                                  bool global_enable, uint32_t timeout_ms);

// Reads active masked pending interrupt flags
FPGA_Status_t	FPGA_Int_Get_Pending(FPGA_HandleTypeDef *hbridge, uint16_t *pending, uint32_t timeout_ms);

// Clears pending interrupt flags using Write-1-to-Clear (W1C) strategy
FPGA_Status_t	FPGA_Int_Clear_Pending(FPGA_HandleTypeDef *hbridge, uint16_t clear_mask, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* FPGA_CONTROL_H */

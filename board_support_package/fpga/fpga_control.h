/**
  ******************************************************************************
  * @file    fpga_control.h
  * @brief   Header for high-level FPGA Module Control API.
  *          Provides simplified hardware management APIs over SPI Polling Bridge.
  *          All comments in English.
  ******************************************************************************
  */

#ifndef FPGA_CONTROL_H
#define FPGA_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32_2_fpga_spi_bridge.h"
#include "fpga_reg_map.h" // Includes reg map and bit definitions
#include <stdbool.h>



extern FPGA_HandleTypeDef hfpga_bridge;

/* ==================== DEBUG MODULE API ==================== */
/**
  * @brief  Verifies the SPI link by validating FPGA hardcoded verification constant.
  *         Constant is expected to match 0xDEAD.
  */
FPGA_Status_t FPGA_Debug_Verify(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms);

/**
  * @brief  Tests SPI read/write integrity by writing to and reading from FPGA Feedback register.
  */
FPGA_Status_t FPGA_Debug_Test_Echo(FPGA_HandleTypeDef *hbridge, uint16_t test_val, uint32_t timeout_ms);

/**
  * @brief  Controls the 3 physical status LEDs on the FPGA board.
  * @param  led_mask: Bitmask of LEDs to turn on (Bits [2:0] mapped to LEDs [2:0]).
  */
FPGA_Status_t FPGA_Debug_Set_LEDs(FPGA_HandleTypeDef *hbridge, uint8_t led_mask, uint32_t timeout_ms);

/**
  * @brief  Sets the programmable tick timer divider rate.
  *         This directly controls the update rate of the FCS sample-and-hold latch and STM32 IRQ generation.
  * @param  divider: 8-bit divider factor written to bits [11:4] of misc_reg.
  */
FPGA_Status_t FPGA_Debug_Set_Tick_Divider(FPGA_HandleTypeDef *hbridge, uint8_t divider, uint32_t timeout_ms);


/**
  * @brief  Directly controls Red, Yellow, and Green LEDs on the FPGA using boolean flags.
  * @param  r: State of Red LED (1 = On, 0 = Off, mapped to Bit 2).
  * @param  y: State of Yellow LED (1 = On, 0 = Off, mapped to Bit 1).
  * @param  g: State of Green LED (1 = On, 0 = Off, mapped to Bit 0).
  */
FPGA_Status_t FPGA_Debug_Write_LEDs(FPGA_HandleTypeDef *hbridge, bool r, bool y, bool g, uint32_t timeout_ms);

/**
  * @brief  Executes a non-blocking running lights sequence cycle (Red -> Yellow -> Green -> Blank).
  *         Designed to be polled frequently (2 ms to 100 ms).
  *         Advances state transition strictly every 200 ms using HAL ticks.
  */
FPGA_Status_t FPGA_Debug_Running_Lights(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms);

/* ==================== FCS MODULE API ==================== */
/**
  * @brief  Reads the complete processed input status from FCS module.
  * @param  status_1: Pointer to store lower 16 debounced/clean inputs.
  * @param  status_2: Pointer to store upper 15 clean inputs + JK Latch status at MSB.
  */
FPGA_Status_t FPGA_FCS_Read_Inputs(FPGA_HandleTypeDef *hbridge, uint16_t *status_1, uint16_t *status_2, uint32_t timeout_ms);

/**
  * @brief  Configures hard/soft override and sets custom software override values.
  */
FPGA_Status_t FPGA_FCS_Configure_Override(FPGA_HandleTypeDef *hbridge, uint16_t hard_soft_1, uint16_t hard_soft_2, uint16_t soft_val_1, uint16_t soft_val_2, uint32_t timeout_ms);

/**
  * @brief  Configures input hardware inversion masks on the PCB level.
  * @param  inv_1: Mask for signals [26:11].
  * @param  inv_2: Mask for signals [30:27].
  */
FPGA_Status_t FPGA_FCS_Configure_Inversions(FPGA_HandleTypeDef *hbridge, uint16_t inv_1, uint8_t inv_2, uint32_t timeout_ms);

/**
  * @brief  Writes 8 discrete output signals (ENA_SHOOTING, GMEE, UOI, etc.).
  */
FPGA_Status_t FPGA_FCS_Set_Control_Outputs(FPGA_HandleTypeDef *hbridge, uint8_t fcs_control_val, uint32_t timeout_ms);

/**
  * @brief  Configures the sample-and-hold latching state.
  * @param  enable: true to enable Sample-and-Hold (latched on 100Hz tick), false for Combinatorial Bypass (real-time).
  */
FPGA_Status_t FPGA_FCS_Set_Latch_Enable(FPGA_HandleTypeDef *hbridge, bool enable, uint32_t timeout_ms);


/* ==================== INTERRUPT CONTROLLER API ==================== */
/**
  * @brief  Configures the Interrupt Controller settings.
  * @param  mask: Bitmask to enable interrupts (1 = Enabled).
  * @param  edge_sel: Trigger edge selection (0 = Rising, 1 = Falling).
  * @param  global_enable: true to enable global interrupt controller.
  */
FPGA_Status_t FPGA_Int_Configure(FPGA_HandleTypeDef *hbridge, uint16_t mask, uint16_t edge_sel, bool global_enable, uint32_t timeout_ms);

/**
  * @brief  Reads active pending interrupts from controller.
  */
FPGA_Status_t FPGA_Int_Get_Pending(FPGA_HandleTypeDef *hbridge, uint16_t *pending, uint32_t timeout_ms);

/**
  * @brief  Clears pending interrupts using Write-1-to-Clear (W1C) strategy.
  */
FPGA_Status_t FPGA_Int_Clear_Pending(FPGA_HandleTypeDef *hbridge, uint16_t clear_mask, uint32_t timeout_ms);




#ifdef __cplusplus
}
#endif

#endif /* FPGA_CONTROL_H */

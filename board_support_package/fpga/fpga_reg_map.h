/**
  ******************************************************************************
  * @file    fpga_reg_map.h
  * @brief   FPGA Register Map Definitions for high-speed SPI Polling Bridge.
  *          10-bit Address Mapping: [9:6] Module ID (4 bits), [5:0] Reg Offset (6 bits).
  *          All comments in English.
  ******************************************************************************
  */

#ifndef FPGA_REG_MAP_H
#define FPGA_REG_MAP_H

#include <stdint.h>

/**
 * Bus Architecture Constants
 */
#define FPGA_S_ADDR_WIDTH        10    // Total address bus width
#define FPGA_S_DEV_ADDR_WIDTH    4     // Module ID (bits 9..6)
#define FPGA_S_CHIP_ADDR_WIDTH   6     // Register offset (bits 5..0)

/**
 * Address Calculation Helper Macro
 * Example: FPGA_S_ADDR(1, 0x02) -> 0x0042
 */
#define FPGA_S_ADDR(dev, reg)    ((((uint16_t)(dev) & 0x0F) << FPGA_S_CHIP_ADDR_WIDTH) | \
                                   ((uint16_t)(reg) & 0x3F))

/**
 * MODULE 1: DEBUG_MODULE (ID = 1)
 */
#define FPGA_S_DEV_DEBUG_ID      1

#define REG_S_DEBUG_FEEDBACK_OFF 0x00  // Echo register (R/W)
#define REG_S_DEBUG_CONST_OFF    0x01  // Validation constant register (Read-Only)
#define REG_S_DEBUG_MISC_OFF     0x02  // Miscellaneous control register (R/W)

#define ADDR_S_DEBUG_FEEDBACK    FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_FEEDBACK_OFF)
#define ADDR_S_DEBUG_CONST       FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_CONST_OFF)
#define ADDR_S_DEBUG_MISC        FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_MISC_OFF)

#define FPGA_DEBUG_CONST_VAL     0xDEAD // Validation constant defined in Verilog parameter

/* SPI Bus Integrity Checkerboard Test Pattern */
#define FPGA_DEBUG_ECHO_TEST_VAL 0xA55A

/**
 * REG_S_DEBUG_MISC Bit Definition (Mapped to misc_reg / debug_out in Verilog)
 *
 * Bits [2:0]  : LED Control (User status LEDs)
 *               Directly mapped to physical FPGA output pins: led[2:0]
 *               1 = LED On, 0 = LED Off
 *
 * Bit 3       : RS-485 / UART Multiplexer Channel Select
 *               0 = Channel 1 (Transceiver DD19: tx2, rx2, de2)
 *               1 = Channel 2 (Transceiver DD20: tx3, rx3, de3)
 *
 * Bits [11:4] : Programmable Tick Timer Divider value (debounce/hold clock rate)
 *               Controls the divisor period of the 1 ms reference tick clock.
 *               Generates 'tick_out_pulse' which controls:
 *                1. The latch rate (hold) of the FCS Sensor Module.
 *                2. The hardware IRQ trigger rate sent to STM32.
 *
 * Bits [15:12]: Reserved
 */
#define FPGA_DEBUG_MISC_LED_GREEN      0x01U // VD35 (Green) - Bit 0
#define FPGA_DEBUG_MISC_LED_YELLOW     0x02U // VD36 (Yellow) - Bit 1
#define FPGA_DEBUG_MISC_LED_RED        0x04U // VD37 (Red) - Bit 2
#define FPGA_DEBUG_MISC_LED_MASK       (FPGA_DEBUG_MISC_LED_GREEN | FPGA_DEBUG_MISC_LED_YELLOW | FPGA_DEBUG_MISC_LED_RED)

/* UART Multiplexer Bit 3 */
#define FPGA_DEBUG_MISC_UART_MUX_MASK  0x0008U // RS-485 Channel Select - Bit 3
#define FPGA_DEBUG_MISC_UART_MUX_SHIFT 3U

/* Programmable Tick Timer Divider Bits [11:4] */
#define FPGA_DEBUG_MISC_TICK_DIV_MASK  0x0FF0U
#define FPGA_DEBUG_MISC_TICK_DIV_SHIFT 4U

/**
 * MODULE 2: FCS_MODULE (ID = 2)
 */
#define FPGA_S_DEV_FCS_ID        2

#define REG_S_FCS_STATUS_1_OFF   0x00  // Lower 16 clean inputs (Read-Only)
#define REG_S_FCS_STATUS_2_OFF   0x01  // Upper 15 clean inputs + JK Flip-Flop at MSB (Read-Only)
#define REG_S_FCS_HARD_SOFT_1_OFF 0x10 // Hard/Soft override selection for inputs [15:0] (Write-Only)
#define REG_S_FCS_HARD_SOFT_2_OFF 0x11 // Hard/Soft override selection for inputs [30:16] (Write-Only)
#define REG_S_FCS_SOFT_VAL_1_OFF  0x12 // Software-injected values for inputs [15:0] (Write-Only)
#define REG_S_FCS_SOFT_VAL_2_OFF  0x13 // Software-injected values for inputs [30:16] (Write-Only)
#define REG_S_FCS_INV_1_OFF       0x14 // Inversion mask for physical inputs [26:11] (Write-Only)
#define REG_S_FCS_INV_2_OFF       0x15 // Inversion mask for physical inputs [30:27] (Write-Only)
#define REG_S_FCS_CONTROL_OFF     0x16 // Core Fire Control outputs (Write-Only)
#define REG_S_FCS_LATCH_EN_OFF    0x17 // Latch enable: 0 = Bypass, 1 = Sample-and-Hold (Write-Only)

#define ADDR_S_FCS_STATUS_1      FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_STATUS_1_OFF)
#define ADDR_S_FCS_STATUS_2      FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_STATUS_2_OFF)
#define ADDR_S_FCS_HARD_SOFT_1   FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_HARD_SOFT_1_OFF)
#define ADDR_S_FCS_HARD_SOFT_2   FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_HARD_SOFT_2_OFF)
#define ADDR_S_FCS_SOFT_VAL_1    FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_SOFT_VAL_1_OFF)
#define ADDR_S_FCS_SOFT_VAL_2    FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_SOFT_VAL_2_OFF)
#define ADDR_S_FCS_INV_1         FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_INV_1_OFF)
#define ADDR_S_FCS_INV_2         FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_INV_2_OFF)
#define ADDR_S_FCS_CONTROL       FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_CONTROL_OFF)
#define ADDR_S_FCS_LATCH_EN      FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_LATCH_EN_OFF)

/**
 * REG_S_FCS_STATUS_1 Bit Mask Mapping (Clean Debounced Inputs [15:0])
 *
 * Bits [10:0]  : DR Sensor 11-bit Bus (DR[10:0])
 * Bit 11       : HEF Clean Input
 * Bit 12       : APDS Clean Input
 * Bit 13       : HEAT Clean Input
 * Bit 14       : MG Clean Input
 * Bit 15       : GM Clean Input
 */
#define FPGA_FCS_STATUS1_DR_MASK       0x07FFU // Mask for 11-bit DR Sensor Bus
#define FPGA_FCS_STATUS1_HEF           0x0800U
#define FPGA_FCS_STATUS1_APDS          0x1000U
#define FPGA_FCS_STATUS1_HEAT          0x2000U
#define FPGA_FCS_STATUS1_MG            0x4000U
#define FPGA_FCS_STATUS1_GM            0x8000U

/**
 * REG_S_FCS_STATUS_2 Bit Mask Mapping (Clean Debounced Inputs [30:16] + JK Flip-Flop)
 *
 * Bit 0        : CC Clean Input
 * Bit 1        : DC Clean Input
 * Bit 2        : SET_R Clean Input (J Signal)
 * Bit 3        : RESET_R Clean Input (K Signal)
 * Bit 4        : BC_EN Clean Input
 * Bit 5        : RL Clean Input
 * Bit 6        : WS Clean Input
 * Bit 7        : PSCC Clean Input
 * Bit 8        : K1 Clean Input
 * Bit 9        : BTN_CANNON Clean Input
 * Bit 10       : RST_FILTR Clean Input
 * Bit 11       : UR Clean Input
 * Bit 12       : REM Clean Input
 * Bit 13       : SCF_ON Clean Input
 * Bit 14       : SCF_ON_ADD Clean Input
 * Bit 15       : FCS_JK_OUT (Reset-Dominant JK Flip-Flop Output at MSB)
 */
#define FPGA_FCS_STATUS2_CC            0x0001U
#define FPGA_FCS_STATUS2_DC            0x0002U
#define FPGA_FCS_STATUS2_SET_R         0x0004U
#define FPGA_FCS_STATUS2_RESET_R       0x0008U
#define FPGA_FCS_STATUS2_BC_EN         0x0010U
#define FPGA_FCS_STATUS2_RL            0x0020U
#define FPGA_FCS_STATUS2_WS            0x0040U
#define FPGA_FCS_STATUS2_PSCC          0x0080U
#define FPGA_FCS_STATUS2_K1            0x0100U
#define FPGA_FCS_STATUS2_BTN_CANNON    0x0200U
#define FPGA_FCS_STATUS2_RST_FILTR     0x0400U
#define FPGA_FCS_STATUS2_UR            0x0800U
#define FPGA_FCS_STATUS2_REM           0x1000U
#define FPGA_FCS_STATUS2_SCF_ON        0x2000U
#define FPGA_FCS_STATUS2_SCF_ON_ADD    0x4000U
#define FPGA_FCS_STATUS2_JK_OUT        0x8000U

/**
 * REG_S_FCS_CONTROL Bit Mask Mapping (Discrete Outputs from STM32 Write Register)
 *
 * Bit 0        : ENA_SHOOTING (Enable Shooting Command)
 * Bit 1        : GMEE (Missile Elevation Output)
 * Bit 2        : RANGE_OVER_1280 (Target Range > 1280m)
 * Bit 3        : UOI Signal Output
 * Bit 4        : INHIBIT_SHOOTING (Inhibit Shooting Command)
 * Bit 5        : WIND_SENSOR_ON (Enable Wind Sensor)
 * Bit 6        : RFU4 (Reserved for Future Use 4)
 * Bit 7        : RFU5 (Reserved for Future Use 5)
 */
#define FPGA_FCS_CONTROL_ENA_SHOOTING  0x01U
#define FPGA_FCS_CONTROL_GMEE          0x02U
#define FPGA_FCS_CONTROL_RANGE_1280    0x04U
#define FPGA_FCS_CONTROL_UOI           0x08U
#define FPGA_FCS_CONTROL_INHIBIT_SHOOT 0x10U
#define FPGA_FCS_CONTROL_WIND_SENS_ON  0x20U
#define FPGA_FCS_CONTROL_RFU4          0x40U
#define FPGA_FCS_CONTROL_RFU5          0x80U

/**
 * MODULE 3: INTERRUPT_CONTROLLER (ID = 3)
 */
#define FPGA_S_DEV_INT_CTRL_ID   3

#define REG_S_INT_PENDING_OFF    0x00  // Masked Pending Interrupt flags (Read-Only) / Clear Pending (Write-1-to-Clear)
#define REG_S_INT_MASK_OFF       0x01  // Interrupt Mask register (R/W): 1 = Enabled, 0 = Disabled
#define REG_S_INT_EDGE_SEL_OFF   0x02  // Edge Select register (R/W): 0 = Rising, 1 = Falling
#define REG_S_INT_CTRL_OFF       0x03  // Interrupt Global Control (R/W): Bit 0 = Global Enable

#define ADDR_S_INT_PENDING       FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_PENDING_OFF)
#define ADDR_S_INT_MASK          FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_MASK_OFF)
#define ADDR_S_INT_EDGE_SEL      FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_EDGE_SEL_OFF)
#define ADDR_S_INT_CTRL          FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_CTRL_OFF)

#endif /* FPGA_REG_MAP_H */

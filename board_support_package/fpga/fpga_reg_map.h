/**
  ******************************************************************************
  * @file    fpga_reg_map.h
  * @brief   FPGA Register Map Definitions for MSB Fire Control System.
  *          10-bit Address Mapping: [9:6] Module ID (4 bits), [5:0] Reg Offset (6 bits).
  *          Target: Xilinx Spartan-6 + STM32F411 (SPI Mode 00).
  *          All comments in pure ASCII English.
  ******************************************************************************
  */

#ifndef FPGA_REG_MAP_H
#define FPGA_REG_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ========================================================================= */
/*  1. BUS ARCHITECTURE & ADDRESSING MACROS                                  */
/* ========================================================================= */
#define FPGA_S_ADDR_WIDTH            10U   // Total address bus width (bits 9..0)
#define FPGA_S_DEV_ADDR_WIDTH        4U    // Module Device ID width (bits 9..6)
#define FPGA_S_CHIP_ADDR_WIDTH       6U    // Register Offset width (bits 5..0)

/**
 * @brief  Helper Macro to Calculate 10-bit SPI Address
 * @param  dev: Device ID (1..3, 4 bits)
 * @param  reg: Register Offset inside device (0..63, 6 bits)
 * @return 10-bit hardware address
 */
#define FPGA_S_ADDR(dev, reg)        ((((uint16_t)(dev) & 0x0FU) << FPGA_S_CHIP_ADDR_WIDTH) | \
                                       ((uint16_t)(reg) & 0x3FU))

/* ========================================================================= */
/*  2. MODULE 1: DEBUG & SYSTEM CONFIGURATION (ID = 1)                       */
/* ========================================================================= */
#define FPGA_S_DEV_DEBUG_ID          1U

#define REG_S_DEBUG_FEEDBACK_OFF     0x00U // [RW] Scratchpad loopback test register
#define REG_S_DEBUG_CONST_OFF        0x01U // [RO] Alive check validation constant (0xDEAD)
#define REG_S_DEBUG_MISC_OFF         0x02U // [RW] System misc control (LEDs, UART MUX, Timer Div)
#define REG_S_DEBUG_TP_MUX_OFF       0x03U // [RW] Dynamic Testpoint Multiplexer routing
#define REG_S_DEBUG_RECONFIG_OFF     0x04U // [WO] MultiBoot IPROG Reconfiguration Trigger

#define ADDR_S_DEBUG_FEEDBACK        FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_FEEDBACK_OFF)
#define ADDR_S_DEBUG_CONST           FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_CONST_OFF)
#define ADDR_S_DEBUG_MISC            FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_MISC_OFF)
#define ADDR_S_DEBUG_TP_MUX          FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_TP_MUX_OFF)
#define ADDR_S_DEBUG_RECONFIG        FPGA_S_ADDR(FPGA_S_DEV_DEBUG_ID, REG_S_DEBUG_RECONFIG_OFF)

#define FPGA_DEBUG_CONST_VAL         0xDEADU // Validation constant from Verilog
#define FPGA_DEBUG_ECHO_TEST_VAL     0x55AAU // SPI Bus Integrity Checkerboard Test Pattern
#define FPGA_DEBUG_RECONFIG_KEY      0xAA55U // Magic key to trigger internal IPROG reboot
#define FPGA_RECONFIG_MAGIC_KEY      FPGA_DEBUG_RECONFIG_KEY

/**
 * REG_S_DEBUG_MISC (0x02) Bit Definitions:
 *
 * Bits [2:0]  : LED Control (1 = LED On, 0 = LED Off)
 *               Bit 0: VD35 (Green)
 *               Bit 1: VD36 (Yellow)
 *               Bit 2: VD37 (Red)
 *
 * Bit 3       : RS-485 / UART Multiplexer Channel Select (from STM32 USART2)
 *               0 = Channel 1 (Transceiver DD19: tx2, rx2, de2)
 *               1 = Channel 2 (Transceiver DD20: tx3, rx3, de3)
 *
 * Bits [11:4] : Programmable Tick Timer Divider (ms period for Latch/Hold strobe)
 *               Default: 10 (10 ms = 100 Hz rate)
 *
 * Bits [15:12]: Reserved
 */
#define FPGA_DEBUG_MISC_LED_GREEN    0x0001U // VD35 (Green) - Bit 0
#define FPGA_DEBUG_MISC_LED_YELLOW   0x0002U // VD36 (Yellow) - Bit 1
#define FPGA_DEBUG_MISC_LED_RED      0x0004U // VD37 (Red) - Bit 2
#define FPGA_DEBUG_MISC_LED_MASK     (FPGA_DEBUG_MISC_LED_GREEN | FPGA_DEBUG_MISC_LED_YELLOW | FPGA_DEBUG_MISC_LED_RED)

#define FPGA_DEBUG_MISC_UART_MUX_DD19 0x0000U // Channel 1 (DD19)
#define FPGA_DEBUG_MISC_UART_MUX_DD20 0x0008U // Channel 2 (DD20)
#define FPGA_DEBUG_MISC_UART_MUX_MASK 0x0008U
#define FPGA_DEBUG_MISC_UART_MUX_SHIFT 3U

#define FPGA_DEBUG_MISC_TICK_DIV_MASK  0x0FF0U
#define FPGA_DEBUG_MISC_TICK_DIV_SHIFT 4U

/**
 * REG_S_DEBUG_TP_MUX (0x03) Bit Definitions (5 bits per Testpoint):
 *
 * Bits [4:0]   : Signal selector for TP5 (Physical Pin 81)
 * Bits [9:5]   : Signal selector for TP6 (Physical Pin 80)
 * Bits [14:10] : Signal selector for TP7 (Physical Pin 79)
 * Bit  15      : Reserved
 */
#define FPGA_TP_MUX_TP5_SHIFT        0U
#define FPGA_TP_MUX_TP5_MASK         (0x1FU << FPGA_TP_MUX_TP5_SHIFT)
#define FPGA_TP_MUX_TP6_SHIFT        5U
#define FPGA_TP_MUX_TP6_MASK         (0x1FU << FPGA_TP_MUX_TP6_SHIFT)
#define FPGA_TP_MUX_TP7_SHIFT        10U
#define FPGA_TP_MUX_TP7_MASK         (0x1FU << FPGA_TP_MUX_TP7_SHIFT)

/* ========================================================================= */
/*  3. MODULE 2: FCS MODULE - FIRE CONTROL SYSTEM (ID = 2)                   */
/* ========================================================================= */
#define FPGA_S_DEV_FCS_ID            2U

#define REG_S_FCS_STATUS_1_OFF       0x00U // [RO] Lower 16 clean inputs (DR[10:0] + HEF..GM)
#define REG_S_FCS_STATUS_2_OFF       0x01U // [RO] Upper 15 clean inputs (CC..SCF_ON_ADD) + JK at MSB
#define REG_S_FCS_HARD_SOFT_1_OFF    0x10U // [WO] Hard/Soft override for inputs [15:0]
#define REG_S_FCS_HARD_SOFT_2_OFF    0x11U // [WO] Hard/Soft override for inputs [30:16]
#define REG_S_FCS_SOFT_VAL_1_OFF     0x12U // [WO] Software virtual values for inputs [15:0]
#define REG_S_FCS_SOFT_VAL_2_OFF     0x13U // [WO] Software virtual values for inputs [30:16]
#define REG_S_FCS_INV_1_OFF          0x14U // [WO] Inversion mask for physical inputs [26:11] (Def: 0xFFE0)
#define REG_S_FCS_INV_2_OFF          0x15U // [WO] Inversion mask for physical inputs [30:27] (Def: 0x0F)
#define REG_S_FCS_CONTROL_OFF        0x16U // [WO] 8 Discrete Control Outputs
#define REG_S_FCS_LATCH_EN_OFF       0x17U // [WO] Latch enable: 0 = Bypass, 1 = Sample-and-Hold

#define ADDR_S_FCS_STATUS_1          FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_STATUS_1_OFF)
#define ADDR_S_FCS_STATUS_2          FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_STATUS_2_OFF)
#define ADDR_S_FCS_HARD_SOFT_1       FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_HARD_SOFT_1_OFF)
#define ADDR_S_FCS_HARD_SOFT_2       FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_HARD_SOFT_2_OFF)
#define ADDR_S_FCS_SOFT_VAL_1        FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_SOFT_VAL_1_OFF)
#define ADDR_S_FCS_SOFT_VAL_2        FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_SOFT_VAL_2_OFF)
#define ADDR_S_FCS_INV_1             FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_INV_1_OFF)
#define ADDR_S_FCS_INV_2             FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_INV_2_OFF)
#define ADDR_S_FCS_CONTROL           FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_CONTROL_OFF)
#define ADDR_S_FCS_LATCH_EN          FPGA_S_ADDR(FPGA_S_DEV_FCS_ID, REG_S_FCS_LATCH_EN_OFF)

/**
 * REG_S_FCS_STATUS_1 Bit Masks (Inputs [15:0])
 */
#define FPGA_FCS_STATUS1_DR_MASK     0x07FFU // Bits [10:0]: 11-bit DR Sensor Bus
#define FPGA_FCS_STATUS1_HEF         0x0800U // Bit 11
#define FPGA_FCS_STATUS1_APDS        0x1000U // Bit 12
#define FPGA_FCS_STATUS1_HEAT        0x2000U // Bit 13
#define FPGA_FCS_STATUS1_MG          0x4000U // Bit 14
#define FPGA_FCS_STATUS1_GM          0x8000U // Bit 15

/**
 * REG_S_FCS_STATUS_2 Bit Masks (Inputs [30:16] + JK Output)
 */
#define FPGA_FCS_STATUS2_CC          0x0001U // Bit 0  (Input 16)
#define FPGA_FCS_STATUS2_DC          0x0002U // Bit 1  (Input 17)
#define FPGA_FCS_STATUS2_SET_R       0x0004U // Bit 2  (Input 18 - J)
#define FPGA_FCS_STATUS2_RESET_R     0x0008U // Bit 3  (Input 19 - K)
#define FPGA_FCS_STATUS2_BC_EN       0x0010U // Bit 4  (Input 20)
#define FPGA_FCS_STATUS2_RL          0x0020U // Bit 5  (Input 21)
#define FPGA_FCS_STATUS2_WS          0x0040U // Bit 6  (Input 22)
#define FPGA_FCS_STATUS2_PSCC        0x0080U // Bit 7  (Input 23)
#define FPGA_FCS_STATUS2_K1          0x0100U // Bit 8  (Input 24)
#define FPGA_FCS_STATUS2_BTN_CANNON  0x0200U // Bit 9  (Input 25)
#define FPGA_FCS_STATUS2_RST_FILTR   0x0400U // Bit 10 (Input 26)
#define FPGA_FCS_STATUS2_UR          0x0800U // Bit 11 (Input 27)
#define FPGA_FCS_STATUS2_REM         0x1000U // Bit 12 (Input 28)
#define FPGA_FCS_STATUS2_SCF_ON      0x2000U // Bit 13 (Input 29)
#define FPGA_FCS_STATUS2_SCF_ON_ADD  0x4000U // Bit 14 (Input 30)
#define FPGA_FCS_STATUS2_JK_OUT      0x8000U // Bit 15 (Reset-Dominant JK Flip-Flop Output)

/**
 * REG_S_FCS_CONTROL Bit Masks (8 Discrete Relay/Transistor Outputs)
 */
#define FPGA_FCS_CONTROL_ENA_SHOOTING     0x01U // Bit 0: Enable Shooting
#define FPGA_FCS_CONTROL_ENA_SHOOT        FPGA_FCS_CONTROL_ENA_SHOOTING

#define FPGA_FCS_CONTROL_GMEE             0x02U // Bit 1: Missile Elevation Output

#define FPGA_FCS_CONTROL_RANGE_1280       0x04U // Bit 2: Target Range > 1280m
#define FPGA_FCS_CONTROL_RANGE_OVER_1280  FPGA_FCS_CONTROL_RANGE_1280

#define FPGA_FCS_CONTROL_UOI              0x08U // Bit 3: UOI Signal Output

#define FPGA_FCS_CONTROL_INHIBIT_SHOOT    0x10U // Bit 4: Inhibit Shooting Command
#define FPGA_FCS_CONTROL_INHIBIT_SHOOTING FPGA_FCS_CONTROL_INHIBIT_SHOOT
#define FPGA_FCS_CONTROL_INHIBIT_SHT      FPGA_FCS_CONTROL_INHIBIT_SHOOT

#define FPGA_FCS_CONTROL_WIND_SENS_ON     0x20U // Bit 5: Enable Wind Sensor
#define FPGA_FCS_CONTROL_WIND_SENSOR_ON   FPGA_FCS_CONTROL_WIND_SENS_ON
#define FPGA_FCS_CONTROL_WIND_SENS        FPGA_FCS_CONTROL_WIND_SENS_ON

#define FPGA_FCS_CONTROL_RFU4             0x40U // Bit 6: Reserved Output 4
#define FPGA_FCS_CONTROL_RFU5             0x80U // Bit 7: Reserved Output 5

/* ========================================================================= */
/*  4. MODULE 3: INTERRUPT CONTROLLER (ID = 3)                               */
/* ========================================================================= */
#define FPGA_S_DEV_INT_CTRL_ID       3U

#define REG_S_INT_PENDING_OFF        0x00U // [R] Masked Pending / [W1C] Clear Pending
#define REG_S_INT_MASK_OFF           0x01U // [RW] Mask (1 = Enabled, 0 = Disabled)
#define REG_S_INT_EDGE_SEL_OFF       0x02U // [RW] Edge Select (0 = Rising, 1 = Falling)
#define REG_S_INT_CTRL_OFF           0x03U // [RW] Global Control (Bit 0 = Global Enable)

#define ADDR_S_INT_PENDING           FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_PENDING_OFF)
#define ADDR_S_INT_MASK              FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_MASK_OFF)
#define ADDR_S_INT_EDGE_SEL          FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_EDGE_SEL_OFF)
#define ADDR_S_INT_CTRL              FPGA_S_ADDR(FPGA_S_DEV_INT_CTRL_ID, REG_S_INT_CTRL_OFF)

#define FPGA_INT_CTRL_GIE_ENABLE     0x0001U // Bit 0: Global Interrupt Enable

#ifdef __cplusplus
}
#endif

#endif /* FPGA_REG_MAP_H */

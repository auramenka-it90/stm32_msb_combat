/**
  ******************************************************************************
  * @file    fcs.h
  * @brief   High-level Fire Control System (FCS) State and Command Definitions.
  *          Provides convenient structured data representations (bool, uint16_t)
  *          aligned with the MSB-BC communication protocol.
  *          All comments in English.
  ******************************************************************************
  */

#ifndef FCS_H
#define FCS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board_support_package.h" // Provides access to hfpga_bridge handle and FPGA APIs
#include "fpga_control.h"          // ADDED: Provides definition of FPGA_Status_t and bridge APIs
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/*  FCS HARDWARE CONFIGURATION CONSTANTS                                     */
/* ========================================================================= */
#define FCS_CONFIG_TICK_DIVIDER        10U      // Tick divider factor (10 ms interval)
#define FCS_CONFIG_LATCH_ENABLE        false    // false = Combinatorial Bypass (Hold disabled)

// Hardware signal inversions (0 = No inversion, 1 = Inverted)
#define FCS_CONFIG_INV_MASK_1          0x0000U  // No inversions for physical inputs [26:11]
#define FCS_CONFIG_INV_MASK_2          0x00U    // No inversions for physical inputs [30:27]

// Input source selection (0 = Hardware/Pins, 1 = Software override)
#define FCS_CONFIG_HARD_SOFT_1         0x0000U  // All hard for inputs [15:0]
#define FCS_CONFIG_HARD_SOFT_2         0x0000U  // All hard for inputs [30:16]
#define FCS_CONFIG_SOFT_VAL_1          0x0000U  // Software injected values [15:0]
#define FCS_CONFIG_SOFT_VAL_2          0x0000U  // Software injected values [30:16]

// FPGA Interrupt Controller configurations
#define FCS_CONFIG_INT_MASK            0x0000U  // Disable all individual interrupts (No IRQ)
#define FCS_CONFIG_INT_EDGE_SEL        0x0000U  // Trigger edge selection (0 = Rising, 1 = Falling)
#define FCS_CONFIG_INT_GLOBAL_EN       false    // Disable Global Interrupt Controller

/**
 * @brief FCS Ammunition Types enum.
 *        Aligned with Byte 2 (Ammunition Type) mapping.
 */
typedef enum {
    FCS_AMMO_NONE = 0x0, // No ammunition / undetermined
    FCS_AMMO_APDS = 0x1, // APDS - Armor-Piercing Discarding Sabot
    FCS_AMMO_HEAT = 0x2, // HEAT - High-Explosive Anti-Tank
    FCS_AMMO_HEF  = 0x3, // HEF  - High-Explosive Fragmentation
    FCS_AMMO_MG   = 0x4, // MG   - Machine Gun
    FCS_AMMO_GM   = 0x5  // GM   - Guided Missile
} FCS_AmmoType_t;

/**
 * @brief Structured representation of the MSB to BC transmit state.
 *        Decoded from standard raw MSB packet layout.
 */
typedef struct {
    uint16_t        distance_meters;   // Target distance in meters (converted from 1 LSB = 5m)
    FCS_AmmoType_t  ammo_type;         // Decoded active ammunition type

    /* Flags Part 1 (Byte 3) */
    bool            cc;                // CC - Co-commander override (Double command)
    bool            dc;                // DC - Target designation command (ЦУ)
    bool            srd;               // SRD - Set/Reset Distance latch status (JK output)
    bool            bc_en;             // BC_EN - Ballistic Computer enable
    bool            rl;                // RL - Rocket Launch completed status
    bool            ws;                // WS - Wind Sensor allowed status
    bool            pscc;              // PSCC - Power Supply of Combination Circuit active

    /* Flags Part 2 (Byte 4) */
    bool            k1;                // K1 - Control status active
    bool            btn_cannon;        // BTN_CANNON - Cannon trigger button pressed
    bool            rf;                // RF - Reset Filters active
    bool            ur;                // UR - Sight unlatch status
    bool            rem;               // REM - Rocket elevation permission allowed
    bool            df;                // DF - Code DF status active
    bool            scf_on;            // Added Bit 6: SCF_ON status from hardware pin
    bool            scf_on_add;        // Added Bit 7: SCF_ON_ADD status from hardware pin

    /* Communication Link Quality Status */
    bool            is_link_error;     // True if SPI communication with the FPGA fails
} FCS_State_t;

/**
 * @brief Structured representation of the BC to MSB command packet.
 *        Decoded from standard incoming BC packet layout.
 */
typedef struct {
    bool            ena_shooting;      // ENA_SHOOTING - Shoot permission granted
    bool            gmee;              // GMEE - Rocket elevation enabled
    bool            range_over_1280;   // RANGE_OVER_1280 - Target range exceeds 1280m
    bool            uoi;               // UOI - UOI signal active
    bool            inhibit_shooting;  // INHIBIT_SHOOTING - Shoot command blocked
    bool            wind_sensor_on;    // WIND_SENSOR_ON - Wind sensor activated
} FCS_Control_Commands_t;

/* Global instantiation of the decoded FCS state and commands */
extern FCS_State_t fcs_state;
extern FCS_Control_Commands_t fcs_commands;

/* High-level sensor acquisition and processing tasks */
void fcs_get_sensors(void);
void fcs_set_sensors(void);
void fcs_task(void);

/* FPGA Modules Initialization wrapper */
void fcs_init_fpga_hardware(void);

/* Prototype for applying terminal override variables */
FPGA_Status_t fcs_apply_terminal_override(uint32_t hard_soft_mask, uint32_t soft_val_mask);

#ifdef __cplusplus
}
#endif

#endif /* FCS_H */

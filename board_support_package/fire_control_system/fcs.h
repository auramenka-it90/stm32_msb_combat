/**
 ******************************************************************************
 * @file    fcs.h
 * @brief   High-Level Fire Control System (FCS) State & Command Definitions.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef FCS_H
#define FCS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board_support_package.h"
#include "fpga_control.h"
#include "configuration.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/*  FCS TASK CONSTANTS                                                       */
/* ========================================================================= */

#define FCS_TICK_MS                    20U      // Periodic execution rate: 20 ms (50 Hz)

/* ========================================================================= */
/*  DATA STRUCTURES                                                          */
/* ========================================================================= */

/**
 * @brief FCS Ammunition Types (Aligned with MSB Packet Byte 2)
 */
typedef enum {
    FCS_AMMO_NONE = 0x0, // No ammunition / undetermined
    FCS_AMMO_APDS = 0x1, // APDS - Armor-Piercing Discarding Sabot (Б)
    FCS_AMMO_HEAT = 0x2, // HEAT - High-Explosive Anti-Tank (К)
    FCS_AMMO_HEF  = 0x3, // HEF  - High-Explosive Fragmentation (О)
    FCS_AMMO_MG   = 0x4, // MG   - Machine Gun (П)
    FCS_AMMO_GM   = 0x5  // GM   - Guided Missile (У)
} FCS_AmmoType_t;

/**
 * @brief Decoded FCS State (Transmitted in MSB -> BC 32-byte packet)
 */
typedef struct {
    uint16_t        distance_meters;   // Target distance in meters (1 LSB = 5m)
    FCS_AmmoType_t  ammo_type;         // Active ammunition type (0..5)

    /* Flags Part 1 (Byte 3) */
    bool            cc;                // CC - Co-commander override (Дубль)
    bool            dc;                // DC - Target designation command (ЦУ)
    bool            srd;               // SRD - Set/Reset Distance latch status (JK output)
    bool            bc_en;             // BC_EN - Ballistic Computer enable
    bool            rl;                // RL - Rocket Launch completed
    bool            ws;                // WS - Wind Sensor allowed
    bool            pscc;              // PSCC - Power Supply of Combination Circuit

    /* Flags Part 2 (Byte 4) */
    bool            k1;                // K1 - Control status
    bool            btn_cannon;        // BTN_CANNON - Cannon trigger button pressed
    bool            rf;                // RF - Reset Filters
    bool            ur;                // UR - Sight unlatch status
    bool            rem;               // REM - Rocket elevation permission
    bool            df;                // DF - Code DF status
    bool            scf_on;            // SCF_ON - Combination circuit active
    bool            scf_on_add;        // SCF_ON_ADD - Additional combination circuit

    /* Hardware Link Quality */
    bool            is_link_error;     // True if SPI communication with FPGA fails
} FCS_State_t;

/**
 * @brief Decoded Command State (Received in BC -> MSB packet)
 */
typedef struct {
    bool            ena_shooting;      // ENA_SHOOTING - Shoot permission granted
    bool            gmee;              // GMEE - Rocket elevation enabled
    bool            range_over_1280;   // RANGE_OVER_1280 - Target range > 1280m
    bool            uoi;               // UOI - UOI signal active
    bool            inhibit_shooting;  // INHIBIT_SHOOTING - Shoot command blocked
    bool            wind_sensor_on;    // WIND_SENSOR_ON - Wind sensor activated
} FCS_Control_Commands_t;

/* Global state and command handles */
extern FCS_State_t            fcs_state;
extern FCS_Control_Commands_t fcs_commands;

/* Public API Functions */
void			fcs_get_sensors(void);
void			fcs_set_sensors(void);
void			fcs_task(void);
void			fcs_init_fpga_hardware(void);
FPGA_Status_t	fcs_apply_terminal_override(uint32_t hard_soft_mask, uint32_t soft_val_mask);

#ifdef __cplusplus
}
#endif

#endif /* FCS_H */

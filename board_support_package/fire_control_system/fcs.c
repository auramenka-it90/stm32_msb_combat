/**
  ******************************************************************************
  * @file    fcs.c
  * @brief   FCS State Management Library Implementation.
  *          All comments in English.
  ******************************************************************************
  */

#include "fcs.h"
#include "fpga_control.h"
#include "cmsis_os.h"
#include "host.h" /* ADDED: Provides declarations for host_send_msb_packet() */


#define FCS_TICK_MS    10U

/* Global state and commands instantiation */
FCS_State_t fcs_state;
FCS_Control_Commands_t fcs_commands;

/* Acquires registers from FCS_MODULE, decodes states, and updates running lights */
void fcs_get_sensors(void){
    uint16_t status_1 = 0;
    uint16_t status_2 = 0;

    /* Read raw 16-bit register values from FPGA FCS_MODULE */
    if (FPGA_FCS_Read_Inputs(&hfpga_bridge, &status_1, &status_2, 100) == FPGA_OK) {

        // Clear error flag on successful transfer
        fcs_state.is_link_error = false;

        /* Decode Distance Bus (DR[10:0]): 1 LSB = 5 meters */
        fcs_state.distance_meters = (status_1 & FPGA_FCS_STATUS1_DR_MASK) * 5;

        /* Decode Active Ammunition Type from status_1 bits [15:11] */
        if ((status_1 & FPGA_FCS_STATUS1_APDS) != 0) {
            fcs_state.ammo_type = FCS_AMMO_APDS;
        } else if ((status_1 & FPGA_FCS_STATUS1_HEAT) != 0) {
            fcs_state.ammo_type = FCS_AMMO_HEAT;
        } else if ((status_1 & FPGA_FCS_STATUS1_HEF) != 0) {
            fcs_state.ammo_type = FCS_AMMO_HEF;
        } else if ((status_1 & FPGA_FCS_STATUS1_MG) != 0) {
            fcs_state.ammo_type = FCS_AMMO_MG;
        } else if ((status_1 & FPGA_FCS_STATUS1_GM) != 0) {
            fcs_state.ammo_type = FCS_AMMO_GM;
        } else {
            fcs_state.ammo_type = FCS_AMMO_NONE;
        }

        /* Decode Flags Part 1 (Byte 3 mapping) */
        fcs_state.cc    = (status_2 & FPGA_FCS_STATUS2_CC) != 0;
        fcs_state.dc    = (status_2 & FPGA_FCS_STATUS2_DC) != 0;
        fcs_state.srd   = (status_2 & FPGA_FCS_STATUS2_JK_OUT) != 0;
        fcs_state.bc_en = (status_2 & FPGA_FCS_STATUS2_BC_EN) != 0;
        fcs_state.rl    = (status_2 & FPGA_FCS_STATUS2_RL) != 0;
        fcs_state.ws    = (status_2 & FPGA_FCS_STATUS2_WS) != 0;
        fcs_state.pscc  = (status_2 & FPGA_FCS_STATUS2_PSCC) != 0;

        /* Decode Flags Part 2 (Byte 4 mapping) */
        fcs_state.k1         = (status_2 & FPGA_FCS_STATUS2_K1) != 0;
        fcs_state.btn_cannon = (status_2 & FPGA_FCS_STATUS2_BTN_CANNON) != 0;
        fcs_state.rf         = (status_2 & FPGA_FCS_STATUS2_RST_FILTR) != 0;
        fcs_state.ur         = (status_2 & FPGA_FCS_STATUS2_UR) != 0;
        fcs_state.rem        = (status_2 & FPGA_FCS_STATUS2_REM) != 0;
        fcs_state.df         = false; // Not mapped to physical input pin, gated to false
        fcs_state.scf_on     = (status_2 & FPGA_FCS_STATUS2_SCF_ON) != 0;
        fcs_state.scf_on_add = (status_2 & FPGA_FCS_STATUS2_SCF_ON_ADD) != 0;
    } else {
        /* Fail-Safe Handling: set safe defaults when hardware link is lost */
        fcs_state.is_link_error = true;
        fcs_state.distance_meters = 0;
        fcs_state.ammo_type = FCS_AMMO_NONE;

        fcs_state.cc = false;
        fcs_state.dc = false;
        fcs_state.srd = false;
        fcs_state.bc_en = false;
        fcs_state.rl = false;
        fcs_state.ws = false;
        fcs_state.pscc = false;
        fcs_state.k1 = false;
        fcs_state.btn_cannon = false;
        fcs_state.rf = false;
        fcs_state.ur = false;
        fcs_state.rem = false;
        fcs_state.df = false;
        fcs_state.scf_on = false;
        fcs_state.scf_on_add = false;
    }

    /* Keep running lights executing smoothly on every poll cycle */
    FPGA_Debug_Running_Lights(&hfpga_bridge, 100);
}

/* Packs high-level commands into a bitmask and writes them to the FPGA FCS control outputs */
void fcs_set_sensors(void){
    uint8_t ctrl_val = 0;

    /* Do not allow any commands to be written if there is an active link error */
    if (fcs_state.is_link_error) {
        return;
    }

    /* Map boolean command fields to the standard FPGA FCS control register bits */
    if (fcs_commands.ena_shooting) {
        ctrl_val |= FPGA_FCS_CONTROL_ENA_SHOOTING;
    }
    if (fcs_commands.gmee) {
        ctrl_val |= FPGA_FCS_CONTROL_GMEE;
    }
    if (fcs_commands.range_over_1280) {
        ctrl_val |= FPGA_FCS_CONTROL_RANGE_1280;
    }
    if (fcs_commands.uoi) {
        ctrl_val |= FPGA_FCS_CONTROL_UOI;
    }
    if (fcs_commands.inhibit_shooting) {
        ctrl_val |= FPGA_FCS_CONTROL_INHIBIT_SHOOT;
    }
    if (fcs_commands.wind_sensor_on) {
        ctrl_val |= FPGA_FCS_CONTROL_WIND_SENS_ON;
    }

    /* Write the updated bitmask to the FPGA register */
    FPGA_FCS_Set_Control_Outputs(&hfpga_bridge, ctrl_val, 100);
}

/* High-level structured periodic СУО execution loop */
void fcs_task(void) {
    static uint32_t last_wake_time = 0;
    static bool is_initialized = false;

    if (!is_initialized) {
        last_wake_time = osKernelGetTickCount();
        is_initialized = true;
    }

    /* 1. Fetch updated inputs from FPGA (SPI) */
    fcs_get_sensors();

    /* 2. Send state packet to Ballistic Computer (UART2, DMA, Non-Blocking) */
    host_send_msb_packet(&fcs_state);

    /* 3. Output current computed commands to FPGA (SPI) */
    fcs_set_sensors();

    /* 4. Strictly maintain periodic rate of 10 ms */
    last_wake_time += (FCS_TICK_MS * osKernelGetTickFreq()) / 1000U;
    osDelayUntil(last_wake_time);
}


/**
  * @brief  Initializes all FPGA hardware modules (Debug, FCS, and Interrupt Controller)
  *         using standard configuration values defined in fcs.h.
  *         Safely sets link error flag if any SPI step fails.
  */
void fcs_init_fpga_hardware(void) {
    FPGA_Status_t status = FPGA_OK;

    // 1. Initialize Tick Timer Divider in Debug Module (for hold and sample rates)
    status = FPGA_Debug_Set_Tick_Divider(&hfpga_bridge, FCS_CONFIG_TICK_DIVIDER, 100);
    if (status != FPGA_OK) goto init_error;

    // 2. Configure Signal Inversion masks
    status = FPGA_FCS_Configure_Inversions(&hfpga_bridge, FCS_CONFIG_INV_MASK_1, FCS_CONFIG_INV_MASK_2, 100);
    if (status != FPGA_OK) goto init_error;

    // 3. Configure Hardware/Software override multiplexers and preset software values
    status = FPGA_FCS_Configure_Override(&hfpga_bridge, FCS_CONFIG_HARD_SOFT_1, FCS_CONFIG_HARD_SOFT_2,
                                         FCS_CONFIG_SOFT_VAL_1, FCS_CONFIG_SOFT_VAL_2, 100);
    if (status != FPGA_OK) goto init_error;

    // 4. Set Latch Enable (Active Sample-and-Hold or Combinatorial Bypass mode)
    status = FPGA_FCS_Set_Latch_Enable(&hfpga_bridge, FCS_CONFIG_LATCH_ENABLE, 100);
    if (status != FPGA_OK) goto init_error;

    // 5. Clear any stale pending interrupts inside Interrupt Controller (W1C behavior)
    uint16_t pending_irqs = 0;
    status = FPGA_Int_Get_Pending(&hfpga_bridge, &pending_irqs, 100);
    if (status == FPGA_OK && pending_irqs != 0) {
        FPGA_Int_Clear_Pending(&hfpga_bridge, pending_irqs, 100);
    }

    // 6. Configure Interrupt Controller masks, edges and global enables
    status = FPGA_Int_Configure(&hfpga_bridge, FCS_CONFIG_INT_MASK, FCS_CONFIG_INT_EDGE_SEL,
                                FCS_CONFIG_INT_GLOBAL_EN, 100);
    if (status != FPGA_OK) goto init_error;

    // Everything initialized successfully
    fcs_state.is_link_error = false;
    return;

init_error:
    // Mark catastrophic bus failure state
    fcs_state.is_link_error = true;
}


/**
  * @brief  Applies 32-bit hardware/software override masks directly to FPGA Write-Only registers.
  *         Splits the 32-bit inputs into 16-bit register slices:
  *         - Lower 16 bits of hard_soft_mask -> REG_S_FCS_HARD_SOFT_1 (inputs [15:0])
  *         - Upper 15 bits of hard_soft_mask -> REG_S_FCS_HARD_SOFT_2 (inputs [30:16])
  *         - Lower 16 bits of soft_val_mask  -> REG_S_FCS_SOFT_VAL_1  (values [15:0])
  *         - Upper 15 bits of soft_val_mask  -> REG_S_FCS_SOFT_VAL_2  (values [30:16])
  * @param  hard_soft_mask: 32-bit selection mask (0 = Hardware Pin, 1 = Software Override)
  * @param  soft_val_mask: 32-bit software injected values mask
  * @retval FPGA_Status_t status of the SPI transaction (FPGA_OK if successful)
  */
FPGA_Status_t fcs_apply_terminal_override(uint32_t hard_soft_mask, uint32_t soft_val_mask) {

    // Split 32-bit Hard/Soft Selection Mask into two 16-bit registers
    uint16_t hard_soft_1 = (uint16_t)(hard_soft_mask & 0xFFFFU);
    uint16_t hard_soft_2 = (uint16_t)((hard_soft_mask >> 16) & 0x7FFFU); // Mask to 15 active bits [30:16]

    // Split 32-bit Software Values Mask into two 16-bit registers
    uint16_t soft_val_1 = (uint16_t)(soft_val_mask & 0xFFFFU);
    uint16_t soft_val_2 = (uint16_t)((soft_val_mask >> 16) & 0x7FFFU);  // Mask to 15 active bits [30:16]

    // Perform direct synchronous SPI writes to all 4 configuration registers
    // ADDR_S_FCS_HARD_SOFT_1, ADDR_S_FCS_HARD_SOFT_2, ADDR_S_FCS_SOFT_VAL_1, ADDR_S_FCS_SOFT_VAL_2
    return FPGA_FCS_Configure_Override(&hfpga_bridge,
                                        hard_soft_1,
                                        hard_soft_2,
                                        soft_val_1,
                                        soft_val_2,
                                        100);
}

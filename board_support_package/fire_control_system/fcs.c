/**
 ******************************************************************************
 * @file    fcs.c
 * @brief   FCS State Management & Periodic Task Implementation.
 *          Uses non-volatile configuration from dev_cfg (Sector 4).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "fcs.h"
#include "fpga_control.h"
#include "configuration.h"
#include "host.h"
#include "cmsis_os.h"

/* Global state instances */
FCS_State_t            fcs_state;
FCS_Control_Commands_t fcs_commands;

/* ========================================================================= */
/*  SENSOR ACQUISITION & CONTROL LOGIC                                       */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Reads 31 inputs from FPGA (Status 1 & 2) and decodes them into fcs_state
//-----------------------------------------------------------------------------
void	fcs_get_sensors(void){
	uint16_t status_1 = 0;
	uint16_t status_2 = 0;

	/* Read raw 16-bit registers from FPGA FCS_MODULE */
	if(FPGA_FCS_Read_Inputs(&hfpga_bridge, &status_1, &status_2, 100) == FPGA_OK){
		fcs_state.is_link_error = false;

		/* Decode Distance Bus (DR[10:0]): 1 LSB = 5 meters */
		fcs_state.distance_meters = (status_1 & FPGA_FCS_STATUS1_DR_MASK) * 5U;

		/* Decode Active Ammunition Type (Status 1 bits [15:11]) */
		if((status_1 & FPGA_FCS_STATUS1_APDS) != 0){
			fcs_state.ammo_type = FCS_AMMO_APDS;
		} else if((status_1 & FPGA_FCS_STATUS1_HEAT) != 0){
			fcs_state.ammo_type = FCS_AMMO_HEAT;
		} else if((status_1 & FPGA_FCS_STATUS1_HEF) != 0){
			fcs_state.ammo_type = FCS_AMMO_HEF;
		} else if((status_1 & FPGA_FCS_STATUS1_MG) != 0){
			fcs_state.ammo_type = FCS_AMMO_MG;
		} else if((status_1 & FPGA_FCS_STATUS1_GM) != 0){
			fcs_state.ammo_type = FCS_AMMO_GM;
		} else {
			fcs_state.ammo_type = FCS_AMMO_NONE;
		}

		/* Decode Flags Part 1 (Status 2 lower byte) */
		fcs_state.cc    = (status_2 & FPGA_FCS_STATUS2_CC) != 0;
		fcs_state.dc    = (status_2 & FPGA_FCS_STATUS2_DC) != 0;
		fcs_state.srd   = (status_2 & FPGA_FCS_STATUS2_JK_OUT) != 0; /* MSB: Hardware JK state */
		fcs_state.bc_en = (status_2 & FPGA_FCS_STATUS2_BC_EN) != 0;
		fcs_state.rl    = (status_2 & FPGA_FCS_STATUS2_RL) != 0;
		fcs_state.ws    = (status_2 & FPGA_FCS_STATUS2_WS) != 0;
		fcs_state.pscc  = (status_2 & FPGA_FCS_STATUS2_PSCC) != 0;

		/* Decode Flags Part 2 (Status 2 upper byte) */
		fcs_state.k1         = (status_2 & FPGA_FCS_STATUS2_K1) != 0;
		fcs_state.btn_cannon = (status_2 & FPGA_FCS_STATUS2_BTN_CANNON) != 0;
		fcs_state.rf         = (status_2 & FPGA_FCS_STATUS2_RST_FILTR) != 0;
		fcs_state.ur         = (status_2 & FPGA_FCS_STATUS2_UR) != 0;
		fcs_state.rem        = (status_2 & FPGA_FCS_STATUS2_REM) != 0;
		fcs_state.df         = false; // Not mapped to physical pin
		fcs_state.scf_on     = (status_2 & FPGA_FCS_STATUS2_SCF_ON) != 0;
		fcs_state.scf_on_add = (status_2 & FPGA_FCS_STATUS2_SCF_ON_ADD) != 0;
	} else {
		/* Fail-Safe Handling: Reset state upon SPI link failure */
		fcs_state.is_link_error   = true;
		fcs_state.distance_meters = 0;
		fcs_state.ammo_type       = FCS_AMMO_NONE;

		fcs_state.cc         = false;
		fcs_state.dc         = false;
		fcs_state.srd        = false;
		fcs_state.bc_en      = false;
		fcs_state.rl         = false;
		fcs_state.ws         = false;
		fcs_state.pscc       = false;
		fcs_state.k1         = false;
		fcs_state.btn_cannon = false;
		fcs_state.rf         = false;
		fcs_state.ur         = false;
		fcs_state.rem        = false;
		fcs_state.df         = false;
		fcs_state.scf_on     = false;
		fcs_state.scf_on_add = false;
	}

	/* Non-blocking LED running lights update */
	FPGA_Debug_Running_Lights(&hfpga_bridge, 100);
}

//-----------------------------------------------------------------------------
// Packs fcs_commands into bitmask and writes to FPGA discrete control outputs
//-----------------------------------------------------------------------------
void	fcs_set_sensors(void){
	uint8_t ctrl_val = 0;

	if(fcs_state.is_link_error){
		return;
	}

	if(fcs_commands.ena_shooting)    ctrl_val |= FPGA_FCS_CONTROL_ENA_SHOOTING;
	if(fcs_commands.gmee)            ctrl_val |= FPGA_FCS_CONTROL_GMEE;
	if(fcs_commands.range_over_1280) ctrl_val |= FPGA_FCS_CONTROL_RANGE_1280;
	if(fcs_commands.uoi)              ctrl_val |= FPGA_FCS_CONTROL_UOI;
	if(fcs_commands.inhibit_shooting)ctrl_val |= FPGA_FCS_CONTROL_INHIBIT_SHOOT;
	if(fcs_commands.wind_sensor_on)  ctrl_val |= FPGA_FCS_CONTROL_WIND_SENS_ON;

	FPGA_FCS_Set_Control_Outputs(&hfpga_bridge, ctrl_val, 100);
}

//-----------------------------------------------------------------------------
// High-level periodic FCS task execution loop (20 ms / 50 Hz rate)
//-----------------------------------------------------------------------------
void	fcs_task(void){
	static uint32_t last_wake_time = 0;
	static bool     is_initialized = false;

	if(!is_initialized){
		last_wake_time = osKernelGetTickCount();
		is_initialized = true;
	}

	/* 1. Fetch 100Hz-latched inputs from FPGA (SPI) */
	fcs_get_sensors();

	/* 2. Send 32-byte packet to Ballistic Computer (UART2 DMA, Non-Blocking) */
	host_send_msb_packet(&fcs_state);

	/* 3. Output current commands to FPGA outputs (SPI) */
	fcs_set_sensors();

	/* 4. Maintain exact periodic rate of 20 ms */
	last_wake_time += (FCS_TICK_MS * osKernelGetTickFreq()) / 1000U;
	osDelayUntil(last_wake_time);
}

//-----------------------------------------------------------------------------
// Initializes FPGA registers using parameters from Flash configuration (dev_cfg)
//-----------------------------------------------------------------------------
void	fcs_init_fpga_hardware(void){
	FPGA_Status_t status = FPGA_OK;

	// 1. Set UART Mux channel from dev_cfg (0 = DD19, 1 = DD20)
	status = FPGA_Debug_Set_UART_Mux(&hfpga_bridge,
	                                 dev_cfg.item.uart_mux ? FPGA_UART_PORT_2 : FPGA_UART_PORT_1,
	                                 100);
	if(status != FPGA_OK) goto init_error;

	// 2. Set Tick Timer Divider from dev_cfg (Default: 10 ms = 100 Hz rate)
	status = FPGA_Debug_Set_Tick_Divider(&hfpga_bridge, dev_cfg.item.latch_period_ms, 100);
	if(status != FPGA_OK) goto init_error;

	// 3. Configure Input Inversion masks from dev_cfg
	status = FPGA_FCS_Configure_Inversions(&hfpga_bridge, dev_cfg.item.fcs_inv_1, dev_cfg.item.fcs_inv_2, 100);
	if(status != FPGA_OK) goto init_error;

	// 4. Default Hardware/Software override: all physical (0x0000)
	status = FPGA_FCS_Configure_Override(&hfpga_bridge, 0x0000, 0x0000, 0x0000, 0x0000, 100);
	if(status != FPGA_OK) goto init_error;

	// 5. Always enable Sample-and-Hold Latch (100 Hz)
	status = FPGA_FCS_Set_Latch_Enable(&hfpga_bridge, true, 100);
	if(status != FPGA_OK) goto init_error;

	// 6. Clear any stale pending interrupts in FPGA
	uint16_t pending_irqs = 0;
	status = FPGA_Int_Get_Pending(&hfpga_bridge, &pending_irqs, 100);
	if(status == FPGA_OK && pending_irqs != 0){
		FPGA_Int_Clear_Pending(&hfpga_bridge, pending_irqs, 100);
	}

	// 7. Disable FPGA Interrupts (Polling task architecture is used)
	status = FPGA_Int_Configure(&hfpga_bridge, 0x0000, 0x0000, false, 100);
	if(status != FPGA_OK) goto init_error;

	fcs_state.is_link_error = false;
	return;

init_error:
	fcs_state.is_link_error = true;
}

//-----------------------------------------------------------------------------
// Applies 32-bit terminal override simulation masks over SPI
//-----------------------------------------------------------------------------
FPGA_Status_t	fcs_apply_terminal_override(uint32_t hard_soft_mask, uint32_t soft_val_mask){
	uint16_t hard_soft_1 = (uint16_t)(hard_soft_mask & 0xFFFFU);
	uint16_t hard_soft_2 = (uint16_t)((hard_soft_mask >> 16) & 0x7FFFU);

	uint16_t soft_val_1  = (uint16_t)(soft_val_mask & 0xFFFFU);
	uint16_t soft_val_2  = (uint16_t)((soft_val_mask >> 16) & 0x7FFFU);

	return FPGA_FCS_Configure_Override(&hfpga_bridge,
	                                   hard_soft_1,
	                                   hard_soft_2,
	                                   soft_val_1,
	                                   soft_val_2,
	                                   100);
}

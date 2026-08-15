/**
 ******************************************************************************
 * @file    terminal_signals.c
 * @brief   DSPA (eAssist) Terminal Telemetry Signals Tree & Live Handlers.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "dspa.h"
#include "dspa_defs.h"
#include "dspa_sigdefs.h"
#include "terminal_signals.h"
#include "fpga_control.h"

/* Telemetry tree hierarchy node strings */
static char *sDEV    = "";
static char *sCFG    = "";
static char *sFCSIN  = "";
static char *sFCSOUT = "";
static char *sDEBUG  = "";

/* 32-bit selection mask: 0 = Hardware Pin, 1 = Software Override */
uint32_t terminal_override_mask = 0x00000000U;

/* 32-bit software values mask: 0 = Logic Low, 1 = Logic High */
uint32_t terminal_override_values = 0x00000000U;

/* ========================================================================= */
/*  DSPA TELEMETRY TREE DEFINITION                                           */
/* ========================================================================= */

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("MSB_Combat", sDEV, NULL),
		_U32_R_	("Test hardware (0=OK)", test_hardware_result, &sDEV),

		/* --- CONFIGURATION & PASSPORT SUBTREE --- */
		_STRING_R_	("Config (Sector 4)", sCFG, &sDEV),
			_U64_R_ ("Serial number",           dev_cfg.item.serial, &sCFG),
			_BOOL_RW_("RS-485 MUX (0=P1, 1=P2)",dev_cfg.item.uart_mux, &sCFG),
			_BYTE_RW_("Latch period [ms]",      dev_cfg.item.latch_period_ms, &sCFG),
			_U16_RW_("Inv Mask 1 (11..26)",     dev_cfg.item.fcs_inv_1, &sCFG),
			_BYTE_RW_("Inv Mask 2 (27..30)",    dev_cfg.item.fcs_inv_2, &sCFG),

		/* --- FCS INPUTS SUBTREE --- */
		_STRING_R_	("FCS inputs", sFCSIN, &sDEV),
			_U16_R_ ("distance [m]",            fcs_state.distance_meters, &sFCSIN),
			_BYTE_R_("ammo type (0..5)",        fcs_state.ammo_type, &sFCSIN),
			_BOOL_R_("CC (Double)",             fcs_state.cc, &sFCSIN),
			_BOOL_R_("DC (Target designation)", fcs_state.dc, &sFCSIN),
			_BOOL_R_("SRD (JK latch output)",   fcs_state.srd, &sFCSIN),
			_BOOL_R_("BC_EN (BC enable)",       fcs_state.bc_en, &sFCSIN),
			_BOOL_R_("RL (Rocket Launch)",      fcs_state.rl, &sFCSIN),
			_BOOL_R_("WS (Wind Sensor)",        fcs_state.ws, &sFCSIN),
			_BOOL_R_("PSCC (Power Supply)",     fcs_state.pscc, &sFCSIN),
			_BOOL_R_("K1",                      fcs_state.k1, &sFCSIN),
			_BOOL_R_("BTN_CANNON",              fcs_state.btn_cannon, &sFCSIN),
			_BOOL_R_("RF (Reset Filters)",      fcs_state.rf, &sFCSIN),
			_BOOL_R_("UR (Sight unlatch)",      fcs_state.ur, &sFCSIN),
			_BOOL_R_("REM (Rocket elevation)",  fcs_state.rem, &sFCSIN),
			_BOOL_R_("DF",                      fcs_state.df, &sFCSIN),
			_BOOL_R_("SCF_ON",                  fcs_state.scf_on, &sFCSIN),
			_BOOL_R_("SCF_ON_ADD",              fcs_state.scf_on_add, &sFCSIN),
			_U32_RW_("MASK_H0/S1",              terminal_override_mask, &sFCSIN),
			_U32_RW_("VALUE_S",                 terminal_override_values, &sFCSIN),

		/* --- FCS OUTPUTS SUBTREE --- */
		_STRING_R_	("FCS outputs", sFCSOUT, &sDEV),
			_BOOL_R_("ENA_SHOOT (Permission)",  fcs_commands.ena_shooting, &sFCSOUT),
			_BOOL_R_("GMEE (Rocket elevation)", fcs_commands.gmee, &sFCSOUT),
			_BOOL_R_("RANG_OVER_1280",          fcs_commands.range_over_1280, &sFCSOUT),
			_BOOL_R_("UOI",                     fcs_commands.uoi, &sFCSOUT),
			_BOOL_R_("INH_SHOOT (Blocked)",     fcs_commands.inhibit_shooting, &sFCSOUT),
			_BOOL_R_("WS_ON (Wind sensor on)",  fcs_commands.wind_sensor_on, &sFCSOUT),

		/* --- DIAGNOSTICS & TELEMETRY SUBTREE --- */
		_STRING_R_	("Diagnostics", sDEBUG, &sDEV),
			_FLOAT_R_("adc voltage [V]",        adc_voltage, &sDEBUG),
			_FLOAT_R_("core temperature [degC]",cpu_temperature, &sDEBUG),
			_U32_RW_ ("fcs task counter",       fcs_task_counter, &sDEBUG),
			_U32_R_  ("host tx count",          host_stats.tx_count, &sDEBUG),
			_U32_R_  ("host rx count",          host_stats.rx_count, &sDEBUG),
			_U32_R_  ("host rx crc err",        host_stats.rx_crc_err, &sDEBUG),
			_U32_R_  ("host rx xor err",        host_stats.rx_xor_err, &sDEBUG),
SIGNALS_END(DSPA_SIGNALS_NAME)

/* ========================================================================= */
/*  PUBLIC API IMPLEMENTATION                                                */
/* ========================================================================= */

// Initializes DSPA signal registry
int		init_terminal_signals(void){
	return SIG_INIT(DSPA_SIGNALS_NAME);
}

// Live handler for parameter updates from eAssist PC utility with strict input sanitation
void	signal_change_handler(void *s){

	// 1. Simulation Override masks (Strictly clamp to 31 active hardware inputs)
	if((s == &terminal_override_mask) || (s == &terminal_override_values)){
		terminal_override_mask   &= 0x7FFFFFFFU; // Clear unused bit 31
		terminal_override_values &= 0x7FFFFFFFU; // Clear unused bit 31
		fcs_apply_terminal_override(terminal_override_mask, terminal_override_values);
	}

	// 2. RS-485 Multiplexer channel changed (Sanitize boolean 0/1)
	if(s == &dev_cfg.item.uart_mux){
		dev_cfg.item.uart_mux = dev_cfg.item.uart_mux ? true : false;
		FPGA_Debug_Set_UART_Mux(&hfpga_bridge,
		                        dev_cfg.item.uart_mux ? FPGA_UART_PORT_2 : FPGA_UART_PORT_1,
		                        100);
	}

	// 3. FCS Latch Period changed (Guards against zero and overflow)
	if(s == &dev_cfg.item.latch_period_ms){
		if(dev_cfg.item.latch_period_ms == 0){
			dev_cfg.item.latch_period_ms = 10; // Защита: не даем выключить таймер (ставим 100 Гц)
		} else if(dev_cfg.item.latch_period_ms > 250){
			dev_cfg.item.latch_period_ms = 250; // Ограничение аппаратного 8-битного делителя
		}
		FPGA_Debug_Set_Tick_Divider(&hfpga_bridge, dev_cfg.item.latch_period_ms, 100);
	}

	// 4. Input Inversion masks changed (Mask 2 strictly clamped to 4 bits)
	if((s == &dev_cfg.item.fcs_inv_1) || (s == &dev_cfg.item.fcs_inv_2)){
		dev_cfg.item.fcs_inv_2 &= 0x0FU; // Для сигналов 27..30 валидны только биты 0..3
		FPGA_FCS_Configure_Inversions(&hfpga_bridge, dev_cfg.item.fcs_inv_1, dev_cfg.item.fcs_inv_2, 100);
	}
}

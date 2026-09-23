/**
 ******************************************************************************
 * @file    terminal_signals.c
 * @brief   DSPA (eAssist) Terminal Telemetry Signals Tree & Live Handlers.
 *          Human-readable hierarchy with lowercase, detailed signal descriptions.
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
static char *sTP     = "";

/* Testpoint multiplexer channel selectors (0..31) */
uint8_t tp5_mux_sel = 0;   /* TP5 (P81): 0=100Hz, 1=50MHz, 2=1kHz, 5=NSS, 6=WR, 7=RD, 8=TX, 9=RX... */
uint8_t tp6_mux_sel = 0;   /* TP6 (P80): 0=1kHz,  1=50MHz, 2=1kHz, 5=NSS, 6=WR, 7=RD, 8=TX, 9=RX... */
uint8_t tp7_mux_sel = 0;   /* TP7 (P79): 0=Reset, 1=50MHz, 2=1kHz, 4=IRQ, 5=NSS, 8=TX, 9=RX... */

/* 32-bit selection mask: 0 = Hardware Pin, 1 = Software Override */
uint32_t terminal_override_mask = 0x00000000U;

/* 32-bit software values mask: 0 = Logic Low, 1 = Logic High */
uint32_t terminal_override_values = 0x00000000U;

/* ========================================================================= */
/*  DSPA TELEMETRY TREE DEFINITION                                           */
/* ========================================================================= */

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)

	_STRING_R_  ("Mode switching board (MSB)", sDEV, NULL),
		_U32_R_	("hardware health status (0=OK)", test_hardware_result, &sDEV),

		/* --- 1. CONFIGURATION & NON-VOLATILE PASSPORT --- */
		_STRING_R_	("Device configuration (Sector 7)", sCFG, &sDEV),
			_U64_R_   ("board hardware serial number",      dev_cfg.item.serial, &sCFG),
			_BOOL_RW_ ("rs485 channel select (0=DD19, 1=DD20)", dev_cfg.item.uart_mux, &sCFG),
			_BYTE_RW_ ("sensor latch timer period [ms]",    dev_cfg.item.latch_period_ms, &sCFG),
			_U16_RW_  ("input inversion mask 1 (bits 11..26) [hex]", dev_cfg.item.fcs_inv_1, &sCFG),
			_BYTE_RW_ ("input inversion mask 2 (bits 27..30) [hex]", dev_cfg.item.fcs_inv_2, &sCFG),

		/* --- 2. FCS DISCRETE INPUT SIGNALS --- */
		_STRING_R_	("FCS discrete inputs (FPGA)", sFCSIN, &sDEV),
			_U16_R_   ("target range distance [m]",         fcs_state.distance_meters, &sFCSIN),
			_BYTE_R_  ("active ammo type index (0..5)",     fcs_state.ammo_type, &sFCSIN),
			_BOOL_R_  ("cc - commander control (double)",   fcs_state.cc, &sFCSIN),
			_BOOL_R_  ("dc - target designation (CU)",      fcs_state.dc, &sFCSIN),
			_BOOL_R_  ("srd - distance latch (JK output)",  fcs_state.srd, &sFCSIN),
			_BOOL_R_  ("bc_en - ballistic computer permit", fcs_state.bc_en, &sFCSIN),
			_BOOL_R_  ("rl - rocket launch active",         fcs_state.rl, &sFCSIN),
			_BOOL_R_  ("ws - wind sensor signal valid",     fcs_state.ws, &sFCSIN),
			_BOOL_R_  ("pscc - power supply circuit OK",    fcs_state.pscc, &sFCSIN),
			_BOOL_R_  ("k1 - valve K1 discrete state",      fcs_state.k1, &sFCSIN),
			_BOOL_R_  ("btn_cannon - cannon trigger button",fcs_state.btn_cannon, &sFCSIN),
			_BOOL_R_  ("rf - reset input filters",          fcs_state.rf, &sFCSIN),
			_BOOL_R_  ("ur - sight unlatch command",        fcs_state.ur, &sFCSIN),
			_BOOL_R_  ("rem - rocket elevation mechanism",  fcs_state.rem, &sFCSIN),
			_BOOL_R_  ("df - diagnostic flag",              fcs_state.df, &sFCSIN),
			_BOOL_R_  ("scf_on - stabilization power ON",   fcs_state.scf_on, &sFCSIN),
			_BOOL_R_  ("scf_on_add - additional power ON",  fcs_state.scf_on_add, &sFCSIN),
			_U32_RW_  ("simulation mode mask (0=HW, 1=Soft) [hex]", terminal_override_mask, &sFCSIN),
			_U32_RW_  ("simulation soft values (0=Low, 1=High) [hex]", terminal_override_values, &sFCSIN),

		/* --- 3. FCS DISCRETE CONTROL OUTPUTS --- */
		_STRING_R_	("FCS discrete outputs (Relays)", sFCSOUT, &sDEV),
			_BOOL_R_  ("enable shooting permission",       fcs_commands.ena_shooting, &sFCSOUT),
			_BOOL_R_  ("gmee - missile elevation permit",   fcs_commands.gmee, &sFCSOUT),
			_BOOL_R_  ("range over 1280m flag",             fcs_commands.range_over_1280, &sFCSOUT),
			_BOOL_R_  ("uoi - optical index active",        fcs_commands.uoi, &sFCSOUT),
			_BOOL_R_  ("inhibit shooting (interlock)",      fcs_commands.inhibit_shooting, &sFCSOUT),
			_BOOL_R_  ("wind sensor power enable",          fcs_commands.wind_sensor_on, &sFCSOUT),

		/* --- 4. DIAGNOSTICS & TELEMETRY --- */
		_STRING_R_	("Diagnostics & telemetry", sDEBUG, &sDEV),
			_FLOAT_R_ ("mcu supply vdda [V]",               real_vref, &sDEBUG),
			_FLOAT_R_ ("sensor diode voltage [V]",          adc_voltage, &sDEBUG),
			_FLOAT_R_ ("cpu core temperature [°C]",         cpu_temperature, &sDEBUG),
			_U32_RW_  ("fcs periodic task counter",         fcs_task_counter, &sDEBUG),
			_U32_R_   ("host uart tx packet count",         host_stats.tx_count, &sDEBUG),
			_U32_R_   ("host uart rx packet count",         host_stats.rx_count, &sDEBUG),
			_U32_R_   ("host uart rx crc error count",      host_stats.rx_crc_err, &sDEBUG),
			_U32_R_   ("host uart rx xor error count",      host_stats.rx_xor_err, &sDEBUG),

			/* --- 4.1. TESTPOINT MULTIPLEXER SUBTREE --- */
			_STRING_R_	("Testpoint multiplexer (FPGA)", sTP, &sDEBUG),
				_BYTE_RW_ ("tp5 channel selector (P81, 0..31)", tp5_mux_sel, &sTP),
				_BYTE_RW_ ("tp6 channel selector (P80, 0..31)", tp6_mux_sel, &sTP),
				_BYTE_RW_ ("tp7 channel selector (P79, 0..31)", tp7_mux_sel, &sTP),

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
			dev_cfg.item.latch_period_ms = 10; // Guard: minimum 100 Hz latch rate
		} else if(dev_cfg.item.latch_period_ms > 250){
			dev_cfg.item.latch_period_ms = 250; // Guard: hardware 8-bit limit
		}
		FPGA_Debug_Set_Tick_Divider(&hfpga_bridge, dev_cfg.item.latch_period_ms, 100);
	}

	// 4. Input Inversion masks changed (Mask 2 strictly clamped to 4 bits)
	if((s == &dev_cfg.item.fcs_inv_1) || (s == &dev_cfg.item.fcs_inv_2)){
		dev_cfg.item.fcs_inv_2 &= 0x0FU; // Only bits 0..3 are valid for signals 27..30
		FPGA_FCS_Configure_Inversions(&hfpga_bridge, dev_cfg.item.fcs_inv_1, dev_cfg.item.fcs_inv_2, 100);
	}

	// 5. Dynamic Testpoint Multiplexer routing (TP5, TP6, TP7)
	if((s == &tp5_mux_sel) || (s == &tp6_mux_sel) || (s == &tp7_mux_sel)){
		if(tp5_mux_sel > 31) tp5_mux_sel = 31;
		if(tp6_mux_sel > 31) tp6_mux_sel = 31;
		if(tp7_mux_sel > 31) tp7_mux_sel = 31;

		FPGA_Debug_Set_TP_Mux(&hfpga_bridge, tp5_mux_sel, tp6_mux_sel, tp7_mux_sel, 100);
	}
}

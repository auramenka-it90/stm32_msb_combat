
#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"

#include	"terminal_signals.h"
#include 	"fcs.h"

static	char *sDEV = "";
static 	char *sFCSIN = "";
static 	char *sFCSOUT = "";
static	char *sDEBUG  = "";

// 32-bit selection mask: 0 = Hardware Pin, 1 = Software Override
uint32_t terminal_override_mask = 0x00000000U;

// 32-bit software values mask: 0 = Logic Low, 1 = Logic High
uint32_t terminal_override_values = 0x00000000U;


SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("FCS", sDEV, NULL),
		_U32_R_	("Test hardware",	test_hardware_result,&sDEV),
		_STRING_R_	("FCS input ", sFCSIN, &sDEV),
			_U16_R_("distance[m]", fcs_state.distance_meters, &sFCSIN),
			_BYTE_R_("ammo type", fcs_state.ammo_type, &sFCSIN),
			_BOOL_R_("CC(Double )", fcs_state.cc, &sFCSIN),
			_BOOL_R_("DC(Target designation)", fcs_state.dc, &sFCSIN),
			_BOOL_R_("SRD(Set/Reset Distance)", fcs_state.srd, &sFCSIN),
			_BOOL_R_("BC_EN(BC enable)", fcs_state.bc_en, &sFCSIN),
			_BOOL_R_("RL(Rocket Launch)", fcs_state.rl, &sFCSIN),
			_BOOL_R_("WS(Wind Sensor)", fcs_state.ws, &sFCSIN),
			_BOOL_R_("PSCC(Power Supply)", fcs_state.pscc, &sFCSIN),
			_BOOL_R_("K1", fcs_state.k1, &sFCSIN),
			_BOOL_R_("BTN_CANNON(trigger btn. pressed)", fcs_state.btn_cannon, &sFCSIN),
			_BOOL_R_("RF(Reset Filters)", fcs_state.rf, &sFCSIN),
			_BOOL_R_("UR(Sight unlatch)", fcs_state.ur, &sFCSIN),
			_BOOL_R_("REM(Rocket elevation)", fcs_state.rem, &sFCSIN),
			_BOOL_R_("DF", fcs_state.df, &sFCSIN),
			_BOOL_R_("FCS_ON", fcs_state.scf_on, &sFCSIN),
			_BOOL_R_("FCS_ON+", fcs_state.scf_on_add, &sFCSIN),
			_U32_RW_("MASK_H0/S1",terminal_override_mask, &sFCSIN),
			_U32_RW_("VALUE_S",terminal_override_values, &sFCSIN),
		_STRING_R_	("FCS output ", sFCSOUT, &sDEV),
			_BOOL_R_("ENA_SHOOT(Shoot permission)", fcs_commands.ena_shooting, &sFCSOUT),
			_BOOL_R_("GMEE(Rocket elevation)", fcs_commands.gmee, &sFCSOUT),
			_BOOL_R_("RANG_OVER_1280", fcs_commands.range_over_1280, &sFCSOUT),
			_BOOL_R_("UOI", fcs_commands.uoi, &sFCSOUT),
			_BOOL_R_("INH_SHOOT(Shoot blocked)", fcs_commands.inhibit_shooting, &sFCSOUT),
			_BOOL_R_("WS_ON(Wind sensor activated)", fcs_commands.wind_sensor_on, &sFCSOUT),
		_STRING_R_	("Debug", sDEBUG, &sDEV),
			_FLOAT_R_("adc voltage[v]", adc_voltage, &sDEBUG),
			_FLOAT_R_("board temperature[°C]", cpu_temperature, &sDEBUG),
			_U32_RW_("fcs task counter", fcs_task_counter, &sDEBUG),
			_U32_R_ ("host tx count", host_stats.tx_count, &sDEBUG),     /* Отправлено пакетов на BC */
			_U32_R_ ("host rx count", host_stats.rx_count, &sDEBUG),     /* Хороших пакетов принято от BC */
			_U32_R_ ("host rx crc err", host_stats.rx_crc_err, &sDEBUG), /* Ошибки CRC (помехи на шине) */
			_U32_R_ ("host rx xor err", host_stats.rx_xor_err, &sDEBUG), /* Ошибки XOR (помехи на шине) */
SIGNALS_END(DSPA_SIGNALS_NAME)





int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


/*
 * 	Reaction to changes in FPGA control signals (eAssist)
 */
void signal_change_handler(void *s) {
	if ((s==&terminal_override_mask)|| (s==&terminal_override_values)){
		fcs_apply_terminal_override(terminal_override_mask, terminal_override_values);
	}

}

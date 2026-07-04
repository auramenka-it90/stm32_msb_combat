
#ifndef TERMINAL_SIGNALS_H_
#define TERMINAL_SIGNALS_H_

#include	"board_support_package.h"
#include 	"fcs.h"
#include 	"host.h"

#define		DSPA_SIGNALS_NAME		dspa


//	test hardware
extern	uint32_t	test_hardware_result;




//	ADC
extern	float adc_voltage;
extern	float	cpu_temperature;

//	FCS counterr
extern 	uint32_t fcs_task_counter;

int	init_terminal_signals(void);

#endif /* TERMINAL_SIGNALS_H_ */

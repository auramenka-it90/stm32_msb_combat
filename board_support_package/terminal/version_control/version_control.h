/**
 ******************************************************************************
 * @file    version_control.h
 * @brief   Software and Hardware Version Information for MSB Board.
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef VERSION_CONTROL_H_
#define VERSION_CONTROL_H_

#include "board_support_package.h"

/* ========================================================================= */
/*  STATIC STRING DEFINITIONS (MSB BOARD ONLY)                               */
/* ========================================================================= */

#define _VC_PRODUCT_NUMBER_     "Mode switching board (MSB)\n"
#define _VC_BOARD_NUMBER_       "Board:  DIAS.901.02.00.200\n"
#define _VC_CHIP_DESIGNATION_   "Chip:   DD16 - STM32F411RET6TR\n"
#define _VC_SOFT_NAME_          "Software: combat\n"
#define _VC_SOFT_NOTE_          "Produced by mr. Andrew Auramenka \n"
#define _VC_SOFT_VERSION_       "Ver. 2.0\n"

/* ========================================================================= */
/*  PUBLIC API                                                               */
/* ========================================================================= */

// Returns pointer to static device info string stored in Flash (.rodata)
const char*	get_device_info(void);

#endif /* VERSION_CONTROL_H_ */

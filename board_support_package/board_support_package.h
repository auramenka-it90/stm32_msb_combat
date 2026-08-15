/**
 ******************************************************************************
 * @file    board_support_package.h
 * @brief   Board Support Package (BSP) global configuration registry (MainAppl).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef BOARD_SUPPORT_PACKAGE_H
#define BOARD_SUPPORT_PACKAGE_H

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "main.h"
#include "cmsis_os2.h"
#include "usefull_define.h"

/* ========================================================================= */
/*  STM32 HARDWARE PERIPHERAL HANDLES                                        */
/* ========================================================================= */

/* SPI handle for STM32 <-> FPGA communication */
extern SPI_HandleTypeDef hspi1;

/* UART1 handle for KPA / RS-485 connector and DMA channels */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* UART6 handle for ST-Link Virtual COM port and DMA channels */
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef  hdma_usart6_rx;
extern DMA_HandleTypeDef  hdma_usart6_tx;

/* UART2 handle for Ballistic Computer (BC) link via FPGA */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_rx;
extern DMA_HandleTypeDef  hdma_usart2_tx;

/* ADC handle for CPU temperature and analog measurements */
extern ADC_HandleTypeDef  hadc1;

/* Hardware CRC peripheral handle */
#ifdef HAL_CRC_MODULE_ENABLED
extern CRC_HandleTypeDef  hcrc;
#endif

/* ========================================================================= */
/*  SYSTEM HARDWARE DIAGNOSTIC BITMASKS                                      */
/* ========================================================================= */
#define _B_TEST_HARDWARE_SUCCESS_   0x00000000U
#define _B_FAULT_OS_                B0  /* System OS startup fault */
#define _B_FAULT_PINS_              B1  /* Pin configuration fault */
#define _B_FAULT_TERMINAL_          B2  /* Terminal / Host UART fault */
#define _B_FAULT_CFG_               B3  /* Configuration parameters fault */
#define _B_FAULT_RESERVED_          B4  /* Reserved */
#define _B_FAULT_FPGA_              B5  /* FPGA boot / SPI communication fault */
#define _B_FAULT_HOST_UART_         B6  /* BC UART2 communication fault */

/* ========================================================================= */
/*  GLOBAL BSP FUNCTION PROTOTYPES                                           */
/* ========================================================================= */

/* Power-On Self Test and System APIs */
uint32_t	init_hardware(void);
bool		test_status_hardware(uint32_t module);
uint32_t	get_status_hardware(void);
bool		get_rcc_csr(void);
void		bsp_system_reset(void);

/* Transceiver Direction Control */
void		ten(bool par);

/* Internal Temperature Measurements */
float		Read_Temperature(void);

/* DWT Microsecond Blocking Delay */
void		DWT_Init(void);
void		DWT_DeInit(void);
void		delay_us(const uint32_t us);

#endif /* BOARD_SUPPORT_PACKAGE_H */

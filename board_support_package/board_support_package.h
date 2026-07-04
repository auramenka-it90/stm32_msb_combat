/**
 * ******************************************************************************
 * @file    board_support_package.h
 * @brief   Clean Board Support Package (BSP) global configuration registry.
 *          Excludes redundant DMA handles and legacy headers.
 *          All comments in English.
 * ******************************************************************************
 */

#ifndef BOARD_SUPPORT_PACKAGE_H
#define BOARD_SUPPORT_PACKAGE_H

/* Standard library includes */
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Device-specific and RTOS includes */
#include "main.h"
#include "cmsis_os2.h" /* Always use RTOS v2 */
#include "usefull_define.h"



/* ========================================================================= */
/*  STM32 HARDWARE PERIPHERAL HANDLES                                        */
/* ========================================================================= */

/* SPI handle for STM32 <-> FPGA external memory communication */
extern SPI_HandleTypeDef hspi1;

/* UART handle for STM32 <-> 221V34F26 connector and its DMA channels */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* UART handle for STM32 <-> ST-Link Virtual COM port and its DMA channels */
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef  hdma_usart6_rx;
extern DMA_HandleTypeDef  hdma_usart6_tx;

/* UART handle for STM32 <-> FPGA(fcs)  */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* ADC handle for CPU temperature and analog measurements */
extern ADC_HandleTypeDef  hadc1;

/* Hardware CRC peripheral handle for fast firmware check (Added here!) */
#ifdef HAL_CRC_MODULE_ENABLED
extern CRC_HandleTypeDef  hcrc;
#endif

/* ========================================================================= */
/*  SYSTEM HARDWARE DIAGNOSTIC BITMASKS                                      */
/* ========================================================================= */
#define _B_TEST_HARDWARE_SUCCESS_   0x00000000U
#define _B_FAULT_OS_                B0  /* System OS startup fault */
#define _B_FAULT_PINS_              B1  /* Pin configuration fault */
#define _B_FAULT_TERMINAL_          B2  /* host UART communication fault */
#define _B_FAULT_CFG_               B3  /* Configuration parameters fault */
#define _B_FAULT_W25Q128_           B4  /* W25Q128 memory access fault */
#define _B_FAULT_FPGA_				B5
#define _B_FAULT_HOST_UART_			B6


/* ========================================================================= */
/*  GLOBAL BSP MODULE FUNCTION PROTOTYPES                                    */
/* ========================================================================= */

/* Hardware and System Power-On Self Test APIs */
uint32_t init_hardware(void);
bool     test_status_hardware(uint32_t module);
uint32_t get_status_hardware(void);
bool     get_rcc_csr(void);
void     bsp_system_reset(void);


/* Internal Analog Measurement APIs */
float    Read_Temperature(void);

/* Hardware Microsecond Delay APIs (DWT-based) */
void     DWT_Init(void);
void     DWT_DeInit(void);
void     delay_us(const uint32_t us);



#endif /* BOARD_SUPPORT_PACKAGE_H */


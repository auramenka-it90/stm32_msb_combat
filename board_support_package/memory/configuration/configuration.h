/**
 * @file    configuration.h
 * @brief   System configuration and Flash memory manager for STM32F411
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif


#include "board_support_package.h"

/* ========================================================================= */
/* FLASH MEMORY SETTINGS (STM32F411 Sector 4: 0x08010000 - 64KB)             */
/* ========================================================================= */

#define CFG_ADDRESS         0x08010000
#define CFG_SECTOR          FLASH_SECTOR_4
#define CFG_START_ADDRESS   CFG_ADDRESS

/* * We allocate a smaller logical page size for our config
 * to speed up RAM buffering, even though the physical sector is 64KB.
 */
#define CFG_PAGE_SIZE       1024
#define CFG_NUM_PAGE        1
#define CFG_SIZE            (CFG_PAGE_SIZE * CFG_NUM_PAGE)

#define CLEAR_FLAG          __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)

/* ========================================================================= */
/* CONFIGURATION DATA STRUCTURES                                             */
/* ========================================================================= */

#define CFG_HEADER          0x55AAF073

/*
 * Device configuration structure
 */
typedef struct {
    uint32_t    header;         /* Configuration header validator */
    uint16_t    boot_counter;   /* Power-on counter */

    /* TODO: Add more variables here (e.g., FreeRTOS specific parameters) */

} t_dev_cfg;

/*
 * Object interface for board configuration memory image
 */
typedef union {
    t_dev_cfg   item;
    uint8_t     u8[CFG_SIZE];
    uint16_t    u16[CFG_SIZE / 2];
} t_memory_cfg;

/* ========================================================================= */
/* EXPORTED VARIABLES                                                        */
/* ========================================================================= */

extern t_memory_cfg dev_cfg;

/* ========================================================================= */
/* API FUNCTION PROTOTYPES                                                   */
/* ========================================================================= */

/**
 * @brief   Read configuration from Flash and validate
 * @retval  true if successful, false if formatted/defaulted
 */
bool get_dev_cfg(void);

/**
 * @brief   Get the initialization status of the configuration
 * @retval  true if ready, false otherwise
 */
bool get_dev_status(void);

/**
 * @brief   Save current configuration to Flash memory
 * @retval  true if successful, false otherwise
 */
bool save_setting(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGURATION_H */

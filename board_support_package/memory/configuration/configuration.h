/**
 ******************************************************************************
 * @file    configuration.h
 * @brief   Non-Volatile System Configuration Manager with Passport Metadata.
 *          Target MCU: STM32F411 (Sector 4: 0x08010000).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board_support_package.h"

/* ========================================================================= */
/*  FLASH MEMORY SETTINGS (Sector 4: 0x08010000 - 64 KB)                     */
/* ========================================================================= */

#define CFG_ADDRESS         0x08010000U
#define CFG_SECTOR          FLASH_SECTOR_4
#define CFG_START_ADDRESS   CFG_ADDRESS

#define CFG_PAGE_SIZE       1024U
#define CFG_NUM_PAGE        1U
#define CFG_SIZE            (CFG_PAGE_SIZE * CFG_NUM_PAGE)

#define CLEAR_FLAG          __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP   | \
                                                   FLASH_FLAG_OPERR | \
                                                   FLASH_FLAG_WRPERR| \
                                                   FLASH_FLAG_PGAERR| \
                                                   FLASH_FLAG_PGPERR| \
                                                   FLASH_FLAG_PGSERR)

/* ========================================================================= */
/*  METADATA & BOARD IDENTIFIERS                                             */
/* ========================================================================= */

#define CFG_HEADER              0x060865U
#define CFG_BOARD_SERIAL_NUMBER 0ULL
#define CFG_STR                 "Mode Switching Board (MSB) produced by Andrew Avramenko ( 2026)"

/* ========================================================================= */
/*  CONFIGURATION DATA STRUCTURES                                            */
/* ========================================================================= */

/* Main device parameters and passport structure */
typedef struct __attribute__((packed)) {
    uint32_t    header;             /* Validation magic word (0x060865) */
    uint64_t    serial;             /* Board hardware 64-bit serial number */
    char        str[256];           /* Board description & build passport string */

    /* Hardware Settings */
    bool        uart_mux;           /* RS-485 Mux: false = Port 1 (DD19), true = Port 2 (DD20) */
    uint8_t     latch_period_ms;    /* FCS Latch period in ms (Default: 10 for 100 Hz) */

    /* Hardware Inversion Masks */
    uint16_t    fcs_inv_1;          /* Inversion mask for inputs 11..26 (Default: 0x0000) */
    uint8_t     fcs_inv_2;          /* Inversion mask for inputs 27..30 (Default: 0x00) */

    /* Reserved space for future expansion */
    uint8_t     reserved[64];

} t_dev_cfg;

/* Memory image union (1024 bytes total, checksum at word index 511) */
typedef union {
    t_dev_cfg   item;
    uint8_t     u8[CFG_SIZE];
    uint16_t    u16[CFG_SIZE / 2];
} t_memory_cfg;

/* ========================================================================= */
/*  EXPORTED VARIABLES & APIS                                                */
/* ========================================================================= */

extern t_memory_cfg dev_cfg;

bool	get_dev_cfg(void);
bool	get_dev_status(void);
bool	save_setting(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGURATION_H */

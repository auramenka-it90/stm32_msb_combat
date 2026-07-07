/**
 * @file    configuration.c
 * @brief   System configuration and Flash memory manager for STM32F411
 */

#include "configuration.h"

/* ========================================================================= */
/* PRIVATE VARIABLES                                                         */
/* ========================================================================= */

/* Global configuration instance */
t_memory_cfg dev_cfg;
static bool dev_cfg_ready = false;

/* ========================================================================= */
/* PRIVATE FUNCTION PROTOTYPES                                               */
/* ========================================================================= */

static uint16_t calc_sum(uint16_t *data, uint32_t len);
static bool __eeprom_memory_write(uint8_t *dst, uint32_t addr);
static void eeprom_memory_read(uint8_t *dst, const uint8_t *src, uint32_t len);
static bool eeprom_memory_erase(uint32_t sector);
static bool eeprom_memory_write_page(uint8_t *dst, uint32_t addr);

/* ========================================================================= */
/* API FUNCTION IMPLEMENTATIONS                                              */
/* ========================================================================= */

/**
 * @brief   Read configuration from Flash, validate, and increment boot counter
 * @retval  true if successful, false if formatted/defaulted
 */
bool get_dev_cfg(void) {
    bool result = true;
    uint32_t i = 0;

    do {
        eeprom_memory_read(dev_cfg.u8, (uint8_t*)CFG_START_ADDRESS, CFG_SIZE);

        /* Validate header and checksum */
        result = (dev_cfg.item.header == CFG_HEADER);
        result &= (dev_cfg.u16[CFG_SIZE / 2 - 1] == calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1));

        i++;
        HAL_Delay(5);

    } while ((i < 5) && (result == false));

    /* Format configuration memory if validation fails */
    if (!result) {
        /* Clear memory to avoid garbage data */
        memset(&dev_cfg, 0, sizeof(t_memory_cfg));

        dev_cfg.item.header = CFG_HEADER;
        dev_cfg.item.boot_counter = 1; /* Initial boot */

        /* Calculate checksum and save defaults */
        dev_cfg.u16[CFG_SIZE / 2 - 1] = calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1);

        result = eeprom_memory_erase(CFG_SECTOR);
        result &= __eeprom_memory_write(dev_cfg.u8, CFG_START_ADDRESS);
        eeprom_memory_read(dev_cfg.u8, (uint8_t*)CFG_START_ADDRESS, CFG_SIZE);
    } else {
        /* Configuration is valid: increment boot counter and update Flash */
        dev_cfg.item.boot_counter++;
        result = save_setting();
    }

    dev_cfg_ready = result;
    return result;
}

/**
 * @brief   Get the initialization status of the configuration
 * @retval  true if ready, false otherwise
 */
bool get_dev_status(void) {
    return dev_cfg_ready;
}

/**
 * @brief   Save current configuration to Flash memory
 * @retval  true if successful, false otherwise
 */
bool save_setting(void) {
    bool result = true;

    /* Recalculate checksum before saving */
    dev_cfg.u16[CFG_SIZE / 2 - 1] = calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1);

    result = eeprom_memory_erase(CFG_SECTOR);
    result &= __eeprom_memory_write(dev_cfg.u8, CFG_START_ADDRESS);

    return result;
}

/* ========================================================================= */
/* PRIVATE FUNCTION IMPLEMENTATIONS                                          */
/* ========================================================================= */

/**
 * @brief   Read raw memory pages from Flash
 */
static void eeprom_memory_read(uint8_t *dst, const uint8_t *src, uint32_t len) {
    memcpy(dst, src, len);
}

/**
 * @brief   Erase specific memory sector using STM32 HAL
 */
static bool eeprom_memory_erase(uint32_t sector) {
    bool result = true;
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    __disable_irq();

    result &= (HAL_FLASH_Unlock() == HAL_OK);
    if (!result) {
        goto finish;
    }

    CLEAR_FLAG;

    /* HAL implementation for STM32F4 sector erase */
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1;

    result &= (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK);

finish:
    result &= (HAL_FLASH_Lock() == HAL_OK);
    __enable_irq();

    return result;
}

/**
 * @brief   Write single page to memory byte-by-byte
 */
static bool eeprom_memory_write_page(uint8_t *dst, uint32_t addr) {
    bool result = true;
    uint8_t *pdata = dst;

    __disable_irq();

    result &= (HAL_FLASH_Unlock() == HAL_OK);
    if (!result) {
        goto finish;
    }

    for (uint32_t i = 0; i < CFG_PAGE_SIZE; i++) {
        CLEAR_FLAG;
        result &= (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, *pdata) == HAL_OK);
        addr += 1;
        pdata++;
        if (!result) {
            goto finish;
        }
    }

finish:
    result &= (HAL_FLASH_Lock() == HAL_OK);
    __enable_irq();

    return result;
}

/**
 * @brief   Write all required memory pages to Flash
 */
static bool __eeprom_memory_write(uint8_t *dst, uint32_t addr) {
    uint32_t i = 0;
    uint32_t address = addr;
    bool result = true;

    for (i = 0; i < CFG_NUM_PAGE; i++) {
        result &= eeprom_memory_write_page(dst, address);
        dst += CFG_PAGE_SIZE;
        address += CFG_PAGE_SIZE;
    }
    return result;
}

/**
 * @brief   Calculate check sum
 */
static uint16_t calc_sum(uint16_t *data, uint32_t len) {
    uint32_t cs = 0;
    uint32_t i;
    uint16_t *dataptr = data;
    uint32_t carry;

    for (i = 0; i < len; i++) {
        cs += *dataptr;
        dataptr++;
    }

    do {
        carry = cs >> 16;
        cs = (cs & 0xFFFF) + carry;
    } while (carry != 0);

    return ~cs;
}



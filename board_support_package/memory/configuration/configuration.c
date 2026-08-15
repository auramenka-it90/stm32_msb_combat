/**
 ******************************************************************************
 * @file    configuration.c
 * @brief   Non-Volatile System Configuration Implementation (Sector 4).
 *          All comments in ASCII English.
 ******************************************************************************
 */

#include "configuration.h"
#include <string.h>

/* ========================================================================= */
/*  PRIVATE VARIABLES                                                        */
/* ========================================================================= */

/* Global configuration instance */
t_memory_cfg dev_cfg;
static bool  dev_cfg_ready = false;

/* Private function prototypes */
static uint16_t	calc_sum(uint16_t *data, uint32_t len);
static bool		__eeprom_memory_write(uint8_t *dst, uint32_t addr);
static void		eeprom_memory_read(uint8_t *dst, const uint8_t *src, uint32_t len);
static bool		eeprom_memory_erase(uint32_t sector);
static bool		eeprom_memory_write_page(uint8_t *dst, uint32_t addr);

/* ========================================================================= */
/*  PUBLIC API IMPLEMENTATION                                                */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Reads config from Flash, validates checksum; formats with defaults on error
//-----------------------------------------------------------------------------
bool	get_dev_cfg(void){
	bool result = true;
	uint32_t i = 0;

	do {
		eeprom_memory_read(dev_cfg.u8, (uint8_t*)CFG_START_ADDRESS, CFG_SIZE);

		/* Validate header and 16-bit tail checksum */
		result = (dev_cfg.item.header == CFG_HEADER);
		result &= (dev_cfg.u16[CFG_SIZE / 2 - 1] == calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1));

		i++;
		HAL_Delay(5);

	} while((i < 5) && (result == false));

	/* Format Flash with safe factory defaults if unprogrammed or corrupted */
	if(!result){
		memset(&dev_cfg, 0, sizeof(t_memory_cfg));

		/* Factory Passport & Defaults */
		dev_cfg.item.header          = CFG_HEADER;
		dev_cfg.item.serial          = CFG_BOARD_SERIAL_NUMBER;
		strncpy(dev_cfg.item.str, CFG_STR, sizeof(dev_cfg.item.str) - 1);
		dev_cfg.item.str[sizeof(dev_cfg.item.str) - 1] = '\0';

		dev_cfg.item.uart_mux        = false;  /* Default: RS-485 Port 1 (DD19) */
		dev_cfg.item.latch_period_ms = 10;     /* Default: 10 ms (100 Hz rate) */
		dev_cfg.item.fcs_inv_1       = 0x0000; /* Default: No input inversions */
		dev_cfg.item.fcs_inv_2       = 0x00;

		/* Calculate checksum and save to Flash */
		dev_cfg.u16[CFG_SIZE / 2 - 1] = calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1);

		result = eeprom_memory_erase(CFG_SECTOR);
		result &= __eeprom_memory_write(dev_cfg.u8, CFG_START_ADDRESS);
		eeprom_memory_read(dev_cfg.u8, (uint8_t*)CFG_START_ADDRESS, CFG_SIZE);
	}

	dev_cfg_ready = result;
	return result;
}

//-----------------------------------------------------------------------------
// Returns configuration readiness flag
//-----------------------------------------------------------------------------
bool	get_dev_status(void){
	return dev_cfg_ready;
}

//-----------------------------------------------------------------------------
// Explicitly saves current RAM configuration to Flash memory (Zero boot wear)
//-----------------------------------------------------------------------------
bool	save_setting(void){
	bool result = true;

	/* Recalculate checksum before writing */
	dev_cfg.u16[CFG_SIZE / 2 - 1] = calc_sum(dev_cfg.u16, CFG_SIZE / 2 - 1);

	result = eeprom_memory_erase(CFG_SECTOR);
	result &= __eeprom_memory_write(dev_cfg.u8, CFG_START_ADDRESS);

	return result;
}

/* ========================================================================= */
/*  PRIVATE FLASH LOW-LEVEL DRIVER                                           */
/* ========================================================================= */

//-----------------------------------------------------------------------------
// Reads raw byte buffer from Flash address space
//-----------------------------------------------------------------------------
static void	eeprom_memory_read(uint8_t *dst, const uint8_t *src, uint32_t len){
	memcpy(dst, src, len);
}

//-----------------------------------------------------------------------------
// Erases single 64KB Flash Sector 4
//-----------------------------------------------------------------------------
static bool	eeprom_memory_erase(uint32_t sector){
	bool result = true;
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SectorError = 0;

	__disable_irq();

	result &= (HAL_FLASH_Unlock() == HAL_OK);
	if(!result) goto finish;

	CLEAR_FLAG;

	EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	EraseInitStruct.Sector       = sector;
	EraseInitStruct.NbSectors    = 1;

	result &= (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK);

finish:
	result &= (HAL_FLASH_Lock() == HAL_OK);
	__enable_irq();

	return result;
}

//-----------------------------------------------------------------------------
// Writes single page byte-by-byte into Flash memory
//-----------------------------------------------------------------------------
static bool	eeprom_memory_write_page(uint8_t *dst, uint32_t addr){
	bool result = true;
	uint8_t *pdata = dst;

	__disable_irq();

	result &= (HAL_FLASH_Unlock() == HAL_OK);
	if(!result) goto finish;

	for(uint32_t i = 0; i < CFG_PAGE_SIZE; i++){
		CLEAR_FLAG;
		result &= (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, *pdata) == HAL_OK);
		addr += 1;
		pdata++;
		if(!result) goto finish;
	}

finish:
	result &= (HAL_FLASH_Lock() == HAL_OK);
	__enable_irq();

	return result;
}

//-----------------------------------------------------------------------------
// Writes all required pages to Flash memory
//-----------------------------------------------------------------------------
static bool	__eeprom_memory_write(uint8_t *dst, uint32_t addr){
	uint32_t address = addr;
	bool result = true;

	for(uint32_t i = 0; i < CFG_NUM_PAGE; i++){
		result &= eeprom_memory_write_page(dst, address);
		dst     += CFG_PAGE_SIZE;
		address += CFG_PAGE_SIZE;
	}
	return result;
}

//-----------------------------------------------------------------------------
// Computes 16-bit 1's complement checksum
//-----------------------------------------------------------------------------
static uint16_t	calc_sum(uint16_t *data, uint32_t len){
	uint32_t cs = 0;
	uint16_t *dataptr = data;

	for(uint32_t i = 0; i < len; i++){
		cs += *dataptr++;
	}

	uint32_t carry;
	do {
		carry = cs >> 16;
		cs    = (cs & 0xFFFF) + carry;
	} while(carry != 0);

	return ~cs;
}

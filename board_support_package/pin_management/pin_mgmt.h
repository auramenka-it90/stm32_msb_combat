/**
 ******************************************************************************
 * @file    pin_mgmt.h
 * @brief   Header for STM32 Pin Management Driver (MainAppl).
 *          All comments in English.
 ******************************************************************************
 */

#ifndef __PIN_MGMT_H
#define __PIN_MGMT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board_support_package.h"

/* Pin descriptor structure */
typedef struct {
    GPIO_TypeDef* port;          /*!< GPIO Port (GPIOA, GPIOB, etc.) */
    uint16_t pin;                /*!< Pin Mask (GPIO_PIN_0, etc.) */
    uint8_t default_state;       /*!< Default Level (0: LOW, 1: HIGH) */
    const char* name;            /*!< Pin Name for diagnostics */
} Pin_Descriptor_t;

/* SPI bus operational modes */
typedef enum {
    SPI_BUS_AF_MODE = 0,         /*!< Active SPI1 Master mode for STM32 */
    SPI_BUS_HIZ_MODE             /*!< Tri-State (Analog mode) during FPGA boot */
} SPI_Bus_Mode_t;

typedef struct {
    uint8_t debug_enabled;
} Pin_Mgmt_Config_t;

#define PIN_MGMT_CONFIG_DEFAULT { .debug_enabled = 0 }

#define PIN_DESC(_port, _pin, _def_state, _name) \
    { .port = _port, .pin = _pin, .default_state = _def_state, .name = _name }

extern Pin_Mgmt_Config_t bsp_pin_config;

/* Initialization */
osStatus_t	PIN_MGMT_Init(Pin_Mgmt_Config_t *config);
osStatus_t	PIN_MGMT_DeInit(void);

/* Standard operations (Mutex protected) */
osStatus_t	PIN_Set(const Pin_Descriptor_t *pin);
osStatus_t	PIN_Reset(const Pin_Descriptor_t *pin);
osStatus_t	PIN_Toggle(const Pin_Descriptor_t *pin);
uint8_t		PIN_Read(const Pin_Descriptor_t *pin);

/* Fast atomic operations (No mutex, direct BSRR / IDR registers) */
osStatus_t	PIN_Set_F(const Pin_Descriptor_t *pin);
osStatus_t	PIN_Reset_F(const Pin_Descriptor_t *pin);
osStatus_t	PIN_Toggle_F(const Pin_Descriptor_t *pin);
uint8_t		PIN_Read_F(const Pin_Descriptor_t *pin);

/* GPIO Mutex controls */
osStatus_t	PIN_GPIO_Mutex_Acquire(uint32_t timeout);
osStatus_t	PIN_GPIO_Mutex_Release(void);
uint8_t		PIN_GPIO_Mutex_Is_Locked(void);

/* FPGA & SPI bus controls */
osStatus_t	FPGA_Reset_With_Check(uint32_t reset_time_ms, uint32_t timeout_ms);
uint8_t		FPGA_Is_Ready(void);
uint8_t		FPGA_Is_In_Reset(void);

osStatus_t	SPI_Bus_Configure(SPI_Bus_Mode_t mode);
osStatus_t	SPI_Bus_Acquire_For_STM32(void);
osStatus_t	SPI_Bus_Release_To_FPGA(void);
uint8_t		SPI_Bus_Is_Acquired(void);

/* Utilities */
uint8_t		STLINK_Is_Connected(void);
osStatus_t	PIN_Delay(uint32_t ms);
osStatus_t	PIN_Blink(const Pin_Descriptor_t *pin, uint8_t count, uint32_t delay_ms);

/* General convenience macros */
#define PIN_ON(pin)          PIN_Set(pin)
#define PIN_OFF(pin)         PIN_Reset(pin)
#define PIN_TOGGLE(pin)      PIN_Toggle(pin)
#define PIN_STATE(pin)       PIN_Read(pin)

#define PIN_ON_F(pin)        PIN_Set_F(pin)
#define PIN_OFF_F(pin)       PIN_Reset_F(pin)
#define PIN_TOGGLE_F(pin)    PIN_Toggle_F(pin)
#define PIN_STATE_F(pin)     PIN_Read_F(pin)

/* Chip Select macros for FPGA (Strictly controls PB0) */
#define FPGA_CS_ENABLE()     PIN_Reset_F(&pin_fpga_cs)  /* PB0 = 0 (FPGA selected) */
#define FPGA_CS_DISABLE()    PIN_Set_F(&pin_fpga_cs)    /* PB0 = 1 (FPGA deselected) */

#define SPI_CSO_ON_F()       FPGA_CS_ENABLE()
#define SPI_CSO_OFF_F()      FPGA_CS_DISABLE()
#define SPI_CSO_ON()         FPGA_CS_ENABLE()
#define SPI_CSO_OFF()        FPGA_CS_DISABLE()

/* Hardware Pin Descriptors */
extern const Pin_Descriptor_t pin_led_green;
extern const Pin_Descriptor_t pin_clk25mhz;
extern const Pin_Descriptor_t pin_tp1;
extern const Pin_Descriptor_t pin_tp2;
extern const Pin_Descriptor_t pin_tp3;
extern const Pin_Descriptor_t pin_tp4;
extern const Pin_Descriptor_t pin_fpga_done;
extern const Pin_Descriptor_t pin_usart2_fpga_tx;
extern const Pin_Descriptor_t pin_usart2_fpga_rx;
extern const Pin_Descriptor_t pin_spi_sck;
extern const Pin_Descriptor_t pin_spi_miso;
extern const Pin_Descriptor_t pin_spi_mosi;

/* Distinct Chip Select Pins (Eliminates confusion) */
extern const Pin_Descriptor_t pin_flash_cs;           /* PA4: Flash W25Q128 CS */
extern const Pin_Descriptor_t pin_fpga_cs;            /* PB0: FPGA Registers CS (Active) */
extern const Pin_Descriptor_t pin_spi_cso;            /* Alias -> PA4 */
extern const Pin_Descriptor_t pin_stm32_2_fpga_nss_p; /* Alias -> PB0 */

extern const Pin_Descriptor_t pin_fpga_2_stm32_misc1;
extern const Pin_Descriptor_t pin_stm32_2_fpga_nss_d; /* PB2: FPGA DMA CS */
extern const Pin_Descriptor_t pin_interrupt_fpga;     /* PB10: FPGA IRQ */
extern const Pin_Descriptor_t pin_mr_fpga;
extern const Pin_Descriptor_t pin_mr_prog;           /* PB15: PROG_B */
extern const Pin_Descriptor_t pin_usart6_debug_tx;
extern const Pin_Descriptor_t pin_usart6_debug_rx;
extern const Pin_Descriptor_t pin_usart1_kpa_te;
extern const Pin_Descriptor_t pin_usart1_kpa_tx;
extern const Pin_Descriptor_t pin_usart1_kpa_rx;
extern const Pin_Descriptor_t pin_stlink_detect;

#ifdef __cplusplus
}
#endif

#endif /* __PIN_MGMT_H */

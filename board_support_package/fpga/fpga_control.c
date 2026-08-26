/**
 ******************************************************************************
 * @file    fpga_control.c
 * @brief   High-Level FPGA Module Control API Implementation for MSB.
 *          Provides structured access to Debug, FCS, and Interrupt IP blocks.
 *          Target: Xilinx Spartan-6 + STM32F411 (SPI Mode 00).
 *          All comments in pure ASCII English.
 ******************************************************************************
 */

#include "fpga_control.h"

/* ========================================================================= */
/*  SECTION 1: DEBUG MODULE APIS (Device ID = 1)                             */
/* ========================================================================= */

/**
 * @brief  Verifies FPGA alive status by reading hardcoded constant 0xDEAD.
 * @note   Target: Device ID 1, Reg 0x01 (REG_S_DEBUG_CONST).
 *         Returns error on SPI bus fault, clock noise, or incorrect bitstream.
 */
FPGA_Status_t FPGA_Debug_Verify(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms){
    uint16_t const_val = 0;
    FPGA_Status_t status;

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_CONST, &const_val, timeout_ms);
    if(status != FPGA_OK){
        return status;
    }

    if(const_val != FPGA_DEBUG_CONST_VAL){
        return FPGA_ERROR; // Signature mismatch (Clock noise, bad bitstream, or wrong CS)
    }

    return FPGA_OK;
}

/**
 * @brief  Executes full-duplex SPI bus loopback test using scratchpad register.
 * @note   Target: Device ID 1, Reg 0x00 (REG_S_DEBUG_FEEDBACK).
 *         Writes test pattern and reads it back to verify line integrity.
 */
FPGA_Status_t FPGA_Debug_Test_Echo(FPGA_HandleTypeDef *hbridge, uint16_t test_val, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t readback_val = 0;

    status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_FEEDBACK, test_val, timeout_ms);
    if(status != FPGA_OK){
        return status;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_FEEDBACK, &readback_val, timeout_ms);
    if(status != FPGA_OK){
        return status;
    }

    if(readback_val != test_val){
        return FPGA_ERROR; // Bus loopback integrity check failed
    }

    return FPGA_OK;
}

/**
 * @brief  Updates on-board status LEDs (VD35..VD37) via thread-safe RMW.
 * @note   Target: Device ID 1, Reg 0x02, Bits [2:0].
 *         Bit 0 = Green (VD35), Bit 1 = Yellow (VD36), Bit 2 = Red (VD37).
 */
FPGA_Status_t FPGA_Debug_Set_LEDs(FPGA_HandleTypeDef *hbridge, uint8_t led_mask, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t reg_val = 0;

    if(!hbridge) return FPGA_ERROR;

    // Recursive mutex allows safe nested acquisition inside Read/Write functions
    if(osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK){
        return FPGA_BUSY;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_MISC, &reg_val, timeout_ms);
    if(status == FPGA_OK){
        reg_val &= ~FPGA_DEBUG_MISC_LED_MASK;
        reg_val |= ((uint16_t)led_mask & FPGA_DEBUG_MISC_LED_MASK);
        status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_MISC, reg_val, timeout_ms);
    }

    osMutexRelease(hbridge->mutex_id);
    return status;
}

/**
 * @brief  Direct boolean control helper for Red, Yellow, and Green LEDs.
 * @param  r: Red LED (VD37), y: Yellow LED (VD36), g: Green LED (VD35).
 */
FPGA_Status_t FPGA_Debug_Write_LEDs(FPGA_HandleTypeDef *hbridge, bool r, bool y, bool g, uint32_t timeout_ms){
    uint8_t led_mask = 0;

    if(r) led_mask |= FPGA_DEBUG_MISC_LED_RED;
    if(y) led_mask |= FPGA_DEBUG_MISC_LED_YELLOW;
    if(g) led_mask |= FPGA_DEBUG_MISC_LED_GREEN;

    return FPGA_Debug_Set_LEDs(hbridge, led_mask, timeout_ms);
}

/**
 * @brief  Configures programmable tick divider period in ms.
 * @note   Target: Device ID 1, Reg 0x02, Bits [11:4].
 *         Controls FCS Sample-and-Hold latch rate and hardware IRQ pulse rate.
 */
FPGA_Status_t FPGA_Debug_Set_Tick_Divider(FPGA_HandleTypeDef *hbridge, uint8_t divider, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t reg_val = 0;

    if(!hbridge) return FPGA_ERROR;

    if(osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK){
        return FPGA_BUSY;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_MISC, &reg_val, timeout_ms);
    if(status == FPGA_OK){
        reg_val &= ~FPGA_DEBUG_MISC_TICK_DIV_MASK;
        reg_val |= (((uint16_t)divider << FPGA_DEBUG_MISC_TICK_DIV_SHIFT) & FPGA_DEBUG_MISC_TICK_DIV_MASK);
        status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_MISC, reg_val, timeout_ms);
    }

    osMutexRelease(hbridge->mutex_id);
    return status;
}

/**
 * @brief  Switches STM32 USART2 routing to target RS-485 transceiver.
 * @note   Target: Device ID 1, Reg 0x02, Bit 3 (0 = DD19 Port 1, 1 = DD20 Port 2).
 */
FPGA_Status_t FPGA_Debug_Set_UART_Mux(FPGA_HandleTypeDef *hbridge, FPGA_Uart_Port_t port, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t reg_val = 0;

    if(!hbridge) return FPGA_ERROR;

    if(osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK){
        return FPGA_BUSY;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_MISC, &reg_val, timeout_ms);
    if(status == FPGA_OK){
        reg_val &= ~FPGA_DEBUG_MISC_UART_MUX_MASK;
        if(port == FPGA_UART_PORT_2){
            reg_val |= FPGA_DEBUG_MISC_UART_MUX_DD20;
        }
        status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_MISC, reg_val, timeout_ms);
    }

    osMutexRelease(hbridge->mutex_id);
    return status;
}

/**
 * @brief  Configures dynamic multiplexers for diagnostic test points TP5..TP7.
 * @note   Target: Device ID 1, Reg 0x03 (5 bits per channel: TP5=[4:0], TP6=[9:5], TP7=[14:10]).
 */
FPGA_Status_t FPGA_Debug_Set_TP_Mux(FPGA_HandleTypeDef *hbridge, uint8_t tp5_sig, uint8_t tp6_sig, uint8_t tp7_sig, uint32_t timeout_ms){
    uint16_t reg_val = 0;

    reg_val |= (((uint16_t)tp5_sig << FPGA_TP_MUX_TP5_SHIFT) & FPGA_TP_MUX_TP5_MASK);
    reg_val |= (((uint16_t)tp6_sig << FPGA_TP_MUX_TP6_SHIFT) & FPGA_TP_MUX_TP6_MASK);
    reg_val |= (((uint16_t)tp7_sig << FPGA_TP_MUX_TP7_SHIFT) & FPGA_TP_MUX_TP7_MASK);

    return FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_TP_MUX, reg_val, timeout_ms);
}

/**
 * @brief  Executes software MultiBoot reboot to Image 2 (Flash 1 MB offset).
 * @note   Flow: Writes 0xAA55 to Reg 0x04 -> Releases SPI to Hi-Z ->
 *         Waits for DONE transition 1->0->1 -> Reclaims SPI -> Verifies 0xDEAD.
 */
FPGA_Status_t FPGA_Reboot_To_Image2(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms){
    FPGA_Status_t status;

    if(!hbridge) return FPGA_ERROR;

    // 1. Send Magic Key 0xAA55 to Register 0x04 to trigger ICAP reboot
    status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_RECONFIG, FPGA_DEBUG_RECONFIG_KEY, 100);
    if(status != FPGA_OK){
        return status;
    }

    // 2. Immediately release SPI bus into Hi-Z mode for FPGA Flash configuration
    SPI_Bus_Release_To_FPGA();

    // 3. Small pause to allow Spartan-6 to clear SRAM and drop DONE pin to 0
    PIN_Delay(10);

    // 4. Wait for FPGA DONE pin (PA1) to become 1
    if(FPGA_Wait_Ready(timeout_ms) != osOK){
        SPI_Bus_Acquire_For_STM32(); // Fallback restore
        return FPGA_TIMEOUT;
    }

    // 5. Re-acquire SPI1 bus in Master mode
    if(SPI_Bus_Acquire_For_STM32() != osOK){
        return FPGA_ERROR;
    }

    // 6. Verify SPI communication with new loaded image (checks 0xDEAD)
    return FPGA_Debug_Verify(hbridge, 100);
}

/**
 * @brief  Non-blocking running lights diagnostic sequence for on-board LEDs.
 * @note   Shifts active LED (Red -> Yellow -> Green) every 200 ms.
 */
FPGA_Status_t FPGA_Debug_Running_Lights(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms){
    static uint32_t last_update = 0;
    static uint8_t  light_state = 0;

    uint32_t current_tick   = osKernelGetTickCount();
    uint32_t interval_ticks = (200U * osKernelGetTickFreq()) / 1000U;

    if((current_tick - last_update) >= interval_ticks){
        last_update = current_tick;
        light_state = (light_state + 1) % 4;

        uint8_t led_mask = 0;
        switch(light_state){
            case 0:  led_mask = FPGA_DEBUG_MISC_LED_RED;    break;
            case 1:  led_mask = FPGA_DEBUG_MISC_LED_YELLOW; break;
            case 2:  led_mask = FPGA_DEBUG_MISC_LED_GREEN;  break;
            default: led_mask = 0x00U;                      break;
        }

        return FPGA_Debug_Set_LEDs(hbridge, led_mask, timeout_ms);
    }

    return FPGA_OK;
}

/* ========================================================================= */
/*  SECTION 2: FCS MODULE APIS (Device ID = 2)                               */
/* ========================================================================= */

/**
 * @brief  Reads all 31 debounced discrete inputs from FPGA FCS module.
 * @param  status_1: Lower 16 inputs (DR[10:0] sensor bus + HEF..GM ammo inputs).
 * @param  status_2: Upper 15 inputs (CC..SCF_ON_ADD) + Reset-Dominant JK latch at MSB.
 */
FPGA_Status_t FPGA_FCS_Read_Inputs(FPGA_HandleTypeDef *hbridge, uint16_t *status_1, uint16_t *status_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    if(!status_1 || !status_2){
        return FPGA_ERROR;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_FCS_STATUS_1, status_1, timeout_ms);
    if(status != FPGA_OK){
        return status;
    }

    return FPGA_Read_Poll(hbridge, ADDR_S_FCS_STATUS_2, status_2, timeout_ms);
}

/**
 * @brief  Configures Hard/Soft simulation overrides and virtual software values.
 * @note   Targets: Reg 0x10..0x13.
 *         0 = Physical hardware pin, 1 = Virtual software injected value.
 */
FPGA_Status_t FPGA_FCS_Configure_Override(FPGA_HandleTypeDef *hbridge, uint16_t hard_soft_1, uint16_t hard_soft_2,
                                         uint16_t soft_val_1, uint16_t soft_val_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_HARD_SOFT_1, hard_soft_1, timeout_ms);
    if(status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_HARD_SOFT_2, hard_soft_2, timeout_ms);
    if(status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_SOFT_VAL_1, soft_val_1, timeout_ms);
    if(status != FPGA_OK) return status;

    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_SOFT_VAL_2, soft_val_2, timeout_ms);
}

/**
 * @brief  Configures hardware input inversion masks on the PCB level.
 * @note   Targets: Reg 0x14 (inputs 11..26) and Reg 0x15 (inputs 27..30).
 *         0 = Active-High (normal), 1 = Active-Low (inverted to 1 when active).
 */
FPGA_Status_t FPGA_FCS_Configure_Inversions(FPGA_HandleTypeDef *hbridge, uint16_t inv_1, uint8_t inv_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_INV_1, inv_1, timeout_ms);
    if(status != FPGA_OK){
        return status;
    }

    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_INV_2, (uint16_t)(inv_2 & 0x0F), timeout_ms);
}

/**
 * @brief  Controls 8 discrete relay and transistor outputs (ENA_SHOOTING, etc.).
 * @note   Target: Reg 0x16. Physical outputs are active-low on PCB.
 */
FPGA_Status_t FPGA_FCS_Set_Control_Outputs(FPGA_HandleTypeDef *hbridge, uint8_t fcs_control_val, uint32_t timeout_ms){
    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_CONTROL, (uint16_t)fcs_control_val, timeout_ms);
}

/**
 * @brief  Selects discrete input reading mode (Sample-and-Hold vs Real-Time Bypass).
 * @note   Target: Reg 0x17.
 *         0 = Combinatorial Bypass (0-latency real-time reading).
 *         1 = Sample-and-Hold Mode (Snapshots latched on periodic 100 Hz pulse).
 */
FPGA_Status_t FPGA_FCS_Set_Latch_Enable(FPGA_HandleTypeDef *hbridge, bool enable, uint32_t timeout_ms){
    uint16_t val = enable ? 0x0001U : 0x0000U;
    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_LATCH_EN, val, timeout_ms);
}

/* ========================================================================= */
/*  SECTION 3: INTERRUPT CONTROLLER APIS (Device ID = 3)                     */
/* ========================================================================= */

/**
 * @brief  Configures hardware Interrupt Mask, Edge polarity, and Global Enable.
 * @note   Targets: Reg 0x01 (Mask), Reg 0x02 (Edge Sel), Reg 0x03 (Control/GIE).
 */
FPGA_Status_t FPGA_Int_Configure(FPGA_HandleTypeDef *hbridge, uint16_t mask, uint16_t edge_sel,
                                bool global_enable, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t ctrl_val = global_enable ? FPGA_INT_CTRL_GIE_ENABLE : 0x0000U;

    status = FPGA_Write_Poll(hbridge, ADDR_S_INT_MASK, mask, timeout_ms);
    if(status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_INT_EDGE_SEL, edge_sel, timeout_ms);
    if(status != FPGA_OK) return status;

    return FPGA_Write_Poll(hbridge, ADDR_S_INT_CTRL, ctrl_val, timeout_ms);
}

/**
 * @brief  Reads active masked pending interrupt flags from FPGA.
 * @note   Target: Reg 0x00. Automatically returns (pending & mask) in hardware.
 */
FPGA_Status_t FPGA_Int_Get_Pending(FPGA_HandleTypeDef *hbridge, uint16_t *pending, uint32_t timeout_ms){
    if(!pending){
        return FPGA_ERROR;
    }
    return FPGA_Read_Poll(hbridge, ADDR_S_INT_PENDING, pending, timeout_ms);
}

/**
 * @brief  Clears pending interrupt flags using Write-1-to-Clear (W1C) strategy.
 * @note   Target: Reg 0x00. Writing '1' clears corresponding pending bit.
 */
FPGA_Status_t FPGA_Int_Clear_Pending(FPGA_HandleTypeDef *hbridge, uint16_t clear_mask, uint32_t timeout_ms){
    return FPGA_Write_Poll(hbridge, ADDR_S_INT_PENDING, clear_mask, timeout_ms);
}

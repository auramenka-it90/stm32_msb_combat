/**
  ******************************************************************************
  * @file    fpga_control.c
  * @brief   Implementation of high-level FPGA Module Control API.
  *          Coordinates bus calls using stm32_2_fpga_spi_bridge.
  *          All comments in English.
  ******************************************************************************
  */

#include "fpga_control.h"

/* ========================================================================= */
/*  DEBUG MODULE IMPLEMENTATION                                              */
/* ========================================================================= */

FPGA_Status_t FPGA_Debug_Verify(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms){
    uint16_t const_val = 0;
    FPGA_Status_t status;

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_CONST, &const_val, timeout_ms);
    if (status != FPGA_OK) {
        return status;
    }

    if (const_val != FPGA_DEBUG_CONST_VAL) {
        return FPGA_ERROR; // Hardcoded constant value mismatch (SPI clock noise or bad bitstream)
    }

    return FPGA_OK;
}

FPGA_Status_t FPGA_Debug_Test_Echo(FPGA_HandleTypeDef *hbridge, uint16_t test_val, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t readback_val = 0;

    status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_FEEDBACK, test_val, timeout_ms);
    if (status != FPGA_OK) {
        return status;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_FEEDBACK, &readback_val, timeout_ms);
    if (status != FPGA_OK) {
        return status;
    }

    if (readback_val != test_val) {
        return FPGA_ERROR; // Bus integrity check failed
    }

    return FPGA_OK;
}

FPGA_Status_t FPGA_Debug_Set_LEDs(FPGA_HandleTypeDef *hbridge, uint8_t led_mask, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t reg_val = 0;

    // Acquire bus mutex to read-modify-write safely.
    // This is safe to call because the mutex is configured as Recursive.
    if (osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK) {
        return FPGA_BUSY;
    }

    // Read current register value
    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_MISC, &reg_val, timeout_ms);
    if (status == FPGA_OK) {
        // Update only LED bits
        reg_val &= ~FPGA_DEBUG_MISC_LED_MASK;
        reg_val |= ((uint16_t)led_mask & FPGA_DEBUG_MISC_LED_MASK);
        status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_MISC, reg_val, timeout_ms);
    }

    osMutexRelease(hbridge->mutex_id);
    return status;
}

FPGA_Status_t FPGA_Debug_Set_Tick_Divider(FPGA_HandleTypeDef *hbridge, uint8_t divider, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t reg_val = 0;

    // Acquire bus mutex to read-modify-write safely.
    // This is safe to call because the mutex is configured as Recursive.
    if (osMutexAcquire(hbridge->mutex_id, timeout_ms) != osOK) {
        return FPGA_BUSY;
    }

    // Read current register value
    status = FPGA_Read_Poll(hbridge, ADDR_S_DEBUG_MISC, &reg_val, timeout_ms);
    if (status == FPGA_OK) {
        // Update only Divider bits
        reg_val &= ~FPGA_DEBUG_MISC_TICK_DIV_MASK;
        reg_val |= (((uint16_t)divider << FPGA_DEBUG_MISC_TICK_DIV_SHIFT) & FPGA_DEBUG_MISC_TICK_DIV_MASK);
        status = FPGA_Write_Poll(hbridge, ADDR_S_DEBUG_MISC, reg_val, timeout_ms);
    }

    osMutexRelease(hbridge->mutex_id);
    return status;
}

/* Direct boolean control of Red (Bit 2), Yellow (Bit 1), and Green (Bit 0) LEDs */
FPGA_Status_t FPGA_Debug_Write_LEDs(FPGA_HandleTypeDef *hbridge, bool r, bool y, bool g, uint32_t timeout_ms){
    uint8_t led_mask = 0;

    if (r) led_mask |= FPGA_DEBUG_MISC_LED_RED;
    if (y) led_mask |= FPGA_DEBUG_MISC_LED_YELLOW;
    if (g) led_mask |= FPGA_DEBUG_MISC_LED_GREEN;

    return FPGA_Debug_Set_LEDs(hbridge, led_mask, timeout_ms);
}

/* Non-blocking running lights cycle (R->Y->G->Blank) shifting every 200ms */
FPGA_Status_t FPGA_Debug_Running_Lights(FPGA_HandleTypeDef *hbridge, uint32_t timeout_ms){
    static uint32_t last_update = 0;
    static uint8_t light_state = 0;

    // Retrieve the current tick count using CMSIS-RTOS v2 API
    uint32_t current_tick = osKernelGetTickCount();

    // Convert 200 ms interval into RTOS system ticks dynamically
    uint32_t interval_ticks = (200U * osKernelGetTickFreq()) / 1000U;

    if ((current_tick - last_update) >= interval_ticks) {
        last_update = current_tick;
        light_state = (light_state + 1) % 4;

        uint8_t led_mask = 0;
        switch (light_state) {
            case 0: led_mask = FPGA_DEBUG_MISC_LED_RED; break;
            case 1: led_mask = FPGA_DEBUG_MISC_LED_YELLOW; break;
            case 2: led_mask = FPGA_DEBUG_MISC_LED_GREEN; break;
            default: led_mask = 0x00U; break;
        }

        return FPGA_Debug_Set_LEDs(hbridge, led_mask, timeout_ms);
    }

    return FPGA_OK;
}


/* ========================================================================= */
/*  FCS MODULE IMPLEMENTATION                                                */
/* ========================================================================= */

FPGA_Status_t FPGA_FCS_Read_Inputs(FPGA_HandleTypeDef *hbridge, uint16_t *status_1, uint16_t *status_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    if (status_1 == NULL || status_2 == NULL) {
        return FPGA_ERROR;
    }

    status = FPGA_Read_Poll(hbridge, ADDR_S_FCS_STATUS_1, status_1, timeout_ms);
    if (status != FPGA_OK) {
        return status;
    }

    return FPGA_Read_Poll(hbridge, ADDR_S_FCS_STATUS_2, status_2, timeout_ms);
}

FPGA_Status_t FPGA_FCS_Configure_Override(FPGA_HandleTypeDef *hbridge, uint16_t hard_soft_1, uint16_t hard_soft_2, uint16_t soft_val_1, uint16_t soft_val_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_HARD_SOFT_1, hard_soft_1, timeout_ms);
    if (status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_HARD_SOFT_2, hard_soft_2, timeout_ms);
    if (status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_SOFT_VAL_1, soft_val_1, timeout_ms);
    if (status != FPGA_OK) return status;

    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_SOFT_VAL_2, soft_val_2, timeout_ms);
}

FPGA_Status_t FPGA_FCS_Configure_Inversions(FPGA_HandleTypeDef *hbridge, uint16_t inv_1, uint8_t inv_2, uint32_t timeout_ms){
    FPGA_Status_t status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_FCS_INV_1, inv_1, timeout_ms);
    if (status != FPGA_OK) {
        return status;
    }

    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_INV_2, (uint16_t)(inv_2 & 0x0F), timeout_ms);
}

FPGA_Status_t FPGA_FCS_Set_Control_Outputs(FPGA_HandleTypeDef *hbridge, uint8_t fcs_control_val, uint32_t timeout_ms){
    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_CONTROL, (uint16_t)fcs_control_val, timeout_ms);
}

FPGA_Status_t FPGA_FCS_Set_Latch_Enable(FPGA_HandleTypeDef *hbridge, bool enable, uint32_t timeout_ms){
    uint16_t val = enable ? 0x0001U : 0x0000U;
    return FPGA_Write_Poll(hbridge, ADDR_S_FCS_LATCH_EN, val, timeout_ms);
}

/* ========================================================================= */
/*  INTERRUPT CONTROLLER IMPLEMENTATION                                      */
/* ========================================================================= */

FPGA_Status_t FPGA_Int_Configure(FPGA_HandleTypeDef *hbridge, uint16_t mask, uint16_t edge_sel, bool global_enable, uint32_t timeout_ms){
    FPGA_Status_t status;
    uint16_t ctrl_val = global_enable ? 0x0001U : 0x0000U;

    status = FPGA_Write_Poll(hbridge, ADDR_S_INT_MASK, mask, timeout_ms);
    if (status != FPGA_OK) return status;

    status = FPGA_Write_Poll(hbridge, ADDR_S_INT_EDGE_SEL, edge_sel, timeout_ms);
    if (status != FPGA_OK) return status;

    return FPGA_Write_Poll(hbridge, ADDR_S_INT_CTRL, ctrl_val, timeout_ms);
}

FPGA_Status_t FPGA_Int_Get_Pending(FPGA_HandleTypeDef *hbridge, uint16_t *pending, uint32_t timeout_ms){
    if (pending == NULL) {
        return FPGA_ERROR;
    }
    return FPGA_Read_Poll(hbridge, ADDR_S_INT_PENDING, pending, timeout_ms);
}

FPGA_Status_t FPGA_Int_Clear_Pending(FPGA_HandleTypeDef *hbridge, uint16_t clear_mask, uint32_t timeout_ms){
    // W1C: Writes the clear mask to the pending register.
    // Writing 1 to a specific bit position in the FPGA pending register clears that specific interrupt.
    return FPGA_Write_Poll(hbridge, ADDR_S_INT_PENDING, clear_mask, timeout_ms);
}

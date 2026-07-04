/**
  ******************************************************************************
  * @file    host.c
  * @brief   Implementation of MSB <-> BC serial communication protocol driver
  *          over USART2 utilizing thread-safe DMA TX/RX operations, FSM
  *          and central statistics engine.
  *          All comments in English.
  ******************************************************************************
  */

#include "host.h"
#include <string.h> // For memset/memcpy

//=============================================================================
//  PRIVATE TYPE DEFINITIONS & VARIABLES
//=============================================================================

/* FSM States for byte-by-byte incoming packet reconstruction */
typedef enum {
    RX_STATE_FIND_START = 0,
    RX_STATE_COLLECT_DATA,
    RX_STATE_GET_CRC_L,
    RX_STATE_GET_CRC_H,
    RX_STATE_VERIFY_XOR
} Rx_ParserState_t;

/* Driver context structure containing synchronization primitives */
typedef struct {
    UART_HandleTypeDef *huart;
    osMutexId_t         tx_mutex;   /* Protects tx_buf from concurrent task access */
    osSemaphoreId_t     tx_sem;     /* Blocks transmitting task until DMA TX finishes */
} host_uart_context_t;

static host_uart_context_t host_ctx = {0};

/* Global statistics and connection trackers */
Host_Stats_t host_stats = {0};
volatile uint32_t last_bc_packet_tick = 0;
volatile bool is_bc_link_error = true; /* Starts as true until first valid packet arrives */

/* Static buffers to avoid dynamic memory allocation and fragmentation */
static uint8_t host_rx_buf[HOST_RX_BUF_SIZE];
static uint8_t host_tx_buf[HOST_TX_BUF_SIZE];

//=============================================================================
//  PRIVATE PROTOCOL HELPER FUNCTIONS
//=============================================================================

/**
  * @brief  Computes the CRC-16-CCITT checksum over the buffer.
  */
static uint16_t compute_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = MSB_CRC_INIT;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000U) {
                crc = (crc << 1) ^ MSB_CRC_POLYNOMIAL;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
  * @brief  Computes cumulative XOR checksum over the buffer.
  */
static uint8_t compute_xor(const uint8_t *data, uint16_t length) {
    uint8_t xor_val = 0U;
    for (uint16_t i = 0; i < length; i++) {
        xor_val ^= data[i];
    }
    return xor_val;
}

/**
  * @brief  Robust Finite State Machine (FSM) byte-by-byte incoming packet parser.
  *         Directly extracts the BC -> MSB control flags on validation success.
  */
static void host_process_rx_byte(uint8_t rx_byte) {
    static Rx_ParserState_t state = RX_STATE_FIND_START;
    static uint16_t data_idx = 0U;
    static uint8_t packet_parsing_buf[MSB_PACKET_SIZE];

    switch (state) {
        case RX_STATE_FIND_START:
            if (rx_byte == MSB_START_BYTE_BC) { // Match 0x55
                packet_parsing_buf[0] = rx_byte;
                data_idx = 0U;
                state = RX_STATE_COLLECT_DATA;
            }
            break;

        case RX_STATE_COLLECT_DATA:
            packet_parsing_buf[1 + data_idx] = rx_byte;
            data_idx++;
            if (data_idx >= MSB_DATA_SIZE) {
                state = RX_STATE_GET_CRC_L;
            }
            break;

        case RX_STATE_GET_CRC_L:
            packet_parsing_buf[29] = rx_byte;
            state = RX_STATE_GET_CRC_H;
            break;

        case RX_STATE_GET_CRC_H:
            packet_parsing_buf[30] = rx_byte;
            state = RX_STATE_VERIFY_XOR;
            break;

        case RX_STATE_VERIFY_XOR:
            packet_parsing_buf[31] = rx_byte;

            // 1. Verify CRC16 calculated over Bytes 0 to 28 (Total 29 Bytes)
            uint16_t expected_crc = compute_crc16(packet_parsing_buf, 29U);
            uint16_t received_crc = ((uint16_t)packet_parsing_buf[30] << 8) | packet_parsing_buf[29];

            if (expected_crc == received_crc) {
                // 2. Verify XOR_ALL checksum calculated over Bytes 0 to 30 (Total 31 Bytes)
                uint8_t expected_xor = compute_xor(packet_parsing_buf, 31U);
                uint8_t received_xor = packet_parsing_buf[31];

                if (expected_xor == received_xor) {

                    /* Update connection timestamp and reset error state */
                    last_bc_packet_tick = osKernelGetTickCount();
                    is_bc_link_error = false;
                    host_stats.rx_count++; /* Increment valid receive packet counter */

                    // CRC & XOR Verified: Safely map and update high-level FCS commands
                    BC_Data_t bc_data;
                    memcpy(&bc_data, &packet_parsing_buf[1], sizeof(BC_Data_t));

                    BC_Flags_t bc_flags;
                    bc_flags.raw = bc_data.flags;

                    // Atomically update control commands structure
                    fcs_commands.ena_shooting     = bc_flags.bits.ena_shooting ? true : false;
                    fcs_commands.gmee             = bc_flags.bits.gmee ? true : false;
                    fcs_commands.range_over_1280  = bc_flags.bits.range_over_1280 ? true : false;
                    fcs_commands.uoi              = bc_flags.bits.uoi ? true : false;
                    fcs_commands.inhibit_shooting = bc_flags.bits.inhibit_shooting ? true : false;
                    fcs_commands.wind_sensor_on   = bc_flags.bits.wind_sensor_on ? true : false;
                } else {
                    host_stats.rx_xor_err++; /* Increment XOR failure counter */
                }
            } else {
                host_stats.rx_crc_err++; /* Increment CRC failure counter */
            }
            state = RX_STATE_FIND_START; // Always reset back to find start byte
            break;

        default:
            state = RX_STATE_FIND_START;
            break;
    }
}

//=============================================================================
//  LOW-LEVEL DMA UART DRIVER IMPLEMENTATION
//=============================================================================

bool host_uart_init(UART_HandleTypeDef *huart) {
    if (huart == NULL) {
        return false;
    }
    host_ctx.huart = huart;

    /* Create Mutex for thread-safe access to TX buffer */
    host_ctx.tx_mutex = osMutexNew(NULL);

    /* Create Binary Semaphore to signal DMA TX completion. Initial count = 1 (free) */
    host_ctx.tx_sem = osSemaphoreNew(1, 1, NULL);

    return (host_ctx.tx_mutex != NULL && host_ctx.tx_sem != NULL);
}

void host_uart_start_receive(void) {
    /* Set up DMA Receive to Idle (IDLE Line interrupt detection) */
    HAL_UARTEx_ReceiveToIdle_DMA(host_ctx.huart, host_rx_buf, HOST_RX_BUF_SIZE);

    /* Disable Half-Transfer interrupt to avoid unnecessary CPU wakeups */
    __HAL_DMA_DISABLE_IT(host_ctx.huart->hdmarx, DMA_IT_HT);
}

bool host_uart_send_raw(const uint8_t *data, uint16_t len) {
    if (len == 0 || len > HOST_TX_BUF_SIZE) {
        return false;
    }

    /* 1. Acquire mutex to lock the TX buffer from concurrent access */
    if (osMutexAcquire(host_ctx.tx_mutex, osWaitForever) != osOK) {
        return false;
    }

    /* 2. Wait for hardware DMA to become free. Current task yields to OS. */
    if (osSemaphoreAcquire(host_ctx.tx_sem, 100) != osOK) {
        osMutexRelease(host_ctx.tx_mutex);
        return false; /* Timeout: bus stuck */
    }

    /* 3. Copy data to protected transmit buffer */
    memcpy(host_tx_buf, data, len);

    /* 4. Trigger non-blocking DMA transmission */
    if (HAL_UART_Transmit_DMA(host_ctx.huart, host_tx_buf, len) != HAL_OK) {
        osSemaphoreRelease(host_ctx.tx_sem); /* Release semaphore immediately on failure */
        osMutexRelease(host_ctx.tx_mutex);
        return false;
    }

    /* 5. Release TX mutex lock. Note: Semaphore is released in TxCpltCallback! */
    osMutexRelease(host_ctx.tx_mutex);
    return true;
}

UART_HandleTypeDef* host_uart_get_handle(void) {
    return host_ctx.huart;
}

void host_uart_tx_complete_handler(UART_HandleTypeDef *huart) {
    if (huart == host_ctx.huart) {
        /* Release binary semaphore to signal the blocked task that TX is finished */
        osSemaphoreRelease(host_ctx.tx_sem);
    }
}

void host_uart_rx_event_handler(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart == host_ctx.huart) {
        if (Size > 0) {
            /* Feed DMA receive buffer directly to FSM parser sequentially */
            for (uint16_t i = 0; i < Size; i++) {
                host_process_rx_byte(host_rx_buf[i]);
            }
        }
        /* Instantly re-arm DMA reception */
        host_uart_start_receive();
    }
}

void host_uart_error_handler(UART_HandleTypeDef *huart) {
    if (huart == host_ctx.huart) {
        /* Clear overrun flags and stop DMA */
        HAL_UART_DMAStop(huart);
        __HAL_UART_CLEAR_OREFLAG(huart);

        /* Force release semaphore to prevent task lockups on hardware error */
        osSemaphoreRelease(host_ctx.tx_sem);

        /* Restart RX */
        host_uart_start_receive();
    }
}

//=============================================================================
//  HIGH-LEVEL FCS PROTOCOL PACKING
//=============================================================================

void host_send_msb_packet(const FCS_State_t *state) {
    if (state == NULL || state->is_link_error) {
        return; // Safety-critical: Do not transmit corrupted/missing sensor data
    }

    MSB_Packet_t packet;
    MSB_Data_t   data;
    MSB_Flags1_t flags1;
    MSB_Flags2_t flags2;

    memset(&packet, 0, sizeof(MSB_Packet_t));
    memset(&data, 0, sizeof(MSB_Data_t));
    flags1.raw = 0U;
    flags2.raw = 0U;

	// 1. Pack Target Distance (1 LSB = 5 m) - Clear to 0 if SRD is inactive (false)
	if (state->srd) {
		data.distance = state->distance_meters / 5U;
	} else {
		data.distance = 0U; // Force 0 if distance latch is inactive
	}

    // 2. Pack active Ammunition Type (4 bits)
    data.ammo_type = (uint8_t)state->ammo_type & 0x0FU;

    // 3. Pack Byte 3 Flags (Flags Part 1)
    flags1.bits.cc    = state->cc ? 1U : 0U;
    flags1.bits.dc    = state->dc ? 1U : 0U;
    flags1.bits.srd   = state->srd ? 1U : 0U;
    flags1.bits.bc_en = state->bc_en ? 1U : 0U;
    flags1.bits.rl    = state->rl ? 1U : 0U;
    flags1.bits.ws    = state->ws ? 1U : 0U;
    flags1.bits.pscc  = state->pscc ? 1U : 0U;
    flags1.bits.rfu1  = 0U;
    data.flags1 = flags1.raw;

    // 4. Pack Byte 4 Flags (Flags Part 2) with modified bits 6 and 7
    flags2.bits.k1         = state->k1 ? 1U : 0U;
    flags2.bits.btn_cannon = state->btn_cannon ? 1U : 0U;
    flags2.bits.rf         = state->rf ? 1U : 0U;
    flags2.bits.ur         = state->ur ? 1U : 0U;
    flags2.bits.rem        = state->rem ? 1U : 0U;
    flags2.bits.df         = state->df ? 1U : 0U;
    flags2.bits.scf_on     = state->scf_on ? 1U : 0U;
    flags2.bits.scf_on_add = state->scf_on_add ? 1U : 0U;
    data.flags2 = flags2.raw;

    // 5. Construct generic packet layout
    packet.start = MSB_START_BYTE_MSB; // 0xAA Header
    memcpy(packet.data, &data, MSB_DATA_SIZE);

    // 6. Calculate CRC-16-CCITT over Byte 0 (Header) + 28 Bytes Data (Total 29 Bytes)
    uint16_t crc_val = compute_crc16((uint8_t*)&packet, 29U);
    packet.crc_l = (uint8_t)(crc_val & 0x00FFU);
    packet.crc_h = (uint8_t)((crc_val >> 8) & 0x00FFU);

    // 7. Calculate XOR_ALL over Bytes 0 to 30 (Total 31 Bytes)
    packet.xor_all = compute_xor((uint8_t*)&packet, 31U);

    // 8. Thread-safe non-blocking DMA transmission
    if (host_uart_send_raw((const uint8_t*)&packet, MSB_PACKET_SIZE) == true) {
        host_stats.tx_count++; /* Increment TX counter on success */
    }
}

//=============================================================================
//  HAL INTERRUPT CALLBACKS SYSTEM INTEGRATION
//=============================================================================

/**
  * @brief  HAL extension callback for UART Idle line and full buffer detection.
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    /* USART2: Ballistic Computer DMA RX Complete */
    if (huart->Instance == USART2) {
        host_uart_rx_event_handler(huart, Size);
    }
}

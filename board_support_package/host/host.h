/**
  ******************************************************************************
  * @file    host.h
  * @brief   Consolidated declarations for MSB <-> BC high-speed UART
  *          communication channel (USART2, 921600 Baud).
  *          Combines packed packet structures, thread-safe DMA UART driver
  *          and communication statistics.
  *          All comments in English.
  ******************************************************************************
  */

#ifndef HOST_H
#define HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board_support_package.h" // Access to huart2 handle and base system APIs
#include "fcs.h"                   // Access to FCS_State_t and FCS_Control_Commands_t
#include <stdint.h>
#include <stdbool.h>

//=============================================================================
//  UART & PROTOCOL CONSTANTS
//=============================================================================
#define MSB_UART_BAUDRATE      921600U
#define MSB_PACKET_SIZE        32U
#define MSB_DATA_SIZE          28U
#define MSB_CRC_SIZE           2U

#define MSB_START_BYTE_MSB     0xAAU  // Outgoing packet header (MSB -> BC)
#define MSB_START_BYTE_BC      0x55U  // Incoming packet header (BC -> MSB)

#define MSB_CRC_POLYNOMIAL     0x1021U
#define MSB_CRC_INIT           0xFFFFU

#define HOST_RX_BUF_SIZE       128U   // Size of double packet receive DMA buffer
#define HOST_TX_BUF_SIZE       128U   // Size of thread-safe transmit DMA buffer

//=============================================================================
//  COMMUNICATION STATISTICS STRUCTURE
//=============================================================================
typedef struct {
    uint32_t tx_count;      // Total transmitted packets (MSB -> BC)
    uint32_t rx_count;      // Total successfully received packets (BC -> MSB)
    uint32_t rx_crc_err;    // Total packets discarded due to CRC mismatch
    uint32_t rx_xor_err;    // Total packets discarded due to XOR mismatch
} Host_Stats_t;

/* Global statistics and connection trackers */
extern  Host_Stats_t host_stats;
extern volatile bool is_bc_link_error;       // true if Ballistic Computer connection is lost
extern volatile uint32_t last_bc_packet_tick; // Last system tick when a valid packet was decoded

//=============================================================================
//  PACKET STRUCTURE DEFINITIONS (PACKED)
//=============================================================================

/**
 * @brief  Byte 3 Flags Part 1 Union (MSB -> BC)
 */
typedef union
{
    uint8_t raw;
    struct __attribute__((packed))
    {
        uint8_t cc      : 1;   // bit 0: Double command override
        uint8_t dc      : 1;   // bit 1: Target designation (ЦУ)
        uint8_t srd     : 1;   // bit 2: Set/Reset Distance status (JK latch output)
        uint8_t bc_en   : 1;   // bit 3: Ballistic Computer enable
        uint8_t rl      : 1;   // bit 4: Rocket Launch completed
        uint8_t ws      : 1;   // bit 5: Wind Sensor allowed
        uint8_t pscc    : 1;   // bit 6: Power Supply of Combination Circuit active
        uint8_t rfu1    : 1;   // bit 7: Reserved (always 0)
    } bits;
} MSB_Flags1_t;

/**
 * @brief  Byte 4 Flags Part 2 Union (MSB -> BC)
 *         Bits 6 and 7 transmit scf_on and scf_on_add states.
 */
typedef union
{
    uint8_t raw;
    struct __attribute__((packed))
    {
        uint8_t k1          : 1;   // bit 0: Control status
        uint8_t btn_cannon  : 1;   // bit 1: Cannon trigger button pressed
        uint8_t rf          : 1;   // bit 2: Reset Filters
        uint8_t ur          : 1;   // bit 3: Sight unlatch
        uint8_t rem         : 1;   // bit 4: Rocket elevation permission
        uint8_t df          : 1;   // bit 5: Code DF active
        uint8_t scf_on      : 1;   // bit 6: SCF_ON hardware state (Modified RFU)
        uint8_t scf_on_add  : 1;   // bit 7: SCF_ON_ADD hardware state (Modified RFU)
    } bits;
} MSB_Flags2_t;

/**
 * @brief  Detailed 28-byte Data Field structure (MSB -> BC)
 */
typedef struct __attribute__((packed))
{
    uint16_t distance;            // bytes 0..1: Target distance (1 LSB = 5 m)
    uint8_t  ammo_type;           // byte 2: Ammo Type (bits 0..3)
    uint8_t  flags1;              // byte 3: Flags Part 1
    uint8_t  flags2;              // byte 4: Flags Part 2
    uint8_t  reserved[23];        // bytes 5..27: Reserved padding (always 0x00)
} MSB_Data_t;

/**
 * @brief  Byte 0 Control Flags Union (BC -> MSB)
 */
typedef union
{
    uint8_t raw;
    struct __attribute__((packed))
    {
        uint8_t ena_shooting      : 1;   // bit 0: Shooting permitted
        uint8_t gmee              : 1;   // bit 1: Rocket elevation enabled
        uint8_t range_over_1280   : 1;   // bit 2: Target distance > 1280 m
        uint8_t uoi               : 1;   // bit 3: UOI signal active
        uint8_t inhibit_shooting  : 1;   // bit 4: Shooting inhibited
        uint8_t wind_sensor_on    : 1;   // bit 5: Wind sensor turned on
        uint8_t rfu4              : 1;   // bit 6: Reserved
        uint8_t rfu5              : 1;   // bit 7: Reserved
    } bits;
} BC_Flags_t;

/**
 * @brief  Detailed 28-byte Data Field structure (BC -> MSB)
 */
typedef struct __attribute__((packed))
{
    uint8_t  flags;               // byte 0: BC Control Flags
    uint8_t  reserved[27];        // bytes 1..27: Reserved padding (always 0x00)
} BC_Data_t;

/**
 * @brief  Generic 32-byte physical packet frame layout (MSB <-> BC)
 */
typedef struct __attribute__((packed))
{
    uint8_t  start;               // byte 0: Start Header (0xAA or 0x55)
    uint8_t  data[MSB_DATA_SIZE]; // bytes 1..28: Processed payload data field
    uint8_t  crc_l;               // byte 29: CRC16 Little-Endian LSB
    uint8_t  crc_h;               // byte 30: CRC16 Little-Endian MSB
    uint8_t  xor_all;             // byte 31: Cumulative XOR checksum
} MSB_Packet_t;

//=============================================================================
//  THREAD-SAFE DMA UART DRIVER & HOST SYSTEM API
//=============================================================================

/**
 * @brief  Initializes UART driver, mutexes and semaphores.
 * @param  huart: Pointer to HAL UART handle (USART2)
 * @return true if success
 */
bool host_uart_init(UART_HandleTypeDef *huart);

/**
 * @brief  Starts DMA reception in ReceiveToIdle mode.
 */
void host_uart_start_receive(void);

/**
 * @brief  Sends a raw byte array via DMA with thread safety and OS sleep block.
 * @param  data: Pointer to data array
 * @param  len: Data length
 * @return true if transmission started successfully
 */
bool host_uart_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief  Retrieves the registered active UART handle.
 */
UART_HandleTypeDef* host_uart_get_handle(void);

/**
 * @brief  Encodes current FCS state, builds the 32-byte packet,
 *         and starts thread-safe DMA transmission.
 * @param  state: Pointer to the active FCS state structure
 */
void host_send_msb_packet(const FCS_State_t *state);

//=============================================================================
//  INTERRUPT HANDLERS (Call from HAL callbacks or it.c)
//=============================================================================

/** Handle RX Event (Idle line detected or buffer filled) */
void host_uart_rx_event_handler(UART_HandleTypeDef *huart, uint16_t Size);

/** Handle TX complete event */
void host_uart_tx_complete_handler(UART_HandleTypeDef *huart);

/** Handle UART hardware errors (Overruns, noise, framing errors) */
void host_uart_error_handler(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* HOST_H */

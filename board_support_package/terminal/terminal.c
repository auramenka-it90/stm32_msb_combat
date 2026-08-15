
/**
 * ******************************************************************************
 * @file    terminal.c
 * @brief   DSPAssist Terminal Communication and Flash Programming Interface.
 *          Adapted to support both FPGA W25Q128 Flash and STM32F411 Internal Flash.
 *          All comments in English.
 * ******************************************************************************
 */

#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"

#include	"board_support_package.h"
#include 	"terminal.h"
#include	"std_com.h"
#include	"wrapper.h"
#include 	"version_control.h"

#include 	"pin_mgmt.h"
#include	"terminal_signals.h"
#include 	"configuration.h"


/* Main terminal UART handle structure */
Uart_ctrl_t	uctrl;

/* DSPAssist engine configuration structures */
static	SYS_Config_t  	SYS_Config;
static 	bool reset_request = false;

static	void led_blink(bool fl_stay_here);
static	char* uint32_to_string(uint32_t n, char* buffer, size_t buffer_size);

static 	bool fl_stay_here = false;
static 	int	handle;

/* Terminal communication functions */
static int		Send(du8 *const pbuf, const unsigned int num);
static int		InBufUsed(void);
static int		GetData(du8 *const pbuf, const unsigned int num);
static void		FlushInBuffer(void);

/* Main device functions */
static dboolean	sys_SaveSettings(void);
static void		sys_Reset(void);
static char*	sys_SelfTest(void);

/* Telemetry descriptor */
TEL_Descriptor_t TEL_dscr = {
    100/*us*/* (_TERMINAL_TASK_TICK_ * 1000),	/* Period (lsb = 0.01 us) */
    TEL_MAX_SIGNALS, 		/* Maximum number of signals permitted in telemetry */
    0,  					/* Frame size (not supported) */
    TEL_ATTR_SUPPORT_BUFFER	/* Attributes */
};

/**
 * @brief  Initializes the DSPAssist terminal engine and binds the memory drivers.
 */
bool terminal_init(void) {

	SYS_Config.FuncPtr_SaveSettings = &sys_SaveSettings;
	SYS_Config.FuncPtr_SelfTest = &sys_SelfTest;
	SYS_Config.FuncPtr_SysReset = &sys_Reset;

	SYS_Config.system_info = (char*)get_device_info();

	SYS_Init(&SYS_Config);

	TEL_Set_descriptor(&TEL_dscr);
	dspa_dispatcher_init(Send, GetData, InBufUsed, FlushInBuffer);
	handle = init_terminal_signals();

	/* UART hardware initialization */
	if (!STLINK_Is_Connected()) {
		/* STM32 -> 221V34F26 external connector */
		uctrl.huart = &huart1;
		uctrl.dma_handle_rx = &hdma_usart1_rx;
		uctrl.dma_handle_tx = &hdma_usart1_tx;
	} else {
        /* STM32 -> ST-Link Virtual COM Port */
		uctrl.huart = &huart6;
		uctrl.dma_handle_rx = &hdma_usart6_rx;
		uctrl.dma_handle_tx = &hdma_usart6_tx;
	}

	uctrl.rx_buf_size = _TERMINAL_RX_BUFF_SIZE_;
	uctrl.tx_buf_size = _TERMINAL_TX_BUFF_SIZE_;
	com_init(&uctrl);

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
	wrp_init(&uctrl, _TERMINAL_WRAPPER_ADDRESS_, _TERMINAL_WRAPPER_ALT_ADDRESS_, _TERMINAL_RECEIVE_TIMEOUT_US_);
#endif

	com_start_receive(&uctrl);
	return true;
}


/**
 * @brief  Handling DSPAssist Task using precise periodic execution.
 *         Uses osDelayUntil to prevent cumulative timing drift caused by processing.
 *         Manages communication, telemetry, and conditional jump based on CRC.
 *         All comments in English.
 */
void terminal_task(void) {
    /* Initialize the absolute tick counter for precise periodic execution */
    static uint32_t tick_periodic = 0;
    if (tick_periodic == 0) {
        tick_periodic = osKernelGetTickCount();
    }
    /* Increment absolute target tick count by the defined task period */
    tick_periodic += _TERMINAL_TASK_TICK_;

    /* 1. Execute background telemetry and dispatcher handlers */
    terminal_telemetry_handler();
    dspa_dispatcher(_TERMINAL_TASK_TICK_ * 1000);

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
    com_check_timeout(&uctrl, _TERMINAL_TASK_TICK_ * 1000);
#endif

    /* 2. Process system reset requests with a small safety delay */
    if (reset_request) {
        osDelay(100);
        bsp_system_reset();
    }

    /* 3. Handle Bootloader Timeout and Conditional Jump */
    static uint32_t tick_start = 0;
    if (tick_start == 0) {
        tick_start = osKernelGetTickCount();
    }

    /* 4. Update status LED blink state */
    led_blink(fl_stay_here);

    /* 5. Precise periodic delay: blocks the thread until absolute target tick is reached */
    osDelayUntil(tick_periodic);
}

/**
 * @brief  Updates and saves current telemetry samples.
 */
void terminal_telemetry_handler(void) {
	TEL_Sample_Update(handle);
	TEL_Sample_Save();
}

/* ========================================================================= */
/*  TERMINAL LOW LEVEL COMMUNICATION FUNCTIONS                               */
/* ========================================================================= */

static int Send(unsigned char *const buf, const unsigned int num) {
	com_send(&uctrl, buf, num);
	fl_stay_here = true;
	return num;
}

static int GetData(unsigned char *const buf, const unsigned int num) {
	unsigned int num_i = num;
	if (num_i == 0) {
		num_i = com_inbuf_used(&uctrl);
	}
	if (num_i == 0) {
		return 0;
	}
	com_inbuf_fetch(&uctrl, buf, num_i);
	return num_i;
}

static int InBufUsed(void) {
	return com_inbuf_used(&uctrl);
}

static void FlushInBuffer(void) {
	com_flush_in_buffer(&uctrl);
}

/* ========================================================================= */
/*  SYSTEM DIAGNOSTICS & MANAGEMENT API                                      */
/* ========================================================================= */

static dboolean sys_SaveSettings(void) {
	bool result = save_setting();
	return result;
}

static void sys_Reset(void) {
    reset_request = true;
}

static char* sys_SelfTest(void) {
	static char result[64];
	char str[16];
	uint32_to_string(test_hardware_result, str, sizeof(str));
	snprintf(result, sizeof(result), "Self-diagnosis: Device status(0=ok): %s", str);
	return result;
}

/* ========================================================================= */
/*  UTILITY FUNCTIONS                                                        */
/* ========================================================================= */

/**
 * @brief  Blinks the green status LED.
 */
static void led_blink(bool f) {
#define		_LED_PERIOD_	(1500 * _TERMINAL_TASK_TICK_)

	static uint32_t tick_old = 0;

	uint32_t now = osKernelGetTickCount();
	uint32_t delta = now - tick_old;
	uint32_t time_on = (f) ? (_LED_PERIOD_ / 2) : (_LED_PERIOD_ / 30);

	if (delta < time_on) {
		PIN_Set(&pin_led_green);
	} else {
		PIN_Reset(&pin_led_green);
	}

	if (delta >= _LED_PERIOD_)
		tick_old = now;
}

/**
 * @brief  Helper utility to convert uint32_t to string.
 */
static char* uint32_to_string(uint32_t n, char *buffer, size_t buffer_size) {
    if (buffer_size < 11) return NULL;

    if (n == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    char temp[11];
    int i = 0;

    while (n > 0) {
        temp[i++] = '0' + (n % 10);
        n /= 10;
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';

    return buffer;
}


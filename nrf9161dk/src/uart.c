#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
/* STEP 3 - Include the header file of the UART driver in main.c */
#include <zephyr/drivers/uart.h>
#include "uart.h"
#include <zephyr/logging/log.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* STEP 10.1.1 - Define the size of the receive buffer */
#define RECEIVE_BUFF_SIZE 10

/* STEP 10.2 - Define the receiving timeout period */
#define RECEIVE_TIMEOUT 100

/* STEP 10.1.2 - Define the receive buffer */
static uint8_t rx_buf[RECEIVE_BUFF_SIZE] = {0};
static uint8_t tx_buf[sizeof(data_point)] = {0};
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));
LOG_MODULE_REGISTER(UART_Module, LOG_LEVEL_INF);

const struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};



static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {

	case UART_RX_RDY:
		if ((evt->data.rx.len) == 1) {

			LOG_INF("qwuiwhj");
		}
		break;
	case UART_RX_DISABLED:
		uart_rx_enable(dev, rx_buf, sizeof rx_buf, RECEIVE_TIMEOUT);
		break;
	default:
		break;
	}
}
int uart_send_data(data_point out_data){
    data_point tx_data = out_data;
    memcpy(tx_buf, &tx_data, sizeof(tx_data));
    LOG_INF("Sending data over UART1");
    if (!device_is_ready(uart2)){
		LOG_ERR("UART device not ready\r\n");
		return 1 ;
	}
    return uart_tx(uart2, tx_buf, sizeof(tx_buf), SYS_FOREVER_MS);
    uart_poll_out(uart2, 65);
    return 0;
}

int uart_module_init(void){
    int err = uart_callback_set(uart2, uart_cb, NULL);
    if(err != 0){
        LOG_ERR("Callback set failed %d", err);
    }
    uart_configure(uart2, &uart_cfg);
    return 0;
}
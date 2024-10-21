#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
/* STEP 3 - Include the header file of the UART driver in main.c */
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include "uart.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* STEP 10.2 - Define the receiving timeout period */
#define RECEIVE_TIMEOUT 1000

/* STEP 10.1.2 - Define the receive buffer */
static uint8_t rx_buf_data[sizeof(data_point)] = {0};
static uint8_t rx_buff_upd[sizeof(update_point)] = {0};
static uint8_t tx_buf[sizeof(data_point)] = {0};
update_point* device_locks_uart;
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));
LOG_MODULE_REGISTER(UART_Module, LOG_LEVEL_INF);
data_point* last_data_point;

uart_module_callback_t user_callback;

const struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};



static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	LOG_INF("Callback called");
	switch (evt->type) {

	case UART_RX_RDY:
		if ((evt->data.rx.len) == 1) {

			LOG_INF("qwuiwhj");
		} else if(evt->data.rx.len == sizeof(data_point)){
			LOG_INF("qwjio");
			void* in_data = &(evt->data.rx.buf[evt->data.rx.offset]);
			LOG_INF("fqwjio");
			printk("%x", *last_data_point);
			//*last_data_point = *((data_point*)in_data);
			memcpy((void*)last_data_point, in_data, sizeof(data_point));
			LOG_INF("UART data_point received");
			user_callback(*last_data_point);
		} 
		break;
	case UART_RX_DISABLED:
		
		uart_rx_enable(dev, rx_buf_data, sizeof(rx_buf_data), RECEIVE_TIMEOUT);
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
    return 0;
}

int uart_module_init(void){
    int err = uart_callback_set(uart2, uart_cb, NULL);
    if(err != 0){
        LOG_ERR("Callback set failed %d", err);
    } else {
		LOG_INF("Callback set success");
	}
    err = uart_configure(uart2, &uart_cfg);
	if(err != 0){
		LOG_ERR("UART config failed %d", err);
	}
	last_data_point =  k_calloc(1, sizeof(data_point));
	uart_rx_enable(uart2, rx_buf_data, sizeof(rx_buf_data), RECEIVE_TIMEOUT);
    return 0;
}

void uart_coap_callback_set(uart_module_callback_t callback){
	user_callback = callback;
}
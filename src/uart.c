#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
/* STEP 3 - Include the header file of the UART driver in main.c */
#include <zephyr/drivers/uart.h>
#ifndef UART_H_
#include "uart.h"
#endif
#ifndef MESH_H_
#include "mesh.h"
#endif
#ifndef MESH_R_H
#include "mesh-root.h"
#endif
#include <zephyr/logging/log.h>
#include <zephyr/sys/dlist.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* STEP 10.2 - Define the receiving timeout period */
#define RECEIVE_TIMEOUT 10000

#define BUFF_SIZE 24

/* STEP 10.1.2 - Define the receive buffer */
uint8_t* rx_buf_data_1;
uint8_t* rx_buf_data_2;
uint8_t* next_buf;
static uint8_t tx_buf[sizeof(data_point)] = {0};
sys_dlist_t* device_list_uart; // devices
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));
LOG_MODULE_REGISTER(UART_Module, LOG_LEVEL_INF);
data_point* last_data_point;

const struct uart_config uart_cfg = {
		.baudrate = 9600,
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

		} else if(evt->data.rx.len == sizeof(data_point)){
			
			void* in_data = &(evt->data.rx.buf[evt->data.rx.offset]);
			
			//printk("%x", *last_data_point);
			//*last_data_point = *((data_point*)in_data);
			memcpy((void*)last_data_point, in_data, sizeof(data_point));
			LOG_INF("UART data_point received: %d", evt->data.rx.offset);
		} else if(evt->data.rx.len == sizeof(update_point)){
			// void* in_data = &(evt->data.rx.buf[evt->data.rx.offset]);
			// update_point new_update_point = *((update_point*)in_data);

			void* in_data = &(evt->data.rx.buf[evt->data.rx.offset]);
			//LOG_INF("1");
			update_point new_update_point;
			memcpy(&new_update_point, in_data, sizeof(update_point));
			
			device_in_list* current_ptr;
			LOG_INF("%x", device_list_uart);
			SYS_DLIST_FOR_EACH_CONTAINER(device_list_uart, current_ptr, node){
				LOG_INF("wfwef");
				if(current_ptr && current_ptr->hwid == new_update_point.hwid){
					current_ptr->locked = new_update_point.locked;
					LOG_INF("State updated, [%d, %d]", current_ptr->hwid, current_ptr->locked);
				}
			}
			LOG_INF("UART update_point received %d [%d, %d]", evt->data.rx.offset, new_update_point.hwid, new_update_point.locked);
		} else {
			LOG_ERR("Malformed UART received (wrong length: %d)", evt->data.rx.len);
		}
		uart_rx_disable(uart2);
		uart_rx_enable(uart2, next_buf, BUFF_SIZE, RECEIVE_TIMEOUT);
		LOG_INF("switched buffers");
		if(next_buf == rx_buf_data_1){
			next_buf = rx_buf_data_2;
		} else {
			next_buf = rx_buf_data_1;
		}
		// uint8_t* old_buf = evt->data.rx_buf.buf;
		// uart_rx_buf_rsp(uart2, next_buf, BUFF_SIZE);
		// next_buf = old_buf;
		break;
	case UART_RX_BUF_REQUEST:
			LOG_INF("BRQ");
			//uart_rx_buf_rsp(uart2, next_buf, BUFF_SIZE);
			// LOG_INF("ewfiuhweiufh");
			break;
	case UART_RX_BUF_RELEASED:
		LOG_INF("BRL");
		//next_buf = evt->data.rx_buf.buf;
	break;
	case UART_RX_DISABLED:
		uart_rx_enable(uart2, next_buf, BUFF_SIZE, RECEIVE_TIMEOUT);
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
	LOG_INF("UART Init Success");
    return 0;
}



int uart_main(data_point* uart_data_point, sys_dlist_t* devices){
	device_list_uart = devices;
	rx_buf_data_1 = k_calloc(BUFF_SIZE, sizeof(uint8_t));
	rx_buf_data_2 = k_calloc(BUFF_SIZE, sizeof(uint8_t));
	next_buf = rx_buf_data_2;
	uart_module_init();
	uart_rx_enable(uart2, rx_buf_data_1, BUFF_SIZE, RECEIVE_TIMEOUT);
	last_data_point = uart_data_point;
	
	return 0;
}
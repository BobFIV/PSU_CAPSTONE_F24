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
#define RECEIVE_BUFF_SIZE 40

/* STEP 10.2 - Define the receiving timeout period */
#define RECEIVE_TIMEOUT 10000

/* STEP 10.1.2 - Define the receive buffer */
static uint8_t rx_buf[MAX(sizeof(data_point), sizeof(update_point))] = {0};
static uint8_t tx_buf_d[sizeof(data_point)] = {0};
static uint8_t tx_buf_u[sizeof(update_point)] = {0};
const struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart1));
LOG_MODULE_REGISTER(UART_Module, LOG_LEVEL_INF);
sys_dlist_t* data_point_queue;
struct k_sem* data_sem;

K_SEM_DEFINE(uart_sem, 1, 1);

const struct uart_config uart_cfg = {
		.baudrate = 9600,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};



static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	//LOG_INF("Callback called");
	switch (evt->type) {

	case UART_RX_RDY:
		if ((evt->data.rx.len) == 1) {

			LOG_INF("qwuiwhj");
		} else if((evt->data.rx.len) == sizeof(data_point)){
			LOG_INF("DP RECEIVED");
			void* in_data = &(evt->data.rx.buf[evt->data.rx.offset]);
			//LOG_INF("1");
			data_point new_data_point;
			memcpy(&new_data_point, in_data, sizeof(data_point));
			//LOG_INF("2");
			bool has_replaced_old_data = false;
			dpq_item* queue_item;
			SYS_DLIST_FOR_EACH_CONTAINER(data_point_queue, queue_item, node){
				if(queue_item->dp.hwid == new_data_point.hwid){
					queue_item->dp = new_data_point;
					has_replaced_old_data = true;
					LOG_INF("REPLACING OLD DATA FOR %d", new_data_point.hwid);
					break;
				}
			}
			if(!has_replaced_old_data){
				dpq_item* new_q_item = k_calloc(1, sizeof(dpq_item));
				//LOG_INF("3");
				new_q_item->dp = new_data_point;
				sys_dnode_init(&(new_q_item->node));
				sys_dlist_append(data_point_queue, &(new_q_item->node));
			}
			if(new_data_point.parent == 0){
				LOG_INF("Final DP Received, giving the semaphore");
				k_sem_give(data_sem); // ROOT'S OWN DATA POINT COMES LAST, SEND DATA BURST
			}
			
		} else {
			LOG_ERR("WRONG LENGTH %d", evt->data.rx.len);
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
    memcpy(tx_buf_d, &tx_data, sizeof(tx_data));
    LOG_INF("Sending data over UART1");
    if (!device_is_ready(uart2)){
		LOG_ERR("UART device not ready\r\n");
		return 1 ;
	}
    return uart_tx(uart2, tx_buf_d, sizeof(tx_data), SYS_FOREVER_MS);
}
int uart_send_upd(update_point out_data){
	k_sem_take(&uart_sem, K_FOREVER);
    update_point tx_data = out_data;
    memcpy(tx_buf_u, &tx_data, sizeof(tx_data));
    LOG_INF("Sending data over UART1 [%d, %d]", out_data.hwid, out_data.locked);
    if (!device_is_ready(uart2)){
		LOG_ERR("UART device not ready\r\n");
		return 1 ;
	}
    int er = uart_tx(uart2, tx_buf_u, sizeof(tx_data), SYS_FOREVER_MS);
	k_sem_give(&uart_sem);
    return er;
}

int uart_module_init(sys_dlist_t* dpq, struct k_sem* d_sem){
	data_sem = d_sem;
    int err = uart_callback_set(uart2, uart_cb, NULL);
    if(err != 0){
        LOG_ERR("Callback set failed %d", err);
    }
    uart_configure(uart2, &uart_cfg);
	
	data_point_queue = dpq;
	uart_rx_enable(uart2, rx_buf, sizeof(rx_buf), RECEIVE_TIMEOUT);
    return 0;
}
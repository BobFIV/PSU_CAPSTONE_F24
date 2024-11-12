/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <zephyr/net/coap.h>
#include "i2c.h"
#include "lte.h"
#include "coap_onem2m.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "gnss.h"
#include <nrf_modem_gnss.h>
#include <zephyr/sys/dlist.h>
#include "uart.h"

#define nanf -0.0f

#define TIME_TO_WAIT_FOR_RESPONSES 800

data_point current_data_point = {
	.temperature = 0,
	.speed = 0,
	.latitude = nanf,
	.longitude = nanf,
	.accelX = 0,
	.accelY = 0,
	.accelZ = 0,
	.snr = 0,
	.parent = 0
};

sys_dlist_t datapoint_queue;
int resolve_address_lock = 0;

//Register this as the main module
LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);

int main_sens(void);
int main_uart(void);
int main_gnss(void);
int main_coap(void);
int coap_receiver(void);
int empty_queue_to_server(void);

struct resource_data data_point_to_resource(data_point data);

K_SEM_DEFINE(op_sem, 1, 1);
K_SEM_DEFINE(modem_sem, 1, 1);
K_SEM_DEFINE(data_pack_sem, 0, 1);
K_SEM_DEFINE(rts_sem, 0, 1);
K_SEM_DEFINE(finished_sending, 0, 1);


K_THREAD_DEFINE(sens, 1024, main_sens, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(uart, 1024, main_uart, NULL, NULL, NULL, 1, 0, 0);
K_THREAD_DEFINE(gnss, 1024, main_gnss, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(coap, 2048, main_coap, NULL, NULL, NULL, 0, 0, 0);
K_THREAD_DEFINE(receiver, 2048, coap_receiver, NULL, NULL, NULL, 1, 0, 0);

int main_sens(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
		return -1;
	}
	/* Setting up the i2c device */
	
	
	i2c_init_temp_probe();
	init_acc_probe();

	gnss_init();

	while (1) {
		k_msleep(4200);
		k_sem_take(&op_sem, K_FOREVER);
		current_data_point.temperature = (int8_t)i2c_get_temp();
		int16_t* accdata = i2c_get_acc();
		current_data_point.accelX = (int8_t)accdata[0];
		LOG_INF("Accelerometer: %d", accdata[0] + accdata[1] + accdata[2]);
		current_data_point.accelY = (int8_t)accdata[1];
		current_data_point.accelZ = (int8_t)accdata[2];
		k_free(accdata);
		k_sem_give(&op_sem);
	}
	return 0;
}

int main_uart(void){
	sys_dlist_init(&datapoint_queue);
	k_sem_take(&op_sem, K_FOREVER);
	uart_module_init(&datapoint_queue, &data_pack_sem);
	k_sem_give(&op_sem);
	while(1){
		
		k_msleep(5144);
		k_sem_take(&op_sem, K_FOREVER);
		uart_send_data(current_data_point);
		k_sem_give(&op_sem);
	}
	return 0;
}

int main_gnss(void){
	k_thread_suspend(k_current_get());
	k_sem_take(&op_sem, K_FOREVER);
	gnss_init();
	k_sem_give(&op_sem);
	while (1)
	{
		// Wait for GNSS fix
		LOG_INF("Waiting for GNSS");
		k_sem_take(&gnss_fix_obtained, K_FOREVER);
		k_sem_take(&op_sem, K_FOREVER);
		struct nrf_modem_gnss_pvt_data_frame gnss_data = get_current_pvt();
		current_data_point.latitude = gnss_data.latitude;
		current_data_point.longitude = gnss_data.longitude;
		current_data_point.speed = gnss_data.speed;
		k_sem_give(&op_sem);
		k_thread_resume(coap);
		
	}
	return 0;
	
}

int main_coap(void){
	
	k_sem_take(&op_sem, K_FOREVER);
	LOG_INF("Test");
	int err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}
	LOG_INF("Modem configured");
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err != 0){
		LOG_ERR("Failed to decativate LTE and enable GNSS functional mode");
	}
	LOG_INF("LTE activated");
	k_sem_give(&op_sem);
	k_thread_resume(gnss);
	//k_thread_suspend(k_current_get());
	
	while (1) {
		k_sem_take(&data_pack_sem, K_FOREVER);
		LOG_INF("LTE thread active");
		k_thread_suspend(gnss);
		if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL) != 0) {
			LOG_ERR("Failed to activate LTE");
			return 0;
		}
		k_sem_take(&lte_connected, K_FOREVER);
		// Connect to the CoAP server
		if (resolve_address_lock == 0){
			LOG_INF("Resolving the server address\n\r");
			if (server_resolve() != 0) {
				LOG_ERR("Failed to resolve server name\n");
					return 0;
			}
			resolve_address_lock = 1;
		}

		if (client_init() != 0) {
			LOG_INF("Failed to initialize CoAP client");
			return 0;
		}

		
		k_thread_resume(receiver);
		empty_queue_to_server();
		k_msleep(TIME_TO_WAIT_FOR_RESPONSES);
		k_thread_suspend(receiver);

		onem2m_close_socket();

		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
		if (err != 0){
			LOG_ERR("Failed to decativate LTE and enable GNSS functional mode");
			break;
		}
		k_thread_resume(gnss);
	}
}

int coap_receiver(void){
	k_thread_suspend(k_current_get());
	while(1){
		int received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d, exit", errno);
			//break;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		/* Parse the received CoAP packet */
		int err = onem2m_parse(received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit");
			//break;
		} else if(err > 2){
			update_point out = {
				.hwid = err/2,
				.locked = err%2
			};
			LOG_INF("Sending update: [%d, %d]", out.hwid, out.locked);
			
			k_sem_take(&op_sem, K_FOREVER);
			uart_send_upd(out);
			k_sem_give(&op_sem);
		}
	}
}

int empty_queue_to_server(void){
	dpq_item* item_pointer;
	dpq_item* scratchptr;
	int received;
	int err;
	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&datapoint_queue, item_pointer, scratchptr, node) {
		struct resource_data resource = data_point_to_resource(item_pointer->dp);
		LOG_INF("latitude: %f", resource.bikedata.latie);
		get_coap_tx_resource(item_pointer->dp.hwid);
		if (client_put_send(resource, BIKEDATA) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
		}
		k_msleep(80);
		if (client_put_send(resource, BATTERY) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
		}
		k_msleep(80);
		if (client_put_send(resource, MESH_CONNECTIVITY) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
		}
		k_msleep(80);
		if (client_get_send(LOCK) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
		}
		k_msleep(80);
		sys_dlist_remove(&(item_pointer->node));
		k_free(item_pointer);
		//k_msleep(75);

	}
}

struct resource_data data_point_to_resource(data_point data){
	struct resource_data resource;
	struct battery battery_placeholder = {
		.lvl = 40,
		.lowBy = false
	};
	struct bike bike_placeholder = {
		.latie = data.latitude,
		.longe = data.longitude,
		.tempe = (double) data.temperature,
		.speed = (double) data.speed,
		.accel = (double) (data.accelX + data.accelY + data.accelZ)
	};
	LOG_INF("lat: %f", bike_placeholder.latie);
	LOG_INF("Temp: %d", data.temperature);
	
	struct mesh_connectivity mesh_placeholder = {
		.neibo = data.parent,
		.stnr = (int)data.snr
	};
	//snprintk(&(mesh_placeholder.neibo[0]), 6,"%05d" , data.hwid);
	
	struct lock lock_placeholder = {
		.lock = true
	};
	resource.batterydata = battery_placeholder;
	resource.bikedata = bike_placeholder;
	
	resource.lockdata = lock_placeholder;
	resource.meshdata = mesh_placeholder;
	// resource.bikedata.latie = data.latitude;
	// resource.bikedata.longe = data.longitude;
	LOG_INF("lati: %f", resource.bikedata.latie);
	return resource;
}
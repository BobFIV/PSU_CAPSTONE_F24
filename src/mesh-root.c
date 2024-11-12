/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/hwinfo.h>
#include "mesh-root.h"
#ifndef MESH_H_
#include "mesh.h"
#endif
#ifndef UART_H_
#include "uart.h"
#endif
#include <dk_buttons_and_leds.h>
#define NAN 0.0f/0.0f
#include "main.h"
#include <stdlib.h>


LOG_MODULE_REGISTER(MeshR_Module, LOG_LEVEL_INF);

sys_dlist_t* device_dlist;
device_in_list* my_node;





bool has_received_request_root;
static uint16_t hwid;
//static uint16_t my_index;
data_point* current_data;


int data_receipt_root(dect_packet data, int snr);
uint16_t device_waiting;
bool continue_listening;
//update_point* device_locks_mesh;

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* Send a GET request or PUT request upon button triggers */
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) 
	{
		device_in_list* s_ptr;
		device_in_list* current_ptr;
		SYS_DLIST_FOR_EACH_CONTAINER_SAFE(device_dlist, current_ptr, s_ptr, node){
			current_ptr->locked = !current_ptr->locked;
		}
	}
}

static void device_add(uint16_t hwid, bool locked){
	device_in_list* device = k_calloc(1, sizeof(device_in_list));
	sys_dnode_init(&(device->node));
	device->hwid = hwid;
	device->locked = locked;
	sys_dlist_prepend(device_dlist, &(device->node));
	LOG_INF("added %d", hwid);
}
static void device_remove(device_in_list* device){
	sys_dlist_remove(&(device->node));
	LOG_INF("removed %d", device->hwid);
	free(device);
}

int mesh_root(struct k_sem* mesh_sem, data_point* last_point, sys_dlist_t* device_list)
{
	sys_dlist_init(device_list); //Initialize the dlist and add itself to it
	LOG_INF("Init.");
	k_thread_suspend(k_current_get()); // DEFAULT IS NON_ROOT
	hwinfo_get_device_id((void *)&hwid, sizeof(hwid));

	//
	device_dlist = device_list;
	device_add(hwid, true);
	device_add(0, true);
	
	current_data = last_point;
	dk_buttons_init(button_handler);
	dk_leds_init();
	
	// for(int i = 0; i < NUM_DEVICES; i++){
	// 	update_point initial_update_point = {
	// 		.hwid = all_devices[i],
	// 		.locked = true
	// 	};
	// 	locks[i] = initial_update_point;
	// 	device_locks_mesh = locks;
	// }
	dect_init();
	
	dect_set_callback(&data_receipt_root);
	bool is_connected[NUM_DEVICES] = {false};

	LOG_INF("I am %u", hwid);
	while (1) {
		LOG_INF("***********************************");
		LOG_INF("I am %u", hwid);

		dect_packet ping_point = {
			.is_ping = true,
			.is_request = false,
			.hwid = hwid,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = 0,
			.longitude = 0,
			.speed = 0,
			.accelX = 0,
			.accelY = 0,
			.accelZ = 0,
			.temperature = 0,
			.locked = false
		};
		dect_packet request_point = {
			.is_ping = false,
			.is_request = true,
			.hwid = 0,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = 0,
			.longitude = 0,
			.speed = 0,
			.temperature = 0,
			.accelX = 0,
			.accelY = 0,
			.accelZ = 0,
			.locked = false
		};
		dect_packet ack_packet = {
			.is_ping = true,
			.is_request = true,
			.hwid = hwid,
			.sender_hwid = 0,
			.target_hwid = 0,
			.latitude = 0,
			.longitude = 0,
			.temperature = 0,
			.accelX = 0,
			.accelY = 0,
			.accelZ = 0,
			.speed = 0,
			.locked = false
		};
		dect_send(ping_point);
		
		LOG_INF("Mesh-building Complete.");
		k_msleep(CONCURRENCY_DELAY);
		LOG_INF("");

		// dect_packet* point_array = k_calloc(NUM_MAX_DEVICES, sizeof(dect_packet));
		// int counter = 0;
		// dect_packet* pointer = point_array;

		/** Receiving messages for CONFIG_RX_PERIOD_MS miliseconds. */
		// for(int i = 0; i < NUM_DEVICES; i++){ // send a request to each device and wait for it to respond.
		// 	k_msleep(30); // REMOVE FOR TESTING HIGHER SPEEDS
		// 	request_point.hwid = all_devices[i];
		// 	request_point.locked = locks[i].locked;
		// 	device_waiting = all_devices[i];
		// 	if(device_waiting == hwid){
		// 		continue;
		// 	}
		// 	has_received_request_root = false;
		// 	int err = dect_send(request_point);
		// 	while (err != 0)
		// 	{
		// 		dect_send(request_point);
		// 	}
			
		// 	LOG_INF("Request sent to device #%u", device_waiting);
			
		// 	continue_listening = false;
		// 	dect_listen();
			
		// 	while (continue_listening) {
		// 		continue_listening = false;
		// 		dect_listen();
		// 	}
			
		// 	if(!has_received_request_root){
		// 		LOG_ERR("Timed out.");
		// 		is_connected[i] = false;
		// 	}
		// 	if(has_received_request_root){
		// 		is_connected[i] = true;
		// 	}
		// 	k_msleep(CONCURRENCY_DELAY);
			
			
		// }
		device_in_list* s_ptr;
		device_in_list* current_ptr;
		SYS_DLIST_FOR_EACH_CONTAINER_SAFE(device_dlist, current_ptr, s_ptr, node){
			k_msleep(30); // REMOVE FOR TESTING HIGHER SPEEDS
			request_point.hwid = current_ptr->hwid;
			request_point.locked = current_ptr->locked;
			device_waiting = current_ptr->hwid;
			if(device_waiting == hwid){
				my_node = current_ptr;
				continue;
			}
			has_received_request_root = false;
			int err = dect_send(request_point);
			while (err != 0)
			{
				dect_send(request_point);
			}
			
			LOG_INF("Request sent to device #%u", device_waiting);
			
			if(device_waiting == 0){
				dect_listen_cont();
			} else {
				continue_listening = false;
				dect_listen();
				
				while (continue_listening) {
					continue_listening = false;
					dect_listen();
				}
			}
			
			
			
			if(!has_received_request_root && device_waiting != 0){
				LOG_ERR("Timed out."); // last request (0) should tiem out
				device_remove(current_ptr);
			}
			if(has_received_request_root){
				LOG_INF("Did not time out.");
			}
			k_msleep(CONCURRENCY_DELAY);
		}
		dect_send(ack_packet);
		LOG_INF("Receipt      Complete.");
		last_point->hwid = hwid;
		last_point->parent = 0;
		last_point->snr = 255;
		dk_set_led(DK_LED1, my_node->locked);
		uart_send_data(*last_point);
		LOG_INF("    Latitude:       %0.6f", (double)last_point->latitude);
		LOG_INF("    Longitude:      %0.6f", (double)last_point->longitude);
		LOG_INF("    Temperature:    %0.6f", (double)last_point->temperature);
		LOG_INF("    Accel:          [%0.2f, %0.2f, %0.2f]", (double)last_point->accelX, (double)last_point->accelY, (double)last_point->accelZ);
		LOG_INF("    Speed:          %0.6f", (double)last_point->speed);
		LOG_INF("    Signal:         %d", last_point->snr);
		LOG_INF("    Parent:         %d", last_point->parent);
		LOG_INF("    Locked:         NA");
		LOG_INF("Sent my own data via UART");
		LOG_INF("");
		//LOG_ERR("END");
		for(int i = 0; i < WAIT_BEFORE_NEXT_CYCLE / CONFIG_RX_PERIOD_MS; i++){
			dect_listen_cont();
		}
		k_msleep(ADDITIONAL_WAIT);
		k_msleep(rand()%64);

	}

	dect_close();

	return 0;
}

int data_receipt_root(dect_packet data, int snr){
	if(data.is_request && data.is_ping){
		LOG_INF("\tACK.");
		if(data.hwid != hwid){
			LOG_INF("--------------------Switching to node--------------------");
			switch_node_root();
		}
	} else if(data.is_request){
		LOG_INF("\trequest.");
	} else if(data.is_ping){
		LOG_INF("\tPing.");
	} else {
		LOG_INF("\tPacket from %d to %d.", data.sender_hwid, data.target_hwid);
	}
	if(!data.is_ping && data.target_hwid == hwid){ //if it's not a ping, store it.
		LOG_INF("Originator:              %u", data.hwid);
		LOG_INF("Sender:                  %u", data.sender_hwid);
		LOG_INF("Intended Target:         %u", data.target_hwid);
		//LOG_INF("Actual recipient (root): %u", hwid);
		LOG_INF("    Latitude:       %0.6f", (double)data.latitude);
		LOG_INF("    Longitude:      %0.6f", (double)data.longitude);
		LOG_INF("    Temperature:    %0.6f", (double)data.temperature);
		LOG_INF("    Accel:          [%0.2f, %0.2f, %0.2f]", (double)data.accelX, (double)data.accelY, (double)data.accelZ);
		LOG_INF("    Speed:          %0.6f", (double)data.speed);
		LOG_INF("    Signal:         %d", data.snr);
		LOG_INF("    Parent:         %d", data.op_hwid);
		LOG_INF("    Locked:         %d", (int)data.locked);
		data_point uart_out = {
			.hwid = data.hwid,
			.parent = data.op_hwid,
			.temperature = data.temperature,
			.speed = data.speed,
			.snr = data.snr,
			.latitude = data.latitude,
			.longitude = data.longitude,
			.accelX = data.accelX,
			.accelY = data.accelY,
			.accelZ = data.accelZ,
		};
		uart_send_data(uart_out);
		if(device_waiting == 0){
			device_add(data.hwid, data.locked);
		}
		has_received_request_root = true;
	} else {
		//LOG_INF("Ping.");
	}
	if(device_waiting == data.hwid && !data.is_request && !data.is_ping && data.target_hwid == hwid){ //The data packet is the form of sending the request back. This is counterintuitive but it's fine.
		//has_received_request_root = true;
	}
	else{
		continue_listening = true;
	}
	return 0;
}

int mesh_root_switch_init(void){
	dect_set_callback(&data_receipt_root);
	return 0;
}

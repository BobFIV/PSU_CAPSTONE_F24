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







bool has_received_request_root;
static uint16_t hwid;
static uint16_t my_index;


int data_receipt_root(dect_packet data, int rssi);
uint16_t device_waiting;
bool continue_listening;
update_point* device_locks_mesh;

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* Send a GET request or PUT request upon button triggers */
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) 
	{
		for(int i = 0; i < NUM_DEVICES; i++){
			if(device_locks_mesh[i].locked){
				device_locks_mesh[i].locked = false;
			} else {
				device_locks_mesh[i].locked = true;
			}
		}
		
	}
}

int mesh_root(update_point* locks, struct k_sem* mesh_sem)
{
	k_thread_suspend(k_current_get()); // DEFAULT IS NON_ROOT
	dk_buttons_init(button_handler);
	dk_leds_init();
	for(int i = 0; i < NUM_DEVICES; i++){
		update_point initial_update_point = {
			.hwid = all_devices[i],
			.locked = true
		};
		locks[i] = initial_update_point;
		device_locks_mesh = locks;
	}
	dect_init();
	hwinfo_get_device_id((void *)&hwid, sizeof(hwid));
	dect_set_callback(&data_receipt_root);
	bool is_connected[NUM_DEVICES] = {false};

	LOG_INF("I am %u", hwid);
	while (1) {
		//LOG_ERR("Start");
		LOG_INF("***********************************");
		LOG_INF("I am %u", hwid);

		dect_packet ping_point = {
			.is_ping = true,
			.is_request = false,
			.hwid = hwid,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.speed = NAN,
			.temperature = NAN,
			.locked = false
		};
		dect_packet request_point = {
			.is_ping = false,
			.is_request = true,
			.hwid = 0,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.speed = NAN,
			.temperature = NAN,
			.locked = false
		};
		dect_packet ack_packet = {
			.is_ping = true,
			.is_request = true,
			.hwid = hwid,
			.sender_hwid = 0,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.temperature = NAN,
			.speed = NAN,
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
		for(int i = 0; i < NUM_DEVICES; i++){ // send a request to each device and wait for it to respond.
			k_msleep(30); // REMOVE FOR TESTING HIGHER SPEEDS
			request_point.hwid = all_devices[i];
			request_point.locked = locks[i].locked;
			device_waiting = all_devices[i];
			if(device_waiting == hwid){
				continue;
			}
			has_received_request_root = false;
			int err = dect_send(request_point);
			while (err != 0)
			{
				dect_send(request_point);
			}
			
			LOG_INF("Request sent to device #%u", device_waiting);
			
			continue_listening = false;
			dect_listen();
			
			while (continue_listening) {
				continue_listening = false;
				dect_listen();
			}
			
			if(!has_received_request_root){
				LOG_ERR("Timed out.");
				is_connected[i] = false;
			}
			if(has_received_request_root){
				is_connected[i] = true;
			}
			k_msleep(CONCURRENCY_DELAY);
			
			
		}
		dect_send(ack_packet);
		LOG_INF("Receipt      Complete.");
		LOG_INF("");
		//LOG_ERR("END");
		for(int i = 0; i < WAIT_BEFORE_NEXT_CYCLE / CONFIG_RX_PERIOD_MS; i++){
			dect_listen_cont();
		}
		k_msleep(ADDITIONAL_WAIT);
		k_msleep(rand()%32);

	}

	dect_close();

	return 0;
}

int data_receipt_root(dect_packet data, int rssi){
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
		LOG_INF("    Speed:          %0.6f", (double)data.speed);
		LOG_INF("    Signal:         %d", data.rssi);
		LOG_INF("    Parent:         %d", data.op_hwid);
		LOG_INF("    Locked:         %d", (int)data.locked);
		data_point uart_out = {
			.hwid = data.hwid,
			.parent = data.op_hwid,
			.temperature = data.temperature,
			.speed = data.speed,
			.rssi = data.rssi,
			.latitude = data.latitude,
			.longitude = data.longitude,
			.accelX = 0.0f,
			.accelY = -0.0f,
			.accelZ = -0.0f,
			.gyroX = -0.0f,
			.gyroY = -0.0f,
			.gyroZ = -0.0f,
		};
		uart_send_data(uart_out);
	} else {
		//LOG_INF("Ping.");
	}
	if(device_waiting == data.hwid && !data.is_request && !data.is_ping && data.target_hwid == hwid){ //The data packet is the form of sending the request back. This is counterintuitive but it's fine.
		has_received_request_root = true;
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

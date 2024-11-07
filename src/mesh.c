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

LOG_MODULE_REGISTER(MeshN_Module, LOG_LEVEL_INF);






bool has_received_ping;
bool has_a_child;
bool has_received_request_node;
bool is_added;
bool received_request_to_add;



dect_packet fwd_data;

static uint16_t hwid;
static uint16_t my_index;
int timeout_counter;

//extern uint16_t all_devices[] = {10639U, 32048U, 0U};






void concurrency_sleep(){
	k_usleep(CONCURRENCY_DELAY * 1000 + rand() % 4096);
}


int get_device_index(uint16_t input_hwid){
	
	return -1;
}

bool has_received_data;
int data_receipt_node(dect_packet data, int rssi);
uint16_t parent; //used to identify leaf nodes.
dect_packet echo_packet;
uint16_t root_id;
bool has_received_ack;
data_point* sensor_data;
bool is_locked;

int mesh_node(data_point* sens_data, struct k_sem* mesh_sem)
{
	is_locked = true;
	dk_leds_init();
	
	k_sem_take(mesh_sem, K_FOREVER);
	sensor_data = sens_data;
	my_index = -1;
	dect_init();
	dect_set_callback(&data_receipt_node);
	hwinfo_get_device_id((void *)&hwid, sizeof(hwid));
	LOG_INF("Init");
	is_added = false;
	// for(int i = 0; i < NUM_DEVICES; i++){
	// 	if(all_devices[i] == hwid){
	// 		my_index = i;
	// 		LOG_INF("I am at index %d", i);
	// 		break;
	// 	}
	// 	if(my_index == -1){
	// 		LOG_ERR("Device is not in list. Please add it.");
	// 	}
	// }

	while (1) {
		
		LOG_INF("I am %u at #%d", hwid, my_index);
		LOG_INF("Waiting for ping from root.");
		has_received_ping = false;
		bool has_sent_ping = false;
		is_added = false;
		received_request_to_add = false;
		has_a_child = false;
		timeout_counter = 0;
		while(!has_received_ping){ // wait for a ping from the parent once the device is reachable.
			dect_listen();
			timeout_counter++;
			if(has_received_ping && !has_sent_ping){
				dect_packet out_data = {
					.is_ping = true,
					.hwid = root_id,
					.sender_hwid = hwid,
					.target_hwid = parent,
					.latitude = -0.0f,
					.longitude = -0.0f,
					.speed = -0.0f,
					.temperature = -0.0f,
					.op_hwid = 0,
					.locked = false
				};
				dect_send(out_data);
				has_sent_ping = true;
				LOG_INF("\tResponded.");
			}
			if(timeout_counter >= TIMEOUT_BECOME_ROOT/CONFIG_RX_PERIOD_MS){
				k_msleep(rand() % 32);
				LOG_INF("--------------------Switching to root--------------------");
				switch_node_root();
			}

		}
		
		LOG_INF("Mesh-building complete");

		//k_msleep(IDLE_TIME_BEFORE_SEND);

		//Now each non-root has a parent. The tree is formed

		has_received_data = false;

		dect_packet send_point = {
			.is_ping = false,
			.hwid = hwid, // HWID is the originator of the data
			.target_hwid = parent,
			.sender_hwid = hwid,
			.latitude =  sensor_data->latitude,
			.longitude = sensor_data->longitude,
			.speed = sensor_data->speed,
			.temperature = sensor_data->temperature,
			.rssi = sensor_data->rssi,
			.accelX = sensor_data->accelX,
			.accelY = sensor_data->accelY,
			.accelZ = sensor_data->accelZ,
			.op_hwid = parent,
			.locked = is_locked
		};
		
		// LOG_INF("Transmission Complete.");
		LOG_INF("");
		has_received_ack = false;
		has_received_ping = true;
		is_added = false;
		/** Receiving messages for CONFIG_RX_PERIOD_S seconds. */
		while(!has_received_ack){ // listen for 1 second to allow packets to propagata back up
			
			LOG_INF("Waiting for requests...");
			has_received_request_node = false;
			while(!has_received_request_node && !has_received_ack){
				dect_listen();
				if(has_received_data){
					//k_msleep(CONCURRENCY_DELAY);
					concurrency_sleep();
					LOG_INF("Forwarding data from %u to %u", echo_packet.sender_hwid, parent);
					dect_send(echo_packet);
					has_received_data = false;
				}
			}
			if(has_received_request_node){ //if it hasnt timed out
				if(echo_packet.hwid == 0 && !is_added) {
					k_msleep(1 + (rand() % 32));
					send_point.locked = is_locked;
					dect_send(send_point);
					
					LOG_INF("Received request to add myself, sent my data.");
					concurrency_sleep();
					is_added = true;
					dk_set_led(DK_LED1, is_locked);
					echo_packet.sender_hwid = hwid;
					dect_send(echo_packet);
					LOG_INF("Re-sent add request so everyone hears it.");
				} else if(echo_packet.hwid == hwid){
					k_msleep(15);
					is_locked = echo_packet.locked;
					send_point.locked = is_locked;
					//k_msleep(CONCURRENCY_DELAY*2);
					concurrency_sleep();
					dect_send(send_point);
					
					LOG_INF("Received request, sent my data.");
					concurrency_sleep();
					is_added = true;
					dk_set_led(DK_LED1, is_locked);
					echo_packet.sender_hwid = hwid;
					dect_send(echo_packet);
					LOG_INF("Re-sent my request so everyone hears it.");
				} else {
					concurrency_sleep();
					echo_packet.sender_hwid = hwid;
					dect_send(echo_packet);
					LOG_INF("Echoing request to ...%u", echo_packet.hwid%1000); // echo the request
				}
			} if(has_received_ack){
				dect_send(echo_packet);
				LOG_INF("ACK packet received and echoed");
			}
		}
		

		LOG_INF("Receipt      Complete.");
		LOG_INF("");
		k_sem_give(mesh_sem);
		k_msleep(WAIT_BEFORE_NEXT_CYCLE);
		

	}

	dect_close();

	return 0;
}

int data_receipt_node(dect_packet data, int rssi){
	dect_packet in_data = data;
	// if(in_data.sender_hwid == 17754051359676071216){
	// 	return 0;
	// 	LOG_INF("\tRoot.");
	// }
	if(in_data.is_request && in_data.is_ping){
		has_received_ack = true;
		LOG_INF("\tACK.");
		in_data.sender_hwid = hwid;
		in_data.target_hwid = 0;
		echo_packet = in_data;
	}
	if(in_data.is_request){
		LOG_INF("\trequest.");
	} else if(in_data.is_ping){
		LOG_INF("\tPing.");
	} else {
		LOG_INF("\tPacket.");
	}
	if(in_data.is_ping && !in_data.is_request){
		
		//LOG_INF("\tPinged.");
		if(!has_received_ping){
			has_received_ping = true;
			parent = in_data.sender_hwid;
			root_id = in_data.hwid;
			LOG_INF("Ping received! parent %u, gparent %u", parent, in_data.target_hwid); // parent of sender is in target, will call this "grandparent"
			sensor_data->rssi = rssi;
		} else if(!has_a_child && in_data.target_hwid == hwid){ //if it hears a ping from a node that claims it as parent, it has a child
			has_a_child = true;
		}
		
	} else if(in_data.is_request && !has_received_request_node && has_received_ping) {
		//LOG_INF("\trequest.");
		if(in_data.sender_hwid == parent){
			has_received_request_node = true;
			received_request_to_add = (in_data.sender_hwid == 0);
			LOG_INF("Received request for %u", in_data.hwid);
			echo_packet = in_data;
		}
	} else if(!data.is_request && !data.is_ping) {
		//LOG_INF("\tPacket.");
		//if the packet is actually meant for this device
		if(in_data.target_hwid == hwid) { // transmit it back to the parent
			LOG_INF("Forwarding packet from %u to %u that originated from %u", in_data.sender_hwid, parent, in_data.hwid);
			in_data.sender_hwid = hwid;
			
			in_data.target_hwid = parent;
			//dect_send(in_data);
			echo_packet = in_data;
			has_received_data = true;
		}
	}
	return 0;
}

int mesh_node_switch_init(void){
	dect_set_callback(&data_receipt_node);
	timeout_counter = 0;
}

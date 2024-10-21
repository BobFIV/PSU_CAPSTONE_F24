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
#include "mesh.h"

#define NAN 0.0f/0.0f

LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);






bool has_received_ping;
bool has_a_child;
bool has_received_vsem;

#define WAIT_BEFORE_NEXT_CYCLE 5000
#define ADDITIONAL_WAIT 50
#define CONCURRENCY_DELAY 1

dect_packet fwd_data;

uint16_t hwid;
uint16_t my_index;
uint16_t all_devices[] = {10639U, 46924U, 32048U};
#define NUM_DEVICES 3





//#define IS_ROOT







#ifdef IS_ROOT
int data_receipt_root(dect_packet data, int rssi);
uint16_t device_waiting;
bool continue_listening;

int mesh(data_point* point, struct k_sem* mesh_sem)
{
	dect_init();
	hwinfo_get_device_id((void *)&hwid, sizeof(hwid));
	dect_set_callback(&data_receipt_root);
	bool is_connected[NUM_DEVICES] = {false};

	LOG_INF("I am %u", hwid);
	while (1) {
		//LOG_ERR("Start");
		LOG_INF("***********************************");

		dect_packet ping_point = {
			.is_ping = true,
			.is_vsem = false,
			.hwid = hwid,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.speed = NAN,
			.temperature = NAN
		};
		dect_packet vsem_point = {
			.is_ping = false,
			.is_vsem = true,
			.hwid = 0,
			.sender_hwid = hwid,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.speed = NAN,
			.temperature = NAN
		};
		dect_packet ack_packet = {
			.is_ping = true,
			.is_vsem = true,
			.hwid = 0,
			.sender_hwid = 0,
			.target_hwid = 0,
			.latitude = NAN,
			.longitude = NAN,
			.temperature = NAN,
			.speed = NAN
		};
		dect_send(ping_point);
		
		LOG_INF("Mesh-building Complete.");
		k_msleep(CONCURRENCY_DELAY);
		LOG_INF("");

		// dect_packet* point_array = k_calloc(NUM_MAX_DEVICES, sizeof(dect_packet));
		// int counter = 0;
		// dect_packet* pointer = point_array;

		/** Receiving messages for CONFIG_RX_PERIOD_MS miliseconds. */
		for(int i = 0; i < NUM_DEVICES; i++){ // send a vsem to each device and wait for it to respond.
			vsem_point.hwid = all_devices[i];
			device_waiting = all_devices[i];
			if(device_waiting == hwid){
				continue;
			}
			has_received_vsem = false;
			int err = dect_send(vsem_point);
			while (err != 0)
			{
				dect_send(vsem_point);
			}
			
			LOG_INF("Request sent to device #%u", device_waiting);
			
			continue_listening = false;
			dect_listen();
			
			while (continue_listening) {
				continue_listening = false;
				dect_listen();
			}
			
			if(!has_received_vsem){
				LOG_ERR("Timed out.");
				is_connected[i] = false;
			}
			if(has_received_vsem){
				is_connected[i] = true;
			}
			k_msleep(CONCURRENCY_DELAY);
			
			
		}
		dect_send(ack_packet);
		LOG_INF("Receipt      Complete.");
		LOG_INF("");
		//LOG_ERR("END");
		k_msleep(WAIT_BEFORE_NEXT_CYCLE + ADDITIONAL_WAIT);

	}

	dect_close();

	return 0;
}

int data_receipt_root(dect_packet data, int rssi){
	if(data.is_vsem && data.is_ping){
		LOG_INF("\tACK.");
	} else if(data.is_vsem){
		LOG_INF("\tVsem.");
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
	} else {
		//LOG_INF("Ping.");
	}
	if(device_waiting == data.hwid && !data.is_vsem && !data.is_ping && data.target_hwid == hwid){ //The data packet is the form of sending the vsem back. This is counterintuitive but it's fine.
		has_received_vsem = true;
	}
	else{
		continue_listening = true;
	}
	return 0;
}
#else

bool has_received_data;
int data_receipt_node(dect_packet data, int rssi);
uint16_t parent; //used to identify leaf nodes.
dect_packet echo_packet;
uint16_t root_id;
uint16_t device_waiting;
bool has_received_ack;
data_point* sensor_data;

int mesh(data_point* sens_data, struct k_sem* mesh_sem)
{
	sensor_data = sens_data;
	my_index = -1;
	dect_init();
	dect_set_callback(&data_receipt_node);
	hwinfo_get_device_id((void *)&hwid, sizeof(hwid));
	
	for(int i = 0; i < NUM_DEVICES; i++){
		if(all_devices[i] == hwid){
			my_index = i;
			LOG_INF("I am at index %d", i);
			break;
		}
		if(my_index == -1){
			LOG_ERR("Device is not in list. Please add it.");
		}
	}

	while (1) {

		LOG_INF("I am %u at #%d", hwid, my_index);
		LOG_INF("Waiting for ping from root.");

		has_received_ping = false;
		bool has_sent_ping = false;
		has_a_child = false;
		while(!has_received_ping){ // wait for a ping from the parent once the device is reachable.
			dect_listen();
			if(has_received_ping && !has_sent_ping){
				dect_packet out_data = {
					.is_ping = true,
					.hwid = root_id,
					.sender_hwid = hwid,
					.target_hwid = parent,
					.latitude = -0.0f,
					.longitude = -0.0f,
					.speed = -0.0f,
					.temperature = -0.0f
				};
				dect_send(out_data);
				has_sent_ping = true;
				LOG_INF("\tResponded.");
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
			.rssi = sensor_data->rssi
		};
		
		// LOG_INF("Transmission Complete.");
		LOG_INF("");
		has_received_ack = false;
		has_received_ping = true;
		/** Receiving messages for CONFIG_RX_PERIOD_S seconds. */
		while(!has_received_ack){ // listen for 1 second to allow packets to propagata back up
			LOG_INF("Waiting for requests...");
			has_received_vsem = false;
			while(!has_received_vsem && !has_received_ack){
				dect_listen();
				if(has_received_data){
					k_msleep(CONCURRENCY_DELAY);
					LOG_INF("Forwarding data from %u to %u", echo_packet.sender_hwid, parent);
					dect_send(echo_packet);
					has_received_data = false;
				}
			}
			if(has_received_vsem){ //if it hasnt timed out
				if(echo_packet.hwid != hwid){
					echo_packet.sender_hwid = hwid;
					dect_send(echo_packet);
					LOG_INF("Echoing request to ...%u", echo_packet.hwid%1000); // echo the Vsem
				} else {
					k_msleep(CONCURRENCY_DELAY);
					dect_send(send_point);
					
					LOG_INF("Received request, sent my data.");
					echo_packet.sender_hwid = hwid;
					dect_send(echo_packet);
					LOG_INF("Re-sent my vsem so everyone hears it.");
				}
			} if(has_received_ack){
				dect_send(echo_packet);
				LOG_INF("ACK packet received and echoed");
			}
		}
		

		LOG_INF("Receipt      Complete.");
		LOG_INF("");
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
	if(in_data.is_vsem && in_data.is_ping){
		has_received_ack = true;
		LOG_INF("\tACK.");
		in_data.sender_hwid = hwid;
		in_data.target_hwid = 0;
		echo_packet = in_data;
	}
	if(in_data.is_vsem){
		LOG_INF("\tVsem.");
	} else if(in_data.is_ping){
		LOG_INF("\tPing.");
	} else {
		LOG_INF("\tPacket.");
	}
	if(in_data.is_ping && !in_data.is_vsem){
		
		//LOG_INF("\tPinged.");
		if(!has_received_ping){
			has_received_ping = true;
			parent = in_data.sender_hwid;
			root_id = in_data.hwid;
			LOG_INF("Ping received! parent ...%u, gparent ...%u", parent%1000, in_data.target_hwid%1000); // parent of sender is in target, will call this "grandparent"
			sensor_data->rssi = rssi;
		} else if(!has_a_child && in_data.target_hwid == hwid){ //if it hears a ping from a node that claims it as parent, it has a child
			has_a_child = true;
		}
		
	} else if(in_data.is_vsem && !has_received_vsem && has_received_ping) {
		//LOG_INF("\tVsem.");
		if(in_data.sender_hwid == parent){
			has_received_vsem = true;
			LOG_INF("Received request for ...%u", in_data.hwid%1000);
			echo_packet = in_data;
		}
	} else if(!data.is_vsem && !data.is_ping) {
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

#endif
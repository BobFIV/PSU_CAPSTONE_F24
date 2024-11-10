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
#include "lte.h"
#include "coap_onem2m.h"
#include <zephyr/net/coap.h>
#include "i2c.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "gnss.h"
#include <nrf_modem_gnss.h>
#include "battery.h"

int testing_mode = 2;

int resolve_address_lock = 0;
union resource_data data;

struct bike bike_placeholder = {
	.latie = 40.022253,
	.longe = -75.632425,
	.tempe = 70.00,
	.speed = 3.20,
	.accel = 1.80
};

struct battery battery_placeholder = {
	.lvl = 40,
	.lowBy = false
};

struct mesh_connectivity mesh_placeholder = {
	.neibo = "00000",
	.rssi = 1
};

struct lock lock_placeholder = {
	.lock = true
};

union resource_data resource_placeholder;

//Register this as the main module
LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) 
	{
		int err;
		int received;

		bike_placeholder.latie += 1.0;
		bike_placeholder.longe += 1.0;

		resource_placeholder.bikedata = bike_placeholder;
		
		err = client_put_send(resource_placeholder, BIKEDATA);
		if (err != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			return;
		}

		received = onem2m_receive();
		onem2m_parse(received);
	}
	else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK)
	{
		int err;
		int received;

		battery_placeholder.lvl = get_battery_level();

		resource_placeholder.batterydata = battery_placeholder;
		
		err = client_put_send(resource_placeholder, BATTERY);
		if (err != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			return;
		}
		
		received = onem2m_receive();
		onem2m_parse(received);
	}
	else if (has_changed & DK_BTN3_MSK && button_state & DK_BTN3_MSK)
	{
		{};
	}
	else if (has_changed & DK_BTN4_MSK && button_state & DK_BTN4_MSK)
	{
		{};
	}
}

int main(void)
{
	int err;
	int received;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	i2c_init_temp_probe();

	battery_init();

	if (testing_mode == 0){
		gnss_init();
	}

	while (1) {
		if (testing_mode == 0){
			// Wait for GNSS fix
			k_sem_take(&gnss_fix_obtained, K_FOREVER);
			
			// Acquire data from GNSS receiver and sensors
			struct nrf_modem_gnss_pvt_data_frame gnss_data = get_current_pvt();
			bike_placeholder.latie = gnss_data.latitude;
			bike_placeholder.longe = gnss_data.longitude;
		}
		
		bike_placeholder.tempe = i2c_get_temp();

		battery_placeholder.lvl = get_battery_level();

		// Activate LTE
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

		// Send the data to the CoAP server

		resource_placeholder.bikedata = bike_placeholder;

		if (client_put_send(resource_placeholder, BIKEDATA) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
			break;
		}
		
		/* Receive response from the CoAP server */
		received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d, exit", errno);
			break;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		/* Parse the received CoAP packet */
		err = onem2m_parse(received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit");
			break;
		}

		// Send the data to the CoAP server

		resource_placeholder.batterydata = battery_placeholder;

		if (client_put_send(resource_placeholder, BATTERY) != 0) {
			LOG_ERR("Failed to send PUT request, exit...\n");
			break;
		}

		/* Receive response from the CoAP server */
		received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d, exit", errno);
			break;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		/* Parse the received CoAP packet */
		err = onem2m_parse(received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit");
			break;
		}

		if (testing_mode > 0) {
			k_sleep(K_SECONDS(60));
		}

		// Disconnect from the CoAP server, deactivate LTE
		onem2m_close_socket();

		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
		if (err != 0){
			LOG_ERR("Failed to decativate LTE and enable GNSS functional mode");
			break;
		}

		if (testing_mode > 1) {
			break;
		}
	}

	onem2m_close_socket();

	return 0;
}

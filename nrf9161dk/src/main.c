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

struct data_point data_placeholder = {
	.temperature = 70.0f,
	.speed = 3.2f,
	.latitude = 40.022253f,
	.longitude = -75.632425f
};

//Register this as the main module
LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* Send a GET request or PUT request upon button triggers */
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) 
	{
		//client_get_send();
		data_placeholder.latitude += 0.03f;
		data_placeholder.longitude -= 0.03f;
		client_post_send(data_placeholder);
	}
	
	else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK)
	{
		data_placeholder.latitude += 0.03f;
		data_placeholder.longitude += 0.03f;
		client_post_send(data_placeholder);
	}
	else if (has_changed & DK_BTN3_MSK && button_state & DK_BTN3_MSK)
	{
		data_placeholder.latitude -= 0.03f;
		data_placeholder.longitude -= 0.03f;
		client_post_send(data_placeholder);
	}
	else if (has_changed & DK_BTN4_MSK && button_state & DK_BTN4_MSK)
	{
		data_placeholder.latitude -= 0.03f;
		data_placeholder.longitude += 0.03f;
		client_post_send(data_placeholder);
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

	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return 0;
	}

	if (client_init() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	while (1) {
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

	}

	onem2m_close_socket();

	return 0;
}

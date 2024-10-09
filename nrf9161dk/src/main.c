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

	/* Setting up the i2c device */
	
	static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);
	init_temp_probe(dev_i2c);
	/*if (!device_is_ready(dev_i2c.bus)) {
		printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
		return -1;
	}*/

	/* Setting it to freerun mode */
	
	/*uint8_t config[2] = {STTS751_CONFIG_REG,0b00000100};
	err = i2c_write_dt(&dev_i2c, config, sizeof(config));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config[0]);
		return -1;
	}*/

	
	while (1) {
		
		// Getting the temperature
		uint8_t temp_reading[2]= {0};
		uint8_t sensor_regs[2] ={STTS751_TEMP_LOW_REG,STTS751_TEMP_HIGH_REG};
		err = i2c_write_read_dt(&dev_i2c,&sensor_regs[0],1,&temp_reading[0],1);
		if (err != 0) {
			printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[0]);
		}
		err = i2c_write_read_dt(&dev_i2c,&sensor_regs[1],1,&temp_reading[1],1);
		if (err != 0) {
			printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[1]);
		}

		// Convert the two bytes
		int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0] & 0xF0)) / 16;
		if (temp > 2047) {
			temp -= 4096;
		}

		// Convert to degrees units 
		double cTemp = temp * 0.0625;
		double fTemp = cTemp * 1.8 + 32;
		
		// setting it as the output
		data_placeholder.temperature = fTemp;

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

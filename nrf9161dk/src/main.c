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

#define I2C0_NODE_TEMP DT_NODELABEL(sensor_temp)
#define I2C0_NODE_ACC DT_NODELABEL(sensor_acc)

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
/*
	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}
*/
	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}
/*
	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return 0;
	}

	if (client_init() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}
*/
	/* Setting up the i2c device */
	
	static const struct i2c_dt_spec dev_i2c_temp = I2C_DT_SPEC_GET(I2C0_NODE_TEMP);
	init_temp_probe(dev_i2c_temp);

	static const struct i2c_dt_spec dev_i2c_acc = I2C_DT_SPEC_GET(I2C0_NODE_ACC);
	init_acc_probe(dev_i2c_acc);

	
	while (1) {
		
		// Getting the temperature
		/*
		uint8_t temp_reading[2]= {0};
		uint8_t sensor_regs[2] ={STTS751_TEMP_LOW_REG,STTS751_TEMP_HIGH_REG};
		err = i2c_write_read_dt(&dev_i2c_temp,&sensor_regs[0],1,&temp_reading[0],1);
		if (err != 0) {
			printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_temp.addr,sensor_regs[0]);
		}
		err = i2c_write_read_dt(&dev_i2c_temp,&sensor_regs[1],1,&temp_reading[1],1);
		if (err != 0) {
			printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_temp.addr,sensor_regs[1]);
		}

		// Convert the two bytes
		int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0] & 0xF0)) / 16;
		if (temp > 2047) {
			temp -= 4096;
		}

		// Convert to degrees units 
		double cTemp = temp * 0.0625;
		double fTemp = cTemp * 1.8 + 32;
		*/
		// setting it as the output
		double* cTemp = get_temp(dev_i2c_temp);
		data_placeholder.temperature = cTemp[0];
		printk("Temperature in Celsius : %.2f C \n", cTemp[0]);
		k_free(cTemp);

		
		int_least16_t* data = get_acc(dev_i2c_acc);
		//printk("X: %i, Y: %i, Z: %i, T: %i\r\n", data[0], data[1], data[2], data[3]);
		k_free(data);
		/*
		uint8_t acc_reading[7]= {0};
		uint8_t acc_sensor_reg = LIS2DUX12_OUT_TAG;
		//uint8_t acc_sensor_regs[7] ={LIS2DUX12_OUT_TAG,LIS2DUX12_OUT_X_L,LIS2DUX12_OUT_X_H,LIS2DUX12_OUT_Y_L,LIS2DUX12_OUT_Y_H,LIS2DUX12_OUT_Z_L,LIS2DUX12_OUT_Z_H};
		//for (int i=0;i<6;i++) {
			//err = i2c_write_read_dt(&dev_i2c_acc,&acc_sensor_regs[i],1,&acc_reading[i],1);
			//if (err != 0) {
				//printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_acc.addr,acc_sensor_regs[i]);
			//}
		//}
		err = i2c_write_read_dt(&dev_i2c_acc,&acc_sensor_reg,1,&acc_reading[0],7);
		if (err != 0) {
			printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_acc.addr,&acc_sensor_reg);
		}
		int_least16_t x = (((int)acc_reading[2]&0x0F) * 256 + ((int)acc_reading[1]))*16;
		int_least16_t y = (((int)acc_reading[3]) * 256 + ((int)acc_reading[2]&0xF0));
		int_least16_t z = (((int)acc_reading[5]&0x0F) * 256 + ((int)acc_reading[4]))*16;
		//printk("TAG: %i\r\n", (int)acc_reading[0]);
		if (acc_reading[0]==17) {
			printk("X: %i, Y: %i, Z: %i\r\n", x/16, y/16, z/16);
			//printk("Temp: %i\r\n",((int)acc_reading[5]&0x0F)*16+(int)acc_reading[6]);
			//printk("X_LOW: %i, Y: %i, Z: %i\r\n", (int)acc_reading[1], (int)acc_reading[3], (int)acc_reading[5]);
		    //printk("X_HIG: %i, Y: %i, Z: %i\r\n", (int)acc_reading[2], (int)acc_reading[4], (int)acc_reading[6]);
			//printk("X: %i, %i, %i, %i\r\n", (int)acc_reading[2]/16, (int)acc_reading[2]&0x0F, (int)acc_reading[1]/16, (int)acc_reading[1]&0x0F);
			//printk("Y: %i, %i, %i, %i\r\n", (int)acc_reading[4]/16, (int)acc_reading[4]&0x0F, (int)acc_reading[3]/16, (int)acc_reading[3]&0x0F);
			//printk("Z: %i, %i, %i, %i\r\n\r\n", (int)acc_reading[6]/16, (int)acc_reading[6]&0x0F, (int)acc_reading[5]/16, (int)acc_reading[5]&0x0F);
			// X bottom half of Hi and all of Lo is for X
			// Y Bottom is highest part of Y, Top part is lower bits of Z.
			// Z Hi and top of Lo is for temperature, Bottom of Lo is highest bits for Z.
			// YL XH XM XL
			// ZM ZL YH YM
			// TH TM TL	ZH
			// Or, in the register,
			// XM XL YL XH
			// YH YM ZM ZL
			// TL ZH TH TM // HOW DOES THIS MAKE ANY SENSE??!?
		}*/
		//printk("X_LOW: %i, Y: %i, Z: %i\r\n", (int)acc_reading[1], (int)acc_reading[3], (int)acc_reading[5]);
		//printk("X_HIG: %i, Y: %i, Z: %i\r\n", (int)acc_reading[2], (int)acc_reading[4], (int)acc_reading[6]);

		/* Receive response from the CoAP server */
		/*
		received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d, exit", errno);
			break;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}
		*/
		/* Parse the received CoAP packet */
		/*
		err = onem2m_parse(received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit");
			break;
		}
		*/
		k_sleep(K_MSEC(10));
	}

	onem2m_close_socket();

	return 0;
}

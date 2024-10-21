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
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include "gnss.h"
#include <nrf_modem_gnss.h>
#include "uart.h"

#define nanf -0.0f

data_point current_data_point = {
	.temperature = nanf,
	.speed = nanf,
	.latitude = nanf,
	.longitude = nanf,
	.accelX = nanf,
	.accelY = nanf,
	.accelZ = nanf,
	.gyroX = nanf,
	.gyroY = nanf,
	.gyroZ = nanf,
	.rssi = -500,
	.parent = 0
};

//Register this as the main module
LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);

int main_sens(void);
int main_uart(void);
int main_gnss(void);
K_SEM_DEFINE(op_sem, 1, 1);


K_THREAD_DEFINE(sens, 1024, main_sens, NULL, NULL, NULL, 1, 0, 0);
K_THREAD_DEFINE(uart, 1024, main_uart, NULL, NULL, NULL, 0, 0, 0);
K_THREAD_DEFINE(gnss, 1024, main_gnss, NULL, NULL, NULL, 1, 0, 0);

int main_sens(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
		return -1;
	}
	/* Setting up the i2c device */
	
	
	i2c_init_temp_probe();

	gnss_init();

	while (1) {
		k_msleep(10000);
		k_sem_take(&op_sem, K_FOREVER);
		current_data_point.temperature = i2c_get_temp();
		k_sem_give(&op_sem);
	}
	return 0;
}

int main_uart(void){
	k_sem_take(&op_sem, K_FOREVER);
	uart_module_init();
	k_sem_give(&op_sem);
	while(1){
		k_msleep(3000);
		
		k_sem_take(&op_sem, K_FOREVER);
		uart_send_data(current_data_point);
		k_sem_give(&op_sem);
	}
	return 0;
}

int main_gnss(void){
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
		k_sem_give(&op_sem);
		current_data_point.latitude = gnss_data.latitude;
		current_data_point.longitude = gnss_data.longitude;
		current_data_point.speed = gnss_data.speed;
	}
	return 0;
	
}
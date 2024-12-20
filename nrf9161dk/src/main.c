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
#include <zephyr/sys/time_units.h>
#include <zephyr/drivers/gpio.h>

/*
testing_mode = 0: GNSS is used
testing_mode = 1: Mock GNSS data is used
testing_mode = 2: Mock GNSS data is used, end after one iteration
*/
int testing_mode = 1;

// Configure GPIO pins for external LED
#define EXTERNAL_LED_NODE DT_NODELABEL(extled1)
static const struct gpio_dt_spec lockled = GPIO_DT_SPEC_GET(EXTERNAL_LED_NODE, gpios);

int resolve_address_lock = 0;

// Define data points structures
union resource_data resource_placeholder;

struct bike bike_placeholder = {
	.latie = 0.0,
	.longe = 0.0,
	.tempe = 70.00,
	.speed = 3.20,
	.accel = 1.80
};

struct battery battery_placeholder = {
	.lvl = 0,
	.lowBy = false
};

struct mesh_connectivity mesh_placeholder = {
	.neibo = "00000",
	.stnr = 0,
};

struct lock lock_placeholder = {
	.lock = true
};

// Register this as the main module
LOG_MODULE_REGISTER(Main_Module, LOG_LEVEL_INF);

// Handler for button events
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	int err, received;

	// Button 1: Get lock data
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		LOG_INF("Button 1 pressed\n");

		err = client_get_send(LOCK);
		if (err != 0) {
			LOG_ERR("Failed to send GET request...\n");
			return;
		}
		
		received = onem2m_receive();
		onem2m_parse(received, LOCK);	

		LOG_INF("Lock status: %s", lock_placeholder.lock ? "true" : "false");
		gpio_pin_set_dt(&lockled, lock_placeholder.lock);
	}

	// Button 2: Send bike data
	else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		LOG_INF("Button 2 pressed");

		bike_placeholder.latie += 1.0;
		bike_placeholder.longe += 1.0;
		resource_placeholder.bikedata = bike_placeholder;
		
		err = client_put_send(resource_placeholder, BIKEDATA);
		if (err != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			return;
		}

		received = onem2m_receive();
		onem2m_parse(received, BIKEDATA);
	}

	// Button 3: Send battery data
	else if (has_changed & DK_BTN3_MSK && button_state & DK_BTN3_MSK) {
		LOG_INF("Button 3 pressed");

		battery_placeholder.lvl = get_battery_level();
		resource_placeholder.batterydata = battery_placeholder;
		
		err = client_put_send(resource_placeholder, BATTERY);
		if (err != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			return;
		}
		
		received = onem2m_receive();
		onem2m_parse(received, BATTERY);
	}

	// Button 4: Send mesh data
	else if (has_changed & DK_BTN4_MSK && button_state & DK_BTN4_MSK) {
		LOG_INF("Button 4 pressed");

		mesh_placeholder.stnr += 1;
		resource_placeholder.meshdata = mesh_placeholder;

		err = client_put_send(resource_placeholder, MESH_CONNECTIVITY);
		if (err != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			return;
		}
		
		received = onem2m_receive();
		onem2m_parse(received, MESH_CONNECTIVITY);
	}
}

// Initialization sequence function
static int init_all(void)
{
	// Initialize LED library
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
		return -1;
	}

	// Configure modem
	if (modem_configure() != 0) {
		LOG_ERR("Failed to configure the modem");
		return -1;
	}

	// Initialize button library
	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
		return -1;
	}

	// Initialize I2C temperature sensor
	if (i2c_init_temp_probe() != 0) {
		return -1;
	}

	// Initialize ADC battery monitoring
	if (battery_init() != 0) {
		return -1;
	}

	// Initialize GNSS if testing_mode is 0
	if (testing_mode == 0) {
		if (gnss_init() != 0) {
			return -1;
		}
	}
	
	// Initialize GPIO for external LED
	gpio_pin_configure_dt(&lockled, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set_dt(&lockled, lock_placeholder.lock);

	return 0;
}

int main(void)
{
	int err, received;

	uint64_t start_time, end_time, loop_duration;
	int sleep_duration;
	int iteration_count = 0;

	struct nrf_modem_gnss_pvt_data_frame gnss_data;
	bool first_fix = false;

	// Initialization
	if (init_all() != 0) {
		LOG_ERR("Failed to initialize the application");
		return 0;
	}

	// Main loop
	while (1) {
		start_time = k_uptime_get();

		// Acquire data from GNSS receiver and sensors
		int lvl_temp = get_battery_level();
		bike_placeholder.tempe = i2c_get_temp();
		
		// A. If GNSS is available, use GNSS data
		if (testing_mode == 0) {
			k_sem_take(&gnss_fix_obtained, K_SECONDS(180)); // Await GNSS fix
			
			// Acquire data from GNSS receiver and sensors
			gnss_data = get_current_pvt();
			if (gnss_data.latitude != 0.0 || gnss_data.longitude != 0.0) {
				bike_placeholder.latie = gnss_data.latitude;
				bike_placeholder.longe = gnss_data.longitude;
				bike_placeholder.speed = gnss_data.speed;
				first_fix = true;
			}
		}
		
		// B. If GNSS is not available, use mock data
		else if (testing_mode >= 1) {
			bike_placeholder.latie = (40.791515) + ((sys_rand16_get() % 2 == 0 ? 1 : -1) * (sys_rand16_get() % 20 * 0.00001));
			bike_placeholder.longe = (-77.870764) + ((sys_rand16_get() % 2 == 0 ? 1 : -1) * (sys_rand16_get() % 20 * 0.00001));
			bike_placeholder.speed += ((sys_rand16_get() % 2 == 0 ? 1 : -1) * (sys_rand16_get() % 100 * 0.1));
			if (bike_placeholder.speed < 0) {
				bike_placeholder.speed = 0;
			}
			first_fix = true;
		}

		// Activate LTE
		if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL) != 0) {
			LOG_ERR("Failed to activate LTE");
			continue;
		}

		if (k_sem_take(&lte_connected, K_SECONDS(30)) != 0) {
			LOG_ERR("Failed to connect to LTE network in time");
			LOG_INF("Modem restarting\n");
			nrf_modem_lib_shutdown();
			nrf_modem_lib_init();
			continue;
		}

		// Connect to the CoAP server
		if (resolve_address_lock == 0) {
			LOG_INF("Resolving the server address\r");
			if (server_resolve() != 0) {
				LOG_ERR("Failed to resolve server name\n");
				continue;
			}
			resolve_address_lock = 1;
		}

		if (client_init() != 0) {
			LOG_INF("Failed to initialize CoAP client");
			continue;
		}

		// Put BIKEDATA to the CoAP server
		resource_placeholder.bikedata = bike_placeholder;

		if (first_fix) {
			if (client_put_send(resource_placeholder, BIKEDATA) != 0) {
				LOG_ERR("Failed to send PUT request, exit...\n");
				break;
			}
			
			// Receive and parse response from the CoAP server
			received = onem2m_receive();
			if (received < 0) {
				LOG_ERR("Socket error: %d, exit", errno);
				break;
			} if (received == 0) {
				LOG_INF("Empty datagram");
				continue;
			}

			err = onem2m_parse(received, BIKEDATA);
			if (err < 0) {
				LOG_ERR("Invalid response, exit");
				break;
			}
		}

		// Put BATTERYDATA to the CoAP server, if the battery level has changed
		if (lvl_temp != battery_placeholder.lvl) {
			battery_placeholder.lvl = lvl_temp;
			resource_placeholder.batterydata = battery_placeholder;

			if (client_put_send(resource_placeholder, BATTERY) != 0) {
				LOG_ERR("Failed to send PUT request...\n");
				continue;
			}

			// Receive and parse response from the CoAP server
			received = onem2m_receive();
			if (received < 0) {
				LOG_ERR("Socket error: %d", errno);
				continue;
			} if (received == 0) {
				LOG_INF("Empty datagram");
				continue;
			}

			err = onem2m_parse(received, BATTERY);
			if (err < 0) {
				LOG_ERR("Invalid response");
				continue;
			}
		}

		// Put MESHDATA to the CoAP server
		if (testing_mode >= 1) {
			mesh_placeholder.stnr += 1;
		}

		resource_placeholder.meshdata = mesh_placeholder;
		
		if (client_put_send(resource_placeholder, MESH_CONNECTIVITY) != 0) {
			LOG_ERR("Failed to send PUT request...\n");
			continue;
		}
		
		// Receive and parse response from the CoAP server
		received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d", errno);
			continue;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		err = onem2m_parse(received, MESH_CONNECTIVITY);
		if (err < 0) {
			LOG_ERR("Invalid response");
			continue;
		}

		// Get LOCKDATA from the CoAP server
		if (client_get_send(LOCK) != 0) {
			LOG_ERR("Failed to send GET request...\n");
			continue;
		}
		
		// Receive and parse response from the CoAP server
		received = onem2m_receive();
		if (received < 0) {
			LOG_ERR("Socket error: %d", errno);
			continue;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		err = onem2m_parse(received, LOCK);
		if (err < 0) {
			LOG_ERR("Invalid response");
			continue;
		}

		LOG_INF("Lock status: %s", lock_placeholder.lock ? "true" : "false");
		gpio_pin_set_dt(&lockled, lock_placeholder.lock);

		// Disconnect from the CoAP server, deactivate LTE
		onem2m_close_socket();

		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
		if (err != 0) {
			LOG_ERR("Failed to deactivate LTE and enable GNSS functional mode");
			continue;
		}

		// END OF LOOP
		if (testing_mode >= 2) {  // If in testing mode >= 2, break after one iteration
			break;
		}
		
		LOG_INF("Iteration %d complete", iteration_count + 1);
		iteration_count++;

		if (iteration_count % 30 == 0) {  // Restart modem every 30 iterations, bypass hourly LTE modem activation limit set by RPM
			LOG_INF("Modem restarting\n");
			nrf_modem_lib_shutdown();
			nrf_modem_lib_init();
		}
		
		// SLEEP
		end_time = k_uptime_get();
		loop_duration = end_time - start_time;
		sleep_duration = 6000 - loop_duration;
		LOG_INF("Loop duration: %llu ms", loop_duration);

		if (sleep_duration > 0) {
			k_sleep(K_MSEC(sleep_duration));
		}

		LOG_INF("\n\n");
	}

	onem2m_close_socket();
	LOG_INF("Application complete");

	return 0;
}

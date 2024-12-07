#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_key_mgmt.h>
#include <nrf_modem_gnss.h>

K_SEM_DEFINE(gnss_fix_obtained, 0, 1);

LOG_MODULE_REGISTER(GNSS_Module, LOG_LEVEL_INF);

struct nrf_modem_gnss_pvt_data_frame last_pvt;  // Last PVT data
struct nrf_modem_gnss_pvt_data_frame current_pvt;  // Most current and valid PVT data

int gnss_event_count = 0;
int period_num_satellites = 0;  // Number of satellites tracked

// Print the latitude and longitude, and other data from PVT data
void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	LOG_INF("Latitude:    %.06f", pvt_data->latitude);
	LOG_INF("Longitude:   %.06f", pvt_data->longitude);
	LOG_INF("Speed: 	  %.02f", pvt_data->speed);
}

// Return the current PVT data
struct nrf_modem_gnss_pvt_data_frame get_current_pvt(void)
{
    return current_pvt;
}

// Handler for GNSS events
void gnss_event_handler(int event)
{
	switch (event)
    {
	case NRF_MODEM_GNSS_EVT_PVT:
	
		gnss_event_count++;

		if (gnss_event_count == 1) {
			LOG_INF("Searching for GNSS Satellites....\r");

			// Count and log the number of tracked satellites

			int last_num_satellites = 0;

			for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; i++) {
				if (last_pvt.sv[i].sv > 0) {
					last_num_satellites++;
				}
			}

			if (last_num_satellites > period_num_satellites) {
				period_num_satellites = last_num_satellites;
			}
			
			LOG_INF("Number of satellites: %d\r", period_num_satellites);
		}

		// Every 30 seconds or 30 PVT events, reset counter and number of satellites tracked
		else if (gnss_event_count == 30) {
			gnss_event_count = 0;
			period_num_satellites = 0;
		}

		break;

	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_INF("GNSS fix event\r");

		// Reset the GNSS event counter and the number of satellites tracked
		gnss_event_count = 0;
		period_num_satellites = 0;
		break;

	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		LOG_INF("GNSS woke up in periodic mode\r");
		break;

	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("GNSS is blocked by LTE event\r");
		break;

	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_INF("GNSS enters sleep because fix was achieved in periodic mode\r");

		// Valid fix achieved, read and store the last PVT data
		int32_t retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (retval == 0)
        {
			current_pvt = last_pvt;
			print_fix_data(&current_pvt);
			k_sem_give(&gnss_fix_obtained);  // Signal that a fix has been obtained
		}
		break;

	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_INF("GNSS enters sleep because fix retry timeout was reached\r");
		break;

	default:
		break;
	}
}

// Initialize GNSS
int gnss_init(void)
{
	// Use high accuracy timing source if configured
	#if defined(CONFIG_GNSS_HIGH_ACCURACY_TIMING_SOURCE)
		if (nrf_modem_gnss_timing_source_set(NRF_MODEM_GNSS_TIMING_SOURCE_TCXO)){
			LOG_ERR("Failed to set TCXO timing source");
			return -1;
		}
	#endif

	// Allow for low accuracy location fixes if configured
	#if defined(CONFIG_GNSS_LOW_ACCURACY) || defined (CONFIG_BOARD_THINGY91_NRF9160_NS)
		uint8_t use_case;
		use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START | NRF_MODEM_GNSS_USE_CASE_LOW_ACCURACY;
		if (nrf_modem_gnss_use_case_set(use_case) != 0) {
			LOG_ERR("Failed to set low accuracy use case");
			return -1;
		}
	#endif

	// Set GNSS event handler to process GNSS events
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}

	// Set the GNSS fix interval (time between fixes)
	if (nrf_modem_gnss_fix_interval_set(CONFIG_TRACKER_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return -1;
	}

	// Set the GNSS fix retry timeout (time to wait for a fix)
	if (nrf_modem_gnss_fix_retry_set(CONFIG_TRACKER_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return -1;
	}

	// Start the GNSS module
	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return -1;
	}

	// Enable GNSS priority mode to prioritize GNSS over LTE
	if (nrf_modem_gnss_prio_mode_enable() != 0){
		LOG_ERR("Error setting GNSS priority mode");
		return -1;
	}
	return 0;
}

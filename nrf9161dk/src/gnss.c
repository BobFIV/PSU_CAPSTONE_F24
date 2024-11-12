#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_key_mgmt.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>


K_SEM_DEFINE(gnss_fix_obtained, 0, 1);

LOG_MODULE_REGISTER(GNSS_Module, LOG_LEVEL_INF);

struct nrf_modem_gnss_pvt_data_frame last_pvt;
struct nrf_modem_gnss_pvt_data_frame current_pvt;

// //Configure the modem
// int modem_configure(void)
// {
// 	int err;

// 	LOG_INF("Initializing modem library");

// 	err = nrf_modem_lib_init();
// 	if (err) {
// 		LOG_ERR("Failed to initialize the modem library, error: %d", err);
// 		return err;
// 	}
	
// 	return 0;
// }


// Print the latitude and longitude, and other data
void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	LOG_INF("Latitude:    %.06f", pvt_data->latitude);
	LOG_INF("Longitude:   %.06f\n", pvt_data->longitude);;
}

// Return the current PVT data
struct nrf_modem_gnss_pvt_data_frame get_current_pvt(void)
{
    return current_pvt;
}

int gnss_event_count = 0;

// Handler for GNSS events
void gnss_event_handler(int event)
{
	int retval;

	switch (event)
    {
	case NRF_MODEM_GNSS_EVT_PVT:
		// Every 30 seconds
		gnss_event_count++;
		
		if (gnss_event_count == 10) {
			gnss_event_count = 0;
		}
		nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		int num_satellites = 0;
		for (int i = 0; i < 12 ; i++) {
			if (last_pvt.sv[i].signal != 0) { 
				LOG_DBG("sv: %d, cn0: %d, signal: %d", last_pvt.sv[i].sv, last_pvt.sv[i].cn0, last_pvt.sv[i].signal);
				num_satellites++;
			}
		}
		LOG_DBG("Number of current satellites: %d", num_satellites);
		if (gnss_event_count == 1) {
			LOG_INF("Searching for GNSS Satellites (%d)....\r", num_satellites);
		}
		break;

	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_INF("GNSS fix event\n\r");
		gnss_event_count = 0;
		break;

	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		LOG_INF("GNSS woke up in periodic mode\n\r");
		break;

	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("GNSS is blocked by LTE event\n\r");
		break;

	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_INF("GNSS enters sleep because fix was achieved in periodic mode\n\r");
		retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (retval == 0)
        {
			current_pvt = last_pvt;
			print_fix_data(&current_pvt);
			k_sem_give(&gnss_fix_obtained);
		}
		break;

	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_INF("GNSS enters sleep because fix retry timeout was reached\n\r");
		break;

	default:
		break;
	}
}

// Initialize GNSS
int gnss_init(void)
{
	// int err;
	// err = modem_configure();
	// if (err) {
	// 	LOG_ERR("Failed to configure the modem");
	// 	return 0;
	// }

	/* STEP 8 - Activate only the GNSS stack */
	// if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS) != 0) {
	// 	LOG_ERR("Failed to activate GNSS functional mode");
	// 	return 0;
	// }

	/* STEP 9 - Register the GNSS event handler */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return 0;
	}

	/* STEP 10 - Set the GNSS fix interval and GNSS fix retry period */
	if (nrf_modem_gnss_fix_interval_set(CONFIG_TRACKER_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return 0;
	}

	if (nrf_modem_gnss_fix_retry_set(CONFIG_TRACKER_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return 0;
	}

	/* STEP 11 - Start the GNSS receiver*/
	LOG_INF("Starting GNSS");
	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return 0;
	}

}

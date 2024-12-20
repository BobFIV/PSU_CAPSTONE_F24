#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(LTE_Module, LOG_LEVEL_INF);

// Handler for LTE events
void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:  // Change in network registration status

		// Verify device network registration, if not connected, break
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			break;
		}
		LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? 
				"Connected - home network" : "Connected - roaming");
		k_sem_give(&lte_connected);  // Signal that device is connected to LTE network
		break;

	case LTE_LC_EVT_RRC_UPDATE:  // Change in RRC connection state
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;

	default:
		break;
	}
}

// Initialize and configure the modem
int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err)
	{
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err)
	{
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}

	// Start with LTE deactivated
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err)
	{
		LOG_ERR("Failed to deactivate LTE and enable GNSS functional mode");
		return err;
	}

	return 0;
}
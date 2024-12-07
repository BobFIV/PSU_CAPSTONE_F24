#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

extern struct k_sem lte_connected;

// Handler for LTE events
void lte_handler(const struct lte_lc_evt *const evt);

// Initialize and configure the modem
int modem_configure(void);
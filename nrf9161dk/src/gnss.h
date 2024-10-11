#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

extern struct k_sem gnss_fix_obtained;

// Print the latitude and longitude, and other data
static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data);

// Return the current PVT data
struct nrf_modem_gnss_pvt_data_frame get_current_pvt(void);

// Handler for GNSS events
void gnss_event_handler(int event);

// Initialize GNSS
int gnss_init(void);
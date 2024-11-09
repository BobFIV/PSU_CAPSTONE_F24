#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>

#define BATTERY_MAX_VOLTAGE_MV 475
#define BATTERY_MIN_VOLTAGE_MV 400
#define BATTERY_MEASUREMENT_OFFSET_MV -56

// Initialize battery monitoring
int battery_init(void);

// Read voltage going to an input pin
int pin_read(void);

// Get the battery level in percentage
int get_battery_level(void);
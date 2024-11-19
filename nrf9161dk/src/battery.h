#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <math.h>

#define BATTERY_MAX_VOLTAGE_MV 1440
#define BATTERY_MIN_VOLTAGE_MV 1218
#define BATTERY_MEASUREMENT_OFFSET_MV -54

// Initialize battery monitoring
int battery_init(void);

// Read voltage at the configured ADC input pin
int pin_read(void);

// Get the battery level as a percentage
int get_battery_level(void);

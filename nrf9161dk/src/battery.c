#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <math.h> // round
#include "battery.h"

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

int16_t adc_sample_buf;
struct adc_sequence sequence = {
    .buffer = &adc_sample_buf,
    .buffer_size = sizeof(adc_sample_buf)
};

LOG_MODULE_REGISTER(Battery_Module, LOG_LEVEL_INF);

/**
 * @brief Initialize the battery monitoring.
 *
 * This function initializes the ADC channel and sequence for reading the battery voltage.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int battery_init(void) {
    int err;

    // Check if the ADC controller device is ready
    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC controller devivce %s not ready", adc_channel.dev->name);
        return -1;
    }

    // Setup the ADC channel
    err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", 0, err);
        return -1;
    }

    // Initialize the ADC sequence
    err = adc_sequence_init_dt(&adc_channel, &sequence);
    if (err < 0) {
        LOG_ERR("Could not initalize sequnce");
        return -1;
    }

    return 0;
}

/**
 * @brief Read the battery voltage from the ADC pin.
 *
 * This function reads the raw ADC value from the specified ADC channel,
 * converts it to millivolts, applies an offset, and returns the result.
 *
 * @return Battery voltage in millivolts, or -1 on error.
 */
int pin_read(void)
{
    int err;
    int val_mv;

    // Read the raw ADC value from the specified ADC channel
    err = adc_read(adc_channel.dev, &sequence);
    if (err < 0) {
        return -1;
    }

    // Convert the raw ADC value to millivolts
    val_mv = (int) adc_sample_buf;
    err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
    if (err < 0) {
        return -1;
    }
    
    // Apply the measurement offset
    val_mv += BATTERY_MEASUREMENT_OFFSET_MV;

    return val_mv;
}

/**
 * @brief Get the battery level as a percentage.
 *
 * This function reads the battery voltage multiple times, averages the readings,
 * and calculates the battery level as a percentage based on predefined voltage ranges.
 *
 * @return Battery level as a percentage (0-100).
 */
int get_battery_level(void)
{
    // Sleep to allow for the voltage to stabilize
    k_sleep(K_MSEC(1000));

    // Collect voltage reading samples in an array
    char str_buf[64] = "Voltage samples (mV): ";
    char str_temp[8];

    int num_samples = 6;
    int val_mv_arr[num_samples];

    for (int i = 0; i < num_samples; i++) {
        val_mv_arr[i] = pin_read();  // Read the voltage
        
        snprintk(str_temp, sizeof(str_temp), "%d ", val_mv_arr[i]);
        strcat(str_buf, str_temp);
    }

    LOG_INF("%s", str_buf);

    // Calculate the average of the voltage readings
    int sum = 0;
    for (int i = 0; i < num_samples; i++) {
        sum += val_mv_arr[i];
    }

    int val_mv = (int) round((double)sum / num_samples);

    LOG_INF("Voltage average (mV): %d", val_mv);

    // Calculate battery level as a percentage
    double voltage_range = BATTERY_MAX_VOLTAGE_MV - BATTERY_MIN_VOLTAGE_MV;
    double normalized_voltage = (val_mv - BATTERY_MIN_VOLTAGE_MV) / voltage_range;
    int battery_level = (int)round(normalized_voltage * 100.0);

    LOG_INF("Battery level (%%): %d", battery_level);
    
    return battery_level;
}

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include "battery.h"

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

int16_t buf;
struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf)
};

LOG_MODULE_REGISTER(Battery_Module, LOG_LEVEL_INF);

int battery_init(void) {
    int err;

    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC controller devivce %s not ready", adc_channel.dev->name);
        return -1;
    }

    err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", 0, err);
        return -1;
    }

    err = adc_sequence_init_dt(&adc_channel, &sequence);
    if (err < 0) {
        LOG_ERR("Could not initalize sequnce");
        return -1;
    }

    return 0;
}

int pin_read(void) {
    int err;
    int val_mv;

    err = adc_read(adc_channel.dev, &sequence);
    if (err < 0) {
        return -1;
    }

    val_mv = (int) buf;

    err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
    if (err < 0) {
        return -1;
    }
    
    LOG_INF("Battery voltage: %d mV", val_mv);

    return val_mv;
}

int get_battery_level(void) {
    int val_mv = pin_read();
    if (val_mv < 0) {
        return -1;
    }

    int battery_level = (val_mv - BATTERY_MIN_VOLTAGE_MV) * (100.0 / (BATTERY_MAX_VOLTAGE_MV - BATTERY_MIN_VOLTAGE_MV));
    LOG_INF("Battery level: %d", battery_level);
    return battery_level;
}

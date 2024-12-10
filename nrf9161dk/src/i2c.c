#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include "i2c.h"
#include <zephyr/logging/log.h>

static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);

LOG_MODULE_REGISTER(I2C_Module, LOG_LEVEL_INF);

int i2c_init_temp_probe(void) {
    int err;
    
	if (!device_is_ready(dev_i2c.bus)) {
		LOG_ERR("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
		return -1;
	}

    /* Setting it to freerun mode */

    uint8_t config[2] = {STTS751_CONFIG_REG,0b00000100};
	err = i2c_write_dt(&dev_i2c, config, sizeof(config));
	if (err != 0) {
		LOG_ERR("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config[0]);
		return -1;
	}
    return 0;
}

double i2c_get_temp(void) {
    int err;
    // Getting the temperature
    uint8_t temp_reading[2]= {0};
    uint8_t sensor_regs[2] ={STTS751_TEMP_LOW_REG,STTS751_TEMP_HIGH_REG};
    err = i2c_write_read_dt(&dev_i2c,&sensor_regs[0],1,&temp_reading[0],1);
    if (err != 0) {
        LOG_ERR("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[0]);
    }
    err = i2c_write_read_dt(&dev_i2c,&sensor_regs[1],1,&temp_reading[1],1);
    if (err != 0) {
        LOG_ERR("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[1]);
    }

    // Convert the two bytes
    int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0] & 0xF0)) / 16;
    if (temp > 2047) {
        temp -= 4096;
    }
    //printk(temp>>4);
    LOG_INF("Temperature (int) retrieved: %d", temp);
    // Convert to degrees units 
    double cTemp = temp * 0.125;
    double fTemp = cTemp * 1.8 + 32;
    LOG_INF("Temperature (C) retrieved: %0.2f", cTemp);
    return cTemp;
}
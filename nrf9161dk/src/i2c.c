#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include "i2c.h"
#include <zephyr/logging/log.h>

static const struct i2c_dt_spec dev_i2c_temp = I2C_DT_SPEC_GET(I2C0_NODE_TEMP);
static const struct i2c_dt_spec dev_i2c_acc = I2C_DT_SPEC_GET(I2C0_NODE_ACC);

LOG_MODULE_REGISTER(I2C_Module, LOG_LEVEL_INF);

int i2c_init_temp_probe() {
    int err;
    
	if (!device_is_ready(dev_i2c_temp.bus)) {
		LOG_ERR("I2C bus %s is not ready!\n\r",dev_i2c_temp.bus->name);
		return -1;
	}

    /* Setting it to freerun mode */

    uint8_t config[2] = {STTS751_CONFIG_REG,0b00000100};
	err = i2c_write_dt(&dev_i2c_temp, config, sizeof(config));
	if (err != 0) {
		LOG_ERR("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c_temp.addr,config[0]);
		return -1;
	}
    return 0;
}

void init_acc_probe() {
    int err;
    //static const struct i2c_dt_spec dev_i2c_temp = I2C_DT_SPEC_GET(I2C0_NODE);
	if (!device_is_ready(dev_i2c_temp.bus)) {
		printk("I2C bus %s is not ready!\n\r",dev_i2c_temp.bus->name);
		return -1;
	}

    /* Setting it to freerun mode */

    uint8_t config1[2] = {LIS2DUX12_CONFIG_REG,LIS2DUX12_CONFIG_REG_FIFO_ENABLE};
	err = i2c_write_dt(&dev_i2c_temp, config1, sizeof(config1));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c_temp.addr,config1[0]);
		return -1;
	}

    uint8_t config2[2] = {LIS2DUX12_FIFO_CONFIG_REG1,LIS2DUX12_FIFO_CONFIG_REG1_25hz|LIS2DUX12_FIFO_CONFIG_REG1_pm2g};
	err = i2c_write_dt(&dev_i2c_temp, config2, sizeof(config2));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c_temp.addr,config2[0]);
		return -1;
	}

    uint8_t config3[2] = {LIS2DUX12_FIFO_CONFIG_REG2,LIS2DUX12_FIFO_CONFIG_REG2_continuous};
	err = i2c_write_dt(&dev_i2c_temp, config3, sizeof(config3));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c_temp.addr,config3[0]);
		return -1;
	}
    return;
}

double i2c_get_temp() {
    int err;
    // Getting the temperature
    uint8_t temp_reading[2]= {0};
    uint8_t sensor_regs[2] ={STTS751_TEMP_LOW_REG,STTS751_TEMP_HIGH_REG};
    err = i2c_write_read_dt(&dev_i2c_temp,&sensor_regs[0],1,&temp_reading[0],1);
    if (err != 0) {
        LOG_ERR("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_temp.addr,sensor_regs[0]);
    }
    err = i2c_write_read_dt(&dev_i2c_temp,&sensor_regs[1],1,&temp_reading[1],1);
    if (err != 0) {
        LOG_ERR("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_temp.addr,sensor_regs[1]);
    }

    // Convert the two bytes
    int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0] & 0xF0)) / 16;
    if (temp > 2047) {
        temp -= 4096;
    }
    //printk(temp>>4);
    LOG_INF("Temperature (int) retrieved: %d", temp);
    // Convert to degrees units 
    double cTemp = temp * 0.01;
    double fTemp = cTemp * 1.8 + 32;
    LOG_INF("Temperature (F) retrieved: %0.2f", fTemp);
    return fTemp;
}

void* i2c_get_acc() {
    int err;
    uint8_t acc_reading[7]= {0};
    uint8_t acc_sensor_reg = LIS2DUX12_OUT_TAG;
    err = i2c_write_read_dt(&dev_i2c_acc,&acc_sensor_reg,1,&acc_reading[0],7);
    if (err != 0) {
        printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c_acc.addr,&acc_sensor_reg);
    }
    int_least16_t x = (((int)acc_reading[2]&0x0F) * 256 + ((int)acc_reading[1]))*16;
    int_least16_t y = (((int)acc_reading[3]) * 256 + ((int)acc_reading[2]&0xF0));
    int_least16_t z = (((int)acc_reading[5]&0x0F) * 256 + ((int)acc_reading[4]))*16;
    int_least16_t t = (((int)acc_reading[6]) * 256 + ((int)acc_reading[5]&0xF0));
    //_Float16 temp = ((double)t)*0.045 - 25;
    
    int_least16_t* data = k_malloc(sizeof(int_least16_t)*4);
    data[0] = x/16;
    data[1] = y/16;
    data[2] = z/16;
    data[3] = t/16;
    return data;
}
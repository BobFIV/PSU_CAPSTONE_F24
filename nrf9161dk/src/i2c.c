#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
/* STEP 3 - Include the header file of the I2C API */
#include <zephyr/drivers/i2c.h>
/* STEP 4.1 - Include the header file of printk() */
#include <zephyr/sys/printk.h>
#include "i2c.h"

void init_temp_probe(const struct i2c_dt_spec dev_i2c) {
    int err;
    //static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);
	if (!device_is_ready(dev_i2c.bus)) {
		printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
		return -1;
	}

    /* Setting it to freerun mode */

    uint8_t config[2] = {STTS751_CONFIG_REG,0b00000100};
	err = i2c_write_dt(&dev_i2c, config, sizeof(config));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config[0]);
		return -1;
	}
    return;
}

void init_acc_probe(const struct i2c_dt_spec dev_i2c) {
    int err;
    //static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C0_NODE);
	if (!device_is_ready(dev_i2c.bus)) {
		printk("I2C bus %s is not ready!\n\r",dev_i2c.bus->name);
		return -1;
	}

    /* Setting it to freerun mode */

    uint8_t config1[2] = {LIS2DUX12_CONFIG_REG,LIS2DUX12_CONFIG_REG_FIFO_ENABLE};
	err = i2c_write_dt(&dev_i2c, config1, sizeof(config1));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config1[0]);
		return -1;
	}

    uint8_t config2[2] = {LIS2DUX12_FIFO_CONFIG_REG1,LIS2DUX12_FIFO_CONFIG_REG1_6hz|LIS2DUX12_FIFO_CONFIG_REG1_pm2g};
	err = i2c_write_dt(&dev_i2c, config2, sizeof(config2));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config2[0]);
		return -1;
	}

    uint8_t config3[2] = {LIS2DUX12_FIFO_CONFIG_REG2,LIS2DUX12_FIFO_CONFIG_REG2_continuous};
	err = i2c_write_dt(&dev_i2c, config3, sizeof(config3));
	if (err != 0) {
		printk("Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr,config3[0]);
		return -1;
	}
    return;
}

void* get_temp(const struct i2c_dt_spec dev_i2c) {
    int err;
    // Getting the temperature
    uint8_t temp_reading[2]= {0};
    uint8_t sensor_regs[2] ={STTS751_TEMP_LOW_REG,STTS751_TEMP_HIGH_REG};
    err = i2c_write_read_dt(&dev_i2c,&sensor_regs[0],1,&temp_reading[0],1);
    if (err != 0) {
        printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[0]);
    }
    err = i2c_write_read_dt(&dev_i2c,&sensor_regs[1],1,&temp_reading[1],1);
    if (err != 0) {
        printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr,sensor_regs[1]);
    }

    // Convert the two bytes
    int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0]));
    if (temp > 32767) {
        temp -= 4096;
    }
    //printk(temp>>4);

    // Convert to degrees units 
    double cTemp = temp * 0.01;//625;
    double fTemp = cTemp * 1.8 + 32;
    double* data = k_malloc(sizeof(double)*1);
    data[0] = cTemp;
    return data;
}

void* get_acc(const struct i2c_dt_spec dev_i2c_acc) {
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
    _Float16 temp = ((double)t)*0.045 - 25;
    //printk("TAG: %i\r\n", (int)acc_reading[0]);
    //if (acc_reading[0]==17) {
        //printk("X: %i, Y: %i, Z: %i\r\n", x/16, y/16, z/16);
    //}
    int_least16_t* data = k_malloc(sizeof(int_least16_t)*5);
    data[0] = x/16;
    data[1] = y/16;
    data[2] = z/16;
    data[3] = t;
    data[4] = acc_reading[0];
    return data;
}
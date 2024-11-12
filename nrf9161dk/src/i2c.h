#include <zephyr/devicetree.h>

#ifndef I2C_H_

#define STTS751_TEMP_HIGH_REG            0x07
#define STTS751_TEMP_LOW_REG             0x06
#define STTS751_CONFIG_REG               0x04

#define I2C0_NODE_TEMP DT_NODELABEL(sensor_temp)
#define I2C0_NODE_ACC DT_NODELABEL(sensor_acc)

#define LIS2DUX12_CONFIG_REG            0x13
#define LIS2DUX12_CONFIG_REG_FIFO_ENABLE 0b00001000

#define LIS2DUX12_FIFO_CONFIG_REG1                          0x14
#define LIS2DUX12_FIFO_CONFIG_REG1_POWER_DOWN               0b00000000
#define LIS2DUX12_FIFO_CONFIG_REG1_1p6hz_ultralow_power     0b00010000
#define LIS2DUX12_FIFO_CONFIG_REG1_3hz_ultralow_power       0b00100000
#define LIS2DUX12_FIFO_CONFIG_REG1_25hz_ultralow_power      0b00110000
#define LIS2DUX12_FIFO_CONFIG_REG1_6hz                      0b01000000
#define LIS2DUX12_FIFO_CONFIG_REG1_12p5hz                   0b01010000
#define LIS2DUX12_FIFO_CONFIG_REG1_25hz                     0b01100000
#define LIS2DUX12_FIFO_CONFIG_REG1_50hz                     0b01110000
#define LIS2DUX12_FIFO_CONFIG_REG1_100hz                    0b10000000
#define LIS2DUX12_FIFO_CONFIG_REG1_200hz                    0b10010000
#define LIS2DUX12_FIFO_CONFIG_REG1_400hz                    0b10100000
#define LIS2DUX12_FIFO_CONFIG_REG1_800hz                    0b10110000
#define LIS2DUX12_FIFO_CONFIG_REG1_pm2g                     0b00000000
#define LIS2DUX12_FIFO_CONFIG_REG1_pm4g                     0b00000001
#define LIS2DUX12_FIFO_CONFIG_REG1_pm8g                     0b00000010
#define LIS2DUX12_FIFO_CONFIG_REG1_pm16g                    0b00000011

#define LIS2DUX12_FIFO_CONFIG_REG2      0x15
#define LIS2DUX12_FIFO_CONFIG_REG2_bypass                   0b00000000
#define LIS2DUX12_FIFO_CONFIG_REG2_continuous               0b00000110

#define LIS2DUX12_OUT_TAG               0x40

//initialize temperature polling
int i2c_init_temp_probe(void);

//retrieve the temperature (fahrenheit) from the device
double i2c_get_temp(void);
void* i2c_get_acc();

#endif
#define I2C_H_
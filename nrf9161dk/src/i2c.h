//#include <zephyr/drivers/i2c.h>

#define STTS751_TEMP_HIGH_REG           0x07
#define STTS751_TEMP_LOW_REG            0x06
#define STTS751_CONFIG_REG              0x04

#define LIS2DUX12_CONFIG_REG            0x13

#define LIS2DUX12_FIFO_CONFIG_REG1      0x14
#define LIS2DUX12_FIFO_CONFIG_REG2      0x15

#define LIS2DUX12_OUT_TAG               0x40/*
#define LIS2DUX12_OUT_X_L               0x28
#define LIS2DUX12_OUT_X_H               0x29
#define LIS2DUX12_OUT_Y_L               0x2A
#define LIS2DUX12_OUT_Y_H               0x2B
#define LIS2DUX12_OUT_Z_L               0x2C
#define LIS2DUX12_OUT_Z_H               0x2D*/
#define LIS2DUX12_OUT_X_L               0x41
#define LIS2DUX12_OUT_X_H               0x42
#define LIS2DUX12_OUT_Y_L               0x43
#define LIS2DUX12_OUT_Y_H               0x44
#define LIS2DUX12_OUT_Z_L               0x45
#define LIS2DUX12_OUT_Z_H               0x46

#define I2C0_NODE DT_NODELABEL(mysensor)


//void init_temp_probe(const struct i2c_dt_spec);
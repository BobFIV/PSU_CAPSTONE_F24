//#include <zephyr/drivers/i2c.h>

#define STTS751_TEMP_HIGH_REG            0x07
#define STTS751_TEMP_LOW_REG             0x06
#define STTS751_CONFIG_REG               0x04

#define I2C0_NODE DT_NODELABEL(mysensor)


//void init_temp_probe(const struct i2c_dt_spec);
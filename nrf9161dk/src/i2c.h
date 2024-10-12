//#include <zephyr/drivers/i2c.h>

#define STTS751_TEMP_HIGH_REG            0x07
#define STTS751_TEMP_LOW_REG             0x06
#define STTS751_CONFIG_REG               0x04

#define I2C0_NODE DT_NODELABEL(mysensor)

//initialize temperature polling
int i2c_init_temp_probe(void);

//retrieve the temperature (fahrenheit) from the device
double i2c_get_temp(void);
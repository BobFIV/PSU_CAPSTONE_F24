#ifndef MAIN_H_
#include <zephyr/sys/dlist.h>
typedef struct {
    int16_t snr;
    uint16_t hwid;
    uint16_t parent;
    float latitude;
    float longitude;
    int8_t temperature;
    uint8_t speed;
    int8_t accelX;
    int8_t accelY;
    int8_t accelZ;
} data_point;
typedef struct {
    uint16_t hwid;
    bool locked;
} update_point;
typedef struct {
    data_point dp;
    sys_dnode_t node;
} dpq_item;
#endif
#define MAIN_H_
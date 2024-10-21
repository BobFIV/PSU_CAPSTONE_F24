#include "dect-nr.h"
#define MESH_H_

typedef struct {
    int rssi;
    uint16_t hwid;
    uint16_t parent;
    float latitude;
    float longitude;
    float temperature;
    float speed;
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
} data_point;
typedef struct {
    uint16_t hwid;
    bool locked;
} update_point;


#define IS_ROOT

#define NUM_DEVICES 2
//device list
extern uint16_t all_devices[NUM_DEVICES];

#ifndef IS_ROOT
int mesh(data_point* sens_data, struct k_sem* mesh_sem);
#else
int mesh(update_point* lock_data, struct k_sem* mesh_sem);
#endif

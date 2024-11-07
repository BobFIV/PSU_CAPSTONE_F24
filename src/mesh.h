#include "dect-nr.h"
#define MESH_H_
#include <stdlib.h>

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


#define WAIT_BEFORE_NEXT_CYCLE 1600
#define ADDITIONAL_WAIT 50
#define CONCURRENCY_DELAY 1
#define TIMEOUT_BECOME_ROOT 3*WAIT_BEFORE_NEXT_CYCLE

#define NUM_DEVICES 3
//device list
//extern uint16_t all_devices[NUM_DEVICES];


int mesh_node(data_point* sens_data, struct k_sem* mesh_sem);
int mesh_node_switch_init(void);

#include "dect-nr.h"
#define MESH_H_
#include <stdlib.h>

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

//int a = sizeof(data_point);

#define WAIT_BEFORE_NEXT_CYCLE 1600
#define ADDITIONAL_WAIT 50
#define CONCURRENCY_DELAY 1
#define TIMEOUT_BECOME_ROOT 3*WAIT_BEFORE_NEXT_CYCLE

#define NUM_DEVICES 3
//device list
//extern uint16_t all_devices[NUM_DEVICES];


int mesh_node(data_point* sens_data, struct k_sem* mesh_sem);
int mesh_node_switch_init(void);

#include "dect-nr.h"


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

int mesh(data_point* sens_data, struct k_sem* mesh_sem);


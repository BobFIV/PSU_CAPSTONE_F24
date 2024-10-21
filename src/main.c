#ifndef UART_H_
#include "uart.h"
#endif
#ifndef MESH_H_
#include "mesh.h"
#endif

data_point point;
LOG_MODULE_REGISTER(main_module, LOG_LEVEL_INF);
int fake_thread(data_point* dpoint){
    LOG_INF("test");
    dpoint->latitude = 0.0f;
    dpoint->longitude = 0.0f;
    while(1){
        LOG_INF("test1");
        dpoint->latitude++;
        dpoint->longitude++;
        k_msleep(500);
    }
}

update_point device_locks[NUM_DEVICES];



K_SEM_DEFINE(mesh_sem, 1, 1);



#ifndef IS_ROOT

K_THREAD_DEFINE(UART_thread, 2048, uart_main, &point, NULL, NULL, 1, 0, 0);
K_THREAD_DEFINE(MESH_thread, 2048, mesh, &point, &mesh_sem, NULL, 0, 0, 0);

#else

K_THREAD_DEFINE(UART_thread, 2048, uart_main, &device_locks, NULL, NULL, 1, 0, 0);
K_THREAD_DEFINE(MESH_thread, 2048, mesh, &device_locks, &mesh_sem, NULL, 0, 0, 0);

#endif


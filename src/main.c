#include "mesh.h"
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

K_SEM_DEFINE(mesh_sem, 1, 1);

K_THREAD_DEFINE(MESH_thread, 2048, mesh, &point, &mesh_sem, NULL, 0, 0, 0);

#ifndef IS_ROOT

K_THREAD_DEFINE(UART_thread, 2048, fake_thread, &point, NULL, NULL, 1, 0, 0);
#endif


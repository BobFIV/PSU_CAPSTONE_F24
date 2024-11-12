#ifndef UART_H_
#include "uart.h"
#endif
#ifndef MESH_H_
#include "mesh.h"
#endif
#ifndef MESHR_H_
#include "mesh-root.h"
#endif
#include <stdlib.h>

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
bool is_root = false;

//update_point device_locks[NUM_DEVICES];
sys_dlist_t devices;


K_THREAD_DEFINE(UART_thread, 2048, uart_main, &point, &devices, NULL, 1, 0, 0);




K_THREAD_DEFINE(MESH_thread_node, 2048, mesh_node, &point, &mesh_sem, NULL, 0, 0, 0);



K_THREAD_DEFINE(MESH_thread_root, 2048, mesh_root, &mesh_sem, &point, &devices, 0, 0, 0);




int switch_node_root(void){
    if(is_root) {
        is_root = false;
        mesh_root_switch_init();
        k_thread_resume(MESH_thread_root);
        k_thread_suspend(MESH_thread_node);
    } else {
        is_root = true;
        mesh_node_switch_init();
        k_thread_resume(MESH_thread_node);
        k_thread_suspend(MESH_thread_root);
    }
    return 0;
}

//#define IS_ROOT






#include "dect-nr.h"
#define MESHR_H_

#ifndef MESH_H_
#include "mesh.h"
#endif
#include <zephyr/sys/dlist.h>

int mesh_root(struct k_sem* mesh_sem, data_point* last_data_point, sys_dlist_t* devices);
int mesh_root_switch_init(void);

typedef struct {
    uint16_t hwid;
    bool locked;
    sys_dnode_t node;
} device_in_list;
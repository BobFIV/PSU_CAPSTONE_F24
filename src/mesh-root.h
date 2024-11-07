#include "dect-nr.h"
#define MESHR_H_

#ifndef MESH_H_
#include "mesh.h"
#endif


int mesh_root(update_point* lock_data, struct k_sem* mesh_sem);
int mesh_root_switch_init(void);
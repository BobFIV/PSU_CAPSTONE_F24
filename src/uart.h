#ifndef MESH_H_
#include "mesh.h"
#endif

#define UART_H_

int uart_send_data(data_point out_data);
int uart_module_init(void);


int uart_main(data_point* uart_data_point, sys_dlist_t* devices);



#ifndef MESH_H_
#include "mesh.h"
#endif

#define UART_H_

int uart_send_data(data_point out_data);
int uart_module_init(void);

#ifndef IS_ROOT
int uart_main(data_point* uart_data_point);
#else
int uart_main(update_point* locks);
#endif


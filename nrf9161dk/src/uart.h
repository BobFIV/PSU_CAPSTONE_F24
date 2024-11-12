#include "main.h"
#include <zephyr/kernel.h>

int uart_send_data(data_point out_data);
int uart_send_upd(update_point out_data);
int uart_module_init(sys_dlist_t* dpq, struct k_sem* data_sem);
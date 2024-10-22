#include "coap_onem2m.h"

#define UART_H_
int uart_send_data(update_point out_data);
int uart_module_init(void);

typedef int (*uart_module_callback_t)(data_point);

void uart_coap_callback_set(uart_module_callback_t callback);
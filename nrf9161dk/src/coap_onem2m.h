#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/random/random.h>
#include <string.h>
#include "lte.h"
#include <zephyr/net/coap.h>

/* oneM2M CoAP Options , here just for now */
#define COAP_OPTION_ONEM2M_OT 259
#define COAP_OPTION_ONEM2M_RTURI 263
#define COAP_OPTION_ONEM2M_TY 267
#define COAP_OPTION_ONEM2M_RVI 271
#define COAP_OPTION_ONEM2M_ASRI 275
#define COAP_OPTION_ONEM2M_FR 279
#define COAP_OPTION_ONEM2M_RQI 283
#define COAP_OPTION_ONEM2M_RQET 291
#define COAP_OPTION_ONEM2M_RSET 295
#define COAP_OPTION_ONEM2M_OET 299
#define COAP_OPTION_ONEM2M_EC 303
#define COAP_OPTION_ONEM2M_RSC 307
#define COAP_OPTION_ONEM2M_GID 311
#define COAP_OPTION_ONEM2M_CTO 319
#define COAP_OPTION_ONEM2M_CTS 323
#define COAP_OPTION_ONEM2M_ATI 327
#define COAP_OPTION_ONEM2M_VSI 331
#define COAP_OPTION_ONEM2M_GTM 335
#define COAP_OPTION_ONEM2M_AUS 339
#define COAP_OPTION_ONEM2M_OMR 343
#define COAP_OPTION_ONEM2M_PRPI 347
#define COAP_OPTION_ONEM2M_MSU 351

/* oneM2M CoAP Content Type */
#define COAP_CONTENT_FORMAT_ONEM2M_TY_AE 2
#define COAP_CONTENT_FORMAT_ONEM2M_TY_CONTAINER 3
#define COAP_CONTENT_FORMAT_ONEM2M_TY_CONTAINER_INSTANCE 4
#define COAP_CONTENT_FORMAT_ONEM2M_TY_FLEXCONTAINER 28

/* CoAP Content Format*/
#define COAP_CONTENT_FORMAT_ONEM2M_JSON 10015

#define APP_COAP_VERSION 1
#define APP_COAP_MAX_MSG_LEN 1280
#define APP_ONEM2M_VERSION 3

struct bike {
    float temperature;
    float speed;
    float latitude;
    float longitude;
};

struct battery {
    int lvl;
    bool lowby;
};

struct mesh_connectivity {
    char neibo[20];
    int rssi;
};

struct lock {
    bool lock;
};

union resource_data {
    struct bike bikedata;
    struct battery batterydata;
    struct mesh_connectivity meshdata;
    struct lock lockdata;
};

enum resource {
    BIKEDATA = 0,
    BATTERY = 1,
    MESH_CONNECTIVITY = 2,
    LOCK = 3
};

ssize_t coap_receive(void);
int create_get_request_payload(char* str_buffer, enum resource res);
int create_put_request_payload(char* str_buffer, union resource_data res_data, enum resource res);
int server_resolve(void);
int client_init(void);
int client_get_send(enum resource res);
int client_put_send(union resource_data res_data, enum resource res);
int client_handle_response(uint8_t *buf, int received);
ssize_t onem2m_receive(void);
void onem2m_close_socket(void);
int onem2m_parse(int received);


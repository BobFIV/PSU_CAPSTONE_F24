

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/hwinfo.h>

#ifndef DECTNR_H_

#define DATA_LEN_MAX 64


typedef struct {
    bool is_ping;
	bool is_request;
    uint16_t target_hwid;
    uint16_t hwid;
    uint16_t sender_hwid;
	uint16_t op_hwid;
	int16_t snr;
    int8_t temperature;
    uint8_t speed;
    float latitude;
    float longitude;
	int8_t accelX;
	int8_t accelY;
	int8_t accelZ;
	bool locked;
} dect_packet;



typedef int (*dect_callback_t)(dect_packet, int snr);

//uint16_t device_id;

/* Header type 1, due to endianness the order is different than in the specification. */
struct phy_ctrl_field_common {
	uint32_t packet_length : 4;
	uint32_t packet_length_type : 1;
	uint32_t header_format : 3;
	uint32_t short_network_id : 8;
	uint32_t transmitter_id_hi : 8;
	uint32_t transmitter_id_lo : 8;
	uint32_t df_mcs : 3;
	uint32_t reserved : 1;
	uint32_t transmit_power : 4;
	uint32_t pad : 24;
};



void dect_data_callback(const void* data, int snr);
void dect_set_callback(dect_callback_t callback);
int dect_init(void);
int dect_close(void);
int dect_send_s(char* string, size_t len);
int dect_send(dect_packet send_data);
int dect_listen_cont(void);
dect_packet from_raw(const void* data);
void to_raw(dect_packet data, void* buffer);
int dect_listen(void);

#endif

#define DECTNR_H_


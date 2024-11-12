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
#include "coap_onem2m.h"

char message_buffer[200];
char originator[] = "CAdmin";

/* current CoAP resource */
char current_coap_tx_resource[sizeof("cse-in/00000-AE/data")] = "cse-in/00000-AE/data";
char current_coap_rx_resource[sizeof("cse-in/00000-AE/lock")] = "cse-in/00000-AE/lock";


/* the buffer to receive the response. */
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];
/* the CoAP token */
static uint16_t next_token;

/* oneM2M RQI request identifier */
static uint8_t request_identifier;
static char request_identifier_str[9];

/* socket and server */
static int sock;
static struct sockaddr_storage server;

//Register this as the coap module
LOG_MODULE_REGISTER(CoAP_Module, LOG_LEVEL_INF);

void get_coap_tx_resource(uint16_t hwid){
	snprintk(current_coap_tx_resource, sizeof(current_coap_tx_resource), "cse-in/ae_%05d", hwid);
	snprintk(current_coap_rx_resource, sizeof(current_coap_rx_resource), "cse-in/ae_%05d", hwid);
}

int create_get_request_payload(char* str_buffer, enum resource res)
{
	char payload_str[200] = {"\0"};

	switch (res) {
		case BIKEDATA:
			snprintk(payload_str, sizeof(payload_str), "{\"bdm:bikDt\": [\"tempe\", \"speed\", \"latie\", \"longe\"]}");
			break;
		case BATTERY:
			snprintk(payload_str, sizeof(payload_str), "{\"cod:bat\"}: [\"lvl\", \"lowBy\"]}");
			break;
		case MESH_CONNECTIVITY:
			snprintk(payload_str, sizeof(payload_str), "{\"bdm:msCoy\": [\"neibo\", \"stnr\"]}");
			break;
		case LOCK:
			snprintk(payload_str, sizeof(payload_str), "{\"m2m:atrl\": [\"lock\", \"pi\"]}");
			break;
	}

	strcpy(str_buffer, payload_str);

	return 0;
}

int create_put_request_payload(char* str_buffer, struct resource_data res_data, enum resource res)
{
    char payload_str[200];

    switch (res) {
        case BIKEDATA:
			struct bike bike_placeholder = res_data.bikedata;

			LOG_DBG("latie: %.06f", (double) bike_placeholder.latie);
			LOG_DBG("longe: %.06f", (double) bike_placeholder.longe);
			LOG_DBG("speed: %.02f", (double) bike_placeholder.speed);
			LOG_DBG("accel: %.02f", (double) bike_placeholder.accel);
			LOG_DBG("tempe: %.02f", (double) bike_placeholder.tempe);

			snprintk(payload_str, sizeof(payload_str), "{\"bdm:bikDt\": {\"latie\": %.06f, \"longe\": %.06f, \"speed\": %.01f, \"accel\": %.01f, \"tempe\": %.01f}}",
					 (double) bike_placeholder.latie, (double) bike_placeholder.longe, (double) bike_placeholder.speed, (double) bike_placeholder.accel, (double) bike_placeholder.tempe);
            break;

        case BATTERY:
			struct battery battery_placeholder = res_data.batterydata;

			LOG_DBG("lvl: %d", battery_placeholder.lvl);
			LOG_DBG("lowBy: %s", battery_placeholder.lowBy ? "true" : "false");

            snprintk(payload_str, sizeof(payload_str), "{\"cod:bat\": {\"lvl\": %d, \"lowBy\": %s}}",
                     battery_placeholder.lvl, battery_placeholder.lowBy ? "true" : "false");
            break;

        case MESH_CONNECTIVITY:
			struct mesh_connectivity mesh_placeholder = res_data.meshdata;
			LOG_DBG("neibo: %05d", mesh_placeholder.neibo);
			LOG_DBG("stnr: %d", mesh_placeholder.stnr);
            snprintk(payload_str, sizeof(payload_str), "{\"bdm:msCoy\": {\"neibo\": \"%05d\", \"stnr\": %d}}",
                     mesh_placeholder.neibo, mesh_placeholder.stnr);
            break;

        case LOCK:
			struct lock lock_placeholder = res_data.lockdata;

            snprintk(payload_str, sizeof(payload_str), "{\"cod:lock\": {\"lock\": %s}}",
                     lock_placeholder.lock ? "true" : "false");
            break;

        default:
            LOG_ERR("Unknown resource type");
            return -EINVAL;
    }

    strcpy(str_buffer, payload_str);

    return 0;
}

int onem2m_coap_options_helper(struct coap_packet *request, enum resource res, char* rx_or_tx)
{
	int err;
	
	// 1. Append the resource path
	
	uint8_t uri_path[256];
	switch(res) {
		case BIKEDATA:
			snprintk(uri_path, sizeof(uri_path), "%s/bikeData", rx_or_tx);
			break;
		case BATTERY:
			snprintk(uri_path, sizeof(uri_path), "%s/battery", rx_or_tx);
			break;
		case MESH_CONNECTIVITY:
			snprintk(uri_path, sizeof(uri_path), "%s/meshConnectivity", rx_or_tx);
			break;
		case LOCK:	
			snprintk(uri_path, sizeof(uri_path), "%s/lock", rx_or_tx);
			break;
	}

	LOG_INF("URI path: %s", uri_path);

	err = coap_packet_append_option(
				request,
				COAP_OPTION_URI_PATH,
				(uint8_t *) uri_path,
				strlen(uri_path)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option URI path, %d", err);
		return err;
	}
	
	// 2. Append the CoAP content format as OneM2M JSON

	const uint8_t onem2m_json = COAP_CONTENT_FORMAT_ONEM2M_JSON;
	err = coap_packet_append_option(
				request,
				COAP_OPTION_CONTENT_FORMAT,
				&onem2m_json,
				sizeof(onem2m_json)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option content format, %d", err);
		return err;
	}

	// 3. Append the OneM2M type as FlexContainer

	const uint8_t coap_content_format_onem2m_ty_flexcontainer = COAP_CONTENT_FORMAT_ONEM2M_TY_FLEXCONTAINER;
	err = coap_packet_append_option(
				request,
				COAP_OPTION_ONEM2M_TY,
				(uint8_t *)&coap_content_format_onem2m_ty_flexcontainer,
				sizeof(coap_content_format_onem2m_ty_flexcontainer)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option oneM2M TY, %d", err);
		return err;
	}

	// 4. Append the OneM2M version as 3

	char app_onem2m_version[2];
	snprintk(app_onem2m_version, sizeof(app_onem2m_version), "%d", APP_ONEM2M_VERSION);
	err = coap_packet_append_option(
				request,
				COAP_OPTION_ONEM2M_RVI,
				(uint8_t *)&app_onem2m_version,
				strlen(app_onem2m_version)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option oneM2M RVI, %d", err);
		return err;
	}

	// 5. Append the OneM2M originator as CAdmin

	err = coap_packet_append_option(
				request,
				COAP_OPTION_ONEM2M_FR,
				(uint8_t *)originator,
				strlen(originator)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option oneM2M FR, %d", err);
		return err;
	}

	// 6. Append the OneM2M request identifier

	request_identifier = sys_rand32_get();
	snprintk(request_identifier_str, sizeof(request_identifier_str), "%08x", request_identifier);
	err = coap_packet_append_option(
				request,
				COAP_OPTION_ONEM2M_RQI,
				(uint8_t *)&request_identifier_str,
				strlen(request_identifier_str)
			);
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option oneM2M RQI, %d", err);
		return err;
	}
}

/**@brief Resolves the configured hostname. */
int server_resolve(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM
	};
	char ipv4_addr[NET_IPV4_ADDR_LEN];

	err = getaddrinfo(CONFIG_COAP_SERVER_HOSTNAME, NULL, &hints, &result);
	if (err != 0) {
		LOG_ERR("ERROR: getaddrinfo failed %d\n", err);
		return -EIO;
	}

	if (result == NULL) {
		LOG_ERR("ERROR: Address not found\n");
		return -ENOENT;
	}

	/* IPv4 Address. */
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);

	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_COAP_SERVER_PORT);

	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);

	/* Free the address. */
	freeaddrinfo(result);

	return 0;
}

/**@brief Initialize the CoAP client */
int client_init(void)
{
	int err;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create CoAP socket: %d.\n", errno);
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connect failed : %d\n", errno);
		return -errno;
	}

	LOG_INF("Successfully connected to server");

	next_token = sys_rand32_get();

	return 0;
}

/**@brief Send CoAP GET request. */
int client_get_send(enum resource res)
{
	int err;
	struct coap_packet request;
	next_token = sys_rand32_get();

	// Initialize the CoAP packet

	err = coap_packet_init(
				&request,
				coap_buf,
				sizeof(coap_buf),
				APP_COAP_VERSION,
				COAP_TYPE_CON,
				sizeof(next_token),
				(uint8_t *)&next_token,
				COAP_METHOD_GET,
				coap_next_id()
			);

	if (err < 0) {
		LOG_ERR("Failed to create CoAP request, %d", err);
		return err;
	}

	// Append CoAP options

	err = onem2m_coap_options_helper(&request, res, current_coap_rx_resource);
	if (err < 0) {
		LOG_ERR("Failed to append CoAP options, %d", err);
		return err;
	}

	// Add the payload to the message

	err = coap_packet_append_payload_marker(&request);
	if (err < 0) {
		LOG_ERR("Failed to append payload marker, %d", err);
		return err;
	}

	create_get_request_payload(message_buffer, res);
	err = coap_packet_append_payload(
					&request,
					(uint8_t *)message_buffer,
					strlen(message_buffer)
				);
	if (err < 0) {
		LOG_ERR("Failed to append payload, %d", err);
		return err;
	}

	// Send the configured CoAP packet
	
	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d", errno);
		return -errno;
	}

	LOG_INF("CoAP GET request sent: Token 0x%04x", next_token);

	return 0;
}

/**@brief Send CoAP PUT request. */
int client_put_send(struct resource_data res_data, enum resource res)
{
	int err;
	struct coap_packet request;
	next_token = sys_rand32_get();

	// Initialize the CoAP packet

	err = coap_packet_init(
				&request,
				coap_buf,
				sizeof(coap_buf),
				APP_COAP_VERSION,
				COAP_TYPE_NON_CON,
				sizeof(next_token),
				(uint8_t *)&next_token,
				COAP_METHOD_PUT,
				coap_next_id()
			);

	if (err < 0) {
		LOG_ERR("Failed to create CoAP request, %d", err);
		return err;
	}

	// Append CoAP options

	err = onem2m_coap_options_helper(&request, res, current_coap_tx_resource);
	if (err < 0) {
		LOG_ERR("Failed to append CoAP options, %d", err);
		return err;
	}

	// Add the payload to the message

	err = coap_packet_append_payload_marker(&request);
	if (err < 0) {
		LOG_ERR("Failed to append payload marker, %d", err);
		return err;
	}

	create_put_request_payload(message_buffer, res_data, res);
	err = coap_packet_append_payload(
				&request,
				(uint8_t *)message_buffer,
				strlen(message_buffer)
			);
	if (err < 0) {
		LOG_ERR("Failed to append payload, %d", err);
		return err;
	}

	// Send the CoAP packet

	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d", errno);
		return -errno;
	}

	LOG_INF("CoAP PUT request sent: Token 0x%04x", next_token);

	return 0;
}

/**@brief Handles responses from the remote CoAP server. */
int client_handle_response(uint8_t *buf, int received)
{
	int out = 0;
	struct coap_packet reply;
	uint8_t token[8];
	uint16_t token_len;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t temp_buf[512];
	/* Parse the received CoAP packet */
	int err = coap_packet_parse(&reply, buf, received, NULL, 0);

	if (err < 0) {
		LOG_ERR("Malformed response received: %d", err);
		return err;
	}

	/* Confirm the token in the response matches the token sent */
	token_len = coap_header_get_token(&reply, token);

	if ((token_len != sizeof(next_token)) || 
		(memcmp(&next_token, token, sizeof(next_token)) != 0)) 
	{
		// LOG_ERR("Invalid token received: 0x%02x%02x", token[1], token[0]);
		// return 0;
	}

	/* Retrieve the payload and confirm it's nonzero */
	payload = coap_packet_get_payload(&reply, &payload_len);

	if (payload_len > 0) { 
		snprintf(temp_buf, MIN(payload_len+1, sizeof(temp_buf)), "%s\0", payload); 
	} 

	else { 
		strcpy(temp_buf, "EMPTY"); 
	}

	/* Log the header code, token and payload of the response */
	
	if(payload_len >= 200) {
		LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: (LONG)",
		coap_header_get_code(&reply), token[1], token[0]);
	} else {
		LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: %s",
		coap_header_get_code(&reply), token[1], token[0], (char *)temp_buf);
	}
	if (payload_len > 0) { 
		int index;
		snprintf(temp_buf, MIN(payload_len+1, sizeof(temp_buf)), "%s", payload); 
		if(payload[9] == 'k'){
			if(payload[22] == 't'){
				out++;
				LOG_INF("Device Locked");
				index = 36;
			} else {
				LOG_INF("Device Unlocked");
				index = 37;
			}
			out += strtol(&(payload[index]), NULL, 10)*2;
			LOG_INF("Hwid: %d", out/2);
		}

	} 

	return out;
}

int onem2m_parse(int received)
{
    return client_handle_response(coap_buf, received);
}


ssize_t onem2m_receive(void)
{
    return recv(sock, coap_buf, sizeof(coap_buf), 0);
}

void onem2m_close_socket(void)
{
    return (void)close(sock);
}

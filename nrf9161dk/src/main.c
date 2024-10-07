/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#if NCS_VERSION_NUMBER < 0x20600
#include <zephyr/random/rand32.h>
#else 
#include <zephyr/random/random.h>
#endif

#include <string.h>

//#include <cJSON.h>

/* STEP 2.2 - Include the header file for the CoAP library */
#include <zephyr/net/coap.h>

/* STEP 4.1 - Define the macro for the message from the board */
// #define MESSAGE_TO_SEND "Hi from nRF91 Series device"
// char message_to_send[] = "{\"m2m:cin\": {\"cnf\": \"text/plain:0\", \"con\": \"{\\\"temperature\\\": 13.2, \\\"speed\\\": 47.5, \\\"latitude\\\": -44.9418, \\\"longitude\\\": -155.4347}\"}}";
char message_buffer[200];

char originator[] = "CAdmin";

/* STEP 4.2 - Define the macros for the CoAP version and message length */
#define APP_COAP_VERSION 1
#define APP_COAP_MAX_MSG_LEN 1280

#define APP_ONEM2M_VERSION 3

/* STEP 5 - Declare the buffer coap_buf to receive the response. */
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];

/* STEP 6.1 - Define the CoAP message token next_token */
static uint16_t next_token;

/* oneM2M RQI request identifier */
static uint8_t request_identifier;
static char request_identifier_str[9];

static int sock;
static struct sockaddr_storage server;

K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Bike_Sharing, LOG_LEVEL_INF);

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

/* oneM2M CoAP Content Formats */
#define COAP_CONTENT_FORMAT_ONEM2M_TY_AE 2
#define COAP_CONTENT_FORMAT_ONEM2M_TY_CONTAINER 3
#define COAP_CONTENT_FORMAT_ONEM2M_TY_CONTAINER_INSTANCE 4

/*
static char* create_request_payload(void)
{
	LOG_INF("Creating payload");

	float latitude = 0.0;
	float longitude = 0.0;

	cJSON *payload = cJSON_CreateObject();

	cJSON *m2mcin = cJSON_CreateObject(); // JSON object to store the sensor data
	cJSON_AddItemToObject(payload, "m2m:cin", m2mcin);

	cJSON_AddStringToObject(m2mcin, "cnf", "text/plain:0");

	cJSON *con = cJSON_CreateObject();
	cJSON_AddItemToObject(m2mcin, "con", con);
	cJSON_AddNumberToObject(con, "latitude", latitude);
	cJSON_AddNumberToObject(con, "longitude", longitude);

	LOG_INF("Payload created");

	char *payload_str = cJSON_PrintUnformatted(payload);
	LOG_INF("Payload: %s", payload_str);
	cJSON_Delete(payload);
	// make sure to free payload_str wherever this is used!! cJSON_free(payload_str);

	return payload_str;
}
*/

static int create_request_payload(char* str_buffer)
{
	float temperature = 0.0;
	float speed = 0.0;
	float latitude = 0.0;
	float longitude = 0.0;

	char payload_str[200] = "{\"m2m:cin\": {\"cnf\": \"text/plain:0\", \"con\": \"{";

	char temperature_str[10];
	sprintf(temperature_str, "%.2f", temperature);
	
	char speed_str[10];
	sprintf(speed_str, "%.2f", speed);
	
	char latitude_str[10];
	sprintf(latitude_str, "%.4f", latitude);
	
	char longitude_str[10];
	sprintf(longitude_str, "%.4f", longitude);

	strcat(payload_str, "\\\"temperature\\\": ");
	strcat(payload_str, temperature_str);

	strcat(payload_str, ", \\\"speed\\\": ");
	strcat(payload_str, speed_str);

	strcat(payload_str, ", \\\"latitude\\\": ");
	strcat(payload_str, latitude_str);

	strcat(payload_str, ", \\\"longitude\\\": ");
	strcat(payload_str, longitude_str);

	strcat(payload_str, "}\"}}");

	strcpy(str_buffer, payload_str);

	return 0;
}

/**@brief Resolves the configured hostname. */
static int server_resolve(void)
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
	LOG_INF("IPv4 Address found %s\n", ipv4_addr);

	/* Free the address. */
	freeaddrinfo(result);

	return 0;
}

/**@brief Initialize the CoAP client */
static int client_init(void)
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

	/* STEP 6.2 - Generate a random token after the socket is connected */
	next_token = sys_rand32_get();

	return 0;
}

static void lte_handler(const struct lte_lc_evt *const evt)
{
     switch (evt->type) {
     case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
            (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
            break;
        }
		LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
				"Connected - home network" : "Connected - roaming");
		k_sem_give(&lte_connected);
        break;
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
				"Connected" : "Idle");
		break;
     default:
             break;
     }
}

static int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

	/* lte_lc_init deprecated in >= v2.6.0 */
	#if NCS_VERSION_NUMBER < 0x20600
	err = lte_lc_init();
	if (err) {
		LOG_ERR("Failed to initialize LTE link control library, error: %d", err);
		return err;
	}
	#endif
	
	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

	return 0;
}

/**@biref Send CoAP GET request. */
static int client_get_send(void)
{
	/* STEP 7.1 - Create the CoAP message*/
	struct coap_packet request;

	next_token = sys_rand32_get();

	int err = coap_packet_init(
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

	if (err < 0)
	{
		LOG_ERR("Failed to create CoAP request, %d", err);
		return err;
	}

	/* STEP 7.2 - Add an option specifying the resource path */
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_RX_RESOURCE,
					strlen(CONFIG_COAP_RX_RESOURCE)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option, %d", err);
		return err;
	}

	/* Append the OneM2M options */
	char app_onem2m_version[2];
	snprintf(app_onem2m_version, sizeof(app_onem2m_version), "%d", APP_ONEM2M_VERSION);
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_RVI,
					(uint8_t *)&app_onem2m_version,
					strlen(app_onem2m_version)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M RVI, %d", err);
		return err;
	}

	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_FR,
					(uint8_t *)originator,
					strlen(originator)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M FR, %d", err);
		return err;
	}

	request_identifier = sys_rand32_get();
	snprintf(request_identifier_str, sizeof(request_identifier_str), "%08x", request_identifier);
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_RQI,
					(uint8_t *)&request_identifier_str,
					strlen(request_identifier_str)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M RQI, %d", err);
		return err;
	}

	/* STEP 7.3 - Send the configured CoAP packet */
	err = send(sock, request.data, request.offset, 0);

	if (err < 0)
	{
		LOG_ERR("Failed to send CoAP request, %d", errno);
		return -errno;
	}

	LOG_INF("CoAP GET request sent: Token 0x%04x", next_token);

	return 0;
}

/**@biref Send CoAP PUT request. */
static int client_put_send(void)
{
	int err;
	struct coap_packet request;

	next_token = sys_rand32_get();

	/* STEP 8.1 - Initialize the CoAP packet and append the resource path */
	err = coap_packet_init(
				&request,
				coap_buf,
				sizeof(coap_buf),
				APP_COAP_VERSION,
				COAP_TYPE_CON,
				sizeof(next_token),
				(uint8_t *)&next_token,
				COAP_METHOD_PUT,  // PUT Method
				coap_next_id()
			);

	if (err < 0)
	{
		LOG_ERR("Failed to create CoAP request, %d", err);
		return err;
	}

	err = coap_packet_append_option(
					&request,
					COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_TX_RESOURCE,
					strlen(CONFIG_COAP_TX_RESOURCE)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option, %d", err);
		return err;
	}

	/* STEP 8.2 - Append the content format as plain text */
	const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_CONTENT_FORMAT,
					&text_plain,
					sizeof(text_plain)
				);

	/* STEP 8.3 - Add the payload to the message */
	err = coap_packet_append_payload_marker(&request);

	if (err < 0)
	{
		LOG_ERR("Failed to append payload marker, %d", err);
		return err;
	}

	create_request_payload(message_buffer);
	err = coap_packet_append_payload(
					&request,
					(uint8_t *)message_buffer,
					sizeof(message_buffer)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to append payload, %d", err);
		return err;
	}

	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	LOG_INF("CoAP PUT request sent: Token 0x%04x\n", next_token);

	return 0;
}

/**@biref Send CoAP POST request. */
static int client_post_send(void)
{
	int err;
	struct coap_packet request;

	next_token = sys_rand32_get();

	/* Initialize the CoAP packet and append the resource path */
	err = coap_packet_init(
				&request,
				coap_buf,
				sizeof(coap_buf),
				APP_COAP_VERSION,
				COAP_TYPE_CON,
				sizeof(next_token),
				(uint8_t *)&next_token,
				COAP_METHOD_POST,  // PUT Method
				coap_next_id()
			);

	if (err < 0)
	{
		LOG_ERR("Failed to create CoAP request, %d", err);
		return err;
	}

	/* Append the resource path */
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_TX_RESOURCE,
					strlen(CONFIG_COAP_TX_RESOURCE)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option resource URI, %d", err);
		return err;
	}

	/* Append the content format as plain text */
	const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_CONTENT_FORMAT,
					&text_plain,
					sizeof(text_plain)
				);

	/* Append the OneM2M options */	
	const uint8_t coap_content_format_onem2m_ty_container = COAP_CONTENT_FORMAT_ONEM2M_TY_CONTAINER_INSTANCE;
	err = coap_packet_append_option(
				&request,
				COAP_OPTION_ONEM2M_TY,
				(uint8_t *)&coap_content_format_onem2m_ty_container,
				sizeof(coap_content_format_onem2m_ty_container)
			);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M TY, %d", err);
		return err;
	}

	char app_onem2m_version[2];
	snprintf(app_onem2m_version, sizeof(app_onem2m_version), "%d", APP_ONEM2M_VERSION);
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_RVI,
					(uint8_t *)&app_onem2m_version,
					strlen(app_onem2m_version)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M RVI, %d", err);
		return err;
	}

	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_FR,
					(uint8_t *)originator,
					strlen(originator)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M FR, %d", err);
		return err;
	}

	request_identifier = sys_rand32_get();
	snprintf(request_identifier_str, sizeof(request_identifier_str), "%08x", request_identifier);
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_ONEM2M_RQI,
					(uint8_t *)&request_identifier_str,
					strlen(request_identifier_str)
				);

	if (err < 0)
	{
		LOG_ERR("Failed to encode CoAP option oneM2M RQI, %d", err);
		return err;
	}

	/* Add the payload to the message */
	err = coap_packet_append_payload_marker(&request);

	if (err < 0)
	{
		LOG_ERR("Failed to append payload marker, %d", err);
		return err;
	}

	create_request_payload(message_buffer);
	err = coap_packet_append_payload(
					&request,
					(uint8_t *)message_buffer,
					strlen(message_buffer)
				);
	// cJSON_free(payload_str);

	if (err < 0)
	{
		LOG_ERR("Failed to append payload, %d", err);
		return err;
	}

	/* Send CoAP request */
	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	LOG_INF("CoAP POST request sent: Token 0x%04x\n", next_token);

	return 0;
}

/**@brief Handles responses from the remote CoAP server. */
static int client_handle_response(uint8_t *buf, int received)
{
	struct coap_packet reply;
	uint8_t token[8];
	uint16_t token_len;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t temp_buf[128];
	/* STEP 9.1 - Parse the received CoAP packet */
	int err = coap_packet_parse(&reply, buf, received, NULL, 0);

	if (err < 0) {
		LOG_ERR("Malformed response received: %d", err);
		return err;
	}

	/* STEP 9.2 - Confirm the token in the response matches the token sent */
	token_len = coap_header_get_token(&reply, token);

	if ((token_len != sizeof(next_token)) || 
		(memcmp(&next_token, token, sizeof(next_token)) != 0)) 
	{
		LOG_ERR("Invalid token received: 0x%02x%02x", token[1], token[0]);
		return 0;
	}

	/* STEP 9.3 - Retrieve the payload and confirm it's nonzero */
	payload = coap_packet_get_payload(&reply, &payload_len);

	if (payload_len > 0) { 
		snprintf(temp_buf, MIN(payload_len+1, sizeof(temp_buf)), "%s", payload); 
	} 

	else { 
		strcpy(temp_buf, "EMPTY"); 
	}

	/* STEP 9.4 - Log the header code, token and payload of the response */
	LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: %s",
	coap_header_get_code(&reply), token[1], token[0], (char *)temp_buf);

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 10 - Send a GET request or PUT request upon button triggers */
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) 
	{
		client_get_send();
	}
	
	else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK)
	{
		client_post_send();
	}
}

int main(void)
{
	int err;
	int received;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return 0;
	}

	if (client_init() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	while (1) {
		/* STEP 11 - Receive response from the CoAP server */
		received = recv(sock, coap_buf, sizeof(coap_buf), 0);
		if (received < 0) {
			LOG_ERR("Socket error: %d, exit", errno);
			break;
		} if (received == 0) {
			LOG_INF("Empty datagram");
			continue;
		}

		/* STEP 12 - Parse the received CoAP packet */
		err = client_handle_response(coap_buf, received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit");
			break;
		}

	}

	(void)close(sock);

	return 0;
}

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

int create_request_payload(char* str_buffer, struct data_point data)
{
	float temperature = data.temperature;
	float speed = data.speed;
	float latitude = data.latitude;
	float longitude = data.longitude;

	LOG_INF("Temperature:  %.02f", temperature);
	LOG_INF("Speed:        %.02f", speed);
	LOG_INF("Latitude:     %.06f", latitude);
	LOG_INF("Longitude:    %.06f", longitude);

	char payload_str[200] = "{\"m2m:cin\": {\"cnf\": \"text/plain:0\", \"con\": \"{";

	char temperature_str[14];
	snprintk(temperature_str, sizeof(temperature_str), "%.02f", temperature);
	
	char speed_str[14];
	snprintk(speed_str, sizeof(speed_str), "%.02f", speed);
	
	char latitude_str[14];
	snprintk(latitude_str, sizeof(latitude_str), "%.06f", latitude);
	
	char longitude_str[14];
	snprintk(longitude_str, sizeof(longitude_str), "%.06f", longitude);

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
	LOG_INF("IPv4 Address found %s\n", ipv4_addr);

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
int client_get_send(void)
{
	
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

	/* an option specifying the resource path */
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

	/* Send the configured CoAP packet */
	err = send(sock, request.data, request.offset, 0);

	if (err < 0)
	{
		LOG_ERR("Failed to send CoAP request, %d", errno);
		return -errno;
	}

	LOG_INF("CoAP GET request sent: Token 0x%04x", next_token);

	return 0;
}

/**@brief Send CoAP PUT request. */
int client_put_send(struct data_point data)
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

	/* Append the content format as plain text */
	const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;
	err = coap_packet_append_option(
					&request,
					COAP_OPTION_CONTENT_FORMAT,
					&text_plain,
					sizeof(text_plain)
				);

	/* Add the payload to the message */
	err = coap_packet_append_payload_marker(&request);

	if (err < 0)
	{
		LOG_ERR("Failed to append payload marker, %d", err);
		return err;
	}

	create_request_payload(message_buffer, data);
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

/**@brief Send CoAP POST request. */
int client_post_send(struct data_point data)
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

	create_request_payload(message_buffer, data);
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
int client_handle_response(uint8_t *buf, int received)
{
	struct coap_packet reply;
	uint8_t token[8];
	uint16_t token_len;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t temp_buf[128];
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
		LOG_ERR("Invalid token received: 0x%02x%02x", token[1], token[0]);
		return 0;
	}

	/* Retrieve the payload and confirm it's nonzero */
	payload = coap_packet_get_payload(&reply, &payload_len);

	if (payload_len > 0) { 
		snprintf(temp_buf, MIN(payload_len+1, sizeof(temp_buf)), "%s", payload); 
	} 

	else { 
		strcpy(temp_buf, "EMPTY"); 
	}

	/* Log the header code, token and payload of the response */
	LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: %s",
	coap_header_get_code(&reply), token[1], token[0], (char *)temp_buf);

	return 0;
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
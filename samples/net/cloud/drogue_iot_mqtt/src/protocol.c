/* Protocol implementation. */
/*
 * Copyright (c) 2018-2019 Linaro Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(net_drogue_iot_mqtt, LOG_LEVEL_DBG);
#include "protocol.h"
#define APP_SLEEP_MSECS 8000

#include <string.h>

#include <zephyr/drivers/entropy.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/zephyr.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>

extern int64_t time_base;

/* private key information */
extern unsigned char zepfull_private_der[];
extern unsigned int zepfull_private_der_len;

/*
 * This is the hard-coded root certificate that we accept.
 */
#include "globalsign.inc"

static uint8_t client_id[] = "foo";
static uint8_t client_username[512];
static uint8_t pub_topic[] = "telemetry";

static struct mqtt_publish_param pub_data;

static bool connected;
static uint64_t next_alive;

/* The mqtt client struct */
static struct mqtt_client client_ctx;

/* MQTT Broker details. */
static struct sockaddr_storage broker;

/* Buffers for MQTT client. */
static uint8_t rx_buffer[1024];
static uint8_t tx_buffer[1024];

static struct mqtt_utf8 username = {
	.utf8 = client_username,
};

static struct mqtt_utf8 password = {
	.utf8 = CONFIG_DROGUE_DEVICE_PASSWORD,
};

static sec_tag_t m_sec_tags[] = {
#if defined(MBEDTLS_X509_CRT_PARSE_C)
	1,
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
	APP_PSK_TAG,
#endif
};

/* Zephyr implementation of POSIX `time`.  Has to be called k_time
 * because time is already taken by newlib.  The clock will be set by
 * the SNTP client when it receives the time.  We make no attempt to
 * adjust it smoothly, and it should not be used for measuring
 * intervals.  Use `k_uptime_get()` directly for that.   Also the
 * time_t defined by newlib is a signed 32-bit value, and will
 * overflow in 2037.
 */
time_t my_k_time(time_t *ptr)
{
	int64_t stamp;
	time_t now;

	stamp = k_uptime_get();
	now = (time_t)((stamp + time_base) / 1000);

	if (ptr) {
		*ptr = now;
	}

	return now;
}

void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_SUBACK:
		LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
		break;

	case MQTT_EVT_UNSUBACK:
		LOG_INF("UNSUBACK packet id: %u", evt->param.suback.message_id);
		break;

	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}

		connected = true;
		LOG_INF("MQTT client connected!");
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected %d", evt->result);

		connected = false;

		break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		/* increment message id for when we send next message */
		pub_data.message_id += 1U;
		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
		break;

	default:
		LOG_INF("MQTT event received %d", evt->type);
		break;
	}
}

static int wait_for_input(int timeout)
{
	int res;
	struct zsock_pollfd fds[1] = {
		[0] = {.fd = client_ctx.transport.tls.sock, .events = ZSOCK_POLLIN, .revents = 0},
	};

	res = zsock_poll(fds, 1, timeout);
	if (res < 0) {
		LOG_ERR("poll read event error");
		return -errno;
	}

	return res;
}

#define ALIVE_TIME (60 * MSEC_PER_SEC)

void mqtt_startup(void)
{
	int err, cnt;
	char pub_msg[64];
	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
	struct mqtt_client *client = &client_ctx;
	static struct zsock_addrinfo hints;
	struct zsock_addrinfo *haddr;
	int res = 0;
	int retries = 5;

	mbedtls_platform_set_time(my_k_time);

	err = tls_credential_add(1, TLS_CREDENTIAL_CA_CERTIFICATE, globalsign_certificate,
				 sizeof(globalsign_certificate));
	if (err < 0) {
		LOG_ERR("Failed to register public certificate: %d", err);
	}

	while (retries) {
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		cnt = 0;
		while ((err = getaddrinfo(CONFIG_DROGUE_HOSTNAME, CONFIG_DROGUE_PORT, &hints,
					  &haddr)) &&
		       cnt < 3) {
			LOG_ERR("Unable to get address for broker, retrying");
			cnt++;
		}

		if (err != 0) {
			LOG_ERR("Unable to get address for broker, error %d", err);
			return;
		}
		LOG_INF("DNS resolved for %s:%s", CONFIG_DROGUE_HOSTNAME, CONFIG_DROGUE_PORT);

		mqtt_client_init(client);

		int port = atoi(CONFIG_DROGUE_PORT);
		LOG_INF("DROGUE PORT: %d", port);
		broker4->sin_family = AF_INET;
		broker4->sin_port = htons(port);
		net_ipaddr_copy(&broker4->sin_addr, &net_sin(haddr->ai_addr)->sin_addr);

		sprintf(client_username, "%s@%s", CONFIG_DROGUE_DEVICE, CONFIG_DROGUE_APPLICATION);

		username.size = strlen(client_username);
		password.size = strlen(CONFIG_DROGUE_DEVICE_PASSWORD);

		/* MQTT client configuration */
		client->broker = &broker;
		client->keepalive = 0;
		client->evt_cb = mqtt_evt_handler;
		client->client_id.utf8 = client_id;
		client->client_id.size = 0;
		client->password = &password;
		client->user_name = &username;
		client->protocol_version = MQTT_VERSION_3_1_1;
		client->clean_session = 1;

		/* MQTT buffers configuration */
		client->rx_buf = rx_buffer;
		client->rx_buf_size = sizeof(rx_buffer);
		client->tx_buf = tx_buffer;
		client->tx_buf_size = sizeof(tx_buffer);

		/* MQTT transport configuration */
		client->transport.type = MQTT_TRANSPORT_SECURE;

		struct mqtt_sec_config *tls_config = &client->transport.tls.config;

		tls_config->peer_verify = TLS_PEER_VERIFY_NONE;
		tls_config->cipher_list = NULL;
		tls_config->sec_tag_list = m_sec_tags;
		tls_config->sec_tag_count = ARRAY_SIZE(m_sec_tags);
		tls_config->hostname = CONFIG_DROGUE_HOSTNAME;

		LOG_INF("Connecting to host: %s", CONFIG_DROGUE_HOSTNAME);
		err = mqtt_connect(client);
		if (err != 0) {
			LOG_ERR("could not connect, error %d", err);
			mqtt_disconnect(client);
			retries--;
			k_msleep(ALIVE_TIME);
			continue;
		}

		if (wait_for_input(5 * MSEC_PER_SEC) > 0) {
			mqtt_input(client);
			if (!connected) {
				LOG_ERR("failed to connect to mqtt_broker");
				mqtt_disconnect(client);
				retries--;
				k_msleep(ALIVE_TIME);
				continue;
			} else {
				break;
			}
		} else {
			LOG_ERR("failed to connect to mqtt broker");
			mqtt_disconnect(client);
			retries--;
			k_msleep(ALIVE_TIME);
			continue;
		}
	}

	if (!connected) {
		LOG_ERR("Failed to connect to client, aborting");
		return;
	}

	/* initialize publish structure */
	pub_data.message.topic.topic.utf8 = pub_topic;
	pub_data.message.topic.topic.size = strlen(pub_topic);
	pub_data.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	pub_data.message.payload.data = (uint8_t *)pub_msg;
	pub_data.message_id = 1U;
	pub_data.dup_flag = 0U;
	pub_data.retain_flag = 0U;

	mqtt_live(client);

	next_alive = k_uptime_get() + ALIVE_TIME;

	while (1) {
		LOG_INF("Publishing data");
		sprintf(pub_msg, "{\"temp\": 42.0}");
		pub_data.message.payload.len = strlen(pub_msg);
		err = mqtt_publish(client, &pub_data);
		if (err) {
			LOG_ERR("could not publish, error %d", err);
			break;
		}

		/* idle and process messages */
		while (k_uptime_get() < next_alive) {
			LOG_INF("... idling ...");
			if (wait_for_input(5 * MSEC_PER_SEC) > 0) {
				mqtt_input(client);
			}
		}

		mqtt_live(client);
		next_alive += ALIVE_TIME;
	}
}

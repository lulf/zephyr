/* Full-stack IoT client example. */

/*
 * Copyright (c) 2018 Linaro Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dhcp.h"
#include "protocol.h"

#include <zephyr/net/net_config.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/sntp.h>
#include <zephyr/zephyr.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(net_drogue_iot_mqtt, LOG_LEVEL_INF);

/* This comes from newlib. */
#include <inttypes.h>
#include <time.h>

int64_t time_base;

int do_sntp(void)
{
	int rc;
	struct sntp_time sntp_time;
	char time_str[sizeof("1970-01-01T00:00:00")];

	LOG_INF("Sending NTP request for current time:");

	rc = sntp_simple("time.google.com", SYS_FOREVER_MS, &sntp_time);
	if (rc == 0) {
		time_base = sntp_time.seconds * MSEC_PER_SEC - k_uptime_get();

		/* Convert time to make sure. */
		time_t now = sntp_time.seconds;
		struct tm now_tm;

		gmtime_r(&now, &now_tm);
		strftime(time_str, sizeof(time_str), "%FT%T", &now_tm);
		// LOG_INF("  Acquired time: %s", log_strdup(time_str));

	} else {
		LOG_ERR("  Failed to acquire SNTP, code %d\n", rc);
	}
	return rc;
}

void main(void)
{
	LOG_INF("Drogue IoT - MQTT Client - Temperature demo");
	app_dhcpv4_startup();

	LOG_INF("Should have DHCPv4 lease at this point.");

	/* early return if we failed to acquire time */
	if (do_sntp() != 0) {
		LOG_ERR("Failed to get NTP time");
		return;
	}

	mqtt_startup();
}

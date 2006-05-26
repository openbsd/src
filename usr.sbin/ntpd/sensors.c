/*	$OpenBSD: sensors.c,v 1.1 2006/05/26 00:33:16 henning Exp $ */

/*
 * Copyright (c) 2006 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

#define SENSORS_MAX	255

void	sensor_add(struct ntpd_conf *, struct sensor *);

void
sensor_init(struct ntpd_conf *conf)
{
	TAILQ_INIT(&conf->ntp_sensors);
}

void
sensor_scan(struct ntpd_conf *conf)
{
	struct sensor	sensor;
	int		i;
	int		mib[3];
	size_t		len;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	for (i = 0; i < SENSORS_MAX; i++) {
		mib[2] = i;
		len = sizeof(sensor);
		if (sysctl(mib, 3, &sensor, &len, NULL, 0) == -1) {
			if (errno != ENOENT)
				log_warn("sensor_scan sysctl");
			break;
		}

		if (sensor.type == SENSOR_TIMEDELTA)
			sensor_add(conf, &sensor);
	}
}

void
sensor_add(struct ntpd_conf *conf, struct sensor *sensor)
{
	struct ntp_sensor	*s;

	/* check wether it is already there */
	TAILQ_FOREACH(s, &conf->ntp_sensors, entry)
		if (!strcmp(s->device, sensor->device))
			return;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal("sensor_add calloc");

	s->next = time(NULL);
	if ((s->device = strdup(sensor->device)) == NULL)
		fatal("sensor_add strdup");
	s->sensorid = sensor->num;

	TAILQ_INSERT_TAIL(&conf->ntp_sensors, s, entry);

	log_debug("sensor %s added", s->device);
}

void
sensor_remove(struct ntpd_conf *conf, struct ntp_sensor *s)
{
	TAILQ_REMOVE(&conf->ntp_sensors, s, entry);
	free(s->device);
	free(s);
}

int
sensor_query(struct ntp_sensor *s)
{
	struct sensor	 sensor;
	int		 mib[3];
	size_t		 len;

	s->next = time(NULL) + SENSOR_QUERY_INTERVAL;
	if (s->update.rcvd < time(NULL) - SENSOR_DATA_MAXAGE)
		s->update.good = 0;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	mib[2] = s->sensorid;
	len = sizeof(sensor);
	if (sysctl(mib, 3, &sensor, &len, NULL, 0) == -1) {
		log_warn("sensor_query sysctl");
		return (0);
	}

	if (sensor.flags & SENSOR_FINVALID ||
	    sensor.status != SENSOR_S_OK)
		return (0);

	if (sensor.type != SENSOR_TIMEDELTA ||
	    strcmp(sensor.device, s->device))
		return (-1);	/* causes sensor removal */

	if (sensor.tv.tv_sec == s->update.rcvd)	/* already seen */
		return (0);

	s->update.offset = 0 - (float)sensor.value / 1000000000.0;
	s->update.status.stratum = 0;	/* increased when sent out */
	s->update.status.rootdelay = 0;
	s->update.status.rootdispersion = 0;
	s->update.status.reftime = sensor.tv.tv_sec;

	/* XXX 4 char, 'DCF ', 'GPS ', 'PPS ' and the like */
	s->update.status.refid = 0;
	s->update.rcvd = sensor.tv.tv_sec;
	s->update.good = 1;

	log_debug("sensor %s: offset %f", s->device, s->update.offset);

	return (0);
}

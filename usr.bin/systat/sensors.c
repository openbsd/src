/*	$OpenBSD: sensors.c,v 1.21 2010/04/20 20:49:35 deraadt Exp $	*/

/*
 * Copyright (c) 2007 Deanna Phillips <deanna@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2006 Constantine A. Murenin <cnst+openbsd@bugmail.mojo.ru>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "systat.h"

struct sensor sensor;
struct sensordev sensordev;

struct sensinfo {
	int sn_dev;
	struct sensor sn_sensor;
};
#define sn_type sn_sensor.type
#define sn_numt sn_sensor.numt
#define sn_desc sn_sensor.desc
#define sn_status sn_sensor.status
#define sn_value sn_sensor.value

#define SYSTAT_MAXSENSORDEVICES 1024
char *devnames[SYSTAT_MAXSENSORDEVICES];

#define ADD_ALLOC 100
static size_t sensor_cnt = 0;
static size_t num_alloc = 0;
static struct sensinfo *sensors = NULL;

static char *fmttime(double);
static void showsensor(struct sensinfo *s);

void print_sn(void);
int read_sn(void);
int select_sn(void);

const char *drvstat[] = {
	NULL,
	"empty", "ready", "powering up", "online", "idle", "active",
	"rebuilding", "powering down", "failed", "degraded"
};


field_def fields_sn[] = {
	{"SENSOR", 16, 32, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0},
	{"VALUE", 16, 20, 1, FLD_ALIGN_RIGHT, -1, 0, 0, 0},
	{"STATUS", 5, 8, 1, FLD_ALIGN_CENTER, -1, 0, 0, 0},
	{"DESCRIPTION", 20, 45, 1, FLD_ALIGN_LEFT, -1, 0, 0, 0}
};

#define FIELD_ADDR(x) (&fields_sn[x])

#define FLD_SN_SENSOR	FIELD_ADDR(0)
#define FLD_SN_VALUE	FIELD_ADDR(1)
#define FLD_SN_STATUS	FIELD_ADDR(2)
#define FLD_SN_DESCR	FIELD_ADDR(3)

/* Define views */
field_def *view_sn_0[] = {
	FLD_SN_SENSOR, FLD_SN_VALUE, FLD_SN_STATUS, FLD_SN_DESCR, NULL
};


/* Define view managers */
struct view_manager sensors_mgr = {
	"Sensors", select_sn, read_sn, NULL, print_header,
	print_sn, keyboard_callback, NULL, NULL
};

field_view views_sn[] = {
	{view_sn_0, "sensors", '3', &sensors_mgr},
	{NULL, NULL, 0, NULL}
};

struct sensinfo *
next_sn(void)
{
	if (num_alloc <= sensor_cnt) {
		struct sensinfo *s;
		size_t a = num_alloc + ADD_ALLOC;
		if (a < num_alloc)
			return NULL;
		s = realloc(sensors, a * sizeof(struct sensinfo));
		if (s == NULL)
			return NULL;
		sensors = s;
		num_alloc = a;
	}

	return &sensors[sensor_cnt++];
}


int
select_sn(void)
{
	num_disp = sensor_cnt;
	return (0);
}

int
read_sn(void)
{
	enum sensor_type type;
	size_t		 slen, sdlen;
	int		 mib[5], dev, numt;
	struct sensinfo	*s;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	sensor_cnt = 0;

	for (dev = 0; dev < SYSTAT_MAXSENSORDEVICES; dev++) {
		mib[2] = dev;
		sdlen = sizeof(struct sensordev);
		if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
			if (errno == ENOENT)
				break;
			if (errno == ENXIO)
				continue;
			error("sysctl: %s", strerror(errno));
		}

		if (devnames[dev] && strcmp(devnames[dev], sensordev.xname)) {
			free(devnames[dev]);
			devnames[dev] = NULL;
		}
		if (devnames[dev] == NULL)
			devnames[dev] = strdup(sensordev.xname);

		for (type = 0; type < SENSOR_MAX_TYPES; type++) {
			mib[3] = type;
			for (numt = 0; numt < sensordev.maxnumt[type]; numt++) {
				mib[4] = numt;
				slen = sizeof(struct sensor);
				if (sysctl(mib, 5, &sensor, &slen, NULL, 0)
				    == -1) {
					if (errno != ENOENT)
						error("sysctl: %s", strerror(errno));
					continue;
				}
				if (sensor.flags & SENSOR_FINVALID)
					continue;

				s = next_sn();
				s->sn_sensor = sensor;
				s->sn_dev = dev;
			}
		}
	}

	num_disp = sensor_cnt;
	return 0;
}


void
print_sn(void)
{
	int n, count = 0;

	for (n = dispstart; n < num_disp; n++) {
		showsensor(sensors + n);
		count++;
		if (maxprint > 0 && count >= maxprint)
			break;
	}
}

int
initsensors(void)
{
	field_view *v;

	memset(devnames, 0, sizeof(devnames));

	for (v = views_sn; v->name != NULL; v++)
		add_view(v);

	return(1);
}

static void
showsensor(struct sensinfo *s)
{
	tb_start();
	tbprintf("%s.%s%d", devnames[s->sn_dev],
		 sensor_type_s[s->sn_type], s->sn_numt);
	print_fld_tb(FLD_SN_SENSOR);

	if (s->sn_desc[0] != '\0')
		print_fld_str(FLD_SN_DESCR, s->sn_desc);

	tb_start();

	switch (s->sn_type) {
	case SENSOR_TEMP:
		tbprintf("%10.2f degC",
		    (s->sn_value - 273150000) / 1000000.0);
		break;
	case SENSOR_FANRPM:
		tbprintf("%11lld RPM", s->sn_value);
		break;
	case SENSOR_VOLTS_DC:
		tbprintf("%10.2f V DC",
		    s->sn_value / 1000000.0);
		break;
	case SENSOR_WATTS:
		tbprintf("%10.2f W", s->sn_value / 1000000.0);
		break;
	case SENSOR_AMPS:
		tbprintf("%10.2f A", s->sn_value / 1000000.0);
		break;
	case SENSOR_INDICATOR:
		tbprintf("%15s", s->sn_value ? "On" : "Off");
		break;
	case SENSOR_INTEGER:
		tbprintf("%11lld raw", s->sn_value);
		break;
	case SENSOR_PERCENT:
		tbprintf("%14.2f%%", s->sn_value / 1000.0);
		break;
	case SENSOR_LUX:
		tbprintf("%15.2f lx", s->sn_value / 1000000.0);
		break;
	case SENSOR_DRIVE:
		if (0 < s->sn_value &&
		    s->sn_value < sizeof(drvstat)/sizeof(drvstat[0])) {
			tbprintf("%15s", drvstat[s->sn_value]);
			break;
		}
		break;
	case SENSOR_TIMEDELTA:
		tbprintf("%15s", fmttime(s->sn_value / 1000000000.0));
		break;
	case SENSOR_WATTHOUR:
		tbprintf("%12.2f Wh", s->sn_value / 1000000.0);
		break;
	case SENSOR_AMPHOUR:
		tbprintf("%10.2f Ah", s->sn_value / 1000000.0);
		break;
	case SENSOR_HUMIDITY:
		tbprintf("%3.2f%%", s->sn_value / 1000.0);
		break;
	case SENSOR_FREQ:
		tbprintf("%11lld Hz", s->sn_value);
		break;
	default:
		tbprintf("%10lld", s->sn_value);
		break;
	}

	print_fld_tb(FLD_SN_VALUE);

	switch (s->sn_status) {
	case SENSOR_S_UNSPEC:
		break;
	case SENSOR_S_UNKNOWN:
		print_fld_str(FLD_SN_STATUS, "unknown");
		break;
	case SENSOR_S_WARN:
		print_fld_str(FLD_SN_STATUS, "WARNING");
		break;
	case SENSOR_S_CRIT:
		print_fld_str(FLD_SN_STATUS, "CRITICAL");
		break;
	case SENSOR_S_OK:
		print_fld_str(FLD_SN_STATUS, "OK");
		break;
	}
	end_line();
}

#define SECS_PER_DAY 86400
#define SECS_PER_HOUR 3600
#define SECS_PER_MIN 60

static char *
fmttime(double in)
{
	int signbit = 1;
	int tiny = 0;
	char *unit;
#define LEN 32
	static char outbuf[LEN];

	if (in < 0){
		signbit = -1;
		in *= -1;
	}

	if (in >= SECS_PER_DAY ){
		unit = "days";
		in /= SECS_PER_DAY;
	} else if (in >= SECS_PER_HOUR ){
		unit = "hr";
		in /= SECS_PER_HOUR;
	} else if (in >= SECS_PER_MIN ){
		unit = "min";
		in /= SECS_PER_MIN;
	} else if (in >= 1 ){
		unit = "s";
		/* in *= 1; */ /* no op */
	} else if (in == 0 ){ /* direct comparisons to floats are scary */
		unit = "s";
	} else if (in >= 1e-3 ){
		unit = "ms";
		in *= 1e3;
	} else if (in >= 1e-6 ){
		unit = "us";
		in *= 1e6;
	} else if (in >= 1e-9 ){
		unit = "ns";
		in *= 1e9;
	} else {
		unit = "ps";
		if (in < 1e-13)
			tiny = 1;
		in *= 1e12;
	}

	snprintf(outbuf, LEN, 
	    tiny ? "%s%f %s" : "%s%.3f %s", 
	    signbit == -1 ? "-" : "", in, unit);

	return outbuf;
}

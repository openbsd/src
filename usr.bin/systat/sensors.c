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
#include "extern.h"

struct sensor sensor;
struct sensordev sensordev;
int row, sensor_cnt;
void printline(void);

WINDOW *
opensensors(void)
{
	return (subwin(stdscr, LINES-1-1, 0, 1, 0));
}

void
closesensors(WINDOW *w)
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelsensors(void)
{
	wmove(wnd, 0, 0);
	wclrtobot(wnd);
	mvwaddstr(wnd, 1, 0, "Sensor");
	mvwaddstr(wnd, 1, 34, "Value");
	mvwaddstr(wnd, 1, 45, "Status");
	mvwaddstr(wnd, 1, 58, "Description");
}

void
fetchsensors(void)
{
	enum sensor_type type;
	size_t		 slen, sdlen;
	int		 mib[5], dev, numt;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	slen = sizeof(struct sensor);
	sdlen = sizeof(struct sensordev);

	row = 2;
	sensor_cnt = 0;
	for (dev = 0; dev < MAXSENSORDEVICES; dev++) {
		mib[2] = dev;
		if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
			if (errno != ENOENT)
				warn("sysctl");
			continue;
		}
		for (type = 0; type < SENSOR_MAX_TYPES; type++) {
			mib[3] = type;
			for (numt = 0; numt < sensordev.maxnumt[type]; numt++) {
				mib[4] = numt;
				if (sysctl(mib, 5, &sensor, &slen, NULL, 0)
				    == -1) {
					if (errno != ENOENT)
						warn("sysctl");
					continue;
				}
				if (sensor.flags & SENSOR_FINVALID)
					continue;
				sensor_cnt++;
				if (sensor_cnt > 0)
					printline();
			}
		}
	}
}

const char *drvstat[] = {
	NULL,
	"empty", "ready", "powerup", "online", "idle", "active",
	"rebuild", "powerdown", "fail", "pfail"
};

void
showsensors(void)
{
	row = 2;
	if (sensor_cnt == 0)
		mvwaddstr(wnd, row, 0, "No sensors found.");
}

int
initsensors(void)
{
	fetchsensors();
	return (1);
}

void
printline(void)
{
	wmove(wnd, row, 0);
	wclrtoeol(wnd);
	mvwprintw(wnd, row, 0, "%s.%s%d", sensordev.xname,
	    sensor_type_s[sensor.type], sensor.numt);
	switch (sensor.type) {
	case SENSOR_TEMP:
		mvwprintw(wnd, row, 24, "%10.2f degC",
		    (sensor.value - 273150000) / 1000000.0);
		break;
	case SENSOR_FANRPM:
		mvwprintw(wnd, row, 24, "%11lld RPM", sensor.value);
		break;
	case SENSOR_VOLTS_DC:
		mvwprintw(wnd, row, 24, "%10.2f V DC",
		    sensor.value / 1000000.0);
		break;
	case SENSOR_AMPS:
		mvwprintw(wnd, row, 24, "%10.2f A", sensor.value / 1000000.0);
		break;
	case SENSOR_INDICATOR:
		mvwprintw(wnd, row, 24, "%15s", sensor.value? "On" : "Off");
		break;
	case SENSOR_INTEGER:
		mvwprintw(wnd, row, 24, "%11lld raw", sensor.value);
		break;
	case SENSOR_PERCENT:
		mvwprintw(wnd, row, 24, "%14.2f%%", sensor.value / 1000.0);
		break;
	case SENSOR_LUX:
		mvwprintw(wnd, row, 24, "%15.2f lx", sensor.value / 1000000.0);
		break;
	case SENSOR_DRIVE:
		if (0 < sensor.value &&
		    sensor.value < sizeof(drvstat)/sizeof(drvstat[0])) {
			mvwprintw(wnd, row, 24, "%15s", drvstat[sensor.value]);
			break;
		}
		break;
	case SENSOR_TIMEDELTA:
		mvwprintw(wnd, row, 24, "%10.6f secs",
		    sensor.value / 1000000000.0);
		break;
	case SENSOR_WATTHOUR:
		mvwprintw(wnd, row, 24, "%12.2f Wh", sensor.value / 1000000.0);
		break;
	case SENSOR_AMPHOUR:
		mvwprintw(wnd, row, 24, "%10.2f Ah", sensor.value / 1000000.0);
		break;
	default:
		mvwprintw(wnd, row, 24, "%10lld", sensor.value);
		break;
	}
	if (strlen(sensor.desc) >= 1)
		mvwprintw(wnd, row, 58, "(%s)", sensor.desc);

	switch (sensor.status) {
	case SENSOR_S_UNKNOWN:
		mvwaddstr(wnd, row, 45, "unknown");
		break;
	case SENSOR_S_WARN:
		mvwaddstr(wnd, row, 45, "WARNING");
		break;
	case SENSOR_S_CRIT:
		mvwaddstr(wnd, row, 45, "CRITICAL");
		break;
	case SENSOR_S_OK:
		mvwaddstr(wnd, row, 45, "OK");
		break;
	}
	row++;
}

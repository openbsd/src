/*	$OpenBSD: sensorsd.c,v 1.10 2004/04/11 20:02:00 otto Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define	RFBUFSIZ	28	/* buffer size for print_sensor */
#define	RFBUFCNT	4	/* ring buffers */
#define REPORT_PERIOD	60	/* report every n seconds */
#define CHECK_PERIOD	60	/* check every n seconds */

int		 main(int, char *[]);
void		 check_sensors(void);
void		 report(time_t);
static char	*print_sensor(enum sensor_type, u_int64_t);
int		 parse_config(char *);
int64_t		 get_val(char *, int, enum sensor_type);
void		 reparse_cfg(int);

enum sensor_status {
	STATUS_OK,
	STATUS_FAIL
};

struct limits_t {
	TAILQ_ENTRY(limits_t)	entries;
	u_int8_t		watch;
	int			num;			/* sensor number */
	enum sensor_type	type;			/* sensor type */
	int64_t			lower;			/* lower limit */
	int64_t			upper;			/* upper limit */
	enum sensor_status	status;			/* last status */
	time_t			status_changed;
	int64_t			last_val;
};

TAILQ_HEAD(limits, limits_t) limits = TAILQ_HEAD_INITIALIZER(limits);

char			 *configfile;
volatile sig_atomic_t	  reload = 0;

int
main(int argc, char *argv[])
{
	struct sensor	 sensor;
	struct limits_t	*limit;
	size_t		 len;
	time_t		 next_report, last_report = 0;
	time_t		 next_check;
	int		 mib[3];
	int		 i, sleeptime, watch_cnt;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	len = sizeof(sensor);

	for (i = 0; i < 256; i++) {
		mib[2] = i;
		if (sysctl(mib, 3, &sensor, &len, NULL, 0) == -1) {
			if (errno == ENXIO)
				break;
			else
				err(1, "sysctl");
		}
		if ((limit = calloc(1, sizeof(struct limits_t))) == NULL)
			err(1, "calloc");
		limit->num = i;
		limit->type = sensor.type;
		TAILQ_INSERT_TAIL(&limits, limit, entries);
	}

	if (i == 0)
		errx(1, "no sensors found");

	openlog("sensorsd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if (configfile == NULL)
		if (asprintf(&configfile, "/etc/sensorsd.conf") == -1)
			err(1, "out of memory");
	if ((watch_cnt = parse_config(configfile)) == -1)
		errx(1, "error in config file");

	if (watch_cnt == 0)
		errx(1, "no watches defined");

	if (daemon(0, 0) == -1)
		err(1, "unable to fork");

	signal(SIGHUP, reparse_cfg);

	syslog(LOG_INFO, "startup, %d watches for %d sensors", watch_cnt, i);

	next_check = next_report = time(NULL);

	for (;;) {
		if (reload) {
			if ((watch_cnt = parse_config(configfile)) == -1)
				syslog(LOG_CRIT, "error in config file %s",
				    configfile);
			else
				syslog(LOG_INFO,
				    "configuration reloaded, %d watches",
				    watch_cnt);
			reload = 0;
		}
		if (next_check <= time(NULL)) {
			check_sensors();
			next_check = time(NULL) + CHECK_PERIOD;
		}
		if (next_report <= time(NULL)) {
			report(last_report);
			last_report = next_report;
			next_report = time(NULL) + REPORT_PERIOD;
		}
		if (next_report < next_check)
			sleeptime = next_report - time(NULL);
		else
			sleeptime = next_check - time(NULL);
		if (sleeptime > 0)
			sleep(sleeptime);
	}
}

void
check_sensors(void)
{
	struct sensor	 sensor;
	struct limits_t	*limit;
	size_t		 len;
	int		 mib[3];
	int		 newstatus;

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	len = sizeof(sensor);

	TAILQ_FOREACH(limit, &limits, entries)
		if (limit->watch) {
			mib[2] = limit->num;
			if (sysctl(mib, 3, &sensor, &len, NULL, 0) == -1)
				err(1, "sysctl");

			limit->last_val = sensor.value;
			if (sensor.value > limit->upper ||
			    sensor.value < limit->lower)
				newstatus = STATUS_FAIL;
			else
				newstatus = STATUS_OK;

			if (limit->status != newstatus) {
				limit->status = newstatus;
				limit->status_changed = time(NULL);
			}
		}
}

void
report(time_t last_report)
{
	struct limits_t	*limit = NULL;

	TAILQ_FOREACH(limit, &limits, entries)
		if (limit->status_changed > last_report) {
			if (limit->status == STATUS_FAIL)
				syslog(LOG_ALERT,
				    "failure for hw.sensors.%d: "
				    "%s not within limits",
				    limit->num,
				    print_sensor(limit->type, limit->last_val));
			else
				syslog(LOG_ALERT,
				    "hw.sensors.%d within limits again, "
				    "current value %s",
				    limit->num,
				    print_sensor(limit->type, limit->last_val));
		}
}

static char *
print_sensor(enum sensor_type type, u_int64_t value)
{
	static char	 rfbuf[RFBUFCNT][RFBUFSIZ];	/* ring buffer */
	static int	 idx;
	char		*fbuf;

	fbuf = rfbuf[idx++];
	if (idx == RFBUFCNT)
		idx = 0;

	switch (type) {
	case SENSOR_TEMP:
		snprintf(fbuf, RFBUFSIZ, "%.2fC/%.2fF",
		    (value - 273150000) / 1000000.0,
		    (value - 273150000) / 1000000.0 * 9 / 5 + 32);
		break;
	case SENSOR_FANRPM:
		snprintf(fbuf, RFBUFSIZ, "%lld RPM", value);
		break;
	case SENSOR_VOLTS_DC:
		snprintf(fbuf, RFBUFSIZ, "%.2fV", value / 1000.0 / 1000.0);
		break;
	default:
		snprintf(fbuf, RFBUFSIZ, "%lld ???", value);
	}

	return (fbuf);
}

int
parse_config(char *cf)
{
	struct limits_t	 *p, *next;
	char		 *buf = NULL, *ebuf = NULL;
	char		  node[24];
	char		**cfa;
	int		  watch_cnt = 0;

	if ((cfa = calloc(2, sizeof(char *))) == NULL)
		err(1, "calloc");
	cfa[0] = cf;
	cfa[1] = NULL;

	for (p = TAILQ_FIRST(&limits); p != NULL; p = next) {
		next = TAILQ_NEXT(p, entries);
		snprintf(node, sizeof(node), "hw.sensors.%d", p->num);
		if (cgetent(&buf, cfa, node) != 0)
			p->watch = 0;
		else {
			p->watch = 1;
			watch_cnt++;
			if (cgetstr(buf, "low", &ebuf) < 0)
				ebuf = NULL;
			p->lower = get_val(ebuf, 0, p->type);
			if (cgetstr(buf, "high", &ebuf) < 0)
				ebuf = NULL;
			p->upper = get_val(ebuf, 1, p->type);
			free(buf);
			buf = NULL;
		}
	}
	free(cfa);
	return (watch_cnt);
}

int64_t
get_val(char *buf, int upper, enum sensor_type type)
{
	double	 val;
	int64_t	 rval = 0;
	char	*p;

	if (buf == NULL) {
		if (upper)
			return (LLONG_MAX);
		else
			return (LLONG_MIN);
	}

	val = strtod(buf, &p);
	if (buf == p)
		err(1, "incorrect value: %s", buf);

	switch(type) {
	case SENSOR_TEMP:
		switch(*p) {
		case 'C':
			printf ("C");
			rval = (val + 273.16) * 1000 * 1000;
			break;
		case 'F':
			printf("F");
			rval = ((val - 32.0) / 9 * 5 + 273.16) * 1000 * 1000;
			break;
		default:
			errx(1, "unknown unit %s for temp sensor", p);
		}
		break;
	case SENSOR_FANRPM:
		rval = val;
		break;
	case SENSOR_VOLTS_DC:
		if (*p != 'V')
			errx(1, "unknown unit %s for voltage sensor", p);
		rval = val * 1000 * 1000;
		break;
	default:
		errx(1, "unsupported sensor type");
		/* not reached */
	}
	free(buf);
	return (rval);
}

void
reparse_cfg(int signum)
{
	reload = 1;
}

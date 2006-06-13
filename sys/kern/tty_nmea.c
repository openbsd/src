/*	$OpenBSD: tty_nmea.c,v 1.7 2006/06/13 07:01:59 mbalmer Exp $ */

/*
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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

/* line discipline to decode NMEA 0183 data */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/sensors.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef NMEA_DEBUG
#define DPRINTFN(n, x)	do { if (nmeadebug > (n)) printf x; } while (0)
int nmeadebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x)	DPRINTFN(0, x)

/* Traditional POSIX base year */
#define	POSIX_BASE_YEAR	1970

static inline int leapyear(int year);
#define FEBRUARY	2
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

static const int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int	nmeaopen(dev_t, struct tty *);
int	nmeaclose(struct tty *, int);
int	nmeainput(int, struct tty *);
void	nmeaattach(int);

#define NMEAMAX	82
#define MAXFLDS	16

int nmea_count = 0;

struct nmea {
	char		cbuf[NMEAMAX];
	struct sensor	time;
	struct timespec	ts;
	int64_t		last;			/* last time rcvd */
	int		sync;
	int		pos;
};

/* NMEA decoding */
void	nmea_scan(struct nmea *);
void	nmea_gprmc(struct nmea *, char *fld[], int fldcnt);

/* date and time conversion */
int	nmea_date_to_nano(char *s, int64_t *nano);
int	nmea_time_to_nano(char *s, int64_t *nano);
static inline int leapyear(int year);
int	nmea_ymd_to_secs(int year, int month, int day, time_t *secs);

void
nmeaattach(int dummy)
{
}

int
nmeaopen(dev_t dev, struct tty *tp)
{
	struct proc *p = curproc;	/* XXX */
	struct nmea *np;
	int error;

	if (tp->t_line == NMEADISC)
		return (ENODEV);
	if ((error = suser(p, 0)) != 0)
		return (error);
	np = malloc(sizeof(struct nmea), M_WAITOK, M_DEVBUF);
	bzero(np, sizeof(*np));
	snprintf(np->time.device, sizeof(np->time.device), "nmea%d",
	    nmea_count++);
	np->time.status = SENSOR_S_UNKNOWN;
	np->time.type = SENSOR_TIMEDELTA;
	np->sync = 1;
	np->time.flags = SENSOR_FINVALID;
	sensor_add(&np->time);
	tp->t_sc = (caddr_t)np;
	
	error = linesw[TTYDISC].l_open(dev, tp);
	if (error) {
		free(np, M_DEVBUF);
		tp->t_sc = NULL;
	}
	return (error);
}

int
nmeaclose(struct tty *tp, int flags)
{
	struct nmea *np = (struct nmea *)tp->t_sc;

	tp->t_line = TTYDISC;	/* switch back to termios */
	sensor_del(&np->time);
	free(np, M_DEVBUF);
	tp->t_sc = NULL;
	nmea_count--;
	return linesw[TTYDISC].l_close(tp, flags);
}

/* collect NMEA sentence from tty */
int
nmeainput(int c, struct tty *tp)
{
	struct nmea *np = (struct nmea *)tp->t_sc;

	switch (c) {
	case '$':
		/* timestamp and delta refs now */
		nanotime(&np->ts);
		np->pos = 0;
		np->sync = 0;
		break;
	case '\r':
	case '\n':
		if (!np->sync) {
			np->cbuf[np->pos] = '\0';
			nmea_scan(np);
			np->sync = 1;
		}
		break;
	default:
		if (!np->sync && np->pos < (NMEAMAX - 1))
			np->cbuf[np->pos++] = c;
		break;
	}

	/* pass data to termios */
	return linesw[TTYDISC].l_rint(c, tp);
}

/* Scan the NMEA sentence just received */
void
nmea_scan(struct nmea *np)
{
	char *fld[MAXFLDS];
	char *cs;
	int fldcnt;
	int cksum, msgcksum;
	int n;

	fldcnt = 0;
	cksum = 0;

	/* split into fields and calc checksum */
	fld[fldcnt++] = &np->cbuf[0];	/* message type */
	for (cs = NULL, n = 0; n < np->pos && cs == NULL; n++) {
		switch (np->cbuf[n]) {
		case '*':
			np->cbuf[n] = '\0';
			cs = &np->cbuf[n + 1];
			break;
		case ',':
			if (fldcnt < MAXFLDS) {
				cksum ^= np->cbuf[n];
				np->cbuf[n] = '\0';
				fld[fldcnt++] = &np->cbuf[n + 1];
			} else {
				DPRINTF(("nr of fields in %s sentence exceeds "
				    "maximum of %d\n", fld[0], MAXFLDS));
				return;
			}
			break;
		default:
			cksum ^= np->cbuf[n];
		}
	}

	/* if we have a checksum, verify it */
	if (cs != NULL) {
		msgcksum = 0;
		while (*cs) {
			if ((*cs >= '0' && *cs <= '9') ||
			    (*cs >= 'A' && *cs <= 'F')) {
				if (msgcksum)
					msgcksum <<= 4;
				if (*cs >= '0' && *cs<= '9')
					msgcksum += *cs - '0';
				else if (*cs >= 'A' && *cs <= 'F')
					msgcksum += 10 + *cs - 'A';
				cs++;
			} else {
				DPRINTF(("bad char %c in checksum\n", *cs));
				return;
			}
		}
		if (msgcksum != cksum) {
			DPRINTF(("cksum mismatch"));
			return;
		}
	}

	/* check message type */
	if (!strcmp(fld[0], "GPRMC"))
		nmea_gprmc(np, fld, fldcnt);
}

/* Decode the minimum recommended nav info sentence (RMC) */
void
nmea_gprmc(struct nmea *np, char *fld[], int fldcnt)
{
	int64_t date_nano, time_nano, nmea_now;
#ifdef NMEA_DEBUG
	int n;

	for (n = 0; n < fldcnt; n++)
		DPRINTF(("%s ", fld[n]));
	DPRINTF(("\n"));
#endif

	if (fldcnt != 12 && fldcnt != 13) {
		DPRINTF(("gprmc: field count mismatch, %d\n", fldcnt));
		return;
	}
	if (nmea_time_to_nano(fld[1], &time_nano)) {
		DPRINTF(("gprmc: illegal time, %s\n", fld[1]));
		return;
	}
	if (nmea_date_to_nano(fld[9], &date_nano)) {
		DPRINTF(("gprmc: illegal date, %s\n", fld[9]));
		return;
	}
	nmea_now = date_nano + time_nano;
	if (nmea_now <= np->last) {
		DPRINTF(("gprmc: time not monotonically increasing\n"));
		return;
	}
	np->last = nmea_now;
	np->time.value = np->ts.tv_sec * 1000000000LL + np->ts.tv_nsec -
	    nmea_now;
	np->time.tv.tv_sec = np->ts.tv_sec;
	np->time.tv.tv_usec = np->ts.tv_nsec / 1000L;
	if (np->time.status == SENSOR_S_UNKNOWN) {
		strlcpy(np->time.desc, "GPS", sizeof(np->time.desc));
		if (fldcnt == 13) {
			switch (*fld[12]) {
			case 'S':
				strlcat(np->time.desc, " simulated",
				    sizeof(np->time.desc));
				break;
			case 'E':
				strlcat(np->time.desc, " estimated",
				    sizeof(np->time.desc));
				break;
			case 'A':
				strlcat(np->time.desc, " autonomous",
				    sizeof(np->time.desc));
				break;
			case 'D':
				strlcat(np->time.desc, " differential",
				    sizeof(np->time.desc));
				break;
			case 'N':
				strlcat(np->time.desc, " not valid",
				    sizeof(np->time.desc));
				break;
			}
		}
		np->time.status = SENSOR_S_OK;
		np->time.flags &= ~SENSOR_FINVALID;
	}
	switch (*fld[2]) {
	case 'A':
		np->time.status = SENSOR_S_OK;
		break;
	case 'V':
		np->time.status = SENSOR_S_WARN;
		break;
	default:
		DPRINTF(("gprmc: unknown warning indication\n"));
	}
}

/*
 * convert a NMEA 0183 formatted date string to seconds since the epoch
 * the string must be of the form DDMMYY
 * return (0) on success, (-1) if illegal characters are encountered
 */
int
nmea_date_to_nano(char *s, int64_t *nano)
{
	time_t secs;
	char *p;
	int year, month, day;
	int n;

	/* make sure the input contains only numbers and is six digits long */
	for (n = 0, p = s; n < 6 && *p && *p >= '0' && *p <= '9'; n++, p++)
		;
	if (n != 6 || (*p != '\0'))
		return (-1);

	year = 2000 + (s[4] - '0') * 10 + (s[5] - '0');
	month = (s[2] - '0') * 10 + (s[3] - '0');
	day = (s[0] - '0') * 10 + (s[1] - '0');

	if (nmea_ymd_to_secs(year, month, day, &secs))
		return (-1);
	*nano = secs * 1000000000LL;
	return (0);
}

/*
 * convert NMEA 0183 formatted time string to nanoseconds since midnight
 * the string must be of the form HHMMSS[.[sss]]
 * (e.g. 143724 or 143723.615)
 * return (0) on success, (-1) if illegal characters are encountered
 */
int
nmea_time_to_nano(char *s, int64_t *nano)
{
	long fac, div;
	long secs, frac;
	int n;
	char ul;

	fac = 36000L;
	div = 6L;
	secs = 0L;

	ul = '2';
	for (n = 0, secs = 0; fac && *s && *s >= '0' && *s <= ul; s++, n++) {
		secs += (*s - '0') * fac;
		div = 16 - div;
		fac /= div;
		switch (n) {
		case 0:
			if (*s <= '1')
				ul = '9';
			else
				ul = '3';
			break;
		case 1:
		case 3:
			ul = '5';
			break;
		case 2:
		case 4:
			ul = '9';
			break;
		}
	}
	if (fac)
		return (-1);

	div = 1L;
	frac = 0L;
	/* handle fractions of a second, max. 6 digits */
	if (*s == '.') {
		for (++s; div < 1000000 && *s && *s >= '0' && *s <= '9'; s++) {
			frac *= 10;
			frac += (*s - '0');
			div *= 10;
		}
	}

	if (*s != '\0')
		return (-1);

	*nano = secs * 1000000000LL + (int64_t)frac * (1000000000 / div);
	return (0);
}

/*
 * the leapyear() and nmea_ymd_to_secs() functions to calculate the number
 * of seconds since the epoch for a certain date are from sys/dev/clock_subr.c,
 * the following copyright applies to these functions:
 */
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static inline int
leapyear(int year)
{
	int rv = 0;

	if ((year & 3) == 0) {
		rv = 1;
		if ((year % 100) == 0) {
			rv = 0;
			if ((year % 400) == 0)
				rv = 1;
		}
	}
	return (rv);
}

/* convert year, month, day to seconds since the epoch */
int
nmea_ymd_to_secs(int year, int month, int day, time_t *secs)
{
	int i, days;
	int leap;

	if (month < 1 || month > 12)
		return (-1);

	days = days_in_month(month);
	leap = leapyear(year);
	if (month == FEBRUARY && leap)
		days++;
	if (day < 1 || day > days)
		return (-1);

	/*
	 * Compute days since start of time
	 * First from years, then from months.
	 */
	days = 0;
	for (i = POSIX_BASE_YEAR; i < year; i++)
		days += days_in_year(i);
	if (leap && month > FEBRUARY)
		days++;

	/* Months */
	for (i = 1; i < month; i++)
	  	days += days_in_month(i);
	days += (day - 1);

	/* convert to seconds. */
	*secs = days * 86400L;
	return (0);
}

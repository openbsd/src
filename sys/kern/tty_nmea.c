/*	$OpenBSD: tty_nmea.c,v 1.9 2006/06/20 14:06:21 deraadt Exp $ */

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
#include <sys/time.h>

#ifdef NMEA_DEBUG
#define DPRINTFN(n, x)	do { if (nmeadebug > (n)) printf x; } while (0)
int nmeadebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x)	DPRINTFN(0, x)

int	nmeaopen(dev_t, struct tty *);
int	nmeaclose(struct tty *, int);
int	nmeainput(int, struct tty *);
void	nmeaattach(int);

#define NMEAMAX	82
#define MAXFLDS	16

int nmea_count;

struct nmea {
	char		cbuf[NMEAMAX];
	struct sensor	time;
	struct timespec	ts;
	int64_t		last;			/* last time rcvd */
	int		sync;
	int		pos;
};

/* NMEA decoding */
void	nmea_scan(struct nmea *, struct tty *);
void	nmea_gprmc(struct nmea *, struct tty *, char *fld[], int fldcnt);

/* date and time conversion */
int	nmea_date_to_nano(char *s, int64_t *nano);
int	nmea_time_to_nano(char *s, int64_t *nano);

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
		/* capture the moment */
		nanotime(&np->ts);
		np->pos = 0;
		np->sync = 0;
		break;
	case '\r':
	case '\n':
		if (!np->sync) {
			np->cbuf[np->pos] = '\0';
			nmea_scan(np, tp);
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
nmea_scan(struct nmea *np, struct tty *tp)
{
	int fldcnt = 0, cksum = 0, msgcksum, n;
	char *fld[MAXFLDS], *cs;

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
		nmea_gprmc(np, tp, fld, fldcnt);
}

/* Decode the recommended minimum specific GPS/TRANSIT data */
void
nmea_gprmc(struct nmea *np, struct tty *tp, char *fld[], int fldcnt)
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

#ifdef NMEA_TSTAMP
	/*
	 * if tty timestamping on DCD or CTS is used, take the timestamp
	 * from the tty, else use the timestamp taken on the initial '$'
	 * character.
	 */
	if (tp->t_flags & (TS_TSTAMPDCDSET | TS_TSTAMPDCDCLR |
	    TS_TSTAMPCTSSET | TS_TSTAMPCTSCLR)) {
		np->time.value = tp->t_tv.tv_sec + 1000000000LL +
		    tp->t_tv.tv_usec * 1000LL - nmea_now;
		np->time.tv.tv_sec = tp->t_tv.tv_sec;
		np->time.tv.tv_usec = tp->t_tv.tv_usec;
	} else {
#endif
		np->time.value = np->ts.tv_sec * 1000000000LL +
		    np->ts.tv_nsec - nmea_now;
		np->time.tv.tv_sec = np->ts.tv_sec;
		np->time.tv.tv_usec = np->ts.tv_nsec / 1000L;
#ifdef NMEA_TSTAMP
	}
#endif
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
	struct clock_ymdhms ymd;
	time_t secs;
	char *p;
	int n;

	/* make sure the input contains only numbers and is six digits long */
	for (n = 0, p = s; n < 6 && *p && *p >= '0' && *p <= '9'; n++, p++)
		;
	if (n != 6 || (*p != '\0'))
		return (-1);

	ymd.dt_year = 2000 + (s[4] - '0') * 10 + (s[5] - '0');
	ymd.dt_mon = (s[2] - '0') * 10 + (s[3] - '0');
	ymd.dt_day = (s[0] - '0') * 10 + (s[1] - '0');
	ymd.dt_hour = ymd.dt_min = ymd.dt_sec = 0;

	secs = clock_ymdhms_to_secs(&ymd);
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
	long fac = 36000L, div = 6L, secs = 0L, frac;
	char ul = '2';
	int n;

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

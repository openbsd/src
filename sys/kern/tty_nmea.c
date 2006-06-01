/*	$OpenBSD: tty_nmea.c,v 1.3 2006/06/01 23:17:08 ckuethe Exp $ */

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

/* line discipline to decode NMEA0183 data */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/sensors.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <dev/clock_subr.h>	/* clock_subr not avail on all arches */

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
#define MAXFLDS	12

/* NMEA talker identifiers */

#define TI_UNK	0
#define TI_GPS	1
#define TI_LORC	2

/* NMEA message types */

#define MSG_RMC	5393731	/* recommended minimum sentence C */
#define MSG_GGA	4671297
#define MSG_GSA	4674369	/* satellites active */
#define MSG_GSV	4674390	/* satellites in view */
#define MSG_VTG	5657671	/* velocity, direction, speed */

int nmea_count = 0;

struct nmea {
	char		cbuf[NMEAMAX];
	struct sensor	time;
	struct timeval	tv;			/* soft timestamp */
	struct timespec	ts;
	time_t 		last;			/* last time rcvd */
	int		state;			/* state we're in */
	int		pos;
	int		fpos[MAXFLDS];
	int		flds;			/* expect nr of fields */
	int		fldcnt;			/* actual count of fields */
	int		cksum;			/* calculated checksum */
	int		msgcksum;		/* received cksum */
	int		ti;			/* talker identifier */
	int		msg;			/* NMEA msg type */
};

/* NMEA protocol state machine */

void	nmea_hdlr(struct nmea *, int c);

/* NMEA decoding */

void	nmea_decode(struct nmea *);
void	nmea_rmc(struct nmea *);

/* helper functions */

int	nmea_atoi(char *, int *);
void	nmea_bufadd(struct nmea *, int);

enum states {
	S_SYNC = 0,
	S_TI_1,
	S_TI_2,
	S_MSG,
	S_DATA,
	S_CKSUM
};

int
nmeaopen(dev_t dev, struct tty *tp)
{
	struct nmea *np;

	if (tp->t_line != NMEADISC) {
		np = malloc(sizeof(struct nmea), M_WAITOK, M_DEVBUF);
		tp->t_sc = (caddr_t)np;

		snprintf(np->time.device, sizeof(np->time.device), "nmea%d",
		    nmea_count++);
		np->time.status = SENSOR_S_UNKNOWN;
		np->time.type = SENSOR_TIMEDELTA;
		np->time.value = 0LL;
		np->time.rfact = 0;
		np->time.flags = 0;
		np->state = S_SYNC;
		np->last = 0L;
	}

	return linesw[0].l_open(dev, tp);
}

int
nmeaclose(struct tty *tp, int flags)
{
	struct nmea *np = (struct nmea *)tp->t_sc;

	tp->t_line = 0;	/* switch back to termios */
	if (np->time.status != SENSOR_S_UNKNOWN)
		sensor_del(&np->time);
	free(np, M_DEVBUF);
	nmea_count--;
	return linesw[0].l_close(tp, flags);
}

/* scan input from tty for NMEA telegrams */
int
nmeainput(int c, struct tty *tp)
{
	struct nmea *np = (struct nmea *)tp->t_sc;

	nmea_hdlr(np, c);

	/* pass data to termios */
	return linesw[0].l_rint(c, tp);
}

void
nmeaattach(int dummy)
{
}

/* NMEA state machine */
void
nmea_hdlr(struct nmea *np, int c)
{
	switch (np->state) {
	case S_SYNC:
		switch (c) {
		case '$':
			/* timestamp and delta refs now */
			microtime(&np->tv);
			nanotime(&np->ts);
			np->pos = 0;
			np->fldcnt = 0;
			np->flds = 0;
			np->cbuf[np->pos++] = c;
			np->cksum = 0;
			np->msgcksum = -1;
			np->ti = TI_UNK;
			np->state = S_TI_1;
		}
		break;
	case S_TI_1:
		nmea_bufadd(np, c);
		np->state = S_TI_2;
		switch (c) {
		case 'G':
			np->ti = TI_GPS;
			break;
		case 'L':
			np->ti = TI_LORC;
			break;
		default:
			np->state = S_SYNC;
		}
		break;
	case S_TI_2:
		nmea_bufadd(np, c);
		np->state = S_SYNC;
		switch (c) {
		case 'P':
			if (np->ti == TI_GPS)
				np->state = S_MSG;
			break;
		case 'C':
			if (np->ti == TI_LORC)
				np->state = S_MSG;
			break;
		}
		break;
	case S_MSG:
		nmea_bufadd(np, c);
		if (np->pos == 6) {
			np->msg = (np->cbuf[3] << 16) + (np->cbuf[4] << 8) +
			    np->cbuf[5];
			switch (np->msg) {
			case MSG_RMC:
				np->flds = 12;	/* or 11 */
				np->state = S_DATA;
				break;
			default:
				np->state = S_SYNC;
			}
		}
		break;
	case S_DATA:
		switch (c) {
		case '\n':
			np->cbuf[np->pos] = '\0';
			nmea_decode(np);
			np->state = S_SYNC;
			break;
		case '*':
			np->cbuf[np->pos++] = c;
			np->msgcksum = 0;
			np->state = S_CKSUM;
			break;
		case ',':
			np->fpos[np->fldcnt++] = np->pos + 1;
		default:
			if (np->pos < NMEAMAX)
				nmea_bufadd(np, c);
			else
				np->state = S_SYNC;
		}
		break;
	case S_CKSUM:
		switch (c) {
		case '\r':
		case '\n':
			np->cbuf[np->pos] = '\0';
			nmea_decode(np);
			np->state = S_SYNC;
			break;
		default:
			if (np->pos < NMEAMAX && ((c >= '0' && c<= '9') ||
			    (c >= 'A' && c <= 'F'))) {
				np->cbuf[np->pos++] = c;
				if (np->msgcksum)
					np->msgcksum <<= 4;
				if (c >= '0' && c<= '9')
					np->msgcksum += c - '0';
				else if (c >= 'A' && c <= 'F')
					np->msgcksum += 10 + c - 'A';
			} else
				np->state = S_SYNC;
		}
		break;
	}
}

/* add a character to the buffer and update the checksum */
void
nmea_bufadd(struct nmea *np, int c)
{
	if (np->pos < NMEAMAX){
		np->cbuf[np->pos++] = c;
		np->cksum ^= c;
	}
}

/*
 * convert a string to a number, stop at the first non-numerical
 * character or the end of the string.  any non-numerical character
 * following the number other than ',' is considered an error.
 */
int
nmea_atoi(char *s, int *num)
{
	int n;

	for (n = 0; *s && *s >= '0' && *s <= '9'; s++) {
		if (n)
			n *= 10;
		n += *s - '0';
	}

	if (*s && *s != ',')	/* no numeric character */
		return (-1);

	*num = n;
	return (0);
}

void
nmea_decode(struct nmea *np)
{
	switch (np->msg) {
	case MSG_RMC:
		nmea_rmc(np);
		break;
	}
}

/* Decode the minimum recommended nav info sentence (RMC) */
void
nmea_rmc(struct nmea *np)
{
	struct clock_ymdhms ymdhms;
	time_t nmea_now;
	int n;

	if (np->fldcnt != 11 && np->fldcnt != 12) {
		DPRINTF(("field count mismatch\n"));
		return;
	}
	if (np->msgcksum >= 0 && np->cksum != np->msgcksum) {
		DPRINTF(("checksum error"));
		return;
	}
	np->cbuf[13] = '\0';
	if (nmea_atoi(&np->cbuf[11], &n)) {
		DPRINTF(("error in sec\n"));
		return;
	}
	ymdhms.dt_sec = n;
	np->cbuf[11] = 0;
	if (nmea_atoi(&np->cbuf[9], &n)) {
		DPRINTF(("error in min\n"));
		return;
	}
	ymdhms.dt_min = n;
	np->cbuf[9] = 0;
	if (nmea_atoi(&np->cbuf[7], &n)) {
		DPRINTF(("error in hour\n"));
		return;
	}
	ymdhms.dt_hour = n;
	if (nmea_atoi(&np->cbuf[np->fpos[8] + 4], &n)) {
		DPRINTF(("error in year\n"));
		return;
	}
	ymdhms.dt_year = 2000 + n;
	np->cbuf[np->fpos[8] + 4] = '\0';
	if (nmea_atoi(&np->cbuf[np->fpos[8] + 2], &n)) {
		DPRINTF(("error in month\n"));
		return;
	}
	ymdhms.dt_mon = n;
	np->cbuf[np->fpos[8] + 2] = '\0';
	if (nmea_atoi(&np->cbuf[np->fpos[8]], &n)) {
		DPRINTF(("error in day\n"));
		return;
	}
	ymdhms.dt_day = n;
	nmea_now = clock_ymdhms_to_secs(&ymdhms);
	if (nmea_now <= np->last) {
		DPRINTF(("time not monotonically increasing\n"));
		return;
	}
	np->last = nmea_now;
	np->time.value = (np->ts.tv_sec - nmea_now)
	    * 1000000000 + np->ts.tv_nsec;
	np->time.tv.tv_sec = np->tv.tv_sec;
	np->time.tv.tv_usec = np->tv.tv_usec;
	if (np->time.status == SENSOR_S_UNKNOWN) {
		strlcpy(np->time.desc, np->ti == TI_GPS ? "GPS  GPS" :
		    "LORC Loran-C", sizeof(np->time.desc));
		if (np->fldcnt == 12) {
			switch (np->cbuf[np->fpos[11]]) {
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
		sensor_add(&np->time);
	}
	switch (np->cbuf[np->fpos[1]]) {
	case 'A':
		np->time.status = SENSOR_S_OK;
		break;
	case 'V':
		np->time.status = SENSOR_S_WARN;
		break;
	default:
		DPRINTF(("unknown warning indication\n"));
	}
}

/*	$OpenBSD: todclock.c,v 1.3 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: todclock.c,v 1.4 2002/10/02 05:02:30 thorpej Exp $	*/

/*
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * clock.c
 *
 * Timer related machine specific code
 *
 * Created      : 29/09/94
 */

/* Include header files */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>

#include <machine/rtc.h>
#include <arm/footbridge/todclockvar.h>

#include "todclock.h"

#if NTODCLOCK > 1
#error "Can only had 1 todclock device"
#endif

static int yeartoday (int);
 
/*
 * softc structure for the todclock device
 */

struct todclock_softc {
	struct device	sc_dev;			/* device node */
	void	*sc_rtc_arg;			/* arg to read/write */
	int	(*sc_rtc_write)	(void *, rtc_t *);	/* rtc write function */
	int	(*sc_rtc_read)	(void *, rtc_t *);	/* rtc read function */
};

/* prototypes for functions */

static void todclockattach (struct device *parent, struct device *self,
				void *aux);
static int  todclockmatch  (struct device *parent, void *cf, void *aux);

/*
 * We need to remember our softc for functions like inittodr()
 * and resettodr()
 * since we only ever have one time-of-day device we can just store
 * the direct pointer to softc.
 */

static struct todclock_softc *todclock_sc = NULL;

/* driver and attach structures */

struct cfattach todclock_ca = {
	sizeof(struct todclock_softc), todclockmatch, todclockattach
};

struct cfdriver todclock_cd = {
	NULL, "todclock", DV_DULL 
};


/*
 * int todclockmatch(struct device *parent, struct cfdata *cf, void *aux)
 *
 * todclock device probe function.
 * just validate the attach args
 */

int
todclockmatch(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct todclock_attach_args *ta = aux;

	if (todclock_sc != NULL)
		return(0);
	if (strcmp(ta->ta_name, "todclock") != 0)
		return(0);

	if (ta->ta_flags & TODCLOCK_FLAG_FAKE)
		return(1);
	return(2);
}

/*
 * void todclockattach(struct device *parent, struct device *self, void *aux)
 *
 * todclock device attach function.
 * Initialise the softc structure and do a search for children
 */

void
todclockattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct todclock_softc *sc = (void *)self;
	struct todclock_attach_args *ta = aux;

	/* set up our softc */
	todclock_sc = sc;
	todclock_sc->sc_rtc_arg = ta->ta_rtc_arg;
	todclock_sc->sc_rtc_write = ta->ta_rtc_write;
	todclock_sc->sc_rtc_read = ta->ta_rtc_read;

	printf("\n");

	/*
	 * Initialise the time of day register.
	 * This is normally left to the filing system to do but not all
	 * filing systems call it e.g. cd9660
	 */

	inittodr(0);
}

static __inline int
yeartoday(year)
	int year;
{
	return((year % 4) ? 365 : 366);
}

                 
static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int timeset = 0;

#define SECPERDAY	(24*60*60)
#define SECPERNYEAR	(365*SECPERDAY)
#define SECPER4YEARS	(4*SECPERNYEAR+SECPERDAY)
#define EPOCHYEAR	1970

/*
 * Globally visable functions
 *
 * These functions are used from other parts of the kernel.
 * These functions use the functions defined in the tod_sc
 * to actually read and write the rtc.
 *
 * The first todclock to be attached will be used for handling
 * the time of day.
 */

/*
 * Write back the time of day to the rtc
 */

void
resettodr()
{
	int s;
	time_t year, mon, day, hour, min, sec;
	rtc_t rtc;

	/* Have we set the system time in inittodr() */
	if (!timeset)
		return;

	/* We need a todclock device and should always have one */
	if (!todclock_sc)
		return;

	/* Abort early if there is not actually an RTC write routine */
	if (todclock_sc->sc_rtc_write == NULL)
		return;

	sec = time.tv_sec;
	sec -= tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		time.tv_sec += 3600;
	year = (sec / SECPER4YEARS) * 4;
	sec %= SECPER4YEARS;

	/* year now hold the number of years rounded down 4 */

	while (sec > (yeartoday(EPOCHYEAR+year) * SECPERDAY)) {
		sec -= yeartoday(EPOCHYEAR+year)*SECPERDAY;
		year++;
	}

	/* year is now a correct offset from the EPOCHYEAR */

	year+=EPOCHYEAR;
	mon=0;
	if (yeartoday(year) == 366)
		month[1]=29;
	else
		month[1]=28;
	while (sec >= month[mon]*SECPERDAY) {
		sec -= month[mon]*SECPERDAY;
		mon++;
	}

	day = sec / SECPERDAY;
	sec %= SECPERDAY;
	hour = sec / 3600;
	sec %= 3600;
	min = sec / 60;
	sec %= 60;
	rtc.rtc_cen = year / 100;
	rtc.rtc_year = year % 100;
	rtc.rtc_mon = mon+1;
	rtc.rtc_day = day+1;
	rtc.rtc_hour = hour;
	rtc.rtc_min = min;
	rtc.rtc_sec = sec;
	rtc.rtc_centi =
	rtc.rtc_micro = 0;

	printf("resettod: %02d/%02d/%02d%02d %02d:%02d:%02d\n", rtc.rtc_day,
	    rtc.rtc_mon, rtc.rtc_cen, rtc.rtc_year, rtc.rtc_hour,
	    rtc.rtc_min, rtc.rtc_sec);

	s = splclock();
	todclock_sc->sc_rtc_write(todclock_sc->sc_rtc_arg, &rtc);
	(void)splx(s);
}

/*
 * Initialise the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */

void
inittodr(base)
	time_t base;
{
	time_t n;
	int i, days = 0;
	int s;
	int year;
	rtc_t rtc;

	/*
	 * Default to the suggested time but replace that we one from an
	 * RTC is we can.
	 */

	/* We expect a todclock device */

	/* Use the suggested time as a fall back */
	time.tv_sec = base;
	time.tv_usec = 0;

	/* Can we read an RTC ? */
	if (todclock_sc->sc_rtc_read) {
		s = splclock();
		if (todclock_sc->sc_rtc_read(todclock_sc->sc_rtc_arg, &rtc) == 0) {
			(void)splx(s);
			return;
		}
		(void)splx(s);
	} else
		return;
			
	/* Convert the rtc time into seconds */

	n = rtc.rtc_sec + 60 * rtc.rtc_min + 3600 * rtc.rtc_hour;
	n += (rtc.rtc_day - 1) * 3600 * 24;
	year = (rtc.rtc_year + rtc.rtc_cen * 100) - 1900;

	if (yeartoday(year) == 366)
		month[1] = 29;
	for (i = rtc.rtc_mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;

	for (i = 70; i < year; i++)
		days += yeartoday(i);

	n += days * 3600 * 24;

	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		time.tv_sec -= 3600;

	time.tv_sec = n;
	time.tv_usec = 0;

	/* timeset is used to ensure the time is valid before a resettodr() */

	timeset = 1;

	/* If the base was 0 then keep quiet */

	if (base) {
		if (n > base + 60) {
			days = (n - base) / SECPERDAY;
			printf("Clock has gained %d day%c %ld hours %ld minutes %ld secs\n",
			    days, ((days == 1) ? 0 : 's'),
			    (long)((n - base) / 3600) % 24,
			    (long)((n - base) / 60)   % 60,
			    (long) (n - base)         % 60);
		}
	}
}  

/* End of todclock.c */

/*	$NetBSD: clock.c,v 1.11 1995/05/16 07:30:46 phil Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)clock.c	7.2 (Berkeley) 5/12/91
 *
 */

/*
 * Primitive clock interrupt routines.
 *
 * Improved by Phil Budne ... 10/17/94.
 * Pulled over code from i386/isa/clock.c (Matthias Pfaller 12/03/94).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>
#include <machine/kbus.h>
#include <machine/autoconf.h>
#include "clockreg.h"

/*
 * Clock driver
 */

/*
 * Zilog Z8530 Dual UART driver (clock interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for a clock.
 */
static int clock_started;

/* Clock state.  */
struct clock_softc {
	struct	device clock_dev;	/* required first: base device */
	struct	zs_chanstate *clock_cs;
};

/****************************************************************
 * Definition of the driver for autoconfig.
 ****************************************************************/

static int	clock_match __P((struct device *, void *, void *));
static void	clock_attach __P((struct device *, struct device *, void *));

struct cfattach clock_ca = {
	sizeof(struct clock_softc), clock_match, clock_attach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROM_BCD(x,y)      ((x) + 10 * (y))

static int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define yeartoday(year) (((year) % 4) ? 365 : 366)

/* Virtual address of RTC.  */
static u_char *rtc_byte;

/* Set when the tod was read.  */
static int timeset;

/* Needed by kern_clock.c  */
void
setstatclockrate(int dummy)
{
	printf ("setstatclockrate\n");
}

static u_char
rtc_read_reg (int reg)
{
  u_char res;

  reg &= 0x0f;
  *rtc_byte = RTC_WRITE_ADDR | reg;
  *rtc_byte = reg;
  *rtc_byte = RTC_READ | 0x0f;
  res = *rtc_byte & 0x0f;
  *rtc_byte = RTC_WRITE_ADDR | 0x0f;
  *rtc_byte = 0x0f;
  return res;
}

static u_char
rtc_write_reg (int reg, u_char val)
{
  u_char res = 0;

  reg &= 0x0f;
  val &= 0x0f;
  *rtc_byte = RTC_WRITE_ADDR | RTC_STOP | reg;
  *rtc_byte = reg | RTC_STOP ;
  *rtc_byte = RTC_STOP | RTC_WRITE | val;
  *rtc_byte = RTC_STOP | RTC_WRITE | val;
  *rtc_byte = RTC_WRITE | val;
  *rtc_byte = RTC_WRITE_ADDR | 0x0f;
  *rtc_byte = 0x0f;
  return res;
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr (base)
	time_t base;
{
        /*
         * We ignore the suggested time for now and go for the RTC
         * clock time stored in the CMOS RAM.
         */
	time_t n;
	int sec, min, hr, dom, mon, yr;
	int hrl, hrh;
	int i, days = 0;
	int s;

	if (!rtc_byte)
	  {
	    rtc_byte = bus_mapin (BUS_KBUS, RTC_ADDR, 1);
	    if (!rtc_byte)
	      panic ("Can't map RTC");
	  }

	timeset = 1;

	sec = FROM_BCD (rtc_read_reg (RTC_SEC_LOW),
			rtc_read_reg (RTC_SEC_HIGH));
	min = FROM_BCD (rtc_read_reg (RTC_MIN_LOW),
			rtc_read_reg (RTC_MIN_HIGH));
	dom = FROM_BCD (rtc_read_reg (RTC_DAY_LOW),
			rtc_read_reg (RTC_DAY_HIGH));
	mon = FROM_BCD (rtc_read_reg (RTC_MON_LOW),
			rtc_read_reg (RTC_MON_HIGH));
	yr = RTC_YEAR_BASE + FROM_BCD (rtc_read_reg (RTC_YEAR_LOW),
				       rtc_read_reg (RTC_YEAR_HIGH));
	hrl = rtc_read_reg (RTC_HOUR_LOW);
	hrh = rtc_read_reg (RTC_HOUR_HIGH);
	if (hrh & RTC_HOUR_PM)
	  hr = 12 + (hrh & 1) * 10 + hrl;
	else
	  hr = (hrh & 0x03) * 10 + hrl;

	yr   = (yr < 70) ? yr + 100 : yr;

	/*
	 * Check to see if it was really the rtc
	 * by checking for bad date info.
	 */
	if (sec > 59 || min > 59 || hr > 23 || dom > 31 || mon > 12) {
		printf("inittodr: No clock found\n");
		time.tv_sec = base;
		return;
	}

	n = sec + 60 * min + 3600 * hr;
	n += (dom - 1) * 3600 * 24;

	if (yeartoday(yr) == 366)
		month[1] = 29;
	for (i = mon - 2; i >= 0; i--)
		days += month[i];
	month[1] = 28;
	for (i = 70; i < yr; i++)
		days += yeartoday(i);
	n += days * 3600 * 24;

	n += tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		n -= 3600;
	s = splclock();
	time.tv_sec = n;
	time.tv_usec = 0;
	splx(s);
}

/*
 * Reset the clock.
 */
void
resettodr()
{
	time_t n;
	int diff, i, j;
	int s;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip in this case.
	 */
	if (!timeset)
		return;

	diff = tz.tz_minuteswest * 60;
	if (tz.tz_dsttime)
		diff -= 3600;

	s = splclock();
	n = (time.tv_sec - diff) % (3600 * 24);   /* hrs+mins+secs */
	rtc_write_reg (RTC_SEC_LOW, (n % 60) % 10);
	rtc_write_reg (RTC_SEC_HIGH, (n % 60) / 10);
	n /= 60;
	rtc_write_reg (RTC_MIN_LOW, (n % 60) % 10);
	rtc_write_reg (RTC_MIN_HIGH, (n % 60) / 10);
	rtc_write_reg (RTC_HOUR_LOW, (n / 60) % 10);
	rtc_write_reg (RTC_HOUR_HIGH, (n / 60) / 10);

	n = (time.tv_sec - diff) / (3600 * 24);	/* days */
	splx(s);
	rtc_write_reg (RTC_WEEK_DAY, (n + 4) % 7); /* 1/1/70 is Thursday */

	for (j = 1970, i = yeartoday(j); n >= i; j++, i = yeartoday(j))
		n -= i;

	rtc_write_reg (RTC_YEAR_LOW, (j - 1900 - RTC_YEAR_BASE) % 10);
	rtc_write_reg (RTC_YEAR_HIGH, (j - 1900 - RTC_YEAR_BASE) / 10);

	if (i == 366)
		month[1] = 29;
	for (i = 0; n >= month[i]; i++)
		n -= month[i];
	month[1] = 28;
	i++;
	rtc_write_reg (RTC_MON_LOW, i % 10);
	rtc_write_reg (RTC_MON_HIGH, i / 10);
	n++;
	rtc_write_reg (RTC_DAY_LOW, n % 10);
	rtc_write_reg (RTC_DAY_HIGH, n / 10);
}

static int
clock_intr (void *arg)
{
  if (clock_started)
    {
      hardclock ((struct clockframe *) arg);
      /* setsoftle (); */
    }
  return 1;
}

static struct intrhand clockhand = {clock_intr};
/*
 * clock_match: how is this zs channel configured?
 */
int 
clock_match(parent, match, aux)
	struct device *parent;
	void   *match, *aux;
{
	struct cfdata *cf = match;
	struct zsc_attach_args *args = aux;

	/* Exact match required for keyboard. */
	if (cf->cf_loc[0] == args->channel)
		return 2;

	return 0;
}

void 
clock_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct zsc_softc *zsc = (void *) parent;
	struct clock_softc *clock = (void *) self;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	int channel;
	int reset, s, tconst;

	channel = args->channel;
	cs = &zsc->zsc_cs[channel];
	cs->cs_private = clock;
	clock->clock_cs = cs;

	printf("\n");

	/* Initialize the speed, etc. */
	hz = 300; /* 60; */
	tconst = 0x515c;
	s = splclock();

	/* May need reset... */
	reset = (channel == 0) ?
		ZSWR9_A_RESET : ZSWR9_B_RESET;
	zs_write_reg(cs, 9, reset);

	/* These are OK as set by zscc: WR3, WR4, WR5 */
	cs->cs_preg[1] = 0; /* no intr.  */
	cs->cs_preg[3] = ZSWR3_RX_8 | ZSWR3_RX_ENABLE;
	cs->cs_preg[4] = ZSWR4_CLK_X1 | ZSWR4_ONESB | ZSWR4_PARENB;
	cs->cs_preg[5] = ZSWR5_TX_8 | ZSWR5_TX_ENABLE;
	cs->cs_preg[9] = 0; /* Clear MIE. */
	cs->cs_preg[10] = 0;
	cs->cs_preg[11] = ZSWR11_RXCLK_RTXC | ZSWR11_TXCLK_RTXC
	  | ZSWR11_TRXC_OUT_ENA | ZSWR11_TRXC_BAUD;
	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;
	cs->cs_preg[14] = ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA;
	cs->cs_preg[15] = 0;

	zs_loadchannelregs(cs);

	intr_establish (INTR_CLOCK, 0, &clockhand);
	splx(s);

	/* Initialize translator. */
	clock_started = 0;
}


void
cpu_initclocks (void)
{
  clock_started = 1;
}

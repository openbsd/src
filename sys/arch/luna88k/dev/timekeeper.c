/* $OpenBSD: timekeeper.c,v 1.3 2004/08/18 13:29:46 aoyama Exp $ */
/* $NetBSD: timekeeper.c,v 1.1 2000/01/05 08:48:56 nisimura Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/evcount.h>

#include <machine/autoconf.h>
#include <machine/board.h>	/* machtype value */
#include <machine/cpu.h>

#include <dev/clock_subr.h>

#include <luna88k/luna88k/clockvar.h>
#include <luna88k/dev/timekeeper.h>

#define	MK_YEAR0	1970	/* year offset of MK */
#define	DS_YEAR0	1990	/* year offset of DS */

struct timekeeper_softc {
	struct device sc_dev;
	void *sc_clock, *sc_nvram;
	int sc_nvramsize;
	struct evcount sc_count;
};

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)      (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)        (((x) / 10 * 16) + ((x) % 10))

int  clock_match(struct device *, void *, void *);
void clock_attach(struct device *, struct device *, void *);

struct cfattach clock_ca = {
	sizeof (struct timekeeper_softc), clock_match, clock_attach
};

struct cfdriver clock_cd = {
        NULL, "clock", DV_DULL
};

void mkclock_get(struct device *, time_t, struct clock_ymdhms *);
void mkclock_set(struct device *, struct clock_ymdhms *);
void dsclock_get(struct device *, time_t, struct clock_ymdhms *);
void dsclock_set(struct device *, struct clock_ymdhms *);

const struct clockfns mkclock_clockfns = {
	NULL /* never used */, mkclock_get, mkclock_set,
};

const struct clockfns dsclock_clockfns = {
	NULL /* never used */, dsclock_get, dsclock_set,
};

int
clock_match(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, clock_cd.cd_name))
		return 0;
	return 1;
}

extern int machtype; /* in machdep.c */

void
clock_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct timekeeper_softc *sc = (void *)self;
	struct mainbus_attach_args *ma = aux;
	const struct clockfns *clockwork;

	switch (machtype) {
	default:
	case LUNA_88K:	/* Mostek MK48T02 */
		sc->sc_clock = (void *)(ma->ma_addr + 2040);
		sc->sc_nvram = (void *)ma->ma_addr;
		sc->sc_nvramsize = 2040;
		clockwork = &mkclock_clockfns;
		printf(": MK48T02\n");
		break;
	case LUNA_88K2: /* Dallas DS1397 */
		sc->sc_clock = (void *)ma->ma_addr;
		sc->sc_nvram = (void *)(ma->ma_addr + 50);
		sc->sc_nvramsize = 50;
		clockwork = &dsclock_clockfns;
		printf(": DS1397\n");
		break;
	}

	evcount_attach(&sc->sc_count, self->dv_xname, (void *)&ma->ma_ilvl, &evcount_intr);

	clockattach(&sc->sc_dev, clockwork, &sc->sc_count);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
mkclock_get(dev, base, dt)
	struct device *dev;
	time_t base;
	struct clock_ymdhms *dt;
{
	struct timekeeper_softc *sc = (void *)dev;
	volatile u_int8_t *chiptime = (void *)sc->sc_clock;
	int s;

	s = splclock();
	chiptime[MK_CSR] |= MK_CSR_READ;	/* enable read (stop time) */
	dt->dt_sec = FROMBCD(chiptime[MK_SEC]);
	dt->dt_min = FROMBCD(chiptime[MK_MIN]);
	dt->dt_hour = FROMBCD(chiptime[MK_HOUR]);
	dt->dt_wday = FROMBCD(chiptime[MK_DOW]);
	dt->dt_day = FROMBCD(chiptime[MK_DOM]);
	dt->dt_mon = FROMBCD(chiptime[MK_MONTH]);
	dt->dt_year = FROMBCD(chiptime[MK_YEAR]) + MK_YEAR0;
	chiptime[MK_CSR] &= ~MK_CSR_READ;	/* time wears on */
	splx(s);
#ifdef TIMEKEEPER_DEBUG
	printf("get %d/%d/%d %d:%d:%d\n",
	    dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif
}

/*
 * Reset the TODR based on the time value.
 */
void
mkclock_set(dev, dt)
	struct device *dev;
	struct clock_ymdhms *dt;
{
	struct timekeeper_softc *sc = (void *)dev;
	volatile u_int8_t *chiptime = (void *)sc->sc_clock;
	volatile u_int8_t *stamp = (u_int8_t *)sc->sc_nvram + 0x10;
	int s;

	s = splclock();
	chiptime[MK_CSR] |= MK_CSR_WRITE;	/* enable write */
	chiptime[MK_SEC] = TOBCD(dt->dt_sec);
	chiptime[MK_MIN] = TOBCD(dt->dt_min);
	chiptime[MK_HOUR] = TOBCD(dt->dt_hour);
	chiptime[MK_DOW] = TOBCD(dt->dt_wday);
	chiptime[MK_DOM] = TOBCD(dt->dt_day);
	chiptime[MK_MONTH] = TOBCD(dt->dt_mon);
	chiptime[MK_YEAR] = TOBCD(dt->dt_year - MK_YEAR0);
	chiptime[MK_CSR] &= ~MK_CSR_WRITE;	/* load them up */
	splx(s);
#ifdef TIMEKEEPER_DEBUG
	printf("set %d/%d/%d %d:%d:%d\n",
	    dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif

	stamp[0] = 'R'; stamp[1] = 'T'; stamp[2] = 'C'; stamp[3] = '\0';
}

#define _DS_GET(off, data) \
	do { *chiptime = (off); (u_int8_t)(data) = (*chipdata); } while (0)
#define _DS_SET(off, data) \
	do { *chiptime = (off); *chipdata = (u_int8_t)(data); } while (0)
#define _DS_GET_BCD(off, data) \
	do { \
		u_int8_t c; \
		*chiptime = (off); \
		c = *chipdata; (u_int8_t)(data) = FROMBCD(c); \
	} while (0)
#define _DS_SET_BCD(off, data) \
	do { \
		*chiptime = (off); \
		*chipdata = TOBCD((u_int8_t)(data)); \
	} while (0)

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
dsclock_get(dev, base, dt)
	struct device *dev;
	time_t base;
	struct clock_ymdhms *dt;
{
	struct timekeeper_softc *sc = (void *)dev;
	volatile u_int8_t *chiptime = (void *)sc->sc_clock;
	volatile u_int8_t *chipdata = (void *)(sc->sc_clock + 1);
	int s;
	u_int8_t c;

	s = splclock();

	/* specify 24hr and BCD mode */
	_DS_GET(DS_REGB, c);
	c |= DS_REGB_24HR;
	c &= ~DS_REGB_BINARY;
	_DS_SET(DS_REGB, c);

	/* update in progress; spin loop */
	*chiptime = DS_REGA;
	while (*chipdata & DS_REGA_UIP)
		;

	_DS_GET_BCD(DS_SEC, dt->dt_sec);
	_DS_GET_BCD(DS_MIN, dt->dt_min);
	_DS_GET_BCD(DS_HOUR, dt->dt_hour);
	_DS_GET_BCD(DS_DOW, dt->dt_wday);
	_DS_GET_BCD(DS_DOM, dt->dt_day);
	_DS_GET_BCD(DS_MONTH, dt->dt_mon);
	_DS_GET_BCD(DS_YEAR, dt->dt_year);
	dt->dt_year += DS_YEAR0;

	splx(s);

#ifdef TIMEKEEPER_DEBUG
	printf("get %d/%d/%d %d:%d:%d\n",
	    dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif
}

/*
 * Reset the TODR based on the time value.
 */
void
dsclock_set(dev, dt)
	struct device *dev;
	struct clock_ymdhms *dt;
{
	struct timekeeper_softc *sc = (void *)dev;
	volatile u_int8_t *chiptime = (void *)sc->sc_clock;
	volatile u_int8_t *chipdata = (void *)(sc->sc_clock + 1);
	int s;
	u_int8_t c;

	s = splclock();
	
	/* enable write */
	_DS_GET(DS_REGB, c);
	c |= DS_REGB_SET;
	_DS_SET(DS_REGB, c);

	_DS_SET_BCD(DS_SEC, dt->dt_sec);
	_DS_SET_BCD(DS_MIN, dt->dt_min);
	_DS_SET_BCD(DS_HOUR, dt->dt_hour);
	_DS_SET_BCD(DS_DOW, dt->dt_wday);
	_DS_SET_BCD(DS_DOM, dt->dt_day);
	_DS_SET_BCD(DS_MONTH, dt->dt_mon);
	_DS_SET_BCD(DS_YEAR, dt->dt_year - DS_YEAR0);

	_DS_GET(DS_REGB, c);
	c &= ~DS_REGB_SET;
	_DS_SET(DS_REGB, c);

	splx(s);

#ifdef TIMEKEEPER_DEBUG
	printf("set %d/%d/%d %d:%d:%d\n",
	    dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif
}

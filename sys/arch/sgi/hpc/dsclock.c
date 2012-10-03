/*	$OpenBSD: dsclock.c,v 1.4 2012/10/03 22:46:09 miod Exp $	*/
/*	$NetBSD: dsclock.c,v 1.5 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2001 Rafal K. Boni
 * Copyright (c) 2001 Christopher Sekiya
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Portions of this code are derived from software contributed to The
 * NetBSD Foundation by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <mips64/dev/clockvar.h>

#include <dev/ic/ds1286reg.h>
#include <sgi/hpc/hpcvar.h>
#include <sgi/sgi/ip22.h>

struct dsclock_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	uint			sc_base;
};

int	dsclock_match(struct device *, void *, void *);
void	dsclock_attach(struct device *, struct device *, void *);

struct cfdriver dsclock_cd = {
	NULL, "dsclock", DV_DULL
};

const struct cfattach dsclock_ca = {
	sizeof(struct dsclock_softc), dsclock_match, dsclock_attach
};

#define	ds1286_read(sc,r) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, ((r) << 2) + 3)
#define	ds1286_write(sc,r,v) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, ((r) << 2) + 3, (v))

static inline int frombcd(int);
static inline int tobcd(int);

static inline int
frombcd(int x)
{
	return (x >> 4) * 10 + (x & 0xf);
}
static inline int
tobcd(int x)
{
	return (x / 10 * 16) + (x % 10);
}

void	dsclock_gettime(void *, time_t, struct tod_time *);
void	dsclock_settime(void *, struct tod_time *);

int
dsclock_match(struct device *parent, void *vcf, void *aux)
{
	struct hpc_attach_args *haa = aux;

	if (strcmp(haa->ha_name, dsclock_cd.cd_name) != 0)
		return 0;

	return 1;
}

void
dsclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct dsclock_softc *sc = (void *)self;
	struct hpc_attach_args *haa = aux;
	ds1286_todregs regs;

	sc->sc_iot = haa->ha_st;
	if (bus_space_subregion(haa->ha_st, haa->ha_sh, haa->ha_devoff,
	    4 * 8192, &sc->sc_ioh) != 0) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	sys_tod.tod_get = dsclock_gettime;
	sys_tod.tod_set = dsclock_settime;
	sys_tod.tod_cookie = self;

	/*
	 * Try and figure out what year the chip time is relative to.
	 * Different Indy PROM versions appear to use different base:
	 *  PROM Monitor SGI Version 5.1 Rev B3 IP24 Sep 17, 1993 (BE)
	 * uses 1970, while
	 *  PROM Monitor SGI Version 5.3 Rev B10 R4X00/R5000 IP24 Feb 12, 1996 (BE)
	 * uses 1940.
	 */

	DS1286_GETTOD(sc, &regs);
	sc->sc_base = bios_year - frombcd(regs[DS1286_YEAR]);
	/* year might have changed between the ARCBios call and now... */
	if ((sc->sc_base % 10) == 9)
		sc->sc_base++;

	/*
	 * If the data in the chip does not make sense, assume the usual
	 * IRIX timebase (1940 because it's a leap year).
	 */
	if (sc->sc_base != 1940 && sc->sc_base != POSIX_BASE_YEAR)
		sc->sc_base = 1940;

	/* mips64 clock code expects year relative to 1900 */
	sc->sc_base -= 1900;
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
dsclock_gettime(void *cookie, time_t base, struct tod_time *ct)
{
	struct dsclock_softc *sc = (void *)cookie;
	ds1286_todregs regs;
	int s;

	s = splhigh();
	DS1286_GETTOD(sc, &regs)
	splx(s);

	ct->sec = frombcd(regs[DS1286_SEC]);
	ct->min = frombcd(regs[DS1286_MIN]);

	if (regs[DS1286_HOUR] & DS1286_HOUR_12MODE) {
		ct->hour = frombcd(regs[DS1286_HOUR] & DS1286_HOUR_12HR_MASK) +
		    ((regs[DS1286_HOUR] & DS1286_HOUR_12HR_PM) ? 12 : 0);

		/*
		 * In AM/PM mode, hour range is 01-12, so adding in 12 hours
		 * for PM gives us 01-24, whereas we want 00-23, so map hour
		 * 24 to hour 0.
		 */
		if (ct->hour == 24)
			ct->hour = 0;
	} else {
		 ct->hour = frombcd(regs[DS1286_HOUR] & DS1286_HOUR_24HR_MASK);
	}

	ct->day = frombcd(regs[DS1286_DOM]);
	ct->mon = frombcd(regs[DS1286_MONTH] & DS1286_MONTH_MASK);
	ct->year = frombcd(regs[DS1286_YEAR]) + sc->sc_base;
}

/*
 * Reset the TODR based on the time value.
 */
void
dsclock_settime(void *cookie, struct tod_time *ct)
{
	struct dsclock_softc *sc = (void *)cookie;
	ds1286_todregs regs;
	int s;

	s = splhigh();
	DS1286_GETTOD(sc, &regs);
	splx(s);

	regs[DS1286_SUBSEC] = 0;
	regs[DS1286_SEC] = tobcd(ct->sec);
	regs[DS1286_MIN] = tobcd(ct->min);
	regs[DS1286_HOUR] = tobcd(ct->hour) & DS1286_HOUR_24HR_MASK;
	regs[DS1286_DOW] = tobcd(ct->dow);
	regs[DS1286_DOM] = tobcd(ct->day);

	/* Leave wave-generator bits as set originally */
	regs[DS1286_MONTH] &=  ~DS1286_MONTH_MASK;
	regs[DS1286_MONTH] |=  tobcd(ct->mon) & DS1286_MONTH_MASK;

	regs[DS1286_YEAR] = tobcd(ct->year - sc->sc_base);

	s = splhigh();
	DS1286_PUTTOD(sc, &regs);
	splx(s);
}

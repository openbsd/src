/*	$OpenBSD: dpclock.c,v 1.3 2014/10/11 18:41:18 miod Exp $	*/
/*	$NetBSD: dpclock.c,v 1.3 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
/*
 * Copyright (c) 2001 Erik Reid
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
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	8.2 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <mips64/dev/clockvar.h>

#include <dev/ic/dp8573areg.h>
#include <sgi/hpc/hpcvar.h>

#define	IRIX_BASE_YEAR	1940

struct dpclock_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	dpclock_match(struct device *, void *, void *);
void	dpclock_attach(struct device *, struct device *, void *);

struct cfdriver dpclock_cd = {
	NULL, "dpclock", DV_DULL
};

const struct cfattach dpclock_ca = {
	sizeof(struct dpclock_softc), dpclock_match, dpclock_attach
};

#define	dpclock_read(sc,r) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, ((r) << 2) + 3)
#define	dpclock_write(sc,r,v) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, ((r) << 2) + 3, (v))

static inline int frombcd(int);
static inline int tobcd(int);
static inline int leapyear(int year);

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
/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 * (borrowed from kern/clock_subr.c)
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

void	dpclock_gettime(void *, time_t, struct tod_time *);
void	dpclock_settime(void *, struct tod_time *);

int
dpclock_match(struct device *parent, void *vcf, void *aux)
{
	struct hpc_attach_args *haa = aux;

	if (strcmp(haa->ha_name, dpclock_cd.cd_name) != 0)
		return 0;

	return 1;
}

void
dpclock_attach(struct device *parent, struct device *self, void *aux)
{
	struct dpclock_softc *sc = (void *)self;
	struct hpc_attach_args *haa = aux;
	uint8_t st, mode, pflag;

	sc->sc_iot = haa->ha_st;
	if (bus_space_subregion(haa->ha_st, haa->ha_sh, haa->ha_devoff,
	    4 * DP8573A_NREG, &sc->sc_ioh) != 0) {
		printf(": can't map registers\n");
		return;
	}

	st = dpclock_read(sc, DP8573A_STATUS);
	dpclock_write(sc, DP8573A_STATUS, st | DP8573A_STATUS_REGSEL);
	mode = dpclock_read(sc, DP8573A_RT_MODE);
	if ((mode & DP8573A_RT_MODE_CLKSS) == 0) {
		printf(": clock stopped");
		dpclock_write(sc, DP8573A_RT_MODE,
		    mode | DP8573A_RT_MODE_CLKSS);
		dpclock_write(sc, DP8573A_INT0_CTL, 0);
		dpclock_write(sc, DP8573A_INT1_CTL, DP8573A_INT1_CTL_PWRINT);
	}
	dpclock_write(sc, DP8573A_STATUS, st & ~DP8573A_STATUS_REGSEL);
	pflag = dpclock_read(sc, DP8573A_PFLAG);
	if (pflag & DP8573A_PFLAG_OFSS) {
		dpclock_write(sc, DP8573A_PFLAG, pflag);
		pflag = dpclock_read(sc, DP8573A_PFLAG);
		if (pflag & DP8573A_PFLAG_OFSS) {
			/*
			 * If the `oscillator failure' condition sticks,
			 * the battery needs replacement and the clock
			 * is not ticking. Do not claim sys_tod.
			 */
			printf("%s oscillator failure\n",
			    (mode & DP8573A_RT_MODE_CLKSS) == 0 ?  "," : ":");
			return;
		}
	}
	if (pflag & DP8573A_PFLAG_TESTMODE) {
		dpclock_write(sc, DP8573A_RAM_1F, 0);
		dpclock_write(sc, DP8573A_PFLAG,
		    pflag & ~DP8573A_PFLAG_TESTMODE);
	}

	printf("\n");

	sys_tod.tod_get = dpclock_gettime;
	sys_tod.tod_set = dpclock_settime;
	sys_tod.tod_cookie = self;
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
dpclock_gettime(void *cookie, time_t base, struct tod_time *ct)
{
	struct dpclock_softc *sc = (void *)cookie;
	uint i;
	uint8_t regs[DP8573A_NREG];

	i = dpclock_read(sc, DP8573A_TIMESAVE_CTL);
	dpclock_write(sc, DP8573A_TIMESAVE_CTL, i | DP8573A_TIMESAVE_CTL_EN);
	dpclock_write(sc, DP8573A_TIMESAVE_CTL, i);

	for (i = 0; i < nitems(regs); i++)
		regs[i] = dpclock_read(sc, i);

	ct->sec = frombcd(regs[DP8573A_SAVE_SEC]);
	ct->min = frombcd(regs[DP8573A_SAVE_MIN]);

	if (regs[DP8573A_RT_MODE] & DP8573A_RT_MODE_1224) {
		ct->hour = frombcd(regs[DP8573A_SAVE_HOUR] &
		    DP8573A_HOUR_12HR_MASK) +
		    ((regs[DP8573A_SAVE_HOUR] & DP8573A_RT_MODE_1224) ? 0 : 12);

		/*
		 * In AM/PM mode, hour range is 01-12, so adding in 12 hours
		 * for PM gives us 01-24, whereas we want 00-23, so map hour
		 * 24 to hour 0.
		 */
		if (ct->hour == 24)
			ct->hour = 0;
	} else {
		ct->hour = frombcd(regs[DP8573A_SAVE_HOUR] &
		    DP8573A_HOUR_24HR_MASK);
	}

	ct->day = frombcd(regs[DP8573A_SAVE_DOM]);
	ct->mon = frombcd(regs[DP8573A_SAVE_MONTH]);
	ct->year = frombcd(regs[DP8573A_YEAR]) + (IRIX_BASE_YEAR - 1900);
}

/*
 * Reset the TODR based on the time value.
 */
void
dpclock_settime(void *cookie, struct tod_time *ct)
{
	struct dpclock_softc *sc = (void *)cookie;
	uint i;
	uint st, r, delta;
	uint8_t regs[DP8573A_NREG];

	r = dpclock_read(sc, DP8573A_TIMESAVE_CTL);
	dpclock_write(sc, DP8573A_TIMESAVE_CTL, r | DP8573A_TIMESAVE_CTL_EN);
	dpclock_write(sc, DP8573A_TIMESAVE_CTL, r);

	for (i = 0; i < nitems(regs); i++)
		regs[i] = dpclock_read(sc, i);

	regs[DP8573A_SUBSECOND] = 0;
	regs[DP8573A_SECOND] = tobcd(ct->sec);
	regs[DP8573A_MINUTE] = tobcd(ct->min);
	regs[DP8573A_HOUR] = tobcd(ct->hour) & DP8573A_HOUR_24HR_MASK;
	regs[DP8573A_DOW] = tobcd(ct->dow);
	regs[DP8573A_DOM] = tobcd(ct->day);
	regs[DP8573A_MONTH] = tobcd(ct->mon);
	regs[DP8573A_YEAR] = tobcd(ct->year - (IRIX_BASE_YEAR - 1900));

	st = dpclock_read(sc, DP8573A_STATUS);
	dpclock_write(sc, DP8573A_STATUS, st | DP8573A_STATUS_REGSEL);
	r = dpclock_read(sc, DP8573A_RT_MODE);
	dpclock_write(sc, DP8573A_RT_MODE, r & ~DP8573A_RT_MODE_CLKSS);

	for (i = 0; i < 10; i++)
		dpclock_write(sc, DP8573A_COUNTERS + i,
		    regs[DP8573A_COUNTERS + i]);

	/*
	 * We now need to set the leap year counter to the correct value.
	 * Unfortunately it is only two bits wide, while eight years can
	 * happen between two leap years.  Skirting this is left as an
	 * exercise to the reader with an Indigo in working condition
	 * by year 2100.
	 */
	delta = 0;
	while (delta < 3 && !leapyear(ct->year - delta))
		delta++;
	
	r &= ~(DP8573A_RT_MODE_LYLSB | DP8573A_RT_MODE_LYMSB);
	dpclock_write(sc, DP8573A_RT_MODE, r | delta | DP8573A_RT_MODE_CLKSS);

	dpclock_write(sc, DP8573A_STATUS, st & ~DP8573A_STATUS_REGSEL);
}

/*	$NetBSD: clock_mc.c,v 1.3 1995/11/23 02:33:45 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pte.h>		/* XXX */

#include <alpha/alpha/clockvar.h>
#include <dev/ic/mc146818reg.h>

#include "tc.h"
#if NTC
	/* XXX */
#endif

#include "isa.h"
#if NISA
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#endif

void		mcclock_attach __P((struct device *parent,
		    struct device *self, void *aux));
static void	mcclock_init __P((struct clock_softc *csc));
static void	mcclock_get __P((struct clock_softc *csc, time_t base,
		    struct clocktime *ct));
static void	mcclock_set __P((struct clock_softc *csc,
		    struct clocktime *ct));

struct mcclockdata {
	void	(*mc_write) __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
	u_int	(*mc_read) __P((struct clock_softc *csc, u_int reg));
	__const void *mc_accessfns;
	void	*mc_accessarg;
	void	*mc_addr;
};

#define	mc146818_write(sc, reg, datum)					\
	    (*((struct mcclockdata *)sc->sc_data)->mc_write)(sc, reg, datum)
#define	mc146818_read(sc, reg)						\
	    (*((struct mcclockdata *)sc->sc_data)->mc_read)(sc, reg)

#if NTC
static void	mc_write_tc __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
static u_int	mc_read_tc __P((struct clock_softc *csc, u_int reg));
static struct mcclockdata mcclockdata_tc = { mc_write_tc, mc_read_tc };
#endif

#if NISA
static void	mc_write_isa __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
static u_int	mc_read_isa __P((struct clock_softc *csc, u_int reg));
static struct mcclockdata mcclockdata_isa = { mc_write_isa, mc_read_isa };
#endif

void
mcclock_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *csc = (struct clock_softc *)self;
	struct isadev_attach_args *ida = aux;
	register volatile struct chiptime *c;
	struct confargs *ca = aux;
#if NTC
	extern struct cfdriver asiccd;				/* XXX */
#endif

	printf(": mc146818 or compatible");

	csc->sc_init = mcclock_init;
	csc->sc_get = mcclock_get;
	csc->sc_set = mcclock_set;

#if NTC
	if (parent->dv_cfdata->cf_driver == &asiccd) {
		/* 
		 * XXX should really allocate a new one and copy, or
		 * something.  unlikely we'll have more than one...
		 */
		csc->sc_data = &mcclockdata_tc;
		mcclockdata_tc.mc_addr = BUS_CVTADDR(ca);
	} else
#endif
#if NISA
	if (parent->dv_cfdata->cf_driver == &isacd) {
		/* 
		 * XXX should really allocate a new one and copy, or
		 * something.  unlikely we'll have more than one...
		 */
		csc->sc_data = &mcclockdata_isa;
		mcclockdata_isa.mc_accessfns = ida->ida_piofns;
		mcclockdata_isa.mc_accessarg = ida->ida_pioarg;
	} else
#endif
	{
		printf("\n");
		panic("clockattach: can't tell what bus we're on");
	}

	/* Turn interrupts off, just in case. */
	mc146818_write(csc, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);
}

static void
mcclock_init(csc)
	struct clock_softc *csc;
{

	mc146818_write(csc, MC_REGA, MC_BASE_32_KHz | MC_RATE_1024_Hz);
	mc146818_write(csc, MC_REGB,
	    MC_REGB_PIE | MC_REGB_BINARY | MC_REGB_24HR);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
static void
mcclock_get(csc, base, ct)
	struct clock_softc *csc;
	time_t base;
	struct clocktime *ct;
{
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(csc, &regs)
	splx(s);

	ct->sec = regs[MC_SEC];
	ct->min = regs[MC_MIN];
	ct->hour = regs[MC_HOUR];
	ct->dow = regs[MC_DOW];
	ct->day = regs[MC_DOM];
	ct->mon = regs[MC_MONTH];
	ct->year = regs[MC_YEAR];
}

/*
 * Reset the TODR based on the time value.
 */
static void
mcclock_set(csc, ct)
	struct clock_softc *csc;
	struct clocktime *ct;
{
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(csc, &regs);
	splx(s);

	regs[MC_SEC] = ct->sec;
	regs[MC_MIN] = ct->min;
	regs[MC_HOUR] = ct->hour;
	regs[MC_DOW] = ct->dow;
	regs[MC_DOM] = ct->day;
	regs[MC_MONTH] = ct->mon;
	regs[MC_YEAR] = ct->year;

	s = splclock();
	MC146818_PUTTOD(csc, &regs);
	splx(s);
}


#if NTC
struct tc_clockdatum {
	u_char	datum;
	char	pad[3];
};

static void
mc_write_tc(csc, reg, datum)
	struct clock_softc *csc;
	u_int reg, datum;
{
	struct mcclockdata *mccdp;
	struct tc_clockdatum *dp;

	mccdp = csc->sc_data;
	dp = (struct tc_clockdatum *)(mccdp->mc_addr);

	dp[reg].datum = datum;
}

static u_int
mc_read_tc(csc, reg)
	struct clock_softc *csc;
	u_int reg;
{
	struct mcclockdata *mccdp;
	struct tc_clockdatum *dp;

	mccdp = csc->sc_data;
	dp = (struct tc_clockdatum *)(mccdp->mc_addr);

	return (dp[reg].datum);
}
#endif /* NTC */


#if NISA
static void
mc_write_isa(csc, reg, datum)
	struct clock_softc *csc;
	u_int reg, datum;
{
	struct mcclockdata *cdp = csc->sc_data;
	__const struct isa_pio_fns *ipf = cdp->mc_accessfns;
	void *ipfa = cdp->mc_accessarg;

	OUTB(ipf, ipfa, IO_RTC, reg);
	OUTB(ipf, ipfa, IO_RTC + 1, datum);
}

static u_int
mc_read_isa(csc, reg)
	struct clock_softc *csc;
	u_int reg;
{
	struct mcclockdata *cdp = csc->sc_data;
	__const struct isa_pio_fns *ipf = cdp->mc_accessfns;
	void *ipfa = cdp->mc_accessarg;

	OUTB(ipf, ipfa, IO_RTC, reg);
	return INB(ipf, ipfa, IO_RTC + 1);
}
#endif /* NISA */

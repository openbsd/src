/*	$NetBSD: clock_mc.c,v 1.2 1995/06/28 04:30:30 cgd Exp $	*/

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

#include <pica/pica/clockvar.h>
#include <pica/pica/picatype.h>
#include <pica/pica/pica.h>
#include <dev/ic/mc146818reg.h>

extern u_int	cputype;
extern int	cpu_int_mask;

void		mcclock_attach __P((struct device *parent,
		    struct device *self, void *aux));
static void	mcclock_init __P((struct clock_softc *csc));
static void	mcclock_get __P((struct clock_softc *csc, time_t base,
		    struct tod_time *ct));
static void	mcclock_set __P((struct clock_softc *csc,
		    struct tod_time *ct));

struct mcclockdata {
	void	(*mc_write) __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
	u_int	(*mc_read) __P((struct clock_softc *csc, u_int reg));
	void	*mc_addr;
};

#define	mc146818_write(sc, reg, datum)					\
	    (*((struct mcclockdata *)sc->sc_data)->mc_write)(sc, reg, datum)
#define	mc146818_read(sc, reg)						\
	    (*((struct mcclockdata *)sc->sc_data)->mc_read)(sc, reg)

#if defined(MIPS_PICA_61)
static void	mc_write_pica __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
static u_int	mc_read_pica __P((struct clock_softc *csc, u_int reg));
static struct mcclockdata mcclockdata_pica = { mc_write_pica, mc_read_pica };
#endif

void
mcclock_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *csc = (struct clock_softc *)self;

	register volatile struct chiptime *c;
	struct confargs *ca = aux;

	printf(": mc146818 or compatible");

	csc->sc_init = mcclock_init;
	csc->sc_get = mcclock_get;
	csc->sc_set = mcclock_set;

        switch (cputype) {

        case MIPS_PICA_61:
		/* 
		 * XXX should really allocate a new one and copy, or
		 * something.  unlikely we'll have more than one...
		 */
		csc->sc_data = &mcclockdata_pica;
		mcclockdata_pica.mc_addr = BUS_CVTADDR(ca);
		break;

	default:
		printf("\n");
		panic("don't know how to set up for other system types.");
	}

	/* Turn interrupts off, just in case. */
	mc146818_write(csc, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);
}

static void
mcclock_init(csc)
	struct clock_softc *csc;
{
/* XXX Does not really belong here but for the moment we don't care */
	out32(PICA_SYS_IT_VALUE, 9); /* 10ms - 1 */
	/* Enable periodic clock interrupt */
	out32(PICA_SYS_EXT_IMASK, cpu_int_mask);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
static void
mcclock_get(csc, base, ct)
	struct clock_softc *csc;
	time_t base;
	struct tod_time *ct;
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
	struct tod_time *ct;
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


#if defined(ACER_PICA_61)

static void
mc_write_pica(csc, reg, datum)
	struct clock_softc *csc;
	u_int reg, datum;
{
	int i,as;

	as = in32(PICA_SYS_ISA_AS) & 0x80;
	out32(PICA_SYS_ISA_AS, as | reg);
	outb(PICA_SYS_CLOCK, datum);
}

static u_int
mc_read_pica(csc, reg)
	struct clock_softc *csc;
	u_int reg;
{
	int i,as;

	as = in32(PICA_SYS_ISA_AS) & 0x80;
	out32(PICA_SYS_ISA_AS, as | reg);
	i = inb(PICA_SYS_CLOCK);
	return(i);
}
#endif /*ACER_PICA_61*/

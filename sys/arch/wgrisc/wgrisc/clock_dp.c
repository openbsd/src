/*	$OpenBSD: clock_dp.c,v 1.2 1997/08/24 12:01:14 pefo Exp $	*/
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
#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/dp857xreg.h>

#include <wgrisc/wgrisc/clockvar.h>
#include <wgrisc/wgrisc/wgrisctype.h>
#include <wgrisc/riscbus/riscbus.h>
#include <wgrisc/isa/isa_machdep.h>

extern u_int	cputype;
extern int	cpu_int_mask;

void		dpclock_attach __P((struct device *parent,
		    struct device *self, void *aux));
static void	dpclock_init_riscbus __P((struct clock_softc *csc));
static void	dpclock_get __P((struct clock_softc *csc, time_t base,
		    struct tod_time *ct));
static void	dpclock_set __P((struct clock_softc *csc,
		    struct tod_time *ct));

struct dpclockdata {
	void	(*dp_write) __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
	u_int	(*dp_read) __P((struct clock_softc *csc, u_int reg));
	void	*dp_addr;
};

#define	dp857x_write(sc, reg, datum)					\
	    (*((struct dpclockdata *)sc->sc_data)->dp_write)(sc, reg, datum)
#define	dp857x_read(sc, reg)						\
	    (*((struct dpclockdata *)sc->sc_data)->dp_read)(sc, reg)

/* riscbus clock read code */
static void	dp_write_riscbus __P((struct clock_softc *csc, u_int reg,
		    u_int datum));
static u_int	dp_read_riscbus __P((struct clock_softc *csc, u_int reg));
static struct dpclockdata dpclockdata_riscbus = { dp_write_riscbus, dp_read_riscbus };

void
dpclock_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *csc = (struct clock_softc *)self;

	register volatile struct chiptime *c;
	struct confargs *ca = aux;

	printf(": dp857[012] or compatible");

	csc->sc_get = dpclock_get;
	csc->sc_set = dpclock_set;

        switch (cputype) {

        case WGRISC9100:
		csc->sc_init = dpclock_init_riscbus;
		csc->sc_data = &dpclockdata_riscbus;
		dpclockdata_riscbus.dp_addr = BUS_CVTADDR(ca);
		break;

	default:
		printf("\n");
		panic("don't know how to set up for other system types.");
	}

	/* Initialize and turn interrupts off, just in case. */

	dp857x_write(csc, MAIN_STATUS, 0x40);
	dp857x_write(csc, INTERRUPT_CTRL0, 0);
	dp857x_write(csc, INTERRUPT_CTRL1, 0);
	dp857x_write(csc, OUTPUT_MODE, 0x08);
	dp857x_write(csc, REAL_TIME_MODE,
		(dp857x_read(csc, REAL_TIME_MODE) & 3) |  0x08);

	dp857x_write(csc, MAIN_STATUS, 0x3c);	/* clears pending ints */
	dp857x_write(csc, INTERRUPT_ROUT, 0);
	dp857x_write(csc, TIMER0_CTRL, 0);
	dp857x_write(csc, TIMER1_CTRL, 0);
}
/*
 * TOD clock also used for periodic interrupts.
 */
static struct clock_softc *int_csc;

static void
dpclock_init_riscbus(csc)
	struct clock_softc *csc;
{
	int_csc = csc;
	dp857x_write(csc, MAIN_STATUS, 0x40);
	dp857x_write(csc, INTERRUPT_CTRL0, 0x10);	/* 100 Hz */
	dp857x_write(csc, MAIN_STATUS, 0);
}

int
dpclock_interrupt()
{
	if(dp857x_read(int_csc, MAIN_STATUS) & 0x04) { /* periodic interrupt */
		dp857x_write(int_csc, MAIN_STATUS, 0x04);
		return(1);
	}
	return(0);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
static void
dpclock_get(csc, base, ct)
	struct clock_softc *csc;
	time_t base;
	struct tod_time *ct;
{
	dp_todregs regs;
	int s;

	s = splclock();
	DP857X_GETTOD(csc, &regs)
	splx(s);

	ct->sec = regs[CLK_SECONDS];
	ct->min = regs[CLK_MINUTES];
	ct->hour = regs[CLK_HOURS];
	ct->dow = regs[CLK_WEEKDAY];
	ct->day = regs[CLK_DAY];
	ct->mon = regs[CLK_MONTH];
	ct->year = regs[CLK_YEAR];
}

/*
 * Reset the TODR based on the time value.
 */
static void
dpclock_set(csc, ct)
	struct clock_softc *csc;
	struct tod_time *ct;
{
	dp_todregs regs;
	int s;

	s = splclock();
	DP857X_GETTOD(csc, &regs)

	regs[CLK_SECONDS] = ct->sec;
	regs[CLK_MINUTES] = ct->min;
	regs[CLK_HOURS] = ct->hour;
	regs[CLK_WEEKDAY] = ct->dow;
	regs[CLK_DAY] = ct->day;
	regs[CLK_MONTH] = ct->mon;
	regs[CLK_YEAR] = ct->year;

	DP857X_PUTTOD(csc, &regs);
	splx(s);
}

static void
dp_write_riscbus(csc, reg, datum)
	struct clock_softc *csc;
	u_int reg, datum;
{
	outb(((struct dpclockdata *)csc->sc_data)->dp_addr + reg, datum);
}

static u_int
dp_read_riscbus(csc, reg)
	struct clock_softc *csc;
	u_int reg;
{
	int i;
	i = inb(((struct dpclockdata *)csc->sc_data)->dp_addr + reg);
	return(i);
}

#define WWDELAY 50
#define WDELAY() \
{\
   register int i; \
   wbflush(); \
   for (i = WWDELAY; i != 0; i--) continue; \
}

/*----------------------------------------------------------------------
 *	Read non-volatile ram part of clock chip.
 *	-----------------------------------------
 *
 *	First byte is located in page 0, and read first
 * 	then page 1 is switched in and the rest is read.
 ----------------------------------------------------------------------*/
unsigned char *ReadNVram(unsigned char *ptr)
{
	volatile unsigned char *clockram = (volatile unsigned char *)RISC_RTC;
	unsigned char  main_stat_save;
	int	       count;

	main_stat_save = inb(clockram + MAIN_STATUS);
	WDELAY();	
	outb(clockram + MAIN_STATUS, 0);
	WDELAY();
	*ptr++ = inb(clockram + RAM_1E);
	WDELAY();
	outb(clockram + MAIN_STATUS, 0x80);
	WDELAY();
	for(count=1; count < 32; count++) {
		WDELAY();
		*ptr++ = inb(clockram + count);
	}
	WDELAY();
	outb(clockram + MAIN_STATUS, main_stat_save & 0xc0);
	return(0);
}

/*----------------------------------------------------------------------*
 *	Write non-volatile ram part of clock chip.
 *	------------------------------------------
 *
 *	First byte is located in page 0, and written first
 * 	then page 1 is switched in and the rest is written.
 *----------------------------------------------------------------------*/
unsigned char *WriteNVram(unsigned char *ptr)
{
	volatile unsigned char *clockram = (volatile unsigned char *)RISC_RTC;
	unsigned char  main_stat_save;
	int	       count;

	main_stat_save = inb(clockram+MAIN_STATUS);
	WDELAY();
	outb(clockram + MAIN_STATUS, 0);
	outb(clockram + RAM_1E, *ptr++);
	outb(clockram + MAIN_STATUS, 0x80);
	for(count=1; count < 32; count++) {
		outb(clockram + count, *ptr++);
	}
	outb(clockram + MAIN_STATUS, main_stat_save & 0xc0);
	return 0;
}

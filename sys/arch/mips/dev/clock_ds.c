/*	$OpenBSD: clock_ds.c,v 1.1 1998/01/29 15:06:17 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <machine/cpu.h>

#include <mips/dev/clockvar.h>
#include <mips/archtype.h>
#include <mips/dev/ds1386reg.h>


void        md_clk_attach __P((struct device *, struct device *, void *));
static void dsclk_init __P((struct clock_softc *));
static void dsclk_get __P((struct clock_softc *, time_t, struct tod_time *));
static void dsclk_set __P((struct clock_softc *, struct tod_time *));

struct dsclkdata {
	void	(*ds_write) __P((struct clock_softc *, u_int, u_int));
	u_int	(*ds_read) __P((struct clock_softc *, u_int));
	void	*ds_addr;
};

#define	ds1386_write(sc, reg, datum)					\
	    (*((struct dsclkdata *)sc->sc_data)->ds_write)(sc, reg, datum)
#define	ds1386_read(sc, reg)						\
	    (*((struct dsclkdata *)sc->sc_data)->ds_read)(sc, reg)

#ifdef hkmips
static void  ds_write_laguna __P((struct clock_softc *, u_int, u_int));
static u_int ds_read_laguna __P((struct clock_softc *, u_int));
static struct dsclkdata dsclkdata_laguna = {
	ds_write_laguna,
	ds_read_laguna,
	(void *)NULL
};
static u_char ethaddr[8];
#endif

#ifdef sgi
u_int32_t cpu_counter_interval;  /* Number of counter ticks/tick */
u_int32_t cpu_counter_last;      /* Last compare value loaded    */

static void  ds_write_sgi_indy __P((struct clock_softc *, u_int, u_int));
static u_int ds_read_sgi_indy __P((struct clock_softc *, u_int));
static struct dsclkdata dsclkdata_sgi_indy = {
	ds_write_sgi_indy,
	ds_read_sgi_indy,
	(void *)NULL
};
#endif

void
md_clk_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *sc;
	struct confargs *ca;

	sc = (struct clock_softc *)self;
	ca = aux;

	printf(": DS1[234]86 or compatible");

	sc->sc_init = dsclk_init;
	sc->sc_get = dsclk_get;
	sc->sc_set = dsclk_set;

        switch (system_type) {

#ifdef hkmips
        case LAGUNA:
		/* 
		 * XXX should really allocate a new one and copy, or
		 * something.  unlikely we'll have more than one...
		 */
		sc->sc_data = &dsclkdata_laguna;
		dsclkdata_laguna.ds_addr = BUS_CVTADDR(ca);

		ethaddr[0] = ds_read_laguna(sc, DS_ETHERADR+0);
		ethaddr[1] = ds_read_laguna(sc, DS_ETHERADR+1);
		ethaddr[2] = ds_read_laguna(sc, DS_ETHERADR+2);
		ethaddr[3] = ds_read_laguna(sc, DS_ETHERADR+3);
		ethaddr[4] = ds_read_laguna(sc, DS_ETHERADR+4);
		ethaddr[5] = ds_read_laguna(sc, DS_ETHERADR+5);
		break;
#endif

#ifdef sgi
	case SGI_INDY:
		sc->sc_data = &dsclkdata_sgi_indy;
		dsclkdata_sgi_indy.ds_addr = BUS_CVTADDR(ca);
		break;
#endif
	default:
		printf("\n");
		panic("don't know how to set up for other system types.");
	}

	/* Turn interrupts off, just in case. */
	ds1386_write(sc, DS_REGC, DS_REGC_TE);
}

/*
 *	Clock initialization.
 *
 *	INDY's use the CPU timer register to generate the
 *	system ticker. This due to a rumour that the 8254
 *	has a bug that makes it unusable. Well who knows...
 */
static void
dsclk_init (sc)
	struct clock_softc *sc ;
{
#ifdef sgi
	/* XXX get cpu frquency! */
	cpu_counter_interval = 50000000 / hz;
	cpu_counter_last = R4K_GetCOUNT();
	cpu_counter_last += cpu_counter_interval;
	R4K_SetCOMPARE(cpu_counter_last);
#endif
}

/*
 *	Get the clock device idea of the time of day.
 */
static void
dsclk_get(sc, base, ct)
	struct clock_softc *sc;
	time_t base;
	struct tod_time *ct;
{
	ds_todregs regs;
	int s;

	s = splclock();
	DS1386_GETTOD(sc, &regs)
	splx(s);

#define	hex_to_bin(x,m) (((x) & m & 0xf) + (((x) & m) >> 4) * 10)
	ct->sec = hex_to_bin(regs[DS_SEC], 0x7f);
	ct->min = hex_to_bin(regs[DS_MIN], 0x7f);
	ct->hour = hex_to_bin(regs[DS_HOUR], 0x3f);
	ct->dow = hex_to_bin(regs[DS_DOW], 0x07);
	ct->day = hex_to_bin(regs[DS_DOM], 0x3f);
	ct->mon = hex_to_bin(regs[DS_MONTH], 0x1f);
	ct->year = hex_to_bin(regs[DS_YEAR], 0xff);

printf("%d:%d:%d-%d-%d-%d\n", ct->hour, ct->min, ct->sec, ct->year, ct->mon, ct->day);
}

/*
 *	Reset the clock device to the current time value.
 */
static void
dsclk_set(sc, ct)
	struct clock_softc *sc;
	struct tod_time *ct;
{
	ds_todregs regs;
	int s;

	s = splclock();
	DS1386_GETTOD(sc, &regs);
	splx(s);

#define	bin_to_hex(x,m) ((((x) / 10 << 4) + (x) % 10) & m)
	regs[DS_SEC_100] = 0x00;
	regs[DS_SEC] = bin_to_hex(ct->sec, 0x7f);
	regs[DS_MIN] = bin_to_hex(ct->min, 0x7f);
	regs[DS_HOUR] = bin_to_hex(ct->hour, 0x3f);
	regs[DS_DOW] = bin_to_hex(ct->dow, 0x07);
	regs[DS_DOM] = bin_to_hex(ct->day, 0x3f);
	regs[DS_MONTH] = bin_to_hex(ct->mon, 0x1f);
	regs[DS_YEAR] = bin_to_hex(ct->year, 0xff);

	s = splclock();
	DS1386_PUTTOD(sc, &regs);
	splx(s);
}


/*
 *	Clock register acces routines for different clock chips
 */

#ifdef hkmips
static void
ds_write_laguna(sc, reg, datum)
	struct clock_softc *sc;
	u_int reg, datum;
{
	int i,brc1;

	brc1 = inb(LAGUNA_BRC1_REG);
	outb(LAGUNA_BRC1_REG, brc1 | LAGUNA_BRC1_ENRTCWR);
	outb(((struct dsclkdata *)sc->sc_data)->ds_addr + (reg * 4), datum);
}

static u_int
ds_read_laguna(sc, reg)
	struct clock_softc *sc;
	u_int reg;
{
	int i;

	i = inb(((struct dsclkdata *)sc->sc_data)->ds_addr + (reg * 4));
	i &= 0xff;
	return(i);
}
#endif

#ifdef sgi
static void
ds_write_sgi_indy(sc, reg, datum)
	struct clock_softc *sc;
	u_int reg, datum;
{
	outb(((struct dsclkdata *)sc->sc_data)->ds_addr + (reg * 4), datum);
}

static u_int
ds_read_sgi_indy(sc, reg)
	struct clock_softc *sc;
	u_int reg;
{
	int i;

	i = inb(((struct dsclkdata *)sc->sc_data)->ds_addr + (reg * 4));
	i &= 0xff;
	return(i);
}
#endif

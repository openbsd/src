/*	$OpenBSD: riscbus.c,v 1.4 1999/01/11 05:12:10 millert Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/autoconf.h>

#include <wgrisc/riscbus/riscbus.h>
#include <wgrisc/wgrisc/wgrisctype.h>

struct riscbus_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	riscbus_dev *sc_devs;
};

/* Definition of the driver for autoconfig. */
int	riscbusmatch(struct device *, void *, void *);
void	riscbusattach(struct device *, struct device *, void *);
int	riscbusprint(void *, const char *);

struct cfattach riscbus_ca = {
	sizeof(struct riscbus_softc), riscbusmatch, riscbusattach
};
struct cfdriver riscbus_cd = {
	NULL, "riscbus", DV_DULL, NULL, 0
};

void	riscbus_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	riscbus_intr_disestablish __P((struct confargs *));
caddr_t	riscbus_cvtaddr __P((struct confargs *));
int	riscbus_matchname __P((struct confargs *, char *));
int	riscbus_iointr __P((unsigned, struct clockframe *));
int	riscbus_clkintr __P((unsigned, struct clockframe *));

extern int cputype;

/*
 *  Interrupt dispatch table.
 */
struct riscbus_int_desc int_table[] = {
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  0 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  1 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  2 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  3 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  4 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  5 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  6 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  7 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  8 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /*  9 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 10 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 11 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 12 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 13 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 14 */
	{0, riscbus_intrnull, (void *)NULL, 0 },  /* 15 */
};

struct riscbus_dev {
	struct confargs	ps_ca;
	u_int		ps_mask;
	intr_handler_t	ps_handler;
	void 		*ps_base;
};
#ifdef WGRISC9100
struct riscbus_dev wgrisc9100_cpu[] = {
	{{ "dp8571rtc",	0, 0, },
	   INT_MASK_2,	riscbus_intrnull, (void *)RISC_RTC, },
	{{ "sonic",	1, 0, },
	   INT_MASK_4,	riscbus_intrnull, (void *)RISC_SONIC, },
	{{ "asc",	2, 0, },
	   INT_MASK_1,	riscbus_intrnull, (void *)RISC_SCSI, },
	{{ "com",	3, NULL, },
	   INT_MASK_5,	riscbus_intrnull, (void *)RISC_COM0, },
	{{ "com",	4, 0, },
	   INT_MASK_5,	riscbus_intrnull, (void *)RISC_COM1, },
	{{ "com",       5, 0, },
	   INT_MASK_5,	riscbus_intrnull, (void *)RISC_COM2, },
	{{ "com",       6, 0, },
	   INT_MASK_5,	riscbus_intrnull, (void *)RISC_COM3, },
	{{ "fl",     7, 0, },
	   0, NULL, (void *)NULL, },
	{{ NULL,       -1, NULL, },
	   0, NULL, (void *)NULL, },
};
#define NUM_RISC_DEVS (sizeof(wgrisc9100_cpu) / sizeof(struct riscbus_dev))
#endif

struct riscbus_dev *riscbus_cpu_devs[] = {
        NULL,                   /* 0: Unused */
#ifdef WGRISC9100
        wgrisc9100_cpu,		/* 1: WGRISC 9100 R3081 board */
#else
	NULL,
#endif
};
int nriscbus_cpu_devs = sizeof riscbus_cpu_devs / sizeof riscbus_cpu_devs[0];

int local_int_mask = 0;	/* Local interrupt enable mask */

int
riscbusmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

        /* Make sure that we're looking for a RISC bus. */
        if (strcmp(ca->ca_name, riscbus_cd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 ||
	    cputype > nriscbus_cpu_devs || riscbus_cpu_devs[cputype] == NULL)
		return (0);

	return (1);
}

void
riscbusattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct riscbus_softc *sc = (struct riscbus_softc *)self;
	struct confargs *nca;
	int i;

	printf("\n");

	/* keep our CPU device description handy */
	sc->sc_devs = riscbus_cpu_devs[cputype];

	/* set up interrupt handlers */
	set_intr(INT_MASK_1 | INT_MASK_4 | INT_MASK_5, riscbus_iointr, 2);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_RISC;
	sc->sc_bus.ab_intr_establish = riscbus_intr_establish;
	sc->sc_bus.ab_intr_disestablish = riscbus_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = riscbus_cvtaddr;
	sc->sc_bus.ab_matchname = riscbus_matchname;

	/* Try to configure each attached device */
	for (i = 0; sc->sc_devs[i].ps_ca.ca_slot >= 0; i++) {

		if(sc->sc_devs[i].ps_ca.ca_name == NULL)
			continue; /* Empty slot */

		nca = &sc->sc_devs[i].ps_ca;
		nca->ca_bus = &sc->sc_bus;

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, riscbusprint);
	}
}

int
riscbusprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        printf(" slot %ld offset 0x%lx", ca->ca_slot, ca->ca_offset);
        return (UNCONF);
}

caddr_t
riscbus_cvtaddr(ca)
	struct confargs *ca;
{
	struct riscbus_softc *sc = riscbus_cd.cd_devs[0];

	return(sc->sc_devs[ca->ca_slot].ps_base + ca->ca_offset);

}

void
riscbus_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
	void *val;
{
	struct riscbus_softc *sc = riscbus_cd.cd_devs[0];

	int slot;

	slot = ca->ca_slot;

	if(int_table[slot].int_mask != 0) {
		panic("riscbus intr already set");
	}
	else {
		int_table[slot].int_mask = sc->sc_devs[slot].ps_mask;
		local_int_mask |= int_table[slot].int_mask;
		int_table[slot].int_hand = handler;
		int_table[slot].param = val;
	}
	if(slot == 0) {		/* Slot 0 is special, clock */
		set_intr(int_table[0].int_mask, riscbus_clkintr, 0);
	}
}

void
riscbus_intr_disestablish(ca)
	struct confargs *ca;
{
	struct riscbus_softc *sc = riscbus_cd.cd_devs[0];

	int slot;

	slot = ca->ca_slot;
	if(slot = 0) {		/* Slot 0 is special, clock */
	}
	else {
		local_int_mask &= ~int_table[slot].int_mask;
		int_table[slot].int_mask = 0;
		int_table[slot].int_hand = riscbus_intrnull;
		int_table[slot].param = (void *)NULL;
	}
}

int
riscbus_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (strcmp(name, ca->ca_name) == 0);
}

int
riscbus_intrnull(val)
	void *val;
{
	panic("uncaught RISCBUS interrupt for slot %d", val);
}

/*
 *   Handle riscbus i/o interrupt the generic way.
 */
int
riscbus_iointr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int i;

	for(i = 0; i < NUM_RISC_DEVS; i++) {
		if(mask & int_table[i].int_mask)
			(*int_table[i].int_hand)(int_table[i].param);
	}
	return(~0);  /* Dont reenable */
}

/*
 * Handle riscbus interval clock interrupt.
 */
int
riscbus_clkintr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int temp;

	switch (cputype) {

	case WGRISC9100:
		if(dpclock_interrupt())
			 hardclock(cf);	
		break;
	}

	/* Re-enable clock interrupts */
	splx(int_table[0].int_mask | SR_INT_ENAB);

	return(~int_table[0].int_mask); /* Keep clock interrupts enabled */
}


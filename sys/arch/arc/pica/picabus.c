/*	$OpenBSD: picabus.c,v 1.10 1998/03/16 09:38:52 pefo Exp $	*/
/*	$NetBSD: tc.c,v 1.2 1995/03/08 00:39:05 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * Author: Per Fogelstrom. (Mips R4x00)
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/autoconf.h>

#include <mips/archtype.h>
#include <arc/pica/pica.h>
#include <arc/dev/dma.h>

struct pica_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	pica_dev *sc_devs;
};

/* Definition of the driver for autoconfig. */
int	picamatch(struct device *, void *, void *);
void	picaattach(struct device *, struct device *, void *);
int	picaprint(void *, const char *);

struct cfattach pica_ca = {
	sizeof(struct pica_softc), picamatch, picaattach
};
struct cfdriver pica_cd = {
	NULL, "pica", DV_DULL, NULL, 0
};

void	pica_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	pica_intr_disestablish __P((struct confargs *));
caddr_t	pica_cvtaddr __P((struct confargs *));
int	pica_matchname __P((struct confargs *, char *));
int	pica_iointr __P((unsigned int, struct clockframe *));
int	pica_clkintr __P((unsigned int, struct clockframe *));

/*
 *  Interrupt dispatch table.
 */
struct pica_int_desc int_table[] = {
	{0, pica_intrnull, (void *)NULL, 0 },  /*  0 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  1 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  2 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  3 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  4 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  5 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  6 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  7 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  8 */
	{0, pica_intrnull, (void *)NULL, 0 },  /*  9 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 10 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 11 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 12 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 13 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 14 */
	{0, pica_intrnull, (void *)NULL, 0 },  /* 15 */
};

struct pica_dev {
	struct confargs	ps_ca;
	u_int		ps_mask;
	intr_handler_t	ps_handler;
	void 		*ps_base;
};
struct pica_dev acer_pica_61_cpu[] = {
	{{ "dallas_rtc",0, 0, },
	   0,			 pica_intrnull, (void *)PICA_SYS_CLOCK, },
	{{ "lpt",	1, 0, },
	   PICA_SYS_LB_IE_PAR1,	 pica_intrnull, (void *)PICA_SYS_PAR1, },
	{{ "fdc",	2, 0, },
	   PICA_SYS_LB_IE_FLOPPY,pica_intrnull, (void *)PICA_SYS_FLOPPY, },
	{{ NULL,	3, NULL, },
	   0, pica_intrnull, (void *)NULL, },
	{{ NULL,	4, NULL, },
	   0, pica_intrnull, (void *)NULL, },
	{{ "sonic",	5, 0, },
	   PICA_SYS_LB_IE_SONIC, pica_intrnull, (void *)PICA_SYS_SONIC, },
	{{ "asc",	6, 0, },
	   PICA_SYS_LB_IE_SCSI,  pica_intrnull, (void *)PICA_SYS_SCSI, },
	{{ "pc",	7, 0, },
	   PICA_SYS_LB_IE_KBD,	 pica_intrnull, (void *)PICA_SYS_KBD, },
	{{ "pms",	8, NULL, },
	   PICA_SYS_LB_IE_MOUSE, pica_intrnull, (void *)PICA_SYS_KBD, },
	{{ "com",	9, 0, },
	   PICA_SYS_LB_IE_COM1,	 pica_intrnull, (void *)PICA_SYS_COM1, },
	{{ "com",      10, 0, },
	   PICA_SYS_LB_IE_COM2,	 pica_intrnull, (void *)PICA_SYS_COM2, },
	{{ NULL,       -1, NULL, },
	   0, NULL, (void *)NULL, },
};

struct pica_dev *pica_cpu_devs[] = {
        NULL,                   /* Unused */
        acer_pica_61_cpu,       /* Acer PICA */
        acer_pica_61_cpu,       /* MAGNUMS same as Acer PICA */
	NULL,
	NULL,
	NULL,
        acer_pica_61_cpu,       /* NEC-R94 same as MAGNUM */
};
int npica_cpu_devs = sizeof pica_cpu_devs / sizeof pica_cpu_devs[0];

int local_int_mask = 0;	/* Local interrupt enable mask */

int
picamatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

        /* Make sure that we're looking for a PICA. */
        if (strcmp(ca->ca_name, pica_cd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 ||
	    system_type > npica_cpu_devs || pica_cpu_devs[system_type] == NULL)
		return (0);

	return (1);
}

void
picaattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pica_softc *sc = (struct pica_softc *)self;
	struct confargs *nca;
	int i;

	printf("\n");

	/* keep our CPU device description handy */
	sc->sc_devs = pica_cpu_devs[system_type];

	/* set up interrupt handlers */
	set_intr(INT_MASK_1, pica_iointr, 2);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_PICA;
	sc->sc_bus.ab_intr_establish = pica_intr_establish;
	sc->sc_bus.ab_intr_disestablish = pica_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = pica_cvtaddr;
	sc->sc_bus.ab_matchname = pica_matchname;

	/* Initialize PICA Dma */
	picaDmaInit();

	/* Try to configure each PICA attached device */
	for (i = 0; sc->sc_devs[i].ps_ca.ca_slot >= 0; i++) {

		if(sc->sc_devs[i].ps_ca.ca_name == NULL)
			continue; /* Empty slot */

		nca = &sc->sc_devs[i].ps_ca;
		nca->ca_bus = &sc->sc_bus;

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, picaprint);
	}
}

int
picaprint(aux, pnp)
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
pica_cvtaddr(ca)
	struct confargs *ca;
{
	struct pica_softc *sc = pica_cd.cd_devs[0];

	return(sc->sc_devs[ca->ca_slot].ps_base + ca->ca_offset);

}

void
pica_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
	void *val;
{
	struct pica_softc *sc = pica_cd.cd_devs[0];

	int slot;

	slot = ca->ca_slot;
	if(slot == 0) {		/* Slot 0 is special, clock */
		set_intr(INT_MASK_4, pica_clkintr, 1);
	}

	if(int_table[slot].int_mask != 0) {
		panic("pica intr already set");
	}
	else {
		int_table[slot].int_mask = sc->sc_devs[slot].ps_mask;;
		local_int_mask |= int_table[slot].int_mask;
		int_table[slot].int_hand = handler;
		int_table[slot].param = val;
	}
	out16(PICA_SYS_LB_IE, local_int_mask);
}

void
pica_intr_disestablish(ca)
	struct confargs *ca;
{
	int slot;

	slot = ca->ca_slot;
	if(slot != 0)		 {	/* Slot 0 is special, clock */
		local_int_mask &= ~int_table[slot].int_mask;
		int_table[slot].int_mask = 0;
		int_table[slot].int_hand = pica_intrnull;
		int_table[slot].param = (void *)NULL;
	}
}

int
pica_matchname(ca, name)
	struct confargs *ca;
	char *name;
{
	return (strcmp(name, ca->ca_name) == 0);
}

int
pica_intrnull(val)
	void *val;
{
	panic("uncaught PICA intr for slot %d\n", val);
}

/*
 *   Handle pica i/o interrupt.
 */
int
pica_iointr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int vector;

	while((vector = inb(PVIS) >> 2) != 0) {
		(*int_table[vector].int_hand)(int_table[vector].param);
	}
	return(~0);  /* Dont reenable */
}

/*
 * Handle pica interval clock interrupt.
 */
int
pica_clkintr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int temp;

	temp = inw(R4030_SYS_IT_STAT);
	hardclock(cf);

	/* Re-enable clock interrupts */
	splx(INT_MASK_4 | SR_INT_ENAB);

	return(~INT_MASK_4); /* Keep clock interrupts enabled */
}


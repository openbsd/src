/*	$OpenBSD: desktechbus.c,v 1.2 1996/09/14 15:58:24 pefo Exp $ */

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

#include <arc/arc/arctype.h>
#include <arc/dti/desktech.h>

struct dti_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	dti_dev *sc_devs;
};

/* Definition of the driver for autoconfig. */
int	dtimatch(struct device *, void *, void *);
void	dtiattach(struct device *, struct device *, void *);
int	dtiprint(void *, char *);

struct cfattach dti_ca = {
	sizeof(struct dti_softc), dtimatch, dtiattach
};
struct cfdriver dti_cd = {
	NULL, "dti", DV_DULL, NULL, 0
};

void	dti_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	dti_intr_disestablish __P((struct confargs *));
caddr_t	dti_cvtaddr __P((struct confargs *));
int	dti_matchname __P((struct confargs *, char *));
int	dti_iointr __P((void *));
int	dti_clkintr __P((unsigned, unsigned, unsigned, unsigned));

extern int cputype;

/*
 *  Interrupt dispatch table.
 */
struct dti_int_desc int_table[] = {
	{0, dti_intrnull, (void *)NULL, 0 },  /*  0 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  1 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  2 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  3 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  4 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  5 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  6 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  7 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  8 */
	{0, dti_intrnull, (void *)NULL, 0 },  /*  9 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 10 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 11 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 12 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 13 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 14 */
	{0, dti_intrnull, (void *)NULL, 0 },  /* 15 */
};

struct dti_dev {
	struct confargs	ps_ca;
	u_int		ps_mask;
	intr_handler_t	ps_handler;
	void 		*ps_base;
};
#ifdef ACER_PICA_61
struct dti_dev acer_dti_61_cpu[] = {
	{{ "dallas_rtc",0, 0, },
	   0,			 dti_intrnull, (void *)PICA_SYS_CLOCK, },
	{{ "lpr",	1, 0, },
	   PICA_SYS_LB_IE_PAR1,	 dti_intrnull, (void *)PICA_SYS_PAR1, },
	{{ "fdc",	2, 0, },
	   PICA_SYS_LB_IE_FLOPPY,dti_intrnull, (void *)PICA_SYS_FLOPPY, },
	{{ NULL,	3, NULL, },
	   0, dti_intrnull, (void *)NULL, },
	{{ NULL,	4, NULL, },
	   0, dti_intrnull, (void *)NULL, },
	{{ "sonic",	5, 0, },
	   PICA_SYS_LB_IE_SONIC, dti_intrnull, (void *)PICA_SYS_SONIC, },
	{{ "asc",	6, 0, },
	   PICA_SYS_LB_IE_SCSI,  dti_intrnull, (void *)PICA_SYS_SCSI, },
	{{ "pc",	7, 0, },
	   PICA_SYS_LB_IE_KBD,	 dti_intrnull, (void *)PICA_SYS_KBD, },
	{{ "pms",	8, NULL, },
	   PICA_SYS_LB_IE_MOUSE, dti_intrnull, (void *)PICA_SYS_KBD, },
	{{ "com",	9, 0, },
	   PICA_SYS_LB_IE_COM1,	 dti_intrnull, (void *)PICA_SYS_COM1, },
	{{ "com",      10, 0, },
	   PICA_SYS_LB_IE_COM2,	 dti_intrnull, (void *)PICA_SYS_COM2, },
	{{ NULL,       -1, NULL, },
	   0, NULL, (void *)NULL, },
};
#endif

struct dti_dev *dti_cpu_devs[] = {
        NULL,                   /* Unused */
#ifdef ACER_PICA_61
        acer_dti_61_cpu,       /* Acer PICA */
#else
	NULL,
#endif
};
int ndti_cpu_devs = sizeof dti_cpu_devs / sizeof dti_cpu_devs[0];

int local_int_mask = 0;	/* Local interrupt enable mask */

int
dtimatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

        /* Make sure that we're looking for a PICA. */
        if (strcmp(ca->ca_name, dti_cd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 ||
	    cputype > ndti_cpu_devs || dti_cpu_devs[cputype] == NULL)
		return (0);

	return (1);
}

void
dtiattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct dti_softc *sc = (struct dti_softc *)self;
	struct confargs *nca;
	int i;

	printf("\n");

	/* keep our CPU device description handy */
	sc->sc_devs = dti_cpu_devs[cputype];

	/* set up interrupt handlers */
	set_intr(INT_MASK_1, dti_iointr, 2);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_PICA;
	sc->sc_bus.ab_intr_establish = dti_intr_establish;
	sc->sc_bus.ab_intr_disestablish = dti_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = dti_cvtaddr;
	sc->sc_bus.ab_matchname = dti_matchname;

	/* Initialize PICA Dma */
	dtiDmaInit();

	/* Try to configure each PICA attached device */
	for (i = 0; sc->sc_devs[i].ps_ca.ca_slot >= 0; i++) {

		if(sc->sc_devs[i].ps_ca.ca_name == NULL)
			continue; /* Empty slot */

		nca = &sc->sc_devs[i].ps_ca;
		nca->ca_bus = &sc->sc_bus;

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, dtiprint);
	}
}

int
dtiprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct confargs *ca = aux;

        if (pnp)
                printf("%s at %s", ca->ca_name, pnp);
        printf(" slot %ld offset 0x%lx", ca->ca_slot, ca->ca_offset);
        return (UNCONF);
}

caddr_t
dti_cvtaddr(ca)
	struct confargs *ca;
{
	struct dti_softc *sc = dti_cd.cd_devs[0];

	return(sc->sc_devs[ca->ca_slot].ps_base + ca->ca_offset);

}

void
dti_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
	void *val;
{
	struct dti_softc *sc = dti_cd.cd_devs[0];

	int slot;

	slot = ca->ca_slot;
	if(slot == 0) {		/* Slot 0 is special, clock */
		set_intr(INT_MASK_4, dti_clkintr, 1);
	}

	if(int_table[slot].int_mask != 0) {
		panic("dti intr already set");
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
dti_intr_disestablish(ca)
	struct confargs *ca;
{
	struct dti_softc *sc = dti_cd.cd_devs[0];

	int slot;

	slot = ca->ca_slot;
	if(slot = 0) {		/* Slot 0 is special, clock */
	}
	else {
		local_int_mask &= ~int_table[slot].int_mask;
		int_table[slot].int_mask = 0;
		int_table[slot].int_hand = dti_intrnull;
		int_table[slot].param = (void *)NULL;
	}
}

int
dti_matchname(ca, name)
	struct confargs *ca;
	char *name;
{
	return (strcmp(name, ca->ca_name) == 0);
}

int
dti_intrnull(val)
	void *val;
{
	panic("uncaught PICA intr for slot %d\n", val);
}

/*
 *   Handle dti i/o interrupt.
 */
int
dti_iointr(val)
	void *val;
{
	int vector;

	while((vector = inb(PVIS) >> 2) != 0) {
		(*int_table[vector].int_hand)(int_table[vector].param);
	}
	return(~0);  /* Dont reenable */
}

/*
 * Handle dti interval clock interrupt.
 */
int
dti_clkintr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	struct clockframe cf;
	int temp;

	temp = inw(R4030_SYS_IT_STAT);
	cf.pc = pc;
	cf.sr = statusReg;
	hardclock(&cf);

	/* Re-enable clock interrupts */
	splx(INT_MASK_4 | SR_INT_ENAB);

	return(~INT_MASK_4); /* Keep clock interrupts enabled */
}


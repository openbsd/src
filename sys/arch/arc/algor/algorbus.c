/*	$OpenBSD: algorbus.c,v 1.4 1998/01/29 14:54:45 pefo Exp $ */

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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <arc/arc/arctype.h>
#include <arc/algor/algor.h>

#include <dev/ic/mc146818reg.h>

struct algor_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	algor_dev *sc_devs;
};

/* Definition of the driver for autoconfig. */
int	algormatch(struct device *, void *, void *);
void	algorattach(struct device *, struct device *, void *);
int	algorprint(void *, const char *);

struct cfattach algor_ca = {
	sizeof(struct algor_softc), algormatch, algorattach
};
struct cfdriver algor_cd = {
	NULL, "algor", DV_DULL, NULL, 0
};

void	algor_intr_establish __P((struct confargs *, int (*)(void *), void *));
void	algor_intr_disestablish __P((struct confargs *));
caddr_t	algor_cvtaddr __P((struct confargs *));
int	algor_matchname __P((struct confargs *, char *));
int	algor_iointr __P((unsigned, struct clockframe *));
int	algor_clkintr __P((unsigned, struct clockframe *));
int	algor_errintr __P((unsigned, struct clockframe *));

int p4032_imask = 0;
int p4032_ixr = 0;

/*
 *  Interrupt dispatch table.
 */
static struct algor_int_desc int_table[] = {
	{0, algor_intrnull, (void *)NULL, 0 },  /*  0 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  1 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  2 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  3 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  4 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  5 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  6 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  7 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  8 */
	{0, algor_intrnull, (void *)NULL, 0 },  /*  9 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 10 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 11 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 12 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 13 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 14 */
	{0, algor_intrnull, (void *)NULL, 0 },  /* 15 */
};
#define NUM_INT_SLOTS (sizeof(int_table) / sizeof(struct algor_int_desc))

struct algor_dev {
	struct confargs	ps_ca;
	u_int8_t	ps_mask;
	u_int8_t	ps_ipl;
	u_int16_t	ps_route;
	intr_handler_t	ps_handler;
	void 		*ps_base;
};
struct algor_dev algor_4032_cpu[] = {
    {{ "dallas_rtc",	 0, 0, },
       P4032_IM_RTC,  IPL_CLOCK, 0xc000, algor_intrnull, (void *)P4032_CLOCK, },
    {{ "com",		 1, 0, },
       P4032_IM_COM1, IPL_TTY,   0x00c0, algor_intrnull, (void *)P4032_COM1,  },
    {{ "com",      	 2, 0, },
       P4032_IM_COM2, IPL_TTY,   0x0300, algor_intrnull, (void *)P4032_COM2,  },
    {{ "lpt",      	 3, 0, },
       P4032_IM_CENTR,IPL_TTY,   0x0c00, algor_intrnull, (void *)P4032_CENTR, },
    {{ NULL,     	 -1, NULL, },
       0, 		0x0000,	NULL,		(void *)NULL, },
};
#define NUM_ALGOR_DEVS (sizeof(algor_4032_cpu) / sizeof(struct algor_dev))

/* IPL routing values */
static int ipxrtab[] = {
	0x000000,	/* IPL_BIO */
	0x555555,	/* IPL_NET */
	0xaaaaaa,	/* IPL_TTY */
	0xffffff,	/* IPL_CLOCK */
};
	


struct algor_dev *algor_cpu_devs[] = {
	NULL,			/* Unused */
	NULL,			/* Unused */
	NULL,			/* Unused */
	NULL,			/* Unused */
	NULL,			/* Unused */
	NULL,			/* Unused */
	algor_4032_cpu,		/* 6 = ALGORITHMICS R4032 Board */
	NULL,
};
int nalgor_cpu_devs = sizeof algor_cpu_devs / sizeof algor_cpu_devs[0];

int
algormatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

        /* Make sure that we're looking for a ALGORITHMICS BUS */
        if (strcmp(ca->ca_name, algor_cd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 || system_type > nalgor_cpu_devs
	    || algor_cpu_devs[system_type] == NULL)
		return (0);

	return (1);
}

void
algorattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct algor_softc *sc = (struct algor_softc *)self;
	struct confargs *nca;
	int i;

	printf("\n");

	/* keep our CPU device description handy */
	sc->sc_devs = algor_cpu_devs[system_type];

	/* set up interrupt handlers */
	set_intr(INT_MASK_1, algor_iointr, 2);
	set_intr(INT_MASK_4, algor_errintr, 0);

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_ALGOR;
	sc->sc_bus.ab_intr_establish = algor_intr_establish;
	sc->sc_bus.ab_intr_disestablish = algor_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = algor_cvtaddr;
	sc->sc_bus.ab_matchname = algor_matchname;

	/* Try to configure each ALGOR attached device */
	for (i = 0; sc->sc_devs[i].ps_ca.ca_slot >= 0; i++) {

		if(sc->sc_devs[i].ps_ca.ca_name == NULL)
			continue; /* Empty slot */

		nca = &sc->sc_devs[i].ps_ca;
		nca->ca_bus = &sc->sc_bus;

		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, algorprint);
	}
}

int
algorprint(aux, pnp)
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
algor_cvtaddr(ca)
	struct confargs *ca;
{
	struct algor_softc *sc = algor_cd.cd_devs[0];

	return(sc->sc_devs[ca->ca_slot].ps_base + ca->ca_offset);

}

void
algor_intr_establish(ca, handler, arg)
	struct confargs *ca;
	intr_handler_t handler;
	void *arg;
{
	struct algor_softc *sc = algor_cd.cd_devs[0];
	int slot = ca->ca_slot;
	struct algor_dev *dev = &sc->sc_devs[slot];
	int ipl = dev->ps_ipl;

	if(int_table[slot].int_mask != 0) {
		panic("algor intr already set");
	}
	else {
		int_table[slot].int_mask = dev->ps_mask;
		int_table[slot].int_hand = handler;
		int_table[slot].param = arg;
	}
	p4032_ixr |= ipxrtab[ipl] & dev->ps_route;
	outb(P4032_IXR0, p4032_ixr);
	outb(P4032_IXR1, p4032_ixr >> 8);
	outb(P4032_IXR2, p4032_ixr >> 16);

	if(slot == 0) {		/* Slot 0 is special, clock */
		set_intr(INT_MASK_0 << ipl, algor_clkintr, ipl + 1);
	}
	else {
		set_intr(INT_MASK_0 << ipl, algor_iointr, ipl + 1);
	}

	p4032_imask |= dev->ps_mask;
	outb(P4032_IMR, p4032_imask);
	outb(P4032_PCIIMR, p4032_imask >> 8);
}

void *
algor_pci_intr_establish(ih, level, handler, arg, name)
	int ih;
	int level;
	intr_handler_t handler;
	void *arg;
	void *name;
{
	int imask;
	int route;
	int slot;

	if(level < IPL_BIO || level >= IPL_CLOCK) {
		panic("pci intr: ipl level out of range");
	}
	if(ih < 0 || ih >= 4) {
		panic("pci intr: irq out of range");
	}

	imask = (0x1000 << ih);
	route = (0x30000 << (ih+ih));

	slot = NUM_INT_SLOTS;
	while(slot > 0) {
		if(int_table[slot].int_mask == 0)
			break;
		slot--;
	}
	if(slot < 0) {
		panic("pci intr: out of int slots");
	}

	int_table[slot].int_mask = imask;
	int_table[slot].int_hand = handler;
	int_table[slot].param = arg;

	p4032_ixr |= ipxrtab[level] & route;
	outb(P4032_IXR0, p4032_ixr);
	outb(P4032_IXR1, p4032_ixr >> 8);
	outb(P4032_IXR2, p4032_ixr >> 16);

	set_intr(INT_MASK_0 << level, algor_iointr, level + 1);

	p4032_imask |= imask;
	outb(P4032_IMR, p4032_imask);
	outb(P4032_PCIIMR, p4032_imask >> 8);

	return((void *)slot);
}

void
algor_intr_disestablish(ca)
	struct confargs *ca;
{
	int slot;

	slot = ca->ca_slot;
	p4032_imask &= ~int_table[slot].int_mask;
	outb(P4032_IMR, p4032_imask);
	outb(P4032_PCIIMR, p4032_imask >> 8);

	if(slot != 0) {		/* Slot 0 is special, clock */
		int_table[slot].int_mask = 0;
		int_table[slot].int_hand = algor_intrnull;
		int_table[slot].param = (void *)NULL;
	}
}

void
algor_pci_intr_disestablish(cookie)
	void *cookie;
{
	int slot = (int)cookie;

	p4032_imask &= ~int_table[slot].int_mask;
	outb(P4032_IMR, p4032_imask);
	outb(P4032_PCIIMR, p4032_imask >> 8);

	int_table[slot].int_mask = 0;
	int_table[slot].int_hand = algor_intrnull;
	int_table[slot].param = (void *)NULL;
}

int
algor_matchname(ca, name)
	struct confargs *ca;
	char *name;
{
	return (strcmp(name, ca->ca_name) == 0);
}

int
algor_intrnull(val)
	void *val;
{
	panic("uncaught ALGOR intr for slot %d\n", val);
}

/*
 *   Handle algor i/o interrupt.
 */
int
algor_iointr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int i;
	int pend;

	pend  = inb(P4032_IRR);
	pend |= inb(P4032_PCIIRR) << 8;
	pend &= p4032_imask;

	for(i = 0; i < NUM_INT_SLOTS; i++) {
		if(pend & int_table[i].int_mask)
			(*int_table[i].int_hand)(int_table[i].param);
	}
	outb(P4032_ICR, pend & P4032_IM_CENTR);	/* Ack any centronics int */
	return(~0);  /* Dont reenable */
}

/*
 * Handle algor interval clock interrupt.
 */
int
algor_clkintr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	/* Ack clock interrupt */
	outb(P4032_CLOCK, MC_REGC);
	(void) inb(P4032_CLOCK + 4);

	hardclock(cf);

	/* Re-enable clock interrupts */
	splx(INT_MASK_0 << IPL_CLOCK | SR_INT_ENAB);

	return(~(INT_MASK_0 << IPL_CLOCK)); /* Keep clock interrupts enabled */
}

/*
 * Handle algor interval clock interrupt.
 */
int
algor_errintr(mask, cf)
	unsigned mask;
	struct clockframe *cf;
{
	int why;

	why = inb(P4032_EIRR);

	if(why & P4032_IRR_BER) {
		printf("Bus error interrupt\n");
		outb(P4032_ICR, P4032_IRR_BER);
	}
	if(why & P4032_IRR_PFAIL) {
		printf("Power failure!\n");
	}
	if(why & P4032_IRR_DBG) {
		printf("Debug switch\n");
		outb(P4032_ICR, P4032_IRR_DBG);
#ifdef DEBUG
		mdbpanic();
#else
		printf("Not DEBUG compiled, sorry!\n");
#endif
	}
	return(~0);
}

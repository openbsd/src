/*	$NetBSD: ioasic.c,v 1.1 1995/12/20 00:43:20 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/rpb.h>

#include <dev/tc/tcvar.h>
#include <alpha/tc/ioasicreg.h>
#include <dev/tc/ioasicvar.h>

struct ioasic_softc {
	struct	device sc_dv;

	tc_addr_t sc_base;
	void	*sc_cookie;
};

/* Definition of the driver for autoconfig. */
int	ioasicmatch __P((struct device *, void *, void *));
void	ioasicattach __P((struct device *, struct device *, void *));
int     ioasicprint(void *, char *);
struct cfdriver ioasiccd =
    { NULL, "ioasic", ioasicmatch, ioasicattach, DV_DULL,
    sizeof(struct ioasic_softc) };

int	ioasic_intr __P((void *));
int	ioasic_intrnull __P((void *));

#define	C(x)	((void *)(x))

#define	IOASIC_DEV_LANCE	0
#define	IOASIC_DEV_SCC0		1
#define	IOASIC_DEV_SCC1		2
#define	IOASIC_DEV_ISDN		3

#define	IOASIC_DEV_BOGUS	-1

#define	IOASIC_NCOOKIES		4

struct ioasic_dev {
	char		*iad_modname;
	tc_offset_t	iad_offset;
	void		*iad_cookie;
	u_int32_t	iad_intrbits;
} ioasic_devs[] = {
	{ "lance   ", 0x000c0000, C(IOASIC_DEV_LANCE), IOASIC_INTR_LANCE, },
	{ "z8530   ", 0x00100000, C(IOASIC_DEV_SCC0),  IOASIC_INTR_SCC_0, },
	{ "z8530   ", 0x00180000, C(IOASIC_DEV_SCC1),  IOASIC_INTR_SCC_1, },
	{ "TOY_RTC ", 0x00200000, C(IOASIC_DEV_BOGUS), 0,                 },
	{ "AMD79c30", 0x00240000, C(IOASIC_DEV_ISDN),  IOASIC_INTR_ISDN,  },
};
int ioasic_ndevs = sizeof(ioasic_devs) / sizeof(ioasic_devs[0]);

struct ioasicintr {
	int	(*iai_func) __P((void *));
	void	*iai_arg;
} ioasicintrs[IOASIC_NCOOKIES];

tc_addr_t ioasic_base;		/* XXX XXX XXX */

/* There can be only one. */
int ioasicfound;

extern int cputype;

int
ioasicmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct tcdev_attach_args *tcdev = aux;

	/* Make sure that we're looking for this type of device. */
	if (strncmp("FLAMG-IO", tcdev->tcda_modname, TC_ROM_LLEN))
		return (0);

	/* Check that it can actually exist. */
	if ((cputype != ST_DEC_3000_500) && (cputype != ST_DEC_3000_300))
		panic("ioasicmatch: how did we get here?");

	if (ioasicfound)
		return (0);

	return (1);
}

void
ioasicattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasic_softc *sc = (struct ioasic_softc *)self;
	struct tcdev_attach_args *tcdev = aux;
	struct ioasicdev_attach_args ioasicdev;
	u_long i;

	ioasicfound = 1;

	sc->sc_base = tcdev->tcda_addr;
	ioasic_base = sc->sc_base;			/* XXX XXX XXX */
	sc->sc_cookie = tcdev->tcda_cookie;

#ifdef DEC_3000_300
	if (cputype == ST_DEC_3000_300) {
		*(volatile u_int *)IOASIC_REG_CSR(sc->sc_base) |=
		    IOASIC_CSR_FASTMODE;
		tc_mb();
		printf(": slow mode\n");
	} else
#endif
		printf(": fast mode\n");

	/*
	 * Turn off all device interrupt bits.
	 * (This does _not_ include 3000/300 TC option slot bits.
	 */
	for (i = 0; i < ioasic_ndevs; i++)
		*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
			~ioasic_devs[i].iad_intrbits;
	tc_mb();

	/*
	 * Set up interrupt handlers.
	 */
	for (i = 0; i < IOASIC_NCOOKIES; i++) {
		ioasicintrs[i].iai_func = ioasic_intrnull;
		ioasicintrs[i].iai_arg = (void *)i;
	}
	tc_intr_establish(parent, sc->sc_cookie, TC_IPL_NONE, ioasic_intr, sc);

        /*
	 * Try to configure each device.
	 */
        for (i = 0; i < ioasic_ndevs; i++) {
		strncpy(ioasicdev.iada_modname, ioasic_devs[i].iad_modname,
			TC_ROM_LLEN);
		ioasicdev.iada_modname[TC_ROM_LLEN] = '\0';
		ioasicdev.iada_offset = ioasic_devs[i].iad_offset;
		ioasicdev.iada_addr = sc->sc_base + ioasic_devs[i].iad_offset;
		ioasicdev.iada_cookie = ioasic_devs[i].iad_cookie;

                /* Tell the autoconfig machinery we've found the hardware. */
                config_found(self, &ioasicdev, ioasicprint);
        }
}

int
ioasicprint(aux, pnp)
	void *aux;
	char *pnp;
{
	struct ioasicdev_attach_args *d = aux;

        if (pnp)
                printf("%s at %s", d->iada_modname, pnp);
        printf(" offset 0x%lx", (long)d->iada_offset);
        return (UNCONF);
}

int
ioasic_submatch(match, d)
	struct cfdata *match;
	struct ioasicdev_attach_args *d;
{

	return ((match->ioasiccf_offset == d->iada_offset) ||
		(match->ioasiccf_offset == IOASIC_OFFSET_UNKNOWN));
}

void
ioasic_intr_establish(ioa, cookie, level, func, arg)
	struct device *ioa;
	void *cookie, *arg;
	tc_intrlevel_t level;
	int (*func) __P((void *));
{
	u_long dev, i;

	dev = (u_long)cookie;
#ifdef DIAGNOSTIC
	/* XXX check cookie. */
#endif

	if (ioasicintrs[dev].iai_func != ioasic_intrnull)
		panic("ioasic_intr_establish: cookie %d twice", dev);

	ioasicintrs[dev].iai_func = func;
	ioasicintrs[dev].iai_arg = arg;

	/* Enable interrupts for the device. */
	for (i = 0; i < ioasic_ndevs; i++)
		if (ioasic_devs[i].iad_cookie == cookie)
			break;
	if (i == ioasic_ndevs)
		panic("ioasic_intr_establish: invalid cookie.");
	*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) |=
		ioasic_devs[i].iad_intrbits;
	tc_mb();
}

void
ioasic_intr_disestablish(ioa, cookie)
	struct device *ioa;
	void *cookie;
{
	u_long dev, i;

	dev = (u_long)cookie;
#ifdef DIAGNOSTIC
	/* XXX check cookie. */
#endif

	if (ioasicintrs[dev].iai_func == ioasic_intrnull)
		panic("ioasic_intr_disestablish: cookie %d missing intr", dev);

	/* Enable interrupts for the device. */
	for (i = 0; i < ioasic_ndevs; i++)
		if (ioasic_devs[i].iad_cookie == cookie)
			break;
	if (i == ioasic_ndevs)
		panic("ioasic_intr_disestablish: invalid cookie.");
	*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
		~ioasic_devs[i].iad_intrbits;
	tc_mb();

	ioasicintrs[dev].iai_func = ioasic_intrnull;
	ioasicintrs[dev].iai_arg = (void *)dev;
}

int
ioasic_intrnull(val)
	void *val;
{

	panic("ioasic_intrnull: uncaught IOASIC intr for cookie %ld\n",
	    (u_long)val);
}

/*
 * asic_intr --
 *	ASIC interrupt handler.
 */
int
ioasic_intr(val)
	void *val;
{
	register struct ioasic_softc *sc = val;
	register int i, ifound;
	int gifound;
	u_int32_t sir, junk;
	volatile u_int32_t *sirp, *junkp;

	sirp = (volatile u_int32_t *)IOASIC_REG_INTR(sc->sc_base);

	gifound = 0;
	do {
		ifound = 0;
		tc_syncbus();

		sir = *sirp;

		/* XXX DUPLICATION OF INTERRUPT BIT INFORMATION... */
#define	CHECKINTR(slot, bits)						\
		if (sir & bits) {					\
			ifound = 1;					\
			(*ioasicintrs[slot].iai_func)			\
			    (ioasicintrs[slot].iai_arg);		\
		}
		CHECKINTR(IOASIC_DEV_SCC0, IOASIC_INTR_SCC_0);
		CHECKINTR(IOASIC_DEV_SCC1, IOASIC_INTR_SCC_1);
		CHECKINTR(IOASIC_DEV_LANCE, IOASIC_INTR_LANCE);
		CHECKINTR(IOASIC_DEV_ISDN, IOASIC_INTR_ISDN);

		gifound |= ifound;
	} while (ifound);

	return (gifound);
}

/* XXX */
char *
ioasic_lance_ether_address()
{

	return (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);
}

void
ioasic_lance_dma_setup(v)
	void *v;
{
	volatile u_int32_t *ldp;
	tc_addr_t tca;

	tca = (tc_addr_t)v;

	ldp = (volatile u_int *)IOASIC_REG_LANCE_DMAPTR(ioasic_base);
	*ldp = ((tca << 3) & ~(tc_addr_t)0x1f) | ((tca >> 29) & 0x1f);
	tc_wmb();

	*(volatile u_int32_t *)IOASIC_REG_CSR(ioasic_base) |=
	    IOASIC_CSR_DMAEN_LANCE;
	tc_mb();
}

#ifdef DEC_3000_300
void
ioasic_intr_300_opt0_enable(enable)
	int enable;
{

	if (enable)
		*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) |=
			IOASIC_INTR_300_OPT0;
	else
		*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
			~IOASIC_INTR_300_OPT0;
}

void
ioasic_intr_300_opt1_enable(enable)
	int enable;
{

	if (enable)
		*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) |=
			IOASIC_INTR_300_OPT1;
	else
		*(volatile u_int32_t *)IOASIC_REG_IMSK(ioasic_base) &=
			~IOASIC_INTR_300_OPT1;
}

void
ioasic_300_opts_isintr(opt0, opt1)
	int *opt0, *opt1;
{
	u_int32_t sir;

	sir = *(volatile u_int32_t *)IOASIC_REG_INTR(ioasic_base);
	*opt0 = sir & IOASIC_INTR_300_OPT0;
	*opt1 = sir & IOASIC_INTR_300_OPT1;
}
#endif

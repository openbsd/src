/*	$NetBSD: tc.c,v 1.7 1996/01/03 20:39:10 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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
#include <sys/device.h>
#include <dev/cons.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>

#ifdef alpha
#include <machine/rpb.h>
#endif

/* Which TC framebuffers have drivers, for configuring a console device. */
#include <cfb.h>
#include <mfb.h>
#include <sfb.h>

extern int pmax_boardtype;


struct tc_softc {
	struct	device sc_dv;
	int	sc_nslots;
	struct tc_slotdesc *sc_slots;

	void	(*sc_intr_establish) __P((struct device *, void *,
		    tc_intrlevel_t, int (*)(void *), void *));
	void	(*sc_intr_disestablish) __P((struct device *, void *));
#ifndef goneverysoon
	struct	abus sc_bus;
	struct	tc_cpu_desc *sc_desc;
#endif /* goneverysoon */
};

/*
 * Old-style model-specific autoconfiguration description.
 */
struct tc_cpu_desc {
	struct	tc_slotdesc *tcd_slots;
	long	tcd_nslots;
	struct	confargs *tcd_devs;
	long	tcd_ndevs;
	void	(*tc_intr_setup) __P((void));
	void	(*tc_intr_establish) __P((struct device *dev, void *cookie,
			 int level, intr_handler_t handler, void *arg));
	void	(*tc_intr_disestablish) __P((struct device *, void *));
	int	(*tc_iointr) __P((u_int mask, u_int pc,
				  u_int statusReg, u_int causeReg));
};

/* Return the appropriate tc_cpu_desc for a given cputype */
extern struct tc_cpu_desc *  cpu_tcdesc __P ((int cputype));


/* Definition of the driver for autoconfig. */
int	tcmatch(struct device *, void *, void *);
void	tcattach(struct device *, struct device *, void *);
int	tcprint(void *, char *);
struct cfdriver tccd =
    { NULL, "tc", tcmatch, tcattach, DV_DULL, sizeof (struct tc_softc) };

void	tc_intr_establish __P((struct device *, void *, tc_intrlevel_t,
				intr_handler_t handler, intr_arg_t arg));
void	tc_intr_disestablish __P((struct device *dev, void *cookie));
caddr_t	tc_cvtaddr __P((struct confargs *));
int	tc_matchname __P((struct confargs *, char *));

extern int cputype;
extern int tc_findconsole __P((int prom_slot));

/* Forward declarations */
int consprobeslot __P((int slot));


/*XXX*/ /* should be in separate source file  */

/*
 *  tc config structures for DECstations.
 *  Since there will never be new decstations, we just
 *  bash it in here, for now.
 */

#include <machine/machConst.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/trap.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>

#include <pmax/pmax/turbochannel.h>

/*#include <pmax/pmax/nameglue.h>*/
#define KV(x) ((tc_addr_t)MACH_PHYS_TO_UNCACHED(x))



void	tc_ds_ioasic_intr_setup __P((void));
void	tc_ds_ioasic_intr_establish __P((struct device *dev, void *cookie,
			 int level, intr_handler_t handler, void *arg));
void	tc_ds_ioasic_intr_disestablish __P((struct device *, void *));
void	tc_ds_ioasic_iointr __P((void *, int));
int	tc_ds_ioasic_getdev __P((struct confargs *));


/* XXX*/
/* should be handled elsewhere? */
typedef void (*tc_enable_t) __P ((u_int slotno, intr_handler_t,
				  void *intr_arg, int on)); 
typedef int (*tc_handler_t) __P((void *intr_arg));
    
extern void (*tc_enable_interrupt)  __P ((u_int slotno, tc_handler_t,
				     void *intr_arg, int on)); 
extern void kn03_enable_intr __P((u_int slot, tc_handler_t,
				  void *intr_arg, int on)); 
extern void kn02_enable_intr __P ((u_int slot, tc_handler_t,
				   void *intr_arg, int on)); 
extern void kmin_enable_intr __P ((u_int slot, tc_handler_t,
				   void *intr_arg, int on)); 
extern void xine_enable_intr __P ((u_int slot, tc_handler_t,
				   void *intr_arg, int on)); 

/*
 * Configuration tables for the four models of
 * Decstation that have turbochannels. 
 * None of the four are the same.
 */
#include "ds-tc-conf.c"


/*
 * Function to map from a CPU code to a tc_cpu_desc.
 * This hould really be in machine-dependent code, where
 * it could even be a macro.
 */
struct tc_cpu_desc *
cpu_tcdesc(cpu)
    int cpu;
{
	if (cpu == DS_3MAXPLUS) {
		tc_enable_interrupt = kn03_enable_intr;
		return &kn03_tc_desc;
	} else if (cpu == DS_3MAX) {
		tc_enable_interrupt = kn02_enable_intr;
		return &kn02_tc_desc;
	} else if (cpu == DS_3MIN) {
		tc_enable_interrupt = kmin_enable_intr;
		return &kmin_tc_desc;
	} else if (cpu == DS_MAXINE) {
#ifdef DEBUG
		printf("MAXINE turbochannel\n");
#endif
		tc_enable_interrupt = xine_enable_intr;
		return &xine_tc_desc;
	} else if (cpu == DS_PMAX) {
#ifdef DIAGNOSTIC
		printf("tcattach: PMAX, no turbochannel\n");
#endif
		return NULL;
	} else if (cpu == DS_MIPSFAIR) {
		printf("tcattach: Mipsfair (5100), no turbochannel\n");
		return NULL;
	} else {
		panic("tcattach: Unrecognized bus type 0x%x\n", cpu);
	}
}


/*
 * Temporary glue:
 * present the old-style signatures as used by BUS_INTR_ESTABLISH(),
 * but using the new NetBSD machine-independent TC infrastructure.
 */

void
confglue_tc_intr_establish(ca, handler, arg)
	struct confargs *ca;
	intr_handler_t handler;
	intr_arg_t arg;
{
	struct tc_softc *sc = tccd.cd_devs[0]; /* XXX */
	/* XXX guess at level */
	(*sc->sc_desc->tc_intr_establish)
		((struct device*)sc, (void*)ca->ca_slotpri, 0, handler, arg);
}

void
confglue_tc_intr_disestablish(ca)
	struct confargs *ca;
{
	struct tc_softc *sc = tccd.cd_devs[0]; /* XXX */

	(*sc->sc_desc->tc_intr_disestablish)(
	     (struct device*)sc, (void*)ca->ca_slotpri);
}
/*
 * End of temporary glue.
 */


int
tcmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;


        /* Make sure that we're looking for a TC. */
        if (strcmp(ca->ca_name, tccd.cd_name) != 0)
                return (0);

        /* Make sure that unit exists. */
	if (cf->cf_unit != 0 ||
#ifdef pmax
	    0
#else
	    cputype > ntc_cpu_devs || tc_cpu_devs[cputype] == NULL
#endif
	    )
		return (0);

	return (1);
}

/*
 * Attach a turbochannel bus.   Once the turbochannel is attached,
 * search recursively for a system slot (which contains registers
 * for baseboard devices  in "subslots"), and for "real" on-board or
 * option turbochannel slots (that have their own turbochannel ROM
 * signature.
 */
void
tcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_softc *sc = (struct tc_softc *)self;
	struct confargs *nca;
	char namebuf[TC_ROM_LLEN+1];
	int i;

	printf("\n");

	/* keep our CPU description handy */
	sc->sc_desc = cpu_tcdesc(cputype);

#ifndef pmax /*XXX*/
	/* set up interrupt handlers */
	(*sc->sc_desc->tc_intr_setup)();
	set_iointr(sc->sc_desc->tc_iointr);
#endif /*!PMAX*/

	sc->sc_bus.ab_dv = (struct device *)sc;
	sc->sc_bus.ab_type = BUS_TC;
	sc->sc_bus.ab_intr_establish = confglue_tc_intr_establish;
	sc->sc_bus.ab_intr_disestablish = confglue_tc_intr_disestablish;
	sc->sc_bus.ab_cvtaddr = tc_cvtaddr;
	sc->sc_bus.ab_matchname = tc_matchname;

	if (sc->sc_desc == NULL)
		return;

	/* Try to configure each turbochannel (or CPU-internal) device */
	for (i = 0; i < sc->sc_desc->tcd_ndevs; i++) {

		nca = &sc->sc_desc->tcd_devs[i];
		if (nca == NULL) {
			printf("tcattach: bad config for slot %d\n", i);
			break;
		}
		nca->ca_bus = &sc->sc_bus;

#if defined(DIAGNOSTIC) || defined(DEBUG)
		if (nca->ca_slot > sc->sc_desc->tcd_nslots)
			panic("tcattach: dev slot > number of slots for %s",
			    nca->ca_name);
#endif

		if (tc_checkdevmem(BUS_CVTADDR(nca)) == 0)
			continue;

		/* If no name, we have to see what might be there. */
		if (nca->ca_name == NULL) {
			if (tc_checkslot(BUS_CVTADDR(nca), namebuf) == 0)
				continue;
			nca->ca_name = namebuf;
		}
		/* Tell the autoconfig machinery we've found the hardware. */
		config_found(self, nca, tcprint);
	}
}

int
tcprint(aux, pnp)
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
tc_cvtaddr(ca)
	struct confargs *ca;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	return ((caddr_t)sc->sc_desc->tcd_slots[ca->ca_slot].tcs_addr +
		ca->ca_offset);

}

void
tc_intr_establish(dev, cookie, level, handler, arg)
	/*struct confargs *ca;*/
	struct device *dev;
	void *cookie;
	tc_intrlevel_t level;
	intr_handler_t handler;
	intr_arg_t arg;
{
	struct tc_softc *sc = (struct tc_softc *)dev;

#ifdef DEBUG
	printf("tc_intr_establish: %s parent intrcode %d\n",
	       dev->dv_xname, dev->dv_parent->dv_xname, (int) cookie);
#endif

	/* XXX pmax interrupt-enable interface */
	(*sc->sc_desc->tc_intr_establish)(sc->sc_dv.dv_parent, cookie,
		 level, handler, arg);
}

void
tc_intr_disestablish(dev, cookie)
	struct device *dev;
	void *cookie;
{
	struct tc_softc *sc = (struct tc_softc *)dev;

	(*sc->sc_intr_disestablish)(sc->sc_dv.dv_parent, cookie);
}

int
tc_matchname(ca, name)
	struct confargs *ca;
	char *name;
{

	return (bcmp(name, ca->ca_name, TC_ROM_LLEN) == 0);
}

int
tc_checkdevmem(addr)
	caddr_t addr;
{
	u_int32_t *datap = (u_int32_t *) addr;

	/* Return non-zero if memory was there (i.e. address wasn't bad). */
	return (!badaddr(datap, sizeof (u_int32_t)));
}

u_int tc_slot_romoffs[] = { TC_SLOT_ROM, TC_SLOT_PROTOROM };
int ntc_slot_romoffs = sizeof tc_slot_romoffs / sizeof tc_slot_romoffs[0];

int
tc_checkslot(addr, namep)
	caddr_t addr;
	char *namep;
{
	struct tc_rommap *romp;
	int i, j;

	for (i = 0; i < ntc_slot_romoffs; i++) {
		romp = (struct tc_rommap *)
		    (addr + tc_slot_romoffs[i]);

		switch (romp->tcr_width.v) {
		case 1:
		case 2:
		case 4:
			break;

		default:
			continue;
		}

		if (romp->tcr_stride.v != 4)
			continue;

		for (j = 0; j < 4; j++)
			if (romp->tcr_test[j+0*romp->tcr_stride.v] != 0x55 ||
			    romp->tcr_test[j+1*romp->tcr_stride.v] != 0x00 ||
			    romp->tcr_test[j+2*romp->tcr_stride.v] != 0xaa ||
			    romp->tcr_test[j+3*romp->tcr_stride.v] != 0xff)
				continue;

		for (j = 0; j < TC_ROM_LLEN; j++)
			namep[j] = romp->tcr_modname[j].v;
		namep[TC_ROM_LLEN] = '\0';
		return (1);
	}
	return (0);
}

int
tc_intrnull(val)
	void *val;
{

	panic("uncaught TC intr for slot %ld\n", (long)val);
}




/*
 * Probe the turbochannel for a framebuffer option card, starting
 * at the preferred slot and then scanning all slots. Configure the first
 * supported framebuffer device found, if any, as the console, and return
 * 1 if found.
 * Called before autoconfiguration, to find a system console.
 */
int
tc_findconsole(preferred_slot)
	int preferred_slot;
{
	int slot;

	struct tc_cpu_desc * sc_desc;

	/* First, try the slot configured as console in NVRAM. */
	 /* if (consprobeslot(preferred_slot)) return (1); */

	/*
	 * Try to configure each turbochannel (or CPU-internal) device.
	 * Knows about gross internals of TurboChannel bus autoconfig
	 * descriptor, which needs to be fixed badly.
	 */
	if ((sc_desc = cpu_tcdesc(pmax_boardtype)) == NULL)
		return 0;
	for (slot = 0; slot < sc_desc->tcd_ndevs; slot++) {

		if (tc_consprobeslot(slot))
			return (1);
	}
	return (0);
}

/*
 * Try and configure one slot as framebuffer console.
 * Accept only the framebuffers for which driver are configured into
 * the kernel.  If a suitable framebuffer is found, attach it and
 * set up glass-tty emulation.
 */
int
tc_consprobeslot(slot)
	int slot;
{
	void *slotaddr;
	char name[20];
	struct tc_cpu_desc * sc_desc;

	if (slot < 0 || ((sc_desc = cpu_tcdesc(pmax_boardtype)) == NULL))
		return 0;
	slotaddr = (void *)(sc_desc->tcd_slots[slot].tcs_addr);

	if (tc_checkdevmem(slotaddr) == 0)
		return (0);

	if (tc_checkslot(slotaddr, name) == 0)
		return (0);

	/*
	 * We found an device in the given slot. Now see if it's a
	 * framebuffer for which we have a driver. 
	 */

	/*printf(", trying to init a \"%s\"", name);*/

#define DRIVER_FOR_SLOT(slotname, drivername) \
	(strcmp (slotname, drivername) == 0)

#if NMFB > 0
	if (DRIVER_FOR_SLOT(name, "PMAG-AA ") &&
	    mfbinit(slotaddr, 0, 1)) {
		return (1);
	}
#endif /* NMFB */

#if NSFB > 0
	if (DRIVER_FOR_SLOT(name, "PMAGB-BA") &&
	    sfbinit(slotaddr, 0, 1)) {
		return (1);
	}
#endif /* NSFB */

#if NCFB > 0
	/*"cfb"*/
	if (DRIVER_FOR_SLOT(name, "PMAG-BA ") &&
	    cfbinit(NULL, slotaddr, 0, 1)) {
		return (1);
	}
#endif /* NCFB */
	return (0);
}


/* hack for kn03 ioasic */

void
tc_ds_ioasic_intr_setup ()
{
	printf("not setting up TC intrs\n");
}

/*
 * Estabish an interrupt handler, but on what bus -- TC or ioctl asic?
 */
void
tc_ds_ioasic_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    int level;
    intr_handler_t handler;
    void *val;
{

#ifdef notanymore
	 if (BUS_MATCHNAME(ca, "IOCTL   ")) {
		 printf("(no interrupt for asic");
		 return;
	 }

	/* The kn02 doesn't really have an ASIC */
	 if (BUS_MATCHNAME(ca, KN02_ASIC_NAME)) {
		 printf("(no interrupt for proto-asic)\n");
		 return;
	 }
#endif


	/* Never tested on these processors */
	if (cputype == DS_3MIN || cputype == DS_MAXINE)
	    printf("tc_enable %s sc %x slot %d\n",
		   dev->dv_xname, (int)val, cookie);

#ifdef DIAGNOSTIC
	if (tc_enable_interrupt == NULL)
	    panic("tc_intr_establish: tc_enable not set\n");
#endif

	 /* Enable interrupt number "cookie" on this CPU */
	 (*tc_enable_interrupt) ((int)cookie, handler, val, 1);
}

void
tc_ds_ioasic_intr_disestablish(dev, arg)
    struct device *dev;
    void *arg;
{
	/*(*tc_enable_interrupt) (ca->ca_slot, handler, 0);*/
    	printf("cannot dis-establish TC intrs\n");
}

void
tc_ds_ioasic_iointr (framep, vec)
    void * framep;
    int vec;
{
	printf("bogus interrupt handler fp %x vec %d\n", framep, vec);
}

/*	$NetBSD: tc.c,v 1.6 1995/12/28 06:44:57 jonathan Exp $	*/

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

#include <machine/autoconf.h>

#ifdef alpha
#include <machine/rpb.h>
#include <alpha/tc/tc.h>
#endif

#ifdef pmax
#include <pmax/tc/tc.h>
#endif

struct tc_softc {
	struct	device sc_dv;
	struct	abus sc_bus;
	struct	tc_cpu_desc *sc_desc;
};

/* Definition of the driver for autoconfig. */
int	tcmatch(struct device *, void *, void *);
void	tcattach(struct device *, struct device *, void *);
int	tcprint(void *, char *);
struct cfdriver tccd =
    { NULL, "tc", tcmatch, tcattach, DV_DULL, sizeof (struct tc_softc) };

void	tc_intr_establish __P((struct confargs *, intr_handler_t handler,
			       intr_arg_t));
void	tc_intr_disestablish __P((struct confargs *));
caddr_t	tc_cvtaddr __P((struct confargs *));
int	tc_matchname __P((struct confargs *, char *));

extern int cputype;


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

#include <pmax/pmax/nameglue.h>


void	tc_ds_ioasic_intr_setup __P((void));
void	tc_ds_ioasic_intr_establish
	    __P((struct confargs *, intr_handler_t, void *));
void	tc_ds_ioasic_intr_disestablish __P((struct confargs *));
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
 * configuration tables for the four models of
 * Decstation that have turbochannels. 
 * None of the four are the same.
 */
#include "ds-tc-conf.c"


/*
 * Mapping from CPU type to a tc_cpu_desc for that CPU type.
 * (Alpha-specific.)
 */
struct tc_cpu_desc *tc_cpu_devs[] = {
        NULL,                   /* Unused */
        NULL,                   /* ST_ADU */
        NULL,                   /* ST_DEC_4000 */
        NULL,                   /* ST_DEC_7000 */
#ifdef DEC_3000_500
        &dec_3000_500_cpu,      /* ST_DEC_3000_500 */
#else
        NULL,
#endif
        NULL,                   /* Unused  */
        NULL,                   /* ST_DEC_2000_300 */
#ifdef DEC_3000_300
        &dec_3000_300_cpu,      /* ST_DEC_3000_300 */
#else
        NULL,
#endif
};
int ntc_cpu_devs = sizeof tc_cpu_devs / sizeof tc_cpu_devs[0];

/*
 * Function to map from a CPU code to a tc_cpu_desc.
 * This hould really be in machine-dependent code, where
 * it could even be a macro.
 */
struct tc_cpu_desc *
cpu_tcdesc(cpu)
    int cpu;
{
 /*XXX*/
#ifdef	pmax
	if (cpu == DS_3MAXPLUS) {
		tc_enable_interrupt = kn03_enable_intr;
		return &kn03_tc_desc;
	} else if (cpu == DS_3MAX) {
		tc_enable_interrupt = kn02_enable_intr;
		return &kn02_tc_desc;
	} else if (cpu == DS_3MIN) {
		DPRINTF(("tcattach: 3MIN Turbochannel\n"));
		tc_enable_interrupt = kmin_enable_intr;
		return &kmin_tc_desc;
	} else if (cpu == DS_MAXINE) {
		DPRINTF(("MAXINE turbochannel\n"));
		tc_enable_interrupt = xine_enable_intr;
		return &xine_tc_desc;
	} else if (cpu == DS_PMAX) {
		DPRINTF(("tcattach: PMAX, no turbochannel\n"));
		return NULL;
	} else if (cpu == DS_MIPSFAIR) {
		DPRINTF(("tcattach: Mipsfair (5100), no turbochannel\n"));
	} else {
		panic("tcattach: Unrecognized bus type 0x%x\n", cpu);
	}

#else  /* alpha?*/
	return tc_cpu_devs[cputype];
#endif /* alpha?*/
}

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
	sc->sc_bus.ab_intr_establish = tc_intr_establish;
	sc->sc_bus.ab_intr_disestablish = tc_intr_disestablish;
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

	return (sc->sc_desc->tcd_slots[ca->ca_slot].tsd_dense + ca->ca_offset);

}

void
tc_intr_establish(ca, handler, val)
	struct confargs *ca;
	intr_handler_t handler;
	intr_arg_t val;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	(*sc->sc_desc->tc_intr_establish)(ca, handler, val);
}

void
tc_intr_disestablish(ca)
	struct confargs *ca;
{
	struct tc_softc *sc = tccd.cd_devs[0];

	(*sc->sc_desc->tc_intr_disestablish)(ca);
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


/* hack for kn03 */

void
tc_ds_ioasic_intr_setup ()
{
	printf("not setting up TC intrs\n");
}

void
tc_ds_ioasic_intr_establish(ca, handler, val)
    struct confargs *ca;
    intr_handler_t handler;
    void *val;
{
	int unit = (int) val;

	 if (BUS_MATCHNAME(ca, "IOCTL   ")) {
		 printf("(no interrupt for asic");
		 return;
	 }

	/* The kn02 doesn't really have an ASIC */
	 if (BUS_MATCHNAME(ca, KN02_ASIC_NAME)) {
		 printf("(no interrupt for proto-asic)\n");
		 return;
	 }

	/* Never tested on these processors */
	if (cputype == DS_3MIN || cputype == DS_MAXINE)
	    printf("tc_enable %s%d slot %d\n",
		   ca->ca_name, (int)unit, ca->ca_slotpri);

#ifdef DIAGNOSTIC
	if (tc_enable_interrupt == NULL)
	    panic("tc_intr_establish: tc_enable not set\n");
#endif

	(*tc_enable_interrupt) (ca->ca_slotpri, handler, (void*)unit, 1);
}

void
tc_ds_ioasic_intr_disestablish(args)
    struct confargs *args;
{
	/*(*tc_enable_interrupt) (ca->ca_slot, handler, 0);*/
    	printf("cannot dis-establish TC intrs\n");
}

void
tc_ds_ioasic_iointr (framep, vec)
    void * framep;
    int vec;
    
			   
{
	printf("bogus interrupt handler\n");
}

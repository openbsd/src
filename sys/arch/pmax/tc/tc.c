/*	$NetBSD: tc.c,v 1.9 1996/02/02 18:08:06 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jonathan Stone
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

#define TC_DEBUG	/* until decstatn autoconfig works with dev/tc/tc.c*/

#include <sys/param.h>
#include <sys/device.h>
#include <dev/cons.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>


/* Which TC framebuffers have drivers, for configuring a console device. */
#include "cfb.h"
#include "mfb.h"
#include "sfb.h"

extern int pmax_boardtype;


/*
 * Old-style model-specific autoconfiguration description.
 */
struct tcbus_attach_args {
	u_int	tca_nslots;
	struct	tc_slotdesc *tca_slots;

	u_int	tca_nbuiltins;
	const struct	tc_builtin *tca_builtins;

	void	(*tca_intr_establish) __P((struct device *dev, void *cookie,
					   tc_intrlevel_t level,
					   intr_handler_t handler,
					   void *arg));
	void	(*tca_intr_disestablish) __P((struct device *, void *));
};

/* Return the appropriate tc_attach_args for a given cputype */
extern struct tc_attach_args *  cpu_tcdesc __P ((int cputype));


/* Definition of the driver for autoconfig. */
int	tcmatch(struct device *, void *, void *);
void	tcattach(struct device *, struct device *, void *);
int	tcprint(void *, char *);

void	tc_ds_intr_establish __P((struct device *, void *, tc_intrlevel_t,
				intr_handler_t handler, intr_arg_t arg));
void	tc_intr_disestablish __P((struct device *dev, void *cookie));
caddr_t	tc_cvtaddr __P((struct confargs *));

extern int cputype;
extern int tc_findconsole __P((int prom_slot));

/* Forward declarations */
int consprobeslot __P((int slot));


/*
 *  TurboChannel autoconfiguration declarations and tables for DECstations.
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
					 tc_intrlevel_t level,
					 intr_handler_t handler,
					 void *arg));
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
 * Function to map from a CPU code to a tcbus tc_attach_args struct.
 * This should really be in machine-dependent code, where
 * it could even be a macro.
 */
struct tc_attach_args *
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
		panic("cpu_tc: Unrecognized bus type 0x%x\n", cpu);
	}
}

/*
 * We have a TurboChannel bus.  Configure it.
 */
void
config_tcbus(parent, cputype, printfn)
     	struct device *parent;
	int cputype;
	int	printfn __P((void *, char *));

{
	struct tc_attach_args tc;

	struct tc_attach_args * tcbus = cpu_tcdesc(pmax_boardtype);

	/*
	 * Set up important CPU/chipset information.
	 */
	tc.tca_nslots = tcbus->tca_nslots;
	tc.tca_slots = tcbus->tca_slots;

	tc.tca_nbuiltins = tcbus->tca_nbuiltins;
	tc.tca_builtins = tcbus->tca_builtins;
	tc.tca_intr_establish = tc_ds_intr_establish;	/*XXX*/
	tc.tca_intr_disestablish = tc_ds_ioasic_intr_disestablish;	/*XXX*/

	config_found(parent, (struct confargs*)&tc, printfn);
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

	struct tc_attach_args * sc_desc;

	/* First, try the slot configured as console in NVRAM. */
	 /* if (consprobeslot(preferred_slot)) return (1); */

	/*
	 * Try to configure each turbochannel (or CPU-internal) device.
	 * Knows about gross internals of TurboChannel bus autoconfig
	 * descriptor, which needs to be fixed badly.
	 */
	if ((sc_desc = cpu_tcdesc(pmax_boardtype)) == NULL)
		return 0;
	for (slot = 0; slot < sc_desc->tca_nslots; slot++) {

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
	struct tc_attach_args * sc_desc;

	if (slot < 0 || ((sc_desc = cpu_tcdesc(pmax_boardtype)) == NULL))
		return 0;
	slotaddr = (void *)(sc_desc->tca_slots[slot].tcs_addr);

	if (tc_badaddr(slotaddr))
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

/*
 * Estabish an interrupt handler, but on what bus -- TC or ioctl asic?
 */
void
tc_ds_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    tc_intrlevel_t level;
    intr_handler_t handler;
    void *val;
{

	/* Never tested on these processors */
	if (cputype == DS_3MIN || cputype == DS_MAXINE)
	    printf("tc_enable %s sc %x slot %d\n",
		   dev->dv_xname, (int)val, cookie);

#ifdef DIAGNOSTIC
	if (tc_enable_interrupt == NULL)
	    panic("tc_intr_establish: tc_enable not set\n");
#endif

#ifdef DEBUG
	printf("tc_intr_establish: slot %d level %d handler %p sc %p on\n",
		(int) cookie, (int) level, handler,  val);
#endif

	 /*
	  * Enable the interrupt from tc (or ioctl asic) slot with NetBSD/pmax
	  * sw-convention name ``cookie'' on this CPU.
	  * XXX store the level somewhere for selective enabling of
	  * interrupts from TC option slots.
	  */
	 (*tc_enable_interrupt) ((int)cookie, handler, val, 1);
}


/* hack for kn03 ioasic */

void
tc_ds_ioasic_intr_setup ()
{
	printf("not setting up TC intrs\n");
}


/*
 * establish an interrupt handler for an ioasic device.
 * On NetBSD/pmax, there is currently a single, merged interrupt handler for
 * both TC and ioasic.  Just use the tc interrupt-establish function.
*/
void
tc_ds_ioasic_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    tc_intrlevel_t level;
    intr_handler_t handler;
    void *val;
{
	tc_intr_establish(dev, cookie, level, handler, val);
}

void
tc_ds_ioasic_intr_disestablish(dev, arg)
    struct device *dev;
    void *arg;
{
	/*(*tc_enable_interrupt) (ca->ca_slot, handler, 0);*/
    	printf("cannot dis-establish IOASIC interrupts\n");
}

void
tc_ds_ioasic_iointr (framep, vec)
    void * framep;
    int vec;
{
	printf("bogus interrupt handler fp %x vec %d\n", framep, vec);
}

/* XXX */
#include <dev/tc/tc.c>

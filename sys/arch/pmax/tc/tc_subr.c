/*	$NetBSD: tc_subr.c,v 1.10 1997/05/24 09:17:24 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <sys/param.h>
#include <sys/systm.h>			/* printf() */
#include <sys/device.h>
#include <dev/cons.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>

extern int pmax_boardtype;


/* Return the appropriate tcbus_attach_args for a given cputype */
extern struct tcbus_attach_args *  cpu_tcdesc __P ((int cputype));


/* Definition of the driver for autoconfig. */
int	tcmatch(struct device *, void *, void *);
void	tcattach(struct device *, struct device *, void *);
int	tcprint(void *, const char *);

void	tc_ds_intr_establish __P((struct device *, void *, tc_intrlevel_t,
				intr_handler_t handler, intr_arg_t arg));
void	tc_intr_disestablish __P((struct device *dev, void *cookie));
caddr_t	tc_cvtaddr __P((struct confargs *));

extern int cputype;
extern int tc_findconsole __P((int prom_slot));

/* Forward declarations */
static int tc_consprobeslot __P((tc_addr_t slotaddr));


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

#include <machine/fbio.h>
#include <machine/fbvar.h>
#include <pmax/dev/cfbvar.h>
#include <pmax/dev/mfbvar.h>
#include <pmax/dev/sfbvar.h>
#include <pmax/dev/xcfbvar.h>


/* Which TC framebuffers have drivers, for configuring a console device. */
#include "cfb.h"
#include "mfb.h"
#include "sfb.h"


/*#include <pmax/pmax/nameglue.h>*/
#define KV(x) ((tc_addr_t)MIPS_PHYS_TO_KSEG1(x))



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

#define C(x)	((void *)(u_long)x)

#define TC_SCSI  "PMAZ-AA "
#define TC_ETHER "PMAD-AA "

/*
 * 3MIN and 3MAXPLUS turbochannel slots.
 * The kmin (3MIN) and kn03 (3MAXPLUS) have the same number of slots.
 * We can share one configuration-struct table and use two slot-address
 * tables to handle the fact that the turbochannel slot size and base
 * addresses are different on the two machines.
 * (thankfully, the IOCTL ASIC subslots are all the same size on all
 * DECstations with IOASICs.) The devices are listed in the order in which
 *  we should probe and attach them.
 */

/*
 * The only builtin Turbochannel device on the kn03 and kmin
 * is the IOCTL asic, which is mapped into TC slot 3.
 */
const struct tc_builtin tc_kn03_builtins[] = {
	{ "IOCTL   ",	3, 0x0, C(3), /*C(3)*/ }
};

/* 3MAXPLUS TC slot addresses */
static struct tc_slotdesc tc_kn03_slots [4] = {
       	{ KV(KN03_PHYS_TC_0_START), C(0) },  /* slot0 - tc option slot 0 */
	{ KV(KN03_PHYS_TC_1_START), C(1) },  /* slot1 - tc option slot 1 */
	{ KV(KN03_PHYS_TC_2_START), C(2) },  /* slot2 - tc option slot 2 */
	{ KV(KN03_PHYS_TC_3_START), C(3) }   /* slot3 - IO asic on b'board */
};
int tc_kn03_nslots =
    sizeof(tc_kn03_slots) / sizeof(tc_kn03_slots[0]);


/* 3MAXPLUS turbochannel autoconfiguration table */
struct tcbus_attach_args kn03_tc_desc =
{
	"tc",				/* XXX common substructure */
	1,
	KN03_TC_NSLOTS, tc_kn03_slots,
	1, tc_kn03_builtins,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish
};

/************************************************************************/

/* 3MIN slot addreseses */
static struct tc_slotdesc tc_kmin_slots [] = {
       	{ KV(KMIN_PHYS_TC_0_START), C(0) },   /* slot0 - tc option slot 0 */
	{ KV(KMIN_PHYS_TC_1_START), C(1) },   /* slot1 - tc option slot 1 */
	{ KV(KMIN_PHYS_TC_2_START), C(2) },   /* slot2 - tc option slot 2 */
	{ KV(KMIN_PHYS_TC_3_START), C(3) }    /* slot3 - IO asic on b'board */
};

int tc_kmin_nslots =
    sizeof(tc_kmin_slots) / sizeof(tc_kmin_slots[0]);

/* 3MIN turbochannel autoconfiguration table */
struct tcbus_attach_args kmin_tc_desc =
{
	"tc",				/* XXX common substructure */
	0,
	KMIN_TC_NSLOTS, tc_kmin_slots,
	1, tc_kn03_builtins, /*XXX*/
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish,
};

/************************************************************************/

/*
 * The builtin Turbonchannel devices on the MAXINE
 * is the IOCTL asic, which is mapped into TC slot 3, and the PMAG-DV
 * xcfb framebuffer, which is built into the baseboard.
 */
const struct tc_builtin tc_xine_builtins[] = {
	{ "IOCTL   ",	3, 0x0, C(3), /*C(3)*/ },
	{ "PMAG-DV ",	2, 0x0, C(2), /*C(4)*/ }
};

/* MAXINE slot addreseses */
static struct tc_slotdesc tc_xine_slots [4] = {
       	{ KV(XINE_PHYS_TC_0_START), C(0) },   /* slot 0 - tc option slot 0 */
	{ KV(XINE_PHYS_TC_1_START), C(1) },   /* slot 1 - tc option slot 1 */
	/*{ KV(-1), C(-1) },*/  /* physical space for ``slot 2'' is reserved */
	{ KV(XINE_PHYS_CFB_START), C(2) },    /* slot 2 - fb on b'board */
	{ KV(XINE_PHYS_TC_3_START), C(3) }   /* slot 3 - IO asic on b'board */
};

int tc_xine_nslots =
    sizeof(tc_xine_slots) / sizeof(tc_xine_slots[0]);

struct tcbus_attach_args xine_tc_desc =
{
	"tc",				/* XXX common substructure */
  	0,				/* number of slots */
	XINE_TC_NSLOTS, tc_xine_slots,
	2, tc_xine_builtins,
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish
};


/************************************************************************/

/* 3MAX (kn02) turbochannel slots  */
/* slot addreseses */
static struct tc_slotdesc tc_kn02_slots [8] = {
       	{ KV(KN02_PHYS_TC_0_START), C(0)},	/* slot 0 - tc option slot 0 */
	{ KV(KN02_PHYS_TC_1_START), C(1), },	/* slot 1 - tc option slot 1 */
	{ KV(KN02_PHYS_TC_2_START), C(2), },	/* slot 2 - tc option slot 2 */
	{ KV(KN02_PHYS_TC_3_START), C(3), },	/* slot 3 - reserved */
	{ KV(KN02_PHYS_TC_4_START), C(4), },	/* slot 4 - reserved */
	{ KV(KN02_PHYS_TC_5_START), C(5), },	/* slot 5 - SCSI on b`board */
	{ KV(KN02_PHYS_TC_6_START), C(6), },	/* slot 6 - b'board Ether */
	{ KV(KN02_PHYS_TC_7_START), C(7), }	/* slot 7 - system CSR, etc. */
};

int tc_kn02_nslots =
    sizeof(tc_kn02_slots) / sizeof(tc_kn02_slots[0]);

#define KN02_ROM_NAME KN02_ASIC_NAME

#define TC_KN02_DEV_IOASIC     -1
#define TC_KN02_DEV_ETHER	6
#define TC_KN02_DEV_SCSI	5

const struct tc_builtin tc_kn02_builtins[] = {
	{ KN02_ROM_NAME,7, 0x0, C(TC_KN02_DEV_IOASIC) /* C(7)*/ },
	{ TC_ETHER,	6, 0x0, C(TC_KN02_DEV_ETHER)  /* C(6)*/ },
	{ TC_SCSI,	5, 0x0, C(TC_KN02_DEV_SCSI)   /* C(5)*/ }
};


struct tcbus_attach_args kn02_tc_desc =
{
	"tc",				/* XXX common substructure */
  	1,
	8, tc_kn02_slots,
	3, tc_kn02_builtins,	/*XXX*/
	tc_ds_ioasic_intr_establish,
	tc_ds_ioasic_intr_disestablish
};

/************************************************************************/



/*
 * Function to map from a CPU code to a  tcbus_attach_args struct.
 * This should really be in machine-dependent code, where
 * it could even be a macro.
 */
struct tcbus_attach_args *
cpu_tcdesc(cpu)
    int cpu;
{
	if (cpu == DS_3MAXPLUS) {
#ifdef DS5000_240
		tc_enable_interrupt = kn03_enable_intr;
		return &kn03_tc_desc;
#else
		return (0);
#endif /* DS5000_240 */
	} else if (cpu == DS_3MAX) {
#ifdef DS5000_200
		tc_enable_interrupt = kn02_enable_intr;
		return &kn02_tc_desc;
#else
		return (0);
#endif /* DS5000_240 */
	} else if (cpu == DS_3MIN) {
#ifdef DS5000_100
		tc_enable_interrupt = kmin_enable_intr;
		return &kmin_tc_desc;
#else
		return (0);
#endif /*DS5000_100*/
	} else if (cpu == DS_MAXINE) {
#ifdef DS5000_25
		tc_enable_interrupt = xine_enable_intr;
		return &xine_tc_desc;
#else
		return (0);
#endif /*DS5000_25*/
	} else if (cpu == DS_PMAX) {
#ifdef DIAGNOSTIC
		printf("tcattach: PMAX, no turbochannel\n");
#endif /*DIAGNOSTIC*/
		return NULL;
	} else if (cpu == DS_MIPSFAIR) {
		printf("tcattach: Mipsfair (5100), no turbochannel\n");
		return NULL;
	} else {
		panic("cpu_tc: Unrecognized bus type 0x%x", cpu);
	}
}

/*
 * We have a TurboChannel bus.  Configure it.
 */
void
config_tcbus(parent, cputype, printfn)
     	struct device *parent;
	int cputype;
	int	printfn __P((void *, const char *));

{
	struct tcbus_attach_args tcb;

	struct tcbus_attach_args * tcbus = cpu_tcdesc(pmax_boardtype);

	if (tcbus == NULL) {
		printf("no TurboChannel configuration info for this machine\n");
		return;
	}

	/*
	 * Set up important CPU/chipset information.
	 */
	/*XXX*/
	tcb.tba_busname =  tcbus->tba_busname;

	tcb.tba_speed = tcbus->tba_speed;
	tcb.tba_nslots = tcbus->tba_nslots;
	tcb.tba_slots = tcbus->tba_slots;

	tcb.tba_nbuiltins = tcbus->tba_nbuiltins;
	tcb.tba_builtins = tcbus->tba_builtins;
	tcb.tba_intr_establish = tc_ds_intr_establish;	/*XXX*/
	tcb.tba_intr_disestablish = tc_ds_ioasic_intr_disestablish;	/*XXX*/

	config_found(parent, (struct confargs*)&tcb, printfn);
}


/*
 * Called before autoconfiguration, to find a system console.
 *
 * Probe the turbochannel for a framebuffer option card, starting at
 * the preferred slot and then scanning all slots. Configure the first
 * supported framebuffer device found, if any, as the console, and
 * return 1 if found.
 */
int
tc_findconsole(preferred_slot)
	int preferred_slot;
{
	int slot;

	struct tcbus_attach_args * sc_desc;

	/* First, try the slot configured as console in NVRAM. */
	 /* if (consprobeslot(preferred_slot)) return (1); */

	/*
	 * Try to configure each turbochannel (or CPU-internal) device.
	 * Knows about gross internals of TurboChannel bus autoconfig
	 * descriptor, which needs to be fixed badly.
	 */
	if ((sc_desc = cpu_tcdesc(pmax_boardtype)) == NULL)
		return 0;
	for (slot = 0; slot < sc_desc->tba_nslots; slot++) {

		if (tc_consprobeslot(sc_desc->tba_slots[slot].tcs_addr))
			return (1);
	}
	return (0);
}


/*
 * Look in a single TC option slot to see if it contains a possible
 * framebuffer console device.
 * Configure only the framebuffers for which driver are configured
 * into the kernel.  If a suitable framebuffer is found, initialize
 * it, and set up glass-tty emulation.
 */
static int
tc_consprobeslot(tc_slotaddr)
	tc_addr_t tc_slotaddr;
{

	char name[20];
	void *slotaddr = (void *) tc_slotaddr;

	if (tc_badaddr(slotaddr))
		return (0);

	if (tc_checkslot(tc_slotaddr, name) == 0)
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
	    mfbinit(NULL, slotaddr, 0, 1)) {
		return (1);
	}
#endif /* NMFB */

#if NSFB > 0
	if (DRIVER_FOR_SLOT(name, "PMAGB-BA") &&
	    sfbinit(NULL, slotaddr, 0, 1)) {
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
	    printf("tc_enable %s sc %x slot %p\n",
		   dev->dv_xname, (int)val, cookie);

#ifdef DIAGNOSTIC
	if (tc_enable_interrupt == NULL)
	    panic("tc_intr_establish: tc_enable not set");
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
	printf("bogus interrupt handler fp %p vec %d\n", framep, vec);
}

/*	$NetBSD: pm_ds.c,v 1.4 1997/05/24 08:19:52 jonathan Exp $	*/

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
 *
 * this driver contributed by Jonathan Stone
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <machine/autoconf.h>

#include <pmax/pmax/kn01.h>
#include <pmax/pmax/cons.h>

#include <sys/ioctl.h>
#include <machine/pmioctl.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/fbreg.h>
#include <machine/dc7085cons.h>		/* XXX */
#include <pmax/dev/pmvar.h>		/* XXX move */

#include "fb.h"
#include "pm.h"
#include "dc_ds.h"

#if 0
#if NDC_DS == 0
pm needs dc device
#endif
#endif


extern int pminit __P((struct fbinfo *fi, int unit, int cold_console_flag));
int ds_pm_init __P ((struct fbinfo *fi, int unti, int cold_console_flag));

int pm_ds_match __P((struct device *, void *, void *));
void pm_ds_attach __P((struct device *, struct device *, void *));

/*
 * Define decstation pm front-end driver for autoconfig
 */
extern struct cfattach pm_ds_ca;
struct cfattach pm_ds_ca = {
	sizeof(struct device), pm_ds_match, pm_ds_attach
};

/* XXX pmvar.h */
extern struct fbuacces pmu;

/* static struct for cold console init */
struct fbinfo	pmfi;		/*XXX*/

/*
 * rcons methods and globals.
 */
extern struct pmax_fbtty pmfb;

/*
 * rcons methods and globals.
 */
extern struct pmax_fbtty pmfb;

/*
 * XXX
 * pmax raster-console infrastructure needs to reset the mouse, so
 * we need a driver callback.
 * pm framebuffers are only found in {dec,vax}station 3100s with dc7085s..
 * we hardcode an entry point.
 * XXX
 */
void dcPutc	__P((dev_t, int));		/* XXX */


/*
 * Intialize pm framebuffer as console while cold
 */
int
ds_pm_init (fi, unit, cold_console_flag)
	struct fbinfo *fi;
	int unit;
	int cold_console_flag;
{
	/* only have one pm, address &c hardcoded in pminit() */
	return (pminit(fi, unit, cold_console_flag));
}

int
pm_ds_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;
	caddr_t pmaddr = (caddr_t)ca->ca_addr;

	/* make sure that we're looking for this type of device. */
	if (strcmp(ca->ca_name, "pm") != 0)
		return (0);

	if (badaddr(pmaddr, 4))
		return (0);

	return (1);
}

void
pm_ds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	/*struct confargs *ca = aux;*/
	/*caddr_t pmaddr = (caddr_t)ca->ca_addr;*/

	if (!pminit(&pmfi, 0, 0))
		return;

	/* no interrupts for PM */
	/*BUS_INTR_ESTABLISH(ca, sccintr, self->dv_unit);*/
	printf("\n");
	return;
}


/*
 * pmax FB initialization.  This is abstracted out from pmattach() so
 * that a console framebuffer can be initialized early in boot.
 *
 * Compute cursor-chip address, raster address, depth, initialize
 * statically-allocated "softc", and pass to pmattach().
 */
int
pminit(fi, unit, cold_console_flag)
	struct fbinfo *fi;
	int unit;
	int cold_console_flag;
{
	/*
	 * If this device is being initialized as the console, malloc()
	 * is not yet up and we must use statically-allocated space.
	 */
	if (fi == NULL) {
		fi = &pmfi;	/* XXX */
	}
	/* cmap_bits set in MI back-end */


	/* Set address of frame buffer... */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)MIPS_PHYS_TO_KSEG1(KN01_PHYS_FBUF_START);
	fi->fi_base = (caddr_t)MIPS_PHYS_TO_KSEG1(KN01_SYS_PCC);
	fi->fi_vdac = (caddr_t)MIPS_PHYS_TO_KSEG1(KN01_SYS_VDAC);

	/* check for no frame buffer */
	if (badaddr((char *)fi->fi_pixels, 4))
		return (0);

	/* Fill in the stuff that differs from monochrome to color. */
	if (*(volatile u_short *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CSR) &
	    KN01_CSR_MONO) {
		fi->fi_type.fb_depth = 1;
	}
	else {
		fi->fi_type.fb_depth = 8;
	}

	/*
	 * Set mmap'able address of qvss-compatible user info structure.
	 *
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 *
	 * XXX can go away when MI support for d_mmap entrypoints added.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MIPS_PHYS_TO_KSEG1(MIPS_KSEG0_TO_PHYS(&pmu));

	fi->fi_glasstty = &pmfb;

	fi->fi_glasstty = &pmfb;

	/*
	 * set putc/getc entry point
	 */
	fi->fi_glasstty->KBDPutc = dcPutc;	/* XXX */
	fi->fi_glasstty->kbddev = makedev(DCDEV, DCKBD_PORT);

	/* call back-end to initialize hardware and attach to rcons */
	return (pmattach(fi, unit, cold_console_flag));
}

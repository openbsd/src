/*	$OpenBSD: cg4.c,v 1.12 2002/08/02 16:13:07 millert Exp $	*/
/*	$NetBSD: cg4.c,v 1.11 1996/10/29 19:54:19 gwr Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cg4) driver.
 *
 * Credits, history:
 * Gordon Ross created this driver based on the cg3 driver from
 * the sparc port as distributed in BSD 4.4 Lite, but included
 * support for only the "type B" adapter (Brooktree DACs).
 * Ezra Story added support for the "type A" (AMD DACs).
 *
 * Todo:
 * Make this driver handle video interrupts.
 * Defer colormap updates to vertical retrace interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>

#include "fbvar.h"
#include "btreg.h"
#include "btvar.h"
#include "cg4reg.h"

#define	CG4_MMAP_SIZE (CG4_OVERLAY_SIZE + CG4_ENABLE_SIZE + CG4_PIXMAP_SIZE)

extern unsigned char cpu_machine_id;

#define CMAP_SIZE 256
struct soft_cmap {
	u_char r[CMAP_SIZE];
	u_char g[CMAP_SIZE];
	u_char b[CMAP_SIZE];
};

/* per-display variables */
struct cg4_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	int 	sc_cg4type;		/* A or B */
	void	*sc_va_cmap;		/* Colormap h/w (mapped KVA) */
	int 	sc_pa_overlay;		/* phys. addr. of overlay plane */
	int 	sc_pa_enable;		/* phys. addr. of enable plane */
	int 	sc_pa_pixmap;		/* phys. addr. of color plane */
	int 	sc_blanked;		/* true if blanked */

	union bt_cmap *sc_btcm;		/* Brooktree color map */
	struct soft_cmap sc_cmap;	/* Generic soft colormap. */
};

/* autoconfiguration driver */
static void	cg4attach(struct device *, struct device *, void *);
static int	cg4match(struct device *, void *, void *);

struct cfattach cgfour_ca = {
	sizeof(struct cg4_softc), cg4match, cg4attach
};

struct cfdriver cgfour_cd = {
	NULL, "cgfour", DV_DULL
};

/* frame buffer generic driver */

static int	cg4gattr(struct fbdevice *, struct fbgattr *);
static int	cg4gvideo(struct fbdevice *, int *);
static int	cg4svideo(struct fbdevice *, int *);
static int	cg4getcmap(struct fbdevice *, struct fbcmap *);
static int	cg4putcmap(struct fbdevice *, struct fbcmap *);

static void	cg4a_init(struct cg4_softc *);
static void	cg4a_svideo(struct cg4_softc *, int);
static void	cg4a_ldcmap(struct cg4_softc *);

static void	cg4b_init(struct cg4_softc *);
static void	cg4b_svideo(struct cg4_softc *, int);
static void	cg4b_ldcmap(struct cg4_softc *);

static struct fbdriver cg4_fbdriver = {
	cg4open, cg4close, cg4mmap, cg4gattr,
	cg4gvideo, cg4svideo,
	cg4getcmap, cg4putcmap
};

/*
 * Match a cg4.
 */
static int
cg4match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	int paddr;

	/* XXX: Huge hack due to lack of probe info... */
	/* XXX: Machines that might have a cg4 (gag). */
	/* XXX: Need info on the "P4" register... */
	switch (cpu_machine_id) {

	case SUN3_MACH_110:
		/* XXX: Assume type A. */
		if (ca->ca_paddr == -1)
			ca->ca_paddr = CG4A_DEF_BASE;
		if (bus_peek(ca->ca_bustype, ca->ca_paddr, 1) == -1)
			return (0);
		if (bus_peek(BUS_OBIO, CG4A_OBIO_CMAP, 1) == -1)
			return (0);
		break;

	case SUN3_MACH_60:
		/* XXX: Assume type A. */
		if (ca->ca_paddr == -1)
			ca->ca_paddr = CG4B_DEF_BASE;
		paddr = ca->ca_paddr;
		if (bus_peek(ca->ca_bustype, paddr, 1) == -1)
			return (0);
        /* Make sure we're color */
		paddr += CG4B_OFF_PIXMAP;
		if (bus_peek(ca->ca_bustype, paddr, 1) == -1)
			return (0);
		break;

	default:
		return (0);
	}

	return (1);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
static void
cg4attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct cg4_softc *sc = (struct cg4_softc *)self;
	struct fbdevice *fb = &sc->sc_fb;
	struct confargs *ca = args;
	struct fbtype *fbt;

	/* XXX: should do better than this... */
	switch (cpu_machine_id) {
	case SUN3_MACH_110:
		sc->sc_cg4type = CG4_TYPE_A;
		break;
	case SUN3_MACH_60:
	default:
		sc->sc_cg4type = CG4_TYPE_B;
	}

	fb->fb_driver = &cg4_fbdriver;
	fb->fb_private = sc;
	fb->fb_name = sc->sc_dev.dv_xname;

	fbt = &fb->fb_fbtype;
	fbt->fb_type = FBTYPE_SUN4COLOR;
	fbt->fb_depth = 8;
	fbt->fb_cmsize = 256;

	fbt->fb_width = 1152;
	fbt->fb_height = 900;
	fbt->fb_size = CG4_MMAP_SIZE;

	switch (sc->sc_cg4type) {
	case CG4_TYPE_A:	/* Sun3/110 */
		sc->sc_va_cmap = bus_mapin(BUS_OBIO, CG4A_OBIO_CMAP,
		                           sizeof(struct amd_regs));
		sc->sc_pa_overlay = ca->ca_paddr + CG4A_OFF_OVERLAY;
		sc->sc_pa_enable  = ca->ca_paddr + CG4A_OFF_ENABLE;
		sc->sc_pa_pixmap  = ca->ca_paddr + CG4A_OFF_PIXMAP;
		sc->sc_btcm = NULL;
		cg4a_init(sc);
		break;

	case CG4_TYPE_B:	/* Sun3/60 */
	default:
		sc->sc_va_cmap = (struct bt_regs *)
			bus_mapin(ca->ca_bustype, ca->ca_paddr,
					  sizeof(struct bt_regs *));
		sc->sc_pa_overlay = ca->ca_paddr + CG4B_OFF_OVERLAY;
		sc->sc_pa_enable  = ca->ca_paddr + CG4B_OFF_ENABLE;
		sc->sc_pa_pixmap  = ca->ca_paddr + CG4B_OFF_PIXMAP;
		sc->sc_btcm = malloc(sizeof(union bt_cmap), M_DEVBUF, M_WAITOK);
		cg4b_init(sc);
		break;
	}

	printf(" (%dx%d)\n", fbt->fb_width, fbt->fb_height);
	fb_attach(fb, 4);
}

int
cg4open(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgfour_cd.cd_ndevs || cgfour_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
cg4close(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
cg4ioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cg4_softc *sc = cgfour_cd.cd_devs[minor(dev)];

	return (fbioctlfb(&sc->sc_fb, cmd, data));
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * X11 expects its mmap'd region to look like this:
 * 	128k overlay data memory
 * 	128k overlay enable bitmap
 * 	1024k color memory
 *
 * The hardware really looks like this (starting at ca_paddr)
 *  4 bytes Brooktree DAC registers
 *  2MB-4 gap
 * 	128k overlay memory
 * 	1920k gap
 * 	128k overlay-enable bitmap
 * 	1920k gap
 * 	1024k color memory
 */
paddr_t
cg4mmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	struct cg4_softc *sc = cgfour_cd.cd_devs[minor(dev)];
	register int physbase;

	if (off & PGOFSET)
		panic("cg4mmap");

	if ((unsigned)off >= CG4_MMAP_SIZE)
		return (-1);

	if (off < 0x40000) {
		if (off < 0x20000) {
			physbase = sc->sc_pa_overlay;
		} else {
			/* enable plane */
			off -= 0x20000;
			physbase = sc->sc_pa_enable;
		}
	} else {
		/* pixel map */
		off -= 0x40000;
		physbase = sc->sc_pa_pixmap;
	}

	/*
	 * I turned on PMAP_NC here to disable the cache as I was
	 * getting horribly broken behaviour with it on.
	 */
	return ((physbase + off) | PMAP_NC);
}

/*
 * Internal ioctl functions.
 */

/* FBIOGATTR: */
static int
cg4gattr(fb, fba)
	struct fbdevice *fb;
	struct fbgattr *fba;
{

	fba->real_type = fb->fb_fbtype.fb_type;
	fba->owner = 0;		/* XXX - TIOCCONS stuff? */
	fba->fbtype = fb->fb_fbtype;
	fba->sattr.flags = 0;
	fba->sattr.emu_type = fb->fb_fbtype.fb_type;
	fba->sattr.dev_specific[0] = -1;
	fba->emu_types[0] = fb->fb_fbtype.fb_type;
	fba->emu_types[1] = -1;
	return (0);
}

/* FBIOGVIDEO: */
static int
cg4gvideo(fb, on)
	struct fbdevice *fb;
	int *on;
{
	struct cg4_softc *sc = fb->fb_private;

	*on = !sc->sc_blanked;
	return (0);
}

/* FBIOSVIDEO: */
static int
cg4svideo(fb, on)
	struct fbdevice *fb;
	int *on;
{
	struct cg4_softc *sc = fb->fb_private;
	int state;

	state = *on;
	if (sc->sc_cg4type == CG4_TYPE_A)
		cg4a_svideo(sc, state);
	else
		cg4b_svideo(sc, state);
	return (0);
}

/*
 * FBIOGETCMAP:
 * Copy current colormap out to user space.
 */
static int
cg4getcmap(fb, fbcm)
	struct fbdevice *fb;
	struct fbcmap *fbcm;
{
	struct cg4_softc *sc = fb->fb_private;
	struct soft_cmap *cm = &sc->sc_cmap;
	u_int start, count;
	int error;

	start = fbcm->index;
	count = fbcm->count;
	if (start >= CMAP_SIZE || count > CMAP_SIZE - start)
		return (EINVAL);

	if ((error = copyout(&cm->r[start], fbcm->red, count)) != 0)
		return (error);

	if ((error = copyout(&cm->g[start], fbcm->green, count)) != 0)
		return (error);

	if ((error = copyout(&cm->b[start], fbcm->blue, count)) != 0)
		return (error);

	return (0);
}

/*
 * FBIOPUTCMAP:
 * Copy new colormap from user space and load.
 */
static int 
cg4putcmap(fb, fbcm)
	struct fbdevice *fb;
	struct fbcmap *fbcm;
{
	struct cg4_softc *sc = fb->fb_private;
	struct soft_cmap *cm = &sc->sc_cmap;
	u_int start, count;
	int error;

	start = fbcm->index;
	count = fbcm->count;
	if (start >= CMAP_SIZE || count > CMAP_SIZE - start)
		return (EINVAL);

	if ((error = copyin(fbcm->red, &cm->r[start], count)) != 0)
		return (error);

	if ((error = copyin(fbcm->green, &cm->g[start], count)) != 0)
		return (error);

	if ((error = copyin(fbcm->blue, &cm->b[start], count)) != 0)
		return (error);

	if (sc->sc_cg4type == CG4_TYPE_A)
		cg4a_ldcmap(sc);
	else
		cg4b_ldcmap(sc);

	return (0);
}

/****************************************************************
 * Routines for the "Type A" hardware
 ****************************************************************/

static void
cg4a_init(sc)
	struct cg4_softc *sc;
{
	volatile struct amd_regs *ar = sc->sc_va_cmap;
	struct soft_cmap *cm = &sc->sc_cmap;
	int i;

	/* grab initial (current) color map */
	for(i = 0; i < 256; i++) {
		cm->r[i] = ar->r[i];
		cm->g[i] = ar->g[i];
		cm->b[i] = ar->b[i];
	}
}

static void
cg4a_ldcmap(sc)
	struct cg4_softc *sc;
{
	volatile struct amd_regs *ar = sc->sc_va_cmap;
	struct soft_cmap *cm = &sc->sc_cmap;
	int i;

	/*
	 * Now blast them into the chip!
	 * XXX Should use retrace interrupt!
	 * Just set a "need load" bit and let the
	 * retrace interrupt handler do the work.
	 */
	for(i = 0; i < 256; i++) {
		ar->r[i] = cm->r[i];
		ar->g[i] = cm->g[i];
		ar->b[i] = cm->b[i];
	}
}

static void
cg4a_svideo(sc, on)
	struct cg4_softc *sc;
	int on;
{
	volatile struct amd_regs *ar = sc->sc_va_cmap;
	int i;

	if ((on == 0) && (sc->sc_blanked == 0)) {
		/* Turn OFF video (make it blank). */
		sc->sc_blanked = 1;
		/* Load fake "all zero" colormap. */
		for (i = 0; i < 256; i++) {
			ar->r[i] = 0;
			ar->g[i] = 0;
			ar->b[i] = 0;
		}
	}

	if ((on != 0) && (sc->sc_blanked != 0)) {
		/* Turn video back ON (unblank). */
		sc->sc_blanked = 0;
		/* Restore normal colormap. */
		cg4a_ldcmap(sc);
	}
}


/****************************************************************
 * Routines for the "Type B" hardware
 ****************************************************************/

static void
cg4b_init(sc)
	struct cg4_softc *sc;
{
	volatile struct bt_regs *bt = sc->sc_va_cmap;
	struct soft_cmap *cm = &sc->sc_cmap;
	union bt_cmap *btcm = sc->sc_btcm;
	int i;

	/*
	 * BT458 chip initialization as described in Brooktree's
	 * 1993 Graphics and Imaging Product Databook (DB004-1/93).
	 */
	bt->bt_addr = 0x04;	/* select read mask register */
	bt->bt_ctrl = 0xff;	/* all planes on */
	bt->bt_addr = 0x05;	/* select blink mask register */
	bt->bt_ctrl = 0x00;	/* all planes non-blinking */
	bt->bt_addr = 0x06;	/* select command register */
	bt->bt_ctrl = 0x43;	/* palette enabled, overlay planes enabled */
	bt->bt_addr = 0x07;	/* select test register */
	bt->bt_ctrl = 0x00;	/* set test mode */

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < (256 * 3 / 4); i++) {
		btcm->cm_chip[i] = bt->bt_cmap;
	}

	/* Transpose into S/W form. */
	for (i = 0; i < 256; i++) {
		cm->r[i] = btcm->cm_map[i][0];
		cm->g[i] = btcm->cm_map[i][1];
		cm->b[i] = btcm->cm_map[i][2];
	}
}

static void
cg4b_ldcmap(sc)
	struct cg4_softc *sc;
{
	volatile struct bt_regs *bt = sc->sc_va_cmap;
	struct soft_cmap *cm = &sc->sc_cmap;
	union bt_cmap *btcm = sc->sc_btcm;
	int i;

	/*
	 * Now blast them into the chip!
	 * XXX Should use retrace interrupt!
	 * Just set a "need load" bit and let the
	 * retrace interrupt handler do the work.
	 */

	/* Transpose into H/W form. */
	for (i = 0; i < 256; i++) {
		btcm->cm_map[i][0] = cm->r[i];
		btcm->cm_map[i][1] = cm->g[i];
		btcm->cm_map[i][2] = cm->b[i];
	}

	bt->bt_addr = 0;
	for (i = 0; i < (256 * 3 / 4); i++) {
		bt->bt_cmap = btcm->cm_chip[i];
	}
}

static void
cg4b_svideo(sc, on)
	struct cg4_softc *sc;
	int on;
{
	volatile struct bt_regs *bt = sc->sc_va_cmap;
	int i;

	if ((on == 0) && (sc->sc_blanked == 0)) {
		/* Turn OFF video (make it blank). */
		sc->sc_blanked = 1;
		/* Load fake "all zero" colormap. */
		bt->bt_addr = 0;
		for (i = 0; i < (256 * 3 / 4); i++)
			bt->bt_cmap = 0;
	}

	if ((on != 0) && (sc->sc_blanked != 0)) {
		/* Turn video back ON (unblank). */
		sc->sc_blanked = 0;
		/* Restore normal colormap. */
		cg4b_ldcmap(sc);
	}
}


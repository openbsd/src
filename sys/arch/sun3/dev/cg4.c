/*	$NetBSD: cg4.c,v 1.7 1996/03/17 02:03:45 thorpej Exp $	*/

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
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>

#include "fbvar.h"
#include "btreg.h"
#include "btvar.h"
#include "cg4reg.h"

extern unsigned char cpu_machine_id;

/* per-display variables */
struct cg4_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	volatile struct bt_regs *sc_bt;	/* Brooktree registers */
	int 	sc_phys;		/* display RAM (phys addr) */
	int 	sc_blanked;		/* true if blanked */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	cg4attach __P((struct device *, struct device *, void *));
static int	cg4match __P((struct device *, void *, void *));

struct cfattach cgfour_ca = {
	sizeof(struct cg4_softc), cg4match, cg4attach
};

struct cfdriver cgfour_cd = {
	NULL, "cgfour", DV_DULL
};

/* frame buffer generic driver */
int cg4open(), cg4close(), cg4mmap();

static int  cg4gattr __P((struct fbdevice *, struct fbgattr *));
static int  cg4gvideo __P((struct fbdevice *, int *));
static int	cg4svideo __P((struct fbdevice *, int *));
static int	cg4getcmap __P((struct fbdevice *, struct fbcmap *));
static int	cg4putcmap __P((struct fbdevice *, struct fbcmap *));

static struct fbdriver cg4fbdriver = {
	cg4open, cg4close, cg4mmap, cg4gattr,
	cg4gvideo, cg4svideo,
	cg4getcmap, cg4putcmap };

static void cg4loadcmap __P((struct cg4_softc *, int, int));

/*
 * Match a cg4.
 */
static int
cg4match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	int paddr, x;

	/* XXX - Huge hack due to lack of probe info... */
	switch (cpu_machine_id) {
		/* Machines that might have a cg4 (gag). */
	case SUN3_MACH_50:
	case SUN3_MACH_60:
	case SUN3_MACH_110:
		break;
	default:
		return (0);
	}

	if (ca->ca_paddr == -1)
		ca->ca_paddr = 0xFF200000;

	paddr = ca->ca_paddr;
	x = bus_peek(ca->ca_bustype, paddr, 1);
	if (x == -1)
		return (0);

	paddr += CG4REG_PIXMAP;
	x = bus_peek(ca->ca_bustype, paddr, 1);
	if (x == -1)
		return (0);

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
	volatile struct bt_regs *bt;
	int i, ramsize, pa;

	fb->fb_driver = &cg4fbdriver;
	fb->fb_private = sc;
	fb->fb_name = sc->sc_dev.dv_xname;

	fbt = &fb->fb_fbtype;
	fbt->fb_type = FBTYPE_SUN4COLOR;
	fbt->fb_depth = 8;
	fbt->fb_cmsize = 256;

	fbt->fb_width = 1152;
	fbt->fb_height = 900;
	fbt->fb_size = CG4_MMAP_SIZE;

	sc->sc_phys = ca->ca_paddr;
	sc->sc_bt = (struct bt_regs *)
		bus_mapin(ca->ca_bustype, ca->ca_paddr,
				  sizeof(struct bt_regs *));

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < (256 * 3 / 4); i++)
		sc->sc_cmap.cm_chip[i] = bt->bt_cmap;

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
 * 	128k overlay memory
 * 	128k overlay-enable bitmap
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
int
cg4mmap(dev, off, prot)
	dev_t dev;
	register int off;
	int prot;
{
	struct cg4_softc *sc = cgfour_cd.cd_devs[minor(dev)];
	register int physbase;

	if (off & PGOFSET)
		panic("cg4mmap");

	if ((unsigned)off >= CG4_MMAP_SIZE)
		return (-1);

	physbase = sc->sc_phys;
	if (off < 0x40000) {
		if (off < 0x20000) {
			/* overlay plane */
			physbase += CG4REG_OVERLAY;
		} else {
			/* enable plane */
			off -= 0x20000;
			physbase += CG4REG_ENABLE;
		}
	} else {
		/* pixel map */
		off -= 0x40000;
		physbase += CG4REG_PIXMAP;
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
static int  cg4gattr(fb, fba)
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
static int  cg4gvideo(fb, on)
	struct fbdevice *fb;
	int *on;
{
	struct cg4_softc *sc = fb->fb_private;

	*on = !sc->sc_blanked;
	return (0);
}

/* FBIOSVIDEO: */
static int cg4svideo(fb, on)
	struct fbdevice *fb;
	int *on;
{
	struct cg4_softc *sc = fb->fb_private;
	register volatile struct bt_regs *bt = sc->sc_bt;

	if ((*on == 0) && (sc->sc_blanked == 0)) {
		/* Turn OFF video (blank it). */
		bt->bt_addr = 0x06;	/* command reg */
		bt->bt_ctrl = 0x70;	/* overlay plane */
		bt->bt_addr = 0x04;	/* read mask */
		bt->bt_ctrl = 0x00;	/* color planes */
		/*
		 * Set color 0 to black -- note that this overwrites
		 * R of color 1.
		 */
		bt->bt_addr = 0;
		bt->bt_cmap = 0;

		sc->sc_blanked = 1;
	}

	if ((*on != 0) && (sc->sc_blanked != 0)) {
		/* Turn video back ON (unblank). */
		sc->sc_blanked = 0;

		/* restore color 0 (and R of color 1) */
		bt->bt_addr = 0;
		bt->bt_cmap = sc->sc_cmap.cm_chip[0];

		/* restore read mask */
		bt->bt_addr = 0x06;	/* command reg */
		bt->bt_ctrl = 0x73;	/* overlay plane */
		bt->bt_addr = 0x04;	/* read mask */
		bt->bt_ctrl = 0xff;	/* color planes */
	}
	return (0);
}

/* FBIOGETCMAP: */
static int cg4getcmap(fb, cmap)
	struct fbdevice *fb;
	struct fbcmap *cmap;
{
	struct cg4_softc *sc = fb->fb_private;

	return (bt_getcmap(cmap, &sc->sc_cmap, 256));
}

/* FBIOPUTCMAP: */
static int cg4putcmap(fb, cmap)
	struct fbdevice *fb;
	struct fbcmap *cmap;
{
	struct cg4_softc *sc = fb->fb_private;
	int error;

	/* copy to software map */
	error = bt_putcmap(cmap, &sc->sc_cmap, 256);
	if (error == 0) {
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		cg4loadcmap(sc, cmap->index, cmap->count);
	}
	return (error);
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cg4loadcmap(sc, start, ncolors)
	struct cg4_softc *sc;
	int start, ncolors;
{
	volatile struct bt_regs *bt;
	u_int *ip;
	int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = sc->sc_bt;
	bt->bt_addr = BT_D4M4(start);
	while (--count >= 0)
		bt->bt_cmap = *ip++;
}

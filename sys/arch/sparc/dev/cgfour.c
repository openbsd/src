/*	$OpenBSD: cgfour.c,v 1.9 1998/11/20 15:57:21 deraadt Exp $	*/
/*	$NetBSD: cgfour.c,v 1.13 1997/05/24 20:16:06 pk Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1995 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt.
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
 *	from @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgfour) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <vm/vm.h>

#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/fbvar.h>
#include <machine/eeprom.h>
#include <machine/conf.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/pfourreg.h>

/* per-display variables */
struct cgfour_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	struct rom_reg	sc_phys;	/* display RAM (phys addr) */
	volatile struct fbcontrol *sc_fbc;	/* Brooktree registers */
	int	sc_bustype;		/* type of bus we live on */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	cgfourattach __P((struct device *, struct device *, void *));
static int	cgfourmatch __P((struct device *, void *, void *));
#if defined(SUN4)
static void	cgfourunblank __P((struct device *));
#endif

/* cdevsw prototypes */
cdev_decl(cgfour);

struct cfattach cgfour_ca = {
	sizeof(struct cgfour_softc), cgfourmatch, cgfourattach
};

struct cfdriver cgfour_cd = {
	NULL, "cgfour", DV_DULL
};

#if defined(SUN4)
/* frame buffer generic driver */
static struct fbdriver cgfourfbdriver = {
	cgfourunblank, cgfouropen, cgfourclose, cgfourioctl, cgfourmmap
};

extern int fbnode;
extern struct tty *fbconstty;

static void cgfourloadcmap __P((struct cgfour_softc *, int, int));
static int cgfour_get_video __P((struct cgfour_softc *));
static void cgfour_set_video __P((struct cgfour_softc *, int));
#endif

/*
 * Match a cgfour.
 */
int
cgfourmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	/*
	 * Only exists on a sun4.
	 */
	if (!CPU_ISSUN4)
		return (0);

	/*
	 * Only exists on obio.
	 */
	if (ca->ca_bustype != BUS_OBIO)
		return (0);

	/*
	 * Make sure there's hardware there.
	 */
	if (probeget(ra->ra_vaddr, 4) == -1)
		return (0);

#if defined(SUN4)
	/*
	 * Check the pfour register.
	 */
	if (fb_pfour_id(ra->ra_vaddr) == PFOUR_ID_COLOR8P1) {
		cf->cf_flags |= FB_PFOUR;
		return (1);
	}
#endif

	return (0);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgfourattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
#if defined(SUN4)
	register struct cgfour_softc *sc = (struct cgfour_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize, i;
	register volatile struct bt_regs *bt;
	struct fbdevice *fb = &sc->sc_fb;
	int isconsole;

	fb->fb_driver = &cgfourfbdriver;
	fb->fb_device = &sc->sc_dev;
	fb->fb_type.fb_type = FBTYPE_SUN4COLOR;
	fb->fb_flags = sc->sc_dev.dv_cfdata->cf_flags;

	/*
	 * Only pfour cgfours, thank you...
	 */
	if ((ca->ca_bustype != BUS_OBIO) ||
	    ((fb->fb_flags & FB_PFOUR) == 0)) {
		printf("%s: ignoring; not a pfour\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Map the pfour register. */
	fb->fb_pfour = (volatile u_int32_t *)
		mapiodev(ca->ca_ra.ra_reg, 0, sizeof(u_int32_t));

	ramsize = PFOUR_COLOR_OFF_END - PFOUR_COLOR_OFF_OVERLAY;

	fb->fb_type.fb_depth = 8;
	fb_setsize(fb, fb->fb_type.fb_depth, 1152, 900, node, ca->ca_bustype);

	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = ramsize;
	printf(": cgfour/p4, %d x %d", fb->fb_type.fb_width,
	    fb->fb_type.fb_height);

	isconsole = 0;

	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == EE_CONS_P4OPT)
			isconsole = (fbconstty != NULL);
	}

#if 0
	/*
	 * We don't do any of the console handling here.  Instead,
	 * we let the bwtwo driver pick up the overlay plane and
	 * use it instead.  Rconsole should have better performance
	 * with the 1-bit depth.
	 *	-- Jason R. Thorpe <thorpej@NetBSD.ORG>
	 */

	/*
	 * When the ROM has mapped in a cgfour display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */

	if (isconsole) {
		/* XXX this is kind of a waste */
		fb->fb_pixels = mapiodev(ca->ca_ra.ra_reg,
					 PFOUR_COLOR_OFF_OVERLAY, ramsize);
	}
#endif

	/* Map the Brooktree. */
	sc->sc_fbc = (volatile struct fbcontrol *)
		mapiodev(ca->ca_ra.ra_reg,
			 PFOUR_COLOR_OFF_CMAP, sizeof(struct fbcontrol));

	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	/* grab initial (current) color map */
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	BT_INIT(bt, 24);

#if 0	/* See above. */
	if (isconsole) {
		printf(" (console)\n");
#if defined(RASTERCONSOLE) && 0	/* XXX been told it doesn't work well. */
		fbrcons_init(fb);
#endif
	} else
#endif /* 0 */
		printf("\n");

	/*
	 * Even though we're not using rconsole, we'd still like
	 * to notice if we're the console framebuffer.
	 */
	fb_attach(fb, isconsole);
#endif
}

int
cgfouropen(dev, flags, mode, p)
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
cgfourclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
cgfourioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
#if defined(SUN4)
	register struct cgfour_softc *sc = cgfour_cd.cd_devs[minor(dev)];
	register struct fbgattr *fba;
	int error;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
		fba = (struct fbgattr *)data;
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		break;

	case FBIOGETCMAP:
		return (bt_getcmap((struct fbcmap *)data, &sc->sc_cmap, 256));

	case FBIOPUTCMAP:
		/* copy to software map */
#define p ((struct fbcmap *)data)
		error = bt_putcmap(p, &sc->sc_cmap, 256);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		cgfourloadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = cgfour_get_video(sc);
		break;

	case FBIOSVIDEO:
		cgfour_set_video(sc, *(int *)data);
		break;

	default:
		return (ENOTTY);
	}
#endif
	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * the cg4 maps it's overlay plane for 128K, followed by the enable
 * plane for 128K, followed by the colour plane (for as much colour
 * as their is.)
 *
 * As well, mapping at an offset of 0x04000000 causes the cg4 to map
 * only it's colour plane, at 0.
 */
int
cgfourmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	register struct cgfour_softc *sc = cgfour_cd.cd_devs[minor(dev)];
	int poff;

#define START_ENABLE	(128*1024)
#define START_COLOR	((128*1024) + (128*1024))
#define COLOR_SIZE	(sc->sc_fb.fb_type.fb_width * \
			    sc->sc_fb.fb_type.fb_height)
#define END_COLOR	(START_COLOR + COLOR_SIZE)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgfourmap");

	if (off < 0)
		return (-1);
	if ((u_int)off >= NOOVERLAY) {
		off -= NOOVERLAY;

		/*
		 * X11 maps a huge chunk of the frame buffer; far more than
		 * there really is. We compensate by double-mapping the
		 * first page for as many other pages as it wants
		 */
		while ((u_int)off >= COLOR_SIZE)
			off -= COLOR_SIZE;	/* XXX thorpej ??? */

		poff = off + PFOUR_COLOR_OFF_COLOR;
	} else if ((u_int)off < START_ENABLE) {
		/*
		 * in overlay plane
		 */
		poff = PFOUR_COLOR_OFF_OVERLAY + off;
	} else if ((u_int)off < START_COLOR) {
		/*
		 * in enable plane
		 */
		poff = (off - START_ENABLE) + PFOUR_COLOR_OFF_ENABLE;
	} else if ((u_int)off < sc->sc_fb.fb_type.fb_size) {
		/*
		 * in colour plane
		 */
		poff = (off - START_COLOR) + PFOUR_COLOR_OFF_COLOR;
	} else
		return (-1);

	return (REG2PHYS(&sc->sc_phys, poff) | PMAP_NC);
}

#if defined(SUN4)
/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
static void
cgfourunblank(dev)
	struct device *dev;
{

	cgfour_set_video((struct cgfour_softc *)dev, 1);
}

static int
cgfour_get_video(sc)
	struct cgfour_softc *sc;
{

	return (fb_pfour_get_video(&sc->sc_fb));
}

static void
cgfour_set_video(sc, enable)
	struct cgfour_softc *sc;
	int enable;
{

	fb_pfour_set_video(&sc->sc_fb, enable);
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cgfourloadcmap(sc, start, ncolors)
	register struct cgfour_softc *sc;
	register int start, ncolors;
{
	register volatile struct bt_regs *bt;
	register u_int *ip, i;
	register int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = BT_D4M4(start) << 24;
	while (--count >= 0) {
		i = *ip++;
		/* hardware that makes one want to pound boards with hammers */
		bt->bt_cmap = i;
		bt->bt_cmap = i << 8;
		bt->bt_cmap = i << 16;
		bt->bt_cmap = i << 24;
	}
}
#endif

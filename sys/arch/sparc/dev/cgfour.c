/*	$NetBSD: cgfour.c,v 1.12 1994/11/23 07:02:07 deraadt Exp $ */

/*
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
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>

#include <vm/vm.h>

#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/cgfourreg.h>
#include <sparc/dev/pfourreg.h>

/* per-display variables */
struct cgfour_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	volatile struct bt_regs *sc_bt;	/* Brooktree registers */
	caddr_t	sc_phys;		/* display RAM (phys addr) */
	int	sc_blanked;		/* true if blanked */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	cgfourattach __P((struct device *, struct device *, void *));
static int	cgfourmatch __P((struct device *, void *, void *));
int		cgfouropen __P((dev_t, int, int, struct proc *));
int		cgfourclose __P((dev_t, int, int, struct proc *));
int		cgfourioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int		cgfourmmap __P((dev_t, int, int));
static void	cgfourunblank __P((struct device *));

struct cfdriver cgfourcd = {
	NULL, "cgfour", cgfourmatch, cgfourattach,
	DV_DULL, sizeof(struct cgfour_softc)
};

/* frame buffer generic driver */
static struct fbdriver cgfourfbdriver = {
	cgfourunblank, cgfouropen, cgfourclose, cgfourioctl, cgfourmmap
};

extern int fbnode;
extern struct tty *fbconstty;

static void cgfourloadcmap __P((struct cgfour_softc *, int, int));

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
	if (PFOUR_ID(ra->ra_pfour) == PFOUR_ID_COLOR8P1)
		return (1);
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
	register struct cgfour_softc *sc = (struct cgfour_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize, i;
	register volatile struct bt_regs *bt;
	register struct cgfour_all *p;
	int isconsole;

	sc->sc_fb.fb_driver = &cgfourfbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN4COLOR;

	pfour_reset();
	pfour_videosize(ca->ca_ra.ra_pfour, &sc->sc_fb.fb_type.fb_width,
	    &sc->sc_fb.fb_type.fb_height);

	sc->sc_fb.fb_linebytes = sc->sc_fb.fb_type.fb_width;

	ramsize = CG4REG_END - CG4REG_OVERLAY;
	sc->sc_fb.fb_type.fb_depth = 8;
	sc->sc_fb.fb_type.fb_cmsize = 256;
	sc->sc_fb.fb_type.fb_size = ramsize;
	printf(": %d x %d", sc->sc_fb.fb_type.fb_width,
	    sc->sc_fb.fb_type.fb_height);

	/*
	 * When the ROM has mapped in a cgfour display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 *
	 * XXX: it is insane to map the full 0x800000 space, when
	 * the mmap code down below doesn't use it all. Ridiculous!
	 */
	isconsole = node == fbnode && fbconstty != NULL;
	p = (struct cgfour_all *)ca->ca_ra.ra_paddr;
	if (ca->ca_ra.ra_vaddr == NULL) {
		/* this probably cannot happen, but what the heck */
		ca->ca_ra.ra_vaddr = mapiodev(p->ba_overlay, ramsize,
		    ca->ca_bustype);
	}
	sc->sc_fb.fb_pixels = (char *)((int)ca->ca_ra.ra_vaddr +
	    CG4REG_COLOUR - CG4REG_OVERLAY);

	sc->sc_bt = bt = (volatile struct bt_regs *)
	    mapiodev((caddr_t)&p->ba_btreg, sizeof(p->ba_btreg),
	    ca->ca_bustype);
	sc->sc_phys = p->ba_overlay;

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	/* make sure we are not blanked (see cgfourunblank) */
	bt->bt_addr = 0x06 << 24; 	/* command reg */
	bt->bt_ctrl = 0x73 << 24;	/* overlay plane */
	bt->bt_addr = 0x04 << 24;	/* read mask */
	bt->bt_ctrl = 0xff << 24;	/* color planes */

	if (isconsole) {
		printf(" (console)\n");
	} else
		printf("\n");
	if ((node == fbnode && cputyp != CPU_SUN4) ||
	    (isconsole && cputyp == CPU_SUN4))
		fb_attach(&sc->sc_fb);
}

int
cgfouropen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgfourcd.cd_ndevs || cgfourcd.cd_devs[unit] == NULL)
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
	register struct cgfour_softc *sc = cgfourcd.cd_devs[minor(dev)];
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
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		if (*(int *)data)
			cgfourunblank(&sc->sc_dev);
		else if (!sc->sc_blanked) {
			register volatile struct bt_regs *bt;

			bt = sc->sc_bt;
			bt->bt_addr = 0x06 << 24;	/* command reg */
			bt->bt_ctrl = 0x70 << 24;	/* overlay plane */
			bt->bt_addr = 0x04 << 24;	/* read mask */
			bt->bt_ctrl = 0x00 << 24;	/* color planes */
			/*
			 * Set color 0 to black -- note that this overwrites
			 * R of color 1.
			 */
			bt->bt_addr = 0 << 24;
			bt->bt_cmap = 0 << 24;

			sc->sc_blanked = 1;
		}
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
static void
cgfourunblank(dev)
	struct device *dev;
{
	struct cgfour_softc *sc = (struct cgfour_softc *)dev;
	register volatile struct bt_regs *bt;

	if (sc->sc_blanked) {
		sc->sc_blanked = 0;
		bt = sc->sc_bt;
		/* restore color 0 (and R of color 1) */
		bt->bt_addr = 0 << 24;
		bt->bt_cmap = sc->sc_cmap.cm_chip[0];
		bt->bt_cmap = sc->sc_cmap.cm_chip[0] << 8;
		bt->bt_cmap = sc->sc_cmap.cm_chip[0] << 16;

		/* restore read mask */
		bt->bt_addr = 0x06 << 24;	/* command reg */
		bt->bt_ctrl = 0x73 << 24;	/* overlay plane */
		bt->bt_addr = 0x04 << 24;	/* read mask */
		bt->bt_ctrl = 0xff << 24;	/* color planes */
	}
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
	bt = sc->sc_bt;
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
	register struct cgfour_softc *sc = cgfourcd.cd_devs[minor(dev)];
	int poff;

#define START_ENABLE	(128*1024)
#define START_COLOUR	(128*1024 + 128*1024)
#define COLOUR_SIZE	(sc->sc_fb.fb_type.fb_width * sc->sc_fb.fb_type.fb_height)
#define END_COLOUR	(START_COLOUR + COLOUR_SIZE)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgfourmap");

	if ((u_int)off >= NOOVERLAY) {
		off = off - NOOVERLAY;

		/*
		 * X11 maps a huge chunk of the frame buffer; far more than
		 * there really is. We compensate by double-mapping the
		 * first page for as many other pages as it wants
		 */
		while (off >= COLOUR_SIZE)
			off = 0;
		poff = off + (CG4REG_COLOUR - CG4REG_OVERLAY);
	} else if ((u_int)off < START_ENABLE)	/* in overlay plane */
		poff = off;
	else if ((u_int)off < START_COLOUR)	/* in enable plane */
		poff = off + (CG4REG_ENABLE - CG4REG_OVERLAY) - START_ENABLE;
	else if ((u_int)off < (CG4REG_END - CG4REG_OVERLAY)) 	/* in colour plane */ 
		poff = off + (CG4REG_COLOUR - CG4REG_OVERLAY) - START_COLOUR;
	else
		return (-1);
	return ((u_int)sc->sc_phys + poff + PMAP_OBIO + PMAP_NC);
}

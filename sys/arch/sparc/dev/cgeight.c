/*	$NetBSD: cgeight.c,v 1.12 1994/11/23 07:02:07 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	from @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgeight) driver.
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
#include <sparc/dev/cgeightreg.h>
#include <sparc/dev/pfourreg.h>

/* per-display variables */
struct cgeight_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	volatile struct bt_regs *sc_bt;	/* Brooktree registers */
	struct rom_reg	sc_phys;	/* display RAM (phys addr) */
	int	sc_bustype;		/* type of bus we live on */
	int	sc_blanked;		/* true if blanked */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	cgeightattach(struct device *, struct device *, void *);
static int	cgeightmatch(struct device *, void *, void *);
int		cgeightopen __P((dev_t, int, int, struct proc *));
int		cgeightclose __P((dev_t, int, int, struct proc *));
int		cgeightioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int		cgeightmmap __P((dev_t, int, int));
static void	cgeightunblank __P((struct device *));

struct cfdriver cgeightcd = {
	NULL, "cgeight", cgeightmatch, cgeightattach,
	DV_DULL, sizeof(struct cgeight_softc)
};

/* frame buffer generic driver */
static struct fbdriver cgeightfbdriver = {
	cgeightunblank, cgeightopen, cgeightclose, cgeightioctl, cgeightmmap
};

extern int fbnode;
extern struct tty *fbconstty;

static void cgeightloadcmap __P((struct cgeight_softc *, int, int));

/*
 * Match a cgeight.
 */
int
cgeightmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (PFOUR_ID(ra->ra_pfour) == PFOUR_ID_COLOR24)
		return (1);
	return (0);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgeightattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct cgeight_softc *sc = (struct cgeight_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize, i;
	register volatile struct bt_regs *bt;
	register struct cgeight_all *p;
	int isconsole;
#ifdef RASTERCONSOLE
	struct fbdevice fbd;
#endif

	sc->sc_fb.fb_driver = &cgeightfbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_type.fb_type = FBTYPE_MEMCOLOR;

	pfour_reset();
	pfour_videosize(ca->ca_ra.ra_pfour, &sc->sc_fb.fb_type.fb_width,
	    &sc->sc_fb.fb_type.fb_height);

	sc->sc_fb.fb_linebytes = sc->sc_fb.fb_type.fb_width * 4;

	ramsize = CG8REG_END - CG8REG_OVERLAY;
	sc->sc_fb.fb_type.fb_depth = 24;
	sc->sc_fb.fb_type.fb_cmsize = 256;
	sc->sc_fb.fb_type.fb_size = ramsize;
	printf(": %d x %d", sc->sc_fb.fb_type.fb_width,
	    sc->sc_fb.fb_type.fb_height);

	/*
	 * When the ROM has mapped in a cgeight display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 *
	 * XXX: it is insane to map the full 0x800000 space, when
	 * the mmap code down below doesn't want it that way.
	 * Ridiculous!
	 */
	isconsole = node == fbnode && fbconstty != NULL;
	if (ca->ca_ra.ra_vaddr == NULL) {
		/* this probably cannot happen, but what the heck */
		ca->ca_ra.ra_vaddr = mapiodev(ca->ca_ra.ra_reg, 0,
		    ramsize, ca->ca_bustype);
	}
	sc->sc_fb.fb_pixels = (char *)((int)ca->ca_ra.ra_vaddr +
	    CG8REG_COLOUR - CG8REG_OVERLAY);

#define	O(memb) ((u_int)(&((struct cgeight_all *)0)->memb))
	sc->sc_bt = bt = (volatile struct bt_regs *)mapiodev(ca->ca_ra.ra_reg,
	    O(ba_btreg), sizeof(struct bt_regs), ca->ca_bustype);
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	/* tell the enable plane to look at the mono image */
	memset(ca->ca_ra.ra_vaddr, 0xff,
	    sc->sc_fb.fb_type.fb_width * sc->sc_fb.fb_type.fb_height / 8);
#if 0
	memset((caddr_t)((int)ca->ca_ra.ra_vaddr +
	    CG8REG_ENABLE - CG8REG_OVERLAY), 0x00,
	    sc->sc_fb.fb_type.fb_width * sc->sc_fb.fb_type.fb_height / 8);
#endif

	/* grab initial (current) color map */
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		sc->sc_cmap.cm_chip[i] = bt->bt_cmap;
	/* make sure we are not blanked (see cgeightunblank) */
	bt->bt_addr = 0x06;		/* command reg */
	bt->bt_ctrl = 0x73;		/* overlay plane */
	bt->bt_addr = 0x04;		/* read mask */
	bt->bt_ctrl = 0xff;		/* color planes */

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		/*
		 * Like SunOS and the bootrom, we want to do full-screen
		 * text on the overlay plane. But rcons_init() requires
		 * our fbdevice pointer to remain the same; so we hack
		 * the fbdevice, pass it in, and then restore it's
		 * values. Ugly -- should change rcons_init()'s interface.
		 */
		bcopy(&sc->sc_fb, &fbd, sizeof fbd);
		sc->sc_fb.fb_type.fb_depth = 1;
		sc->sc_fb.fb_linebytes = sc->sc_fb.fb_type.fb_width / 8;
		sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddr;
		rcons_init(&fbd);
		bcopy(&fbd, &sc->sc_fb, sizeof fbd);
#endif
	} else
		printf("\n");
	if ((node == fbnode && cputyp != CPU_SUN4) ||
	    (isconsole && cputyp == CPU_SUN4))
		fb_attach(&sc->sc_fb);
}

int
cgeightopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgeightcd.cd_ndevs || cgeightcd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
cgeightclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
cgeightioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
	register struct cgeight_softc *sc = cgeightcd.cd_devs[minor(dev)];
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
		cgeightloadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		if (*(int *)data)
			cgeightunblank(&sc->sc_dev);
		else if (!sc->sc_blanked) {
			register volatile struct bt_regs *bt;

			bt = sc->sc_bt;
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
cgeightunblank(dev)
	struct device *dev;
{
	struct cgeight_softc *sc = (struct cgeight_softc *)dev;
	register volatile struct bt_regs *bt;

	if (sc->sc_blanked) {
		sc->sc_blanked = 0;
		bt = sc->sc_bt;
		/* restore color 0 (and R of color 1) */
		bt->bt_addr = 0;
		bt->bt_cmap = sc->sc_cmap.cm_chip[0];

		/* restore read mask */
		bt->bt_addr = 0x06;	/* command reg */
		bt->bt_ctrl = 0x73;	/* overlay plane */
		bt->bt_addr = 0x04;	/* read mask */
		bt->bt_ctrl = 0xff;	/* color planes */
	}
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cgeightloadcmap(sc, start, ncolors)
	register struct cgeight_softc *sc;
	register int start, ncolors;
{
	register volatile struct bt_regs *bt;
	register u_int *ip;
	register int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = sc->sc_bt;
	bt->bt_addr = BT_D4M4(start);
	while (--count >= 0)
		bt->bt_cmap = *ip++;
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * The cg8 maps it's overlay plane at 0 for 128K, followed by the
 * enable plane for 128K, followed by the colour for as long as it
 * goes. Starting at 8MB, it maps the ramdac for NBPG, then the p4
 * register for NBPG, then the bootrom for 0x40000.
 */
int
cgeightmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	register struct cgeight_softc *sc = cgeightcd.cd_devs[minor(dev)];
	int poff;

#define START_ENABLE	(128*1024)
#define START_COLOUR	(128*1024 + 128*1024)
#define COLOUR_SIZE	(sc->sc_fb.fb_type.fb_width * \
			    sc->sc_fb.fb_type.fb_height * 4)
#define END_COLOUR	(START_COLOUR + COLOUR_SIZE)
#define START_SPECIAL	0x800000
#define PROMSIZE	0x40000
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgeightmap");

	if ((u_int)off >= NOOVERLAY) {
		off = off - NOOVERLAY;

		/*
		 * X11 maps a huge chunk of the frame buffer; far more than
		 * there really is. We compensate by double-mapping the
		 * first page for as many other pages as it wants
		 */
		while (off >= COLOUR_SIZE)
			off = 0;
		poff = off + (CG8REG_COLOUR - CG8REG_OVERLAY);
	} else if ((u_int)off < START_ENABLE)	/* in overlay plane */
		poff = off;
	else if ((u_int)off < START_COLOUR)	/* in enable plane */
		poff = off + (CG8REG_ENABLE - CG8REG_OVERLAY) - START_ENABLE;
	else if ((u_int)off < END_COLOUR) 	/* in colour plane */ 
		poff = off + (CG8REG_COLOUR - CG8REG_OVERLAY) - START_COLOUR;
	else if ((u_int)off < START_SPECIAL)	/* hole */
		poff = 0;
	else if ((u_int)off == START_SPECIAL)	/* colour map */
		poff = CG8REG_CMAP;
	else if ((u_int)off == START_SPECIAL + NBPG) /* p4 register */
		poff = PFOUR_REG;
	else if ((u_int)off > START_SPECIAL + NBPG*2 &&
	    (u_int) off < START_SPECIAL + NBPG*2 + PROMSIZE)	/* rom */
		poff = CG8REG_P4REG + 0x8000 + off - START_SPECIAL + NBPG*2;
	else
		return (-1);
	/*
	 * I turned on PMAP_NC here to disable the cache as I was
	 * getting horribly broken behaviour with it on.
	 */
	return (REG2PHYS(&sc->sc_phys, off, sc->sc_bustype) | PMAP_NC);
}

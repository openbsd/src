/*	$OpenBSD: cgthree.c,v 1.8 1998/11/20 15:57:22 deraadt Exp $	*/
/*	$NetBSD: cgthree.c,v 1.33 1997/05/24 20:16:11 pk Exp $ */

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
 *	@(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgthree) driver.
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
#include <machine/cpu.h>
#include <machine/conf.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/cgthreereg.h>
#include <sparc/dev/sbusvar.h>

/* per-display variables */
struct cgthree_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	struct rom_reg	sc_phys;	/* phys address description */
	volatile struct fbcontrol *sc_fbc;	/* Brooktree registers */
	int	sc_bustype;		/* type of bus we live on */
	int	sc_isrdi;		/* is an RDI */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	cgthreeattach(struct device *, struct device *, void *);
static int	cgthreematch(struct device *, void *, void *);
static void	cgthreeunblank(struct device *);

/* cdevsw prototypes */
cdev_decl(cgthree);

struct cfattach cgthree_ca = {
	sizeof(struct cgthree_softc), cgthreematch, cgthreeattach
};

struct cfdriver cgthree_cd = {
	NULL, "cgthree", DV_DULL
};

/* frame buffer generic driver */
static struct fbdriver cgthreefbdriver = {
	cgthreeunblank, cgthreeopen, cgthreeclose, cgthreeioctl, cgthreemmap
};

extern int fbnode;
extern struct tty *fbconstty;

static void cgthreeloadcmap __P((struct cgthree_softc *, int, int));
static void cgthree_set_video __P((struct cgthree_softc *, int));
static int cgthree_get_video __P((struct cgthree_softc *));

/* Video control parameters */
struct cg3_videoctrl {
	unsigned char	sense;		/* Monitor sense value */
	unsigned char	vctrl[12];
} cg3_videoctrl[] = {
/* Missing entries: sense 0x10, 0x30, 0x50 */
	{ 0x40, /* this happens to be my 19'' 1152x900 gray-scale monitor */
	   {0xbb, 0x2b, 0x3, 0xb, 0xb3, 0x3, 0xaf, 0x2b, 0x2, 0xa, 0xff, 0x1}
	},
	{ 0x00, /* default? must be last */
	   {0xbb, 0x2b, 0x3, 0xb, 0xb3, 0x3, 0xaf, 0x2b, 0x2, 0xa, 0xff, 0x1}
	}
};

/*
 * Match a cgthree.
 */
int
cgthreematch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) && 
	    strcmp("cgRDI",ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return(1);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 4) != -1);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
cgthreeattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct cgthree_softc *sc = (struct cgthree_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize, i;
	register volatile struct bt_regs *bt;
	int isconsole;
	int sbus = 1;
	char *nam = NULL;

	sc->sc_fb.fb_driver = &cgthreefbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_flags = sc->sc_dev.dv_cfdata->cf_flags;
	/*
	 * The defaults below match my screen, but are not guaranteed
	 * to be correct as defaults go...
	 */
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN3COLOR;
	switch (ca->ca_bustype) {
	case BUS_OBIO:
		if (CPU_ISSUN4M) {	/* 4m has framebuffer on obio */
			sbus = 0;
			node = ca->ca_ra.ra_node;
			nam = getpropstring(node, "model");
			break;
		}
	case BUS_VME32:
	case BUS_VME16:
		sbus = node = 0;
		nam = "cgthree";
		break;

	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		nam = getpropstring(node, "model");
		break;
	}

	sc->sc_fb.fb_type.fb_depth = 8;
	fb_setsize(&sc->sc_fb, sc->sc_fb.fb_type.fb_depth,
    		1152, 900, node, ca->ca_bustype);
	if(!strcmp(ca->ca_ra.ra_name, "cgRDI")) {
		sc->sc_isrdi = 1;
		nam = "cgRDI";
	}

	ramsize = roundup(sc->sc_fb.fb_type.fb_height * sc->sc_fb.fb_linebytes,
		NBPG);
	sc->sc_fb.fb_type.fb_cmsize = 256;
	sc->sc_fb.fb_type.fb_size = ramsize;
	printf(": %s, %d x %d", nam,
	    sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height);

	/*
	 * When the ROM has mapped in a cgthree display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	isconsole = node == fbnode && fbconstty != NULL;
	if ((sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddr) == NULL && isconsole) {
		/* this probably cannot happen, but what the heck */
		sc->sc_fb.fb_pixels = mapiodev(ca->ca_ra.ra_reg, CG3REG_MEM,
					       ramsize);
	}
	sc->sc_fbc = (volatile struct fbcontrol *)
	    mapiodev(ca->ca_ra.ra_reg, CG3REG_REG,
		     sizeof(struct fbcontrol));

	/* Transfer video magic to board, if it's not running */
	if (!sc->sc_isrdi && (sc->sc_fbc->fbc_ctrl & FBC_TIMING) == 0)
		for (i = 0; i < sizeof(cg3_videoctrl)/sizeof(cg3_videoctrl[0]);
		     i++) {
			volatile struct fbcontrol *fbc = sc->sc_fbc;
			if ((fbc->fbc_status & FBS_MSENSE) ==
			     cg3_videoctrl[i].sense) {
				int j;
				printf(" setting video ctrl");
				for (j = 0; j < 12; j++)
					fbc->fbc_vcontrol[j] =
						cg3_videoctrl[i].vctrl[j];
				fbc->fbc_ctrl |= FBC_TIMING;
				break;
			}
		}

	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	/* grab initial (current) color map */
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3 / 4; i++)
		sc->sc_cmap.cm_chip[i] = bt->bt_cmap;

	/* make sure we are not blanked */
	cgthree_set_video(sc, 1);
	BT_INIT(bt, 0);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		fbrcons_init(&sc->sc_fb);
#endif
	} else
		printf("\n");
	if (sbus)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);
	if (node == fbnode)
		fb_attach(&sc->sc_fb, isconsole);
}

int
cgthreeopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgthree_cd.cd_ndevs || cgthree_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
cgthreeclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
cgthreeioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
	register struct cgthree_softc *sc = cgthree_cd.cd_devs[minor(dev)];
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
		cgthreeloadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = cgthree_get_video(sc);
		break;

	case FBIOSVIDEO:
		cgthree_set_video(sc, *(int *)data);
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
cgthreeunblank(dev)
	struct device *dev;
{

	cgthree_set_video((struct cgthree_softc *)dev, 1);
}

static void
cgthree_set_video(sc, enable)
	struct cgthree_softc *sc;
	int enable;
{

	if (enable)
		sc->sc_fbc->fbc_ctrl |= FBC_VENAB;
	else
		sc->sc_fbc->fbc_ctrl &= ~FBC_VENAB;
}

static int
cgthree_get_video(sc)
	struct cgthree_softc *sc;
{

	return ((sc->sc_fbc->fbc_ctrl & FBC_VENAB) != 0);
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
static void
cgthreeloadcmap(sc, start, ncolors)
	register struct cgthree_softc *sc;
	register int start, ncolors;
{
	register volatile struct bt_regs *bt;
	register u_int *ip;
	register int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = BT_D4M4(start);
	while (--count >= 0)
		bt->bt_cmap = *ip++;
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * The cg3 is mapped starting at 256KB, for pseudo-compatibility with
 * the cg4 (which had an overlay plane in the first 128K and an enable
 * plane in the next 128K).  X11 uses only 256k+ region but tries to
 * map the whole thing, so we repeatedly map the first 256K to the
 * first page of the color screen.  If someone tries to use the overlay
 * and enable regions, they will get a surprise....
 *
 * As well, mapping at an offset of 0x04000000 causes the cg3 to be
 * mapped in flat mode without the cg4 emulation.
 */
int
cgthreemmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	register struct cgthree_softc *sc = cgthree_cd.cd_devs[minor(dev)];
#define START		(128*1024 + 128*1024)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("cgthreemmap");

	if (off < 0)
		return (-1);
	if ((u_int)off >= NOOVERLAY)
		off -= NOOVERLAY;
	else if ((u_int)off >= START)
		off -= START;
	else
		off = 0;
	if ((unsigned)off >= sc->sc_fb.fb_type.fb_size)
		return (-1);
	/*
	 * I turned on PMAP_NC here to disable the cache as I was
	 * getting horribly broken behaviour with it on.
	 */
	return (REG2PHYS(&sc->sc_phys, CG3REG_MEM+off) | PMAP_NC);
}

/*	$OpenBSD: bwtwo.c,v 1.15 1998/11/20 15:57:21 deraadt Exp $	*/
/*	$NetBSD: bwtwo.c,v 1.33 1997/05/24 20:16:02 pk Exp $ */

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
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
 *	@(#)bwtwo.c	8.1 (Berkeley) 6/11/93
 */

/*
 * black&white display (bwtwo) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * P4 and overlay plane support by Jason R. Thorpe <thorpej@NetBSD.ORG>.
 * Overlay plane handling hints and ideas provided by Brad Spencer.
 */

#include <sys/param.h>
#include <sys/systm.h>
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
#include <machine/ctlreg.h>
#include <machine/conf.h>
#include <sparc/sparc/asm.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/bwtworeg.h>
#include <sparc/dev/sbusvar.h>
#if defined(SUN4)
#include <sparc/dev/pfourreg.h>
#endif

/* per-display variables */
struct bwtwo_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	volatile struct fbcontrol *sc_reg;/* control registers */
	struct rom_reg	sc_phys;	/* phys address description */
	int	sc_bustype;		/* type of bus we live on */
	int	sc_pixeloffset;		/* offset to framebuffer */
#if defined(SUN4)
	/*
	 * Additional overlay plane goo.
	 */
	int	sc_ovtype;		/* what kind of color fb? */
#define BWO_NONE	0x00
#define BWO_CGFOUR	0x01
#define BWO_CGEIGHT	0x02
#endif
};

/* autoconfiguration driver */
static void	bwtwoattach __P((struct device *, struct device *, void *));
static int	bwtwomatch __P((struct device *, void *, void *));
static void	bwtwounblank __P((struct device *));
static void	bwtwo_set_video __P((struct bwtwo_softc *, int));
static int	bwtwo_get_video __P((struct bwtwo_softc *));

/* cdevsw prototypes */
cdev_decl(bwtwo);

struct cfattach bwtwo_ca = {
	sizeof(struct bwtwo_softc), bwtwomatch, bwtwoattach
};

struct cfdriver bwtwo_cd = {
	NULL, "bwtwo", DV_DULL
};

/* XXX we do not handle frame buffer interrupts (do not know how) */

/* frame buffer generic driver */
static struct fbdriver bwtwofbdriver = {
	bwtwounblank, bwtwoopen, bwtwoclose, bwtwoioctl, bwtwommap
};

extern int fbnode;
extern struct tty *fbconstty;

/*
 * Match a bwtwo.
 */
int
bwtwomatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4 && cf->cf_unit != 0)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (ca->ca_bustype == BUS_SBUS)
		return(1);

	/*
	 * Make sure there's hardware there.
	 */
	if (probeget(ra->ra_vaddr, 4) == -1)
		return (0);

#if defined(SUN4)
	if (CPU_ISSUN4 && (ca->ca_bustype == BUS_OBIO)) {
		/*
		 * Check for a pfour framebuffer.
		 */
		switch (fb_pfour_id(ra->ra_vaddr)) {
		case PFOUR_ID_BW:
		case PFOUR_ID_COLOR8P1:		/* bwtwo in ... */
		case PFOUR_ID_COLOR24:		/* ...overlay plane */
			cf->cf_flags |= FB_PFOUR;
			/* FALLTHROUGH */

		case PFOUR_NOTPFOUR:
			return (1);

		default:
			return (0);
		}
	}
#endif

	return (0);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
bwtwoattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct bwtwo_softc *sc = (struct bwtwo_softc *)self;
	register struct confargs *ca = args;
	register int node = ca->ca_ra.ra_node, ramsize;
	struct fbdevice *fb = &sc->sc_fb;
	int isconsole = 0;
	int sbus = 1;
	char *nam = NULL;

	fb->fb_driver = &bwtwofbdriver;
	fb->fb_device = &sc->sc_dev;
	fb->fb_type.fb_type = FBTYPE_SUN2BW;
	fb->fb_flags = sc->sc_dev.dv_cfdata->cf_flags;

	/*
	 * Map the control register.
	 */
	if (fb->fb_flags & FB_PFOUR) {
		fb->fb_pfour = (volatile u_int32_t *)
		    mapiodev(ca->ca_ra.ra_reg, 0, sizeof(u_int32_t));
		sc->sc_reg = NULL;
	} else {
		sc->sc_reg = (volatile struct fbcontrol *)
		    mapiodev(ca->ca_ra.ra_reg, BWREG_REG,
			     sizeof(struct fbcontrol));
		fb->fb_pfour = NULL;
	}

	/* Set up default pixel offset.  May be changed below. */
	sc->sc_pixeloffset = BWREG_MEM;

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		if (CPU_ISSUN4M)	/* 4m has framebuffer on obio */
			goto obp_name;

		sbus = node = 0;
#if defined(SUN4)
		if (fb->fb_flags & FB_PFOUR) {
			nam = "bwtwo/p4";
			/*
			 * Notice if this is an overlay plane on a color
			 * framebuffer.  Note that PFOUR_COLOR_OFF_OVERLAY
			 * is the same as PFOUR_BW_OFF, but we use the
			 * different names anyway.
			 */
			switch (PFOUR_ID(*fb->fb_pfour)) {
			case PFOUR_ID_COLOR8P1:
				sc->sc_ovtype = BWO_CGFOUR;
				sc->sc_pixeloffset = PFOUR_COLOR_OFF_OVERLAY;
				break;

			case PFOUR_ID_COLOR24:
				sc->sc_ovtype = BWO_CGEIGHT;
				sc->sc_pixeloffset = PFOUR_COLOR_OFF_OVERLAY;
				break;

			default:
				sc->sc_ovtype = BWO_NONE;
				sc->sc_pixeloffset = PFOUR_BW_OFF;
				break;
			}
		} else
#endif
			nam = "bwtwo";
		break;

	case BUS_VME32:
	case BUS_VME16:
		sbus = node = 0;
		nam = "bwtwo";
		break;

	case BUS_SBUS:
	obp_name:
#if defined(SUN4C) || defined(SUN4M)
		nam = getpropstring(node, "model");
#endif
		break;
	}

	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	fb->fb_type.fb_depth = 1;
	fb_setsize(fb, fb->fb_type.fb_depth, 1152, 900, node, ca->ca_bustype);

	ramsize = fb->fb_type.fb_height * fb->fb_linebytes;
	fb->fb_type.fb_cmsize = 0;
	fb->fb_type.fb_size = ramsize;
	printf(": %s, %d x %d", nam,
	    fb->fb_type.fb_width, fb->fb_type.fb_height);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		int constype = (fb->fb_flags & FB_PFOUR) ? EE_CONS_P4OPT :
		    EE_CONS_BW;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == constype)
			isconsole = (fbconstty != NULL);
		else
			isconsole = 0;
	}
#endif

	if (CPU_ISSUN4COR4M)
		isconsole = node == fbnode && fbconstty != NULL;

	/*
	 * When the ROM has mapped in a bwtwo display, the address
	 * maps only the video RAM, hence we always map the control
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	if ((fb->fb_pixels = ca->ca_ra.ra_vaddr) == NULL && isconsole) {
		/* this probably cannot happen (on sun4c), but what the heck */
		fb->fb_pixels =
		    mapiodev(ca->ca_ra.ra_reg, sc->sc_pixeloffset, ramsize);
	}

	/* Insure video is enabled */
	bwtwo_set_video(sc, 1);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
#if defined(SUN4)
		/*
		 * XXX rcons doesn't seem to work properly on the overlay
		 * XXX plane.  This is a temporary kludge until someone
		 * XXX fixes it.
		 */
		if ((fb->fb_flags & FB_PFOUR) == 0 ||
		    (sc->sc_ovtype == BWO_NONE))
#endif
			fbrcons_init(fb);
#endif
	} else
		printf("\n");

#if defined(SUN4C) || defined(SUN4M)
	if (sbus)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);
#endif

#if defined(SUN4)
	if ((fb->fb_flags & FB_PFOUR) && (sc->sc_ovtype != BWO_NONE)) {
		char *ovnam;

		switch (sc->sc_ovtype) {
		case BWO_CGFOUR:
			ovnam = "cgfour";
			break;

		case BWO_CGEIGHT:
			ovnam = "cgeight";
			break;

		default:
			ovnam = "unknown";
			break;
		}
		printf("%s: %s overlay plane\n", sc->sc_dev.dv_xname, ovnam);
	}
#endif

	if (CPU_ISSUN4 || node == fbnode) {
#if defined(SUN4)
		/*
		 * If we're on an overlay plane of a color framebuffer,
		 * then we don't force the issue in fb_attach() because
		 * we'd like the color framebuffer to actually be the
		 * "console framebuffer".  We're only around to speed
		 * up rconsole.
		 */
		if ((fb->fb_flags & FB_PFOUR) && (sc->sc_ovtype != BWO_NONE ))
			fb_attach(fb, 0);
		else
#endif
			fb_attach(fb, isconsole);
	}
}

int
bwtwoopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= bwtwo_cd.cd_ndevs || bwtwo_cd.cd_devs[unit] == NULL)
		return (ENXIO);

	return (0);
}

int
bwtwoclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	return (0);
}

int
bwtwoioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct bwtwo_softc *sc = bwtwo_cd.cd_devs[minor(dev)];

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGVIDEO:
		*(int *)data = bwtwo_get_video(sc);
		break;

	case FBIOSVIDEO:
		bwtwo_set_video(sc, (*(int *)data));
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

static void
bwtwounblank(dev)
	struct device *dev;
{
	struct bwtwo_softc *sc = (struct bwtwo_softc *)dev;

	bwtwo_set_video(sc, 1);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
int
bwtwommap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	register struct bwtwo_softc *sc = bwtwo_cd.cd_devs[minor(dev)];

	if (off & PGOFSET)
		panic("bwtwommap");
	if (off < 0)
		return (-1);
	if ((unsigned)off >= sc->sc_fb.fb_type.fb_size)
		return (-1);
	/*
	 * I turned on PMAP_NC here to disable the cache as I was
	 * getting horribly broken behaviour with it on.
	 */
	return (REG2PHYS(&sc->sc_phys, sc->sc_pixeloffset + off) | PMAP_NC);
}

static int
bwtwo_get_video(sc)
	struct bwtwo_softc *sc;
{

#if defined(SUN4)
	if (CPU_ISSUN4 && (sc->sc_bustype == BUS_OBIO)) {
		if (sc->sc_fb.fb_flags & FB_PFOUR) {
			/*
			 * This handles the overlay plane case, too.
			 */
			return (fb_pfour_get_video(&sc->sc_fb));
		} else
			return ((lduba(AC_SYSENABLE,
			    ASI_CONTROL) & SYSEN_VIDEO) != 0);
	}
#endif

	return ((sc->sc_reg->fbc_ctrl & FBC_VENAB) != 0);
}

static void
bwtwo_set_video(sc, enable)
	struct bwtwo_softc *sc;
	int enable;
{

#if defined(SUN4)
	if (CPU_ISSUN4 && (sc->sc_bustype == BUS_OBIO)) {
		if (sc->sc_fb.fb_flags & FB_PFOUR) {
			/*
			 * This handles the overlay plane case, too.
			 */
			fb_pfour_set_video(&sc->sc_fb, enable);
			return;
		}
		if (enable)
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_VIDEO);
		else
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) & ~SYSEN_VIDEO);

		return;
	}
#endif

	if (enable)
		sc->sc_reg->fbc_ctrl |= FBC_VENAB;
	else
		sc->sc_reg->fbc_ctrl &= ~FBC_VENAB;
}

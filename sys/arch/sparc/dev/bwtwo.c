/*	$NetBSD: bwtwo.c,v 1.15 1995/10/09 15:39:34 pk Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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
 */

#include <sys/param.h>
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
#if defined(SUN4)
#include <machine/eeprom.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#endif

#include <sparc/dev/bwtworeg.h>
#include <sparc/dev/pfourreg.h>
#include <sparc/dev/sbusvar.h>
#include "pfour.h"

/* per-display variables */
struct bwtwo_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	volatile struct bwtworeg *sc_reg;/* control registers */
	struct rom_reg sc_phys;		/* display RAM (phys addr) */
	int	sc_bustype;
};

/* autoconfiguration driver */
static void	bwtwoattach __P((struct device *, struct device *, void *));
static int	bwtwomatch __P((struct device *, void *, void *));
int		bwtwoopen __P((dev_t, int, int, struct proc *));
int		bwtwoclose __P((dev_t, int, int, struct proc *));
int		bwtwoioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int		bwtwommap __P((dev_t, int, int));
static void	bwtwounblank __P((struct device *));

struct cfdriver bwtwocd = {
	NULL, "bwtwo", bwtwomatch, bwtwoattach,
	DV_DULL, sizeof(struct bwtwo_softc)
};

/* frame buffer generic driver */
static struct fbdriver bwtwofbdriver = {
	bwtwounblank, bwtwoopen, bwtwoclose, bwtwoioctl, bwtwommap
};

static void	bwtwoenable __P((struct bwtwo_softc *, int));
static int	bwtwostatus __P((struct bwtwo_softc *));

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

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return(1);
#if NPFOUR > 0
	if (ca->ca_bustype == BUS_PFOUR) {
		if (PFOUR_ID(ra->ra_pfour) == PFOUR_ID_BW)
			return (1);
		return (0);
	}
#endif
	return (probeget(ra->ra_vaddr, 4) != -1);
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
	int isconsole;
	char *nam;

	sc->sc_fb.fb_driver = &bwtwofbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN2BW;

	sc->sc_bustype = ca->ca_bustype;
	switch (ca->ca_bustype) {
#if defined(SUN4)
#if NPFOUR > 0
	case BUS_PFOUR:
		node = 0;
		pfour_reset();
		pfour_videosize(ca->ca_ra.ra_pfour,
		    &sc->sc_fb.fb_type.fb_width,
		    &sc->sc_fb.fb_type.fb_height);
		sc->sc_fb.fb_linebytes = sc->sc_fb.fb_type.fb_width / 8;
		nam = "bwtwo";
		break;  
#endif
#endif
	case BUS_OBIO:
#if defined(SUN4M)
		if (cputyp == CPU_SUN4M) {   /* 4m has framebuffer on obio */
			nam = getpropstring(node, "model");
			break;
		}
#endif
#if defined(SUN4)
		node = 0;
		nam = "bwtwo";
		break;
#endif
	case BUS_SBUS:
#if defined(SUN4C) || defined(SUN4M)
		nam = getpropstring(node, "model");
		break;
#endif
	}
	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	sc->sc_fb.fb_type.fb_depth = 1;
	fb_setsize(&sc->sc_fb, sc->sc_fb.fb_type.fb_depth,
	    1152, 900, node, ca->ca_bustype);

	ramsize = sc->sc_fb.fb_type.fb_height * sc->sc_fb.fb_linebytes;
	sc->sc_fb.fb_type.fb_cmsize = 0;
	sc->sc_fb.fb_type.fb_size = ramsize;
	printf(": %s, %d x %d", nam,
	    sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height);

#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->ee_diag.eed_console == EED_CONS_BW)
			isconsole = (fbconstty != NULL);
		else
			isconsole = 0;
	}
#endif
#if defined(SUN4C) || defined(SUN4M)
	if (cputyp == CPU_SUN4C || cputyp == CPU_SUN4M)
		isconsole = node == fbnode && fbconstty != NULL;
#endif
	/*
	 * When the ROM has mapped in a bwtwo display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	if ((sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddr) == NULL && isconsole) {
		/* this probably cannot happen (on sun4c), but what the heck */
		sc->sc_fb.fb_pixels = mapiodev(ca->ca_ra.ra_reg, BWREG_MEM,
		    ramsize, ca->ca_bustype);
	}
	sc->sc_reg = (volatile struct bwtworeg *)mapiodev(ca->ca_ra.ra_reg,
	    BWREG_REG, sizeof(struct bwtworeg), ca->ca_bustype);

	/* Insure video is enabled */
	bwtwoenable(sc, 1);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		fbrcons_init(&sc->sc_fb);
#endif
	} else
		printf("\n");
#if defined(SUN4C) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);
#endif
	if ((node == fbnode && cputyp != CPU_SUN4) ||
	    (isconsole && cputyp == CPU_SUN4))
		fb_attach(&sc->sc_fb);
}

int
bwtwoopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= bwtwocd.cd_ndevs || bwtwocd.cd_devs[unit] == NULL)
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
	struct bwtwo_softc *sc = bwtwocd.cd_devs[minor(dev)];

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGVIDEO:
		bwtwostatus(sc);
		break;

	case FBIOSVIDEO:
		bwtwoenable(sc, *(int *)data);
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

	bwtwoenable(sc, 1);
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
	register struct bwtwo_softc *sc = bwtwocd.cd_devs[minor(dev)];

	if (off & PGOFSET)
		panic("bwtwommap");
	if ((unsigned)off >= sc->sc_fb.fb_type.fb_size)
		return (-1);
	/*
	 * I turned on PMAP_NC here to disable the cache as I was
	 * getting horribly broken behaviour with it on.
	 */
	return (REG2PHYS(&sc->sc_phys, BWREG_MEM+off, sc->sc_bustype) | PMAP_NC);
}


int
bwtwostatus(sc)
	struct bwtwo_softc *sc;
{
#ifdef SUN4
#if NPFOUR > 0
	if (sc->sc_bustype == BUS_PFOUR)
		return pfourstatus();
#endif
	if (sc->sc_bustype == BUS_OBIO)
		return (lduba(AC_SYSENABLE, ASI_CONTROL) & SYSEN_VIDEO);
#endif
#if defined(SUN4C) || defined(SUN4M)
	return (sc->sc_reg->bw_ctl & CTL_VE);
#endif
}

void
bwtwoenable(sc, on)
	struct bwtwo_softc *sc;
	int on;
{
#if NPFOUR > 0
		if (sc->sc_bustype == BUS_PFOUR) {
			pfourenable(on);
			return;
		}
#endif
	if (on) {
#ifdef SUN4
		if (sc->sc_bustype == BUS_OBIO) {
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_VIDEO);
			return;
		}
#endif
		sc->sc_reg->bw_ctl |= CTL_VE;
	} else {
#ifdef SUN4
		if (sc->sc_bustype == BUS_OBIO) {
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) & ~SYSEN_VIDEO);
			return;
		}
#endif
		sc->sc_reg->bw_ctl &= ~CTL_VE;
	}
}

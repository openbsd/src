/*	$OpenBSD: bwtwo.c,v 1.36 2010/05/15 15:28:09 miod Exp $	*/
/*	$NetBSD: bwtwo.c,v 1.33 1997/05/24 20:16:02 pk Exp $ */

/*
 * Copyright (c) 2002 Miodrag Vallat.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <machine/pmap.h>

/* per-display variables */
struct bwtwo_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	volatile struct fbcontrol *sc_reg;/* control registers */
	struct rom_reg	sc_phys;	/* phys address description */
	int	sc_bustype;		/* type of bus we live on */
	int	sc_pixeloffset;		/* offset to framebuffer */
};

void	bwtwo_burner(void *, u_int, u_int);
int	bwtwo_intr(void *);
int	bwtwo_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	bwtwo_mmap(void *, off_t, int);

struct wsdisplay_accessops bwtwo_accessops = {
	bwtwo_ioctl,
	bwtwo_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	bwtwo_burner,
	NULL	/* pollc */
};


/* autoconfiguration driver */
void	bwtwoattach(struct device *, struct device *, void *);
int	bwtwomatch(struct device *, void *, void *);

struct cfattach bwtwo_ca = {
	sizeof(struct bwtwo_softc), bwtwomatch, bwtwoattach
};

struct cfdriver bwtwo_cd = {
	NULL, "bwtwo", DV_DULL
};

int
bwtwomatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

#if 0
	if (CPU_ISSUN4 && cf->cf_unit != 0)
		return (0);
#endif

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	/*
	 * Make sure there's hardware there.
	 */
	if (probeget(ra->ra_vaddr, 4) == -1)
		return (0);

	switch (ca->ca_bustype) {
	case BUS_VME16:
	case BUS_VME32:
		return (1);
	case BUS_OBIO:
#if defined(SUN4)
		if (CPU_ISSUN4) {
			/*
			 * Check for a pfour framebuffer, but do not match the
			 * overlay planes for color pfour framebuffers.
			 */
			switch (fb_pfour_id(ra->ra_vaddr)) {
			case PFOUR_ID_BW:
			case PFOUR_NOTPFOUR:
				return (1);
			case PFOUR_ID_COLOR8P1:		/* bwtwo in ... */
			case PFOUR_ID_COLOR24:		/* ...overlay plane */
			default:
				return (0);
			}
		}
#endif
		return (1);
	default:
		return (0);
	}
}

void
bwtwoattach(struct device *parent, struct device *self, void *args)
{
	struct bwtwo_softc *sc = (struct bwtwo_softc *)self;
	struct confargs *ca = args;
	int node = ca->ca_ra.ra_node;
	int isconsole = 0;
	int sbus = 1;
	char *nam;

	printf(": ");

	/*
	 * Check if this is a P4 attachment, and map the P4 control
	 * register if necessary.
	 */
#if defined(SUN4)
	if (CPU_ISSUN4 && ca->ca_bustype == BUS_OBIO) {
		sc->sc_sunfb.sf_pfour = (volatile u_int32_t *)
		    mapiodev(ca->ca_ra.ra_reg, 0, sizeof(u_int32_t));
		if (fb_pfour_id(sc->sc_sunfb.sf_pfour) != PFOUR_NOTPFOUR)
			SET(sc->sc_sunfb.sf_flags, FB_PFOUR);
		else {
			/* XXX unmapiodev */
			sc->sc_sunfb.sf_pfour = NULL;
		}
	}
#endif

	/*
	 * Map the control register (unless done above for a P4 device).
	 */
#if defined(SUN4)
	if (!CPU_ISSUN4 || !ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR))
#endif
	{
		sc->sc_reg = (volatile struct fbcontrol *)
		    mapiodev(ca->ca_ra.ra_reg, BWREG_REG,
			     sizeof(struct fbcontrol));
	}

	/* Set up default pixel offset.  May be changed below. */
	sc->sc_pixeloffset = BWREG_MEM;

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		if (CPU_ISSUN4M)	/* 4m has framebuffer on obio */
			goto obp_name;

		sbus = node = 0;
#if defined(SUN4)
		if (ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR)) {
			nam = "p4";
			sc->sc_pixeloffset = PFOUR_BW_OFF;
		} else
#endif
			nam = NULL;
		break;

	case BUS_VME32:
	case BUS_VME16:
		sbus = node = 0;
		nam = NULL;
		break;

	case BUS_SBUS:
obp_name:
#if defined(SUN4C) || defined(SUN4M)
		nam = getpropstring(node, "model");
#endif
		break;
	}

	if (nam != NULL && *nam != '\0')
		printf("%s, ", nam);

#if defined(SUN4)
	if (CPU_ISSUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;
		int constype = ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR) ?
		    EE_CONS_P4OPT : EE_CONS_BW;
		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->eeConsole == constype)
			isconsole = 1;
		else
		/*
		 * On sun4 systems without on-board framebuffers (such as
		 * the 4/3xx models), the PROM will accept the EE_CONS_BW
		 * setting although the framebuffer is a P4.
		 * Accept this setting as well.
		 */
		if (eep->eeConsole == EE_CONS_BW)
			isconsole = 1;
	}
#endif

	if (CPU_ISSUN4COR4M)
		isconsole = node == fbnode;

	sc->sc_phys = ca->ca_ra.ra_reg[0];
	sc->sc_bustype = ca->ca_bustype;

	/* enable video */
	bwtwo_burner(sc, 1, 0);

	fb_setsize(&sc->sc_sunfb, 1, 1152, 900, node, ca->ca_bustype);
	printf("%dx%d\n", sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(ca->ca_ra.ra_reg,
	    sc->sc_pixeloffset, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, isconsole);

	if (isconsole)
		fbwscons_console_init(&sc->sc_sunfb, -1);

	fbwscons_attach(&sc->sc_sunfb, &bwtwo_accessops, isconsole);
}

int
bwtwo_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct bwtwo_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNBW;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 0;	/* no colormap */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		break;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return (-1);	/* not supported yet */
	}

	return (0);
}

paddr_t
bwtwo_mmap(void *v, off_t offset, int prot)
{
	struct bwtwo_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, sc->sc_pixeloffset + offset) |
		    PMAP_NC);
	}

	return (-1);
}

void
bwtwo_burner(void *v, u_int on, u_int flags)
{
	struct bwtwo_softc *sc = v;
	int s;

#if defined(SUN4)
	if (CPU_ISSUN4 && (sc->sc_bustype == BUS_OBIO)) {
		if (ISSET(sc->sc_sunfb.sf_flags, FB_PFOUR)) {
			fb_pfour_burner(v, on, flags);
			return;
		}
		if (on)
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) | SYSEN_VIDEO);
		else
			stba(AC_SYSENABLE, ASI_CONTROL,
			    lduba(AC_SYSENABLE, ASI_CONTROL) & ~SYSEN_VIDEO);

		return;
	}
#endif

	s = splhigh();
	if (on)
		sc->sc_reg->fbc_ctrl |= FBC_VENAB | FBC_TIMING;
	else {
		sc->sc_reg->fbc_ctrl &= ~FBC_VENAB;
		if (flags & WSDISPLAY_BURN_VBLANK)
			sc->sc_reg->fbc_ctrl &= ~FBC_TIMING;
	}
	splx(s);
}

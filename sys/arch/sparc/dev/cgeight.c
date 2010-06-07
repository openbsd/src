/*	$OpenBSD: cgeight.c,v 1.30 2010/06/07 19:43:45 miod Exp $	*/
/*	$NetBSD: cgeight.c,v 1.13 1997/05/24 20:16:04 pk Exp $	*/

/*
 * Copyright (c) 2002 Miodrag Vallat.  All rights reserved.
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
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
 *	from @(#)cgthree.c	8.2 (Berkeley) 10/30/93
 */

/*
 * color display (cgeight) driver.
 *
 * For performance reasons, we run the emulation mode in the (monochrome)
 * overlay plane, but X11 in the 24-bit color plane.
 *
 * Does not handle interrupts, even though they can occur.
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

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/eeprom.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/pfourreg.h>

/* per-display variables */
struct cgeight_softc {
	struct	sunfb sc_sunfb;			/* common base device */
	struct	rom_reg	sc_phys;
	volatile struct fbcontrol *sc_fbc;	/* Brooktree registers */
	volatile u_char *sc_enable;		/* enable plane */
};

int	cgeight_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	cgeight_mmap(void *, off_t, int);
void	cgeight_reset(struct cgeight_softc *, int);

struct wsdisplay_accessops cgeight_accessops = {
	cgeight_ioctl,
	cgeight_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	fb_pfour_burner,
	NULL	/* pollc */
};

void	cgeightattach(struct device *, struct device *, void *);
int	cgeightmatch(struct device *, void *, void *);

struct cfattach cgeight_ca = {
	sizeof(struct cgeight_softc), cgeightmatch, cgeightattach
};

struct cfdriver cgeight_cd = {
	NULL, "cgeight", DV_DULL
};

int
cgeightmatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	if (!CPU_ISSUN4 || ca->ca_bustype != BUS_OBIO)
		return (0);

	/*
	 * Make sure there's hardware there.
	 */
	if (probeget(ra->ra_vaddr, 4) == -1)
		return (0);

	/*
	 * Check the pfour register.
	 */
	if (fb_pfour_id(ra->ra_vaddr) == PFOUR_ID_COLOR24)
		return (1);

	return (0);
}

void
cgeightattach(struct device *parent, struct device *self, void *args)
{
	struct cgeight_softc *sc = (struct cgeight_softc *)self;
	struct confargs *ca = args;
	int node = 0;
	volatile struct bt_regs *bt;
	int isconsole = 0;

	/* Map the pfour register. */
	SET(sc->sc_sunfb.sf_flags, FB_PFOUR);
	sc->sc_sunfb.sf_pfour = (volatile u_int32_t *)
	    mapiodev(ca->ca_ra.ra_reg, 0, sizeof(u_int32_t));

	if (cputyp == CPU_SUN4) {
		struct eeprom *eep = (struct eeprom *)eeprom_va;

		/*
		 * Assume this is the console if there's no eeprom info
		 * to be found.
		 */
		if (eep == NULL || eep->ee_diag.eed_console == EED_CONS_P4)
			isconsole = 1;
	}

	/* Map the Brooktree. */
	sc->sc_fbc = (volatile struct fbcontrol *)
	    mapiodev(ca->ca_ra.ra_reg,
		     PFOUR_COLOR_OFF_CMAP, sizeof(struct fbcontrol));

	sc->sc_phys = ca->ca_ra.ra_reg[0];

	/* enable video */
	fb_pfour_burner(sc, 1, 0);
	bt = &sc->sc_fbc->fbc_dac;
	BT_INIT(bt, 0);

	fb_setsize(&sc->sc_sunfb, 1, 1152, 900, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	/*
	 * Map the overlay and overlay enable planes.
	 */
	sc->sc_enable = (volatile u_char *)mapiodev(ca->ca_ra.ra_reg,
	    PFOUR_COLOR_OFF_ENABLE, round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_bits =  mapiodev(ca->ca_ra.ra_reg,
	    PFOUR_COLOR_OFF_OVERLAY, round_page(sc->sc_sunfb.sf_fbsize));

	cgeight_reset(sc, WSDISPLAYIO_MODE_EMUL);
	printf(": p4, %dx%d", sc->sc_sunfb.sf_width,
	    sc->sc_sunfb.sf_height);

	fbwscons_init(&sc->sc_sunfb, isconsole);
	if (isconsole)
		fbwscons_console_init(&sc->sc_sunfb, -1);

	fbwscons_attach(&sc->sc_sunfb, &cgeight_accessops, isconsole);
}

void
cgeight_reset(struct cgeight_softc *sc, int mode)
{
	volatile struct bt_regs *bt;
	union bt_cmap cm;
	u_int c;

	bt = &sc->sc_fbc->fbc_dac;

	/*
	 * Depending on the mode requested, disable or enable
	 * the overlay plane.
	 */
	if (mode == WSDISPLAYIO_MODE_EMUL) {
		memset((void *)sc->sc_enable, 0xff,
		    round_page(sc->sc_sunfb.sf_fbsize));

		/* Setup a strict mono colormap */
		cm.cm_map[0][0] = cm.cm_map[0][1] = cm.cm_map[0][2] = 0x00;
		for (c = 1; c < 256; c++) {
			cm.cm_map[c][0] = cm.cm_map[c][1] = cm.cm_map[c][2] =
			    0xff;
		}
	} else {
		memset((void *)sc->sc_enable, 0x00,
		    round_page(sc->sc_sunfb.sf_fbsize));

		/* Setup a ramp colormap (direct color) */
		for (c = 0; c < 256; c++) {
			cm.cm_map[c][0] = cm.cm_map[c][1] = cm.cm_map[c][2] = c;
		}
	}

	/* Upload the colormap into the DAC */
	bt_loadcmap(&cm, bt, 0, 256, 0);
}

int
cgeight_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct cgeight_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNCG8;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width = sc->sc_sunfb.sf_width;
		wdf->depth = 24;
		wdf->cmsize = 0;
		break;
	case WSDISPLAYIO_SMODE:
		cgeight_reset(sc, *(int *)data);
		break;
	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
		break;

	case WSDISPLAYIO_GETCMAP:	/* nothing to do */
	case WSDISPLAYIO_PUTCMAP:	/* nothing to do */
	case WSDISPLAYIO_GCURPOS:	/* not supported */
	case WSDISPLAYIO_SCURPOS:	/* not supported */
	case WSDISPLAYIO_GCURMAX:	/* not supported */
	case WSDISPLAYIO_GCURSOR:	/* not supported */
	case WSDISPLAYIO_SCURSOR:	/* not supported */
	default:
		return (-1);
	}

	return (0);
}

paddr_t
cgeight_mmap(void *v, off_t offset, int prot)
{
	struct cgeight_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	/* Allow mapping of the 24-bit color planes */
	if (offset >= 0 && offset < round_page(24 * sc->sc_sunfb.sf_fbsize)) {
		return (REG2PHYS(&sc->sc_phys,
		    offset + PFOUR_COLOR_OFF_COLOR) | PMAP_NC);
	}

	return (-1);
}

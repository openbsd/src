/*	$OpenBSD: dvbox.c,v 1.4 2005/01/19 10:51:23 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: grf_dv.c 1.12 93/08/13$
 *
 *	@(#)grf_dv.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the DaVinci, HP98730/98731 Graphics system.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/ioctl.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>
#include <hp300/dev/dvboxreg.h>

struct	dvbox_softc {
	struct device	sc_dev;
	struct diofb	*sc_fb;
	struct diofb	sc_fb_store;
	int		sc_scode;
};

int	dvbox_dio_match(struct device *, void *, void *);
void	dvbox_dio_attach(struct device *, struct device *, void *);
int	dvbox_intio_match(struct device *, void *, void *);
void	dvbox_intio_attach(struct device *, struct device *, void *);

struct cfattach dvbox_dio_ca = {
	sizeof(struct dvbox_softc), dvbox_dio_match, dvbox_dio_attach
};

struct cfattach dvbox_intio_ca = {
	sizeof(struct dvbox_softc), dvbox_intio_match, dvbox_intio_attach
};

struct cfdriver dvbox_cd = {
	NULL, "dvbox", DV_DULL
};

int	dvbox_reset(struct diofb *, int, struct diofbreg *);
void	dvbox_setcolor(struct diofb *, u_int,
	    u_int8_t, u_int8_t, u_int8_t);
void	dvbox_windowmove(struct diofb *, u_int16_t, u_int16_t,
	    u_int16_t, u_int16_t, u_int16_t, u_int16_t, int);

int	dvbox_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	dvbox_burner(void *, u_int, u_int);

struct	wsdisplay_accessops	dvbox_accessops = {
	dvbox_ioctl,
	diofb_mmap,
	diofb_alloc_screen,
	diofb_free_screen,
	diofb_show_screen,
	NULL,   /* load_font */
	NULL,   /* scrollback */
	NULL,   /* getchar */
	dvbox_burner
};

/*
 * Attachment glue
 */

int
dvbox_intio_match(struct device *parent, void *match, void *aux)
{
	struct intio_attach_args *ia = aux;
	struct diofbreg *fbr;

	fbr = (struct diofbreg *)IIOV(GRFIADDR);

	if (badaddr((caddr_t)fbr))
		return (0);

	if (fbr->id == GRFHWID && fbr->id2 == GID_DAVINCI) {
		ia->ia_addr = (caddr_t)GRFIADDR;
		return (1);
	}

	return (0);
}

void
dvbox_intio_attach(struct device *parent, struct device *self, void *aux)
{
	struct dvbox_softc *sc = (struct dvbox_softc *)self;
	struct diofbreg *fbr;

	fbr = (struct diofbreg *)IIOV(GRFIADDR);
	sc->sc_scode = -1;	/* XXX internal i/o */

        if (sc->sc_scode == conscode) {
                sc->sc_fb = &diofb_cn;
        } else {
                sc->sc_fb = &sc->sc_fb_store;
                dvbox_reset(sc->sc_fb, sc->sc_scode, fbr);
        }

	diofb_end_attach(sc, &dvbox_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, 4 /* XXX */, NULL);
}

int
dvbox_dio_match(struct device *parent, void *match, void *aux)
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_DAVINCI)
		return (1);

	return (0);
}

void
dvbox_dio_attach(struct device *parent, struct device *self, void *aux)
{
	struct dvbox_softc *sc = (struct dvbox_softc *)self;
	struct dio_attach_args *da = aux;
	struct diofbreg *fbr;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == conscode) {
		fbr = (struct diofbreg *)conaddr;	/* already mapped */
		sc->sc_fb = &diofb_cn;
	} else {
		sc->sc_fb = &sc->sc_fb_store;
		fbr = (struct diofbreg *)
		    iomap(dio_scodetopa(sc->sc_scode), da->da_size);
		if (fbr == NULL ||
		    dvbox_reset(sc->sc_fb, sc->sc_scode, fbr) != 0) {
			printf(": can't map framebuffer\n");
			return;
		}
	}

	diofb_end_attach(sc, &dvbox_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, 4 /* XXX */, NULL);
}

/*
 * Initialize hardware and display routines.
 */
int
dvbox_reset(struct diofb *fb, int scode, struct diofbreg *fbr)
{
	volatile struct dvboxfb *db = (struct dvboxfb *)fbr;
	int rc;
	int i;

	if ((rc = diofb_fbinquire(fb, scode, fbr)) != 0)
		return (rc);

	fb->planes = 8;
	fb->planemask = (1 << fb->planes) - 1;

	/*
	 * Magic initialization code.
	 */

  	db->reset = 0x80;
	DELAY(100);

	db->interrupt = 0x04;
	db->en_scan   = 0x01;
	db->fbwen     = ~0;
	db->opwen     = ~0;
	db->fold      = 0x01;	/* 8bpp */
	db->drive     = 0x01;	/* use FB plane */
	db->rep_rule  = DVBOX_DUALROP(RR_COPY);
	db->alt_rr    = DVBOX_DUALROP(RR_COPY);
	db->zrr       = DVBOX_DUALROP(RR_COPY);

	db->fbvenp    = 0xFF;	/* enable video */
	db->dispen    = 0x01;	/* enable display */
	db->fbvens    = 0x0;
	db->fv_trig   = 0x01;
	DELAY(100);
	db->vdrive    = 0x0;
	db->zconfig   = 0x0;

	while (db->wbusy & 0x01)
		DELAY(10);

	db->cmapbank = 0;

	db->red0   = 0;
	db->red1   = 0;
	db->green0 = 0;
	db->green1 = 0;
	db->blue0  = 0;
	db->blue1  = 0;

	db->panxh   = 0;
	db->panxl   = 0;
	db->panyh   = 0;
	db->panyl   = 0;
	db->zoom    = 0;
	db->cdwidth = 0x50;
	db->chstart = 0x52;
	db->cvwidth = 0x22;
	db->pz_trig = 1;

	/*
	 * Turn on frame buffer, turn on overlay planes, set replacement
	 * rule, enable top overlay plane writes for ite, disable all frame
	 * buffer planes, set byte per pixel, and display frame buffer 0.
	 * Lastly, turn on the box.
	 */
	db->interrupt = 0x04;
	db->drive     = 0x10;
 	db->rep_rule  = DVBOX_DUALROP(RR_COPY);
	db->opwen     = 0x01;
	db->fbwen     = 0x0;
	db->fold      = 0x01;
	db->vdrive    = 0x0;
	db->dispen    = 0x01;

	/*
	 * Video enable top overlay plane.
	 */
	db->opvenp = 0x01;
	db->opvens = 0x01;

	/*
	 * Make sure that overlay planes override frame buffer planes.
	 */
	db->ovly0p  = 0x0;
	db->ovly0s  = 0x0;
	db->ovly1p  = 0x0;
	db->ovly1s  = 0x0;
	db->fv_trig = 0x1;
	DELAY(100);

	fb->bmv = dvbox_windowmove;

	diofb_fbsetup(fb);
	diofb_fontunpack(fb);

	/*
	 * Setup the overlay colormaps. Need to set the 0,1 (black/white)
	 * color for both banks.
	 */

	db_waitbusy(db);
	for (i = 0; i <= 1; i++) {
		db->cmapbank = i;
		db->rgb[0].red   = 0x00;
		db->rgb[0].green = 0x00;
		db->rgb[0].blue  = 0x00;
		db->rgb[1].red   = 0xFF;
		db->rgb[1].green = 0xFF;
		db->rgb[1].blue  = 0xFF;
	}
	db->cmapbank = 0;
	db_waitbusy(db);

	return (0);
}

int
dvbox_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct diofb *fb = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_DVBOX;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = fb->dheight;
		wdf->width = fb->dwidth;
		wdf->depth = fb->planes;
		wdf->cmsize = 8;	/* XXX 16 because of overlay? */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = (fb->fbwidth * fb->planes) >> 3;
		break;
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		/* XXX TBD */
		break;
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;
	default:
		return (-1);
	}

	return (0);
}

void
dvbox_burner(void *v, u_int on, u_int flags)
{
	struct diofb *fb = v;
	volatile struct dvboxfb *db = (struct dvboxfb *)fb->regkva;

	if (on)
		db->dispen = 0x01;
	else
		db->dispen = 0x00;
}

void
dvbox_windowmove(struct diofb *fb, u_int16_t sx, u_int16_t sy,
    u_int16_t dx, u_int16_t dy, u_int16_t cx, u_int16_t cy, int rop)
{
	volatile struct dvboxfb *db = (struct dvboxfb *)fb->regkva;

	db_waitbusy(db);
	db->rep_rule = DVBOX_DUALROP(rop);
	db->source_y = sy;
	db->source_x = sx;
	db->dest_y   = dy;
	db->dest_x   = dx;
	db->wheight  = cy;
	db->wwidth   = cx;
	db->wmove    = 1;
}

/*
 * DaVinci console support
 */

int dvbox_console_scan(int, caddr_t, void *);
cons_decl(dvbox);

int
dvbox_console_scan(int scode, caddr_t va, void *arg)
{
	struct diofbreg *fbr = (struct diofbreg *)va;
	struct consdev *cp = arg;
	int force = 0, pri;

	if (fbr->id != GRFHWID || fbr->id2 != GID_DAVINCI)
		return (0);

	pri = CN_NORMAL;

#ifdef CONSCODE
	/*
	 * Raise our priority, if appropriate.
	 */
	if (scode == CONSCODE) {
		pri = CN_REMOTE;
		force = conforced = 1;
	}
#endif

	/* Only raise priority. */
	if (pri > cp->cn_pri)
		cp->cn_pri = pri;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, stash our priority.
	 */
	if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) || force) {
		cn_tab = cp;
		return (DIO_SIZE(scode, va));
	}
	return (0);
}

void
dvboxcnprobe(struct consdev *cp)
{
	int maj;
	caddr_t va;
	struct diofbreg *fbr;
	int force = 0;

	/* Abort early if console is already forced. */
	if (conforced)
		return;

	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == wsdisplayopen)
			break;
	}

	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_DEAD;

	/* Look for "internal" framebuffer. */
	va = (caddr_t)IIOV(GRFIADDR);
	fbr = (struct diofbreg *)va;
	if (!badaddr(va) &&
	    fbr->id == GRFHWID && fbr->id2 == GID_DAVINCI) {
		cp->cn_pri = CN_INTERNAL;

#ifdef CONSCODE
		if (CONSCODE == -1) {
			force = conforced = 1;
		}
#endif

		/*
		 * If our priority is higher than the currently
		 * remembered console, stash our priority, and
		 * unmap whichever device might be currently mapped.
		 * Since we're internal, we set the saved size to 0
		 * so they don't attempt to unmap our fixed VA later.
		 */
		if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) ||
		    force) {
			cn_tab = cp;
			if (convasize)
				iounmap(conaddr, convasize);
			conscode = -1;
			conaddr = va;
			convasize = 0;
		}
	}

	console_scan(dvbox_console_scan, cp, HP300_BUS_DIO);
}

void
dvboxcninit(struct consdev *cp)
{
	long defattr;

	dvbox_reset(&diofb_cn, conscode, (struct diofbreg *)conaddr);
	diofb_alloc_attr(NULL, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&diofb_cn.wsd, &diofb_cn, 0, 0, defattr);
}

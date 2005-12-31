/*	$OpenBSD: dvbox.c,v 1.12 2005/12/31 18:13:41 miod Exp $	*/

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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

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
void	dvbox_restore(struct diofb *);
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

	if (fbr->id == GRFHWID && fbr->fbid == GID_DAVINCI) {
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
	sc->sc_scode = CONSCODE_INTERNAL;

        if (sc->sc_scode == conscode) {
                sc->sc_fb = &diofb_cn;
        } else {
                sc->sc_fb = &sc->sc_fb_store;
                dvbox_reset(sc->sc_fb, sc->sc_scode, fbr);
        }

	diofb_end_attach(sc, &dvbox_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, NULL);
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
	    sc->sc_scode == conscode, NULL);
}

/*
 * Initialize hardware and display routines.
 */
int
dvbox_reset(struct diofb *fb, int scode, struct diofbreg *fbr)
{
	int rc;

	if ((rc = diofb_fbinquire(fb, scode, fbr)) != 0)
		return (rc);

	/*
	 * Restrict the framebuffer to a monochrome view for now, until
	 * I know better how to detect and frob overlay planes, and
	 * setup a proper colormap. -- miod
	 */
	fb->planes = fb->planemask = 1;

	fb->bmv = dvbox_windowmove;
	dvbox_restore(fb);
	diofb_fbsetup(fb);

	return (0);
}

/*
 * Magic initialization code.
 */
void
dvbox_restore(struct diofb *fb)
{
	volatile struct dvboxfb *db = (struct dvboxfb *)fb->regkva;
	u_int i;

  	db->regs.id = 0x80;
	DELAY(100);

	db->regs.interrupt = 0x04;
	db->en_scan = 0x01;
	db->fbwen = ~0;
	db->opwen = ~0;
	db->fold = 0x01;	/* 8bpp */
	db->drive = 0x01;	/* use FB plane */
	db->rep_rule = DVBOX_DUALROP(RR_COPY);
	db->alt_rr = DVBOX_DUALROP(RR_COPY);
	db->zrr = DVBOX_DUALROP(RR_COPY);

	db->fbvenp = 0xFF;	/* enable video */
	db->dispen = 0x01;	/* enable display */
	db->fbvens = 0x0;
	db->fv_trig = 0x01;
	DELAY(100);
	db->vdrive = 0x0;
	db->zconfig = 0x0;

	while (db->wbusy & 0x01)
		DELAY(10);

	db->cmapbank = 0;

	db->red0 = 0;
	db->red1 = 0;
	db->green0 = 0;
	db->green1 = 0;
	db->blue0 = 0;
	db->blue1 = 0;

	db->panxh = 0;
	db->panxl = 0;
	db->panyh = 0;
	db->panyl = 0;
	db->zoom = 0;
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
	db->regs.interrupt = 0x04;
	db->drive = 0x10;
 	db->rep_rule = DVBOX_DUALROP(RR_COPY);
	db->opwen = 0x01;
	db->fbwen = 0x0;
	db->fold = 0x01;
	db->vdrive = 0x0;
	db->dispen = 0x01;

	/*
	 * Video enable top overlay plane.
	 */
	db->opvenp = 0x01;
	db->opvens = 0x01;

	/*
	 * Make sure that overlay planes override frame buffer planes.
	 */
	db->ovly0p = 0x0;
	db->ovly0s = 0x0;
	db->ovly1p = 0x0;
	db->ovly1s = 0x0;
	db->fv_trig = 0x1;
	DELAY(100);

	/*
	 * Setup the overlay colormaps. Need to set the 0,1 (black/white)
	 * color for both banks.
	 */
	db_waitbusy(db);
	for (i = 0; i <= 1; i++) {
		db->cmapbank = i;
		db->rgb[0].red = 0x00;
		db->rgb[0].green = 0x00;
		db->rgb[0].blue = 0x00;
		db->rgb[1].red = 0xff;
		db->rgb[1].green = 0xff;
		db->rgb[1].blue = 0xff;
	}
	db->cmapbank = 0;
	db_waitbusy(db);
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
	case WSDISPLAYIO_SMODE:
		fb->mapmode = *(u_int *)data;
		if (fb->mapmode == WSDISPLAYIO_MODE_EMUL)
			dvbox_restore(fb);
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->width = fb->ri.ri_width;
		wdf->height = fb->ri.ri_height;
		wdf->depth = fb->ri.ri_depth;
		wdf->cmsize = 0;	/* XXX */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = fb->ri.ri_stride;
		break;
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		break;		/* XXX until color support is implemented */
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
	db->dest_y = dy;
	db->dest_x = dx;
	db->wheight = cy;
	db->wwidth = cx;
	db->wmove = 1;

	db_waitbusy(db);
}

/*
 * DaVinci console support
 */

void
dvboxcninit()
{
	dvbox_reset(&diofb_cn, conscode, (struct diofbreg *)conaddr);
	diofb_cnattach(&diofb_cn);
}

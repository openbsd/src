/*	$OpenBSD: tvrx.c,v 1.4 2013/10/21 10:36:13 miod Exp $	*/

/*
 * Copyright (c) 2006, 2011, Miodrag Vallat.
 * All rights reserved.
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
 *
 */

/*
 * Graphics routines for the ``TigerShark'' PersonalVRX frame buffer,
 * in its non-STI flavour (DIO-II 98702-66501 interface board; the SGC
 * 98705-66582 board is expected to be supported by the sti(4) driver).
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

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <hp300/dev/diofbreg.h>
#include <hp300/dev/diofbvar.h>

/*
 * Hardware registers
 */

#define	TVRX_FV_TRIG		0x5003	/* commit mode settings */
#define	TVRX_DISPLAY_ENABLE	0x500f	/* enable display */
#define	TVRX_FB_P_ENABLE	0x5017	/* enable primary fb planes */
#define	TVRX_FB_S_ENABLE	0x501b	/* enable secondary fb planes */
#define	TVRX_O_P_ENABLE		0x5023	/* enable primary overlay planes */
#define	TVRX_O_S_ENABLE		0x5027	/* enable secondary overlay planes */
#define	TVRX_WBUSY		0x7047	/* window mover busy */
#define	TVRX_ZHERE		0x7053	/* Z buffer available */
#define	TVRX_FB_WEN		0x7093	/* fb planes write enable */
#define	TVRX_WMOVE		0x709f	/* trigger window mover */
#define	TVRX_O_WEN		0x70b7	/* overlay planes write enable */
#define	TVRX_DRIVE		0x70bf	/* vram access mode */
#define		DRIVE_OVERLAY_ENABLE	0x10	/* drive vram to overlays */
#define		DRIVE_1BPP		0x80	/* force 1bpp packed memory */
#define		DRIVE_PLANE_MASK	0x0f	/* overlay planes read mask */
#define	TVRX_REP_RULE		0x70ef	/* window mover replacement rule */
#define		TVRX_ROP(rop)		((rop) << 4 | (rop))
#define	TVRX_SRC_X		0x70f2	/* window mover source position */
#define	TVRX_SRC_Y		0x70f6
#define	TVRX_DST_X		0x70fa	/* window mover destination position */
#define	TVRX_DST_Y		0x70fe
#define	TVRX_CNT_X		0x7102	/* window mover span */
#define	TVRX_CNT_Y		0x7106

#define	TVRX_CMAP_O_P		0x5203	/* primary overlay colormap (16xRGB) */
#define	TVRX_CMAP_O_S		0x5303	/* secondary overlay colormap */
#define	TVRX_CMAP_FB_P_R	0x5403	/* primary fb colormap, 256xR */
#define	TVRX_CMAP_FB_P_G	0x5803	/* primary fb colormap, 256xG */
#define	TVRX_CMAP_FB_P_B	0x5c03	/* primary fb colormap, 256xB */
#define	TVRX_CMAP_FB_S_R	0x6403	/* secondary fb colormap, 256xR */
#define	TVRX_CMAP_FB_S_G	0x6803	/* secondary fb colormap, 256xG */
#define	TVRX_CMAP_FB_S_B	0x6c03	/* secondary fb colormap, 256xB */

#define	tvrx_reg(kva,type,offset) \
	(*(volatile type *)((kva) + (offset)))

/* wait for window mover to become idle */
#define	tvrx_waitbusy(fb) \
do { \
	while (tvrx_reg((fb)->regkva, uint8_t, TVRX_WBUSY) & 0x01) \
		; \
} while (0)


struct	tvrx_softc {
	struct device		 sc_dev;
	struct diofb		*sc_fb;
	struct diofb		 sc_fb_store;

	int			 sc_scode;
};

int	tvrx_match(struct device *, void *, void *);
void	tvrx_attach(struct device *, struct device *, void *);

struct cfattach tvrx_ca = {
	sizeof(struct tvrx_softc), tvrx_match, tvrx_attach
};

struct cfdriver tvrx_cd = {
	NULL, "tvrx", DV_DULL
};

int	tvrx_reset(struct diofb *, int, struct diofbreg *);
void	tvrx_restore(struct diofb *);
int	tvrx_setcmap(struct diofb *, struct wsdisplay_cmap *);
void	tvrx_setcolor(struct diofb *, u_int);
int	tvrx_windowmove(struct diofb *, u_int16_t, u_int16_t, u_int16_t,
	    u_int16_t, u_int16_t, u_int16_t, int16_t, int16_t);

int	tvrx_ioctl(void *, u_long, caddr_t, int, struct proc *);
void	tvrx_burner(void *, u_int, u_int);

struct	wsdisplay_accessops	tvrx_accessops = {
	.ioctl = tvrx_ioctl,
	.mmap = diofb_mmap,
	.alloc_screen = diofb_alloc_screen,
	.free_screen = diofb_free_screen,
	.show_screen = diofb_show_screen,
	.load_font = diofb_load_font,
	.list_font = diofb_list_font,
	.burn_screen = tvrx_burner
};

/*
 * Attachment glue
 */

int
tvrx_match(struct device *parent, void *match, void *aux)
{
	struct dio_attach_args *da = aux;

	if (da->da_id != DIO_DEVICE_ID_FRAMEBUFFER ||
	    da->da_secid != DIO_DEVICE_SECID_TIGERSHARK)
		return (0);

	return (1);
}

void
tvrx_attach(struct device *parent, struct device *self, void *aux)
{
	struct tvrx_softc *sc = (struct tvrx_softc *)self;
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
		    tvrx_reset(sc->sc_fb, sc->sc_scode, fbr) != 0) {
			printf(": can't map framebuffer\n");
			return;
		}
	}

	diofb_end_attach(sc, &tvrx_accessops, sc->sc_fb,
	    sc->sc_scode == conscode, NULL);
}

/*
 * Initialize hardware and display routines.
 */
int
tvrx_reset(struct diofb *fb, int scode, struct diofbreg *fbr)
{
	int rc;
	u_int i;

	if ((rc = diofb_fbinquire(fb, scode, fbr)) != 0)
		return (rc);

	/* diofb_fbinquire will return 8 (or maybe 16) planes, but we
	   only use the 4 overlay planes */
	fb->planes = 4;
	fb->planemask = (1 << 4) - 1;

	fb->bmv = tvrx_windowmove;
	tvrx_restore(fb);
	diofb_fbsetup(fb);
	for (i = 0; i <= fb->planemask; i++)
		tvrx_setcolor(fb, i);

	return (0);
}

void
tvrx_restore(struct diofb *fb)
{
	volatile struct diofbreg *fbr = (volatile struct diofbreg *)fb->regkva;

	/*
	 * Resetting the hardware is slow, disables display output, and
	 * does not clear video memory. Give it some time before we setup
	 * ourselves.
	 */
	fbr->id = GRFHWID;
	DELAY(100000);

	/* run the overlay planes unpacked... */
	tvrx_reg(fb->regkva, uint8_t, TVRX_DRIVE) =
	    DRIVE_OVERLAY_ENABLE | fb->planemask;
	/* ...and enable the four of them */
	tvrx_reg(fb->regkva, uint8_t, TVRX_O_P_ENABLE) = fb->planemask;
	tvrx_reg(fb->regkva, uint8_t, TVRX_O_S_ENABLE) = fb->planemask;
	tvrx_reg(fb->regkva, uint8_t, TVRX_O_WEN) = fb->planemask;
	/* disable fb planes for safety */
	tvrx_reg(fb->regkva, uint8_t, TVRX_FB_P_ENABLE) = 0;
	tvrx_reg(fb->regkva, uint8_t, TVRX_FB_S_ENABLE) = 0;

	tvrx_reg(fb->regkva, uint8_t, TVRX_REP_RULE) = TVRX_ROP(RR_COPY);
	tvrx_reg(fb->regkva, uint8_t, TVRX_DISPLAY_ENABLE) = 0x01;
	tvrx_reg(fb->regkva, uint8_t, TVRX_FV_TRIG) = 0x01;
}

int
tvrx_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct diofb *fb = v;
	struct wsdisplay_fbinfo *wdf;
	u_int i;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_TVRX;
		break;
	case WSDISPLAYIO_SMODE:
		fb->mapmode = *(u_int *)data;
		if (fb->mapmode == WSDISPLAYIO_MODE_EMUL) {
			tvrx_restore(fb);
			/* clear display */
			(*fb->bmv)(fb, 0, 0, 0, 0, fb->fbwidth, fb->fbheight,
			    RR_CLEAR, fb->planemask);
			/* restore colormap */
			diofb_resetcmap(fb);
			for (i = 0; i <= fb->planemask; i++)
				tvrx_setcolor(fb, i);
		}
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->width = fb->ri.ri_width;
		wdf->height = fb->ri.ri_height;
		wdf->depth = fb->ri.ri_depth;
		wdf->cmsize = 1 << fb->planes;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = fb->ri.ri_stride;
		break;
	case WSDISPLAYIO_GETCMAP:
		return (diofb_getcmap(fb, (struct wsdisplay_cmap *)data));
	case WSDISPLAYIO_PUTCMAP:
		return (tvrx_setcmap(fb, (struct wsdisplay_cmap *)data));
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;
	default:
		return (-1);
	}

	return (0);
}

void
tvrx_burner(void *v, u_int on, u_int flags)
{
	struct diofb *fb = v;

	tvrx_reg(fb->regkva, uint8_t, TVRX_DISPLAY_ENABLE) = on ? 0x01 : 0x00;
	tvrx_reg(fb->regkva, uint8_t, TVRX_FV_TRIG) = 0x01;
}

void
tvrx_setcolor(struct diofb *fb, u_int index)
{
	u_int index_scaled = index * 3 * 4;

	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_P + index_scaled) =
	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_S + index_scaled) =
	    fb->cmap.r[index];
	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_P + 4 + index_scaled) =
	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_S + 4 + index_scaled) =
	    fb->cmap.g[index];
	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_P + 2 * 4 + index_scaled) =
	tvrx_reg(fb->regkva, uint8_t, TVRX_CMAP_O_S + 2 * 4 + index_scaled) =
	    fb->cmap.b[index];
}

int
tvrx_setcmap(struct diofb *fb, struct wsdisplay_cmap *cm)
{
	u_int8_t r[256], g[256], b[256];
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << fb->planes;
	int error;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	if ((error = copyin(cm->red, r, count)) != 0)
		return (error);
	if ((error = copyin(cm->green, g, count)) != 0)
		return (error);
	if ((error = copyin(cm->blue, b, count)) != 0)
		return (error);

	bcopy(r, fb->cmap.r + index, count);
	bcopy(g, fb->cmap.g + index, count);
	bcopy(b, fb->cmap.b + index, count);

	while (count-- != 0)
		tvrx_setcolor(fb, index++);

	return (0);
}

int
tvrx_windowmove(struct diofb *fb, u_int16_t sx, u_int16_t sy, u_int16_t dx,
    u_int16_t dy, u_int16_t cx, u_int16_t cy, int16_t rop, int16_t planemask)
{
#ifdef TVRX_DEBUG
	printf("%s: %dx%d %dx%d %dx%d rx %x planemask %x\n",
	    __func__, fb->sx, sy, dx, dy, cx, cy, rop, planemask);
#endif

	planemask &= fb->planemask;

	tvrx_reg(fb->regkva, uint16_t, TVRX_SRC_Y) = sy;
	tvrx_reg(fb->regkva, uint16_t, TVRX_SRC_X) = sx;
	tvrx_reg(fb->regkva, uint16_t, TVRX_DST_Y) = dy;
	tvrx_reg(fb->regkva, uint16_t, TVRX_DST_X) = dx;
	tvrx_reg(fb->regkva, uint16_t, TVRX_CNT_Y) = cy;
	tvrx_reg(fb->regkva, uint16_t, TVRX_CNT_X) = cx;

	tvrx_reg(fb->regkva, uint8_t, TVRX_REP_RULE) = TVRX_ROP(rop);
	tvrx_reg(fb->regkva, uint8_t, TVRX_O_WEN) = planemask;
	tvrx_reg(fb->regkva, uint8_t, TVRX_WMOVE) = 1;
	tvrx_waitbusy(fb);

	if (planemask != fb->planemask) {
		rop ^= 0x0f;
		planemask ^= fb->planemask;
		tvrx_reg(fb->regkva, uint8_t, TVRX_REP_RULE) = TVRX_ROP(rop);
		tvrx_reg(fb->regkva, uint8_t, TVRX_O_WEN) = planemask;
		tvrx_reg(fb->regkva, uint8_t, TVRX_WMOVE) = 1;
		tvrx_waitbusy(fb);
	}

	tvrx_reg(fb->regkva, uint8_t, TVRX_O_WEN) = fb->planemask;
	tvrx_reg(fb->regkva, uint8_t, TVRX_REP_RULE) = TVRX_ROP(RR_COPY);

	return 0;
}

/*
 * Console support
 */

void
tvrxcninit()
{
	tvrx_reset(&diofb_cn, conscode, (struct diofbreg *)conaddr);
	diofb_cnattach(&diofb_cn);
}

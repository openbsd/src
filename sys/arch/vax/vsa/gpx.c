/*	$OpenBSD: gpx.c,v 1.25 2014/12/23 21:39:12 miod Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*-
 * Copyright (c) 1988 Regents of the University of California.
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
 *	@(#)qd.c	7.1 (Berkeley) 6/28/91
 */

/************************************************************************
*									*
*			Copyright (c) 1985-1988 by			*
*		Digital Equipment Corporation, Maynard, MA		*
*			All rights reserved.				*
*									*
*   This software is furnished under a license and may be used and	*
*   copied  only  in accordance with the terms of such license and	*
*   with the  inclusion  of  the  above  copyright  notice.   This	*
*   software  or  any  other copies thereof may not be provided or	*
*   otherwise made available to any other person.  No title to and	*
*   ownership of the software is hereby transferred.			*
*									*
*   The information in this software is subject to change  without	*
*   notice  and should not be construed as a commitment by Digital	*
*   Equipment Corporation.						*
*									*
*   Digital assumes no responsibility for the use  or  reliability	*
*   of its software on equipment which is not supplied by Digital.	*
*									*
*************************************************************************/

/*
 * Driver for the GPX color option on VAXstation 3100, based on the
 * MicroVAX II qdss driver.
 *
 * The frame buffer memory itself is not directly accessible (unlike
 * the on-board monochrome smg frame buffer), and writes through the
 * Dragon chip can only happen in multiples of 16 pixels, horizontally.
 *
 * Because of this limitation, the font image is copied to offscreen
 * memory (which there is plenty of), and screen to screen blt operations
 * are done for everything.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/ka420.h>
#include <machine/scb.h>
#include <machine/vsbus.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <dev/ic/bt458reg.h>
#if 0
#include <dev/ic/dc503reg.h>
#endif
#include <vax/qbus/qdreg.h>

#define	GPXADDR		0x3c000000	/* base address on VAXstation 3100 */

#define	GPX_ADDER_OFFSET	0x0000
#define	GPX_VDAC_OFFSET		0x0300
#define	GPX_CURSOR_OFFSET	0x0400	/* DC503 */
#define	GPX_READBACK_OFFSET	0x0500

#define	GPX_WIDTH	1024
#define	GPX_VISHEIGHT	864
#define	GPX_HEIGHT	2048

/* 4 plane option RAMDAC */
struct	ramdac4 {
	u_int16_t	colormap[16];
	u_int8_t	unknown[0x20];
	u_int16_t	cursormap[4];
	u_int8_t	unknown2[0x18];
	u_int16_t	control;
#define	RAMDAC4_INIT	0x0047
#define	RAMDAC4_ENABLE	0x0002
};

/* 8 plane option RAMDAC - Bt458 or compatible */
struct	ramdac8 {
	u_int16_t	address;
	u_int16_t	cmapdata;
	u_int16_t	control;
	u_int16_t	omapdata;
};

int	gpx_match(struct device *, void *, void *);
void	gpx_attach(struct device *, struct device *, void *);

struct	gpx_screen {
	struct rasops_info ss_ri;
	int		ss_console;
	u_int		ss_depth;
	u_int		ss_gpr;		/* font glyphs per row */
	struct adder	*ss_adder;
	void 		*ss_vdac;
	u_int8_t	ss_cmap[256 * 3];
#if 0
	struct dc503reg	*ss_cursor;
	u_int16_t	ss_curcmd;
#endif
};

/* for console */
struct gpx_screen gpx_consscr;

struct	gpx_softc {
	struct device sc_dev;
	struct gpx_screen *sc_scr;
	int	sc_nscreens;
};

struct cfattach gpx_ca = {
	sizeof(struct gpx_softc), gpx_match, gpx_attach,
};

struct	cfdriver gpx_cd = {
	NULL, "gpx", DV_DULL
};

struct wsscreen_descr gpx_stdscreen = {
	"std",
};

const struct wsscreen_descr *_gpx_scrlist[] = {
	&gpx_stdscreen,
};

const struct wsscreen_list gpx_screenlist = {
	sizeof(_gpx_scrlist) / sizeof(struct wsscreen_descr *),
	_gpx_scrlist,
};

int	gpx_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	gpx_mmap(void *, off_t, int);
int	gpx_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	gpx_free_screen(void *, void *);
int	gpx_show_screen(void *, void *, int,
	    void (*) (void *, int, int), void *);
int	gpx_load_font(void *, void *, struct wsdisplay_font *);
int	gpx_list_font(void *, struct wsdisplay_font *);
void	gpx_burner(void *, u_int, u_int);

const struct wsdisplay_accessops gpx_accessops = {
	.ioctl = gpx_ioctl,
	.mmap = gpx_mmap,
	.alloc_screen = gpx_alloc_screen,
	.free_screen = gpx_free_screen,
	.show_screen = gpx_show_screen,
	.load_font = gpx_load_font,
	.list_font = gpx_list_font,
	.burn_screen = gpx_burner
};

void	gpx_clear_screen(struct gpx_screen *);
void	gpx_copyrect(struct gpx_screen *, int, int, int, int, int, int);
void	gpx_fillrect(struct gpx_screen *, int, int, int, int, long, u_int);
int	gpx_getcmap(struct gpx_screen *, struct wsdisplay_cmap *);
void	gpx_loadcmap(struct gpx_screen *, int, int);
int	gpx_putcmap(struct gpx_screen *, struct wsdisplay_cmap *);
void	gpx_resetcmap(struct gpx_screen *);
void	gpx_reset_viper(struct gpx_screen *);
int	gpx_setup_screen(struct gpx_screen *);
void	gpx_upload_font(struct gpx_screen *);
int	gpx_viper_write(struct gpx_screen *, u_int, u_int16_t);
int	gpx_wait(struct gpx_screen *, int);

int	gpx_copycols(void *, int, int, int, int);
int	gpx_copyrows(void *, int, int, int);
int	gpx_do_cursor(struct rasops_info *);
int	gpx_erasecols(void *, int, int, int, long);
int	gpx_eraserows(void *, int, int, long);
int	gpx_putchar(void *, int, int, u_int, long);

/*
 * Autoconf glue
 */

int
gpx_match(struct device *parent, void *vcf, void *aux)
{
	struct vsbus_attach_args *va = aux;
	volatile struct adder *adder;
	vaddr_t tmp;
	u_int depth;
	u_short status;
	extern struct consdev wsdisplay_cons;
	extern int oldvsbus;

	switch (vax_boardtype) {
	default:
		return (0);

	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if (va->va_paddr != GPXADDR)
			return (0);

		/* not present on microvaxes */
		if ((vax_confdata & KA420_CFG_MULTU) != 0)
			return (0);

		if ((vax_confdata & KA420_CFG_VIDOPT) == 0)
			return (0);
		break;
	}

	/* Check for hardware */
	adder = (volatile struct adder *)
	    vax_map_physmem(va->va_paddr + GPX_ADDER_OFFSET, 1);
	if (adder == NULL)
		return (0);
	adder->status = 0;
	status = adder->status;
	vax_unmap_physmem((vaddr_t)adder, 1);
	if (status == offsetof(struct adder, status))
		return (0);

	/* Check for a recognized color depth */
	tmp = vax_map_physmem(va->va_paddr + GPX_READBACK_OFFSET, 1);
	if (tmp == 0L)
		return (0);
	depth = (*(u_int16_t *)tmp) & 0x00f0;
	vax_unmap_physmem(tmp, 1);
	if (depth != 0x00f0 && depth != 0x0080)
		return (0);

	/* when already running as console, always fake things */
	if ((vax_confdata & KA420_CFG_L3CON) == 0 &&
	    cn_tab == &wsdisplay_cons) {
		struct vsbus_softc *sc = (void *)parent;
		sc->sc_mask = 0x08;
		scb_fake(0x44, oldvsbus ? 0x14 : 0x15);
	} else {
		adder = (struct adder *)vax_map_physmem(va->va_paddr +
		    GPX_ADDER_OFFSET, 1);
		if (adder == NULL)
			return (0);
		adder->interrupt_enable = FRAME_SYNC;
		DELAY(100000);	/* enough to get a retrace interrupt */
		adder->interrupt_enable = 0;
		vax_unmap_physmem((vaddr_t)adder, 1);
	}
	return (20);
}

void
gpx_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpx_softc *sc = (struct gpx_softc *)self;
	struct vsbus_attach_args *va = aux;
	struct gpx_screen *scr;
	struct wsemuldisplaydev_attach_args aa;
	int console;
	vaddr_t tmp;
	extern struct consdev wsdisplay_cons;

	console = (vax_confdata & KA420_CFG_L3CON) == 0 &&
	    cn_tab == &wsdisplay_cons;
	if (console) {
		scr = &gpx_consscr;
		sc->sc_nscreens = 1;
	} else {
		scr = malloc(sizeof(struct gpx_screen), M_DEVBUF, M_NOWAIT);
		if (scr == NULL) {
			printf(": can not allocate memory\n");
			return;
		}

		tmp = vax_map_physmem(va->va_paddr + GPX_READBACK_OFFSET, 1);
		if (tmp == 0L) {
			printf(": can not probe depth\n");
			goto bad1;
		}
		scr->ss_depth = (*(u_int16_t *)tmp & 0x00f0) == 0x00f0 ? 4 : 8;
		vax_unmap_physmem(tmp, 1);

		scr->ss_adder = (struct adder *)vax_map_physmem(va->va_paddr +
		    GPX_ADDER_OFFSET, 1);
		if (scr->ss_adder == NULL) {
			printf(": can not map frame buffer registers\n");
			goto bad1;
		}

		scr->ss_vdac = (void *)vax_map_physmem(va->va_paddr +
		    GPX_VDAC_OFFSET, 1);
		if (scr->ss_vdac == NULL) {
			printf(": can not map RAMDAC\n");
			goto bad2;
		}

#if 0
		scr->ss_cursor =
		    (struct dc503reg *)vax_map_physmem(va->va_paddr +
		    GPX_CURSOR_OFFSET, 1);
		if (scr->ss_cursor == NULL) {
			printf(": can not map cursor chip\n");
			goto bad3;
		}
#endif

		if (gpx_setup_screen(scr) != 0) {
			printf(": initialization failed\n");
			goto bad4;
		}
	}
	sc->sc_scr = scr;

	printf("\n%s: %dx%d %d plane color framebuffer\n",
	    self->dv_xname, GPX_WIDTH, GPX_VISHEIGHT, scr->ss_depth);

	aa.console = console;
	aa.scrdata = &gpx_screenlist;
	aa.accessops = &gpx_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found(self, &aa, wsemuldisplaydevprint);

	return;

bad4:
#if 0
	vax_unmap_physmem((vaddr_t)scr->ss_cursor, 1);
bad3:
#endif
	vax_unmap_physmem((vaddr_t)scr->ss_vdac, 1);
bad2:
	vax_unmap_physmem((vaddr_t)scr->ss_adder, 1);
bad1:
	free(scr, M_DEVBUF, sizeof(struct gpx_screen));
}

/*
 * wsdisplay accessops
 */

int
gpx_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct gpx_softc *sc = v;
	struct gpx_screen *ss = sc->sc_scr;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_GPX;
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = ss->ss_ri.ri_height;
		wdf->width = ss->ss_ri.ri_width;
		wdf->depth = ss->ss_depth;
		wdf->cmsize = 1 << ss->ss_depth;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = gpx_getcmap(ss, cm);
		if (error != 0)
			return (error);
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = gpx_putcmap(ss, cm);
		if (error != 0)
			return (error);
		gpx_loadcmap(ss, cm->index, cm->count);
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	case WSDISPLAYIO_LINEBYTES:	/* no linear mapping */
	default:
		return (-1);
	}

	return (0);
}

paddr_t
gpx_mmap(void *v, off_t offset, int prot)
{
	return (-1);
}

int
gpx_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct gpx_softc *sc = v;
	struct gpx_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, defattrp);
	sc->sc_nscreens++;

	return (0);
}

void
gpx_free_screen(void *v, void *cookie)
{
	struct gpx_softc *sc = v;

	sc->sc_nscreens--;
}

int
gpx_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

int
gpx_load_font(void *v, void *emulcookie, struct wsdisplay_font *font)
{
	struct gpx_softc *sc = v;
	struct gpx_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;
	int wsfcookie;
	struct wsdisplay_font *wsf;
	const char *name;

	/*
	 * We can't use rasops_load_font() directly, as we need to make
	 * sure that, when switching fonts, the font bits are set up in
	 * the correct bit order, and uploaded off-screen.
	 */

	if (font->data != NULL)
		return rasops_load_font(ri, emulcookie, font);

	/* allow an empty font name to revert to the initial font choice */
	name = font->name;
	if (*name == '\0')
		name = NULL;

	wsfcookie = wsfont_find(name, ri->ri_font->fontwidth,
	    ri->ri_font->fontheight, 0);
	if (wsfcookie < 0) {
		wsfcookie = wsfont_find(name, 0, 0, 0);
		if (wsfcookie < 0)
			return ENOENT;
		else
			return EINVAL;
	}
	if (wsfont_lock(wsfcookie, &wsf,
	    WSDISPLAY_FONTORDER_R2L, WSDISPLAY_FONTORDER_L2R) <= 0)
		return EINVAL;

	/* if (ri->ri_wsfcookie >= 0) */
		wsfont_unlock(ri->ri_wsfcookie);
	ri->ri_wsfcookie = wsfcookie;
	ri->ri_font = wsf;
	ri->ri_fontscale = ri->ri_font->fontheight * ri->ri_font->stride;

	gpx_upload_font(ss);

	return 0;
}

int
gpx_list_font(void *v, struct wsdisplay_font *font)
{
	struct gpx_softc *sc = v;
	struct gpx_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	return rasops_list_font(ri, font);
}

void
gpx_burner(void *v, u_int on, u_int flags)
{
	struct gpx_softc *sc = v;
	struct gpx_screen *ss = sc->sc_scr;

	if (ss->ss_depth == 8) {
		struct ramdac8 *rd = ss->ss_vdac;
		rd->address = BT_CR;
		if (on)
			rd->control = BTCR_RAMENA | BTCR_BLINK_1648 |
			    BTCR_MPLX_4;
		else
			/* fade colormap to black as well? */
			rd->control = BTCR_BLINK_1648 | BTCR_MPLX_4;
	} else {
		struct ramdac4 *rd = ss->ss_vdac;
		if (on)
			rd->control = RAMDAC4_INIT;
		else
			rd->control = RAMDAC4_INIT & ~RAMDAC4_ENABLE;
	}
}

/*
 * wsdisplay emulops
 */

int
gpx_putchar(void *v, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = v;
	struct gpx_screen *ss = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int dx, dy, sx, sy, fg, bg, ul;

	ri->ri_ops.unpack_attr(v, attr, &fg, &bg, &ul);

	/* find where to output the glyph... */
	dx = col * font->fontwidth + ri->ri_xorigin;
	dy = row * font->fontheight + ri->ri_yorigin;
	/* ... and where to pick it from */
	uc -= font->firstchar;
	sx = (uc % ss->ss_gpr) * font->stride * NBBY;
	sy = GPX_HEIGHT - (1 + uc / ss->ss_gpr) * font->fontheight;

	/* setup VIPER operand control registers */
	while (gpx_viper_write(ss, CS_UPDATE_MASK, 0x00ff));
	gpx_viper_write(ss, SRC1_OCR_B,
	    EXT_NONE | INT_SOURCE | ID | BAR_SHIFT_DELAY);
	gpx_viper_write(ss, DST_OCR_B,
	    EXT_NONE | INT_NONE | NO_ID | NO_BAR_SHIFT_DELAY);
	gpx_viper_write(ss, MASK_1, 0xffff);
	gpx_viper_write(ss, VIPER_Z_LOAD | FOREGROUND_COLOR_Z, fg);
	gpx_viper_write(ss, VIPER_Z_LOAD | BACKGROUND_COLOR_Z, bg);
	ss->ss_adder->x_clip_min = 0;
	ss->ss_adder->x_clip_max = GPX_WIDTH;
	ss->ss_adder->y_clip_min = 0;
	ss->ss_adder->y_clip_max = GPX_VISHEIGHT;
	/* load DESTINATION origin and vectors */
	ss->ss_adder->fast_dest_dy = 0;
	ss->ss_adder->slow_dest_dx = 0;
	ss->ss_adder->error_1 = 0;
	ss->ss_adder->error_2 = 0;
	ss->ss_adder->rasterop_mode = DST_WRITE_ENABLE | NORMAL;
	gpx_wait(ss, RASTEROP_COMPLETE);
	ss->ss_adder->destination_x = dx;
	ss->ss_adder->fast_dest_dx = font->fontwidth;
	ss->ss_adder->destination_y = dy;
	ss->ss_adder->slow_dest_dy = font->fontheight;
	/* load SOURCE origin and vectors */
	ss->ss_adder->source_1_x = sx;
	ss->ss_adder->source_1_y = sy;
	ss->ss_adder->source_1_dx = font->fontwidth;
	ss->ss_adder->source_1_dy = font->fontheight;
	ss->ss_adder->cmd = RASTEROP | OCRB | S1E | DTE | LF_R1;

	if (ul != 0) {
		gpx_fillrect(ss, dx, dy + font->fontheight - 2, font->fontwidth,
		    1, attr, LF_R3);	/* fg fill */
	}

	return 0;
}

int
gpx_copycols(void *v, int row, int src, int dst, int cnt)
{
	struct rasops_info *ri = v;
	struct gpx_screen *ss = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int sx, y, dx, w, h;

	sx = ri->ri_xorigin + src * font->fontwidth;
	dx = ri->ri_xorigin + dst * font->fontwidth;
	w = cnt * font->fontwidth;
	y = ri->ri_yorigin + row * font->fontheight;
	h = font->fontheight;

	gpx_copyrect(ss, sx, y, dx, y, w, h);

	return 0;
}

int
gpx_erasecols(void *v, int row, int col, int cnt, long attr)
{
	struct rasops_info *ri = v;
	struct gpx_screen *ss = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, y, dx, dy;

	x = ri->ri_xorigin + col * font->fontwidth;
	dx = cnt * font->fontwidth;
	y = ri->ri_yorigin + row * font->fontheight;
	dy = font->fontheight;

	gpx_fillrect(ss, x, y, dx, dy, attr, LF_R2); /* bg fill */

	return 0;
}

int
gpx_copyrows(void *v, int src, int dst, int cnt)
{
	struct rasops_info *ri = v;
	struct gpx_screen *ss = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, sy, dy, w, h;

	x = ri->ri_xorigin;
	w = ri->ri_emustride;
	sy = ri->ri_yorigin + src * font->fontheight;
	dy = ri->ri_yorigin + dst * font->fontheight;
	h = cnt * font->fontheight;

	gpx_copyrect(ss, x, sy, x, dy, w, h);

	return 0;
}

int
gpx_eraserows(void *v, int row, int cnt, long attr)
{
	struct rasops_info *ri = v;
	struct gpx_screen *ss = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, y, dx, dy;

	x = ri->ri_xorigin;
	dx = ri->ri_emustride;
	y = ri->ri_yorigin + row * font->fontheight;
	dy = cnt * font->fontheight;

	gpx_fillrect(ss, x, y, dx, dy, attr, LF_R2); /* bg fill */

	return 0;
}

int
gpx_do_cursor(struct rasops_info *ri)
{
	struct gpx_screen *ss = ri->ri_hw;
	int x, y, w, h;

	x = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;
	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;

	gpx_fillrect(ss, x, y, w, h, WSCOL_WHITE << 24, LF_R4);	/* invert */

	return 0;
}

/*
 * low-level programming routines
 */

int
gpx_wait(struct gpx_screen *ss, int bits)
{
	int i;

	ss->ss_adder->status = 0;
	for (i = 100000; i != 0; i--) {
		if ((ss->ss_adder->status & bits) == bits)
			break;
		DELAY(1);
	}

	return (i == 0);
}

int
gpx_viper_write(struct gpx_screen *ss, u_int reg, u_int16_t val)
{
	if (gpx_wait(ss, ADDRESS_COMPLETE) == 0 &&
	    gpx_wait(ss, TX_READY) == 0) {
		ss->ss_adder->id_data = val;
		ss->ss_adder->command = ID_LOAD | reg;
		return (0);
	}
#ifdef DEBUG
	if (ss->ss_console == 0)	/* don't make things worse! */
		printf("gpx_viper_write failure, reg %x val %x\n", reg, val);
#endif
	return (1);
}

/* Initialize the damned beast. Straight from qdss. */
void
gpx_reset_viper(struct gpx_screen *ss)
{
	int i;

	ss->ss_adder->interrupt_enable = 0;
	ss->ss_adder->command = CANCEL;
	/* set monitor timing */
	ss->ss_adder->x_scan_count_0 = 0x2800;
	ss->ss_adder->x_scan_count_1 = 0x1020;
	ss->ss_adder->x_scan_count_2 = 0x003a;
	ss->ss_adder->x_scan_count_3 = 0x38f0;
	ss->ss_adder->x_scan_count_4 = 0x6128;
	ss->ss_adder->x_scan_count_5 = 0x093a;
	ss->ss_adder->x_scan_count_6 = 0x313c;
	ss->ss_adder->sync_phase_adj = 0x0100;
	ss->ss_adder->x_scan_conf = 0x00c8;
	/*
	 * got a bug in second pass ADDER! lets take care of it...
	 *
	 * normally, just use the code in the following bug fix code, but to
	 * make repeated demos look pretty, load the registers as if there was
	 * no bug and then test to see if we are getting sync
	 */
	ss->ss_adder->y_scan_count_0 = 0x135f;
	ss->ss_adder->y_scan_count_1 = 0x3363;
	ss->ss_adder->y_scan_count_2 = 0x2366;
	ss->ss_adder->y_scan_count_3 = 0x0388;
	/*
	 * if no sync, do the bug fix code
	 */
	if (gpx_wait(ss, FRAME_SYNC) != 0) {
		/*
		 * First load all Y scan registers with very short frame and
		 * wait for scroll service.  This guarantees at least one SYNC
		 * to fix the pass 2 Adder initialization bug (synchronizes
		 * XCINCH with DMSEEDH)
		 */
		ss->ss_adder->y_scan_count_0 = 0x01;
		ss->ss_adder->y_scan_count_1 = 0x01;
		ss->ss_adder->y_scan_count_2 = 0x01;
		ss->ss_adder->y_scan_count_3 = 0x01;
		/* delay at least 1 full frame time */
		gpx_wait(ss, FRAME_SYNC);
		gpx_wait(ss, FRAME_SYNC);
		/*
		 * now load the REAL sync values (in reverse order just to
		 * be safe).
		 */
		ss->ss_adder->y_scan_count_3 = 0x0388;
		ss->ss_adder->y_scan_count_2 = 0x2366;
		ss->ss_adder->y_scan_count_1 = 0x3363;
		ss->ss_adder->y_scan_count_0 = 0x135f;
	}
	/* zero the index registers */
	ss->ss_adder->x_index_pending = 0;
	ss->ss_adder->y_index_pending = 0;
	ss->ss_adder->x_index_new = 0;
	ss->ss_adder->y_index_new = 0;
	ss->ss_adder->x_index_old = 0;
	ss->ss_adder->y_index_old = 0;
	ss->ss_adder->pause = 0;
	/* set rasterop mode to normal pen down */
	ss->ss_adder->rasterop_mode =
	    DST_WRITE_ENABLE | DST_INDEX_ENABLE | NORMAL;
	/* set the rasterop registers to default values */
	ss->ss_adder->source_1_dx = 1;
	ss->ss_adder->source_1_dy = 1;
	ss->ss_adder->source_1_x = 0;
	ss->ss_adder->source_1_y = 0;
	ss->ss_adder->destination_x = 0;
	ss->ss_adder->destination_y = 0;
	ss->ss_adder->fast_dest_dx = 1;
	ss->ss_adder->fast_dest_dy = 0;
	ss->ss_adder->slow_dest_dx = 0;
	ss->ss_adder->slow_dest_dy = 1;
	ss->ss_adder->error_1 = 0;
	ss->ss_adder->error_2 = 0;
	/* scale factor = UNITY */
	ss->ss_adder->fast_scale = UNITY;
	ss->ss_adder->slow_scale = UNITY;
	/* set the source 2 parameters */
	ss->ss_adder->source_2_x = 0;
	ss->ss_adder->source_2_y = 0;
	ss->ss_adder->source_2_size = 0x0022;
	/* initialize plane addresses for eight vipers */
	for (i = 0; i < 8; i++) {
		gpx_viper_write(ss, CS_UPDATE_MASK, 1 << i);
		gpx_viper_write(ss, PLANE_ADDRESS, i);
	}
	/* initialize the external registers. */
	gpx_viper_write(ss, CS_UPDATE_MASK, 0x00ff);
	gpx_viper_write(ss, CS_SCROLL_MASK, 0x00ff);
	/* initialize resolution mode */
	gpx_viper_write(ss, MEMORY_BUS_WIDTH, 0x000c);	/* bus width = 16 */
	gpx_viper_write(ss, RESOLUTION_MODE, 0x0000);	/* one bit/pixel */
	/* initialize viper registers */
	gpx_viper_write(ss, SCROLL_CONSTANT,
	    SCROLL_ENABLE | VIPER_LEFT | VIPER_UP);
	gpx_viper_write(ss, SCROLL_FILL, 0x0000);
	/* set clipping and scrolling limits to full screen */
	gpx_wait(ss, ADDRESS_COMPLETE);
	ss->ss_adder->x_clip_min = 0;
	ss->ss_adder->x_clip_max = GPX_WIDTH;
	ss->ss_adder->y_clip_min = 0;
	ss->ss_adder->y_clip_max = GPX_HEIGHT;
	ss->ss_adder->scroll_x_min = 0;
	ss->ss_adder->scroll_x_max = GPX_WIDTH;
	ss->ss_adder->scroll_y_min = 0;
	ss->ss_adder->scroll_y_max = GPX_HEIGHT;
	gpx_wait(ss, FRAME_SYNC);	/* wait at LEAST 1 full frame */
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->x_index_pending = 0;
	ss->ss_adder->y_index_pending = 0;
	ss->ss_adder->x_index_new = 0;
	ss->ss_adder->y_index_new = 0;
	ss->ss_adder->x_index_old = 0;
	ss->ss_adder->y_index_old = 0;
	gpx_wait(ss, ADDRESS_COMPLETE);
	gpx_viper_write(ss, LEFT_SCROLL_MASK, 0x0000);
	gpx_viper_write(ss, RIGHT_SCROLL_MASK, 0x0000);
	/* set source and the mask register to all ones */
	gpx_viper_write(ss, SOURCE, 0xffff);
	gpx_viper_write(ss, MASK_1, 0xffff);
	gpx_viper_write(ss, VIPER_Z_LOAD | FOREGROUND_COLOR_Z, 255);
	gpx_viper_write(ss, VIPER_Z_LOAD | BACKGROUND_COLOR_Z, 0);
	/* initialize Operand Control Register banks for fill command */
	gpx_viper_write(ss, SRC1_OCR_A, EXT_NONE | INT_M1_M2  | NO_ID | WAIT);
	gpx_viper_write(ss, SRC2_OCR_A, EXT_NONE | INT_SOURCE | NO_ID | NO_WAIT);
	gpx_viper_write(ss, DST_OCR_A, EXT_NONE | INT_NONE | NO_ID | NO_WAIT);
	gpx_viper_write(ss, SRC1_OCR_B, EXT_NONE | INT_SOURCE | NO_ID | WAIT);
	gpx_viper_write(ss, SRC2_OCR_B, EXT_NONE | INT_M1_M2  | NO_ID | NO_WAIT);
	gpx_viper_write(ss, DST_OCR_B, EXT_NONE | INT_NONE | NO_ID | NO_WAIT);

	/*
	 * Init Logic Unit Function registers.
	 */
	/* putchar */
	gpx_viper_write(ss, LU_FUNCTION_R1, FULL_SRC_RESOLUTION | LF_SOURCE);
	/* erase{cols,rows} */
	gpx_viper_write(ss, LU_FUNCTION_R2, FULL_SRC_RESOLUTION | LF_ZEROS);
	/* underline */
	gpx_viper_write(ss, LU_FUNCTION_R3, FULL_SRC_RESOLUTION | LF_ONES);
	/* cursor */
	gpx_viper_write(ss, LU_FUNCTION_R4, FULL_SRC_RESOLUTION | LF_NOT_D);
}

/* Clear the whole screen. Straight from qdss. */
void
gpx_clear_screen(struct gpx_screen *ss)
{
	ss->ss_adder->x_limit = GPX_WIDTH;
	ss->ss_adder->y_limit = GPX_HEIGHT;
	ss->ss_adder->y_offset_pending = 0;
	gpx_wait(ss, FRAME_SYNC);	/* wait at LEAST 1 full frame */
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_scroll_constant = SCROLL_ERASE;
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_offset_pending = GPX_VISHEIGHT;
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_scroll_constant = SCROLL_ERASE;
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_offset_pending = 2 * GPX_VISHEIGHT;
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_scroll_constant = SCROLL_ERASE;
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->y_offset_pending = 0;	 /* back to normal */
	gpx_wait(ss, FRAME_SYNC);
	gpx_wait(ss, FRAME_SYNC);
	ss->ss_adder->x_limit = GPX_WIDTH;
	ss->ss_adder->y_limit = GPX_VISHEIGHT;
}

int
gpx_setup_screen(struct gpx_screen *ss)
{
	struct rasops_info *ri = &ss->ss_ri;
	int cookie;

	bzero(ri, sizeof(*ri));
	ri->ri_depth = 8;	/* masquerade as a 8 bit device for rasops */
	ri->ri_width = GPX_WIDTH;
	ri->ri_height = GPX_VISHEIGHT;
	ri->ri_stride = GPX_WIDTH;
	ri->ri_flg = RI_CENTER;		/* no RI_CLEAR as ri_bits is NULL! */
	ri->ri_hw = ss;

	/*
	 * We can not let rasops select our font, because we need to use
	 * a font with right-to-left bit order on this frame buffer.
	 */
	wsfont_init();
	cookie = wsfont_find(NULL, 12, 0, 0);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 8, 0, 0);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 0, 0, 0);
	if (cookie <= 0)
		return (-1);
	if (wsfont_lock(cookie, &ri->ri_font,
	    WSDISPLAY_FONTORDER_R2L, WSDISPLAY_FONTORDER_L2R) <= 0)
		return (-1);
	ri->ri_wsfcookie = cookie;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

	/*
	 * Override the rasops emulops.
	 */
	ri->ri_ops.copyrows = gpx_copyrows;
	ri->ri_ops.copycols = gpx_copycols;
	ri->ri_ops.eraserows = gpx_eraserows;
	ri->ri_ops.erasecols = gpx_erasecols;
	ri->ri_ops.putchar = gpx_putchar;
	ri->ri_do_cursor = gpx_do_cursor;

	gpx_stdscreen.ncols = ri->ri_cols;
	gpx_stdscreen.nrows = ri->ri_rows;
	gpx_stdscreen.textops = &ri->ri_ops;
	gpx_stdscreen.fontwidth = ri->ri_font->fontwidth;
	gpx_stdscreen.fontheight = ri->ri_font->fontheight;
	gpx_stdscreen.capabilities = ri->ri_caps;

	/*
	 * Initialize RAMDAC.
	 */
	if (ss->ss_depth == 8) {
		struct ramdac8 *rd = ss->ss_vdac;
		rd->address = BT_CR;
		rd->control = BTCR_RAMENA | BTCR_BLINK_1648 | BTCR_MPLX_4;
	} else {
		struct ramdac4 *rd = ss->ss_vdac;
		rd->control = RAMDAC4_INIT;
	}

	/*
	 * Put the ADDER and VIPER in a good state.
	 */
	gpx_reset_viper(ss);

	/*
	 * Initialize colormap.
	 */
	gpx_resetcmap(ss);

	/*
	 * Clear display (including non-visible area), in 864 lines chunks.
	 */
	gpx_clear_screen(ss);

	/*
	 * Copy our font to the offscreen area.
	 */
	gpx_upload_font(ss);

#if 0
	ss->ss_cursor->cmdr = ss->ss_curcmd = PCCCMD_HSHI;
#endif

	return (0);
}

/*
 * Copy the selected wsfont to non-visible frame buffer area.
 * This is necessary since the only way to send data to the frame buffer
 * is through the ID interface, which is slow and needs 16 bit wide data.
 * Adapted from qdss.
 */
void
gpx_upload_font(struct gpx_screen *ss)
{
	struct rasops_info *ri = &ss->ss_ri;
	struct wsdisplay_font *font = ri->ri_font;
	u_int8_t *fontbits, *fb;
	u_int remaining, nchars, row;
	u_int i, j;
	u_int16_t data;

	/* setup VIPER operand control registers */

	gpx_viper_write(ss, MASK_1, 0xffff);
	gpx_viper_write(ss, VIPER_Z_LOAD | FOREGROUND_COLOR_Z, 255);
	gpx_viper_write(ss, VIPER_Z_LOAD | BACKGROUND_COLOR_Z, 0);

	gpx_viper_write(ss, SRC1_OCR_B,
	    EXT_NONE | INT_NONE | ID | BAR_SHIFT_DELAY);
	gpx_viper_write(ss, SRC2_OCR_B,
	    EXT_NONE | INT_NONE | ID | BAR_SHIFT_DELAY);
	gpx_viper_write(ss, DST_OCR_B,
	    EXT_SOURCE | INT_NONE | NO_ID | NO_BAR_SHIFT_DELAY);

	ss->ss_adder->rasterop_mode =
	    DST_WRITE_ENABLE | DST_INDEX_ENABLE | NORMAL;
	gpx_wait(ss, RASTEROP_COMPLETE);

	/*
	 * Load font data. The font is uploaded in 8 or 16 bit wide cells, on
	 * as many ``lines'' as necessary at the end of the display.
	 */
	ss->ss_gpr = MIN(GPX_WIDTH / (NBBY * font->stride), font->numchars);
	if (ss->ss_gpr & 1)
		ss->ss_gpr--;
	fontbits = font->data;
	for (row = 1, remaining = font->numchars; remaining != 0;
	    row++, remaining -= nchars) {
		nchars = MIN(ss->ss_gpr, remaining);

		ss->ss_adder->destination_x = 0;
		ss->ss_adder->destination_y =
		    GPX_HEIGHT - row * font->fontheight;
		ss->ss_adder->fast_dest_dx = nchars * 16;
		ss->ss_adder->slow_dest_dy = font->fontheight;

		/* setup for processor to bitmap xfer */
		gpx_viper_write(ss, CS_UPDATE_MASK, 0x00ff);
		ss->ss_adder->cmd = PBT | OCRB | DTE | LF_R1 | 2; /*XXX why 2?*/

		/* iteratively do the processor to bitmap xfer */
		for (i = font->fontheight; i != 0; i--) {
			fb = fontbits;
			fontbits += font->stride;
			/* PTOB a scan line */
			for (j = nchars; j != 0; j--) {
				/* PTOB one scan of a char cell */
				if (font->stride == 1) {
					data = *fb;
					fb += font->fontheight;
					/*
					 * Do not access past font memory if
					 * it has an odd number of characters
					 * and this is the last pair.
					 */
					if (j != 1 || (nchars & 1) == 0 ||
					    remaining != nchars) {
						data |= ((u_int16_t)*fb) << 8;
						fb += font->fontheight;
					}
				} else {
					data =
					    fb[0] | (((u_int16_t)fb[1]) << 8);
					fb += font->fontheight * font->stride;
				}

				gpx_wait(ss, TX_READY);
				ss->ss_adder->id_data = data;
			}
		}
		fontbits += (nchars - 1) * font->stride * font->fontheight;
	}
}

void
gpx_copyrect(struct gpx_screen *ss,
    int sx, int sy, int dx, int dy, int w, int h)
{
	while (gpx_viper_write(ss, CS_UPDATE_MASK, 0x00ff));
	gpx_viper_write(ss, MASK_1, 0xffff);
	gpx_viper_write(ss, VIPER_Z_LOAD | FOREGROUND_COLOR_Z, 255);
	gpx_viper_write(ss, VIPER_Z_LOAD | BACKGROUND_COLOR_Z, 0);
	gpx_viper_write(ss, SRC1_OCR_B,
	    EXT_NONE | INT_SOURCE | ID | BAR_SHIFT_DELAY);
	gpx_viper_write(ss, DST_OCR_B,
	    EXT_NONE | INT_NONE | NO_ID | NO_BAR_SHIFT_DELAY);
	ss->ss_adder->fast_dest_dy = 0;
	ss->ss_adder->slow_dest_dx = 0;
	ss->ss_adder->error_1 = 0;
	ss->ss_adder->error_2 = 0;
	ss->ss_adder->rasterop_mode = DST_WRITE_ENABLE | NORMAL;
	gpx_wait(ss, RASTEROP_COMPLETE);
	ss->ss_adder->destination_x = dx;
	ss->ss_adder->fast_dest_dx = w;
	ss->ss_adder->destination_y = dy;
	ss->ss_adder->slow_dest_dy = h;
	ss->ss_adder->source_1_x = sx;
	ss->ss_adder->source_1_dx = w;
	ss->ss_adder->source_1_y = sy;
	ss->ss_adder->source_1_dy = h;
	ss->ss_adder->cmd = RASTEROP | OCRB | S1E | DTE | LF_R1;
}

/*
 * Fill a rectangle with the given attribute and function (i.e. rop).
 */
void
gpx_fillrect(struct gpx_screen *ss, int x, int y, int dx, int dy, long attr,
    u_int function)
{
	struct rasops_info *ri = &ss->ss_ri;
	int fg, bg;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	while (gpx_viper_write(ss, CS_UPDATE_MASK, 0x00ff));
	gpx_viper_write(ss, MASK_1, 0xffff);
	gpx_viper_write(ss, SOURCE, 0xffff);
	gpx_viper_write(ss, VIPER_Z_LOAD | FOREGROUND_COLOR_Z, fg);
	gpx_viper_write(ss, VIPER_Z_LOAD | BACKGROUND_COLOR_Z, bg);
	gpx_viper_write(ss, SRC1_OCR_B,
	    EXT_NONE | INT_SOURCE | ID | BAR_SHIFT_DELAY);
	gpx_viper_write(ss, DST_OCR_B,
	    EXT_NONE | INT_NONE | NO_ID | NO_BAR_SHIFT_DELAY);
	ss->ss_adder->fast_dest_dx = 0;
	ss->ss_adder->fast_dest_dy = 0;
	ss->ss_adder->slow_dest_dx = 0;
	ss->ss_adder->error_1 = 0;
	ss->ss_adder->error_2 = 0;
	ss->ss_adder->rasterop_mode = DST_WRITE_ENABLE | NORMAL;
	gpx_wait(ss, RASTEROP_COMPLETE);
	ss->ss_adder->destination_x = x;
	ss->ss_adder->fast_dest_dx = dx;
	ss->ss_adder->destination_y = y;
	ss->ss_adder->slow_dest_dy = dy;
	ss->ss_adder->source_1_x = x;
	ss->ss_adder->source_1_dx = dx;
	ss->ss_adder->source_1_y = y;
	ss->ss_adder->source_1_dy = dy;
	ss->ss_adder->cmd = RASTEROP | OCRB | S1E | DTE | function;
}

/*
 * Colormap handling routines
 */

int
gpx_getcmap(struct gpx_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count, i;
	u_int colcount = 1 << ss->ss_depth;
	int error;
	u_int8_t ramp[256], *c, *r;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	/* extract reds */
	c = ss->ss_cmap + 0 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c << (8 - ss->ss_depth), c += 3;
	if ((error = copyout(ramp, cm->red, count)) != 0)
		return (error);

	/* extract greens */
	c = ss->ss_cmap + 1 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c << (8 - ss->ss_depth), c += 3;
	if ((error = copyout(ramp, cm->green, count)) != 0)
		return (error);

	/* extract blues */
	c = ss->ss_cmap + 2 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c << (8 - ss->ss_depth), c += 3;
	if ((error = copyout(ramp, cm->blue, count)) != 0)
		return (error);

	return (0);
}

int
gpx_putcmap(struct gpx_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << ss->ss_depth;
	int i, error;
	u_int8_t r[256], g[256], b[256], *nr, *ng, *nb, *c;

	if (index >= colcount || count > colcount - index)
		return (EINVAL);

	if ((error = copyin(cm->red, r, count)) != 0)
		return (error);
	if ((error = copyin(cm->green, g, count)) != 0)
		return (error);
	if ((error = copyin(cm->blue, b, count)) != 0)
		return (error);

	nr = r, ng = g, nb = b;
	c = ss->ss_cmap + index * 3;
	for (i = count; i != 0; i--) {
		*c++ = *nr++ >> (8 - ss->ss_depth);
		*c++ = *ng++ >> (8 - ss->ss_depth);
		*c++ = *nb++ >> (8 - ss->ss_depth);
	}

	return (0);
}

void
gpx_loadcmap(struct gpx_screen *ss, int from, int count)
{
	u_int8_t *cmap = ss->ss_cmap;
	int i, color12;

	gpx_wait(ss, FRAME_SYNC);
	if (ss->ss_depth == 8) {
		struct ramdac8 *rd = ss->ss_vdac;

		cmap += from * 3;
		rd->address = from;
		for (i = 0; i < count * 3; i++)
			rd->cmapdata = *cmap++;
	} else {
		struct ramdac4 *rd = ss->ss_vdac;

		cmap = ss->ss_cmap + from;
		for (i = from; i < from + count; i++) {
			color12  = (*cmap++ >> 4) << 0;
			color12 |= (*cmap++ >> 4) << 8;
			color12 |= (*cmap++ >> 4) << 4;
			rd->colormap[i] = color12;
		}
	}
}

void
gpx_resetcmap(struct gpx_screen *ss)
{
	if (ss->ss_depth == 8)
		bcopy(rasops_cmap, ss->ss_cmap, sizeof(ss->ss_cmap));
	else {
		bcopy(rasops_cmap, ss->ss_cmap, 8 * 3);
		bcopy(rasops_cmap + 0xf8 * 3, ss->ss_cmap + 8 * 3, 8 * 3);
	}
	gpx_loadcmap(ss, 0, 1 << ss->ss_depth);

	/*
	 * On the 4bit RAMDAC, make the hardware cursor black on black
	 */
	if (ss->ss_depth != 8) {
		struct ramdac4 *rd = ss->ss_vdac;

		rd->cursormap[0] = rd->cursormap[1] =
		    rd->cursormap[2] = rd->cursormap[3] = 0x0000;
	}
}

/*
 * Console support code
 */

int	gpxcnprobe(void);
int	gpxcninit(void);

int
gpxcnprobe()
{
	extern vaddr_t virtual_avail;
	volatile struct adder *adder;
	vaddr_t tmp;
	int depth;
	u_short status;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & (KA420_CFG_L3CON | KA420_CFG_MULTU)) != 0)
			break; /* doesn't use graphics console */

		if ((vax_confdata & KA420_CFG_VIDOPT) == 0)
			break; /* no color option */

		/* Check for hardware */
		tmp = virtual_avail;
		ioaccess(tmp, vax_trunc_page(GPXADDR + GPX_ADDER_OFFSET), 1);
		adder = (struct adder *)tmp;
		adder->status = 0;
		status = adder->status;
		iounaccess(tmp, 1);
		if (status == offsetof(struct adder, status))
			return (0);

		/* Check for a recognized color depth */
		tmp = virtual_avail;
		ioaccess(tmp, vax_trunc_page(GPXADDR + GPX_READBACK_OFFSET), 1);
		depth = *(u_int16_t *)
		    (tmp + (GPX_READBACK_OFFSET & VAX_PGOFSET)) & 0x00f0;
		iounaccess(tmp, 1);
		if (depth == 0x00f0 || depth == 0x0080)
			return (1);

		break;

	default:
		break;
	}

	return (0);
}

/*
 * Called very early to setup the glass tty as console.
 * Because it's called before the VM system is initialized, virtual memory
 * for the framebuffer can be stolen directly without disturbing anything.
 */
int
gpxcninit()
{
	struct gpx_screen *ss = &gpx_consscr;
	extern vaddr_t virtual_avail;
	vaddr_t ova;
	long defattr;
	struct rasops_info *ri;

	ova = virtual_avail;

	ioaccess(virtual_avail,
	    vax_trunc_page(GPXADDR + GPX_READBACK_OFFSET), 1);
	ss->ss_depth = (0x00f0 & *(u_int16_t *)(virtual_avail +
	    (GPX_READBACK_OFFSET & VAX_PGOFSET))) == 0x00f0 ? 4 : 8;

	ioaccess(virtual_avail, GPXADDR + GPX_ADDER_OFFSET, 1);
	ss->ss_adder = (struct adder *)virtual_avail;
	virtual_avail += VAX_NBPG;

	ioaccess(virtual_avail, vax_trunc_page(GPXADDR + GPX_VDAC_OFFSET), 1);
	ss->ss_vdac = (void *)(virtual_avail + (GPX_VDAC_OFFSET & VAX_PGOFSET));
	virtual_avail += VAX_NBPG;

#if 0
	ioaccess(virtual_avail, GPXADDR + GPX_CURSOR_OFFSET, 1);
	ss->ss_cursor = (struct dc503reg *)virtual_avail;
	virtual_avail += VAX_NBPG;
#endif

	virtual_avail = round_page(virtual_avail);

	/* this had better not fail */
	if (gpx_setup_screen(ss) != 0) {
#if 0
		iounaccess((vaddr_t)ss->ss_cursor, 1);
#endif
		iounaccess((vaddr_t)ss->ss_vdac, 1);
		iounaccess((vaddr_t)ss->ss_adder, 1);
		virtual_avail = ova;
		return (1);
	}

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&gpx_stdscreen, ri, 0, 0, defattr);

	return (0);
}

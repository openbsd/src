/*	$OpenBSD: creator.c,v 1.30 2004/11/29 22:07:40 miod Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc64/dev/creatorreg.h>
#include <sparc64/dev/creatorvar.h>

int	creator_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	creator_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	creator_free_screen(void *, void *);
int	creator_show_screen(void *, void *, int, void (*cb)(void *, int, int),
	    void *);
paddr_t creator_mmap(void *, off_t, int);
void	creator_ras_fifo_wait(struct creator_softc *, int);
void	creator_ras_wait(struct creator_softc *);
void	creator_ras_init(struct creator_softc *);
void	creator_ras_copyrows(void *, int, int, int);
void	creator_ras_erasecols(void *, int, int, int, long int);
void	creator_ras_eraserows(void *, int, int, long int);
void	creator_ras_updatecursor(struct rasops_info *);
void	creator_ras_fill(struct creator_softc *);
void	creator_ras_setfg(struct creator_softc *, int32_t);
int	creator_setcursor(struct creator_softc *, struct wsdisplay_cursor *);
int	creator_updatecursor(struct creator_softc *, u_int);
void	creator_curs_enable(struct creator_softc *, u_int);

struct wsdisplay_accessops creator_accessops = {
	creator_ioctl,
	creator_mmap,
	creator_alloc_screen,
	creator_free_screen,
	creator_show_screen,
	NULL,	/* load font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL,	/* burner */
};

struct cfdriver creator_cd = {
	NULL, "creator", DV_DULL
};

void
creator_attach(struct creator_softc *sc)
{
	char *model;
	int btype;

	printf(":");

	/*
	 * Prom reports only the length of the fcode header, we need
	 * the whole thing.
	 */
	sc->sc_sizes[0] = 0x00400000;

	if (sc->sc_type == FFB_CREATOR) {
		btype = getpropint(sc->sc_node, "board_type", 0);
		if ((btype & 7) == 3)
			printf(" Creator3D");
		else
			printf(" Creator");
	} else
		printf(" Elite3D");

	model = getpropstring(sc->sc_node, "model");
	if (model == NULL || strlen(model) == 0)
		model = "unknown";

	DAC_WRITE(sc, FFB_DAC_TYPE, DAC_TYPE_GETREV);
	sc->sc_dacrev = DAC_READ(sc, FFB_DAC_VALUE) >> 28;

	printf(", model %s, dac %u\n", model, sc->sc_dacrev);

	if (sc->sc_type == FFB_AFB)
		sc->sc_dacrev = 10;

	fb_setsize(&sc->sc_sunfb, 32, 1152, 900, sc->sc_node, 0);
	/* linesize has a fixed value, compensate */
	sc->sc_sunfb.sf_linebytes = 8192;
	sc->sc_sunfb.sf_fbsize = sc->sc_sunfb.sf_height * 8192;

	sc->sc_sunfb.sf_ro.ri_bits = (void *)bus_space_vaddr(sc->sc_bt,
	    sc->sc_pixel_h);
	sc->sc_sunfb.sf_ro.ri_hw = sc;
	fbwscons_init(&sc->sc_sunfb, sc->sc_console ? 0 : RI_CLEAR);

	if ((sc->sc_sunfb.sf_dev.dv_cfdata->cf_flags & CREATOR_CFFLAG_NOACCEL)
	    == 0) {
		sc->sc_sunfb.sf_ro.ri_ops.eraserows = creator_ras_eraserows;
		sc->sc_sunfb.sf_ro.ri_ops.erasecols = creator_ras_erasecols;
		sc->sc_sunfb.sf_ro.ri_ops.copyrows = creator_ras_copyrows;
		creator_ras_init(sc);
	}

	if (sc->sc_console) {
		sc->sc_sunfb.sf_ro.ri_updatecursor = creator_ras_updatecursor;
		fbwscons_console_init(&sc->sc_sunfb, -1,
		    NULL);
	}

	fbwscons_attach(&sc->sc_sunfb, &creator_accessops, sc->sc_console);
}

int
creator_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct creator_softc *sc = v;
	struct wsdisplay_cursor *curs;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_curpos *pos;
	u_char r[2], g[2], b[2];
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SUNFFB;
		break;
	case WSDISPLAYIO_SMODE:
		sc->sc_mode = *(u_int *)data;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width  = sc->sc_sunfb.sf_width;
		wdf->depth  = 32;
		wdf->cmsize = 0;
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;
	case WSDISPLAYIO_GCURSOR:
		curs = (struct wsdisplay_cursor *)data;
		if (curs->which & WSDISPLAY_CURSOR_DOCUR)
			curs->enable = sc->sc_curs_enabled;
		if (curs->which & WSDISPLAY_CURSOR_DOPOS) {
			curs->pos.x = sc->sc_curs_pos.x;
			curs->pos.y = sc->sc_curs_pos.y;
		}
		if (curs->which & WSDISPLAY_CURSOR_DOHOT) {
			curs->hot.x = sc->sc_curs_hot.x;
			curs->hot.y = sc->sc_curs_hot.y;
		}
		if (curs->which & WSDISPLAY_CURSOR_DOCMAP) {
			curs->cmap.index = 0;
			curs->cmap.count = 2;
			r[0] = sc->sc_curs_fg >> 0;
			g[0] = sc->sc_curs_fg >> 8;
			b[0] = sc->sc_curs_fg >> 16;
			r[1] = sc->sc_curs_bg >> 0;
			g[1] = sc->sc_curs_bg >> 8;
			b[1] = sc->sc_curs_bg >> 16;
			error = copyout(r, curs->cmap.red, sizeof(r));
			if (error)
				return (error);
			error = copyout(g, curs->cmap.green, sizeof(g));
			if (error)
				return (error);
			error = copyout(b, curs->cmap.blue, sizeof(b));
			if (error)
				return (error);
		}
		if (curs->which & WSDISPLAY_CURSOR_DOSHAPE) {
			size_t l;

			curs->size.x = sc->sc_curs_size.x;
			curs->size.y = sc->sc_curs_size.y;
			l = (sc->sc_curs_size.x * sc->sc_curs_size.y) / NBBY;
			error = copyout(sc->sc_curs_image, curs->image, l);
			if (error)
				return (error);
			error = copyout(sc->sc_curs_mask, curs->mask, l);
			if (error)
				return (error);
		}
		break;
	case WSDISPLAYIO_SCURPOS:
		pos = (struct wsdisplay_curpos *)data;
		sc->sc_curs_pos.x = pos->x;
		sc->sc_curs_pos.y = pos->y;
		creator_updatecursor(sc, WSDISPLAY_CURSOR_DOPOS);
		break;
	case WSDISPLAYIO_GCURPOS:
		pos = (struct wsdisplay_curpos *)data;
		pos->x = sc->sc_curs_pos.x;
		pos->y = sc->sc_curs_pos.y;
		break;
	case WSDISPLAYIO_SCURSOR:
		curs = (struct wsdisplay_cursor *)data;
		return (creator_setcursor(sc, curs));
	case WSDISPLAYIO_GCURMAX:
		pos = (struct wsdisplay_curpos *)data;
		pos->x = CREATOR_CURS_MAX;
		pos->y = CREATOR_CURS_MAX;
		break;
	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	default:
		return -1; /* not supported yet */
        }

	return (0);
}

int
creator_setcursor(struct creator_softc *sc, struct wsdisplay_cursor *curs)
{
	u_int8_t r[2], g[2], b[2], image[128], mask[128];
	int error;
	size_t imcount;

	/*
	 * Do stuff that can generate errors first, then we'll blast it
	 * all at once.
	 */
	if (curs->which & WSDISPLAY_CURSOR_DOCMAP) {
		if (curs->cmap.count < 2)
			return (EINVAL);
		error = copyin(curs->cmap.red, r, sizeof(r));
		if (error)
			return (error);
		error = copyin(curs->cmap.green, g, sizeof(g));
		if (error)
			return (error);
		error = copyin(curs->cmap.blue, b, sizeof(b));
		if (error)
			return (error);
	}

	if (curs->which & WSDISPLAY_CURSOR_DOSHAPE) {
		if (curs->size.x > CREATOR_CURS_MAX ||
		    curs->size.y > CREATOR_CURS_MAX)
			return (EINVAL);
		imcount = (curs->size.x * curs->size.y) / NBBY;
		error = copyin(curs->image, image, imcount);
		if (error)
			return (error);
		error = copyin(curs->mask, mask, imcount);
		if (error)
			return (error);
	}

	/*
	 * Ok, everything is in kernel space and sane, update state.
	 */

	if (curs->which & WSDISPLAY_CURSOR_DOCUR)
		sc->sc_curs_enabled = curs->enable;
	if (curs->which & WSDISPLAY_CURSOR_DOPOS) {
		sc->sc_curs_pos.x = curs->pos.x;
		sc->sc_curs_pos.y = curs->pos.y;
	}
	if (curs->which & WSDISPLAY_CURSOR_DOHOT) {
		sc->sc_curs_hot.x = curs->hot.x;
		sc->sc_curs_hot.y = curs->hot.y;
	}
	if (curs->which & WSDISPLAY_CURSOR_DOCMAP) {
		sc->sc_curs_fg = ((r[0] << 0) | (g[0] << 8) | (b[0] << 16));
		sc->sc_curs_bg = ((r[1] << 0) | (g[1] << 8) | (b[1] << 16));
	}
	if (curs->which & WSDISPLAY_CURSOR_DOSHAPE) {
		sc->sc_curs_size.x = curs->size.x;
		sc->sc_curs_size.y = curs->size.y;
		bcopy(image, sc->sc_curs_image, imcount);
		bcopy(mask, sc->sc_curs_mask, imcount);
	}

	creator_updatecursor(sc, curs->which);

	return (0);
}

void
creator_curs_enable(struct creator_softc *sc, u_int ena)
{
	u_int32_t v;

	DAC_WRITE(sc, FFB_DAC_TYPE2, DAC_TYPE2_CURSENAB);
	if (sc->sc_dacrev <= 2)
		v = ena ? 3 : 0;
	else
		v = ena ? 0 : 3;
	DAC_WRITE(sc, FFB_DAC_VALUE2, v);
}

int
creator_updatecursor(struct creator_softc *sc, u_int which)
{
	creator_curs_enable(sc, 0);

	if (which & WSDISPLAY_CURSOR_DOCMAP) {
		DAC_WRITE(sc, FFB_DAC_TYPE2, DAC_TYPE2_CURSCMAP);
		DAC_WRITE(sc, FFB_DAC_VALUE2, sc->sc_curs_fg);
		DAC_WRITE(sc, FFB_DAC_VALUE2, sc->sc_curs_bg);
	}

	if (which & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		u_int32_t x, y;

		x = sc->sc_curs_pos.x + CREATOR_CURS_MAX - sc->sc_curs_hot.x;
		y = sc->sc_curs_pos.y + CREATOR_CURS_MAX - sc->sc_curs_hot.y;
		DAC_WRITE(sc, FFB_DAC_TYPE2, DAC_TYPE2_CURSPOS);
		DAC_WRITE(sc, FFB_DAC_VALUE2,
		    ((x & 0xffff) << 16) | (y & 0xffff));
	}

	if (which & WSDISPLAY_CURSOR_DOCUR)
		creator_curs_enable(sc, sc->sc_curs_enabled);

	return (0);
}

int
creator_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct creator_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
	    0, 0, 0, attrp);
	sc->sc_nscreens++;
	return (0);
}

void
creator_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct creator_softc *sc = v;

	sc->sc_nscreens--;
}

int
creator_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

const struct creator_mappings {
	bus_addr_t uoff;
	bus_addr_t poff;
	bus_size_t ulen;
} creator_map[] = {
	{ FFB_VOFF_SFB8R, FFB_POFF_SFB8R, FFB_VLEN_SFB8R },
	{ FFB_VOFF_SFB8G, FFB_POFF_SFB8G, FFB_VLEN_SFB8G },
	{ FFB_VOFF_SFB8B, FFB_POFF_SFB8B, FFB_VLEN_SFB8B },
	{ FFB_VOFF_SFB8X, FFB_POFF_SFB8X, FFB_VLEN_SFB8X },
	{ FFB_VOFF_SFB32, FFB_POFF_SFB32, FFB_VLEN_SFB32 },
	{ FFB_VOFF_SFB64, FFB_POFF_SFB64, FFB_VLEN_SFB64 },
	{ FFB_VOFF_FBC_REGS, FFB_POFF_FBC_REGS, FFB_VLEN_FBC_REGS },
	{ FFB_VOFF_BM_FBC_REGS, FFB_POFF_BM_FBC_REGS, FFB_VLEN_BM_FBC_REGS },
	{ FFB_VOFF_DFB8R, FFB_POFF_DFB8R, FFB_VLEN_DFB8R },
	{ FFB_VOFF_DFB8G, FFB_POFF_DFB8G, FFB_VLEN_DFB8G },
	{ FFB_VOFF_DFB8B, FFB_POFF_DFB8B, FFB_VLEN_DFB8B },
	{ FFB_VOFF_DFB8X, FFB_POFF_DFB8X, FFB_VLEN_DFB8X },
	{ FFB_VOFF_DFB24, FFB_POFF_DFB24, FFB_VLEN_DFB24 },
	{ FFB_VOFF_DFB32, FFB_POFF_DFB32, FFB_VLEN_DFB32 },
	{ FFB_VOFF_DFB422A, FFB_POFF_DFB422A, FFB_VLEN_DFB422A },
	{ FFB_VOFF_DFB422AD, FFB_POFF_DFB422AD, FFB_VLEN_DFB422AD },
	{ FFB_VOFF_DFB24B, FFB_POFF_DFB24B, FFB_VLEN_DFB24B },
	{ FFB_VOFF_DFB422B, FFB_POFF_DFB422B, FFB_VLEN_DFB422B },
	{ FFB_VOFF_DFB422BD, FFB_POFF_DFB422BD, FFB_VLEN_DFB422BD },
	{ FFB_VOFF_SFB16Z, FFB_POFF_SFB16Z, FFB_VLEN_SFB16Z },
	{ FFB_VOFF_SFB8Z, FFB_POFF_SFB8Z, FFB_VLEN_SFB8Z },
	{ FFB_VOFF_SFB422, FFB_POFF_SFB422, FFB_VLEN_SFB422 },
	{ FFB_VOFF_SFB422D, FFB_POFF_SFB422D, FFB_VLEN_SFB422D },
	{ FFB_VOFF_FBC_KREGS, FFB_POFF_FBC_KREGS, FFB_VLEN_FBC_KREGS },
	{ FFB_VOFF_DAC, FFB_POFF_DAC, FFB_VLEN_DAC },
	{ FFB_VOFF_PROM, FFB_POFF_PROM, FFB_VLEN_PROM },
	{ FFB_VOFF_EXP, FFB_POFF_EXP, FFB_VLEN_EXP },
};
#define	CREATOR_NMAPPINGS	(sizeof(creator_map)/sizeof(creator_map[0]))

paddr_t
creator_mmap(vsc, off, prot)
	void *vsc;
	off_t off;
	int prot;
{
	paddr_t x;
	struct creator_softc *sc = vsc;
	int i;

	switch (sc->sc_mode) {
	case WSDISPLAYIO_MODE_MAPPED:
		/* Turn virtual offset into physical offset */
		for (i = 0; i < CREATOR_NMAPPINGS; i++) {
			if (off >= creator_map[i].uoff &&
			    off < (creator_map[i].uoff + creator_map[i].ulen))
				break;
		}
		if (i == CREATOR_NMAPPINGS)
			break;

		off -= creator_map[i].uoff;
		off += creator_map[i].poff;
		off += sc->sc_addrs[0];

		/* Map based on physical offset */
		for (i = 0; i < sc->sc_nreg; i++) {
			/* Before this set? */
			if (off < sc->sc_addrs[i])
				continue;
			/* After this set? */
			if (off >= (sc->sc_addrs[i] + sc->sc_sizes[i]))
				continue;

			x = bus_space_mmap(sc->sc_bt, 0, off, prot,
			    BUS_SPACE_MAP_LINEAR);
			return (x);
		}
		break;
	case WSDISPLAYIO_MODE_DUMBFB:
		if (sc->sc_nreg < FFB_REG_DFB24)
			break;
		if (off >= 0 && off < sc->sc_sizes[FFB_REG_DFB24])
			return (bus_space_mmap(sc->sc_bt,
			    sc->sc_addrs[FFB_REG_DFB24], off, prot,
			    BUS_SPACE_MAP_LINEAR));
		break;
	}

	return (-1);
}

void
creator_ras_fifo_wait(sc, n)
	struct creator_softc *sc;
	int n;
{
	int32_t cache = sc->sc_fifo_cache;

	if (cache < n) {
		do {
			cache = FBC_READ(sc, FFB_FBC_UCSR);
			cache = (cache & FBC_UCSR_FIFO_MASK) - 8;
		} while (cache < n);
	}
	sc->sc_fifo_cache = cache - n;
}

void
creator_ras_wait(sc)
	struct creator_softc *sc;
{
	u_int32_t ucsr, r;

	while (1) {
		ucsr = FBC_READ(sc, FFB_FBC_UCSR);
		if ((ucsr & (FBC_UCSR_FB_BUSY|FBC_UCSR_RP_BUSY)) == 0)
			break;
		r = ucsr & (FBC_UCSR_READ_ERR | FBC_UCSR_FIFO_OVFL);
		if (r != 0)
			FBC_WRITE(sc, FFB_FBC_UCSR, r);
	}
}

void
creator_ras_init(sc)
	struct creator_softc *sc;
{
	creator_ras_fifo_wait(sc, 7);
	FBC_WRITE(sc, FFB_FBC_PPC,
	    FBC_PPC_VCE_DIS | FBC_PPC_TBE_OPAQUE |
	    FBC_PPC_APE_DIS | FBC_PPC_CS_CONST);
	FBC_WRITE(sc, FFB_FBC_FBC,
	    FFB_FBC_WB_A | FFB_FBC_RB_A | FFB_FBC_SB_BOTH |
	    FFB_FBC_XE_OFF | FFB_FBC_RGBE_MASK);
	FBC_WRITE(sc, FFB_FBC_ROP, FBC_ROP_NEW);
	FBC_WRITE(sc, FFB_FBC_DRAWOP, FBC_DRAWOP_RECTANGLE);
	FBC_WRITE(sc, FFB_FBC_PMASK, 0xffffffff);
	FBC_WRITE(sc, FFB_FBC_FONTINC, 0x10000);
	sc->sc_fg_cache = 0;
	FBC_WRITE(sc, FFB_FBC_FG, sc->sc_fg_cache);
	creator_ras_wait(sc);
}

void
creator_ras_eraserows(cookie, row, n, attr)
	void *cookie;
	int row, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct creator_softc *sc = ri->ri_hw;

	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row + n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return;

	creator_ras_fill(sc);
	creator_ras_setfg(sc, ri->ri_devcmap[(attr >> 16) & 0xf]);
	creator_ras_fifo_wait(sc, 4);
	if ((n == ri->ri_rows) && (ri->ri_flg & RI_FULLCLEAR)) {
		FBC_WRITE(sc, FFB_FBC_BY, 0);
		FBC_WRITE(sc, FFB_FBC_BX, 0);
		FBC_WRITE(sc, FFB_FBC_BH, ri->ri_height);
		FBC_WRITE(sc, FFB_FBC_BW, ri->ri_width);
	} else {
		row *= ri->ri_font->fontheight;
		FBC_WRITE(sc, FFB_FBC_BY, ri->ri_yorigin + row);
		FBC_WRITE(sc, FFB_FBC_BX, ri->ri_xorigin);
		FBC_WRITE(sc, FFB_FBC_BH, n * ri->ri_font->fontheight);
		FBC_WRITE(sc, FFB_FBC_BW, ri->ri_emuwidth);
	}
	creator_ras_wait(sc);
}

void
creator_ras_erasecols(cookie, row, col, n, attr)
	void *cookie;
	int row, col, n;
	long int attr;
{
	struct rasops_info *ri = cookie;
	struct creator_softc *sc = ri->ri_hw;

	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col + n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	creator_ras_fill(sc);
	creator_ras_setfg(sc, ri->ri_devcmap[(attr >> 16) & 0xf]);
	creator_ras_fifo_wait(sc, 4);
	FBC_WRITE(sc, FFB_FBC_BY, ri->ri_yorigin + row);
	FBC_WRITE(sc, FFB_FBC_BX, ri->ri_xorigin + col);
	FBC_WRITE(sc, FFB_FBC_BH, ri->ri_font->fontheight);
	FBC_WRITE(sc, FFB_FBC_BW, n - 1);
	creator_ras_wait(sc);
}

void
creator_ras_fill(sc)
	struct creator_softc *sc;
{
	creator_ras_fifo_wait(sc, 2);
	FBC_WRITE(sc, FFB_FBC_ROP, FBC_ROP_NEW);
	FBC_WRITE(sc, FFB_FBC_DRAWOP, FBC_DRAWOP_RECTANGLE);
	creator_ras_wait(sc);
}

void
creator_ras_copyrows(cookie, src, dst, n)
	void *cookie;
	int src, dst, n;
{
	struct rasops_info *ri = cookie;
	struct creator_softc *sc = ri->ri_hw;

	if (dst == src)
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if ((src + n) > ri->ri_rows)
		n = ri->ri_rows - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if ((dst + n) > ri->ri_rows)
		n = ri->ri_rows - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;

	creator_ras_fifo_wait(sc, 8);
	FBC_WRITE(sc, FFB_FBC_ROP, FBC_ROP_OLD | (FBC_ROP_OLD << 8));
	FBC_WRITE(sc, FFB_FBC_DRAWOP, FBC_DRAWOP_VSCROLL);
	FBC_WRITE(sc, FFB_FBC_BY, ri->ri_yorigin + src);
	FBC_WRITE(sc, FFB_FBC_BX, ri->ri_xorigin);
	FBC_WRITE(sc, FFB_FBC_DY, ri->ri_yorigin + dst);
	FBC_WRITE(sc, FFB_FBC_DX, ri->ri_xorigin);
	FBC_WRITE(sc, FFB_FBC_BH, n);
	FBC_WRITE(sc, FFB_FBC_BW, ri->ri_emuwidth);
	creator_ras_wait(sc);
}

void
creator_ras_setfg(sc, fg)
	struct creator_softc *sc;
	int32_t fg;
{
	creator_ras_fifo_wait(sc, 1);
	if (fg == sc->sc_fg_cache)
		return;
	sc->sc_fg_cache = fg;
	FBC_WRITE(sc, FFB_FBC_FG, fg);
	creator_ras_wait(sc);
}

void
creator_ras_updatecursor(ri)
	struct rasops_info *ri;
{
	struct creator_softc *sc = ri->ri_hw;

	if (sc->sc_sunfb.sf_crowp != NULL)
		*sc->sc_sunfb.sf_crowp = ri->ri_crow;
	if (sc->sc_sunfb.sf_ccolp != NULL)
		*sc->sc_sunfb.sf_ccolp = ri->ri_ccol;
}

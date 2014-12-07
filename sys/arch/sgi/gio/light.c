/*	$OpenBSD: light.c,v 1.7 2014/12/07 17:15:56 miod Exp $	*/
/*	$NetBSD: light.c,v 1.5 2007/03/04 06:00:39 christos Exp $	*/

/*
 * Copyright (c) 2012, 2014 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2006 Stephen M. Rumble
 * Copyright (c) 2003 Ilpo Ruotsalainen
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

/*
 * SGI "Light" graphics, a.k.a. "Entry", "Starter", "LG1", and "LG2".
 *
 * 1024x768 8bpp at 60Hz.
 *
 * This driver supports the boards found in Indigo R3k and R4k machines.
 * There is a Crimson variant, but the register offsets differ significantly.
 *
 * Light's REX chip is the precursor of the REX3 found in "newport", hence
 * much similarity exists.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <sgi/dev/gl.h>
#include <sgi/gio/giovar.h>
#include <sgi/gio/lightvar.h>
#include <sgi/gio/lightreg.h>

#include <dev/cons.h>

struct light_softc {
	struct device		sc_dev;

	struct light_devconfig *sc_dc;

	int			sc_nscreens;
	struct wsscreen_list	sc_wsl;
	const struct wsscreen_descr *sc_scrlist[1];
};

struct light_devconfig {
	struct rasops_info	dc_ri;
	long			dc_defattr;

	uint32_t		dc_addr;
	bus_space_tag_t		dc_st;
	bus_space_handle_t	dc_sh;

	int			dc_boardrev;

	struct light_softc	*dc_sc;
	struct wsscreen_descr	dc_wsd;

	uint8_t			dc_cmap[256 * 3];
};

/* always 1024x768x8 */
#define LIGHT_XRES	1024
#define LIGHT_YRES	768
#define LIGHT_DEPTH	8

int	light_match(struct device *, void *, void *);
void	light_attach(struct device *, struct device *, void *);

struct cfdriver light_cd = {
	NULL, "light", DV_DULL
};

const struct cfattach light_ca = {
	sizeof(struct light_softc), light_match, light_attach
};

/* wsdisplay_accessops */
int	light_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	light_mmap(void *, off_t, int);
int	light_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	light_free_screen(void *, void *);
int	light_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
int	light_load_font(void *, void *, struct wsdisplay_font *);
int	light_list_font(void *, struct wsdisplay_font *);
void	light_burner(void *, u_int, u_int);

struct wsdisplay_accessops light_accessops = {
	.ioctl = light_ioctl,
	.mmap = light_mmap,
	.alloc_screen = light_alloc_screen,
	.free_screen = light_free_screen,
	.show_screen = light_show_screen,
	.load_font = light_load_font,
	.list_font = light_list_font,
	.burn_screen = light_burner
};

int	light_do_cursor(struct rasops_info *);
int	light_putchar(void *, int, int, u_int, long);
int	light_copycols(void *, int, int, int, int);
int	light_erasecols(void *, int, int, int, long);
int	light_copyrows(void *, int, int, int);
int	light_eraserows(void *, int, int, long);

static __inline__
uint32_t rex_read(struct light_devconfig *, uint32_t, uint32_t);
static __inline__
void	 rex_write(struct light_devconfig *, uint32_t, uint32_t, uint32_t);
uint8_t	 rex_vc1_sysctl_read(struct light_devconfig *);
void	 rex_vc1_sysctl_write(struct light_devconfig *, uint8_t);
static __inline__
void	 rex_wait(struct light_devconfig *);
static __inline__
int	 rex_revision(struct light_devconfig *);
void	 rex_copy_rect(struct light_devconfig *, int, int, int, int, int, int,
	    int);
void	 rex_fill_rect(struct light_devconfig *, int, int, int, int, int);

int	light_getcmap(struct light_devconfig *, struct wsdisplay_cmap *);
void	light_loadcmap(struct light_devconfig *, int, int);
int	light_putcmap(struct light_devconfig *, struct wsdisplay_cmap *);

void	light_attach_common(struct light_devconfig *, struct gio_attach_args *);
void	light_init_screen(struct light_devconfig *);

static struct light_devconfig light_console_dc;

#define LIGHT_IS_LG1(_rev)		((_rev) < 2)	/* else LG2 */

/*
 * REX routines and helper functions.
 */

static __inline__
uint32_t
rex_read(struct light_devconfig *dc, uint32_t rset, uint32_t r)
{
	return bus_space_read_4(dc->dc_st, dc->dc_sh, rset + r);
}

static __inline__
void
rex_write(struct light_devconfig *dc, uint32_t rset, uint32_t r, uint32_t v)
{
	bus_space_write_4(dc->dc_st, dc->dc_sh, rset + r, v);
}

uint8_t
rex_vc1_sysctl_read(struct light_devconfig *dc)
{
	rex_write(dc, REX_PAGE1_GO, REX_P1REG_CFGSEL, REX_CFGSEL_VC1_SYSCTL);

	rex_read(dc, REX_PAGE1_GO, REX_P1REG_VC1_ADDRDATA);
	return rex_read(dc, REX_PAGE1_SET, REX_P1REG_VC1_ADDRDATA);
}

void
rex_vc1_sysctl_write(struct light_devconfig *dc, uint8_t val)
{
	rex_write(dc, REX_PAGE1_GO, REX_P1REG_CFGSEL, REX_CFGSEL_VC1_SYSCTL);

	rex_write(dc, REX_PAGE1_SET, REX_P1REG_VC1_ADDRDATA, val);
	rex_write(dc, REX_PAGE1_GO, REX_P1REG_VC1_ADDRDATA, val);
}

static __inline__
void
rex_wait(struct light_devconfig *dc)
{
	while (rex_read(dc, REX_PAGE1_SET,REX_P1REG_CFGMODE) & REX_CFGMODE_BUSY)
		continue;
}

static __inline__
int
rex_revision(struct light_devconfig *dc)
{
	rex_write(dc, REX_PAGE1_SET, REX_P1REG_CFGSEL, REX_CFGSEL_VC1_LADDR);
	rex_read(dc, REX_PAGE1_GO, REX_P1REG_WCLOCKREV);
	return rex_read(dc, REX_PAGE1_SET, REX_P1REG_WCLOCKREV) & 0x7;
}

void
rex_copy_rect(struct light_devconfig *dc, int from_x, int from_y, int to_x,
    int to_y, int width, int height, int rop)
{
	int dx, dy, ystarti, yendi;

	dx = from_x - to_x;
	dy = from_y - to_y;

	if (to_y > from_y) {
		ystarti = to_y + height - 1;
		yendi = to_y;
	} else {
		ystarti = to_y;
		yendi = to_y + height - 1;
	}

	rex_wait(dc);

	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XSTARTI, to_x);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XENDI, to_x + width - 1);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YSTARTI, ystarti);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YENDI, yendi);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COMMAND, REX_OP_DRAW |
	    REX_OP_FLG_LOGICSRC | REX_OP_FLG_QUADMODE |
	    REX_OP_FLG_BLOCK | REX_OP_FLG_STOPONX | REX_OP_FLG_STOPONY |
	    (rop << REX_LOGICOP_SHIFT));
	rex_write(dc, REX_PAGE0_GO, REX_P0REG_XYMOVE,
	    ((dx << 16) & 0xffff0000) | (dy & 0x0000ffff));
}

void
rex_fill_rect(struct light_devconfig *dc, int from_x, int from_y, int to_x,
    int to_y, int bg)
{
	struct rasops_info *ri = &dc->dc_ri;

	rex_wait(dc);

	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YSTARTI, from_y);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YENDI, to_y - 1);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XSTARTI, from_x);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XENDI, to_x - 1);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COLORREDI,
	    ri->ri_devcmap[bg] & 0xff);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COMMAND, REX_OP_DRAW |
	    REX_OP_FLG_QUADMODE | REX_OP_FLG_BLOCK |
	    REX_OP_FLG_STOPONX | REX_OP_FLG_STOPONY |
	    (OPENGL_LOGIC_OP_COPY << REX_LOGICOP_SHIFT));
	rex_read(dc, REX_PAGE0_GO, REX_P0REG_COMMAND);
}

/*
 * Colormap routines
 */

int
light_getcmap(struct light_devconfig *dc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count, i;
	u_int colcount = 1 << dc->dc_ri.ri_depth;
	int rc;
	u_int8_t color[256], *c, *r;

	if (index >= colcount || count > colcount - index)
		return EINVAL;

	c = dc->dc_cmap + 0 + index * 3;
	for (i = count, r = color; i != 0; i--) {
		*r++ = *c;
		c += 3;
	}
	if ((rc = copyout(color, cm->red, count)) != 0)
		return rc;

	c = dc->dc_cmap + 1 + index * 3;
	for (i = count, r = color; i != 0; i--) {
		*r++ = *c;
		c += 3;
	}
	if ((rc = copyout(color, cm->green, count)) != 0)
		return rc;

	c = dc->dc_cmap + 2 + index * 3;
	for (i = count, r = color; i != 0; i--) {
		*r++ = *c;
		c += 3;
	}
	if ((rc = copyout(color, cm->blue, count)) != 0)
		return rc;

	return 0;
}

int
light_putcmap(struct light_devconfig *dc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << dc->dc_ri.ri_depth;
	int i, rc;
	u_int8_t r[256], g[256], b[256], *nr, *ng, *nb, *c;

	if (index >= colcount || count > colcount - index)
		return EINVAL;

	if ((rc = copyin(cm->red, r, count)) != 0)
		return rc;
	if ((rc = copyin(cm->green, g, count)) != 0)
		return rc;
	if ((rc = copyin(cm->blue, b, count)) != 0)
		return rc;

	nr = r, ng = g, nb = b;
	c = dc->dc_cmap + index * 3;
	for (i = count; i != 0; i--) {
		*c++ = *nr++;
		*c++ = *ng++;
		*c++ = *nb++;
	}

	return 0;
}

void
light_loadcmap(struct light_devconfig *dc, int from, int count)
{
	u_int8_t *cmap = dc->dc_cmap;

	/* XXX should wait for retrace first */

	cmap += 3 * from;
	rex_write(dc, REX_PAGE1_GO, REX_P1REG_CFGSEL, REX_CFGSEL_DAC_WADDR);
	rex_write(dc, REX_PAGE1_SET, REX_P1REG_DAC_ADDRDATA, from);
	rex_write(dc, REX_PAGE1_GO, REX_P1REG_DAC_ADDRDATA, from);

	rex_write(dc, REX_PAGE1_GO, REX_P1REG_CFGSEL, REX_CFGSEL_DAC_CMAP);
	while (count-- > 0) {
		rex_write(dc, REX_PAGE1_GO, REX_P1REG_DAC_ADDRDATA, *cmap);
		rex_write(dc, REX_PAGE1_SET, REX_P1REG_DAC_ADDRDATA, *cmap++);
		rex_write(dc, REX_PAGE1_GO, REX_P1REG_DAC_ADDRDATA, *cmap);
		rex_write(dc, REX_PAGE1_SET, REX_P1REG_DAC_ADDRDATA, *cmap++);
		rex_write(dc, REX_PAGE1_GO, REX_P1REG_DAC_ADDRDATA, *cmap);
		rex_write(dc, REX_PAGE1_SET, REX_P1REG_DAC_ADDRDATA, *cmap++);
	}
}

/*
 * Autoconf and console glue
 */

int
light_match(struct device *parent, void *vcf, void *aux)
{
	struct gio_attach_args *ga = aux;

	if (ga->ga_product != GIO_PRODUCT_FAKEID_LIGHT)
		return 0;

	return 1;
}

void
light_attach(struct device *parent, struct device *self, void *aux)
{
	struct light_softc *sc = (struct light_softc *)self;
	struct gio_attach_args *ga = aux;
	struct light_devconfig *dc;
	struct wsemuldisplaydev_attach_args waa;
	extern struct consdev wsdisplay_cons;

	if (cn_tab == &wsdisplay_cons &&
	    ga->ga_addr == light_console_dc.dc_addr) {
		waa.console = 1;
		dc = &light_console_dc;
		sc->sc_nscreens = 1;
	} else {
		waa.console = 0;
		dc = malloc(sizeof(struct light_devconfig), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		light_attach_common(dc, ga);
		light_init_screen(dc);
	}
	sc->sc_dc = dc;
	dc->dc_sc = sc;

	if (ga->ga_descr != NULL && *ga->ga_descr != '\0')
		printf(": %s", ga->ga_descr);
	else
		printf(": LG%dMC\n",
		    LIGHT_IS_LG1(sc->sc_dc->dc_boardrev) ? 1 : 2);
	printf(", revision %d\n", dc->dc_boardrev);
	printf("%s: %dx%d %d-bit frame buffer\n", self->dv_xname,
	    dc->dc_ri.ri_width, dc->dc_ri.ri_height, dc->dc_ri.ri_depth);

	sc->sc_scrlist[0] = &dc->dc_wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = sc->sc_scrlist;

	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &light_accessops;
	waa.accesscookie = dc;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
light_cnprobe(struct gio_attach_args *ga)
{
	return light_match(NULL, NULL, ga);
}

int
light_cnattach(struct gio_attach_args *ga)
{
	struct rasops_info *ri = &light_console_dc.dc_ri;
	long defattr;

	light_attach_common(&light_console_dc, ga);
	light_init_screen(&light_console_dc);

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&light_console_dc.dc_wsd, ri, 0, 0, defattr);

	return 0;
}

void
light_attach_common(struct light_devconfig *dc, struct gio_attach_args *ga)
{
	dc->dc_addr = ga->ga_addr;
	dc->dc_st = ga->ga_iot;
	dc->dc_sh = ga->ga_ioh;

	dc->dc_boardrev = rex_revision(dc);

	rex_vc1_sysctl_write(dc, rex_vc1_sysctl_read(dc) &
	    ~(VC1_SYSCTL_CURSOR | VC1_SYSCTL_CURSOR_ON));
}

void
light_init_screen(struct light_devconfig *dc)
{
	struct rasops_info *ri = &dc->dc_ri;

	memset(ri, 0, sizeof(struct rasops_info));
	ri->ri_hw = dc;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	/* for the proper operation of rasops computations, pretend 8bpp */
	ri->ri_depth = 8;
	ri->ri_stride = LIGHT_XRES;
	ri->ri_width = LIGHT_XRES;
	ri->ri_height = LIGHT_YRES;

	rasops_init(ri, 160, 160);

	ri->ri_do_cursor = light_do_cursor;
	ri->ri_ops.copyrows = light_copyrows;
	ri->ri_ops.eraserows = light_eraserows;
	ri->ri_ops.copycols = light_copycols;
	ri->ri_ops.erasecols = light_erasecols;
	ri->ri_ops.putchar = light_putchar;

	strlcpy(dc->dc_wsd.name, "std", sizeof(dc->dc_wsd.name));
	dc->dc_wsd.ncols = ri->ri_cols;
	dc->dc_wsd.nrows = ri->ri_rows;
	dc->dc_wsd.textops = &ri->ri_ops;
	dc->dc_wsd.fontwidth = ri->ri_font->fontwidth;
	dc->dc_wsd.fontheight = ri->ri_font->fontheight;
	dc->dc_wsd.capabilities = ri->ri_caps;

	memcpy(dc->dc_cmap, rasops_cmap, sizeof(dc->dc_cmap));
	light_loadcmap(dc, 0, 1 << ri->ri_depth);

	rex_fill_rect(dc, 0, 0, ri->ri_width, ri->ri_height, WSCOL_BLACK);
}

/*
 * wsdisplay_emulops
 */

int
light_do_cursor(struct rasops_info *ri)
{
	struct light_devconfig *dc = ri->ri_hw;
	int x, y, w, h;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_ccol * w + ri->ri_xorigin;
	y = ri->ri_crow * h + ri->ri_yorigin;

	rex_copy_rect(dc, x, y, x, y, w, h, OPENGL_LOGIC_OP_COPY_INVERTED);

	return 0;
}

int
light_putchar(void *c, int row, int col, u_int ch, long attr)
{
	struct rasops_info *ri = c;
	struct light_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	uint8_t *bitmap;
	uint32_t pattern;
	int x = col * font->fontwidth + ri->ri_xorigin;
	int y = row * font->fontheight + ri->ri_yorigin;
	int i;
	int bg, fg, ul;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, &ul);

	if ((ch == ' ' || ch == 0) && ul == 0) {
		rex_fill_rect(dc, x, y, x + font->fontwidth,
		    y + font->fontheight, bg);
		return 0;
	}

	rex_wait(dc);

	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YSTARTI, y);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_YENDI, y + font->fontheight - 1);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XSTARTI, x);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_XENDI, x + font->fontwidth - 1);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COLORREDI,
	    ri->ri_devcmap[fg] & 0xff);
	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COLORBACK,
	    ri->ri_devcmap[bg] & 0xff);
	rex_write(dc, REX_PAGE0_GO, REX_P0REG_COMMAND, REX_OP_NOP);

	rex_wait(dc);

	rex_write(dc, REX_PAGE0_SET, REX_P0REG_COMMAND, REX_OP_DRAW |
	    REX_OP_FLG_ENZPATTERN | REX_OP_FLG_QUADMODE |
	    REX_OP_FLG_XYCONTINUE | REX_OP_FLG_STOPONX | REX_OP_FLG_BLOCK |
	    REX_OP_FLG_LENGTH32 | REX_OP_FLG_ZOPAQUE |
	    (OPENGL_LOGIC_OP_COPY << REX_LOGICOP_SHIFT));

	bitmap = (uint8_t *)font->data +
	    (ch - font->firstchar) * ri->ri_fontscale;
	if (font->fontwidth <= 8) {
		for (i = font->fontheight; i != 0; i--) {
			if (ul && i == 1)
				pattern = 0xff;
			else
				pattern = *bitmap;
			rex_write(dc, REX_PAGE0_GO, REX_P0REG_ZPATTERN,
			    pattern << 24);
			bitmap += font->stride;
		}
	} else {
		for (i = font->fontheight; i != 0; i--) {
			if (ul && i == 1)
				pattern = 0xffff;
			else
				pattern = *(uint16_t *)bitmap;
			rex_write(dc, REX_PAGE0_GO, REX_P0REG_ZPATTERN,
			    pattern << 16);
			bitmap += font->stride;
		}
	}

	return 0;
}

/* copy set of columns within the same line */
int
light_copycols(void *c, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = c;
	struct light_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int from_x, to_x, y, width, height;

	from_x = ri->ri_xorigin + srccol * font->fontwidth;
	to_x = ri->ri_xorigin + dstcol * font->fontwidth;
	y = ri->ri_yorigin + row * font->fontheight;
	width = ncols * font->fontwidth;
	height = font->fontheight;

	rex_copy_rect(dc, from_x, y, to_x, y, width, height,
	    OPENGL_LOGIC_OP_COPY);

	return 0;
}

/* erase a set of columns in the same line */
int
light_erasecols(void *c, int row, int startcol, int ncols, long attr)
{
	struct rasops_info *ri = c;
	struct light_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int from_x, from_y, to_x, to_y;
	int bg, fg;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	from_x = ri->ri_xorigin + startcol * font->fontwidth;
	from_y = ri->ri_yorigin + row * font->fontheight;
	to_x = from_x + ncols * font->fontwidth;
	to_y = from_y + font->fontheight;

	rex_fill_rect(dc, from_x, from_y, to_x, to_y, bg);

	return 0;
}

/* copy a set of complete rows */
int
light_copyrows(void *c, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = c;
	struct light_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, from_y, to_y, width, height;

	x = ri->ri_xorigin;
	from_y = ri->ri_yorigin + srcrow * font->fontheight;
	to_y = ri->ri_yorigin + dstrow * font->fontheight;
	width = ri->ri_emuwidth;
	height = nrows * font->fontheight;

	rex_copy_rect(dc, x, from_y, x, to_y, width, height,
	    OPENGL_LOGIC_OP_COPY);

	return 0;
}

/* erase a set of complete rows */
int
light_eraserows(void *c, int row, int nrows, long attr)
{
	struct rasops_info *ri = c;
	struct light_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int bg, fg;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	if (nrows == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		rex_fill_rect(dc, 0, 0, ri->ri_width, ri->ri_height, bg);
		return 0;
	}

	rex_fill_rect(dc, ri->ri_xorigin,
	    ri->ri_yorigin + row * font->fontheight,
	    ri->ri_xorigin + ri->ri_emuwidth,
	    ri->ri_yorigin + (row + nrows) * font->fontheight, bg);

	return 0;
}

/*
 * wsdisplay_accessops
 */

int
light_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct light_devconfig *dc = v;
	struct wsdisplay_fbinfo *fb;
	struct wsdisplay_cmap *cm;
	int rc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_LIGHT;
		break;
	case WSDISPLAYIO_GINFO:
		fb = (struct wsdisplay_fbinfo *)data;
		fb->width = dc->dc_ri.ri_width;
		fb->height = dc->dc_ri.ri_height;
		fb->depth = dc->dc_ri.ri_depth;
		fb->cmsize = 1 << fb->depth;
		break;
	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = light_getcmap(dc, cm);
		if (rc != 0)
			return rc;
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		rc = light_putcmap(dc, cm);
		if (rc != 0)
			return rc;
		light_loadcmap(dc, cm->index, cm->count);
		break;
	default:
		return -1;
	}

	return 0;
}

paddr_t
light_mmap(void *v, off_t off, int prot)
{
	return -1;
}

int
light_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct light_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;
	struct light_softc *sc = dc->dc_sc;

	if (sc->sc_nscreens > 0)
		return ENOMEM;

	sc->sc_nscreens++;

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &dc->dc_defattr);
	*attrp = dc->dc_defattr;

	return 0;
}

void
light_free_screen(void *v, void *cookie)
{
}

int
light_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

int
light_load_font(void *v, void *emulcookie, struct wsdisplay_font *font)
{
	struct light_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_load_font(ri, emulcookie, font);
}

int
light_list_font(void *v, struct wsdisplay_font *font)
{
	struct light_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_list_font(ri, font);
}

void
light_burner(void *v, u_int on, u_int flags)
{
	struct light_devconfig *dc = v;

	if (on)
		rex_vc1_sysctl_write(dc, rex_vc1_sysctl_read(dc) |
		    VC1_SYSCTL_VIDEO_ON);
	else
		rex_vc1_sysctl_write(dc, rex_vc1_sysctl_read(dc) &
		    ~VC1_SYSCTL_VIDEO_ON);
}

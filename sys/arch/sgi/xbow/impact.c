/*	$OpenBSD: impact.c,v 1.2 2010/03/08 04:32:28 deraadt Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
 * Copyright (c) 2009, 2010 Joel Sing <jsing@openbsd.org>
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
 * Driver for the SGI ImpactSR graphics board.
 */

/*
 * The details regarding the design and operation of this hardware, along with
 * the necessary magic numbers, are only available thanks to the reverse
 * engineering work undertaken by Stanislaw Skowronek <skylark@linux-mips.org>. 
 */

/*
 * This driver currently lacks support for the HQ4 DMA engine, which could be
 * used to speed up rasops `copy' operations a lot by doing framebuffer to
 * memory, then memory to framebuffer operations.
 *
 * Of course, in an ideal world, these operations would be done with
 * framebuffer to framebuffer operations, but according to Skowronek, these
 * operations are not reliable and IRIX' Xsgi X server does not even try to
 * use them.
 *
 * Thus this driver currently implements rasops `copy' operations by repainting
 * affected areas with PIO routines. This is unfortunately slower than DMA,
 * but this will work until I have more time to spend on this. -- miod
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <mips64/arcbios.h>

#include <sgi/xbow/impactreg.h>
#include <sgi/xbow/impactvar.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#define	IMPACT_WIDTH	1280
#define	IMPACT_HEIGHT	1024
#define	IMPACT_DEPTH	32

struct impact_screen;

struct impact_softc {
	struct device		 sc_dev;
	struct impact_screen	*curscr;
	int			 console;
	int			 nscreens;
};

int	impact_match(struct device *, void *, void *);
void	impact_attach(struct device *, struct device *, void *);

const struct cfattach impact_ca = {
	sizeof(struct impact_softc), impact_match, impact_attach,
};

struct cfdriver impact_cd = {
	NULL, "impact", DV_DULL,
};

struct impact_screen {
	struct rasops_info		 ri;
	long				 defattr;
	struct wsdisplay_charcell	*bs;

	struct impact_softc		*sc;

	bus_space_tag_t			 iot;
	bus_space_handle_t		 ioh;

	struct wsscreen_descr		 wsd;
	struct wsscreen_list		 wsl;
	struct wsscreen_descr		*scrlist[1];

	struct mips_bus_space		 iot_store;
};

int	impact_is_console(struct xbow_attach_args *);

static inline void
	impact_cmd_fifo_write(struct impact_screen *, uint64_t, uint32_t, int);
int	impact_cmd_fifo_wait(struct impact_screen *);

void	impact_setup(struct impact_screen *);
int	impact_init_screen(struct impact_screen *);

/*
 * Hardware acceleration (sort of) for rasops.
 */
void	impact_rop(struct impact_screen *, int, int, int, int, int, u_int);
void	impact_fillrect(struct impact_screen *, int, int, int, int, u_int);
int	impact_do_cursor(struct rasops_info *);
int	impact_putchar(void *, int, int, u_int, long);
int	impact_copycols(void *, int, int, int, int);
int	impact_erasecols(void *, int, int, int, long);
int	impact_copyrows(void *, int, int, int);
int	impact_eraserows(void *, int, int, long);

/* 
 * Interfaces for wscons.
 */
int 	impact_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t impact_mmap(void *, off_t, int);
int	impact_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	impact_free_screen(void *, void *);
int	impact_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);

static struct impact_screen impact_cons;

struct wsdisplay_accessops impact_accessops = {
	impact_ioctl,
	impact_mmap,
	impact_alloc_screen,
	impact_free_screen,
	impact_show_screen,
	NULL,			/* load_font */
	NULL,			/* scrollback */
	NULL,			/* getchar */
	NULL,			/* burner */
	NULL			/* pollc */
};

int
impact_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI5 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI5_IMPACT)
		return 1;

	return 0;
}

void
impact_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	struct wsemuldisplaydev_attach_args waa;
	struct impact_softc *sc = (void *)self;
	struct impact_screen *scr;

	if (strncmp(bios_graphics, "alive", 5) != 0) {
		printf(" device has not been setup by firmware!\n");
		return;
	}

	printf(" revision %d\n", xaa->xaa_revision);

	if (impact_is_console(xaa)) {
		/*
		 * Setup has already been done via impact_cnattach().
		 */
		scr = &impact_cons;
		scr->sc = sc;
       		sc->curscr = scr;
		sc->console = 1;
	} else {
		/*
		 * Setup screen data.
		 */
		scr = malloc(sizeof(struct impact_screen), M_DEVBUF,
		    M_NOWAIT);
		if (scr == NULL) {
			printf("failed to allocate screen memory!\n");
			return;
		}

		/*
		 * Create a copy of the bus space tag.
		 */
		bcopy(xaa->xaa_iot, &scr->iot_store,
		    sizeof(struct mips_bus_space));
		scr->iot = &scr->iot_store;

		/* Setup bus space mappings. */
		if (bus_space_map(scr->iot, IMPACTSR_REG_OFFSET,
		    IMPACTSR_REG_SIZE, 0, &scr->ioh)) {
			printf("failed to map registers\n");
			free(scr, M_DEVBUF);
			return;
		}

		scr->sc = sc;
       		sc->curscr = scr;

		/* Setup hardware and clear screen. */
		impact_setup(scr);
		if (impact_init_screen(scr) != 0) {
			printf("failed to allocate memory\n");
			bus_space_unmap(scr->iot, scr->ioh, IMPACTSR_REG_SIZE);
			free(scr, M_DEVBUF);
			return;
		}
	}

	scr->scrlist[0] = &scr->wsd;
	scr->wsl.nscreens = 1;
	scr->wsl.screens = (const struct wsscreen_descr **)scr->scrlist;

	waa.console = sc->console;
	waa.scrdata = &scr->wsl;
	waa.accessops = &impact_accessops;
	waa.accesscookie = scr;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
impact_init_screen(struct impact_screen *scr)
{
	struct rasops_info *ri = &scr->ri;
	size_t bssize;

	bzero(ri, sizeof(struct rasops_info));

	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	ri->ri_depth = IMPACT_DEPTH;
	ri->ri_width = IMPACT_WIDTH;
	ri->ri_height = IMPACT_HEIGHT;
	ri->ri_stride = IMPACT_WIDTH * IMPACT_DEPTH / 8;

	rasops_init(ri, 160, 160);

	/*
	 * Allocate backing store to remember character cells, to
	 * be able to fulfill scrolling requests.
	 */
	if (scr->bs == NULL) {
		bssize = ri->ri_rows * ri->ri_cols *
		    sizeof(struct wsdisplay_charcell);
		scr->bs = malloc(bssize, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (scr->bs == NULL)
			return ENOMEM;
	}

	ri->ri_hw = scr;

	ri->ri_ops.putchar = impact_putchar;
	ri->ri_do_cursor = impact_do_cursor;
	ri->ri_ops.copyrows = impact_copyrows;
	ri->ri_ops.copycols = impact_copycols;
	ri->ri_ops.eraserows = impact_eraserows;
	ri->ri_ops.erasecols = impact_erasecols;

	/* clear display */
	impact_fillrect(scr, 0, 0, ri->ri_width, ri->ri_height,
	    ri->ri_devcmap[WSCOL_BLACK]);

	strlcpy(scr->wsd.name, "std", sizeof(scr->wsd.name));
	scr->wsd.ncols = ri->ri_cols;
	scr->wsd.nrows = ri->ri_rows;
	scr->wsd.textops = &ri->ri_ops;
	scr->wsd.fontwidth = ri->ri_font->fontwidth;
	scr->wsd.fontheight = ri->ri_font->fontheight;
	scr->wsd.capabilities = ri->ri_caps;

	return 0;
}

/*
 * Hardware initialization.
 */

void
impact_setup(struct impact_screen *scr)
{
	/* HQ4 initialization */
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_CFIFO_HW, 0x00000047);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_CFIFO_LW, 0x00000014);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_CFIFO_DELAY, 0x00000064);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_DFIFO_HW, 0x00000040);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_DFIFO_LW, 0x00000010);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_DFIFO_DELAY, 0x00000000);

	/* VC3 initialization: disable hardware cursor */
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_VC3_INDEXDATA,
	    0x1d000100);

	/* RSS initialization */
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_COLORMASKLSBSA, 0xffffff, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_COLORMASKLSBSB, 0xffffff, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_COLORMASKMSBS, 0, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRMASKLO, 0xffffff, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRMASKHI, 0xffffff, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_DRBPOINTERS, 0xc8240, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_CONFIG, 0xcac, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_XYWIN,
	    IMPACTSR_YXCOORDS(0, 0x3ff), 0);

	/* XMAP initialization */
	bus_space_write_1(scr->iot, scr->ioh, IMPACTSR_XMAP_PP1SELECT, 0x01);
	bus_space_write_1(scr->iot, scr->ioh, IMPACTSR_XMAP_INDEX, 0x00);
	bus_space_write_4(scr->iot, scr->ioh, IMPACTSR_XMAP_MAIN_MODE,
	    0x000007a4);
}

/*
 * Write to the command FIFO.
 */
static inline void
impact_cmd_fifo_write(struct impact_screen *scr, uint64_t reg, uint32_t val,
    int exec)
{
	uint64_t cmd;

	cmd = IMPACTSR_CFIFO_WRITE | (reg << IMPACTSR_CFIFO_REG_SHIFT);
	if (exec)
		cmd |= IMPACTSR_CFIFO_EXEC;
	bus_space_write_8(scr->iot, scr->ioh, IMPACTSR_CFIFO, cmd | val);
}

/*
 * Wait until the command FIFO is empty.
 */
int
impact_cmd_fifo_wait(struct impact_screen *scr)
{
	u_int32_t val, timeout = 1000000;
	struct impact_softc *sc = scr->sc;

	val = bus_space_read_4(scr->iot, scr->ioh, IMPACTSR_FIFOSTATUS);
	while ((val & IMPACTSR_FIFO_MASK) != 0) {
		delay(1);
		if (--timeout == 0) {
#ifdef DIAGNOSTIC
			if (sc != NULL && sc->console == 0)
				printf("%s: timeout waiting for command fifo\n",
				    sc != NULL ? sc->sc_dev.dv_xname :
				      impact_cd.cd_name);
#endif
			return ETIMEDOUT;
		}
		val = bus_space_read_4(scr->iot, scr->ioh, IMPACTSR_FIFOSTATUS);
	}
	
	return 0;
}

/*
 * Interfaces for wscons.
 */

int
impact_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct impact_screen *scr = (struct impact_screen *)v;
	struct rasops_info *ri;
	struct wsdisplay_fbinfo *fb;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_IMPACT;
		break;
	case WSDISPLAYIO_GINFO:
		fb = (struct wsdisplay_fbinfo *)data;
		ri = &scr->ri;
		fb->height = ri->ri_height;
		fb->width = ri->ri_width;
		fb->depth = ri->ri_depth;
		fb->cmsize = 0;
		break;
#if 0
	case WSDISPLAYIO_LINEBYTES:
		ri = &scr->ri;
		*(u_int *)data = ri->ri_stride;
		break;
#endif
	default:
		return -1;
	}

	return 0;
}

paddr_t
impact_mmap(void *v, off_t offset, int prot)
{
	if (offset < 0 || (offset & PAGE_MASK) != 0)
		return -1;

	return -1;
}

int
impact_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct impact_screen *scr = (struct impact_screen *)v;
	struct rasops_info *ri = &scr->ri;
	struct impact_softc *sc = (struct impact_softc *)scr->sc;

	/* We do not allow multiple consoles at the moment. */
	if (sc->nscreens > 0)
		return ENOMEM;

	sc->nscreens++;

	*cookiep = ri;
	*curxp = 0;
	*curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &scr->defattr);
	*attrp = scr->defattr;

	return 0;
}

void
impact_free_screen(void *v, void *cookie)
{
	/* We do not allow multiple consoles at the moment. */
}

int
impact_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	/* We do not allow multiple consoles at the moment. */
	return 0;
}

/*
 * Hardware accelerated functions.
 */

void
impact_rop(struct impact_screen *scr, int x, int y, int w, int h, int op,
    u_int c)
{
	impact_cmd_fifo_wait(scr);
	if (op == OPENGL_LOGIC_OP_COPY)
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_PP1FILLMODE,
		    IMPACTSR_PP1FILLMODE(0x6300, op), 0);
	else
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_PP1FILLMODE,
		    IMPACTSR_PP1FILLMODE(0x6304, op), 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_FILLMODE, 0, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_PACKEDCOLOR, c & 0x00ffffff, 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYSTARTI,
	    IMPACTSR_XYCOORDS(x, y), 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYENDI,
	    IMPACTSR_XYCOORDS(x + w - 1, y + h - 1), 0);
	impact_cmd_fifo_write(scr, IMPACTSR_CMD_IR_ALIAS, 0x18, 1);
}

void
impact_fillrect(struct impact_screen *scr, int x, int y, int w, int h, u_int c)
{
	impact_rop(scr, x, y, w, h, OPENGL_LOGIC_OP_COPY, c);
}

int
impact_do_cursor(struct rasops_info *ri)
{
	struct impact_screen *scr = ri->ri_hw;
	int y, x, w, h, fg, bg;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_xorigin + ri->ri_ccol * w;
	y = ri->ri_yorigin + ri->ri_crow * h;

	ri->ri_ops.unpack_attr(ri, scr->defattr, &fg, &bg, NULL);

	impact_rop(scr, x, y, w, h, OPENGL_LOGIC_OP_XOR, ri->ri_devcmap[fg]);
	impact_cmd_fifo_wait(scr);

	return 0;
}

int
impact_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct impact_screen *scr = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, y, w, h, bg, fg, ul;
	u_int8_t *fontbitmap;
	u_int chunk;
	struct wsdisplay_charcell *cell;

	/* Update backing store. */
	cell = scr->bs + row * ri->ri_cols + col;
	cell->uc = uc;
	cell->attr = attr;
	
	w = font->fontwidth;
	h = font->fontheight;
	x = ri->ri_xorigin + col * w;
	y = ri->ri_yorigin + row * h;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, &ul);

	/* Handle spaces with a single fillrect. */
	if ((uc == ' ' || uc == 0) && ul == 0) {
		impact_fillrect(scr, x, y, w, h, ri->ri_devcmap[bg]);
		return 0;
	}

	fontbitmap = (u_int8_t *)(font->data + (uc - font->firstchar) *
	    ri->ri_fontscale);

	impact_cmd_fifo_wait(scr);
	if (ri->ri_devcmap[bg] == 0) {
		/*
		 * 1bpp pixel expansion; fast but background color ends up
		 * being incorrect (white renders as light green, and other
		 * colors end up slightly more greenish), which is why it is
		 * only done for black background.
		 */
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_PP1FILLMODE,
		    IMPACTSR_PP1FILLMODE(0x6300, OPENGL_LOGIC_OP_COPY), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_FILLMODE,
		    0x00400018, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_PACKEDCOLOR,
		    ri->ri_devcmap[fg], 0);
#if 0
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BKGRD_RG,
		    (ri->ri_devcmap[bg] & 0x0000ffff) << 8, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BKGRD_BA,
		    (ri->ri_devcmap[bg] & 0x00ff0000) >> 8, 0);
#else
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BKGRD_RG, 0, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BKGRD_BA, 0, 0);
#endif

		if (w <= 8) {
			impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYSTARTI,
			    IMPACTSR_XYCOORDS(x, y), 0);
			impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYENDI,
			    IMPACTSR_XYCOORDS(x + w - 1, y + h - 1), 0);
			impact_cmd_fifo_write(scr, IMPACTSR_CMD_IR_ALIAS,
			    0x18, 1);

			for (; h != 0; h--) {
				chunk = *fontbitmap;
				fontbitmap += font->stride;

				/* Handle underline. */
				if (ul && h == 1)
					chunk = 0xff;

				impact_cmd_fifo_wait(scr);
				impact_cmd_fifo_write(scr, IMPACTSR_CMD_CHAR_H,
				    chunk << 24, 1);
			}
		} else {
			for (; h != 0; h--) {
				chunk = *(u_int16_t *)fontbitmap;
				fontbitmap += font->stride;

				/* Handle underline. */
				if (ul && h == 1)
					chunk = 0xffff;

				impact_cmd_fifo_write(scr,
				    IMPACTSR_CMD_BLOCKXYSTARTI,
				    IMPACTSR_XYCOORDS(x, y), 0);
				impact_cmd_fifo_write(scr,
				    IMPACTSR_CMD_BLOCKXYENDI,
				    IMPACTSR_XYCOORDS(x + w - 1, y + 1 - 1), 0);
				impact_cmd_fifo_write(scr,
				    IMPACTSR_CMD_IR_ALIAS, 0x18, 1);

				impact_cmd_fifo_wait(scr);
				impact_cmd_fifo_write(scr, IMPACTSR_CMD_CHAR_H,
				    chunk << 16, 0);
				impact_cmd_fifo_write(scr, IMPACTSR_CMD_CHAR_L,
				    chunk << 24, 1);

				y++;
			}
		}
	} else {
		uint mask;
		uint32_t data;
		int i, flip;

		/* slower 8bpp blt; hopefully gives us the correct colors */
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_PP1FILLMODE,
		    IMPACTSR_PP1FILLMODE(0x6300, OPENGL_LOGIC_OP_COPY), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYSTARTI,
		    IMPACTSR_XYCOORDS(x, y), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_BLOCKXYENDI,
		    IMPACTSR_XYCOORDS(x + w - 1, y + h - 1), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_FILLMODE,
		    0x00c00000, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRMODE, 0x0080, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRSIZE,
		    IMPACTSR_YXCOORDS(w, h), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRCOUNTERS,
		    IMPACTSR_YXCOORDS(w, h), 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_GLINE_XSTARTF, 1, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_IR_ALIAS, 0x18, 1);
		for (i = 0; i < 32 + 1; i++)
			impact_cmd_fifo_write(scr, IMPACTSR_CMD_ALPHA, 0, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRCONTROL, 2, 0);

		flip = 0;
		for (; h != 0; h--) {
			if (w > 8)
				chunk = *(u_int16_t *)fontbitmap;
			else
				chunk = *fontbitmap;
			fontbitmap += font->stride;

			/* Handle underline. */
			if (ul && h == 1)
				chunk = 0xffff;

			/* font data is packed towards most significant bit */
			mask = w > 8 ? 0x8000 : 0x80;
			for (i = w; i != 0; i--, mask >>= 1) {
				data = ri->ri_devcmap[(chunk & mask) ? fg : bg];
				impact_cmd_fifo_write(scr, flip ?
				    IMPACTSR_CMD_CHAR_L : IMPACTSR_CMD_CHAR_H,
				    data, flip);
				flip ^= 1;
			}
			if (flip)
				impact_cmd_fifo_write(scr, IMPACTSR_CMD_CHAR_L,
				    0, 1);
		}

		impact_cmd_fifo_write(scr, IMPACTSR_CMD_GLINE_XSTARTF, 0, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_RE_TOGGLECNTX, 0, 0);
		impact_cmd_fifo_write(scr, IMPACTSR_CMD_XFRCOUNTERS,
		    IMPACTSR_YXCOORDS(0, 0), 0);
	}

	return 0;
}

int
impact_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct impact_screen *scr = ri->ri_hw;
	struct wsdisplay_charcell *cell;

	/* Copy columns in backing store. */
	cell = scr->bs + row * ri->ri_cols;
	ovbcopy(cell + src, cell + dst,
	    num * sizeof(struct wsdisplay_charcell));

	/* Repaint affected area */
	cell += dst;
	for (; num != 0; num--, dst++, cell++)
		impact_putchar(cookie, row, dst, cell->uc, cell->attr);

	return 0;
}

int
impact_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct impact_screen *scr = ri->ri_hw;
	int bg, fg, i;
	struct wsdisplay_charcell *cell;

	/* Erase columns in backing store. */
	cell = scr->bs + row * ri->ri_cols + col;
	for (i = num; i != 0; i--, cell++) {
		cell->uc = 0;
		cell->attr = attr;
	}

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	row *= ri->ri_font->fontheight;
	col *= ri->ri_font->fontwidth;
	num *= ri->ri_font->fontwidth;

	impact_fillrect(scr, ri->ri_xorigin + col, ri->ri_yorigin + row,
	    num, ri->ri_font->fontheight, ri->ri_devcmap[bg]);

	return 0;
}

int
impact_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct impact_screen *scr = ri->ri_hw;
	struct wsdisplay_charcell *cell;
	int col;

	/* Copy rows in backing store. */
	cell = scr->bs + dst * ri->ri_cols;
	ovbcopy(scr->bs + src * ri->ri_cols, cell,
	    num * ri->ri_cols * sizeof(struct wsdisplay_charcell));

	/* Repaint affected area */
	for (; num != 0; num--, dst++) {
		for (col = 0; col < ri->ri_cols; col++, cell++)
			impact_putchar(cookie, dst, col, cell->uc, cell->attr);
	}

	return 0;
}

int
impact_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct impact_screen *scr = ri->ri_hw;
	int x, y, w, bg, fg;
	struct wsdisplay_charcell *cell;

	/* Erase rows in backing store. */
	cell = scr->bs + row * ri->ri_cols;
	for (x = ri->ri_cols; x != 0; x--, cell++) {
		cell->uc = 0;
		cell->attr = attr;
	}
	for (y = 1; y < num; y++)
		ovbcopy(scr->bs + row * ri->ri_cols,
		    scr->bs + (row + y) * ri->ri_cols,
		    ri->ri_cols * sizeof(struct wsdisplay_charcell));

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	if ((num == ri->ri_rows) && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		num = ri->ri_height;
		x = y = 0;
		w = ri->ri_width;
	} else {
		num *= ri->ri_font->fontheight;
		x = ri->ri_xorigin;
		y = ri->ri_yorigin + row * ri->ri_font->fontheight;
		w = ri->ri_emuwidth;
	}

	impact_fillrect(scr, x, y, w, num, ri->ri_devcmap[bg]);
	impact_cmd_fifo_wait(scr);

	return 0;
}

/*
 * Console support.
 */

static int16_t impact_console_nasid;
static int impact_console_widget;

/* console backing store, worst case font selection */
static struct wsdisplay_charcell
	impact_cons_bs[(IMPACT_WIDTH / 8) * (IMPACT_HEIGHT / 16)];

int
impact_cnprobe(int16_t nasid, int widget)
{
	u_int32_t wid, vendor, product;

	if (xbow_widget_id(nasid, widget, &wid) != 0)
		return 0;

	vendor = (wid & WIDGET_ID_VENDOR_MASK) >> WIDGET_ID_VENDOR_SHIFT;
	product = (wid & WIDGET_ID_PRODUCT_MASK) >> WIDGET_ID_PRODUCT_SHIFT;

	if (vendor != XBOW_VENDOR_SGI5 || product != XBOW_PRODUCT_SGI5_IMPACT)
		return 0;

	if (strncmp(bios_graphics, "alive", 5) != 0)
		return 0;

	return 1;
}

int
impact_cnattach(int16_t nasid, int widget)
{
	struct impact_screen *scr = &impact_cons;
	struct rasops_info *ri = &scr->ri;
	int rc;

	/* Build bus space accessor. */
	xbow_build_bus_space(&scr->iot_store, nasid, widget);
	scr->iot = &scr->iot_store;

	rc = bus_space_map(scr->iot, IMPACTSR_REG_OFFSET, IMPACTSR_REG_SIZE,
	    0, &scr->ioh);
	if (rc != 0)
		return rc;

	impact_setup(scr);
	scr->bs = impact_cons_bs;
	rc = impact_init_screen(scr);
	if (rc != 0) {
		bus_space_unmap(scr->iot, scr->ioh, IMPACTSR_REG_SIZE);
		return rc;
	}

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &scr->defattr);
	wsdisplay_cnattach(&scr->wsd, ri, 0, 0, scr->defattr);

	impact_console_nasid = nasid;
	impact_console_widget = widget;

	return 0;
}

int
impact_is_console(struct xbow_attach_args *xaa)
{
	return xaa->xaa_nasid == impact_console_nasid &&
	    xaa->xaa_widget == impact_console_widget;
}

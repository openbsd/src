/*	$OpenBSD: odyssey.c,v 1.3 2010/03/08 20:54:45 miod Exp $ */
/*
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
 * Driver for the SGI VPro (aka Odyssey) Graphics Card.
 */

/*
 * The details regarding the design and operation of this hardware, along with
 * the necessary magic numbers, are only available thanks to the reverse
 * engineering work undertaken by Stanislaw Skowronek <skylark@linux-mips.org>. 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <mips64/arcbios.h>

#include <sgi/xbow/odysseyreg.h>
#include <sgi/xbow/odysseyvar.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

/*
 * Colourmap data.
 */
struct  odyssey_cmap {
	u_int8_t cm_red[256];
	u_int8_t cm_green[256];
	u_int8_t cm_blue[256];
};

/*
 * Screen data.
 */
struct odyssey_screen {
	struct device *sc;		/* Back pointer. */

	struct rasops_info ri;		/* Screen raster display info. */
	struct odyssey_cmap cmap;	/* Display colour map. */
	long attr;			/* Rasops attributes. */

	int width;			/* Width in pixels. */
	int height;			/* Height in pixels. */
	int depth;			/* Colour depth in bits. */
	int linebytes;			/* Bytes per line. */
	int ro_curpos;			/* Current position in rasops tile. */
};

struct odyssey_softc {
	struct device		sc_dev;

	struct mips_bus_space	iot_store;
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;

	int console;				/* Is this the console? */
	int screens;				/* No of screens allocated. */

	struct odyssey_screen	*curscr;	/* Current screen. */
};

int	odyssey_match(struct device *, void *, void *);
void	odyssey_attach(struct device *, struct device *, void *);

int	odyssey_is_console(struct xbow_attach_args *);

void	odyssey_cmd_wait(struct odyssey_softc *);
void	odyssey_data_wait(struct odyssey_softc *);
void	odyssey_cmd_flush(struct odyssey_softc *, int);

void	odyssey_setup(struct odyssey_softc *);
void	odyssey_init_screen(struct odyssey_screen *);

/*
 * Colour map handling for indexed modes.
 */
int	odyssey_getcmap(struct odyssey_cmap *, struct wsdisplay_cmap *);
int	odyssey_putcmap(struct odyssey_cmap *, struct wsdisplay_cmap *);

/*
 * Hardware acceleration for rasops.
 */
void	odyssey_rop(struct odyssey_softc *, int, int, int, int, int, int);
void	odyssey_copyrect(struct odyssey_softc *, int, int, int, int, int, int);
void	odyssey_fillrect(struct odyssey_softc *, int, int, int, int, u_int);
int	odyssey_do_cursor(struct rasops_info *);
int	odyssey_putchar(void *, int, int, u_int, long);
int	odyssey_copycols(void *, int, int, int, int);
int	odyssey_erasecols(void *, int, int, int, long);
int	odyssey_copyrows(void *, int, int, int);
int	odyssey_eraserows(void *, int, int, long);

u_int32_t ieee754_sp(int32_t);

/* 
 * Interfaces for wscons.
 */
int 	odyssey_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t odyssey_mmap(void *, off_t, int);
int	odyssey_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	odyssey_free_screen(void *, void *);
int	odyssey_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
void	odyssey_burner(void *, u_int, u_int);

static struct odyssey_screen odyssey_consdata;
static struct odyssey_softc odyssey_cons_sc;

struct wsscreen_descr odyssey_stdscreen = {
	"std",			/* Screen name. */
};

struct wsdisplay_accessops odyssey_accessops = {
	odyssey_ioctl,
	odyssey_mmap,
	odyssey_alloc_screen,
	odyssey_free_screen,
	odyssey_show_screen,
	NULL,			/* load_font */
	NULL,			/* scrollback */
	NULL,			/* getchar */
	odyssey_burner,
	NULL			/* pollc */
};

const struct wsscreen_descr *odyssey_scrlist[] = {
	&odyssey_stdscreen
};

struct wsscreen_list odyssey_screenlist = {
	nitems(odyssey_scrlist), odyssey_scrlist
};

const struct cfattach odyssey_ca = {
	sizeof(struct odyssey_softc), odyssey_match, odyssey_attach,
};

struct cfdriver odyssey_cd = {
	NULL, "odyssey", DV_DULL,
};

int
odyssey_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI2 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI2_ODYSSEY)
		return 1;

	return 0;
}

void
odyssey_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	struct wsemuldisplaydev_attach_args waa;
	struct odyssey_softc *sc = (void *)self;
	struct odyssey_screen *screen;

	if (strncmp(bios_graphics, "alive", 5) != 0) {
		printf(" device has not been setup by firmware!\n");
		return;
	}

	printf(" revision %d\n", xaa->xaa_revision);

	/*
	 * Create a copy of the bus space tag.
	 */
	bcopy(xaa->xaa_iot, &sc->iot_store, sizeof(struct mips_bus_space));
	sc->iot = &sc->iot_store;

	/* Setup bus space mappings. */
	if (bus_space_map(sc->iot, ODYSSEY_REG_OFFSET, ODYSSEY_REG_SIZE,
	    BUS_SPACE_MAP_LINEAR, &sc->ioh)) {
		printf("failed to map bus space!\n");
		return;
	}

	if (odyssey_is_console(xaa)) {
		/*
		 * Setup has already been done via odyssey_cnattach().
		 */
		screen = &odyssey_consdata;
       		sc->curscr = screen;
		sc->curscr->sc = (void *)sc;
		sc->console = 1;
	} else {
		/*
		 * Setup screen data.
		 */
		sc->curscr = malloc(sizeof(struct odyssey_screen), M_DEVBUF,
		    M_NOWAIT);
		if (sc->curscr == NULL) {
			printf("failed to allocate screen memory!\n");
			return;
		}
		sc->curscr->sc = (void *)sc;
		screen = sc->curscr;

		/* Setup hardware and clear screen. */
		odyssey_setup(sc);
		odyssey_fillrect(sc, 0, 0, 1280, 1024, 0x000000);
		odyssey_cmd_flush(sc, 0);

		/* Set screen defaults. */
		screen->width = 1280;
		screen->height = 1024;
		screen->depth = 32;
		screen->linebytes = screen->width * screen->depth / 8;

		odyssey_init_screen(screen);
	}

	waa.console = sc->console;
	waa.scrdata = &odyssey_screenlist;
	waa.accessops = &odyssey_accessops;
	waa.accesscookie = screen;
	waa.defaultscreens = 0;
	config_found(self, &waa, wsemuldisplaydevprint);
}

void
odyssey_init_screen(struct odyssey_screen *screen)
{
	u_char *colour;
	int i;

	/*
	 * Initialise screen.
	 */

	/* Initialise rasops. */
	memset(&screen->ri, 0, sizeof(struct rasops_info));

	screen->ri.ri_flg = RI_CENTER;
	screen->ri.ri_depth = screen->depth;
	screen->ri.ri_width = screen->width;
	screen->ri.ri_height = screen->height;
	screen->ri.ri_stride = screen->linebytes;

	if (screen->depth == 32) {
		screen->ri.ri_bpos = 16;
		screen->ri.ri_bnum = 8;
		screen->ri.ri_gpos = 8;
		screen->ri.ri_gnum = 8;
		screen->ri.ri_rpos = 0;
		screen->ri.ri_rnum = 8;
	} else if (screen->depth == 16) {
		screen->ri.ri_rpos = 10;
		screen->ri.ri_rnum = 5;
		screen->ri.ri_gpos = 5;
		screen->ri.ri_gnum = 5;
		screen->ri.ri_bpos = 0;
		screen->ri.ri_bnum = 5;
	}

	rasops_init(&screen->ri, screen->height / 8, screen->width / 8);

	/*
	 * Initialise colourmap, if required.
	 */
	if (screen->depth == 8) {
		for (i = 0; i < 16; i++) {
			colour = (u_char *)&rasops_cmap[i * 3];
			screen->cmap.cm_red[i] = colour[0];
			screen->cmap.cm_green[i] = colour[1];
			screen->cmap.cm_blue[i] = colour[2];
		}
		for (i = 240; i < 256; i++) {
			colour = (u_char *)&rasops_cmap[i * 3];
			screen->cmap.cm_red[i] = colour[0];
			screen->cmap.cm_green[i] = colour[1];
			screen->cmap.cm_blue[i] = colour[2];
		}
	}

	screen->ri.ri_hw = screen->sc;

	screen->ri.ri_ops.putchar = odyssey_putchar;
	screen->ri.ri_do_cursor = odyssey_do_cursor;
	screen->ri.ri_ops.copyrows = odyssey_copyrows;
	screen->ri.ri_ops.copycols = odyssey_copycols;
	screen->ri.ri_ops.eraserows = odyssey_eraserows;
	screen->ri.ri_ops.erasecols = odyssey_erasecols;

	odyssey_stdscreen.ncols = screen->ri.ri_cols;
	odyssey_stdscreen.nrows = screen->ri.ri_rows;
	odyssey_stdscreen.textops = &screen->ri.ri_ops;
	odyssey_stdscreen.fontwidth = screen->ri.ri_font->fontwidth;
	odyssey_stdscreen.fontheight = screen->ri.ri_font->fontheight;
	odyssey_stdscreen.capabilities = screen->ri.ri_caps;
}

/*
 * Hardware initialisation.
 */
void
odyssey_setup(struct odyssey_softc *sc)
{
	u_int64_t val;
	int i;

	/* Initialise Buzz Graphics Engine. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x20008003);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x21008010);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x22008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x23008002);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2400800c);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2500800e);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x27008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x28008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x290080d6);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2a0080e0);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2c0080ea);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2e008380);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2f008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x30008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x31008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x32008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x33008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x34008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x35008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x310081e0);
	odyssey_cmd_flush(sc, 0);

	/* Initialise Buzz X-Form. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x9080bda2);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0xbf800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x4e000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x40400000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x4e000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x4d000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x34008000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x9080bdc8);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x34008010);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x908091df);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3f800000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x34008000);
	odyssey_cmd_flush(sc, 0);

	/* Initialise Buzz Raster. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0001203b);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00001000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00001000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00001000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00001000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0001084a);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000080);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000080);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010845);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x000000ff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x000076ff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0001141b);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000001);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00011c16);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x03000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010404);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00011023);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00ff0ff0);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00ff0ff0);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x000000ff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00011017);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000050);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x20004950);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0001204b);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x004ff3ff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00ffffff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00ffffff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00ffffff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	odyssey_cmd_flush(sc, 0);

	/*
	 * Initalise Pixel Blaster & Jammer.
	 */
	for (i = 0; i < 32; i++) {
		if ((i & 0xf) == 0)
			odyssey_data_wait(sc);

		bus_space_write_8(sc->iot, sc->ioh, ODYSSEY_DATA_FIFO,
		    ((0x30000001ULL | ((0x2900 + i) << 14)) << 32) |
		    0x905215a6);
	}

	odyssey_data_wait(sc);
	bus_space_write_8(sc->iot, sc->ioh, ODYSSEY_DATA_FIFO,
	    ((0x30000001ULL | (0x2581 << 14)) << 32) | 0x0);

	/* Gamma ramp. */
	for (i = 0; i < 0x600; i++) {
		if ((i & 0xf) == 0)
			odyssey_data_wait(sc);

		if (i < 0x200)
			val = i >> 2;
		else if (i < 0x300)
			val = ((i - 0x200) >> 1) + 0x80;
		else
			val = ((i - 0x300) >> 1) + 0x100;

		val = (val << 20) | (val << 10) | val;

		bus_space_write_8(sc->iot, sc->ioh, ODYSSEY_DATA_FIFO,
		    ((0x30000001ULL | ((0x1a00 + i) << 14)) << 32) | val);
	}
}

void
odyssey_cmd_wait(struct odyssey_softc *sc)
{
	u_int32_t val, timeout = 1000000;

	val = bus_space_read_4(sc->iot, sc->ioh, ODYSSEY_STATUS);
	while ((val & ODYSSEY_STATUS_CMD_FIFO_LOW) == 0) {
		delay(1);
		if (--timeout == 0) {
			printf("odyssey: timeout waiting for command fifo!\n");
			return;
		}
		val = bus_space_read_4(sc->iot, sc->ioh, ODYSSEY_STATUS);
	}
}

void
odyssey_data_wait(struct odyssey_softc *sc)
{
	u_int32_t val, timeout = 1000000;

	val = bus_space_read_4(sc->iot, sc->ioh, ODYSSEY_DBE_STATUS);
	while ((val & 0x7f) > 0) {
		delay(1);
		if (--timeout == 0) {
			printf("odyssey: timeout waiting for data fifo!\n");
			return;
		}
		val = bus_space_read_4(sc->iot, sc->ioh, ODYSSEY_DBE_STATUS);
	}
}

void
odyssey_cmd_flush(struct odyssey_softc *sc, int quick)
{

	odyssey_cmd_wait(sc);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010443);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x000000fa);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);

	if (quick)
		return;

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010019);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010443);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000096);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010443);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x000000fa);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010046);
}

/*
 * Interfaces for wscons.
 */

int
odyssey_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct odyssey_screen *screen = (struct odyssey_screen *)v;
	int rc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_ODYSSEY;
		break;

	case WSDISPLAYIO_GINFO:
	{
		struct wsdisplay_fbinfo *fb = (struct wsdisplay_fbinfo *)data;

		fb->height = screen->height;
		fb->width = screen->width;
		fb->depth = screen->depth;
		fb->cmsize = screen->depth == 8 ? 256 : 0;
	}
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = screen->linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		if (screen->depth == 8) {
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = odyssey_getcmap(&screen->cmap, cm);
			if (rc != 0)
				return (rc);
		}
		break;

	case WSDISPLAYIO_PUTCMAP:
		if (screen->depth == 8) {
			struct wsdisplay_cmap *cm =
			    (struct wsdisplay_cmap *)data;

			rc = odyssey_putcmap(&screen->cmap, cm);
			if (rc != 0)
				return (rc);
		}
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		/* Handled by the upper layer. */
		break;

	default:
		return (-1);
	}

	return (0);
}

int
odyssey_getcmap(struct odyssey_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int rc;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((rc = copyout(&cm->cm_red[index], rcm->red, count)) != 0)
		return (rc);
	if ((rc = copyout(&cm->cm_green[index], rcm->green, count)) != 0)
		return (rc);
	if ((rc = copyout(&cm->cm_blue[index], rcm->blue, count)) != 0)
		return (rc);

	return (0);
}

int
odyssey_putcmap(struct odyssey_cmap *cm, struct wsdisplay_cmap *rcm)
{
	u_int index = rcm->index, count = rcm->count;
	int rc;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((rc = copyin(rcm->red, &cm->cm_red[index], count)) != 0)
		return (rc);
	if ((rc = copyin(rcm->green, &cm->cm_green[index], count)) != 0)
		return (rc);
	if ((rc = copyin(rcm->blue, &cm->cm_blue[index], count)) != 0)
		return (rc);

	return (0);
}

paddr_t
odyssey_mmap(void *v, off_t offset, int protection)
{
	return (-1);
}

int
odyssey_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct odyssey_screen *screen = (struct odyssey_screen *)v;
	struct odyssey_softc *sc = (struct odyssey_softc *)screen->sc;

	/* We do not allow multiple consoles at the moment. */
	if (sc->screens > 0)
		return (ENOMEM);

	sc->screens++;

	/* Return rasops_info via cookie. */
	*cookiep = &screen->ri;

	/* Move cursor to top left of screen. */
	*curxp = 0;
	*curyp = 0;

	/* Correct screen attributes. */
	screen->ri.ri_ops.alloc_attr(&screen->ri, 0, 0, 0, attrp);
	screen->attr = *attrp;

	return (0);
}

void
odyssey_free_screen(void *v, void *cookie)
{
	/* We do not allow multiple consoles at the moment. */
}

int
odyssey_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	/* We do not allow multiple consoles at the moment. */
	return (0);
}

void
odyssey_burner(void *v, u_int on, u_int flags)
{
}

/*
 * Hardware accelerated functions.
 */

void
odyssey_rop(struct odyssey_softc *sc, int x, int y, int w, int h, int op, int c)
{
	/* Setup raster operation. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010404);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00100000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_LOGIC_OP);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, op);
	odyssey_cmd_flush(sc, 1);

	odyssey_fillrect(sc, x, y, w, h, c);

	/* Return to copy mode. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010404);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00100000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_LOGIC_OP);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
	    OPENGL_LOGIC_OP_COPY);
	odyssey_cmd_flush(sc, 1);
}

void
odyssey_copyrect(struct odyssey_softc *sc, int sx, int sy, int dx, int dy,
    int w, int h)
{
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010658);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00120000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002031);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, sx | (sy << 16));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x80502050);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, w | (h << 16));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x82223042);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, dx | (dy << 16));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x3222204b);

	odyssey_cmd_flush(sc, 1);
}

void
odyssey_fillrect(struct odyssey_softc *sc, int x, int y, int w, int h, u_int c)
{
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_BEGIN);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_QUADS);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_COLOR_3UB);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, c & 0xff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, (c >> 8) & 0xff);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, (c >> 16) & 0xff);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_VERTEX_2I);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, x);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, y);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_VERTEX_2I);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, x + w);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, y);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_VERTEX_2I);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, x + w);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, y + h);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_VERTEX_2I);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, x);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, y + h);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, OPENGL_END);

	odyssey_cmd_flush(sc, 1);
}

int
odyssey_do_cursor(struct rasops_info *ri)
{
	struct odyssey_softc *sc = ri->ri_hw;
	struct odyssey_screen *screen = sc->curscr;
	int y, x, w, h, fg, bg;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_xorigin + ri->ri_ccol * w;
	y = ri->ri_yorigin + ri->ri_crow * h;

	ri->ri_ops.unpack_attr(ri, screen->attr, &fg, &bg, NULL);

	odyssey_rop(sc, x, y, w, h, OPENGL_LOGIC_OP_XOR, ri->ri_devcmap[fg]);
	odyssey_cmd_flush(sc, 0);

	return 0;
}

int
odyssey_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct odyssey_softc *sc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int x, y, w, h, bg, fg, ul, i, j, ci, l;
	u_int8_t *fontbitmap;
	u_int chunk;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_xorigin + col * w;
	y = ri->ri_yorigin + row * h;

	fontbitmap = (u_int8_t *)(font->data + (uc - font->firstchar) *
	    ri->ri_fontscale);
	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, &ul);

	/* Handle spaces with a single fillrect. */
	if (uc == ' ') {
		odyssey_fillrect(sc, x, y, w, h, ri->ri_devcmap[bg]);
		odyssey_cmd_flush(sc, 0);
		return 0;
	}

	odyssey_fillrect(sc, x, y, w, h, 0xff0000);

	/* Setup pixel painting. */
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010405);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002400);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0xc580cc08);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00011453);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000002);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	odyssey_cmd_flush(sc, 0);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x2900812f);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00014400);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0000000a);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0xcf80a92f);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, ieee754_sp(x));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, ieee754_sp(y));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
	    ieee754_sp(x + font->fontwidth));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
	    ieee754_sp(y + font->fontheight));
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x8080c800);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00004570);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x0f00104c);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000071);

	for (i = font->fontheight; i != 0; i--) {

		/* Get bitmap for current line. */
		if (font->fontwidth <= 8)
			chunk = *fontbitmap;
		else
			chunk = *(u_int16_t *)fontbitmap;
		fontbitmap += font->stride;

		/* Handle underline. */
		if (ul && i == 1)
			chunk = 0xffff;

		/* Draw character. */
		bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
		    0x00004570);
		bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
		    0x0fd1104c);
		bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO,
		    0x00000071);

		w = font->fontwidth;
		l = 0;

		for (j = 0; j < font->fontwidth; j++) {

			if (l == 0) {

				l = (w > 14 ? 14 : w);
				w -= 14;

				/* Number of pixels. */
				bus_space_write_4(sc->iot, sc->ioh,
				    ODYSSEY_CMD_FIFO, (0x00014011 |
				    (l << 10)));

			}
		
			if (font->fontwidth > 8)
				ci = (chunk & (1 << (16 - j)) ? fg : bg);
			else
				ci = (chunk & (1 << (8 - j)) ? fg : bg);

			bus_space_write_4(sc->iot, sc->ioh,
			    ODYSSEY_CMD_FIFO, ri->ri_devcmap[ci]);
			
			l--;
		}
	}

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00014001);
	odyssey_cmd_flush(sc, 1);

	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x290080d6);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00011453);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00000000);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00010405);
	bus_space_write_4(sc->iot, sc->ioh, ODYSSEY_CMD_FIFO, 0x00002000);
	odyssey_cmd_flush(sc, 0);

	return 0;
}

int
odyssey_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct odyssey_softc *sc = ri->ri_hw;
	int i;

	if (src < dst) {

		/* We cannot control copy direction, so copy col by col. */
		for (i = num - 1; i >= 0; i--)
			odyssey_copyrect(sc,
			    ri->ri_xorigin + (src + i) * ri->ri_font->fontwidth,
			    ri->ri_yorigin + row * ri->ri_font->fontheight,
			    ri->ri_xorigin + (dst + i) * ri->ri_font->fontwidth,
			    ri->ri_yorigin + row * ri->ri_font->fontheight,
			    ri->ri_font->fontwidth, ri->ri_font->fontheight);

	} else {

		odyssey_copyrect(sc,
		    ri->ri_xorigin + src * ri->ri_font->fontwidth,
		    ri->ri_yorigin + row * ri->ri_font->fontheight,
		    ri->ri_xorigin + dst * ri->ri_font->fontwidth,
		    ri->ri_yorigin + row * ri->ri_font->fontheight,
		    num * ri->ri_font->fontwidth, ri->ri_font->fontheight);

	}

	odyssey_cmd_flush(sc, 0);

	return 0;
}

int
odyssey_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct odyssey_softc *sc = ri->ri_hw;
	int bg, fg;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	row *= ri->ri_font->fontheight;
	col *= ri->ri_font->fontwidth;
	num *= ri->ri_font->fontwidth;

	odyssey_fillrect(sc, ri->ri_xorigin + col, ri->ri_yorigin + row,
	    num, ri->ri_font->fontheight, ri->ri_devcmap[bg]);
	odyssey_cmd_flush(sc, 0);

	return 0;
}

int
odyssey_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct odyssey_softc *sc = ri->ri_hw;
	int i;

	if (src < dst) {

		/* We cannot control copy direction, so copy row by row. */
		for (i = num - 1; i >= 0; i--)
			odyssey_copyrect(sc, ri->ri_xorigin, ri->ri_yorigin +
			    (src + i) * ri->ri_font->fontheight,
			    ri->ri_xorigin, ri->ri_yorigin +
			    (dst + i) * ri->ri_font->fontheight,
			    ri->ri_emuwidth, ri->ri_font->fontheight);

	} else {

		odyssey_copyrect(sc, ri->ri_xorigin,
		    ri->ri_yorigin + src * ri->ri_font->fontheight,
		    ri->ri_xorigin,
		    ri->ri_yorigin + dst * ri->ri_font->fontheight,
		    ri->ri_emuwidth, num * ri->ri_font->fontheight);

	}

	odyssey_cmd_flush(sc, 0);

	return 0;
}

int
odyssey_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct odyssey_softc *sc = ri->ri_hw;
	int x, y, w, bg, fg;

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

	odyssey_fillrect(sc, x, y, w, num, ri->ri_devcmap[bg]);
	odyssey_cmd_flush(sc, 0);

	return 0;
}

u_int32_t
ieee754_sp(int32_t v)
{
	u_int8_t exp = 0, sign = 0;
	int i = 32;

	/*
	 * Convert an integer to IEEE754 single precision floating point:
	 *
	 * 	Sign - 1 bit
	 * 	Exponent - 8 bits (with 2^(8-1)-1 = 127 bias)
	 * 	Fraction - 23 bits
	 */

	if (v < 0) {
		sign = 1;
		v = -v;
	}

	/* Determine shift for fraction. */
	while (i && (v & (1 << --i)) == 0);

	if (v != 0)
		exp = 127 + i;

	return (sign << 31) | (exp << 23) | ((v << (23 - i)) & 0x7fffff);
}

/*
 * Console support.
 */

static int16_t odyssey_console_nasid;
static int odyssey_console_widget;

int
odyssey_cnprobe(int16_t nasid, int widget)
{
	u_int32_t wid, vendor, product;

	/* Probe for Odyssey graphics card. */
	if (xbow_widget_id(nasid, widget, &wid) != 0)
		return 0;

	vendor = (wid & WIDGET_ID_VENDOR_MASK) >> WIDGET_ID_VENDOR_SHIFT;
	product = (wid & WIDGET_ID_PRODUCT_MASK) >> WIDGET_ID_PRODUCT_SHIFT;

	if (vendor != XBOW_VENDOR_SGI2 || product != XBOW_PRODUCT_SGI2_ODYSSEY)
		return 0;

	if (strncmp(bios_graphics, "alive", 5) != 0)
		return 0;

	return 1;
}

int
odyssey_cnattach(int16_t nasid, int widget)
{
	struct odyssey_softc *sc;
	struct odyssey_screen *screen;
	long attr;
	int rc;

	sc = &odyssey_cons_sc;
	screen = &odyssey_consdata;
	sc->curscr = screen;
	sc->curscr->sc = (void *)sc;

	/* Build bus space accessor. */
	xbow_build_bus_space(&sc->iot_store, nasid, widget);
	sc->iot = &sc->iot_store;

	/* Setup bus space mappings. */
	rc = bus_space_map(sc->iot, ODYSSEY_REG_OFFSET, ODYSSEY_REG_SIZE,
	    BUS_SPACE_MAP_LINEAR, &sc->ioh);
	if (rc != 0)
		return rc;

	/* Setup hardware and clear screen. */
	odyssey_setup(sc);
	odyssey_fillrect(sc, 0, 0, 1280, 1024, 0x000000);
	odyssey_cmd_flush(sc, 0);

	/* Set screen defaults. */
	screen->width = 1280;
	screen->height = 1024;
	screen->depth = 32;
	screen->linebytes = screen->width * screen->depth / 8;

	odyssey_init_screen(screen);

	/*
	 * Attach wsdisplay.
	 */
	odyssey_consdata.ri.ri_ops.alloc_attr(&odyssey_consdata.ri,
	    0, 0, 0, &attr);
	wsdisplay_cnattach(&odyssey_stdscreen, &odyssey_consdata.ri,
	    0, 0, attr);
	odyssey_console_nasid = nasid;
	odyssey_console_widget = widget;

	return 0;
}

int
odyssey_is_console(struct xbow_attach_args *xaa)
{
	return xaa->xaa_nasid == odyssey_console_nasid &&
	    xaa->xaa_widget == odyssey_console_widget;
}

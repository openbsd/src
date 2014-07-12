/*	$OpenBSD: newport.c,v 1.9 2014/07/12 18:44:42 tedu Exp $	*/
/*	$NetBSD: newport.c,v 1.15 2009/05/12 23:51:25 macallan Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <sgi/dev/gl.h>
#include <sgi/gio/gioreg.h>
#include <sgi/gio/giovar.h>
#include <sgi/gio/newportreg.h>
#include <sgi/gio/newportvar.h>

#include <dev/cons.h>

struct newport_softc {
	struct device		sc_dev;

	struct newport_devconfig *sc_dc;

	int			sc_nscreens;
	struct wsscreen_list	sc_wsl;
	const struct wsscreen_descr *sc_scrlist[1];
};

struct newport_devconfig {
	struct rasops_info	dc_ri;
	long			dc_defattr;

	uint32_t		dc_addr;
	bus_space_tag_t		dc_st;
	bus_space_handle_t	dc_sh;

	int			dc_xres;
	int			dc_yres;
	int			dc_depth;

#ifdef notyet
	int			dc_mode;
#endif

	int			dc_boardrev;
	int			dc_vc2rev;
	int			dc_xmaprev;

	struct newport_softc	*dc_sc;
	struct wsscreen_descr	dc_wsd;
};

int	newport_match(struct device *, void *, void *);
void	newport_attach(struct device *, struct device *, void *);

struct cfdriver newport_cd = {
	NULL, "newport", DV_DULL
};

const struct cfattach newport_ca = {
	sizeof(struct newport_softc), newport_match, newport_attach
};

/* accessops */
int	newport_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	newport_mmap(void *, off_t, int);
int	newport_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	newport_free_screen(void *, void *);
int	newport_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
int	newport_load_font(void *, void *, struct wsdisplay_font *);
int	newport_list_font(void *, struct wsdisplay_font *);

struct wsdisplay_accessops newport_accessops = {
	.ioctl = newport_ioctl,
	.mmap = newport_mmap,
	.alloc_screen = newport_alloc_screen,
	.free_screen = newport_free_screen,
	.show_screen = newport_show_screen,
	.load_font = newport_load_font,
	.list_font = newport_list_font
};

int	newport_do_cursor(struct rasops_info *);
int	newport_putchar(void *, int, int, u_int, long);
int	newport_copycols(void *, int, int, int, int);
int	newport_erasecols(void *, int, int, int, long);
int	newport_copyrows(void *, int, int, int);
int	newport_eraserows(void *, int, int, long);

static __inline__
void	 rex3_write(struct newport_devconfig *, bus_size_t, uint32_t);
static __inline__
void	 rex3_write_go(struct newport_devconfig *, bus_size_t, uint32_t);
static __inline__
uint32_t rex3_read(struct newport_devconfig *, bus_size_t);
int	 rex3_wait_gfifo(struct newport_devconfig *, const char *);

void	 vc2_write_ireg(struct newport_devconfig *, uint8_t, uint16_t);
uint16_t vc2_read_ireg(struct newport_devconfig *, uint8_t);
uint16_t vc2_read_ram(struct newport_devconfig *, uint16_t);
void	 vc2_write_ram(struct newport_devconfig *, uint16_t, uint16_t);
uint32_t xmap9_read(struct newport_devconfig *, int);
void	 xmap9_write(struct newport_devconfig *, int, uint8_t);
void	 xmap9_write_mode(struct newport_devconfig *, uint8_t, uint32_t);

void	newport_attach_common(struct newport_devconfig *,
	    struct gio_attach_args *);
int	newport_bitblt(struct newport_devconfig *, int, int, int, int, int, int,
	    int);
void	newport_cmap_setrgb(struct newport_devconfig *, int, uint8_t, uint8_t,
	    uint8_t);
int	newport_fill_rectangle(struct newport_devconfig *, int, int, int, int,
	    int);
void	newport_get_resolution(struct newport_devconfig *);
int	newport_init_screen(struct newport_devconfig *);
void	newport_setup_hw(struct newport_devconfig *);

static struct newport_devconfig newport_console_dc;

/**** Low-level hardware register groveling functions ****/
static __inline__ void
rex3_write(struct newport_devconfig *dc, bus_size_t rexreg, uint32_t val)
{
	bus_space_write_4(dc->dc_st, dc->dc_sh, NEWPORT_REX3_OFFSET + rexreg,
	    val);
}

static __inline__ void
rex3_write_go(struct newport_devconfig *dc, bus_size_t rexreg, uint32_t val)
{
	rex3_write(dc, rexreg + REX3_REG_GO, val);
}

static __inline__ uint32_t
rex3_read(struct newport_devconfig *dc, bus_size_t rexreg)
{
	return bus_space_read_4(dc->dc_st, dc->dc_sh, NEWPORT_REX3_OFFSET +
	    rexreg);
}

int
rex3_wait_gfifo(struct newport_devconfig *dc, const char *from)
{
	unsigned int iter;
	uint32_t rxstatus;

	for (iter = 100000; iter != 0; iter--) {
		rxstatus = rex3_read(dc, REX3_REG_STATUS);
		if ((rxstatus &
		    (REX3_STATUS_GFXBUSY | REX3_STATUS_FIFOLEVEL_MASK)) == 0)
			return 0;
	}

#ifdef DEBUG
	printf("%s: failed to idle, %05x\n", from, rxstatus);
#endif
	return EAGAIN;
}

void
vc2_write_ireg(struct newport_devconfig *dc, uint8_t ireg, uint16_t val)
{
	rex3_write(dc, REX3_REG_DCBMODE,
	    REX3_DCBMODE_DW_3 | REX3_DCBMODE_ENCRSINC |
	    (NEWPORT_DCBADDR_VC2 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (VC2_DCBCRS_INDEX << REX3_DCBMODE_DCBCRS_SHIFT) |
	    REX3_DCBMODE_ENASYNCACK | (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, (ireg << 24) | (val << 8));
}

uint16_t
vc2_read_ireg(struct newport_devconfig *dc, uint8_t ireg)
{
	rex3_write(dc, REX3_REG_DCBMODE,
	    REX3_DCBMODE_DW_1 | REX3_DCBMODE_ENCRSINC |
	    (NEWPORT_DCBADDR_VC2 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (VC2_DCBCRS_INDEX << REX3_DCBMODE_DCBCRS_SHIFT) |
	    REX3_DCBMODE_ENASYNCACK | (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, ireg << 24);

	rex3_write(dc, REX3_REG_DCBMODE,
	    REX3_DCBMODE_DW_2 | REX3_DCBMODE_ENCRSINC |
	    (NEWPORT_DCBADDR_VC2 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (VC2_DCBCRS_IREG << REX3_DCBMODE_DCBCRS_SHIFT) |
	    REX3_DCBMODE_ENASYNCACK | (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	return (uint16_t)(rex3_read(dc, REX3_REG_DCBDATA0) >> 16);
}

uint16_t
vc2_read_ram(struct newport_devconfig *dc, uint16_t addr)
{
	vc2_write_ireg(dc, VC2_IREG_RAM_ADDRESS, addr);

	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_2 |
	    (NEWPORT_DCBADDR_VC2 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (VC2_DCBCRS_RAM << REX3_DCBMODE_DCBCRS_SHIFT) |
	    REX3_DCBMODE_ENASYNCACK | (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	return (uint16_t)(rex3_read(dc, REX3_REG_DCBDATA0) >> 16);
}

#if 0
void
vc2_write_ram(struct newport_devconfig *dc, uint16_t addr, uint16_t val)
{
	vc2_write_ireg(dc, VC2_IREG_RAM_ADDRESS, addr);

	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_2 |
	    (NEWPORT_DCBADDR_VC2 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (VC2_DCBCRS_RAM << REX3_DCBMODE_DCBCRS_SHIFT) |
	    REX3_DCBMODE_ENASYNCACK | (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, val << 16);
}
#endif

uint32_t
xmap9_read(struct newport_devconfig *dc, int crs)
{
	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_1 |
		(NEWPORT_DCBADDR_XMAP_0 << REX3_DCBMODE_DCBADDR_SHIFT) |
		(crs << REX3_DCBMODE_DCBCRS_SHIFT) |
		(3 << REX3_DCBMODE_CSWIDTH_SHIFT) |
		(2 << REX3_DCBMODE_CSHOLD_SHIFT) |
		(1 << REX3_DCBMODE_CSSETUP_SHIFT));
	return rex3_read(dc, REX3_REG_DCBDATA0);
}

void
xmap9_write(struct newport_devconfig *dc, int crs, uint8_t val)
{
	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_1 |
	    (NEWPORT_DCBADDR_XMAP_BOTH << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (crs << REX3_DCBMODE_DCBCRS_SHIFT) |
	    (3 << REX3_DCBMODE_CSWIDTH_SHIFT) |
	    (2 << REX3_DCBMODE_CSHOLD_SHIFT) |
	    (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, val << 24);
}

void
xmap9_write_mode(struct newport_devconfig *dc, uint8_t index, uint32_t mode)
{
	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_4 |
	    (NEWPORT_DCBADDR_XMAP_BOTH << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (XMAP9_DCBCRS_MODE_SETUP << REX3_DCBMODE_DCBCRS_SHIFT) |
	    (3 << REX3_DCBMODE_CSWIDTH_SHIFT) |
	    (2 << REX3_DCBMODE_CSHOLD_SHIFT) |
	    (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, (index << 24) | mode);
}

/**** Helper functions ****/
int
newport_fill_rectangle(struct newport_devconfig *dc, int x1, int y1, int x2,
    int y2, int bg)
{
	struct rasops_info *ri = &dc->dc_ri;

	if (rex3_wait_gfifo(dc, __func__) != 0)
		return EAGAIN;

	rex3_write(dc, REX3_REG_DRAWMODE0, REX3_DRAWMODE0_OPCODE_DRAW |
	    REX3_DRAWMODE0_ADRMODE_BLOCK | REX3_DRAWMODE0_DOSETUP |
	    REX3_DRAWMODE0_STOPONX | REX3_DRAWMODE0_STOPONY);
	rex3_write(dc, REX3_REG_DRAWMODE1, REX3_DRAWMODE1_PLANES_CI |
	    REX3_DRAWMODE1_DD_DD8 | REX3_DRAWMODE1_RWPACKED |
	    REX3_DRAWMODE1_HD_HD8 | REX3_DRAWMODE1_COMPARE_LT |
	    REX3_DRAWMODE1_COMPARE_EQ | REX3_DRAWMODE1_COMPARE_GT |
	    (OPENGL_LOGIC_OP_COPY << REX3_DRAWMODE1_LOGICOP_SHIFT));
	rex3_write(dc, REX3_REG_WRMASK, 0xffffffff);
	rex3_write(dc, REX3_REG_COLORI, ri->ri_devcmap[bg] & 0xff);
	rex3_write(dc, REX3_REG_XYSTARTI, (x1 << REX3_XYSTARTI_XSHIFT) | y1);

	rex3_write_go(dc, REX3_REG_XYENDI, (x2 << REX3_XYENDI_XSHIFT) | y2);

	return 0;
}

int
newport_bitblt(struct newport_devconfig *dc, int xs, int ys, int xd,
    int yd, int wi, int he, int rop)
{
	int xe, ye;
	uint32_t tmp;

	if (rex3_wait_gfifo(dc, __func__) != 0)
		return EAGAIN;

	if (yd > ys) {
		/* need to copy bottom up */
		ye = ys;
		yd += he - 1;
		ys += he - 1;
	} else
		ye = ys + he - 1;

	if (xd > xs) {
		/* need to copy right to left */
		xe = xs;
		xd += wi - 1;
		xs += wi - 1;
	} else
		xe = xs + wi - 1;

	rex3_write(dc, REX3_REG_DRAWMODE0, REX3_DRAWMODE0_OPCODE_SCR2SCR |
	    REX3_DRAWMODE0_ADRMODE_BLOCK | REX3_DRAWMODE0_DOSETUP |
	    REX3_DRAWMODE0_STOPONX | REX3_DRAWMODE0_STOPONY);
	rex3_write(dc, REX3_REG_DRAWMODE1, REX3_DRAWMODE1_PLANES_CI |
	    REX3_DRAWMODE1_DD_DD8 | REX3_DRAWMODE1_RWPACKED |
	    REX3_DRAWMODE1_HD_HD8 | REX3_DRAWMODE1_COMPARE_LT |
	    REX3_DRAWMODE1_COMPARE_EQ | REX3_DRAWMODE1_COMPARE_GT |
	    (rop << REX3_DRAWMODE1_LOGICOP_SHIFT));
	rex3_write(dc, REX3_REG_XYSTARTI, (xs << REX3_XYSTARTI_XSHIFT) | ys);
	rex3_write(dc, REX3_REG_XYENDI, (xe << REX3_XYENDI_XSHIFT) | ye);

	tmp = (yd - ys) & 0xffff;
	tmp |= (xd - xs) << REX3_XYMOVE_XSHIFT;

	rex3_write_go(dc, REX3_REG_XYMOVE, tmp);

	return 0;
}

void
newport_cmap_setrgb(struct newport_devconfig *dc, int index, uint8_t r,
    uint8_t g, uint8_t b)
{
	rex3_write(dc, REX3_REG_DCBMODE,
	    REX3_DCBMODE_DW_2 | REX3_DCBMODE_ENCRSINC |
	    (NEWPORT_DCBADDR_CMAP_BOTH << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (CMAP_DCBCRS_ADDRESS_LOW << REX3_DCBMODE_DCBCRS_SHIFT) |
	    (1 << REX3_DCBMODE_CSWIDTH_SHIFT) |
	    (1 << REX3_DCBMODE_CSHOLD_SHIFT) |
	    (1 << REX3_DCBMODE_CSSETUP_SHIFT) | REX3_DCBMODE_SWAPENDIAN);

	rex3_write(dc, REX3_REG_DCBDATA0, index << 16);

	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_3 |
	    (NEWPORT_DCBADDR_CMAP_BOTH << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (CMAP_DCBCRS_PALETTE << REX3_DCBMODE_DCBCRS_SHIFT) |
	    (1 << REX3_DCBMODE_CSWIDTH_SHIFT) |
	    (1 << REX3_DCBMODE_CSHOLD_SHIFT) |
	    (1 << REX3_DCBMODE_CSSETUP_SHIFT));

	rex3_write(dc, REX3_REG_DCBDATA0, (r << 24) | (g << 16) | (b << 8));
}

void
newport_get_resolution(struct newport_devconfig *dc)
{
	uint16_t vep, lines;
	uint16_t linep, cols;
	uint16_t data;

	vep = vc2_read_ireg(dc, VC2_IREG_VIDEO_ENTRY);

	dc->dc_xres = 0;
	dc->dc_yres = 0;

	for (;;) {
		/* Iterate over runs in video timing table */

		cols = 0;

		linep = vc2_read_ram(dc, vep++);
		lines = vc2_read_ram(dc, vep++);

		if (lines == 0)
			break;

#define	VC2_SRUN_EOL	0x8000	/* end of line */
#define	VC2_SRUN_SBSC	0x0080	/* zero if SB/SC follows */
#define	VC2_SRUN_MASK	0x7f00
#define	VC2_SRUN_SHIFT	8
#define	VC2_SA_MASK	0x007f
#define	VC2_SA_SHIFT	0
#define	VC2_SA_VISIBLE	(1 << (14 % 7))
		do {
			/* Iterate over state runs in line sequence table */
			data = vc2_read_ram(dc, linep++);

			if ((((data & VC2_SA_MASK) >> VC2_SA_SHIFT) &
			    VC2_SA_VISIBLE) == 0)
				cols += (data & VC2_SRUN_MASK) >> VC2_SRUN_SHIFT;
			if ((data & VC2_SRUN_SBSC) == 0)
				data = vc2_read_ram(dc, linep++);
		} while ((data & VC2_SRUN_EOL) == 0);

		if (cols != 0) {
			cols <<= 1;	/* was in 2 pixels unit */
			if (cols > dc->dc_xres)
				dc->dc_xres = cols;
			dc->dc_yres += lines;
		}
	}
}

void
newport_setup_hw(struct newport_devconfig *dc)
{
	uint16_t tmp;
	int i;
	uint32_t scratch;

	/* Get various revisions */
	rex3_write(dc, REX3_REG_DCBMODE, REX3_DCBMODE_DW_1 |
	    (NEWPORT_DCBADDR_CMAP_0 << REX3_DCBMODE_DCBADDR_SHIFT) |
	    (CMAP_DCBCRS_REVISION << REX3_DCBMODE_DCBCRS_SHIFT) |
	    (1 << REX3_DCBMODE_CSWIDTH_SHIFT) |
	    (1 << REX3_DCBMODE_CSHOLD_SHIFT) |
	    (1 << REX3_DCBMODE_CSSETUP_SHIFT));
	scratch = rex3_read(dc, REX3_REG_DCBDATA0) >> 24;

	dc->dc_boardrev = (scratch >> 4) & 0x07;
	/* cmaprev = scratch & 0x07; */
	dc->dc_xmaprev = xmap9_read(dc, XMAP9_DCBCRS_REVISION) & 0x07;
	dc->dc_depth = ((dc->dc_boardrev > 1) && (scratch & 0x80)) ? 8 : 24;

	scratch = vc2_read_ireg(dc, VC2_IREG_CONFIG);
	dc->dc_vc2rev = (scratch & VC2_IREG_CONFIG_REVISION) >> 5;

	/* Setup VC2 to a known state */
	tmp = vc2_read_ireg(dc, VC2_IREG_CONTROL) & VC2_CONTROL_INTERLACE;
	vc2_write_ireg(dc, VC2_IREG_CONTROL, tmp |
	    VC2_CONTROL_DISPLAY_ENABLE | VC2_CONTROL_VTIMING_ENABLE |
	    VC2_CONTROL_DID_ENABLE | VC2_CONTROL_CURSORFUNC_ENABLE);

	/* Setup XMAP9s */
	xmap9_write(dc, XMAP9_DCBCRS_CONFIG,
	    XMAP9_CONFIG_8BIT_SYSTEM | XMAP9_CONFIG_RGBMAP_CI);

	xmap9_write(dc, XMAP9_DCBCRS_CURSOR_CMAP, 0);

	xmap9_write_mode(dc, 0,
	    XMAP9_MODE_GAMMA_BYPASS | XMAP9_MODE_PIXSIZE_8BPP);
	xmap9_write(dc, XMAP9_DCBCRS_MODE_SELECT, 0);

	/* Setup REX3 */
	rex3_write(dc, REX3_REG_XYWIN, (4096 << 16) | 4096);
	rex3_write(dc, REX3_REG_TOPSCAN, 0x3ff); /* XXX Why? XXX */

	/* Setup CMAP */
	for (i = 0; i < 256; i++)
		newport_cmap_setrgb(dc, i, rasops_cmap[i * 3],
		    rasops_cmap[i * 3 + 1], rasops_cmap[i * 3 + 2]);
}

/**** Attach routines ****/
int
newport_match(struct device *parent, void *vcf, void *aux)
{
	struct gio_attach_args *ga = aux;

	if (ga->ga_product != GIO_PRODUCT_FAKEID_NEWPORT)
		return 0;

	return 1;
}

void
newport_attach(struct device *parent, struct device *self, void *aux)
{
	struct newport_softc *sc = (struct newport_softc *)self;
	struct gio_attach_args *ga = aux;
	struct newport_devconfig *dc;
	struct wsemuldisplaydev_attach_args waa;
	const char *descr;
	extern struct consdev wsdisplay_cons;
	int fail = 0;

	if (cn_tab == &wsdisplay_cons &&
	    ga->ga_addr == newport_console_dc.dc_addr) {
		waa.console = 1;
		dc = &newport_console_dc;
		sc->sc_nscreens = 1;
	} else {
		waa.console = 0;
		dc = malloc(sizeof(struct newport_devconfig),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		newport_attach_common(dc, ga);
		if (newport_init_screen(dc) != 0)
			fail = 1;
	}
	sc->sc_dc = dc;
	dc->dc_sc = sc;

	descr = ga->ga_descr;
	if (descr == NULL || *descr == '\0')
		descr = "NG1";
	printf(": %s (board rev %d, xmap rev %d, vc2 rev %d)\n",
	    descr, dc->dc_boardrev, dc->dc_xmaprev, dc->dc_vc2rev);
	printf("%s: %dx%d %d-bit frame buffer\n",
	    self->dv_xname, dc->dc_xres, dc->dc_yres, dc->dc_depth);
#ifdef DEBUG
	printf("%s: REX3 config = %06x\n",
	    self->dv_xname, rex3_read(dc, REX3_REG_CONFIG));
#endif

	if (fail) {
		printf("%s: failed to initialize screen\n", self->dv_xname);
		free(dc, M_DEVBUF, 0);
		return;
	}

	sc->sc_scrlist[0] = &dc->dc_wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = sc->sc_scrlist;

	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &newport_accessops;
	waa.accesscookie = dc;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
newport_cnprobe(struct gio_attach_args *ga)
{
	return newport_match(NULL, NULL, ga);
}

int
newport_cnattach(struct gio_attach_args *ga)
{
	struct rasops_info *ri = &newport_console_dc.dc_ri;
	long defattr;
	int rc;

	newport_attach_common(&newport_console_dc, ga);
	rc = newport_init_screen(&newport_console_dc);
	if (rc != 0)
		return rc;

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&newport_console_dc.dc_wsd, ri, 0, 0, defattr);

	return 0;
}

void
newport_attach_common(struct newport_devconfig *dc, struct gio_attach_args *ga)
{
	dc->dc_addr = ga->ga_addr;
	dc->dc_st = ga->ga_iot;
	dc->dc_sh = ga->ga_ioh;

	newport_setup_hw(dc);
	newport_get_resolution(dc);
}

int
newport_init_screen(struct newport_devconfig *dc)
{
	struct rasops_info *ri = &dc->dc_ri;
	int rc;

	memset(ri, 0, sizeof(struct rasops_info));
	ri->ri_hw = dc;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;
	/* for the proper operation of rasops computations, pretend 8bpp */
	ri->ri_depth = 8;
	ri->ri_stride = dc->dc_xres;
	ri->ri_width = dc->dc_xres;
	ri->ri_height = dc->dc_yres;

	rasops_init(ri, 160, 160);

	ri->ri_do_cursor = newport_do_cursor;
	ri->ri_ops.copyrows = newport_copyrows;
	ri->ri_ops.eraserows = newport_eraserows;
	ri->ri_ops.copycols = newport_copycols;
	ri->ri_ops.erasecols = newport_erasecols;
	ri->ri_ops.putchar = newport_putchar;

	strlcpy(dc->dc_wsd.name, "std", sizeof(dc->dc_wsd.name));
	dc->dc_wsd.ncols = ri->ri_cols;
	dc->dc_wsd.nrows = ri->ri_rows;
	dc->dc_wsd.textops = &ri->ri_ops;
	dc->dc_wsd.fontwidth = ri->ri_font->fontwidth;
	dc->dc_wsd.fontheight = ri->ri_font->fontheight;
	dc->dc_wsd.capabilities = ri->ri_caps;

	rc = newport_fill_rectangle(dc, 0, 0, ri->ri_width - 1,
	    ri->ri_height - 1, WSCOL_BLACK);

#ifdef notyet
	dc->dc_mode = WSDISPLAYIO_MODE_EMUL;
#endif

	return rc;
}

/**** wsdisplay textops ****/

int
newport_do_cursor(struct rasops_info *ri)
{
	struct newport_devconfig *dc = ri->ri_hw;
	int x, y, w, h;

	w = ri->ri_font->fontwidth;
	h = ri->ri_font->fontheight;
	x = ri->ri_ccol * w + ri->ri_xorigin;
	y = ri->ri_crow * h + ri->ri_yorigin;

	return newport_bitblt(dc, x, y, x, y, w, h,
	    OPENGL_LOGIC_OP_COPY_INVERTED);
}

int
newport_putchar(void *c, int row, int col, u_int ch, long attr)
{
	struct rasops_info *ri = c;
	struct newport_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	uint8_t *bitmap;
	uint32_t pattern;
	int x = col * font->fontwidth + ri->ri_xorigin;
	int y = row * font->fontheight + ri->ri_yorigin;
	int i;
	int bg, fg, ul;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, &ul);

	if ((ch == ' ' || ch == 0) && ul == 0) {
		return newport_fill_rectangle(dc, x, y, x + font->fontwidth - 1,
		    y + font->fontheight - 1, bg);
	}

	if (rex3_wait_gfifo(dc, __func__) != 0)
		return EAGAIN;

	rex3_write(dc, REX3_REG_DRAWMODE0, REX3_DRAWMODE0_OPCODE_DRAW |
	    REX3_DRAWMODE0_ADRMODE_BLOCK | REX3_DRAWMODE0_STOPONX |
	    REX3_DRAWMODE0_ENZPATTERN | REX3_DRAWMODE0_ZPOPAQUE);

	rex3_write(dc, REX3_REG_DRAWMODE1, REX3_DRAWMODE1_PLANES_CI |
	    REX3_DRAWMODE1_DD_DD8 | REX3_DRAWMODE1_RWPACKED |
	    REX3_DRAWMODE1_HD_HD8 | REX3_DRAWMODE1_COMPARE_LT |
	    REX3_DRAWMODE1_COMPARE_EQ | REX3_DRAWMODE1_COMPARE_GT |
	    (OPENGL_LOGIC_OP_COPY << REX3_DRAWMODE1_LOGICOP_SHIFT));

	rex3_write(dc, REX3_REG_XYSTARTI, (x << REX3_XYSTARTI_XSHIFT) | y);
	rex3_write(dc, REX3_REG_XYENDI,
	    (x + font->fontwidth - 1) << REX3_XYENDI_XSHIFT);

	rex3_write(dc, REX3_REG_COLORI, ri->ri_devcmap[fg] & 0xff);
	rex3_write(dc, REX3_REG_COLORBACK, ri->ri_devcmap[bg] & 0xff);

	rex3_write(dc, REX3_REG_WRMASK, 0xffffffff);

	bitmap = (uint8_t *)font->data +
	    (ch - font->firstchar) * ri->ri_fontscale;
	if (font->fontwidth <= 8) {
		for (i = font->fontheight; i != 0; i--) {
			if (ul && i == 1)
				pattern = 0xff000000;
			else
				pattern = *bitmap << 24;
			rex3_write_go(dc, REX3_REG_ZPATTERN, pattern);
			bitmap += font->stride;
		}
	} else {
		for (i = font->fontheight; i != 0; i--) {
			if (ul && i == 1)
				pattern = 0xffff0000;
			else
				pattern = *(uint16_t *)bitmap << 16;
			rex3_write_go(dc, REX3_REG_ZPATTERN, pattern);
			bitmap += font->stride;
		}
	}

	return 0;
}

int
newport_copycols(void *c, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = c;
	struct newport_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int32_t xs, xd, y, width, height;

	xs = ri->ri_xorigin + font->fontwidth * srccol;
	xd = ri->ri_xorigin + font->fontwidth * dstcol;
	y = ri->ri_yorigin + font->fontheight * row;
	width = font->fontwidth * ncols;
	height = font->fontheight;
	return newport_bitblt(dc, xs, y, xd, y, width, height,
	    OPENGL_LOGIC_OP_COPY);
}

int
newport_erasecols(void *c, int row, int startcol, int ncols, long attr)
{
	struct rasops_info *ri = c;
	struct newport_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int sx, sy, dx, dy;
	int bg, fg;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	sx = ri->ri_xorigin + startcol * font->fontwidth;
	sy = ri->ri_yorigin + row * font->fontheight;
	dx = sx + ncols * font->fontwidth - 1;
	dy = sy + font->fontheight - 1;

	return newport_fill_rectangle(dc, sx, sy, dx, dy, bg);
}

int
newport_copyrows(void *c, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = c;
	struct newport_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int32_t x, ys, yd, width, height;

	x = ri->ri_xorigin;
	ys = ri->ri_yorigin + font->fontheight * srcrow;
	yd = ri->ri_yorigin + font->fontheight * dstrow;
	width = ri->ri_emuwidth;
	height = font->fontheight * nrows;

	return newport_bitblt(dc, x, ys, x, yd, width, height,
	    OPENGL_LOGIC_OP_COPY);
}

int
newport_eraserows(void *c, int startrow, int nrows, long attr)
{
	struct rasops_info *ri = c;
	struct newport_devconfig *dc = ri->ri_hw;
	struct wsdisplay_font *font = ri->ri_font;
	int bg, fg;

	ri->ri_ops.unpack_attr(ri, attr, &fg, &bg, NULL);

	if (nrows == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR)) {
		return newport_fill_rectangle(dc, 0, 0, ri->ri_width - 1,
		    ri->ri_height - 1, bg);
	}

	return newport_fill_rectangle(dc, ri->ri_xorigin,
	    ri->ri_yorigin + startrow * font->fontheight,
	    ri->ri_xorigin + ri->ri_emuwidth - 1,
	    ri->ri_yorigin + (startrow + nrows) * font->fontheight - 1, bg);
}

/**** wsdisplay accessops ****/

int
newport_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct newport_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;
	struct newport_softc *sc = dc->dc_sc;

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
newport_free_screen(void *v, void *cookie)
{
}

int
newport_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

int
newport_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct newport_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;
	struct wsdisplay_fbinfo *fb;
#ifdef notyet
	int nmode;
#endif

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_NEWPORT;
		break;
	case WSDISPLAYIO_GINFO:
		fb = (struct wsdisplay_fbinfo *)data;
		fb->width = ri->ri_width;
		fb->height = ri->ri_height;
		fb->depth = dc->dc_depth;	/* real depth */
		if (dc->dc_depth > 8)
			fb->cmsize = 0;
		else
			fb->cmsize = 1 << ri->ri_depth;
		break;
#ifdef notyet
	case WSDISPLAYIO_SMODE:
		nmode = *(int *)data;
		if (nmode != dc->dc_mode) {
			if (nmode == WSDISPLAYIO_MODE_EMUL) {
				if (rex3_wait_gfifo(dc, __func__) != 0)
					return EAGAIN;
				dc->dc_mode = nmode;
				newport_setup_hw(dc);
			} else
				dc->dc_mode = nmode;
		}
		break;
#endif
	default:
		return -1;
	}

	return 0;
}

paddr_t
newport_mmap(void *v, off_t offset, int prot)
{
	return -1;
}

int
newport_load_font(void *v, void *emulcookie, struct wsdisplay_font *font)
{
	struct newport_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_load_font(ri, emulcookie, font);
}

int
newport_list_font(void *v, struct wsdisplay_font *font)
{
	struct newport_devconfig *dc = v;
	struct rasops_info *ri = &dc->dc_ri;

	return rasops_list_font(ri, font);
}

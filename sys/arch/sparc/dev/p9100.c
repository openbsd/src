/*	$OpenBSD: p9100.c,v 1.30 2004/09/29 07:35:11 miod Exp $	*/

/*
 * Copyright (c) 2003, Miodrag Vallat.
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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

/*
 * color display (p9100) driver.
 * Initially based on cgthree.c and the NetBSD p9100 driver, then hacked
 * beyond recognition.
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
#include <machine/cpu.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/sbusvar.h>

#include <dev/ic/p9000.h>

#include "tctrl.h"
#if NTCTRL > 0
#include <sparc/dev/tctrlvar.h>
#endif

/* per-display variables */
struct p9100_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	rom_reg	sc_phys;	/* phys address description */
	volatile u_int8_t *sc_cmd;	/* command registers (dac, etc) */
	volatile u_int8_t *sc_ctl;	/* control registers (draw engine) */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	struct	intrhand sc_ih;
	int	sc_nscreens;
	u_int32_t	sc_junk;	/* throwaway value */
};

struct wsscreen_descr p9100_stdscreen = {
	"std",
};

const struct wsscreen_descr *p9100_scrlist[] = {
	&p9100_stdscreen,
};

struct wsscreen_list p9100_screenlist = {
	sizeof(p9100_scrlist) / sizeof(struct wsscreen_descr *),
	    p9100_scrlist
};

int	p9100_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	p9100_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	p9100_free_screen(void *, void *);
int	p9100_show_screen(void *, void *, int, void (*cb)(void *, int, int),
	    void *);
paddr_t	p9100_mmap(void *, off_t, int);
static __inline__ void p9100_loadcmap_deferred(struct p9100_softc *,
    u_int, u_int);
void	p9100_loadcmap_immediate(struct p9100_softc *, u_int, u_int);
void	p9100_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void	p9100_burner(void *, u_int, u_int);
int	p9100_intr(void *);

struct wsdisplay_accessops p9100_accessops = {
	p9100_ioctl,
	p9100_mmap,
	p9100_alloc_screen,
	p9100_free_screen,
	p9100_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	p9100_burner,
};

void	p9100_ras_init(struct p9100_softc *);
void	p9100_ras_copycols(void *, int, int, int, int);
void	p9100_ras_copyrows(void *, int, int, int);
void	p9100_ras_do_cursor(struct rasops_info *);
void	p9100_ras_erasecols(void *, int, int, int, long int);
void	p9100_ras_eraserows(void *, int, int, long int);

int	p9100match(struct device *, void *, void *);
void	p9100attach(struct device *, struct device *, void *);

struct cfattach pnozz_ca = {
	sizeof (struct p9100_softc), p9100match, p9100attach
};

struct cfdriver pnozz_cd = {
	NULL, "pnozz", DV_DULL
};

/*
 * SBus registers mappings
 */
#define	P9100_NREG	3
#define	P9100_REG_CTL	0
#define	P9100_REG_CMD	1
#define	P9100_REG_VRAM	2

/*
 * IBM RGB525 RAMDAC registers
 */

/* Palette write address */
#define	IBM525_WRADDR			0
/* Palette data */
#define	IBM525_DATA			1
/* Pixel mask */
#define	IBM525_PIXMASK			2
/* Read palette address */
#define	IBM525_RDADDR			3
/* Register index low */
#define	IBM525_IDXLOW			4
/* Register index high */
#define	IBM525_IDXHIGH			5
/* Register data */
#define	IBM525_REGDATA			6
/* Index control */
#define	IBM525_IDXCONTROL		7

/*
 * P9100 read/write macros
 */

#define	P9100_READ_CTL(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg))
#define	P9100_READ_CMD(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg))
#define	P9100_READ_RAMDAC(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_ctl + P9100_RAMDAC_REGISTER(reg))

#define	P9100_WRITE_CTL(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg)) = (value)
#define	P9100_WRITE_CMD(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg)) = (value)
#define	P9100_WRITE_RAMDAC(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_ctl + P9100_RAMDAC_REGISTER(reg)) = \
	    (value)

/*
 * On the Tadpole, the first write to a register group is ignored until
 * the proper group address is latched, which can be done by reading from the
 * register group first.
 *
 * Register groups are 0x80 bytes long (i.e. it is necessary to force a read
 * when writing to an address which upper 25 bit differ from the previous
 * read or write operation).
 *
 * This is specific to the Tadpole design, and not a limitation of the
 * Power 9100 hardware.
 */
#define	P9100_SELECT_SCR(sc) \
	(sc)->sc_junk = P9100_READ_CTL(sc, P9000_SYSTEM_CONFIG)
#define	P9100_SELECT_VCR(sc) \
	(sc)->sc_junk = P9100_READ_CTL(sc, P9000_HCR)
#define	P9100_SELECT_VRAM(sc) \
	(sc)->sc_junk = P9100_READ_CTL(sc, P9000_MCR)
#define	P9100_SELECT_DAC(sc) \
	(sc)->sc_junk = P9100_READ_CTL(sc, P9100_RAMDAC_REGISTER(0))
#define	P9100_SELECT_PE(sc) \
	(sc)->sc_junk = P9100_READ_CMD(sc, P9000_PE_STATUS)
#define	P9100_SELECT_DE_LOW(sc)	\
	(sc)->sc_junk = P9100_READ_CMD(sc, P9000_DE_FG_COLOR)
#define	P9100_SELECT_DE_HIGH(sc) \
	(sc)->sc_junk = P9100_READ_CMD(sc, P9000_DE_PATTERN(0))
#define	P9100_SELECT_COORD(sc,field) \
	(sc)->sc_junk = P9100_READ_CMD(sc, field)

/*
 * For some reason, every write to a DAC register needs to be followed by a
 * read from the ``free fifo number'' register, supposedly to have the write
 * take effect faster...
 */
#define	P9100_FLUSH_DAC(sc) \
	do { \
		P9100_SELECT_VRAM(sc); \
		(sc)->sc_junk = P9100_READ_CTL(sc, P9100_FREE_FIFO); \
	} while (0)

int
p9100match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp("p9100", ra->ra_name))
		return (0);

	return (1);
}

/*
 * Attach a display.
 */
void
p9100attach(struct device *parent, struct device *self, void *args)
{
	struct p9100_softc *sc = (struct p9100_softc *)self;
	struct confargs *ca = args;
	struct wsemuldisplaydev_attach_args waa;
	int node, row, scr;
	int isconsole, fb_depth;

#ifdef DIAGNOSTIC
	if (ca->ca_ra.ra_nreg < P9100_NREG) {
		printf(": expected %d registers, got only %d\n",
		    P9100_NREG, ca->ca_ra.ra_nreg);
		return;
	}
#endif

	sc->sc_phys = ca->ca_ra.ra_reg[P9100_REG_VRAM];

	sc->sc_ctl = mapiodev(&(ca->ca_ra.ra_reg[P9100_REG_CTL]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);
	sc->sc_cmd = mapiodev(&(ca->ca_ra.ra_reg[P9100_REG_CMD]), 0,
	    ca->ca_ra.ra_reg[1].rr_len);

	node = ca->ca_ra.ra_node;
	isconsole = node == fbnode;

	P9100_SELECT_SCR(sc);
	scr = P9100_READ_CTL(sc, P9000_SYSTEM_CONFIG);
	switch (scr & SCR_PIXEL_MASK) {
	case SCR_PIXEL_32BPP:
		fb_depth = 32;
		break;
	case SCR_PIXEL_24BPP:
		fb_depth = 24;
		break;
	case SCR_PIXEL_16BPP:
		fb_depth = 16;
		break;
	default:
#ifdef DIAGNOSTIC
		printf(": unknown color depth code 0x%x, assuming 8\n%s",
		    scr & SCR_PIXEL_MASK, self->dv_xname);
#endif
	case SCR_PIXEL_8BPP:
		fb_depth = 8;
		break;
	}
	fb_setsize(&sc->sc_sunfb, fb_depth, 800, 600, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&sc->sc_phys, 0,
	    round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	printf(": rev %x, %dx%d, depth %d\n", scr & SCR_ID_MASK,
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height,
	    sc->sc_sunfb.sf_depth);

	sc->sc_ih.ih_fun = p9100_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, IPL_FB,
	    self->dv_xname);

	/* Disable frame buffer interrupts */
	P9100_SELECT_SCR(sc);
	P9100_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE, IER_MASTER_ENABLE | 0);

	/*
	 * If the framebuffer width is under 1024x768, we will switch from the
	 * PROM font to the more adequate 8x16 font here.
	 * However, we need to adjust two things in this case:
	 * - the display row should be overrided from the current PROM metrics,
	 *   to prevent us from overwriting the last few lines of text.
	 * - if the 80x34 screen would make a large margin appear around it,
	 *   choose to clear the screen rather than keeping old prom output in
	 *   the margins.
	 * XXX there should be a rasops "clear margins" feature
	 */
	fbwscons_init(&sc->sc_sunfb,
	    isconsole && (sc->sc_sunfb.sf_width >= 1024) ? 0 : RI_CLEAR);
	fbwscons_setcolormap(&sc->sc_sunfb, p9100_setcolor);

	/*
	 * Plug-in accelerated console operations if we can.
	 */
	if (sc->sc_sunfb.sf_depth == 8)
		p9100_ras_init(sc);

	p9100_stdscreen.capabilities = sc->sc_sunfb.sf_ro.ri_caps;
	p9100_stdscreen.nrows = sc->sc_sunfb.sf_ro.ri_rows;
	p9100_stdscreen.ncols = sc->sc_sunfb.sf_ro.ri_cols;
	p9100_stdscreen.textops = &sc->sc_sunfb.sf_ro.ri_ops;

	sbus_establish(&sc->sc_sd, &sc->sc_sunfb.sf_dev);

	/* enable video */
	p9100_burner(sc, 1, 0);

	if (isconsole) {
		if (sc->sc_sunfb.sf_width < 1024)
			row = 0;	/* screen has been cleared above */
		else
			row = -1;

		fbwscons_console_init(&sc->sc_sunfb, &p9100_stdscreen, row,
		    p9100_burner);
	}

	waa.console = isconsole;
	waa.scrdata = &p9100_screenlist;
	waa.accessops = &p9100_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);
}

int
p9100_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct p9100_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
#if NTCTRL > 0
	struct wsdisplay_param *dp;
#endif
	int error;

	switch (cmd) {

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SB_P9100;
		break;

	case WSDISPLAYIO_SMODE:
		/* Restore proper acceleration state upon leaving X11 */
		if (*(u_int *)data == WSDISPLAYIO_MODE_EMUL &&
		    sc->sc_sunfb.sf_depth == 8) {
			p9100_ras_init(sc);
		}
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = sc->sc_sunfb.sf_height;
		wdf->width  = sc->sc_sunfb.sf_width;
		wdf->depth  = sc->sc_sunfb.sf_depth;
		wdf->cmsize = 256;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_sunfb.sf_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = bt_getcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		break;

	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = bt_putcmap(&sc->sc_cmap, cm);
		if (error)
			return (error);
		p9100_loadcmap_deferred(sc, cm->index, cm->count);
		break;

#if NTCTRL > 0
	case WSDISPLAYIO_GETPARAM:
		dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			dp->min = 0;
			dp->max = 255;
			dp->curval = tadpole_get_brightness();
			break;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			dp->min = 0;
			dp->max = 1;
			dp->curval = tadpole_get_video() & TV_ON;
			break;
		default:
			return (-1);
		}
		break;

	case WSDISPLAYIO_SETPARAM:
		dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			tadpole_set_brightness(dp->curval);
			break;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			tadpole_set_video(dp->curval);
			break;
		default:
			return (-1);
		}
		break;
#endif	/* NTCTRL > 0 */

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return (-1);	/* not supported yet */
	}

	return (0);
}

int
p9100_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct p9100_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_sunfb.sf_ro;
	*curyp = 0;
	*curxp = 0;
	if (sc->sc_sunfb.sf_depth == 8) {
		sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
		    WSCOL_BLACK, WSCOL_WHITE, WSATTR_WSCOLORS, attrp);
	} else {
		sc->sc_sunfb.sf_ro.ri_ops.alloc_attr(&sc->sc_sunfb.sf_ro,
		    0, 0, 0, attrp);
	}
	sc->sc_nscreens++;
	return (0);
}

void
p9100_free_screen(void *v, void *cookie)
{
	struct p9100_softc *sc = v;

	sc->sc_nscreens--;
}

int
p9100_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 */
paddr_t
p9100_mmap(void *v, off_t offset, int prot)
{
	struct p9100_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, offset) | PMAP_NC);
	}

	return (-1);
}

void
p9100_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct p9100_softc *sc = v;
	union bt_cmap *bcm = &sc->sc_cmap;

	bcm->cm_map[index][0] = r;
	bcm->cm_map[index][1] = g;
	bcm->cm_map[index][2] = b;
	p9100_loadcmap_immediate(sc, index, 1);
}

void
p9100_loadcmap_immediate(struct p9100_softc *sc, u_int start, u_int ncolors)
{
	u_char *p;

	P9100_SELECT_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_WRADDR, start << 16);
	P9100_FLUSH_DAC(sc);

	for (p = sc->sc_cmap.cm_map[start], ncolors *= 3; ncolors-- > 0; p++) {
		P9100_SELECT_DAC(sc);
		P9100_WRITE_RAMDAC(sc, IBM525_DATA, (*p) << 16);
		P9100_FLUSH_DAC(sc);
	}
}

static __inline__ void
p9100_loadcmap_deferred(struct p9100_softc *sc, u_int start, u_int ncolors)
{
	/* Schedule an interrupt for next retrace */
	P9100_SELECT_SCR(sc);
	P9100_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE,
	    IER_MASTER_ENABLE | IER_MASTER_INTERRUPT |
	    IER_VBLANK_ENABLE | IER_VBLANK_INTERRUPT);
}

void
p9100_burner(void *v, u_int on, u_int flags)
{
	struct p9100_softc *sc = v;
	u_int32_t vcr;
	int s;

	s = splhigh();
	P9100_SELECT_VCR(sc);
	vcr = P9100_READ_CTL(sc, P9000_SRTC1);
	if (on)
		vcr |= SRTC1_VIDEN;
	else
		vcr &= ~SRTC1_VIDEN;
	P9100_WRITE_CTL(sc, P9000_SRTC1, vcr);
#if NTCTRL > 0
	tadpole_set_video(on);
#endif
	splx(s);
}

int
p9100_intr(void *v)
{
	struct p9100_softc *sc = v;

	if (P9100_READ_CTL(sc, P9000_INTERRUPT) & IER_VBLANK_INTERRUPT) {
		p9100_loadcmap_immediate(sc, 0, 256);

		/* Disable further interrupts now */
		/* P9100_SELECT_SCR(sc); */
		P9100_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE,
		    IER_MASTER_ENABLE | 0);

		return (1);
	}

	return (0);
}

/*
 * Accelerated text console code
 */

static __inline__ void p9100_drain(struct p9100_softc *);

static __inline__ void
p9100_drain(struct p9100_softc *sc)
{
	while (P9100_READ_CMD(sc, P9000_PE_STATUS) &
	    (STATUS_QUAD_BUSY | STATUS_BLIT_BUSY));
}

void
p9100_ras_init(struct p9100_softc *sc)
{
	sc->sc_sunfb.sf_ro.ri_ops.copycols = p9100_ras_copycols;
	sc->sc_sunfb.sf_ro.ri_ops.copyrows = p9100_ras_copyrows;
#if NTCTRL > 0
	if (tadpole_get_video() & TV_ACCEL) {
		sc->sc_sunfb.sf_ro.ri_ops.erasecols = p9100_ras_erasecols;
		sc->sc_sunfb.sf_ro.ri_ops.eraserows = p9100_ras_eraserows;
		sc->sc_sunfb.sf_ro.ri_do_cursor = p9100_ras_do_cursor;
	}
#endif
	/*
	 * Setup safe defaults for the parameter and drawing engines, in
	 * order to minimize the operations to do for ri_ops.
	 */

	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_DRAWMODE,
	    DM_PICK_CONTROL | 0 | DM_BUFFER_CONTROL | DM_BUFFER_ENABLE0);

	P9100_WRITE_CMD(sc, P9000_DE_PATTERN_ORIGIN_X, 0);
	P9100_WRITE_CMD(sc, P9000_DE_PATTERN_ORIGIN_Y, 0);
	/* enable all planes */
	P9100_WRITE_CMD(sc, P9000_DE_PLANEMASK, 0xffffffff);

	/* Unclip */
	P9100_WRITE_CMD(sc, P9000_DE_WINMIN, 0);
	P9100_WRITE_CMD(sc, P9000_DE_WINMAX,
	    P9000_COORDS(sc->sc_sunfb.sf_width - 1, sc->sc_sunfb.sf_height - 1));

	P9100_SELECT_DE_HIGH(sc);
	P9100_WRITE_CMD(sc, P9100_DE_B_WINMIN, 0);
	P9100_WRITE_CMD(sc, P9100_DE_B_WINMAX,
	    P9000_COORDS(sc->sc_sunfb.sf_width - 1, sc->sc_sunfb.sf_height - 1));

	P9100_SELECT_PE(sc);
	P9100_WRITE_CMD(sc, P9000_PE_WINOFFSET, 0);
	P9100_WRITE_CMD(sc, P9000_PE_INDEX, 0);
	P9100_WRITE_CMD(sc, P9000_PE_WINMIN, 0);
	P9100_WRITE_CMD(sc, P9000_PE_WINMAX,
	    P9000_COORDS(sc->sc_sunfb.sf_width - 1, sc->sc_sunfb.sf_height - 1));
}

void
p9100_ras_copycols(void *v, int row, int src, int dst, int n)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;

	n *= ri->ri_font->fontwidth;
	n--;
	src *= ri->ri_font->fontwidth;
	src += ri->ri_xorigin;
	dst *= ri->ri_font->fontwidth;
	dst += ri->ri_xorigin;
	row *= ri->ri_font->fontheight;
	row += ri->ri_yorigin;

	p9100_drain(sc);
	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9100_RASTER_SRC & P9100_RASTER_MASK);

	P9100_SELECT_COORD(sc, P9000_DC_COORD(0));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(0) + P9000_COORD_XY,
	    P9000_COORDS(src, row));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(1) + P9000_COORD_XY,
	    P9000_COORDS(src + n, row + ri->ri_font->fontheight - 1));
	P9100_SELECT_COORD(sc, P9000_DC_COORD(2));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(2) + P9000_COORD_XY,
	    P9000_COORDS(dst, row));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(3) + P9000_COORD_XY,
	    P9000_COORDS(dst + n, row + ri->ri_font->fontheight - 1));

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_BLIT);

	p9100_drain(sc);
}

void
p9100_ras_copyrows(void *v, int src, int dst, int n)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;

	n *= ri->ri_font->fontheight;
	n--;
	src *= ri->ri_font->fontheight;
	src += ri->ri_yorigin;
	dst *= ri->ri_font->fontheight;
	dst += ri->ri_yorigin;

	p9100_drain(sc);
	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9100_RASTER_SRC & P9100_RASTER_MASK);

	P9100_SELECT_COORD(sc, P9000_DC_COORD(0));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(0) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin, src));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(1) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth - 1, src + n));
	P9100_SELECT_COORD(sc, P9000_DC_COORD(2));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(2) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin, dst));
	P9100_WRITE_CMD(sc, P9000_DC_COORD(3) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth - 1, dst + n));

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_BLIT);

	p9100_drain(sc);
}

void
p9100_ras_erasecols(void *v, int row, int col, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;
	int fg, bg;

	rasops_unpack_attr(attr, &fg, &bg, NULL);

	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	col += ri->ri_xorigin;
	row *= ri->ri_font->fontheight;
	row += ri->ri_yorigin;

	p9100_drain(sc);
	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9100_RASTER_PATTERN & P9100_RASTER_MASK);
	P9100_WRITE_CMD(sc, P9100_DE_COLOR0, P9100_COLOR8(bg));

	P9100_SELECT_COORD(sc, P9000_LC_RECT);
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col, row));
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col + n, row + ri->ri_font->fontheight));

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_QUAD);

	p9100_drain(sc);
}

void
p9100_ras_eraserows(void *v, int row, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;
	int fg, bg;

	rasops_unpack_attr(attr, &fg, &bg, NULL);

	p9100_drain(sc);
	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9100_RASTER_PATTERN & P9100_RASTER_MASK);
	P9100_WRITE_CMD(sc, P9100_DE_COLOR0, P9100_COLOR8(bg));

	P9100_SELECT_COORD(sc, P9000_LC_RECT);
	if (n == ri->ri_rows && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(0, 0));
		P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_width, ri->ri_height));
	} else {
		n *= ri->ri_font->fontheight;
		row *= ri->ri_font->fontheight;
		row += ri->ri_yorigin;

		P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_xorigin, row));
		P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth, row + n));
	}

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_QUAD);

	p9100_drain(sc);
}

void
p9100_ras_do_cursor(struct rasops_info *ri)
{
	struct p9100_softc *sc = ri->ri_hw;
	int row, col;

	row = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;
	col = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;

	p9100_drain(sc);

	P9100_SELECT_DE_LOW(sc);
	P9100_WRITE_CMD(sc, P9000_DE_RASTER,
	    (P9100_RASTER_PATTERN ^ P9100_RASTER_DST) & P9100_RASTER_MASK);
	P9100_WRITE_CMD(sc, P9100_DE_COLOR0, P9100_COLOR8(WSCOL_BLACK));

	P9100_SELECT_COORD(sc, P9000_LC_RECT);
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col, row));
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col + ri->ri_font->fontwidth,
	        row + ri->ri_font->fontheight));

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_QUAD);

	p9100_drain(sc);
}

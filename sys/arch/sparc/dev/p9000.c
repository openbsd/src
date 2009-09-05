/*	$OpenBSD: p9000.c,v 1.23 2009/09/05 14:09:35 miod Exp $	*/

/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * Driver for the Tadpole SPARCbook 3 on-board display.
 * Heavily based on the p9100 driver.
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
#include <dev/rasops/rasops.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/bt445reg.h>
#include <sparc/dev/bt445var.h>
#include <sparc/dev/sbusvar.h>

#include <dev/ic/p9000.h>

#include "tctrl.h"
#if NTCTRL > 0
#include <sparc/dev/tctrlvar.h>
#endif

/* per-display variables */
struct p9000_softc {
	struct	sunfb sc_sunfb;		/* common base part */
	struct	rom_reg	sc_phys;	/* phys address description */
	volatile u_int8_t *sc_cmd;	/* command registers (dac, etc) */
	volatile u_int8_t *sc_ctl;	/* control registers (draw engine) */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	volatile u_int8_t *sc_ramdac;	/* BT445 registers */
	struct	intrhand sc_ih;
	u_int32_t	sc_junk;	/* throwaway value */
};

int	p9000_ioctl(void *, u_long, caddr_t, int, struct proc *);
static __inline__
void	p9000_loadcmap_deferred(struct p9000_softc *, u_int, u_int);
void	p9000_loadcmap_immediate(struct p9000_softc *, u_int, u_int);
paddr_t	p9000_mmap(void *, off_t, int);
void	p9000_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
void	p9000_burner(void *, u_int, u_int);
int	p9000_intr(void *);

struct wsdisplay_accessops p9000_accessops = {
	p9000_ioctl,
	p9000_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	p9000_burner,
	NULL	/* pollc */
};

int	p9000_ras_copycols(void *, int, int, int, int);
int	p9000_ras_copyrows(void *, int, int, int);
int	p9000_ras_do_cursor(struct rasops_info *);
int	p9000_ras_erasecols(void *, int, int, int, long int);
int	p9000_ras_eraserows(void *, int, int, long int);
void	p9000_ras_init(struct p9000_softc *);

int	p9000match(struct device *, void *, void *);
void	p9000attach(struct device *, struct device *, void *);

struct cfattach pninek_ca = {
	sizeof (struct p9000_softc), p9000match, p9000attach
};

struct cfdriver pninek_cd = {
	NULL, "pninek", DV_DULL
};

/*
 * SBus registers mappings
 */
#define	P9000_NREG	5	/* actually, 7 total */
#define	P9000_REG_CTL	0
#define	P9000_REG_CMD	1
#define	P9000_REG_VRAM	4

/*
 * P9000 read/write macros
 */

#define	P9000_READ_CTL(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg))
#define	P9000_READ_CMD(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg))

#define	P9000_WRITE_CTL(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg)) = (value)
#define	P9000_WRITE_CMD(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg)) = (value)

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
 * Power 9000 hardware.
 */
#define	P9000_SELECT_SCR(sc) \
	(sc)->sc_junk = P9000_READ_CTL(sc, P9000_SYSTEM_CONFIG)
#define	P9000_SELECT_VCR(sc) \
	(sc)->sc_junk = P9000_READ_CTL(sc, P9000_HCR)
#define	P9000_SELECT_VRAM(sc) \
	(sc)->sc_junk = P9000_READ_CTL(sc, P9000_MCR)
#define	P9000_SELECT_PE(sc) \
	(sc)->sc_junk = P9000_READ_CMD(sc, P9000_PE_STATUS)
#define	P9000_SELECT_DE_LOW(sc)	\
	(sc)->sc_junk = P9000_READ_CMD(sc, P9000_DE_FG_COLOR)
#define	P9000_SELECT_DE_HIGH(sc) \
	(sc)->sc_junk = P9000_READ_CMD(sc, P9000_DE_PATTERN(0))
#define	P9000_SELECT_COORD(sc,field) \
	(sc)->sc_junk = P9000_READ_CMD(sc, field)


int
p9000match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp("p9000", ra->ra_name))
		return (0);

	/*
	 * If this is not the console device, chances are the
	 * frame buffer is not completely initialized, and access
	 * to some of its control registers could hang (this is
	 * the case on p9100). Until this can be verified, do
	 * not attach if console is on serial.
	 */
	if (ra->ra_node != fbnode)
		return (0);

	return (1);
}

void
p9000attach(struct device *parent, struct device *self, void *args)
{
	struct p9000_softc *sc = (struct p9000_softc *)self;
	struct confargs *ca = args;
	int node, pri, isconsole, scr;
	struct device *btdev;
	extern struct cfdriver btcham_cd;

	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);

#ifdef DIAGNOSTIC
	if (ca->ca_ra.ra_nreg < P9000_NREG) {
		printf(": expected %d registers, got only %d\n",
		    P9000_NREG, ca->ca_ra.ra_nreg);
		return;
	}
#endif

	/*
	 * Find the RAMDAC device. It should have attached before, since it
	 * attaches at obio. If, for some reason, it did not, it's not worth
	 * going any further.
	 *
	 * We rely upon the PROM to properly initialize the RAMDAC in a safe
	 * mode.
	 */
	btdev = btcham_cd.cd_ndevs != 0 ? btcham_cd.cd_devs[0] : NULL;
	if (btdev != NULL)
		sc->sc_ramdac = ((struct bt445_softc *)btdev)->sc_regs;

	if (sc->sc_ramdac == NULL) {
		printf(": bt445 did not attach previously\n");
		return;
	}

	sc->sc_phys = ca->ca_ra.ra_reg[P9000_REG_VRAM];

	sc->sc_ctl = mapiodev(&(ca->ca_ra.ra_reg[P9000_REG_CTL]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);
	sc->sc_cmd = mapiodev(&(ca->ca_ra.ra_reg[P9000_REG_CMD]), 0,
	    ca->ca_ra.ra_reg[1].rr_len);

	node = ca->ca_ra.ra_node;
	isconsole = node == fbnode;

	fb_setsize(&sc->sc_sunfb, 8, 640, 480, node, ca->ca_bustype);
	sc->sc_sunfb.sf_ro.ri_bits = mapiodev(&sc->sc_phys, 0,
	    round_page(sc->sc_sunfb.sf_fbsize));
	sc->sc_sunfb.sf_ro.ri_hw = sc;

	P9000_SELECT_SCR(sc);
	scr = P9000_READ_CTL(sc, P9000_SYSTEM_CONFIG);

	printf(": rev %x, %dx%d\n", scr & SCR_ID_MASK,
	    sc->sc_sunfb.sf_width, sc->sc_sunfb.sf_height);

	/* Disable frame buffer interrupts */
	P9000_SELECT_SCR(sc);
	P9000_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE, IER_MASTER_ENABLE | 0);

	sc->sc_ih.ih_fun = p9000_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_FB, self->dv_xname);

	fbwscons_init(&sc->sc_sunfb, isconsole);
	fbwscons_setcolormap(&sc->sc_sunfb, p9000_setcolor);

	/*
	 * Plug-in accelerated console operations.
	 */
	if (sc->sc_sunfb.sf_dev.dv_cfdata->cf_flags != 0)
		p9000_ras_init(sc);

	/* enable video */
	p9000_burner(sc, 1, 0);

	if (isconsole)
		fbwscons_console_init(&sc->sc_sunfb, -1);

	fbwscons_attach(&sc->sc_sunfb, &p9000_accessops, isconsole);
}

int
p9000_ioctl(void *v, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct p9000_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
#if NTCTRL > 0
	struct wsdisplay_param *dp;
#endif
	int error;

	switch (cmd) {

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SB_P9000;
		break;

	case WSDISPLAYIO_SMODE:
		/* Restore proper acceleration state upon leaving X11 */
		if (*(u_int *)data == WSDISPLAYIO_MODE_EMUL) {
			if (sc->sc_sunfb.sf_dev.dv_cfdata->cf_flags != 0)
				p9000_ras_init(sc);
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
		p9000_loadcmap_deferred(sc, cm->index, cm->count);
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
			dp->curval = tadpole_get_video() & TV_ON ? 1 : 0;
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
		break;

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

paddr_t
p9000_mmap(void *v, off_t offset, int prot)
{
	struct p9000_softc *sc = v;

	if (offset & PGOFSET)
		return (-1);

	if (offset >= 0 && offset < sc->sc_sunfb.sf_fbsize) {
		return (REG2PHYS(&sc->sc_phys, offset) | PMAP_NC);
	}

	return (-1);
}

void
p9000_setcolor(void *v, u_int index, u_int8_t r, u_int8_t g, u_int8_t b)
{
	struct p9000_softc *sc = v;
	union bt_cmap *bcm = &sc->sc_cmap;

	bcm->cm_map[index][0] = r;
	bcm->cm_map[index][1] = g;
	bcm->cm_map[index][2] = b;
	p9000_loadcmap_immediate(sc, index, 1);
}

void
p9000_loadcmap_immediate(struct p9000_softc *sc, u_int start, u_int ncolors)
{
	sc->sc_ramdac[BT445_ADDRESS] = start;
	for (ncolors += start; start < ncolors; start++) {
		sc->sc_ramdac[BT445_PALDATA] = sc->sc_cmap.cm_map[start][0];
		sc->sc_ramdac[BT445_PALDATA] = sc->sc_cmap.cm_map[start][1];
		sc->sc_ramdac[BT445_PALDATA] = sc->sc_cmap.cm_map[start][2];
	}
}

static __inline__ void
p9000_loadcmap_deferred(struct p9000_softc *sc, u_int start, u_int ncolors)
{
	/* Schedule an interrupt for next retrace */
	P9000_SELECT_SCR(sc);
	P9000_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE,
	    IER_MASTER_ENABLE | IER_MASTER_INTERRUPT |
	    IER_VBLANK_ENABLE | IER_VBLANK_INTERRUPT);
}

void
p9000_burner(void *v, u_int on, u_int flags)
{
	struct p9000_softc *sc = v;
	u_int32_t vcr;
	int s;

	s = splhigh();
	P9000_SELECT_VCR(sc);
	vcr = P9000_READ_CTL(sc, P9000_SRTC1);
	if (on)
		vcr |= SRTC1_VIDEN;
	else
		vcr &= ~SRTC1_VIDEN;
	P9000_WRITE_CTL(sc, P9000_SRTC1, vcr);
#if NTCTRL > 0
	tadpole_set_video(on);
#endif
	splx(s);
}

int
p9000_intr(void *v)
{
	struct p9000_softc *sc = v;

	if (P9000_READ_CTL(sc, P9000_INTERRUPT) & IER_VBLANK_INTERRUPT) {
		p9000_loadcmap_immediate(sc, 0, 256);

		/* Disable further interrupts now */
		/* P9000_SELECT_SCR(sc); */
		P9000_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE,
		    IER_MASTER_ENABLE | 0);

		/* Clear interrupt condition */
		P9000_WRITE_CTL(sc, P9000_INTERRUPT,
		    IER_VBLANK_ENABLE | 0);

		return (1);
	}

	return (0);
}

/*
 * Accelerated text console code
 */

static int p9000_drain(struct p9000_softc *);

static int
p9000_drain(struct p9000_softc *sc)
{
	u_int i;

	for (i = 10000; i != 0; i--) {
		if ((P9000_READ_CMD(sc, P9000_PE_STATUS) &
		    (STATUS_QUAD_BUSY | STATUS_BLIT_BUSY)) == 0)
			break;
	}

	return (i);
}

void
p9000_ras_init(struct p9000_softc *sc)
{

	if (p9000_drain(sc) == 0)
		return;

	sc->sc_sunfb.sf_ro.ri_ops.copycols = p9000_ras_copycols;
	sc->sc_sunfb.sf_ro.ri_ops.copyrows = p9000_ras_copyrows;
	sc->sc_sunfb.sf_ro.ri_ops.erasecols = p9000_ras_erasecols;
	sc->sc_sunfb.sf_ro.ri_ops.eraserows = p9000_ras_eraserows;
	sc->sc_sunfb.sf_ro.ri_do_cursor = p9000_ras_do_cursor;

	/*
	 * Setup safe defaults for the parameter and drawing engines, in
	 * order to minimize the operations to do for ri_ops.
	 */

	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_DRAWMODE,
	    DM_PICK_CONTROL | 0 | DM_BUFFER_CONTROL | DM_BUFFER_ENABLE0);

	P9000_WRITE_CMD(sc, P9000_DE_PATTERN_ORIGIN_X, 0);
	P9000_WRITE_CMD(sc, P9000_DE_PATTERN_ORIGIN_Y, 0);
	/* enable all planes */
	P9000_WRITE_CMD(sc, P9000_DE_PLANEMASK, 0xff);

	/* Unclip */
	P9000_WRITE_CMD(sc, P9000_DE_WINMIN, 0);
	P9000_WRITE_CMD(sc, P9000_DE_WINMAX,
	    P9000_COORDS(sc->sc_sunfb.sf_width - 1, sc->sc_sunfb.sf_height - 1));

	P9000_SELECT_PE(sc);
	P9000_WRITE_CMD(sc, P9000_PE_WINOFFSET, 0);
	P9000_WRITE_CMD(sc, P9000_PE_INDEX, 0);
	P9000_WRITE_CMD(sc, P9000_PE_WINMIN, 0);
	P9000_WRITE_CMD(sc, P9000_PE_WINMAX,
	    P9000_COORDS(sc->sc_sunfb.sf_width - 1, sc->sc_sunfb.sf_height - 1));
}

int
p9000_ras_copycols(void *v, int row, int src, int dst, int n)
{
	struct rasops_info *ri = v;
	struct p9000_softc *sc = ri->ri_hw;

	n *= ri->ri_font->fontwidth;
	n--;
	src *= ri->ri_font->fontwidth;
	src += ri->ri_xorigin;
	dst *= ri->ri_font->fontwidth;
	dst += ri->ri_xorigin;
	row *= ri->ri_font->fontheight;
	row += ri->ri_yorigin;

	p9000_drain(sc);
	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9000_RASTER_SRC & P9000_RASTER_MASK);

	P9000_SELECT_COORD(sc, P9000_DC_COORD(0));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(0) + P9000_COORD_XY,
	    P9000_COORDS(src, row));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(1) + P9000_COORD_XY,
	    P9000_COORDS(src + n, row + ri->ri_font->fontheight - 1));
	P9000_SELECT_COORD(sc, P9000_DC_COORD(2));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(2) + P9000_COORD_XY,
	    P9000_COORDS(dst, row));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(3) + P9000_COORD_XY,
	    P9000_COORDS(dst + n, row + ri->ri_font->fontheight - 1));

	sc->sc_junk = P9000_READ_CMD(sc, P9000_PE_BLIT);

	p9000_drain(sc);

	return 0;
}

int
p9000_ras_copyrows(void *v, int src, int dst, int n)
{
	struct rasops_info *ri = v;
	struct p9000_softc *sc = ri->ri_hw;

	n *= ri->ri_font->fontheight;
	n--;
	src *= ri->ri_font->fontheight;
	src += ri->ri_yorigin;
	dst *= ri->ri_font->fontheight;
	dst += ri->ri_yorigin;

	p9000_drain(sc);
	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9000_RASTER_SRC & P9000_RASTER_MASK);

	P9000_SELECT_COORD(sc, P9000_DC_COORD(0));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(0) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin, src));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(1) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth - 1, src + n));
	P9000_SELECT_COORD(sc, P9000_DC_COORD(2));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(2) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin, dst));
	P9000_WRITE_CMD(sc, P9000_DC_COORD(3) + P9000_COORD_XY,
	    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth - 1, dst + n));

	sc->sc_junk = P9000_READ_CMD(sc, P9000_PE_BLIT);

	p9000_drain(sc);

	return 0;
}

int
p9000_ras_erasecols(void *v, int row, int col, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9000_softc *sc = ri->ri_hw;
	int fg, bg;

	ri->ri_ops.unpack_attr(v, attr, &fg, &bg, NULL);

	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	col += ri->ri_xorigin;
	row *= ri->ri_font->fontheight;
	row += ri->ri_yorigin;

	p9000_drain(sc);
	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9000_RASTER_PATTERN & P9000_RASTER_MASK);
	P9000_WRITE_CMD(sc, P9000_DE_FG_COLOR, bg);

	P9000_SELECT_COORD(sc, P9000_LC_RECT);
	P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col, row));
	P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col + n, row + ri->ri_font->fontheight));

	sc->sc_junk = P9000_READ_CMD(sc, P9000_PE_QUAD);

	p9000_drain(sc);

	return 0;
}

int
p9000_ras_eraserows(void *v, int row, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9000_softc *sc = ri->ri_hw;
	int fg, bg;

	ri->ri_ops.unpack_attr(v, attr, &fg, &bg, NULL);

	p9000_drain(sc);
	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_RASTER,
	    P9000_RASTER_PATTERN & P9000_RASTER_MASK);
	P9000_WRITE_CMD(sc, P9000_DE_FG_COLOR, bg);

	P9000_SELECT_COORD(sc, P9000_LC_RECT);
	if (n == ri->ri_rows && ISSET(ri->ri_flg, RI_FULLCLEAR)) {
		P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(0, 0));
		P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_width, ri->ri_height));
	} else {
		n *= ri->ri_font->fontheight;
		row *= ri->ri_font->fontheight;
		row += ri->ri_yorigin;

		P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_xorigin, row));
		P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
		    P9000_COORDS(ri->ri_xorigin + ri->ri_emuwidth, row + n));
	}

	sc->sc_junk = P9000_READ_CMD(sc, P9000_PE_QUAD);

	p9000_drain(sc);

	return 0;
}

int
p9000_ras_do_cursor(struct rasops_info *ri)
{
	struct p9000_softc *sc = ri->ri_hw;
	int row, col;

	row = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;
	col = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;

	p9000_drain(sc);

	P9000_SELECT_DE_LOW(sc);
	P9000_WRITE_CMD(sc, P9000_DE_RASTER,
	    (P9000_RASTER_PATTERN ^ P9000_RASTER_DST) & P9000_RASTER_MASK);
	P9000_WRITE_CMD(sc, P9000_DE_FG_COLOR, ri->ri_devcmap[WSCOL_BLACK]);

	P9000_SELECT_COORD(sc, P9000_LC_RECT);
	P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col, row));
	P9000_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col + ri->ri_font->fontwidth,
	        row + ri->ri_font->fontheight));

	sc->sc_junk = P9000_READ_CMD(sc, P9000_PE_QUAD);

	p9000_drain(sc);

	return 0;
}

/*	$OpenBSD: p9100.c,v 1.50 2011/05/31 17:40:19 miod Exp $	*/

/*
 * Copyright (c) 2003, 2005, 2006, 2008, Miodrag Vallat.
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
#include <machine/bsd_openprom.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>
#include <machine/fbvar.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/sbusvar.h>

#include <dev/ic/ibm525reg.h>
#include <dev/ic/p9000.h>

#include "tctrl.h"
#if NTCTRL > 0
#include <sparc/dev/tctrlvar.h>
#endif

#undef	FIDDLE_WITH_PCI_REGISTERS

/*
 * SBus registers mappings
 */
#define	P9100_NREG		4
#define	P9100_REG_CTL		0
#define	P9100_REG_CMD		1
#define	P9100_REG_VRAM		2
#define	P9100_REG_CONFIG	3

#ifdef	FIDDLE_WITH_PCI_REGISTERS
/*
 * This structure, mapped at register address 0x9100, allows non-PCI
 * designs (such as the SPARCbook) to access the PCI configuration space.
 */
struct p9100_pci {
	volatile u_int32_t	address;	/* within configuration space */
	volatile u_int32_t	data;		/* _byte_ to read or write */
};
#endif

/* per-display variables */
struct p9100_softc {
	struct sunfb	sc_sunfb;	/* common base part */
	struct rom_reg	sc_phys[P9100_NREG - 1];
	volatile u_int8_t *sc_cmd;	/* command registers (dac, etc) */
	volatile u_int8_t *sc_ctl;	/* control registers (draw engine) */
#ifdef	FIDDLE_WITH_PCI_REGISTERS
	struct p9100_pci *sc_pci;	/* pci configuration space access */
#endif
	vsize_t		sc_vramsize;	/* total VRAM available */
	union bt_cmap	sc_cmap;	/* Brooktree color map */
	struct intrhand	sc_ih;
	int		sc_mapmode;
	u_int		sc_flags;
#define	SCF_INTERNAL		0x01	/* internal video enabled */
#define	SCF_EXTERNAL		0x02	/* external video enabled */
#if NTCTRL > 0
#define	SCF_MAPPEDSWITCH	0x04	/* switch mode when leaving emul */
	u_int		sc_mapwidth;	/* non-emul video mode parameters */
	u_int		sc_mapheight;
	u_int		sc_mapdepth;
#endif
	u_int		sc_lcdheight;	/* LCD panel geometry */
	u_int		sc_lcdwidth;

	u_int32_t	sc_junk;	/* throwaway value */
};

void	p9100_burner(void *, u_int, u_int);
void	p9100_external_video(void *, int);
void	p9100_initialize_ramdac(struct p9100_softc *, u_int, u_int);
int	p9100_intr(void *);
int	p9100_ioctl(void *, u_long, caddr_t, int, struct proc *);
static __inline__
void	p9100_loadcmap_deferred(struct p9100_softc *, u_int, u_int);
void	p9100_loadcmap_immediate(struct p9100_softc *, u_int, u_int);
paddr_t	p9100_mmap(void *, off_t, int);
void	p9100_pick_romfont(struct p9100_softc *);
void	p9100_prom(void *);
void	p9100_setcolor(void *, u_int, u_int8_t, u_int8_t, u_int8_t);
u_int	p9100_read_ramdac(struct p9100_softc *, u_int);
void	p9100_write_ramdac(struct p9100_softc *, u_int, u_int);

struct wsdisplay_accessops p9100_accessops = {
	p9100_ioctl,
	p9100_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	p9100_burner,
	NULL	/* pollc */
};

int	p9100_ras_copycols(void *, int, int, int, int);
int	p9100_ras_copyrows(void *, int, int, int);
int	p9100_ras_do_cursor(struct rasops_info *);
int	p9100_ras_erasecols(void *, int, int, int, long int);
int	p9100_ras_eraserows(void *, int, int, long int);
void	p9100_ras_init(struct p9100_softc *);

int	p9100match(struct device *, void *, void *);
void	p9100attach(struct device *, struct device *, void *);

struct cfattach pnozz_ca = {
	sizeof (struct p9100_softc), p9100match, p9100attach
};

struct cfdriver pnozz_cd = {
	NULL, "pnozz", DV_DULL
};

/*
 * IBM RGB525 RAMDAC registers
 */

#define	IBM525_WRADDR			0	/* Palette write address */
#define	IBM525_DATA			1	/* Palette data */
#define	IBM525_PIXMASK			2	/* Pixel mask */
#define	IBM525_RDADDR			3	/* Read palette address */
#define	IBM525_IDXLOW			4	/* Register index low */
#define	IBM525_IDXHIGH			5	/* Register index high */
#define	IBM525_REGDATA			6	/* Register data */
#define	IBM525_IDXCONTROL		7	/* Index control */

/*
 * P9100 read/write macros
 */

#define	P9100_READ_CTL(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg))
#define	P9100_READ_CMD(sc,reg) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg))
#define	P9100_READ_RAMDAC(sc,reg) \
	(*(volatile u_int32_t *)((sc)->sc_ctl + P9100_RAMDAC_REGISTER(reg)) \
	    >> 16)

#define	P9100_WRITE_CTL(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_ctl + (reg)) = (value)
#define	P9100_WRITE_CMD(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_cmd + (reg)) = (value)
#define	P9100_WRITE_RAMDAC(sc,reg,value) \
	*(volatile u_int32_t *)((sc)->sc_ctl + P9100_RAMDAC_REGISTER(reg)) = \
	    ((value) << 16)

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

#if NTCTRL == 0
	/*
	 * If this is not the console device, the frame buffer is
	 * not completely initialized, and access to some of its
	 * control registers will hang. We'll need to reprogram
	 * the RAMDAC, and currently this requires assistance
	 * from the tctrl code. Do not attach if it is not available
	 * and console is on serial.
	 */
	if (ra->ra_node != fbnode)
		return (0);
#endif

	return (1);
}

/*
 * Attach a display.
 */
void
p9100attach(struct device *parent, struct device *self, void *args)
{
	struct p9100_softc *sc = (struct p9100_softc *)self;
	struct rasops_info *ri = &sc->sc_sunfb.sf_ro;
	struct confargs *ca = args;
	struct romaux *ra = &ca->ca_ra;
	int node, pri, scr;
	int isconsole;

	pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", pri);

#ifdef DIAGNOSTIC
	if (ra->ra_nreg < P9100_NREG) {
		printf(": expected %d registers, got only %d\n",
		    P9100_NREG, ra->ra_nreg);
		return;
	}
#endif

	sc->sc_flags = SCF_INTERNAL;
	sc->sc_mapmode = WSDISPLAYIO_MODE_EMUL;

	bcopy(ra->ra_reg, sc->sc_phys, sizeof(sc->sc_phys));

	sc->sc_ctl = mapiodev(&ra->ra_reg[P9100_REG_CTL], 0,
	    ra->ra_reg[P9100_REG_CTL].rr_len);
	sc->sc_cmd = mapiodev(&ra->ra_reg[P9100_REG_CMD], 0,
	    ra->ra_reg[P9100_REG_CMD].rr_len);
#ifdef	FIDDLE_WITH_PCI_REGISTERS
	sc->sc_pci = (struct p9100_pci *)
	    mapiodev(&ra->ra_reg[P9100_REG_CONFIG], 0,
	      ra->ra_reg[P9100_REG_CONFIG].rr_len);
#endif

	node = ra->ra_node;
	isconsole = node == fbnode;

	P9100_SELECT_SCR(sc);
	scr = P9100_READ_CTL(sc, P9000_SYSTEM_CONFIG);

	fb_setsize(&sc->sc_sunfb, 8, 800, 600, node, ca->ca_bustype);

	/*
	 * We expect the PROM to initialize us in the best 8 bit mode
	 * supported by the LCD (640x480 on 3XP, 800x600 on 3GS/3GX).
	 */
	sc->sc_lcdwidth = sc->sc_sunfb.sf_width;
	sc->sc_lcdheight = sc->sc_sunfb.sf_height;

#if NTCTRL > 0
	/*
	 * ... but it didn't if we are running on serial console.
	 * In this case, do it ourselves.
	 */
	if (!isconsole)
		p9100_initialize_ramdac(sc, sc->sc_lcdwidth, 8);
#endif

#if NTCTRL > 0
	/*
	 * We want to run the frame buffer in 8bpp mode for the emulation mode,
	 * and use a potentially better mode for the mapped (X11) mode.
	 * Eventually this will become runtime user-selectable.
	 */

	sc->sc_mapwidth = sc->sc_lcdwidth;
	sc->sc_mapheight = sc->sc_lcdheight;
	sc->sc_mapdepth = 8;

	if (sc->sc_mapwidth != sc->sc_sunfb.sf_width ||
	    sc->sc_mapdepth != sc->sc_sunfb.sf_depth)
		SET(sc->sc_flags, SCF_MAPPEDSWITCH);
#endif

	ri->ri_bits = mapiodev(&ra->ra_reg[P9100_REG_VRAM], 0,
	    sc->sc_vramsize = round_page(ra->ra_reg[P9100_REG_VRAM].rr_len));
	ri->ri_hw = sc;

	printf(": rev %x, %dx%d\n", scr & SCR_ID_MASK,
	    sc->sc_lcdwidth, sc->sc_lcdheight);

	/* Disable frame buffer interrupts */
	P9100_SELECT_SCR(sc);
	P9100_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE, IER_MASTER_ENABLE | 0);

	sc->sc_ih.ih_fun = p9100_intr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(pri, &sc->sc_ih, IPL_FB, self->dv_xname);

	/*
	 * Try to get a copy of the PROM font.
	 *
	 * If we can't, we'll clear the display and switch to the 8x16 font.
	 */
	if (isconsole)
		p9100_pick_romfont(sc);

	/*
	 * Register the external video control callback with tctrl; tctrl
	 * will invoke it immediately to set the appropriate behaviour.
	 * If tctrl is not configured, simply enable external video.
	 */
#if NTCTRL > 0
	tadpole_register_extvideo(p9100_external_video, sc);
#else
	p9100_external_video(sc, 1);
#endif

	fbwscons_init(&sc->sc_sunfb, isconsole);
	fbwscons_setcolormap(&sc->sc_sunfb, p9100_setcolor);

	/*
	 * Plug-in accelerated console operations.
	 */
	p9100_ras_init(sc);

	/* enable video */
	p9100_burner(sc, 1, 0);

	if (isconsole) {
		fbwscons_console_init(&sc->sc_sunfb, -1);
#if NTCTRL > 0
		shutdownhook_establish(p9100_prom, sc);
#endif
	}

	fbwscons_attach(&sc->sc_sunfb, &p9100_accessops, isconsole);
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
		sc->sc_mapmode = *(u_int *)data;
		switch (sc->sc_mapmode) {
		case WSDISPLAYIO_MODE_DUMBFB:
		case WSDISPLAYIO_MODE_MAPPED:
#if NTCTRL > 0
			if (ISSET(sc->sc_flags, SCF_MAPPEDSWITCH))
				p9100_initialize_ramdac(sc,
				    sc->sc_mapwidth, sc->sc_mapdepth);
#endif
			break;
		case WSDISPLAYIO_MODE_EMUL:
#if NTCTRL > 0
			if (ISSET(sc->sc_flags, SCF_MAPPEDSWITCH))
				p9100_initialize_ramdac(sc, sc->sc_lcdwidth, 8);
#endif
			fbwscons_setcolormap(&sc->sc_sunfb, p9100_setcolor);
			/* Restore proper acceleration state as well */
			p9100_ras_init(sc);
			break;
		}
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
#if NTCTRL > 0
		if (ISSET(sc->sc_flags, SCF_MAPPEDSWITCH)) {
			wdf->width = sc->sc_mapwidth;
			wdf->height = sc->sc_mapheight;
			wdf->depth  = sc->sc_mapdepth;
			wdf->cmsize = sc->sc_mapdepth == 8 ? 256 : 0;
		} else
#endif
		{
			wdf->width  = sc->sc_lcdwidth;
			wdf->height = sc->sc_lcdheight;
			wdf->depth  = 8;
			wdf->cmsize = 256;
		}
		break;

	case WSDISPLAYIO_LINEBYTES:
#if NTCTRL > 0
		if (ISSET(sc->sc_flags, SCF_MAPPEDSWITCH))
			*(u_int *)data = sc->sc_mapwidth *
			    (sc->sc_mapdepth / 8);
		else
#endif
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
			if (ISSET(sc->sc_flags, SCF_INTERNAL))
				dp->curval =
				    tadpole_get_video() & TV_ON ? 1 : 0;
			else
				dp->curval = 0;
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
			if (ISSET(sc->sc_flags, SCF_INTERNAL))
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
p9100_mmap(void *v, off_t offset, int prot)
{
	struct p9100_softc *sc = v;
	struct rom_reg *rr;

	if ((offset & PAGE_MASK) != 0)
		return (-1);

	switch (sc->sc_mapmode) {
	case WSDISPLAYIO_MODE_MAPPED:
		/*
		 * We provide the following mapping:
		 * 000000 - 0000ff  control registers
		 * 002000 - 003fff  command registers
		 * 800000 - 9fffff  vram
		 */
		rr = &sc->sc_phys[P9100_REG_CTL];
		if (offset >= 0 && offset < rr->rr_len)
			break;
		offset -= 0x2000;
		rr = &sc->sc_phys[P9100_REG_CMD];
		if (offset >= 0 && offset < rr->rr_len)
			break;
		offset -= (0x800000 - 0x2000);
		/* FALLTHROUGH */
	case WSDISPLAYIO_MODE_DUMBFB:
		rr = &sc->sc_phys[P9100_REG_VRAM];
		if (offset >= 0 && offset < sc->sc_vramsize)
			break;
		/* FALLTHROUGH */
	default:
		return (-1);
	}

	return (REG2PHYS(rr, offset) | PMAP_NC);
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
	P9100_WRITE_RAMDAC(sc, IBM525_WRADDR, start);
	P9100_FLUSH_DAC(sc);

	for (p = sc->sc_cmap.cm_map[start], ncolors *= 3; ncolors-- > 0; p++) {
		P9100_SELECT_DAC(sc);
		P9100_WRITE_RAMDAC(sc, IBM525_DATA, (*p));
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

u_int
p9100_read_ramdac(struct p9100_softc *sc, u_int reg)
{
	P9100_SELECT_DAC(sc);

	P9100_WRITE_RAMDAC(sc, IBM525_IDXLOW, (reg & 0xff));
	P9100_FLUSH_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_IDXHIGH, ((reg >> 8) & 0xff));
	P9100_FLUSH_DAC(sc);
	return (P9100_READ_RAMDAC(sc, IBM525_REGDATA));
}

void
p9100_write_ramdac(struct p9100_softc *sc, u_int reg, u_int value)
{
	P9100_SELECT_DAC(sc);

	P9100_WRITE_RAMDAC(sc, IBM525_IDXLOW, (reg & 0xff));
	P9100_FLUSH_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_IDXHIGH, ((reg >> 8) & 0xff));
	P9100_FLUSH_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_REGDATA, value);
	P9100_FLUSH_DAC(sc);
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
	if (ISSET(sc->sc_flags, SCF_INTERNAL))
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

		/* Clear interrupt condition */
		P9100_WRITE_CTL(sc, P9000_INTERRUPT,
		    IER_VBLANK_ENABLE | 0);

		return (1);
	}

	return (0);
}

/*
 * Accelerated text console code
 */

static int p9100_drain(struct p9100_softc *);

static int
p9100_drain(struct p9100_softc *sc)
{
	u_int i;

	for (i = 10000; i !=0; i--) {
		if ((P9100_READ_CMD(sc, P9000_PE_STATUS) &
		    (STATUS_QUAD_BUSY | STATUS_BLIT_BUSY)) == 0)
			break;
	}

	return (i);
}

void
p9100_ras_init(struct p9100_softc *sc)
{

	if (p9100_drain(sc) == 0)
		return;

	sc->sc_sunfb.sf_ro.ri_ops.copycols = p9100_ras_copycols;
	sc->sc_sunfb.sf_ro.ri_ops.copyrows = p9100_ras_copyrows;
	sc->sc_sunfb.sf_ro.ri_ops.erasecols = p9100_ras_erasecols;
	sc->sc_sunfb.sf_ro.ri_ops.eraserows = p9100_ras_eraserows;
	sc->sc_sunfb.sf_ro.ri_do_cursor = p9100_ras_do_cursor;

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

int
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

	return 0;
}

int
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

	return 0;
}

int
p9100_ras_erasecols(void *v, int row, int col, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;
	int fg, bg;

	ri->ri_ops.unpack_attr(v, attr, &fg, &bg, NULL);
	bg = ri->ri_devcmap[bg];

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

	return 0;
}

int
p9100_ras_eraserows(void *v, int row, int n, long int attr)
{
	struct rasops_info *ri = v;
	struct p9100_softc *sc = ri->ri_hw;
	int fg, bg;

	ri->ri_ops.unpack_attr(v, attr, &fg, &bg, NULL);
	bg = ri->ri_devcmap[bg];

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

	return 0;
}

int
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
	P9100_WRITE_CMD(sc, P9100_DE_COLOR0,
	    P9100_COLOR8(ri->ri_devcmap[WSCOL_BLACK]));

	P9100_SELECT_COORD(sc, P9000_LC_RECT);
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col, row));
	P9100_WRITE_CMD(sc, P9000_LC_RECT + P9000_COORD_XY,
	    P9000_COORDS(col + ri->ri_font->fontwidth,
	        row + ri->ri_font->fontheight));

	sc->sc_junk = P9100_READ_CMD(sc, P9000_PE_QUAD);

	p9100_drain(sc);

	return 0;
}

/*
 * PROM font managment
 */

#define	ROMFONTNAME	"p9100_romfont"
struct wsdisplay_font p9100_romfont = {
	ROMFONTNAME,
	0,
	0, 256,
	WSDISPLAY_FONTENC_ISO,	/* should check the `character-set' property */
	0, 0, 0,
	WSDISPLAY_FONTORDER_L2R,
	WSDISPLAY_FONTORDER_L2R,
	NULL,
	NULL
};

void
p9100_pick_romfont(struct p9100_softc *sc)
{
	struct rasops_info *ri = &sc->sc_sunfb.sf_ro;
	int *fontwidth, *fontheight, *fontstride;
	u_int8_t **fontaddr;
	char buf[200];

	/*
	 * This code currently only works for PROM >= 2.9; see
	 * autoconf.c romgetcursoraddr() for details.
	 */
	if (promvec->pv_romvec_vers < 2 || promvec->pv_printrev < 0x00020009)
		return;

	/*
	 * Get the PROM font metrics and address
	 */
	if (snprintf(buf, sizeof buf, "stdout @ is my-self "
	    "addr char-height %lx ! addr char-width %lx ! "
	    "addr font-base %lx ! addr fontbytes %lx !",
	    (vaddr_t)&fontheight, (vaddr_t)&fontwidth,
	    (vaddr_t)&fontaddr, (vaddr_t)&fontstride) >= sizeof buf)
		return;
	fontheight = fontwidth = fontstride = NULL;
	fontaddr = NULL;
	rominterpret(buf);

	if (fontheight == NULL || fontwidth == NULL || fontstride == NULL ||
	    fontaddr == NULL || *fontheight == 0 || *fontwidth == 0 ||
	    *fontstride < howmany(*fontwidth, NBBY) ||
	    *fontstride > 4 /* paranoia */)
		return;

	p9100_romfont.fontwidth = *fontwidth;
	p9100_romfont.fontheight = *fontheight;
	p9100_romfont.stride = *fontstride;
	p9100_romfont.data = *fontaddr;
	
#ifdef DEBUG
	printf("%s: PROM font %dx%d-%d @%p",
	    sc->sc_sunfb.sf_dev.dv_xname, *fontwidth, *fontheight,
	    *fontstride, *fontaddr);
#endif

	/*
	 * Build and add a wsfont structure
	 */
	wsfont_init();	/* if not done before */
	if (wsfont_add(&p9100_romfont, 0) != 0)
		return;

	/*
	 * Select this very font in our rasops structure
	 */
	ri->ri_wsfcookie = wsfont_find(ROMFONTNAME, 0, 0, 0);
	if (wsfont_lock(ri->ri_wsfcookie, &ri->ri_font,
	    WSDISPLAY_FONTORDER_L2R, WSDISPLAY_FONTORDER_L2R) <= 0)
		ri->ri_wsfcookie = 0;
}

/*
 * External video control
 */

void
p9100_external_video(void *v, int on)
{
	struct p9100_softc *sc = v;
	int s;

	s = splhigh();

	if (on) {
		p9100_write_ramdac(sc, IBM525_POWER,
		    p9100_read_ramdac(sc, IBM525_POWER) & ~P_DAC_PWR_DISABLE);
		SET(sc->sc_flags, SCF_EXTERNAL);
	} else {
		p9100_write_ramdac(sc, IBM525_POWER,
		    p9100_read_ramdac(sc, IBM525_POWER) | P_DAC_PWR_DISABLE);
		CLR(sc->sc_flags, SCF_EXTERNAL);
	}

	splx(s);
}

/*
 * Video mode programming
 *
 * All magic values come from s3gxtrmb.pdf.
 */

#if NTCTRL > 0

/* IBM RGB525 registers and values */

static const u_int8_t p9100_dacreg[] = {
	IBM525_MISC1,
	IBM525_MISC2,
	IBM525_MISC3,
	IBM525_MISC4,
	IBM525_MISC_CLOCK,
	IBM525_SYNC,
	IBM525_HSYNC_POS,
	IBM525_POWER,
	IBM525_DAC_OP,
	IBM525_PALETTE,
	IBM525_PIXEL,
	IBM525_PF8,
	IBM525_PF16,
	IBM525_PF24,
	IBM525_PF32,
	IBM525_PLL1,
	IBM525_PLL2,
	IBM525_PLL_FIXED_REF,
	IBM525_SYSCLK,
	IBM525_PLL_REF_DIV,
	IBM525_PLL_VCO_DIV,
	0
};

static u_int8_t p9100_dacval[] = {
	M1_SENSE_DISABLE | M1_VRAM_64,
	M2_PCLK_PLL | M2_PALETTE_8 | M2_MODE_VRAM,
	0,
	0,				/* will be computed */
	MC_B24P_SCLK | MC_PLL_ENABLE,	/* will be modified */
	S_HSYN_NORMAL | S_VSYN_NORMAL,
	0,
	0,				/* will be computed */
	DO_FAST_SLEW,
	0,
	0,				/* will be computed */
	PF8_INDIRECT,
	PF16_DIRECT | PF16_LINEAR | PF16_565,
	PF24_DIRECT,
	PF32_DIRECT,
	P1_CLK_REF | P1_SRC_EXT_F | P1_SRC_DIRECT_F,
	0,	/* F0, will be set before */
	5,
	SC_ENABLE,
	5,
	MHZ_TO_PLL(50)
};

/* Power 9100 registers and values */

static const u_int32_t p9100_reg[] = {
	P9000_HTR,
	P9000_HSRE,
	P9000_HBRE,
	P9000_HBFE,
	P9000_HCP,
	P9000_VL,
	P9000_VSRE,
	P9000_VBRE,
	P9000_VBFE,
	P9000_VCP,
	0
};

static const u_int32_t p9100_val_800_32[] = {
	0x1f3, 0x023, 0x053, 0x1e3, 0x000, 0x271, 0x002, 0x016, 0x26e, 0x000
};
#if 0	/* No X server for this mode, yet */
static const u_int32_t p9100_val_800_24[] = {
	0x176, 0x01a, 0x03d, 0x169, 0x000, 0x271, 0x002, 0x016, 0x26e, 0x000
};
#endif
static const u_int32_t p9100_val_800_8[] = {
	0x07c, 0x008, 0x011, 0x075, 0x000, 0x271, 0x002, 0x016, 0x26e, 0x000
};
static const u_int32_t p9100_val_640_32[] = {
	0x18f, 0x02f, 0x043, 0x183, 0x000, 0x205, 0x003, 0x022, 0x202, 0x000
};
static const u_int32_t p9100_val_640_8[] = {
	0x063, 0x00b, 0x00d, 0x05d, 0x000, 0x205, 0x003, 0x022, 0x202, 0x000
};
static const u_int32_t p9100_val_1024_8[] = {
	0x0a7, 0x019, 0x022, 0x0a2, 0x000, 0x325, 0x003, 0x023, 0x323, 0x000
};

void
p9100_initialize_ramdac(struct p9100_softc *sc, u_int width, u_int depth)
{
	int s;
	const u_int8_t *dacregp, *dacvalp;
	const u_int32_t *p9regp, *p9valp;
	u_int8_t pllclk, dacval;
	u_int32_t scr;

	/*
	 * XXX Switching to a low-res 8bpp mode does not work correctly
	 * XXX unless coming from an high-res 8bpp mode, and I have
	 * XXX no idea why.
	 * XXX Of course, this mean that we can't reasonably use this
	 * XXX routine unless NTCTRL > 0.
	 */
	if (depth == 8 && width != 1024)
		p9100_initialize_ramdac(sc, 1024, 8);

	switch (width) {
	case 1024:
		p9valp = p9100_val_1024_8;
		pllclk = MHZ_TO_PLL(65);
		/* 1024 bytes scanline */
		scr = SCR_SC(0, 0, 0, 1) | SCR_PIXEL_8BPP;
		break;
	default:
		/* FALLTHROUGH */
	case 800:
		switch (depth) {
		case 32:
			p9valp = p9100_val_800_32;
			/* 3200 = 128 + 1024 + 2048 bytes scanline */
			scr = SCR_SC(3, 6, 7, 0) |
			    SCR_PIXEL_32BPP | SCR_SWAP_WORDS | SCR_SWAP_BYTES;
			break;
#if 0
		case 24:
			p9valp = p9100_val_800_24;
			/* 2400 = 32 + 64 + 256 + 2048 bytes scanline */
			scr = SCR_SC(1, 2, 4, 2) | SCR_PIXEL_24BPP;
			break;
#endif
		default:
		case 8:
			p9valp = p9100_val_800_8;
			/* 800 = 32 + 256 + 512 bytes scanline */
			scr = SCR_SC(1, 4, 5, 0) | SCR_PIXEL_8BPP;
			break;
		}
		pllclk = MHZ_TO_PLL(36);
		break;
	case 640:
		switch (depth) {
		case 32:
			p9valp = p9100_val_640_32;
			/* 2560 = 512 + 2048 bytes scanline */
			scr = SCR_SC(5, 7, 0, 0) |
			    SCR_PIXEL_32BPP | SCR_SWAP_WORDS | SCR_SWAP_BYTES;
			break;
		default:
		case 8:
			p9valp = p9100_val_640_8;
			/* 640 = 128 + 512 bytes scanline */
			scr = SCR_SC(3, 5, 0, 0) | SCR_PIXEL_8BPP;
			break;
		}
		pllclk = MHZ_TO_PLL(25);
		break;
	}
	dacvalp = p9100_dacval;

	s = splhigh();

#ifdef	FIDDLE_WITH_PCI_REGISTERS
	/*
	 * Magical initialization sequence, from s3gxtrmb.pdf.
	 * DANGER! Sometimes freezes the machine solid, cause unknown.
	 */
	sc->sc_pci->address = 0x13000000;
	sc->sc_pci->data = 0;
	sc->sc_pci->address = 0x30000000;
	sc->sc_pci->data = 0;
	sc->sc_pci->address = 0x41000000;
	sc->sc_pci->data = 0;	/* No register mapping at a0000 */
	sc->sc_pci->address = 0x04000000;
	sc->sc_pci->data = 0xa3000000;
#endif

	/*
	 * Initialize the RAMDAC
	 */
	P9100_SELECT_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_PIXMASK, 0xff);
	P9100_FLUSH_DAC(sc);
	P9100_WRITE_RAMDAC(sc, IBM525_IDXCONTROL, 0x00);
	P9100_FLUSH_DAC(sc);

	p9100_write_ramdac(sc, IBM525_F(0), pllclk);
	for (dacregp = p9100_dacreg; *dacregp != 0; dacregp++, dacvalp++) {
		switch (*dacregp) {
		case IBM525_MISC4:
			dacval =  pllclk >= MHZ_TO_PLL(50) ?
			    M4_FAST : M4_INVERT_DCLK;
			break;
		case IBM525_MISC_CLOCK:
			dacval = *dacvalp & ~MC_DDOT_DIV_MASK;
			switch (depth) {
			case 32:
				dacval |= MC_DDOT_DIV_2;
				break;
			case 16:
				dacval |= MC_DDOT_DIV_4;
				break;
			default:
			case 24:
			case 8:
				dacval |= MC_DDOT_DIV_8;
				break;
			}
			break;
		case IBM525_POWER:
			if (depth == 24)
				dacval = 0;
			else
				dacval = P_SCLK_DISABLE;
			break;
		case IBM525_PIXEL:
			switch (depth) {
			case 32:
				dacval = PIX_32BPP;
				break;
			case 24:
				dacval = PIX_24BPP;
				break;
			case 16:
				dacval = PIX_16BPP;
				break;
			default:
			case 8:
				dacval = PIX_8BPP;
				break;
			}
			break;
		default:
			dacval = *dacvalp;
			break;
		}
		p9100_write_ramdac(sc, *dacregp, dacval);
	}

	/*
	 * Initialize the Power 9100
	 */

	P9100_SELECT_SCR(sc);
	P9100_WRITE_CTL(sc, P9000_SYSTEM_CONFIG, scr);
	P9100_SELECT_VCR(sc);
	P9100_WRITE_CTL(sc, P9000_SRTC1,
	    SRTC1_VSYNC_INTERNAL | SRTC1_HSYNC_INTERNAL | SRTC1_VIDEN | 0x03);
	P9100_WRITE_CTL(sc, P9000_SRTC2, 0x05);
	/* need to wait a bit before VRAM control registers are accessible */
	delay(3000);
	P9100_SELECT_VRAM(sc);
	P9100_WRITE_CTL(sc, P9000_MCR, 0xc808007d);
	delay(3000);

	P9100_SELECT_VCR(sc);
	for (p9regp = p9100_reg; *p9regp != 0; p9regp++, p9valp++)
		P9100_WRITE_CTL(sc, *p9regp, *p9valp);

	P9100_SELECT_VRAM(sc);
	P9100_WRITE_CTL(sc, P9000_REFRESH_PERIOD, 0x3ff);

	/* Disable frame buffer interrupts */
	P9100_SELECT_SCR(sc);
	P9100_WRITE_CTL(sc, P9000_INTERRUPT_ENABLE, IER_MASTER_ENABLE | 0);

	/*
	 * Enable internal video... (it's a kind of magic)
	 */
	p9100_write_ramdac(sc, IBM525_MISC4,
	    p9100_read_ramdac(sc, IBM525_MISC4) | 0xc0);

	/*
	 * ... unless it does not fit.
	 */
	if (width != sc->sc_lcdwidth) {
		CLR(sc->sc_flags, SCF_INTERNAL);
		tadpole_set_video(0);
	} else {
		SET(sc->sc_flags, SCF_INTERNAL);
		tadpole_set_video(1);
	}

	p9100_external_video(sc, ISSET(sc->sc_flags, SCF_EXTERNAL));

	splx(s);
}

void
p9100_prom(void *v)
{
	struct p9100_softc *sc = v;

	if (ISSET(sc->sc_flags, SCF_MAPPEDSWITCH) &&
	    sc->sc_mapmode != WSDISPLAYIO_MODE_EMUL) {
		p9100_initialize_ramdac(sc, sc->sc_lcdwidth, 8);
		fbwscons_setcolormap(&sc->sc_sunfb, p9100_setcolor);
		p9100_ras_init(sc);
	}
}
#endif	/* NTCTRL > 0 */

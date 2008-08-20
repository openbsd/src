/*	$OpenBSD: legss.c,v 1.1 2008/08/20 19:00:01 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * LEGSS frame buffer
 *
 * This beast is different enough from a QDSS or a GPX to need a specific
 * driver.  Unfortunately, it is not yet known how this hardware works.
 *
 * The frame buffer memory is accessible linearly in 32 bit words (one
 * per pixel, although apparently only 20 bits are writable).
 *
 * We currently drive the frame buffer as a monochrome, unaccelerated
 * display.
 *
 * Note that the hardware probe is made easier since graphics can only
 * exist in the last M-Bus slot, and the terminal information in the SSC
 * will tell us whether it is properly populated or not.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/cvax.h>
#include <machine/sid.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

/* Graphics can only exist at mid 7 */
#define	MID_GRAPHICS		7
#define	LEGSS_BASE		MBUS_SLOT_BASE(MID_GRAPHICS)

#define	LEGSS_VRAM_OFFSET	0x00800000

#define	LEGSS_VISWIDTH	1280
#define	LEGSS_WIDTH	2048
#define	LEGSS_VISHEIGHT	1024
#define	LEGSS_HEIGHT	2048

int	legss_match(struct device *, void *, void *);
void	legss_attach(struct device *, struct device *, void *);

struct	legss_screen {
	struct rasops_info ss_ri;
	int		ss_console;
	u_int		ss_depth;
	vaddr_t		ss_vram;
};

/* for console */
struct legss_screen legss_consscr;

struct	legss_softc {
	struct device sc_dev;
	struct legss_screen *sc_scr;
	int	sc_nscreens;
};

struct cfattach legss_ca = {
	sizeof(struct legss_softc), legss_match, legss_attach,
};

struct	cfdriver legss_cd = {
	NULL, "legss", DV_DULL
};

struct wsscreen_descr legss_stdscreen = {
	"std",
};

const struct wsscreen_descr *_legss_scrlist[] = {
	&legss_stdscreen,
};

const struct wsscreen_list legss_screenlist = {
	sizeof(_legss_scrlist) / sizeof(struct wsscreen_descr *),
	_legss_scrlist,
};

int	legss_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	legss_mmap(void *, off_t, int);
int	legss_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	legss_free_screen(void *, void *);
int	legss_show_screen(void *, void *, int,
	    void (*) (void *, int, int), void *);

const struct wsdisplay_accessops legss_accessops = {
	legss_ioctl,
	legss_mmap,
	legss_alloc_screen,
	legss_free_screen,
	legss_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

int	legss_setup_screen(struct legss_screen *);
u_int	legss_probe_depth(vaddr_t);

/*
 * Autoconf glue
 */

int
legss_match(struct device *parent, void *vcf, void *aux)
{
	struct mbus_attach_args *maa = (struct mbus_attach_args *)aux;

	if (maa->maa_class == CLASS_GRAPHICS &&
	    maa->maa_interface == INTERFACE_FBIC &&
	    maa->maa_mid == MID_GRAPHICS)
		return 1;

	return 0;
}

void
legss_attach(struct device *parent, struct device *self, void *aux)
{
	struct legss_softc *sc = (struct legss_softc *)self;
	struct legss_screen *scr;
	struct wsemuldisplaydev_attach_args aa;
	int console;
	vaddr_t tmp;
	extern struct consdev wsdisplay_cons;

	console = (vax_confdata & 0x60) != 0 && cn_tab == &wsdisplay_cons;
	if (console) {
		scr = &legss_consscr;
		sc->sc_nscreens = 1;
	} else {
		scr = malloc(sizeof(struct legss_screen), M_DEVBUF, M_NOWAIT);
		if (scr == NULL) {
			printf(": can not allocate memory\n");
			return;
		}

		tmp = vax_map_physmem(LEGSS_BASE + LEGSS_VRAM_OFFSET, 1);
		if (tmp == 0L) {
			printf(": can not probe depth\n");
			goto bad1;
		}
		scr->ss_depth = legss_probe_depth(tmp);
		vax_unmap_physmem(tmp, 1);

		if (scr->ss_depth == 0) {
			printf(": unrecognized depth\n");
			goto bad1;
		}

		scr->ss_vram = vax_map_physmem(LEGSS_BASE + LEGSS_VRAM_OFFSET,
		    (LEGSS_VISHEIGHT * LEGSS_WIDTH * 32 / NBBY) / VAX_NBPG);
		if (scr->ss_vram == 0L) {
			printf(": can not map frame buffer\n");
			goto bad1;
		}

		if (legss_setup_screen(scr) != 0) {
			printf(": initialization failed\n");
			goto bad2;
		}
	}
	sc->sc_scr = scr;

	printf(": %dx%d %d plane color framebuffer\n",
	    LEGSS_VISWIDTH, LEGSS_VISHEIGHT, scr->ss_depth);

	aa.console = console;
	aa.scrdata = &legss_screenlist;
	aa.accessops = &legss_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found(self, &aa, wsemuldisplaydevprint);

	return;

bad2:
	vax_unmap_physmem(scr->ss_vram,
	    (LEGSS_VISHEIGHT * LEGSS_WIDTH * 32 / NBBY) / VAX_NBPG);
bad1:
	free(scr, M_DEVBUF);
}

/*
 * wsdisplay accessops
 */

int
legss_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct legss_softc *sc = v;
	struct legss_screen *ss = sc->sc_scr;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = 0; /* XXX WSDISPLAY_TYPE_LEGSS; */
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = ss->ss_ri.ri_height;
		wdf->width = ss->ss_ri.ri_width;
		wdf->depth = ss->ss_depth;
		wdf->cmsize = 0;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = LEGSS_WIDTH * 32 / NBBY;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	default:
		return -1;
	}

	return 0;
}

paddr_t
legss_mmap(void *v, off_t offset, int prot)
{
	/* Do not allow mmap yet because of the read-only upper 12 bits */
	return -1;
}

int
legss_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct legss_softc *sc = v;
	struct legss_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	if (sc->sc_nscreens > 0)
		return ENOMEM;

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, defattrp);
	sc->sc_nscreens++;

	return 0;
}

void
legss_free_screen(void *v, void *cookie)
{
	struct legss_softc *sc = v;

	sc->sc_nscreens--;
}

int
legss_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

int
legss_setup_screen(struct legss_screen *ss)
{
	struct rasops_info *ri = &ss->ss_ri;

	bzero(ri, sizeof(*ri));
	ri->ri_depth = 32;	/* masquerade as a 32 bit device for rasops */
	ri->ri_width = LEGSS_VISWIDTH;
	ri->ri_height = LEGSS_VISHEIGHT;
	ri->ri_stride = LEGSS_WIDTH * 32 / NBBY;
	ri->ri_flg = RI_FORCEMONO | RI_CENTER | RI_CLEAR;
	ri->ri_hw = ss;
	ri->ri_bits = (u_char *)ss->ss_vram;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return -1;

	legss_stdscreen.ncols = ri->ri_cols;
	legss_stdscreen.nrows = ri->ri_rows;
	legss_stdscreen.textops = &ri->ri_ops;
	legss_stdscreen.fontwidth = ri->ri_font->fontwidth;
	legss_stdscreen.fontheight = ri->ri_font->fontheight;
	legss_stdscreen.capabilities = ri->ri_caps;

	return 0;
}

u_int
legss_probe_depth(vaddr_t vram)
{
	uint32_t probe;

	*(volatile uint32_t *)vram = 0;
	*(volatile uint32_t *)vram = 0xffffffff;
	probe = *(volatile uint32_t *)vram;

	/*
	 * Need to mask the upper 12 bits, they don't seem to be connected
	 * to anything and latch random bus data.
	 */
	switch (probe & 0x000fffff) {
	case 0x00ff:
		return 8;
	default:
		return 0;
	}
}

/*
 * Console support code
 */

int	legsscnprobe(void);
int	legsscninit(void);

int
legsscnprobe()
{
	extern vaddr_t virtual_avail;
	int depth;

	if (vax_boardtype != VAX_BTYP_60)
		return 0;	/* move along, nothing there */

	/* no working graphics hardware, or forced serial console? */
	if ((vax_confdata & 0x60) == 0)
		return 0;

	/*
	 * Check for a recognized color depth.
	 */

	ioaccess(virtual_avail, LEGSS_BASE + LEGSS_VRAM_OFFSET, 1);
	depth = legss_probe_depth(virtual_avail);
	iounaccess(virtual_avail, 1);

	if (depth == 0)
		return 0;	/* unsupported, default to serial */

	return 1;
}

/*
 * Called very early to setup the glass tty as console.
 * Because it's called before the VM system is initialized, virtual memory
 * for the framebuffer can be stolen directly without disturbing anything.
 */
int
legsscninit()
{
	struct legss_screen *ss = &legss_consscr;
	extern vaddr_t virtual_avail;
	vaddr_t ova;
	long defattr;
	struct rasops_info *ri;

	ova = virtual_avail;

	ioaccess(virtual_avail, LEGSS_BASE + LEGSS_VRAM_OFFSET, 1);
	ss->ss_depth = legss_probe_depth(virtual_avail);
	iounaccess(virtual_avail, 1);
	if (ss->ss_depth == 0)
		return 1;

	ioaccess(virtual_avail, LEGSS_BASE + LEGSS_VRAM_OFFSET,
	    (LEGSS_VISHEIGHT * LEGSS_WIDTH * 32 / NBBY) / VAX_NBPG);
	ss->ss_vram = virtual_avail;
	virtual_avail += (LEGSS_VISHEIGHT * LEGSS_WIDTH * 32 / NBBY);
	virtual_avail = round_page(virtual_avail);

	/* this had better not fail */
	if (legss_setup_screen(ss) != 0) {
		iounaccess(ss->ss_vram,
		    (LEGSS_VISHEIGHT * LEGSS_WIDTH * 32 / NBBY) / VAX_NBPG);
		virtual_avail = ova;
		return 1;
	}

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&legss_stdscreen, ri, 0, 0, defattr);

	return 0;
}

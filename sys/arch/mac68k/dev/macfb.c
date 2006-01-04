/*	$OpenBSD: macfb.c,v 1.1 2006/01/04 20:39:05 miod Exp $	*/
/* $NetBSD: macfb.c,v 1.11 2005/01/15 16:00:59 chs Exp $ */
/*
 * Copyright (c) 1998 Matt DeBergalis
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Matt DeBergalis
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <mac68k/dev/nubus.h>
#include <mac68k/dev/grfvar.h>
#include <mac68k/dev/macfbvar.h>

#include <uvm/uvm_extern.h>

#include <dev/wscons/wsconsio.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

int macfb_match(struct device *, void *, void *);
void macfb_attach(struct device *, struct device *, void *);

struct cfattach macfb_ca = {
	sizeof(struct macfb_softc), macfb_match, macfb_attach
};

struct cfdriver macfb_cd = {
	NULL, "macfb", DV_DULL
};

const struct wsdisplay_emulops macfb_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr macfb_stdscreen = {
	"std",
	0, 0, /* will be filled in -- XXX shouldn't, it's global */
	&macfb_emulops,
	0, 0,
	WSSCREEN_REVERSE
};

const struct wsscreen_descr *_macfb_scrlist[] = {
	&macfb_stdscreen,
};

const struct wsscreen_list macfb_screenlist = {
	sizeof(_macfb_scrlist) / sizeof(struct wsscreen_descr *),
	_macfb_scrlist
};

int	macfb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	macfb_mmap(void *, off_t, int);
int	macfb_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
void	macfb_free_screen(void *, void *);
int	macfb_show_screen(void *, void *, int,
		    void (*)(void *, int, int), void *);

const struct wsdisplay_accessops macfb_accessops = {
	macfb_ioctl,
	macfb_mmap,
	macfb_alloc_screen,
	macfb_free_screen,
	macfb_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

void macfb_init(struct macfb_devconfig *);

paddr_t macfb_consaddr;
static int macfb_is_console(paddr_t);

static struct macfb_devconfig macfb_console_dc;

/* From Booter via locore */
extern long		videoaddr;
extern long		videorowbytes;
extern long		videobitdepth;
extern u_long		videosize;
extern u_int32_t	mac68k_vidlog;
extern u_int32_t	mac68k_vidphys;
extern u_int32_t	mac68k_vidlen;

static int
macfb_is_console(paddr_t addr)
{
	if (addr != macfb_consaddr &&
	    (addr >= 0xf9000000 && addr <= 0xfeffffff)) {
		/*
		 * This is in the NuBus standard slot space range, so we
		 * may well have to look at 0xFssxxxxx, too.  Mask off the
		 * slot number and duplicate it in bits 20-23, per IM:V
		 * pp 459, 463, and IM:VI ch 30 p 17.
		 * Note:  this is an ugly hack and I wish I knew what
		 * to do about it.  -- sr
		 */
		addr = (paddr_t)(((u_long)addr & 0xff0fffff) |
		    (((u_long)addr & 0x0f000000) >> 4));
	}
	return ((mac68k_machine.serial_console & 0x03) == 0
	    && (addr == macfb_consaddr));
}

void
macfb_clear(struct macfb_devconfig *dc)
{
	int i, rows;

	/* clear the display */
	rows = dc->dc_ht;
	for (i = 0; rows-- > 0; i += dc->dc_rowbytes)
		memset((u_char *)dc->dc_vaddr + dc->dc_offset + i,
		    0, dc->dc_rowbytes);
}

void
macfb_init(struct macfb_devconfig *dc)
{
	struct raster *rap;
	struct rcons *rcp;

	macfb_clear(dc);

	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = dc->dc_depth;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)(dc->dc_vaddr + dc->dc_offset);

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 128, 192);

	macfb_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	macfb_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

int
macfb_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
macfb_attach(struct device *parent, struct device *self, void *aux)
{
	struct grfbus_attach_args *ga = aux;
	struct grfmode *gm = ga->ga_grfmode;
	struct macfb_softc *sc;
	struct wsemuldisplaydev_attach_args waa;
	int isconsole;

	sc = (struct macfb_softc *)self;

#ifdef DIAGNOSTIC	/* temporary */
	printf(" offset %p", gm->fboff);
#endif
	printf("\n");

	isconsole = macfb_is_console(ga->ga_phys + ga->ga_grfmode->fboff);

	if (isconsole) {
		sc->sc_dc = &macfb_console_dc;
		sc->nscreens = 1;
	} else {
		sc->sc_dc = malloc(sizeof(struct macfb_devconfig), M_DEVBUF, M_WAITOK);
		sc->sc_dc->dc_vaddr = (vaddr_t)gm->fbbase;
		sc->sc_dc->dc_paddr = ga->ga_phys;
		sc->sc_dc->dc_size = gm->fbsize;

		sc->sc_dc->dc_wid = gm->width;
		sc->sc_dc->dc_ht = gm->height;
		sc->sc_dc->dc_depth = gm->psize;
		sc->sc_dc->dc_rowbytes = gm->rowbytes;

		sc->sc_dc->dc_offset = gm->fboff;

		macfb_init(sc->sc_dc);

		sc->nscreens = 0;
	}

	waa.console = isconsole;
	waa.scrdata = &macfb_screenlist;
	waa.accessops = &macfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}


int
macfb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct macfb_softc *sc = v;
	struct macfb_devconfig *dc = sc->sc_dc;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = 0;	/* XXX */
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = dc->dc_raster.height;
		wdf->width = dc->dc_raster.width;
		wdf->depth = dc->dc_raster.depth;
		wdf->cmsize = 0;	/* until we can change it... */
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = dc->dc_rowbytes;
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
macfb_mmap(void *v, off_t offset, int prot)
{
	struct macfb_softc *sc = v;
	struct macfb_devconfig *dc = sc->sc_dc;
	paddr_t addr;

	if (offset >= 0 &&
	    offset < round_page(dc->dc_size))
		addr = atop(dc->dc_paddr + dc->dc_offset + offset);
	else
		addr = (-1);

	return addr;
}

int
macfb_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct macfb_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*defattrp = defattr;
	sc->nscreens++;
	return (0);
}

void
macfb_free_screen(void *v, void *cookie)
{
	struct macfb_softc *sc = v;

#ifdef DIAGNOSTIC
	if (sc->sc_dc == &macfb_console_dc)
		panic("cfb_free_screen: console");
#endif

	sc->nscreens--;
}

int
macfb_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return 0;
}

int
macfb_cnattach(paddr_t addr)
{
	struct macfb_devconfig *dc = &macfb_console_dc;
	long defattr;

	dc->dc_vaddr = trunc_page(videoaddr);
	dc->dc_paddr = trunc_page(mac68k_vidphys);

	dc->dc_wid = videosize & 0xffff;
	dc->dc_ht = (videosize >> 16) & 0xffff;
	dc->dc_depth = videobitdepth;
	dc->dc_rowbytes = videorowbytes;

	dc->dc_size = (mac68k_vidlen > 0) ?
	    mac68k_vidlen : dc->dc_ht * dc->dc_rowbytes;
	dc->dc_offset = m68k_page_offset(mac68k_vidphys);

	/* set up the display */
	macfb_init(&macfb_console_dc);

	rcons_alloc_attr(&dc->dc_rcons, 0, 0, 0, &defattr);

	wsdisplay_cnattach(&macfb_stdscreen, &dc->dc_rcons,
			0, 0, defattr);

	macfb_consaddr = addr;
	return (0);
}

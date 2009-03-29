/*	$OpenBSD: cfb.c,v 1.21 2009/03/29 21:53:52 sthen Exp $	*/
/*	$NetBSD: cfb.c,v 1.7 1996/12/05 01:39:39 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/ioctl.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/tc/tcvar.h>
#include <machine/cfbreg.h>
#include <alpha/tc/cfbvar.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>
#include <machine/fbio.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

int	cfbmatch(struct device *, void *, void *);
void	cfbattach(struct device *, struct device *, void *);

struct cfattach cfb_ca = {
	sizeof(struct cfb_softc), cfbmatch, cfbattach,
};

struct cfdriver cfb_cd = {
	NULL, "cfb", DV_DULL,
};

void	cfb_getdevconfig(tc_addr_t dense_addr, struct cfb_devconfig *dc);
struct cfb_devconfig cfb_console_dc;
tc_addr_t cfb_consaddr;

struct wsdisplay_emulops cfb_emulfuncs = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr cfb_stdscreen = {
        "std",
        0, 0,        /* will be filled in -- XXX shouldn't, it's global */
        &cfb_emulfuncs,
        0, 0
};

const struct wsscreen_descr *_cfb_scrlist[] = {
        &cfb_stdscreen,
        /* XXX other formats, graphics screen? */
};

struct wsscreen_list cfb_screenlist = {
        sizeof(_cfb_scrlist) / sizeof(struct wsscreen_descr *), _cfb_scrlist
};

int	cfbioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	cfbmmap(void *, off_t, int);

int	cfbintr(void *);
static int      cfb_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
static void     cfb_free_screen(void *, void *);
static int      cfb_show_screen(void *, void *, int,
		    void (*) (void *, int, int), void *);

struct wsdisplay_accessops cfb_accessops = {
	cfbioctl,
	cfbmmap,
	cfb_alloc_screen,
	cfb_free_screen,
	cfb_show_screen,
};

int
cfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAG-BA ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (10);
}

void
cfb_getdevconfig(dense_addr, dc)
	tc_addr_t dense_addr;
	struct cfb_devconfig *dc;
{
	struct raster *rap;
	struct rcons *rcp;
	char *ramdacregp;
	int i;

	dc->dc_vaddr = dense_addr;
	dc->dc_paddr = ALPHA_K0SEG_TO_PHYS(dc->dc_vaddr);	/* XXX */
	dc->dc_size = CFB_SIZE;

	ramdacregp = (char *)dc->dc_vaddr + CFB_RAMDAC_OFFSET;

	dc->dc_wid = 1024;
	dc->dc_ht = 864;
	dc->dc_depth = 8;			/* 8 plane */
	dc->dc_rowbytes = dc->dc_wid * (dc->dc_depth / 8);

	dc->dc_videobase = dc->dc_vaddr + CFB_FB_OFFSET;
	
	/* Initialize the RAMDAC/colormap */
	/* start XXX XXX XXX */
	(*(volatile u_int32_t *)(ramdacregp + CFB_RAMDAC_ADDRLOW)) = 0;
	(*(volatile u_int32_t *)(ramdacregp + CFB_RAMDAC_ADDRHIGH)) = 0;
	tc_wmb();
	for (i = 0; i < 256; i++) {
		(*(volatile u_int32_t *)(ramdacregp + CFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + CFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + CFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
	}
	/* end XXX XXX XXX */
	
	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0x00000000;

	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = 8;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)dc->dc_videobase;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	cfb_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	cfb_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

void
cfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cfb_softc *sc = (struct cfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct wsemuldisplaydev_attach_args waa;
	int console;

	console = (ta->ta_addr == cfb_consaddr);
	if (console) {
		sc->sc_dc = &cfb_console_dc;
		sc->nscreens = 1;
	} else {
		sc->sc_dc = (struct cfb_devconfig *)
		    malloc(sizeof(struct cfb_devconfig), M_DEVBUF, M_WAITOK);
		cfb_getdevconfig(ta->ta_addr, sc->sc_dc);
	}
	if (sc->sc_dc->dc_vaddr == NULL) {
		printf(": can't map mem space\n");
		return;
	}
	printf(": %d x %d, %dbpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_depth);

	/* Establish an interrupt handler, and clear any pending interrupts */
        tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, cfbintr, sc,
	    self->dv_xname);
	*(volatile u_int32_t *)(sc->sc_dc->dc_vaddr + CFB_IREQCTRL_OFFSET) = 0;

	/* initialize the raster */
	waa.console = console;
	waa.scrdata = &cfb_screenlist;
	waa.accessops = &cfb_accessops;
	waa.accesscookie = sc;
	waa.defaultscreens = 0;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
cfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct cfb_softc *sc = v;
	struct cfb_devconfig *dc = sc->sc_dc;

	switch (cmd) {
	case FBIOGTYPE:
#define fbt ((struct fbtype *)data)
		fbt->fb_type = FBTYPE_CFB;
		fbt->fb_height = sc->sc_dc->dc_ht;
		fbt->fb_width = sc->sc_dc->dc_wid;
		fbt->fb_depth = sc->sc_dc->dc_depth;
		fbt->fb_cmsize = 256;		/* XXX ??? */
		fbt->fb_size = sc->sc_dc->dc_size;
#undef fbt
		return (0);

#if 0
	case FBIOPUTCMAP:
		return (*tgar->tgar_set_cmap)(dc, (struct fbcmap *)data);

	case FBIOGETCMAP:
		return (*tgar->tgar_get_cmap)(dc, (struct fbcmap *)data);
#endif

	case FBIOGATTR:
		return (ENOTTY);			/* XXX ? */

#if 0
	case FBIOSVIDEO:
		if (*(int *)data == FBVIDEO_OFF)
			cfb_blank(sc->sc_dc);
		else
			cfb_unblank(sc->sc_dc);
		return (0);
#endif

	case FBIOGVIDEO:
		*(int *)data = dc->dc_blanked ? FBVIDEO_OFF : FBVIDEO_ON;
		return (0);

#if 0
	case FBIOSCURSOR:
		return (*tgar->tgar_set_cursor)(dc, (struct fbcursor *)data);

	case FBIOGCURSOR:
		return (*tgar->tgar_get_cursor)(dc, (struct fbcursor *)data);

	case FBIOSCURPOS:
		return (*tgar->tgar_set_curpos)(dc, (struct fbcurpos *)data);

	case FBIOGCURPOS:
		return (*tgar->tgar_get_curpos)(dc, (struct fbcurpos *)data);

	case FBIOGCURMAX:
		return (*tgar->tgar_get_curmax)(dc, (struct fbcurpos *)data);
#endif
	}
	return (-1);
}

paddr_t
cfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct cfb_softc *sc = v;

	if (offset > CFB_SIZE)
		return (-1);
	return atop(sc->sc_dc->dc_paddr + offset);
}

int
cfbintr(v)
	void *v;
{
	struct cfb_softc *sc = v;

	*(volatile u_int32_t *)(sc->sc_dc->dc_vaddr + CFB_IREQCTRL_OFFSET) = 0;

	return (1);
}

int 
cfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct cfb_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return(0);
}

void
cfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct cfb_softc *sc = v;

	if (sc->sc_dc == &cfb_console_dc)
		panic("cfb_free_screen: console");

	sc->nscreens--;
}

int
cfb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

#if 0
int 
cfb_cnattach(addr)
	tc_addr_t addr;
{
	struct cfb_devconfig *dc = &cfb_console_dc;
	long defattr;

	cfb_getdevconfig(addr, dcp);

	rcons_alloc_attr(&dcp->dc_rcons, 0, 0, 0, &defattr);
	
	wsdisplay_cnattach(&cfb_stdscreen, &dcp->dc_rcons,
		0,0, defattr;);

	cfb_consaddr = addr;
	return (0);
}
#endif

/*	$OpenBSD: sfb.c,v 1.10 1998/11/21 18:13:04 millert Exp $	*/
/*	$NetBSD: sfb.c,v 1.7 1996/12/05 01:39:44 cgd Exp $	*/

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

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/tc/tcvar.h>
#include <machine/sfbreg.h>
#include <alpha/tc/sfbvar.h>
#if 0
#include <alpha/tc/bt459reg.h>
#endif

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsconsvar.h>
#include <machine/fbio.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

#ifdef __BROKEN_INDIRECT_CONFIG
int	sfbmatch __P((struct device *, void *, void *));
#else
int	sfbmatch __P((struct device *, struct cfdata *, void *));
#endif
void	sfbattach __P((struct device *, struct device *, void *));
int	sfbprint __P((void *, const char *));

struct cfattach sfb_ca = {
	sizeof(struct sfb_softc), sfbmatch, sfbattach,
};

struct cfdriver sfb_cd = {
	NULL, "sfb", DV_DULL,
};

void	sfb_getdevconfig __P((tc_addr_t dense_addr, struct sfb_devconfig *dc));
struct sfb_devconfig sfb_console_dc;

struct wscons_emulfuncs sfb_emulfuncs = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_putstr,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_setattr,
};

int	sfbioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	sfbmmap __P((void *, off_t, int));

#if 0
void	sfb_blank __P((struct sfb_devconfig *));
void	sfb_unblank __P((struct sfb_devconfig *));
#endif

int
sfbmatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAGB-BA", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (10);
}

void
sfb_getdevconfig(dense_addr, dc)
	tc_addr_t dense_addr;
	struct sfb_devconfig *dc;
{
	struct raster *rap;
	struct rcons *rcp;
	char *regp, *ramdacregp;
	int i;

	dc->dc_vaddr = dense_addr;
	dc->dc_paddr = ALPHA_K0SEG_TO_PHYS(dc->dc_vaddr);	/* XXX */
	dc->dc_size = SFB_SIZE;

	regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;
	ramdacregp = (char *)dc->dc_vaddr + SFB_RAMDAC_OFFSET;

	dc->dc_wid =
	    (*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_HSETUP) & 0x1ff) * 4;
	dc->dc_ht =
	    (*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VSETUP) & 0x7ff);

	switch (*(volatile u_int32_t *)(regp + SFB_ASIC_DEEP)) {
	case 0:
	case 1:					/* XXX by the book; wrong? */
		dc->dc_depth = 8;		/* 8 plane */
		break;
	case 2:
		dc->dc_depth = 16;		/* 16 plane */
		break;
	case 4:
		dc->dc_depth = 32;		/* 32 plane */
		break;
	default:
		dc->dc_depth = 8;		/* XXX can't happen? */
		break;
	}

	dc->dc_rowbytes = dc->dc_wid * (dc->dc_depth / 8);

	dc->dc_videobase = dc->dc_vaddr + SFB_FB_OFFSET +
	    ((*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_BASE)) *
	     4096 * (dc->dc_depth / 8));
	
	(*(volatile u_int32_t *)(regp + SFB_ASIC_MODE)) = 0;
	tc_wmb();
	(*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID)) = 1;
	tc_wmb();

	/*
	 * Set all bits in the pixel mask, to enable writes to all pixels.
	 * It seems that the console firmware clears some of them
	 * under some circumstances, which causes cute vertical stripes.
	 */
	(*(volatile u_int32_t *)(regp + SFB_ASIC_PIXELMASK)) = 0xffffffff;
	tc_wmb();
	(*(volatile u_int32_t *)(regp + SFB_ASIC_PLANEMASK)) = 0xffffffff;
	tc_wmb();

	/* Initialize the RAMDAC/colormap */
	/* start XXX XXX XXX */
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) = 0;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) = 0;
	tc_wmb();
	for (i = 0; i < 256; i++) {
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    i ? 0xff : 0;
		tc_wmb();
	}
	/* end XXX XXX XXX */
	
	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0x00000000;

	/* initialize the raster */
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
}

void
sfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sfb_softc *sc = (struct sfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct wscons_attach_args waa;
	struct wscons_odev_spec *wo;
	int console;

	console = 0;					/* XXX */
	if (console)
		sc->sc_dc = &sfb_console_dc;
	else {
		sc->sc_dc = (struct sfb_devconfig *)
		    malloc(sizeof(struct sfb_devconfig), M_DEVBUF, M_WAITOK);
		sfb_getdevconfig(ta->ta_addr, sc->sc_dc);
	}
	if (sc->sc_dc->dc_vaddr == NULL) {
		printf(": couldn't map memory space; punt!\n");
		return;
	}
	printf(": %d x %d, %dbpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_depth);

#if 0
	x = (char *)ta->ta_addr + SFB_ASIC_OFFSET;
	printf("%s: Video Base Address = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_BASE));
	printf("%s: Horizontal Setup = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_HSETUP));
	printf("%s: Vertical Setup = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_VSETUP));
#endif

	waa.waa_isconsole = console;
	wo = &waa.waa_odev_spec;

	wo->wo_emulfuncs = &sfb_emulfuncs;
	wo->wo_emulfuncs_cookie = &sc->sc_dc->dc_rcons;

	wo->wo_ioctl = sfbioctl;
	wo->wo_mmap = sfbmmap;
	wo->wo_miscfuncs_cookie = sc;

	wo->wo_nrows = sc->sc_dc->dc_rcons.rc_maxrow;
	wo->wo_ncols = sc->sc_dc->dc_rcons.rc_maxcol;
	wo->wo_crow = 0;
	wo->wo_ccol = 0;

	config_found(self, &waa, sfbprint);
}

int
sfbprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wscons at %s", pnp);
	return (UNCONF);
}

int
sfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct sfb_softc *sc = v;
	struct sfb_devconfig *dc = sc->sc_dc;

	switch (cmd) {
	case FBIOGTYPE:
#define fbt ((struct fbtype *)data)
		fbt->fb_type = FBTYPE_SFB;
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
			sfb_blank(sc->sc_dc);
		else
			sfb_unblank(sc->sc_dc);
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

int
sfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct sfb_softc *sc = v;

	if (offset >= SFB_SIZE || offset < 0)
		return (-1);
	return alpha_btop(sc->sc_dc->dc_paddr + offset);
}

#if 0
void
tga_console(bc, pc, bus, device, function)
	bus_chipset_tag_t bc;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct tga_devconfig *dcp = &tga_console_dc;
	struct wscons_odev_spec wo;

	tga_getdevconfig(bc, pc, pci_make_tag(pc, bus, device, function), dcp);

	/* sanity checks */
	if (dcp->dc_vaddr == NULL)
		panic("tga_console(%d, %d): couldn't map memory space",
		    device, function);
	if (dcp->dc_tgaconf == NULL)
		panic("tga_console(%d, %d): unknown board configuration",
		    device, function);

	/*
	 * Initialize the RAMDAC but DO NOT allocate any private storage.
	 * Initialization includes disabling cursor, setting a sane
	 * colormap, etc.  It will be reinitialized in tgaattach().
	 */
	(*dcp->dc_tgaconf->tgac_ramdac->tgar_init)(dcp, 0);

	wo.wo_ef = &tga_emulfuncs;
	wo.wo_efa = &dcp->dc_rcons;
	wo.wo_nrows = dcp->dc_rcons.rc_maxrow;
	wo.wo_ncols = dcp->dc_rcons.rc_maxcol;
	wo.wo_crow = 0;
	wo.wo_ccol = 0;
	/* ioctl and mmap are unused until real attachment. */

	wscons_attach_console(&wo);
}
#endif

#if 0
/*
 * Functions to blank and unblank the display.
 */
void
sfb_blank(dc)
	struct sfb_devconfig *dc;
{
	char *regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;

	if (!dc->dc_blanked) {
		dc->dc_blanked = 1;
	    	*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID) = 0;
		tc_wmb();
	}
}

void
sfb_unblank(dc)
	struct sfb_devconfig *dc;
{
	char *regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;
	
	if (dc->dc_blanked) {
		dc->dc_blanked = 0;
	    	*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID) = 1;
		tc_wmb();
	}
}
#endif

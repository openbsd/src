/*	$OpenBSD: tga.c,v 1.12 1998/11/21 18:25:48 millert Exp $	*/
/*	$NetBSD: tga.c,v 1.13 1996/12/05 01:39:37 cgd Exp $	*/

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <machine/tgareg.h>
#include <alpha/pci/tgavar.h>
#include <alpha/pci/bt485reg.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsconsvar.h>
#include <machine/fbio.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

#ifdef __BROKEN_INDIRECT_CONFIG
int	tgamatch __P((struct device *, void *, void *));
#else
int	tgamatch __P((struct device *, struct cfdata *, void *));
#endif
void	tgaattach __P((struct device *, struct device *, void *));
int	tgaprint __P((void *, const char *));

struct cfattach tga_ca = {
	sizeof(struct tga_softc), tgamatch, tgaattach,
};

struct cfdriver tga_cd = {
	NULL, "tga", DV_DULL,
};

int	tga_identify __P((tga_reg_t *));
const struct tga_conf *tga_getconf __P((int));
void	tga_getdevconfig __P((bus_space_tag_t memt, pci_chipset_tag_t pc,
	    pcitag_t tag, struct tga_devconfig *dc));

struct tga_devconfig tga_console_dc;

struct wscons_emulfuncs tga_emulfuncs = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_putstr,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_setattr,
};

int	tgaioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	tgammap __P((void *, off_t, int));

void	tga_blank __P((struct tga_devconfig *));
void	tga_unblank __P((struct tga_devconfig *));

int
tgamatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_DEC ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_DEC_21030)
		return (0);

	return (10);
}

void
tga_getdevconfig(memt, pc, tag, dc)
	bus_space_tag_t memt;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct tga_devconfig *dc;
{
	const struct tga_conf *tgac;
	const struct tga_ramdac_conf *tgar;
	struct raster *rap;
	struct rcons *rcp;
	bus_size_t pcisize;
	int i, cacheable;

	dc->dc_memt = memt;
	dc->dc_pc = pc;

	dc->dc_pcitag = tag;

	/* XXX MAGIC NUMBER */
	pci_mem_find(pc, tag, 0x10, &dc->dc_pcipaddr, &pcisize,
	    &cacheable);
	if (!cacheable)						/* sanity */
		panic("tga_getdevconfig: memory not cacheable?");

	/* XXX XXX XXX */
	if (bus_space_map(memt, dc->dc_pcipaddr, pcisize, 1, &dc->dc_vaddr))
		return;
	dc->dc_paddr = ALPHA_K0SEG_TO_PHYS(dc->dc_vaddr);	/* XXX */

	dc->dc_regs = (tga_reg_t *)(dc->dc_vaddr + TGA_MEM_CREGS);
	dc->dc_tga_type = tga_identify(dc->dc_regs);
	tgac = dc->dc_tgaconf = tga_getconf(dc->dc_tga_type);
	if (tgac == NULL)
		return;

#if 0
	/* XXX on the Alpha, pcisize = 4 * cspace_size. */
	if (tgac->tgac_cspace_size != pcisize)			/* sanity */
		panic("tga_getdevconfig: memory size mismatch?");
#endif

	tgar = tgac->tgac_ramdac;

	switch (dc->dc_regs[TGA_REG_VHCR] & 0x1ff) {		/* XXX */
	case 0:
		dc->dc_wid = 8192;
		break;

	case 1:
		dc->dc_wid = 8196;
		break;

	default:
		dc->dc_wid = (dc->dc_regs[TGA_REG_VHCR] & 0x1ff) * 4; /* XXX */
		break;
	}

	dc->dc_rowbytes = dc->dc_wid * (dc->dc_tgaconf->tgac_phys_depth / 8);

	if ((dc->dc_regs[TGA_REG_VHCR] & 0x00000001) != 0 &&	/* XXX */
	    (dc->dc_regs[TGA_REG_VHCR] & 0x80000000) != 0) {	/* XXX */
		dc->dc_wid -= 4;
		/*
		 * XXX XXX turning off 'odd' shouldn't be necesssary,
		 * XXX XXX but i can't make X work with the weird size.
		 */
		dc->dc_regs[TGA_REG_VHCR] &= ~0x80000001;
		dc->dc_rowbytes =
		    dc->dc_wid * (dc->dc_tgaconf->tgac_phys_depth / 8);
	}

	dc->dc_ht = (dc->dc_regs[TGA_REG_VVCR] & 0x7ff);	/* XXX */

	/* XXX this seems to be what DEC does */
	dc->dc_regs[TGA_REG_CCBR] = 0;
	dc->dc_regs[TGA_REG_VVBR] = 1;
	dc->dc_videobase = dc->dc_vaddr + tgac->tgac_dbuf[0] +
	    1 * tgac->tgac_vvbr_units;
	dc->dc_blanked = 1;
	tga_unblank(dc);
	
	/*
	 * Set all bits in the pixel mask, to enable writes to all pixels.
	 * It seems that the console firmware clears some of them
	 * under some circumstances, which causes cute vertical stripes.
	 */
	dc->dc_regs[TGA_REG_GPXR_P] = 0xffffffff;

	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0;

	/* initialize the raster */
	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = tgac->tgac_phys_depth;
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
tgaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct tga_softc *sc = (struct tga_softc *)self;
	struct wscons_attach_args waa;
	struct wscons_odev_spec *wo;
	pci_intr_handle_t intrh;
	const char *intrstr;
	u_int8_t rev;
	int console;

	console = (pa->pa_tag == tga_console_dc.dc_pcitag);
	if (console)
		sc->sc_dc = &tga_console_dc;
	else {
		sc->sc_dc = (struct tga_devconfig *)
		    malloc(sizeof(struct tga_devconfig), M_DEVBUF, M_WAITOK);
		tga_getdevconfig(pa->pa_memt, pa->pa_pc, pa->pa_tag, sc->sc_dc);
	}
	if (sc->sc_dc->dc_vaddr == NULL) {
		printf(": couldn't map memory space; punt!\n");
		return;
	}

	/* XXX say what's going on. */
	intrstr = NULL;
	if (sc->sc_dc->dc_tgaconf->tgac_ramdac->tgar_intr != NULL) {
		if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
		    pa->pa_intrline, &intrh)) {
			printf(": couldn't map interrupt");
			return;
		}
		intrstr = pci_intr_string(pa->pa_pc, intrh);
		sc->sc_intr = pci_intr_establish(pa->pa_pc, intrh, IPL_TTY,
		    sc->sc_dc->dc_tgaconf->tgac_ramdac->tgar_intr, sc->sc_dc,
		    sc->sc_dev.dv_xname);
		if (sc->sc_intr == NULL) {
			printf(": couldn't establish interrupt");
			if (intrstr != NULL)
				printf("at %s", intrstr);
			printf("\n");
			return;
		}
	}

	/*
	 * Initialize the RAMDAC and allocate any private storage it needs.
	 * Initialization includes disabling cursor, setting a sane
	 * colormap, etc.
	 */
	(*sc->sc_dc->dc_tgaconf->tgac_ramdac->tgar_init)(sc->sc_dc, 1);

	printf(": DC21030 ");
	rev = PCI_REVISION(pa->pa_class);
	switch (rev) {
	case 1: case 2: case 3:
		printf("step %c", 'A' + rev - 1);
		break;

	default:
		printf("unknown stepping (0x%x)", rev);
		break;
	}
	printf(", ");

	if (sc->sc_dc->dc_tgaconf == NULL) {
		printf("unknown board configuration\n");
		return;
	}
	printf("board type %s\n", sc->sc_dc->dc_tgaconf->tgac_name);
	printf("%s: %d x %d, %dbpp, %s RAMDAC\n", sc->sc_dev.dv_xname,
	    sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_tgaconf->tgac_phys_depth,
	    sc->sc_dc->dc_tgaconf->tgac_ramdac->tgar_name);

	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname,
		    intrstr);

	waa.waa_isconsole = console;

	wo = &waa.waa_odev_spec;

	wo->wo_emulfuncs = &tga_emulfuncs;
	wo->wo_emulfuncs_cookie = &sc->sc_dc->dc_rcons;

	wo->wo_ioctl = tgaioctl;
	wo->wo_mmap = tgammap;
	wo->wo_miscfuncs_cookie = sc;

	wo->wo_nrows = sc->sc_dc->dc_rcons.rc_maxrow;
	wo->wo_ncols = sc->sc_dc->dc_rcons.rc_maxcol;
	wo->wo_crow = 0;
	wo->wo_ccol = 0;

	config_found(self, &waa, tgaprint);
}

int
tgaprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wscons at %s", pnp);
	return (UNCONF);
}

int
tgaioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct tga_softc *sc = v;
	struct tga_devconfig *dc = sc->sc_dc;
	const struct tga_ramdac_conf *tgar = dc->dc_tgaconf->tgac_ramdac;

	switch (cmd) {
	case FBIOGTYPE:
#define fbt ((struct fbtype *)data)
		fbt->fb_type = FBTYPE_TGA;
		fbt->fb_height = sc->sc_dc->dc_ht;
		fbt->fb_width = sc->sc_dc->dc_wid;
		fbt->fb_depth = sc->sc_dc->dc_tgaconf->tgac_phys_depth;
		fbt->fb_cmsize = 256;		/* XXX ??? */
		fbt->fb_size = sc->sc_dc->dc_tgaconf->tgac_cspace_size;
#undef fbt
		return (0);

	case FBIOPUTCMAP:
		return (*tgar->tgar_set_cmap)(dc, (struct fbcmap *)data);

	case FBIOGETCMAP:
		return (*tgar->tgar_get_cmap)(dc, (struct fbcmap *)data);

	case FBIOGATTR:
		return (ENOTTY);			/* XXX ? */

	case FBIOSVIDEO:
		if (*(int *)data == FBVIDEO_OFF)
			tga_blank(sc->sc_dc);
		else
			tga_unblank(sc->sc_dc);
		return (0);

	case FBIOGVIDEO:
		*(int *)data = dc->dc_blanked ? FBVIDEO_OFF : FBVIDEO_ON;
		return (0);

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
	}
	return (-1);
}

int
tgammap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct tga_softc *sc = v;

	if (offset >= sc->sc_dc->dc_tgaconf->tgac_cspace_size || offset < 0)
		return -1;
	return alpha_btop(sc->sc_dc->dc_paddr + offset);
}

void
tga_console(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct tga_devconfig *dcp = &tga_console_dc;
	struct wscons_odev_spec wo;

	tga_getdevconfig(memt, pc, pci_make_tag(pc, bus, device, function), dcp);

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

	wo.wo_emulfuncs = &tga_emulfuncs;
	wo.wo_emulfuncs_cookie = &dcp->dc_rcons;

	/* ioctl and mmap are unused until real attachment. */

	wo.wo_nrows = dcp->dc_rcons.rc_maxrow;
	wo.wo_ncols = dcp->dc_rcons.rc_maxcol;
	wo.wo_crow = 0;
	wo.wo_ccol = 0;

	wscons_attach_console(&wo);
}

/*
 * Functions to blank and unblank the display.
 */
void
tga_blank(dc)
	struct tga_devconfig *dc;
{

	if (!dc->dc_blanked) {
		dc->dc_blanked = 1;
		dc->dc_regs[TGA_REG_VVVR] |= 0x02;		/* XXX */
	}
}

void
tga_unblank(dc)
	struct tga_devconfig *dc;
{

	if (dc->dc_blanked) {
		dc->dc_blanked = 0;
		dc->dc_regs[TGA_REG_VVVR] &= ~0x02;		/* XXX */
	}
}

/*
 * Functions to manipulate the built-in cursor handing hardware.
 */
int
tga_builtin_set_cursor(dc, fbc)
	struct tga_devconfig *dc;
	struct fbcursor *fbc;
{
	int v;
#if 0
	int count;
#endif

	v = fbc->set;
#if 0
	if (v & FB_CUR_SETCMAP)			/* XXX should be supported */
		return EINVAL;
	if (v & FB_CUR_SETSHAPE) {
		if ((u_int)fbc->size.x != 64 || (u_int)fbc->size.y > 64)
			return (EINVAL);
		/* The cursor is 2 bits deep, and there is no mask */
		count = (fbc->size.y * 64 * 2) / NBBY;
		if (!useracc(fbc->image, count, B_READ))
			return (EFAULT);
	}
	if (v & FB_CUR_SETHOT)			/* not supported */
		return EINVAL;
#endif

	/* parameters are OK; do it */
	if (v & FB_CUR_SETCUR) {
		if (fbc->enable)
			dc->dc_regs[TGA_REG_VVVR] |= 0x04;	/* XXX */
		else
			dc->dc_regs[TGA_REG_VVVR] &= ~0x04;	/* XXX */
	}
#if 0
	if (v & FB_CUR_SETPOS) {
		dc->dc_regs[TGA_REG_CXYR] =
		    ((fbc->pos.y & 0xfff) << 12) | (fbc->pos.x & 0xfff);
	}
	if (v & FB_CUR_SETCMAP) {
		/* XXX */
	}
	if (v & FB_CUR_SETSHAPE) {
		dc->dc_regs[TGA_REG_CCBR] =
		    (dc->dc_regs[TGA_REG_CCBR] & ~0xfc00) | (fbc->size.y << 10);
		copyin(fbc->image, (char *)(dc->dc_vaddr +
		    (dc->dc_regs[TGA_REG_CCBR] & 0x3ff)),
		    count);				/* can't fail. */
	}
#endif
	return (0);
}

int
tga_builtin_get_cursor(dc, fbc)
	struct tga_devconfig *dc;
	struct fbcursor *fbc;
{
	int count, error;

	fbc->set = FB_CUR_SETALL & ~(FB_CUR_SETHOT | FB_CUR_SETCMAP);
	fbc->enable = (dc->dc_regs[TGA_REG_VVVR] & 0x04) != 0;
	fbc->pos.x = dc->dc_regs[TGA_REG_CXYR] & 0xfff;
	fbc->pos.y = (dc->dc_regs[TGA_REG_CXYR] >> 12) & 0xfff;
	fbc->size.x = 64;
	fbc->size.y = (dc->dc_regs[TGA_REG_CCBR] >> 10) & 0x3f;

	if (fbc->image != NULL) {
		count = (fbc->size.y * 64 * 2) / NBBY;
		error = copyout((char *)(dc->dc_vaddr +
		      (dc->dc_regs[TGA_REG_CCBR] & 0x3ff)),
		    fbc->image, count);
		if (error)
			return (error);
		/* No mask */
	}
	/* XXX No color map */
	return (0);
}

int
tga_builtin_set_curpos(dc, fbp)
	struct tga_devconfig *dc;
	struct fbcurpos *fbp;
{

	dc->dc_regs[TGA_REG_CXYR] =
	    ((fbp->y & 0xfff) << 12) | (fbp->x & 0xfff);
	return (0);
}

int
tga_builtin_get_curpos(dc, fbp)
	struct tga_devconfig *dc;
	struct fbcurpos *fbp;
{

	fbp->x = dc->dc_regs[TGA_REG_CXYR] & 0xfff;
	fbp->y = (dc->dc_regs[TGA_REG_CXYR] >> 12) & 0xfff;
	return (0);
}

int
tga_builtin_get_curmax(dc, fbp)
	struct tga_devconfig *dc;
	struct fbcurpos *fbp;
{

	fbp->x = fbp->y = 64;
	return (0);
}

/* $NetBSD: tga.c,v 1.15 1999/12/06 19:25:59 drochner Exp $ */

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
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/tgareg.h>
#include <dev/pci/tgavar.h>
#include <dev/ic/bt485reg.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

int	tgamatch __P((struct device *, struct cfdata *, void *));
void	tgaattach __P((struct device *, struct device *, void *));
int	tgaprint __P((void *, const char *));

struct cfdriver tga_cd = {
	NULL, "tga", DV_DULL
};

struct cfattach tga_ca = {
	sizeof(struct tga_softc), (cfmatch_t)tgamatch, tgaattach,
};

int	tga_identify __P((tga_reg_t *));
const struct tga_conf *tga_getconf __P((int));
void	tga_getdevconfig __P((bus_space_tag_t memt, pci_chipset_tag_t pc,
	    pcitag_t tag, struct tga_devconfig *dc));

struct tga_devconfig tga_console_dc;

int tga_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
int tga_mmap __P((void *, off_t, int));
static void tga_copyrows __P((void *, int, int, int));
static void tga_copycols __P((void *, int, int, int, int));
static int tga_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
static void tga_free_screen __P((void *, void *));
static int tga_show_screen __P((void *, void *, int,
				void (*) (void *, int, int), void *));
static int tga_rop __P((struct raster *, int, int, int, int, int,
	struct raster *, int, int));
static int tga_rop_nosrc __P((struct raster *, int, int, int, int, int));
static int tga_rop_htov __P((struct raster *, int, int, int, int,
	int, struct raster *, int, int ));
static int tga_rop_vtov __P((struct raster *, int, int, int, int,
	int, struct raster *, int, int ));

struct wsdisplay_emulops tga_emulops = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_mapchar,
	rcons_putchar,
	tga_copycols,
	rcons_erasecols,
	tga_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr tga_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	&tga_emulops,
	0, 0,
	WSSCREEN_REVERSE
};

const struct wsscreen_descr *_tga_scrlist[] = {
	&tga_stdscreen,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list tga_screenlist = {
	sizeof(_tga_scrlist) / sizeof(struct wsscreen_descr *), _tga_scrlist
};

struct wsdisplay_accessops tga_accessops = {
	tga_ioctl,
	tga_mmap,
	tga_alloc_screen,
	tga_free_screen,
	tga_show_screen,
	0 /* load_font */
};

void	tga_blank __P((struct tga_devconfig *));
void	tga_unblank __P((struct tga_devconfig *));

int
tgamatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
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

	/* XXX magic number */
	pci_mem_find(pc, tag, 0x10, &dc->dc_pcipaddr, &pcisize, &cacheable);
	if (!cacheable)
		panic("tga memory not cacheable");

	/* XXX XXX XXX */
	if ((i = bus_space_map(memt, dc->dc_pcipaddr, pcisize, 1, &dc->dc_vaddr))) {
printf("bus_space_map(%x, %x, %x, 0, %p): %d\n", memt, dc->dc_pcipaddr, pcisize, &dc->dc_vaddr, i);
		DELAY(90000000);
		return;
	}
#ifdef __alpha__
	dc->dc_paddr = ALPHA_K0SEG_TO_PHYS(dc->dc_vaddr);	/* XXX */
#else
#error "find a way to deal w/ paddr here"
#endif

	dc->dc_regs = (tga_reg_t *)(dc->dc_vaddr + TGA_MEM_CREGS);
	dc->dc_tga_type = tga_identify(dc->dc_regs);
	tgac = dc->dc_tgaconf = tga_getconf(dc->dc_tga_type);
	if (tgac == NULL) {
printf("tgac==0\n");
		return;
	}

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
	rap->data = (caddr_t)dc;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	tga_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	tga_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

void
tgaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct tga_softc *sc = (struct tga_softc *)self;
	struct wsemuldisplaydev_attach_args aa;
	pci_intr_handle_t intrh;
	const char *intrstr;
	u_int8_t rev;
	int console;

#ifdef __alpha__
	console = (pa->pa_tag == tga_console_dc.dc_pcitag);
#else
	console = 0;
#endif
	if (console) {
		sc->sc_dc = &tga_console_dc;
		sc->nscreens = 1;
	} else {
		sc->sc_dc = (struct tga_devconfig *)
		    malloc(sizeof(struct tga_devconfig), M_DEVBUF, M_WAITOK);
		bzero(sc->sc_dc, sizeof(struct tga_devconfig));
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

	aa.console = console;
	aa.scrdata = &tga_screenlist;
	aa.accessops = &tga_accessops;
	aa.accesscookie = sc;

	config_found(self, &aa, wsemuldisplaydevprint);
}

int
tga_ioctl(v, cmd, data, flag, p)
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
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_TGA;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = sc->sc_dc->dc_ht;
		wsd_fbip->width = sc->sc_dc->dc_wid;
		wsd_fbip->depth = sc->sc_dc->dc_tgaconf->tgac_phys_depth;
		wsd_fbip->cmsize = 256;		/* XXX ??? */
#undef wsd_fbip
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return (*tgar->tgar_get_cmap)(dc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return (*tgar->tgar_set_cmap)(dc,
		    (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
		if (*(u_int *)data == WSDISPLAYIO_VIDEO_OFF)
			tga_blank(sc->sc_dc);
		else
			tga_unblank(sc->sc_dc);
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = dc->dc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_GCURPOS:
		return (*tgar->tgar_get_curpos)(dc,
		    (struct wsdisplay_curpos *)data);

	case WSDISPLAYIO_SCURPOS:
		return (*tgar->tgar_set_curpos)(dc,
		    (struct wsdisplay_curpos *)data);

	case WSDISPLAYIO_GCURMAX:
		return (*tgar->tgar_get_curmax)(dc,
		    (struct wsdisplay_curpos *)data);

	case WSDISPLAYIO_GCURSOR:
		return (*tgar->tgar_get_cursor)(dc,
		    (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		return (*tgar->tgar_set_cursor)(dc,
		    (struct wsdisplay_cursor *)data);
	}
	return (-1);
}

int
tga_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{

	/* XXX NEW MAPPING CODE... */

#ifdef __alpha__
	struct tga_softc *sc = v;

	if (offset >= sc->sc_dc->dc_tgaconf->tgac_cspace_size || offset < 0)
		return -1;
	return alpha_btop(sc->sc_dc->dc_paddr + offset);
#else
	return (-1);
#endif
}

int
tga_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct tga_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

void
tga_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct tga_softc *sc = v;

	if (sc->sc_dc == &tga_console_dc)
		panic("tga_free_screen: console");

	sc->nscreens--;
}

int
tga_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
{

	return (0);
}

int
tga_cnattach(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct tga_devconfig *dcp = &tga_console_dc;
	long defattr;

	tga_getdevconfig(memt, pc,
	    pci_make_tag(pc, bus, device, function), dcp);

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

	rcons_alloc_attr(&dcp->dc_rcons, 0, 0, 0, &defattr);

	wsdisplay_cnattach(&tga_stdscreen, &dcp->dc_rcons,
			   0, 0, defattr);

	return(0);
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
		dc->dc_regs[TGA_REG_VVVR] |= VVR_BLANK;		/* XXX */
	}
}

void
tga_unblank(dc)
	struct tga_devconfig *dc;
{

	if (dc->dc_blanked) {
		dc->dc_blanked = 0;
		dc->dc_regs[TGA_REG_VVVR] &= ~VVR_BLANK;	/* XXX */
	}
}

/*
 * Functions to manipulate the built-in cursor handing hardware.
 */
int
tga_builtin_set_cursor(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	int count, error, v;

	v = cursorp->which;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		error = (*dc->dc_tgaconf->tgac_ramdac->tgar_check_curcmap)(dc,
		    cursorp);
		if (error)
			return (error);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if ((u_int)cursorp->size.x != 64 ||
		    (u_int)cursorp->size.y > 64)
			return (EINVAL);
		/* The cursor is 2 bits deep, and there is no mask */
		count = (cursorp->size.y * 64 * 2) / NBBY;
#if defined(UVM)
		if (!uvm_useracc(cursorp->image, count, B_READ))
			return (EFAULT);
#else
		if (!useracc(cursorp->image, count, B_READ))
			return (EFAULT);
#endif
	}
	if (v & WSDISPLAY_CURSOR_DOHOT)		/* not supported */
		return EINVAL;

	/* parameters are OK; do it */
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		if (cursorp->enable)
			dc->dc_regs[TGA_REG_VVVR] |= 0x04;	/* XXX */
		else
			dc->dc_regs[TGA_REG_VVVR] &= ~0x04;	/* XXX */
	}
	if (v & WSDISPLAY_CURSOR_DOPOS) {
		dc->dc_regs[TGA_REG_CXYR] = ((cursorp->pos.y & 0xfff) << 12) |
		    (cursorp->pos.x & 0xfff);
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		/* can't fail. */
		(*dc->dc_tgaconf->tgac_ramdac->tgar_set_curcmap)(dc, cursorp);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		count = ((64 * 2) / NBBY) * cursorp->size.y;
		dc->dc_regs[TGA_REG_CCBR] =
		    (dc->dc_regs[TGA_REG_CCBR] & ~0xfc00) |
		    (cursorp->size.y << 10);
		copyin(cursorp->image, (char *)(dc->dc_vaddr +
		    (dc->dc_regs[TGA_REG_CCBR] & 0x3ff)),
		    count);				/* can't fail. */
	}
	return (0);
}

int
tga_builtin_get_cursor(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	int count, error;

	cursorp->which = WSDISPLAY_CURSOR_DOALL &
	    ~(WSDISPLAY_CURSOR_DOHOT | WSDISPLAY_CURSOR_DOCMAP);
	cursorp->enable = (dc->dc_regs[TGA_REG_VVVR] & 0x04) != 0;
	cursorp->pos.x = dc->dc_regs[TGA_REG_CXYR] & 0xfff;
	cursorp->pos.y = (dc->dc_regs[TGA_REG_CXYR] >> 12) & 0xfff;
	cursorp->size.x = 64;
	cursorp->size.y = (dc->dc_regs[TGA_REG_CCBR] >> 10) & 0x3f;

	if (cursorp->image != NULL) {
		count = (cursorp->size.y * 64 * 2) / NBBY;
		error = copyout((char *)(dc->dc_vaddr +
		      (dc->dc_regs[TGA_REG_CCBR] & 0x3ff)),
		    cursorp->image, count);
		if (error)
			return (error);
		/* No mask */
	}
	error = (*dc->dc_tgaconf->tgac_ramdac->tgar_get_curcmap)(dc, cursorp);
	return (error);
}

int
tga_builtin_set_curpos(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{

	dc->dc_regs[TGA_REG_CXYR] =
	    ((curposp->y & 0xfff) << 12) | (curposp->x & 0xfff);
	return (0);
}

int
tga_builtin_get_curpos(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{

	curposp->x = dc->dc_regs[TGA_REG_CXYR] & 0xfff;
	curposp->y = (dc->dc_regs[TGA_REG_CXYR] >> 12) & 0xfff;
	return (0);
}

int
tga_builtin_get_curmax(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{

	curposp->x = curposp->y = 64;
	return (0);
}

/*
 * Copy columns (characters) in a row (line).
 */
void
tga_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct rcons *rc = id;
	int y, srcx, dstx, nx;

	y = rc->rc_yorigin + rc->rc_font->height * row;
	srcx = rc->rc_xorigin + rc->rc_font->width * srccol;
	dstx = rc->rc_xorigin + rc->rc_font->width * dstcol;
	nx = rc->rc_font->width * ncols;

	tga_rop(rc->rc_sp, dstx, y,
	    nx, rc->rc_font->height, RAS_SRC,
	    rc->rc_sp, srcx, y);
}

/*
 * Copy rows (lines).
 */
void
tga_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct rcons *rc = id;
	int srcy, dsty, ny;

	srcy = rc->rc_yorigin + rc->rc_font->height * srcrow;
	dsty = rc->rc_yorigin + rc->rc_font->height * dstrow;
	ny = rc->rc_font->height * nrows;

	tga_rop(rc->rc_sp, rc->rc_xorigin, dsty,
	    rc->rc_raswidth, ny, RAS_SRC,
	    rc->rc_sp, rc->rc_xorigin, srcy);
}

/* Do we need the src? */
static int needsrc[16] = { 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0 };

/* A mapping between our API and the TGA card */
static int map_rop[16] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6,
	0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
};

/*
 *  Generic TGA raster op.
 *   This covers all possible raster ops, and
 *   clips the sizes and all of that.
 */
static int
tga_rop(dst, dx, dy, w, h, rop, src, sx, sy)
	struct raster *dst;
	int dx, dy, w, h, rop;
	struct raster *src;
	int sx, sy;
{
	if (!dst)
		return -1;
	if (dst->data == NULL)
		return -1;	/* we should be writing to a screen */
	if (needsrc[RAS_GETOP(rop)]) {
		if (src == (struct raster *) 0)
			return -1;	/* We want a src */
		/* Clip against src */
		if (sx < 0) {
			w += sx;
			sx = 0;
		}
		if (sy < 0) {
			h += sy;
			sy = 0;
		}
		if (sx + w > src->width)
			w = src->width - sx;
		if (sy + h > src->height)
			h = src->height - sy;
	} else {
		if (src != (struct raster *) 0)
			return -1;	/* We need no src */
	}
	/* Clip against dst.  We modify src regardless of using it,
	 * since it really doesn't matter.
	 */
	if (dx < 0) {
		w += dx;
		sx -= dx;
		dx = 0;
	}
	if (dy < 0) {
		h += dy;
		sy -= dy;
		dy = 0;
	}
	if (dx + w > dst->width)
		w = dst->width - dx;
	if (dy + h > dst->height)
		h = dst->height - dy;
	if (w <= 0 || h <= 0)
		return 0;	/* Vacuously true; */
	if (!src)
		return tga_rop_nosrc(dst, dx, dy, w, h, rop);
	if (src->data == NULL)
		return tga_rop_htov(dst, dx, dy, w, h, rop, src, sx, sy);
	else
		return tga_rop_vtov(dst, dx, dy, w, h, rop, src, sx, sy);
}

/*
 * No source raster ops.
 * This function deals with all raster ops that don't require a src.
 */
static int
tga_rop_nosrc(dst, dx, dy, w, h, rop)
	struct raster *dst;
	int dx, dy, w, h, rop;
{
	return raster_op(dst, dx, dy, w, h, rop, NULL, 0, 0);
}

/*
 * Host to Video raster ops.
 * This function deals with all raster ops that have a src that is host memory.
 */
static int
tga_rop_htov(dst, dx, dy, w, h, rop, src, sx, sy)
	struct raster *dst;
	int dx, dy, w, h, rop;
	struct raster *src;
	int sx, sy;
{
	return raster_op(dst, dx, dy, w, h, rop, src, sx, sy);
}

/*
 * Video to Video raster ops.
 * This function deals with all raster ops that have a src and dst
 * that are on the card.
 */
static int
tga_rop_vtov(dst, dx, dy, w, h, rop, src, sx, sy)
	struct raster *dst;
	int dx, dy, w, h, rop;
	struct raster *src;
	int sx, sy;
{
	struct tga_devconfig *dc = (struct tga_devconfig *)dst->data;
	tga_reg_t *regs0 = dc->dc_regs;
	tga_reg_t *regs1 = regs0 + 16 * 1024;	/* register alias 1 */
	tga_reg_t *regs2 = regs1 + 16 * 1024;	/* register alias 2 */
	tga_reg_t *regs3 = regs2 + 16 * 1024;	/* register alias 3 */
	int srcb, dstb;
	int x, y;
	int xstart, xend, xdir, xinc;
	int ystart, yend, ydir, yinc;
	int offset = 1 * dc->dc_tgaconf->tgac_vvbr_units;

	/*
	 * I don't yet want to deal with unaligned guys, really.  And we don't
	 * deal with copies from one card to another.
	 */
	if (dx % 8 != 0 || sx % 8 != 0 || src != dst)
		return raster_op(dst, dx, dy, w, h, rop, src, sx, sy);

	if (sy >= dy) {
		ystart = 0;
		yend = h;
		ydir = 1;
	} else {
		ystart = h;
		yend = 0;
		ydir = -1;
	}
	if (sx >= dx) {
		xstart = 0;
		xend = w * (dst->depth / 8);
		xdir = 1;
	} else {
		xstart = w * (dst->depth / 8);
		xend = 0;
		xdir = -1;
	}
	xinc = xdir * 4 * 64;
	yinc = ydir * dst->linelongs * 4;
	ystart *= dst->linelongs * 4;
	yend *= dst->linelongs * 4;
	srcb = offset + sy  * src->linelongs * 4 + sx;
	dstb = offset + dy  * dst->linelongs * 4 + dx;
	regs3[TGA_REG_GMOR] = 0x0007;		/* Copy mode */
	regs3[TGA_REG_GOPR] = map_rop[rop];	/* Set up the op */
	for (y = ystart; (ydir * y) < (ydir * yend); y += yinc) {
		for (x = xstart; (xdir * x) < (xdir * xend); x += xinc) {
			regs0[TGA_REG_GCSR] = srcb + y + x + 3 * 64;
			regs0[TGA_REG_GCDR] = dstb + y + x + 3 * 64;
			regs1[TGA_REG_GCSR] = srcb + y + x + 2 * 64;
			regs1[TGA_REG_GCDR] = dstb + y + x + 2 * 64;
			regs2[TGA_REG_GCSR] = srcb + y + x + 1 * 64;
			regs2[TGA_REG_GCDR] = dstb + y + x + 1 * 64;
			regs3[TGA_REG_GCSR] = srcb + y + x + 0 * 64;
			regs3[TGA_REG_GCDR] = dstb + y + x + 0 * 64;
		}
	}
	regs0[TGA_REG_GOPR] = 0x0003;		/* op -> dst = src */
	regs0[TGA_REG_GMOR] = 0x0000;		/* Simple mode */
	return 0;
}

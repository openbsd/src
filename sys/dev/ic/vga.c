/* $OpenBSD: vga.c,v 1.37 2005/01/05 23:04:25 miod Exp $ */
/* $NetBSD: vga.c,v 1.28.2.1 2000/06/30 16:27:47 simonb Exp $ */

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

#include "vga.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <machine/bus.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/unicode.h>

#include <dev/ic/pcdisplay.h>

#if 0
#include "opt_wsdisplay_compat.h" /* for WSCONS_SUPPORT_PCVTFONTS */
#endif

static struct vgafont {
	char name[WSFONT_NAME_SIZE];
	int height;
	int encoding;
#ifdef notyet
	int firstchar, numchars;
#endif
	int slot;
} vga_builtinfont = {
	"builtin",
	16,
	WSDISPLAY_FONTENC_IBM,
#ifdef notyet
	0, 256,
#endif
	0
};

struct vgascreen {
	struct pcdisplayscreen pcs;

	LIST_ENTRY(vgascreen) next;

	struct vga_config *cfg;

	/* videostate */
	struct vgafont *fontset1, *fontset2;
	/* font data */
	/* palette */

	int mindispoffset, maxdispoffset;
	int vga_rollover;
};

int vgaconsole, vga_console_type, vga_console_attached;
struct vgascreen vga_console_screen;
struct vga_config vga_console_vc;

int	vga_selectfont(struct vga_config *, struct vgascreen *,
    const char *, const char *);
void	vga_init_screen(struct vga_config *, struct vgascreen *,
    const struct wsscreen_descr *, int, long *);
void	vga_init(struct vga_config *, bus_space_tag_t, bus_space_tag_t);
void	vga_setfont(struct vga_config *, struct vgascreen *);

int	vga_mapchar(void *, int, unsigned int *);
void	vga_putchar(void *, int, int, u_int, long);
int	vga_alloc_attr(void *, int, int, int, long *);
void	vga_copyrows(void *, int, int, int);

static const struct wsdisplay_emulops vga_emulops = {
	pcdisplay_cursor,
	vga_mapchar,
	vga_putchar,
	pcdisplay_copycols,
	pcdisplay_erasecols,
	vga_copyrows,
	pcdisplay_eraserows,
	vga_alloc_attr
};

/*
 * translate WS(=ANSI) color codes to standard pc ones
 */
static unsigned char fgansitopc[] = {
#ifdef __alpha__
	/*
	 * XXX DEC HAS SWITCHED THE CODES FOR BLUE AND RED!!!
	 * XXX We should probably not bother with this
	 * XXX (reinitialize the palette registers).
	 */
	FG_BLACK, FG_BLUE, FG_GREEN, FG_CYAN, FG_RED,
	FG_MAGENTA, FG_BROWN, FG_LIGHTGREY
#else
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
#endif
}, bgansitopc[] = {
#ifdef __alpha__
	BG_BLACK, BG_BLUE, BG_GREEN, BG_CYAN, BG_RED,
	BG_MAGENTA, BG_BROWN, BG_LIGHTGREY
#else
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
#endif
};

const struct wsscreen_descr vga_stdscreen = {
	"80x25", 80, 25,
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, vga_stdscreen_mono = {
	"80x25", 80, 25,
	&vga_emulops,
	8, 16,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, vga_stdscreen_bf = {
	"80x25bf", 80, 25,
	&vga_emulops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
}, vga_40lscreen = {
	"80x40", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, vga_40lscreen_mono = {
	"80x40", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, vga_40lscreen_bf = {
	"80x40bf", 80, 40,
	&vga_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
}, vga_50lscreen = {
	"80x50", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, vga_50lscreen_mono = {
	"80x50", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, vga_50lscreen_bf = {
	"80x50bf", 80, 50,
	&vga_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
};

#define VGA_SCREEN_CANTWOFONTS(type) (!((type)->capabilities & WSSCREEN_HILIT))

const struct wsscreen_descr *_vga_scrlist[] = {
	&vga_stdscreen,
	&vga_stdscreen_bf,
	&vga_40lscreen,
	&vga_40lscreen_bf,
	&vga_50lscreen,
	&vga_50lscreen_bf,
	/* XXX other formats, graphics screen? */
}, *_vga_scrlist_mono[] = {
	&vga_stdscreen_mono,
	&vga_40lscreen_mono,
	&vga_50lscreen_mono,
	/* XXX other formats, graphics screen? */
};

const struct wsscreen_list vga_screenlist = {
	sizeof(_vga_scrlist) / sizeof(struct wsscreen_descr *),
	_vga_scrlist
}, vga_screenlist_mono = {
	sizeof(_vga_scrlist_mono) / sizeof(struct wsscreen_descr *),
	_vga_scrlist_mono
};

int	vga_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	vga_mmap(void *, off_t, int);
int	vga_alloc_screen(void *, const struct wsscreen_descr *,
			 void **, int *, int *, long *);
void	vga_free_screen(void *, void *);
int	vga_show_screen(void *, void *, int,
			void (*) (void *, int, int), void *);
int	vga_load_font(void *, void *, struct wsdisplay_font *);
void	vga_scrollback(void *, void *, int);
void	vga_burner(void *v, u_int on, u_int flags);
u_int16_t vga_getchar(void *, int, int);

void vga_doswitch(struct vga_config *);

const struct wsdisplay_accessops vga_accessops = {
	vga_ioctl,
	vga_mmap,
	vga_alloc_screen,
	vga_free_screen,
	vga_show_screen,
	vga_load_font,
	vga_scrollback,
	vga_getchar,
	vga_burner
};

/*
 * The following functions implement back-end configuration grabbing
 * and attachment.
 */
int
vga_common_probe(iot, memt)
	bus_space_tag_t iot, memt;
{
	bus_space_handle_t ioh_vga, ioh_6845, memh;
	u_int8_t regval;
	u_int16_t vgadata;
	int gotio_vga, gotio_6845, gotmem, mono, rv;
	int dispoffset;

	gotio_vga = gotio_6845 = gotmem = rv = 0;

	if (bus_space_map(iot, 0x3c0, 0x10, 0, &ioh_vga))
		goto bad;
	gotio_vga = 1;

	/* read "misc output register" */
	regval = bus_space_read_1(iot, ioh_vga, 0xc);
	mono = !(regval & 1);

	if (bus_space_map(iot, (mono ? 0x3b0 : 0x3d0), 0x10, 0, &ioh_6845))
		goto bad;
	gotio_6845 = 1;

	if (bus_space_map(memt, 0xa0000, 0x20000, 0, &memh))
		goto bad;
	gotmem = 1;

	dispoffset = (mono ? 0x10000 : 0x18000);

	vgadata = bus_space_read_2(memt, memh, dispoffset);
	bus_space_write_2(memt, memh, dispoffset, 0xa55a);
	if (bus_space_read_2(memt, memh, dispoffset) != 0xa55a)
		goto bad;
	bus_space_write_2(memt, memh, dispoffset, vgadata);

	/*
	 * check if this is really a VGA
	 * (try to write "Color Select" register as XFree86 does)
	 * XXX check before if at least EGA?
	 */
	/* reset state */
	(void) bus_space_read_1(iot, ioh_6845, 10);
	bus_space_write_1(iot, ioh_vga, VGA_ATC_INDEX,
			  20 | 0x20); /* colselect | enable */
	regval = bus_space_read_1(iot, ioh_vga, VGA_ATC_DATAR);
	/* toggle the implemented bits */
	bus_space_write_1(iot, ioh_vga, VGA_ATC_DATAW, regval ^ 0x0f);
	bus_space_write_1(iot, ioh_vga, VGA_ATC_INDEX,
			  20 | 0x20);
	/* read back */
	if (bus_space_read_1(iot, ioh_vga, VGA_ATC_DATAR) != (regval ^ 0x0f))
		goto bad;
	/* restore contents */
	bus_space_write_1(iot, ioh_vga, VGA_ATC_DATAW, regval);

	rv = 1;
bad:
	if (gotio_vga)
		bus_space_unmap(iot, ioh_vga, 0x10);
	if (gotio_6845)
		bus_space_unmap(iot, ioh_6845, 0x10);
	if (gotmem)
		bus_space_unmap(memt, memh, 0x20000);

	return (rv);
}

/*
 * We want at least ASCII 32..127 be present in the
 * first font slot.
 */
#define vga_valid_primary_font(f) \
	(f->encoding == WSDISPLAY_FONTENC_IBM || \
	f->encoding == WSDISPLAY_FONTENC_ISO)

int
vga_selectfont(vc, scr, name1, name2)
	struct vga_config *vc;
	struct vgascreen *scr;
	const char *name1, *name2; /* NULL: take first found */
{
	const struct wsscreen_descr *type = scr->pcs.type;
	struct vgafont *f1, *f2;
	int i;

	f1 = f2 = 0;

	for (i = 0; i < 8; i++) {
		struct vgafont *f = vc->vc_fonts[i];
		if (!f || f->height != type->fontheight)
			continue;
		if (!f1 &&
		    vga_valid_primary_font(f) &&
		    (!name1 || !*name1 ||
		     !strncmp(name1, f->name, WSFONT_NAME_SIZE))) {
			f1 = f;
			continue;
		}
		if (!f2 &&
		    VGA_SCREEN_CANTWOFONTS(type) &&
		    (!name2 || !*name2 ||
		     !strncmp(name2, f->name, WSFONT_NAME_SIZE))) {
			f2 = f;
			continue;
		}
	}

	/*
	 * The request fails if no primary font was found,
	 * or if a second font was requested but not found.
	 */
	if (f1 && (!name2 || !*name2 || f2)) {
#ifdef VGAFONTDEBUG
		if (scr != &vga_console_screen || vga_console_attached) {
			printf("vga (%s): font1=%s (slot %d)", type->name,
			       f1->name, f1->slot);
			if (f2)
				printf(", font2=%s (slot %d)",
				       f2->name, f2->slot);
			printf("\n");
		}
#endif
		scr->fontset1 = f1;
		scr->fontset2 = f2;
		return (0);
	}
	return (ENXIO);
}

void
vga_init_screen(vc, scr, type, existing, attrp)
	struct vga_config *vc;
	struct vgascreen *scr;
	const struct wsscreen_descr *type;
	int existing;
	long *attrp;
{
	int cpos;
	int res;

	scr->cfg = vc;
	scr->pcs.hdl = (struct pcdisplay_handle *)&vc->hdl;
	scr->pcs.type = type;
	scr->pcs.active = 0;
	scr->mindispoffset = 0;
	scr->maxdispoffset = 0x8000 - type->nrows * type->ncols * 2;

	if (existing) {
		cpos = vga_6845_read(&vc->hdl, cursorh) << 8;
		cpos |= vga_6845_read(&vc->hdl, cursorl);

		/* make sure we have a valid cursor position */
		if (cpos < 0 || cpos >= type->nrows * type->ncols)
			cpos = 0;

		scr->pcs.dispoffset = vga_6845_read(&vc->hdl, startadrh) << 9;
		scr->pcs.dispoffset |= vga_6845_read(&vc->hdl, startadrl) << 1;

		/* make sure we have a valid memory offset */
		if (scr->pcs.dispoffset < scr->mindispoffset ||
		    scr->pcs.dispoffset > scr->maxdispoffset)
			scr->pcs.dispoffset = scr->mindispoffset;
	} else {
		cpos = 0;
		scr->pcs.dispoffset = scr->mindispoffset;
	}
	scr->pcs.visibleoffset = scr->pcs.dispoffset;
	scr->vga_rollover = 0;

	scr->pcs.vc_crow = cpos / type->ncols;
	scr->pcs.vc_ccol = cpos % type->ncols;
	pcdisplay_cursor_init(&scr->pcs, existing);

#ifdef __alpha__
	if (!vc->hdl.vh_mono)
		/*
		 * DEC firmware uses a blue background.
		 */
		res = vga_alloc_attr(scr, WSCOL_WHITE, WSCOL_BLUE,
				     WSATTR_WSCOLORS, attrp);
	else
#endif
	res = vga_alloc_attr(scr, 0, 0, 0, attrp);
#ifdef DIAGNOSTIC
	if (res)
		panic("vga_init_screen: attribute botch");
#endif

	scr->pcs.mem = NULL;

	scr->fontset1 = scr->fontset2 = 0;
	if (vga_selectfont(vc, scr, 0, 0)) {
		if (scr == &vga_console_screen)
			panic("vga_init_screen: no font");
		else
			printf("vga_init_screen: no font\n");
	}

	vc->nscreens++;
	LIST_INSERT_HEAD(&vc->screens, scr, next);
}

void
vga_init(vc, iot, memt)
	struct vga_config *vc;
	bus_space_tag_t iot, memt;
{
	struct vga_handle *vh = &vc->hdl;
	u_int8_t mor;
	int i;

        vh->vh_iot = iot;
        vh->vh_memt = memt;

        if (bus_space_map(vh->vh_iot, 0x3c0, 0x10, 0, &vh->vh_ioh_vga))
                panic("vga_common_setup: couldn't map vga io");

	/* read "misc output register" */
	mor = bus_space_read_1(vh->vh_iot, vh->vh_ioh_vga, 0xc);
	vh->vh_mono = !(mor & 1);

	if (bus_space_map(vh->vh_iot, (vh->vh_mono ? 0x3b0 : 0x3d0), 0x10, 0,
			  &vh->vh_ioh_6845))
                panic("vga_common_setup: couldn't map 6845 io");

        if (bus_space_map(vh->vh_memt, 0xa0000, 0x20000, 0, &vh->vh_allmemh))
                panic("vga_common_setup: couldn't map memory");

        if (bus_space_subregion(vh->vh_memt, vh->vh_allmemh,
				(vh->vh_mono ? 0x10000 : 0x18000), 0x8000,
				&vh->vh_memh))
                panic("vga_common_setup: mem subrange failed");

	vc->nscreens = 0;
	LIST_INIT(&vc->screens);
	vc->active = NULL;
	vc->currenttype = vh->vh_mono ? &vga_stdscreen_mono : &vga_stdscreen;
#if 0
	callout_init(&vc->vc_switch_callout);
#endif

	vc->vc_fonts[0] = &vga_builtinfont;
	for (i = 1; i < 8; i++)
		vc->vc_fonts[i] = 0;

	vc->currentfontset1 = vc->currentfontset2 = 0;
}

void
vga_common_attach(self, iot, memt, type)
	struct device *self;
	bus_space_tag_t iot, memt;
	int type;
{
	vga_extended_attach(self, iot, memt, type, NULL);
}

void
vga_extended_attach(self, iot, memt, type, map)
	struct device *self;
	bus_space_tag_t iot, memt;
	int type;
	paddr_t (*map)(void *, off_t, int);
{
	int console;
	struct vga_config *vc;
	struct wsemuldisplaydev_attach_args aa;

	console = vga_is_console(iot, type);

	if (console) {
		vc = &vga_console_vc;
		vga_console_attached = 1;
	} else {
		vc = malloc(sizeof(struct vga_config), M_DEVBUF, M_NOWAIT);
		if (vc == NULL)
			return;
		bzero(vc, sizeof(struct vga_config));
		vga_init(vc, iot, memt);
	}

	vc->vc_softc = self;
	vc->vc_type = type;
	vc->vc_mmap = map;

	aa.console = console;
	aa.scrdata = (vc->hdl.vh_mono ? &vga_screenlist_mono : &vga_screenlist);
	aa.accessops = &vga_accessops;
	aa.accesscookie = vc;

        config_found(self, &aa, wsemuldisplaydevprint);
}

int
vga_cnattach(iot, memt, type, check)
	bus_space_tag_t iot, memt;
	int type, check;
{
	long defattr;
	const struct wsscreen_descr *scr;

	if (check && !vga_common_probe(iot, memt))
		return (ENXIO);

	/* set up bus-independent VGA configuration */
	vga_init(&vga_console_vc, iot, memt);
	scr = vga_console_vc.currenttype;
	vga_init_screen(&vga_console_vc, &vga_console_screen, scr, 1, &defattr);

	vga_console_screen.pcs.active = 1;
	vga_console_vc.active = &vga_console_screen;

	wsdisplay_cnattach(scr, &vga_console_screen,
			   vga_console_screen.pcs.vc_ccol,
			   vga_console_screen.pcs.vc_crow,
			   defattr);

	vgaconsole = 1;
	vga_console_type = type;
	return (0);
}

int
vga_is_console(iot, type)
	bus_space_tag_t iot;
	int type;
{
	if (vgaconsole &&
	    !vga_console_attached &&
	    iot == vga_console_vc.hdl.vh_iot &&
	    (vga_console_type == -1 || (type == vga_console_type)))
		return (1);
	return (0);
}

int
vga_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vga_config *vc = v;
#if NVGA_PCI > 0
	int error;

	if (vc->vc_type == WSDISPLAY_TYPE_PCIVGA &&
	    (error = vga_pci_ioctl(v, cmd, data, flag, p)) != ENOTTY)
		return (error);
#endif

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(int *)data = vc->vc_type;
		/* XXX should get detailed hardware information here */
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	case WSDISPLAYIO_GINFO:
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
		/* NONE of these operations are by the generic VGA driver. */
		return ENOTTY;
	}

	return (0);
}

paddr_t
vga_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vga_config *vc = v;

	if (vc->vc_mmap != NULL)
		return (*vc->vc_mmap)(v, offset, prot);

	return -1;
}

int
vga_alloc_screen(v, type, cookiep, curxp, curyp, defattrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *defattrp;
{
	struct vga_config *vc = v;
	struct vgascreen *scr;

	if (vc->nscreens == 1) {
		/*
		 * When allocating the second screen, get backing store
		 * for the first one too.
		 * XXX We could be more clever and use video RAM.
		 */
		LIST_FIRST(&vc->screens)->pcs.mem =
		  malloc(type->ncols * type->nrows * 2, M_DEVBUF, M_WAITOK);
	}

	scr = malloc(sizeof(struct vgascreen), M_DEVBUF, M_WAITOK);
	vga_init_screen(vc, scr, type, vc->nscreens == 0, defattrp);

	if (vc->nscreens == 1) {
		scr->pcs.active = 1;
		vc->active = scr;
		vc->currenttype = type;
	} else {
		scr->pcs.mem = malloc(type->ncols * type->nrows * 2,
				      M_DEVBUF, M_WAITOK);
		pcdisplay_eraserows(&scr->pcs, 0, type->nrows, *defattrp);
	}

	*cookiep = scr;
	*curxp = scr->pcs.vc_ccol;
	*curyp = scr->pcs.vc_crow;

	return (0);
}

void
vga_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct vgascreen *vs = cookie;
	struct vga_config *vc = vs->cfg;

	LIST_REMOVE(vs, next);
	vc->nscreens--;
	if (vs != &vga_console_screen) {
		/*
		 * deallocating the one but last screen
		 * removes backing store for the last one
		 */
		if (vc->nscreens == 1)
			free(LIST_FIRST(&vc->screens)->pcs.mem, M_DEVBUF);

		/* Last screen has no backing store */
		if (vc->nscreens != 0)
			free(vs->pcs.mem, M_DEVBUF);

		free(vs, M_DEVBUF);
	} else
		panic("vga_free_screen: console");

	if (vc->active == vs)
		vc->active = NULL;
}

void
vga_setfont(vc, scr)
	struct vga_config *vc;
	struct vgascreen *scr;
{
	int fontslot1, fontslot2;

	fontslot1 = (scr->fontset1 ? scr->fontset1->slot : 0);
	fontslot2 = (scr->fontset2 ? scr->fontset2->slot : fontslot1);
	if (vc->currentfontset1 != fontslot1 ||
	    vc->currentfontset2 != fontslot2) {
		vga_setfontset(&vc->hdl, fontslot1, fontslot2);
		vc->currentfontset1 = fontslot1;
		vc->currentfontset2 = fontslot2;
	}
}

int
vga_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	struct vgascreen *scr = cookie, *oldscr;
	struct vga_config *vc = scr->cfg;

	oldscr = vc->active; /* can be NULL! */
	if (scr == oldscr) {
		return (0);
	}

	vc->wantedscreen = cookie;
	vc->switchcb = cb;
	vc->switchcbarg = cbarg;
	if (cb) {
		timeout_set(&vc->vc_switch_timeout,
		    (void(*)(void *))vga_doswitch, vc);
		timeout_add(&vc->vc_switch_timeout, 0);
		return (EAGAIN);
	}

	vga_doswitch(vc);
	return (0);
}

void
vga_doswitch(vc)
	struct vga_config *vc;
{
	struct vgascreen *scr, *oldscr;
	struct vga_handle *vh = &vc->hdl;
	const struct wsscreen_descr *type;

	scr = vc->wantedscreen;
	if (!scr) {
		printf("vga_doswitch: disappeared\n");
		(*vc->switchcb)(vc->switchcbarg, EIO, 0);
		return;
	}
	type = scr->pcs.type;
	oldscr = vc->active; /* can be NULL! */
#ifdef DIAGNOSTIC
	if (oldscr) {
		if (!oldscr->pcs.active)
			panic("vga_show_screen: not active");
		if (oldscr->pcs.type != vc->currenttype)
			panic("vga_show_screen: bad type");
	}
#endif
	if (scr == oldscr) {
		return;
	}
#ifdef DIAGNOSTIC
	if (scr->pcs.active)
		panic("vga_show_screen: active");
#endif

	scr->vga_rollover = 0;

	if (oldscr) {
		const struct wsscreen_descr *oldtype = oldscr->pcs.type;

		oldscr->pcs.active = 0;
		bus_space_read_region_2(vh->vh_memt, vh->vh_memh,
					oldscr->pcs.dispoffset, oldscr->pcs.mem,
					oldtype->ncols * oldtype->nrows);
	}

	if (vc->currenttype != type) {
		vga_setscreentype(vh, type);
		vc->currenttype = type;
	}

	vga_setfont(vc, scr);
	/* XXX switch colours! */

	scr->pcs.visibleoffset = scr->pcs.dispoffset = scr->mindispoffset;
	if (!oldscr || (scr->pcs.dispoffset != oldscr->pcs.dispoffset)) {
		vga_6845_write(vh, startadrh, scr->pcs.dispoffset >> 9);
		vga_6845_write(vh, startadrl, scr->pcs.dispoffset >> 1);
	}

	bus_space_write_region_2(vh->vh_memt, vh->vh_memh,
				scr->pcs.dispoffset, scr->pcs.mem,
				type->ncols * type->nrows);
	scr->pcs.active = 1;

	vc->active = scr;

	pcdisplay_cursor(&scr->pcs, scr->pcs.cursoron,
			 scr->pcs.vc_crow, scr->pcs.vc_ccol);

	vc->wantedscreen = 0;
	if (vc->switchcb)
		(*vc->switchcb)(vc->switchcbarg, 0, 0);
}

int
vga_load_font(v, cookie, data)
	void *v;
	void *cookie;
	struct wsdisplay_font *data;
{
	struct vga_config *vc = v;
	struct vgascreen *scr = cookie;
	char *name2;
	int res, slot;
	struct vgafont *f;

	if (scr) {
		if ((name2 = data->name) != NULL) {
			while (*name2 && *name2 != ',')
				name2++;
			if (*name2)
				*name2++ = '\0';
		}
		res = vga_selectfont(vc, scr, data->name, name2);
		if (!res)
			vga_setfont(vc, scr);
		return (res);
	}

	if (data->fontwidth != 8 || data->stride != 1)
		return (EINVAL); /* XXX 1 byte per line */
	if (data->firstchar != 0 || data->numchars != 256)
		return (EINVAL);
#ifndef WSCONS_SUPPORT_PCVTFONTS
	if (data->encoding == WSDISPLAY_FONTENC_PCVT) {
		printf("vga: pcvt font support not built in, see vga(4)\n");
		return (EINVAL);
	}
#endif

	if (data->index < 0) {
		for (slot = 0; slot < 8; slot++)
			if (!vc->vc_fonts[slot])
				break;
	} else
		slot = data->index;

	if (slot >= 8)
		return (ENOSPC);

	if (vc->vc_fonts[slot] != NULL)
		return (EEXIST);
	f = malloc(sizeof(struct vgafont), M_DEVBUF, M_WAITOK);
	if (f == NULL)
		return (ENOMEM);
	strlcpy(f->name, data->name, sizeof(f->name));
	f->height = data->fontheight;
	f->encoding = data->encoding;
#ifdef notyet
	f->firstchar = data->firstchar;
	f->numchars = data->numchars;
#endif
#ifdef VGAFONTDEBUG
	printf("vga: load %s (8x%d, enc %d) font to slot %d\n", f->name,
	       f->height, f->encoding, slot);
#endif
	vga_loadchars(&vc->hdl, slot, 0, 256, f->height, data->data);
	f->slot = slot;
	vc->vc_fonts[slot] = f;
	data->cookie = f;
	data->index = slot;

	return (0);
}

void
vga_scrollback(v, cookie, lines)
	void *v;
	void *cookie;
	int lines;
{
	struct vga_config *vc = v;
	struct vgascreen *scr = cookie;
	struct vga_handle *vh = &vc->hdl;

	if (lines == 0) {
		if (scr->pcs.visibleoffset == scr->pcs.dispoffset)
			return;

		scr->pcs.visibleoffset = scr->pcs.dispoffset;	/* reset */
	}
	else {
		int vga_scr_end;
		int margin = scr->pcs.type->ncols * 2;
		int ul, we, p, st;

		vga_scr_end = (scr->pcs.dispoffset + scr->pcs.type->ncols *
		    scr->pcs.type->nrows * 2);
		if (scr->vga_rollover > vga_scr_end + margin) {
			ul = vga_scr_end;
			we = scr->vga_rollover + scr->pcs.type->ncols * 2;
		} else {
			ul = 0;
			we = 0x8000;
		}
		p = (scr->pcs.visibleoffset - ul + we) % we + lines *
		    (scr->pcs.type->ncols * 2);
		st = (scr->pcs.dispoffset - ul + we) % we;
		if (p < margin)
			p = 0;
		if (p > st - margin)
			p = st;
		scr->pcs.visibleoffset = (p + ul) % we;
	}
	
	/* update visible position */
	vga_6845_write(vh, startadrh, scr->pcs.visibleoffset >> 9);
	vga_6845_write(vh, startadrl, scr->pcs.visibleoffset >> 1);
}

int
vga_alloc_attr(id, fg, bg, flags, attrp)
	void *id;
	int fg, bg;
	int flags;
	long *attrp;
{
	struct vgascreen *scr = id;
	struct vga_config *vc = scr->cfg;

	if (vc->hdl.vh_mono) {
		if (flags & WSATTR_WSCOLORS)
			return (EINVAL);
		if (flags & WSATTR_REVERSE)
			*attrp = 0x70;
		else
			*attrp = 0x07;
		if (flags & WSATTR_UNDERLINE)
			*attrp |= FG_UNDERLINE;
		if (flags & WSATTR_HILIT)
			*attrp |= FG_INTENSE;
	} else {
		if (flags & (WSATTR_UNDERLINE | WSATTR_REVERSE))
			return (EINVAL);
		if (flags & WSATTR_WSCOLORS)
			*attrp = fgansitopc[fg] | bgansitopc[bg];
		else
			*attrp = 7;
		if (flags & WSATTR_HILIT)
			*attrp += 8;
	}
	if (flags & WSATTR_BLINK)
		*attrp |= FG_BLINK;
	return (0);
}

void
vga_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct vgascreen *scr = id;
	bus_space_tag_t memt = scr->pcs.hdl->ph_memt;
	bus_space_handle_t memh = scr->pcs.hdl->ph_memh;
	int ncols = scr->pcs.type->ncols;
	bus_size_t srcoff, dstoff;

	srcoff = srcrow * ncols + 0;
	dstoff = dstrow * ncols + 0;

	if (scr->pcs.active) {
		if (dstrow == 0 && (srcrow + nrows == scr->pcs.type->nrows)) {
#ifdef PCDISPLAY_SOFTCURSOR
			int cursoron = scr->pcs.cursoron;

			if (cursoron)
				pcdisplay_cursor(&scr->pcs, 0,
				    scr->pcs.vc_crow, scr->pcs.vc_ccol);
#endif
			/* scroll up whole screen */
			if ((scr->pcs.dispoffset + srcrow * ncols * 2)
			    <= scr->maxdispoffset) {
				scr->pcs.dispoffset += srcrow * ncols * 2;
			} else {
				bus_space_copy_2(memt, memh,
					scr->pcs.dispoffset + srcoff * 2,
					memh, scr->mindispoffset,
					nrows * ncols);
				scr->vga_rollover = scr->pcs.dispoffset;
				scr->pcs.dispoffset = scr->mindispoffset;
			}
			scr->pcs.visibleoffset = scr->pcs.dispoffset;
			vga_6845_write(&scr->cfg->hdl, startadrh,
				       scr->pcs.dispoffset >> 9);
			vga_6845_write(&scr->cfg->hdl, startadrl,
				       scr->pcs.dispoffset >> 1);
#ifdef PCDISPLAY_SOFTCURSOR
			if (cursoron)
				pcdisplay_cursor(&scr->pcs, 1,
				    scr->pcs.vc_crow, scr->pcs.vc_ccol);
#endif
		} else {
			bus_space_copy_2(memt, memh,
					scr->pcs.dispoffset + srcoff * 2,
					memh, scr->pcs.dispoffset + dstoff * 2,
					nrows * ncols);
		}
	} else
		bcopy(&scr->pcs.mem[srcoff], &scr->pcs.mem[dstoff],
		      nrows * ncols * 2);
}

#ifdef WSCONS_SUPPORT_PCVTFONTS

#define NOTYET 0xffff
static u_int16_t pcvt_unichars[0xa0] = {
/* 0 */	_e006U,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET,
	0x2409, /* SYMBOL FOR HORIZONTAL TABULATION */
	0x240a, /* SYMBOL FOR LINE FEED */
	0x240b, /* SYMBOL FOR VERTICAL TABULATION */
	0x240c, /* SYMBOL FOR FORM FEED */
	0x240d, /* SYMBOL FOR CARRIAGE RETURN */
	NOTYET, NOTYET,
/* 1 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 2 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 3 */	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
	NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET, NOTYET,
/* 4 */	0x03c1, /* GREEK SMALL LETTER RHO */
	0x03c8, /* GREEK SMALL LETTER PSI */
	0x2202, /* PARTIAL DIFFERENTIAL */
	0x03bb, /* GREEK SMALL LETTER LAMDA */
	0x03b9, /* GREEK SMALL LETTER IOTA */
	0x03b7, /* GREEK SMALL LETTER ETA */
	0x03b5, /* GREEK SMALL LETTER EPSILON */
	0x03c7, /* GREEK SMALL LETTER CHI */
	0x2228, /* LOGICAL OR */
	0x2227, /* LOGICAL AND */
	0x222a, /* UNION */
	0x2283, /* SUPERSET OF */
	0x2282, /* SUBSET OF */
	0x03a5, /* GREEK CAPITAL LETTER UPSILON */
	0x039e, /* GREEK CAPITAL LETTER XI */
	0x03a8, /* GREEK CAPITAL LETTER PSI */
/* 5 */	0x03a0, /* GREEK CAPITAL LETTER PI */
	0x21d2, /* RIGHTWARDS DOUBLE ARROW */
	0x21d4, /* LEFT RIGHT DOUBLE ARROW */
	0x039b, /* GREEK CAPITAL LETTER LAMDA */
	0x0398, /* GREEK CAPITAL LETTER THETA */
	0x2243, /* ASYMPTOTICALLY EQUAL TO */
	0x2207, /* NABLA */
	0x2206, /* INCREMENT */
	0x221d, /* PROPORTIONAL TO */
	0x2234, /* THEREFORE */
	0x222b, /* INTEGRAL */
	0x2215, /* DIVISION SLASH */
	0x2216, /* SET MINUS */
	_e00eU,
	_e00dU,
	_e00bU,
/* 6 */	_e00cU,
	_e007U,
	_e008U,
	_e009U,
	_e00aU,
	0x221a, /* SQUARE ROOT */
	0x03c9, /* GREEK SMALL LETTER OMEGA */
	0x00a5, /* YEN SIGN */
	0x03be, /* GREEK SMALL LETTER XI */
	0x00fd, /* LATIN SMALL LETTER Y WITH ACUTE */
	0x00fe, /* LATIN SMALL LETTER THORN */
	0x00f0, /* LATIN SMALL LETTER ETH */
	0x00de, /* LATIN CAPITAL LETTER THORN */
	0x00dd, /* LATIN CAPITAL LETTER Y WITH ACUTE */
	0x00d7, /* MULTIPLICATION SIGN */
	0x00d0, /* LATIN CAPITAL LETTER ETH */
/* 7 */	0x00be, /* VULGAR FRACTION THREE QUARTERS */
	0x00b8, /* CEDILLA */
	0x00b4, /* ACUTE ACCENT */
	0x00af, /* MACRON */
	0x00ae, /* REGISTERED SIGN */
	0x00ad, /* SOFT HYPHEN */
	0x00ac, /* NOT SIGN */
	0x00a8, /* DIAERESIS */
	0x2260, /* NOT EQUAL TO */
	_e005U,
	_e004U,
	_e003U,
	_e002U,
	_e001U,
	0x03c5, /* GREEK SMALL LETTER UPSILON */
	0x00f8, /* LATIN SMALL LETTER O WITH STROKE */
/* 8 */	0x0153, /* LATIN SMALL LIGATURE OE */
	0x00f5, /* LATIN SMALL LETTER O WITH TILDE !!!doc bug */
	0x00e3, /* LATIN SMALL LETTER A WITH TILDE */
	0x0178, /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
	0x00db, /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
	0x00da, /* LATIN CAPITAL LETTER U WITH ACUTE */
	0x00d9, /* LATIN CAPITAL LETTER U WITH GRAVE */
	0x00d8, /* LATIN CAPITAL LETTER O WITH STROKE */
	0x0152, /* LATIN CAPITAL LIGATURE OE */
	0x00d5, /* LATIN CAPITAL LETTER O WITH TILDE */
	0x00d4, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
	0x00d3, /* LATIN CAPITAL LETTER O WITH ACUTE */
	0x00d2, /* LATIN CAPITAL LETTER O WITH GRAVE */
	0x00cf, /* LATIN CAPITAL LETTER I WITH DIAERESIS */
	0x00ce, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
	0x00cd, /* LATIN CAPITAL LETTER I WITH ACUTE */
/* 9 */	0x00cc, /* LATIN CAPITAL LETTER I WITH GRAVE */
	0x00cb, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
	0x00ca, /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
	0x00c8, /* LATIN CAPITAL LETTER E WITH GRAVE */
	0x00c3, /* LATIN CAPITAL LETTER A WITH TILDE */
	0x00c2, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
	0x00c1, /* LATIN CAPITAL LETTER A WITH ACUTE */
	0x00c0, /* LATIN CAPITAL LETTER A WITH GRAVE */
	0x00b9, /* SUPERSCRIPT ONE */
	0x00b7, /* MIDDLE DOT */
	0x03b6, /* GREEK SMALL LETTER ZETA */
	0x00b3, /* SUPERSCRIPT THREE */
	0x00a9, /* COPYRIGHT SIGN */
	0x00a4, /* CURRENCY SIGN */
	0x03ba, /* GREEK SMALL LETTER KAPPA */
	_e000U
};

int vga_pcvt_mapchar(int, unsigned int *);

int
vga_pcvt_mapchar(uni, index)
	int uni;
	unsigned int *index;
{
	int i;

	for (i = 0; i < 0xa0; i++) /* 0xa0..0xff are reserved */
		if (uni == pcvt_unichars[i]) {
			*index = i;
			return (5);
		}
	*index = 0x99; /* middle dot */
	return (0);
}

#endif /* WSCONS_SUPPORT_PCVTFONTS */

int _vga_mapchar(void *, struct vgafont *, int, unsigned int *);

int
_vga_mapchar(id, font, uni, index)
	void *id;
	struct vgafont *font;
	int uni;
	unsigned int *index;
{

	switch (font->encoding) {
	case WSDISPLAY_FONTENC_ISO:
		if (uni < 256) {
			*index = uni;
			return (5);
		} else {
			*index = ' ';
			return (0);
		}
		break;
	case WSDISPLAY_FONTENC_IBM:
		return (pcdisplay_mapchar(id, uni, index));
#ifdef WSCONS_SUPPORT_PCVTFONTS
	case WSDISPLAY_FONTENC_PCVT:
		return (vga_pcvt_mapchar(uni, index));
#endif
	default:
#ifdef VGAFONTDEBUG
		printf("_vga_mapchar: encoding=%d\n", font->encoding);
#endif
		*index = ' ';
		return (0);
	}
}

int
vga_mapchar(id, uni, index)
	void *id;
	int uni;
	unsigned int *index;
{
	struct vgascreen *scr = id;
	unsigned int idx1, idx2;
	int res1, res2;

	res1 = 0;
	idx1 = ' '; /* space */
	if (scr->fontset1)
		res1 = _vga_mapchar(id, scr->fontset1, uni, &idx1);
	res2 = -1;
	if (scr->fontset2) {
		KASSERT(VGA_SCREEN_CANTWOFONTS(scr->pcs.type));
		res2 = _vga_mapchar(id, scr->fontset2, uni, &idx2);
	}
	if (res2 >= res1) {
		*index = idx2 | 0x0800; /* attribute bit 3 */
		return (res2);
	}
	*index = idx1;
	return (res1);
}

void
vga_putchar(c, row, col, uc, attr)
	void *c;
	int row;
	int col;
	u_int uc;
	long attr;
{
	struct vgascreen *scr = c;

	if (scr->pcs.visibleoffset != scr->pcs.dispoffset)
		vga_scrollback(scr->cfg, scr, 0);

	pcdisplay_putchar(c, row, col, uc, attr);
}

void
vga_burner(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct vga_config *vc = v;
	struct vga_handle *vh = &vc->hdl;
	u_int8_t r;
	int s;

	s = splhigh();
	vga_ts_write(vh, syncreset, 0x01);
	if (on) {
		vga_ts_write(vh, mode, (vga_ts_read(vh, mode) & ~0x20));
		r = vga_6845_read(vh, mode) | 0x80;
		DELAY(10000);
		vga_6845_write(vh, mode, r);
	} else {
		vga_ts_write(vh, mode, (vga_ts_read(vh, mode) | 0x20));
		if (flags & WSDISPLAY_BURN_VBLANK) {
			r = vga_6845_read(vh, mode) & ~0x80;
			DELAY(10000);
			vga_6845_write(vh, mode, r);
		}
	}
	vga_ts_write(vh, syncreset, 0x03);
	splx(s);
}

u_int16_t
vga_getchar(c, row, col)
	void *c;
	int row, col;
{
	struct vga_config *vc = c;
	
	return (pcdisplay_getchar(vc->active, row, col));
}	

struct cfdriver vga_cd = {
	NULL, "vga", DV_DULL
};

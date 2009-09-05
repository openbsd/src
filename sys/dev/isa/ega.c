/* $OpenBSD: ega.c,v 1.15 2009/09/05 14:09:35 miod Exp $ */
/* $NetBSD: ega.c,v 1.4.4.1 2000/06/30 16:27:47 simonb Exp $ */

/*
 * Copyright (c) 1999
 *	Matthias Drochner.  All rights reserved.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/isa/egavar.h>

#include <dev/ic/pcdisplay.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

static struct egafont {
	char name[WSFONT_NAME_SIZE];
	int height;
	int encoding;
	int slot;
} ega_builtinfont = {
	"builtin",
	14,
	WSDISPLAY_FONTENC_IBM,
	0
};

struct egascreen {
	struct pcdisplayscreen pcs;
	LIST_ENTRY(egascreen) next;
	struct ega_config *cfg;
	struct egafont *fontset1, *fontset2;

	int mindispoffset, maxdispoffset;
};

struct ega_config {
	struct vga_handle hdl;

	int nscreens;
	LIST_HEAD(, egascreen) screens;
	struct egascreen *active; /* current display */
	const struct wsscreen_descr *currenttype;
	int currentfontset1, currentfontset2;

	struct egafont *vc_fonts[4];

	struct egascreen *wantedscreen;
	void (*switchcb)(void *, int, int);
	void *switchcbarg;

	struct timeout switch_timeout;
};

struct ega_softc {
	struct device sc_dev;
	struct ega_config *sc_dc;
	int nscreens;
};

static int egaconsole, ega_console_attached;
static struct egascreen ega_console_screen;
static struct ega_config ega_console_dc;

int	ega_match(struct device *, void *, void *);
void	ega_attach(struct device *, struct device *, void *);

static int ega_is_console(bus_space_tag_t);
static int ega_probe_col(bus_space_tag_t, bus_space_tag_t);
static int ega_probe_mono(bus_space_tag_t, bus_space_tag_t);
int ega_selectfont(struct ega_config *, struct egascreen *,
			char *, char *);
void ega_init_screen(struct ega_config *, struct egascreen *,
			  const struct wsscreen_descr *,
			  int, long *);
static void ega_init(struct ega_config *,
			  bus_space_tag_t, bus_space_tag_t, int);
static void ega_setfont(struct ega_config *, struct egascreen *);
static int ega_alloc_attr(void *, int, int, int, long *);
static void ega_unpack_attr(void *, long, int *, int *, int *);
int ega_copyrows(void *, int, int, int);

struct cfattach ega_ca = {
	sizeof(struct ega_softc), ega_match, ega_attach,
};

struct cfdriver ega_cd = {
	NULL, "ega", DV_DULL,
};

const struct wsdisplay_emulops ega_emulops = {
	pcdisplay_cursor,
	pcdisplay_mapchar,
	pcdisplay_putchar,
	pcdisplay_copycols,
	pcdisplay_erasecols,
	ega_copyrows,
	pcdisplay_eraserows,
	ega_alloc_attr,
	ega_unpack_attr
};

/*
 * translate WS(=ANSI) color codes to standard pc ones
 */
static const unsigned char fgansitopc[] = {
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
}, bgansitopc[] = {
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
};

/*
 * translate standard pc color codes to WS(=ANSI) ones
 */
static const u_int8_t pctoansi[] = {
#ifdef __alpha__
	WSCOL_BLACK, WSCOL_RED, WSCOL_GREEN, WSCOL_BROWN,
	WSCOL_BLUE, WSCOL_MAGENTA, WSCOL_CYAN, WSCOL_WHITE
#else
	WSCOL_BLACK, WSCOL_BLUE, WSCOL_GREEN, WSCOL_CYAN,
	WSCOL_RED, WSCOL_MAGENTA, WSCOL_BROWN, WSCOL_WHITE
#endif
};
const struct wsscreen_descr ega_stdscreen = {
	"80x25", 80, 25,
	&ega_emulops,
	8, 14,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, ega_stdscreen_mono = {
	"80x25", 80, 25,
	&ega_emulops,
	8, 14,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, ega_stdscreen_bf = {
	"80x25bf", 80, 25,
	&ega_emulops,
	8, 14,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
}, ega_35lscreen = {
	"80x35", 80, 35,
	&ega_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, ega_35lscreen_mono = {
	"80x35", 80, 35,
	&ega_emulops,
	8, 10,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, ega_35lscreen_bf = {
	"80x35bf", 80, 35,
	&ega_emulops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
}, ega_43lscreen = {
	"80x43", 80, 43,
	&ega_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT | WSSCREEN_BLINK
}, ega_43lscreen_mono = {
	"80x43", 80, 43,
	&ega_emulops,
	8, 8,
	WSSCREEN_HILIT | WSSCREEN_UNDERLINE | WSSCREEN_BLINK | WSSCREEN_REVERSE
}, ega_43lscreen_bf = {
	"80x43bf", 80, 43,
	&ega_emulops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_BLINK
};

#define VGA_SCREEN_CANTWOFONTS(type) (!((type)->capabilities & WSSCREEN_HILIT))

const struct wsscreen_descr *_ega_scrlist[] = {
	&ega_stdscreen,
	&ega_stdscreen_bf,
	&ega_35lscreen,
	&ega_35lscreen_bf,
	&ega_43lscreen,
	&ega_43lscreen_bf,
}, *_ega_scrlist_mono[] = {
	&ega_stdscreen_mono,
	&ega_35lscreen_mono,
	&ega_43lscreen_mono,
};


const struct wsscreen_list ega_screenlist = {
	sizeof(_ega_scrlist) / sizeof(struct wsscreen_descr *),
	_ega_scrlist
}, ega_screenlist_mono = {
	sizeof(_ega_scrlist_mono) / sizeof(struct wsscreen_descr *),
	_ega_scrlist_mono
};

static int ega_ioctl(void *, u_long, caddr_t, int, struct proc *);
static paddr_t ega_mmap(void *, off_t, int);
static int ega_alloc_screen(void *, const struct wsscreen_descr *,
			    void **, int *, int *, long *);
static void ega_free_screen(void *, void *);
static int ega_show_screen(void *, void *, int,
			   void (*) (void *, int, int), void *);
static int ega_load_font(void *, void *, struct wsdisplay_font *);

void ega_doswitch(struct ega_config *);

const struct wsdisplay_accessops ega_accessops = {
	ega_ioctl,
	ega_mmap,
	ega_alloc_screen,
	ega_free_screen,
	ega_show_screen,
	ega_load_font
};

static int
ega_probe_col(iot, memt)
	bus_space_tag_t iot, memt;
{
	bus_space_handle_t memh, ioh_6845;
	u_int16_t oldval, val;

	if (bus_space_map(memt, 0xb8000, 0x8000, 0, &memh))
		return (0);
	oldval = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	val = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, oldval);
	bus_space_unmap(memt, memh, 0x8000);
	if (val != 0xa55a)
		return (0);

	if (bus_space_map(iot, 0x3d0, 0x10, 0, &ioh_6845))
		return (0);
	bus_space_unmap(iot, ioh_6845, 0x10);

	return (1);
}

static int
ega_probe_mono(iot, memt)
	bus_space_tag_t iot, memt;
{
	bus_space_handle_t memh, ioh_6845;
	u_int16_t oldval, val;

	if (bus_space_map(memt, 0xb0000, 0x8000, 0, &memh))
		return (0);
	oldval = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	val = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, oldval);
	bus_space_unmap(memt, memh, 0x8000);
	if (val != 0xa55a)
		return (0);

	if (bus_space_map(iot, 0x3b0, 0x10, 0, &ioh_6845))
		return (0);
	bus_space_unmap(iot, ioh_6845, 0x10);

	return (1);
}
/*
 * We want at least ASCII 32..127 be present in the
 * first font slot.
 */
#define vga_valid_primary_font(f) \
	(f->encoding == WSDISPLAY_FONTENC_IBM || \
	f->encoding == WSDISPLAY_FONTENC_ISO)

int
ega_selectfont(vc, scr, name1, name2)
	struct ega_config *vc;
	struct egascreen *scr;
	char *name1, *name2; /* NULL: take first found */
{
	const struct wsscreen_descr *type = scr->pcs.type;
	struct egafont *f1, *f2;
	int i;

	f1 = f2 = 0;

	for (i = 0; i < 4; i++) {
		struct egafont *f = vc->vc_fonts[i];
		if (!f || f->height != type->fontheight)
			continue;
		if (!f1 &&
		    vga_valid_primary_font(f) &&
		    (!name1 || !strcmp(name1, f->name))) {
			f1 = f;
			continue;
		}
		if (!f2 &&
		    VGA_SCREEN_CANTWOFONTS(type) &&
		    (!name2 || !strcmp(name2, f->name))) {
			f2 = f;
			continue;
		}
	}

	/*
	 * The request fails if no primary font was found,
	 * or if a second font was requested but not found.
	 */
	if (f1 && (!name2 || f2)) {
#ifdef EGAFONTDEBUG
		if (scr != &ega_console_screen || ega_console_attached) {
			printf("ega (%s): font1=%s (slot %d)", type->name,
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
ega_init_screen(vc, scr, type, existing, attrp)
	struct ega_config *vc;
	struct egascreen *scr;
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

	scr->pcs.vc_crow = cpos / type->ncols;
	scr->pcs.vc_ccol = cpos % type->ncols;
	pcdisplay_cursor_init(&scr->pcs, existing);

	res = ega_alloc_attr(scr, 0, 0, 0, attrp);
#ifdef DIAGNOSTIC
	if (res)
		panic("ega_init_screen: attribute botch");
#endif

	scr->pcs.mem = NULL;

	scr->fontset1 = scr->fontset2 = 0;
	if (ega_selectfont(vc, scr, 0, 0)) {
		if (scr == &ega_console_screen)
			panic("ega_init_screen: no font");
		else
			printf("ega_init_screen: no font\n");
	}

	vc->nscreens++;
	LIST_INSERT_HEAD(&vc->screens, scr, next);
}

static void
ega_init(vc, iot, memt, mono)
	struct ega_config *vc;
	bus_space_tag_t iot, memt;
	int mono;
{
	struct vga_handle *vh = &vc->hdl;
	int i;

        vh->vh_iot = iot;
        vh->vh_memt = memt;
	vh->vh_mono = mono;

        if (bus_space_map(vh->vh_iot, 0x3c0, 0x10, 0, &vh->vh_ioh_vga))
                panic("ega_common_setup: can't map ega i/o");

	if (bus_space_map(vh->vh_iot, (vh->vh_mono ? 0x3b0 : 0x3d0), 0x10, 0,
			  &vh->vh_ioh_6845))
                panic("ega_common_setup: can't map 6845 i/o");

        if (bus_space_map(vh->vh_memt, 0xa0000, 0x20000, 0, &vh->vh_allmemh))
                panic("ega_common_setup: can't map mem space");

        if (bus_space_subregion(vh->vh_memt, vh->vh_allmemh,
				(vh->vh_mono ? 0x10000 : 0x18000), 0x8000,
				&vh->vh_memh))
                panic("ega_common_setup: mem subrange failed");

	vc->nscreens = 0;
	LIST_INIT(&vc->screens);
	vc->active = NULL;
	vc->currenttype = vh->vh_mono ? &ega_stdscreen_mono : &ega_stdscreen;

	vc->vc_fonts[0] = &ega_builtinfont;
	for (i = 1; i < 4; i++)
		vc->vc_fonts[i] = 0;

	vc->currentfontset1 = vc->currentfontset2 = 0;
}

int
ega_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	int mono;

	/* If values are hardwired to something that they can't be, punt. */
	if ((ia->ia_iobase != IOBASEUNK &&
	     ia->ia_iobase != 0x3d0 &&
	     ia->ia_iobase != 0x3b0) ||
	    /* ia->ia_iosize != 0 || XXX isa.c */
	    (ia->ia_maddr != MADDRUNK &&
	     ia->ia_maddr != 0xb8000 &&
	     ia->ia_maddr != 0xb0000) ||
	    (ia->ia_msize != 0 && ia->ia_msize != 0x8000) ||
	    ia->ia_irq != IRQUNK || ia->ia_drq != DRQUNK)
		return (0);

	if (ega_is_console(ia->ia_iot))
		mono = ega_console_dc.hdl.vh_mono;
	else if (ia->ia_iobase != 0x3b0 && ia->ia_maddr != 0xb0000 &&
		 ega_probe_col(ia->ia_iot, ia->ia_memt))
		mono = 0;
	else if (ia->ia_iobase != 0x3d0 && ia->ia_maddr != 0xb8000 &&
		ega_probe_mono(ia->ia_iot, ia->ia_memt))
		mono = 1;
	else
		return (0);

	ia->ia_iobase = mono ? 0x3b0 : 0x3d0;
	ia->ia_iosize = 0x10;
	ia->ia_maddr = mono ? 0xb0000 : 0xb8000;
	ia->ia_msize = 0x8000;
	return (2); /* beat pcdisplay */
}

void
ega_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct ega_softc *sc = (struct ega_softc *)self;
	int console;
	struct ega_config *dc;
	struct wsemuldisplaydev_attach_args aa;

	printf("\n");

	console = ega_is_console(ia->ia_iot);

	if (console) {
		dc = &ega_console_dc;
		sc->nscreens = 1;
		ega_console_attached = 1;
	} else {
		dc = malloc(sizeof(struct ega_config),
			    M_DEVBUF, M_WAITOK);
		if (ia->ia_iobase != 0x3b0 && ia->ia_maddr != 0xb0000 &&
		    ega_probe_col(ia->ia_iot, ia->ia_memt))
			ega_init(dc, ia->ia_iot, ia->ia_memt, 0);
		else if (ia->ia_iobase != 0x3d0 && ia->ia_maddr != 0xb8000 &&
			 ega_probe_mono(ia->ia_iot, ia->ia_memt))
			ega_init(dc, ia->ia_iot, ia->ia_memt, 1);
		else
			panic("ega_attach: display disappeared");
	}
	sc->sc_dc = dc;

	aa.console = console;
	aa.scrdata = &ega_screenlist;
	aa.accessops = &ega_accessops;
	aa.accesscookie = dc;
	aa.defaultscreens = 0;

        config_found(self, &aa, wsemuldisplaydevprint);
}


int
ega_cnattach(iot, memt)
	bus_space_tag_t iot, memt;
{
	int mono;
	long defattr;
	const struct wsscreen_descr *scr;

	if (ega_probe_col(iot, memt))
		mono = 0;
	else if (ega_probe_mono(iot, memt))
		mono = 1;
	else
		return (ENXIO);

	ega_init(&ega_console_dc, iot, memt, mono);
	scr = ega_console_dc.currenttype;
	ega_init_screen(&ega_console_dc, &ega_console_screen, scr, 1, &defattr);

	ega_console_screen.pcs.active = 1;
	ega_console_dc.active = &ega_console_screen;

	wsdisplay_cnattach(scr, &ega_console_screen,
			   ega_console_screen.pcs.vc_ccol,
			   ega_console_screen.pcs.vc_crow,
			   defattr);

	egaconsole = 1;
	return (0);
}

static int
ega_is_console(iot)
	bus_space_tag_t iot;
{
	if (egaconsole &&
	    !ega_console_attached &&
	    iot == ega_console_dc.hdl.vh_iot)
		return (1);
	return (0);
}

static int
ega_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	/*
	 * XXX "do something!"
	 */
	return (-1);
}

static paddr_t
ega_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	return (-1);
}

static int
ega_alloc_screen(v, type, cookiep, curxp, curyp, defattrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *defattrp;
{
	struct ega_config *vc = v;
	struct egascreen *scr;

	if (vc->nscreens == 1) {
		/*
		 * When allocating the second screen, get backing store
		 * for the first one too.
		 * XXX We could be more clever and use video RAM.
		 */
		scr = LIST_FIRST(&vs->screens);
		scr->pcs.mem =
		  malloc(scr->pcs.type->ncols * scr->pcs.type->nrows * 2,
		    M_DEVBUF, M_WAITOK);
	}

	scr = malloc(sizeof(struct egascreen), M_DEVBUF, M_WAITOK);
	ega_init_screen(vc, scr, type, vc->nscreens == 0, defattrp);

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

static void
ega_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct egascreen *vs = cookie;
	struct ega_config *vc = vs->cfg;

	LIST_REMOVE(vs, next);
	if (vs != &ega_console_screen) {
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
		panic("ega_free_screen: console");

	if (vc->active == vs)
		vc->active = NULL;
}

static void
ega_setfont(vc, scr)
	struct ega_config *vc;
	struct egascreen *scr;
{
	int fontslot1, fontslot2;

	fontslot1 = (scr->fontset1 ? scr->fontset1->slot : 0);
	fontslot2 = (scr->fontset2 ? scr->fontset2->slot : fontslot1);
	if (vc->currentfontset1 != fontslot1 ||
	    vc->currentfontset2 != fontslot2) {
		vga_setfontset(&vc->hdl, 2 * fontslot1, 2 * fontslot2);
		vc->currentfontset1 = fontslot1;
		vc->currentfontset2 = fontslot2;
	}
}

static int
ega_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	struct egascreen *scr = cookie, *oldscr;
	struct ega_config *vc = scr->cfg;

	oldscr = vc->active; /* can be NULL! */
	if (scr == oldscr) {
		return (0);
	}

	vc->wantedscreen = cookie;
	vc->switchcb = cb;
	vc->switchcbarg = cbarg;
	if (cb) {
		timeout_set(&vc->switch_timeout,
		    (void(*)(void *))ega_doswitch, vc);
		timeout_add(&vc->switch_timeout, 0);
		return (EAGAIN);
	}

	ega_doswitch(vc);
	return (0);
}

void
ega_doswitch(vc)
	struct ega_config *vc;
{
	struct egascreen *scr, *oldscr;
	struct vga_handle *vh = &vc->hdl;
	const struct wsscreen_descr *type;

	scr = vc->wantedscreen;
	if (!scr) {
		printf("ega_doswitch: disappeared\n");
		(*vc->switchcb)(vc->switchcbarg, EIO, 0);
		return;
	}
	type = scr->pcs.type;
	oldscr = vc->active; /* can be NULL! */
#ifdef DIAGNOSTIC
	if (oldscr) {
		if (!oldscr->pcs.active)
			panic("ega_show_screen: not active");
		if (oldscr->pcs.type != vc->currenttype)
			panic("ega_show_screen: bad type");
	}
#endif
	if (scr == oldscr) {
		return;
	}
#ifdef DIAGNOSTIC
	if (scr->pcs.active)
		panic("ega_show_screen: active");
#endif

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

	ega_setfont(vc, scr);
	/* XXX swich colours! */

	scr->pcs.dispoffset = scr->mindispoffset;
	if (!oldscr || (scr->pcs.dispoffset != oldscr->pcs.dispoffset)) {
		vga_6845_write(vh, startadrh, scr->pcs.dispoffset >> 9);
		vga_6845_write(vh, startadrl, scr->pcs.dispoffset >> 1);
	}

	bus_space_write_region_2(vh->vh_memt, vh->vh_memh,
				scr->pcs.dispoffset, scr->pcs.mem,
				type->ncols * type->nrows);
	scr->pcs.active = 1;

	vc->active = scr;

	pcdisplay_cursor_reset(&scr->pcs);
	pcdisplay_cursor(&scr->pcs, scr->pcs.cursoron,
			 scr->pcs.vc_crow, scr->pcs.vc_ccol);

	vc->wantedscreen = 0;
	if (vc->switchcb)
		(*vc->switchcb)(vc->switchcbarg, 0, 0);
}

static int
ega_load_font(v, cookie, data)
	void *v;
	void *cookie;
	struct wsdisplay_font *data;
{
	struct ega_config *vc = v;
	struct egascreen *scr = cookie;
	char *name2;
	int res, slot;
	struct egafont *f;

	if (scr) {
		if ((name2 = data->name) != NULL) {
			while (*name2 && *name2 != ',')
				name2++;
			if (name2)
				*name2++ = '\0';
		}
		res = ega_selectfont(vc, scr, data->name, name2);
		if (!res)
			ega_setfont(vc, scr);
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
		for (slot = 0; slot < 4; slot++)
			if (!vc->vc_fonts[slot])
				break;
	} else
		slot = data->index;

	if (slot >= 4)
		return (ENOSPC);

	if (vc->vc_fonts[slot] != NULL)
		return (EEXIST);
	f = malloc(sizeof(struct egafont), M_DEVBUF, M_WAITOK);
	if (f == NULL)
		return (ENOMEM);
	strlcpy(f->name, data->name, sizeof(f->name));
	f->height = data->fontheight;
	f->encoding = data->encoding;
#ifdef notyet
	f->firstchar = data->firstchar;
	f->numchars = data->numchars;
#endif
#ifdef EGAFONTDEBUG
	printf("ega: load %s (8x%d, enc %d) font to slot %d\n", f->name,
	       f->height, f->encoding, slot);
#endif
	vga_loadchars(&vc->hdl, 2 * slot, 0, 256, f->height, data->data);
	f->slot = slot;
	vc->vc_fonts[slot] = f;
	data->cookie = f;
	data->index = slot;

	return (0);
}

static int
ega_alloc_attr(id, fg, bg, flags, attrp)
	void *id;
	int fg, bg;
	int flags;
	long *attrp;
{
	struct egascreen *scr = id;
	struct ega_config *vc = scr->cfg;

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
			*attrp = fgansitopc[fg & 7] | bgansitopc[bg & 7];
		else
			*attrp = 7;
		if ((flags & WSATTR_HILIT) || (fg & 8) || (bg & 8))
			*attrp += 8;
	}
	if (flags & WSATTR_BLINK)
		*attrp |= FG_BLINK;
	return (0);
}

void
ega_unpack_attr(id, attr, fg, bg, ul)
	void *id;
	long attr;
	int *fg, *bg, *ul;
{
	struct egascreen *scr = id;
	struct ega_config *vc = scr->cfg;

	if (vc->hdl.vh_mono) {
		*fg = (attr & 0x07) == 0x07 ? WSCOL_WHITE : WSCOL_BLACK;
		*bg = attr & 0x70 ? WSCOL_WHITE : WSCOL_BLACK;
		if (ul != NULL)
			*ul = *fg != WSCOL_WHITE && (attr & 0x01) ? 1 : 0;
	} else {
		*fg = pctoansi[attr & 0x07];
		*bg = pctoansi[(attr & 0x70) >> 4];
		if (ul != NULL)
			*ul = 0;
	}
	if (attr & FG_INTENSE)
		*fg += 8;
}

int
ega_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct egascreen *scr = id;
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
				scr->pcs.dispoffset = scr->mindispoffset;
			}
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

	return 0;
}

/* $OpenBSD: wsemul_sun.c,v 1.10 2002/09/15 12:54:49 fgsch Exp $ */
/* $NetBSD: wsemul_sun.c,v 1.11 2000/01/05 11:19:36 drochner Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

/*
 * This file implements a sun terminal personality for wscons.
 *
 * Derived from old rcons code.
 * Color support from NetBSD's rcons color code, and wsemul_vt100.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsemulvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/ascii.h>

void	*wsemul_sun_cnattach(const struct wsscreen_descr *, void *,
    int, int, long);
void	*wsemul_sun_attach(int, const struct wsscreen_descr *,
    void *, int, int, void *, long);
void	wsemul_sun_output(void *, const u_char *, u_int, int);
int	wsemul_sun_translate(void *, keysym_t, char **);
void	wsemul_sun_detach(void *, u_int *, u_int *);
void	wsemul_sun_resetop(void *, enum wsemul_resetops);

const struct wsemul_ops wsemul_sun_ops = {
	"sun",
	wsemul_sun_cnattach,
	wsemul_sun_attach,
	wsemul_sun_output,
	wsemul_sun_translate,
	wsemul_sun_detach,
	wsemul_sun_resetop
};

#define	SUN_EMUL_STATE_NORMAL	0	/* normal processing */
#define	SUN_EMUL_STATE_HAVEESC	1	/* seen start of ctl seq */
#define	SUN_EMUL_STATE_CONTROL	2	/* processing ctl seq */

#define	SUN_EMUL_NARGS	2		/* max # of args to a command */

struct wsemul_sun_emuldata {
	const struct wsdisplay_emulops *emulops;
	void *emulcookie;
	void *cbcookie;
	int scrcapabilities;
	u_int nrows, ncols, crow, ccol;
	long defattr;			/* default attribute (rendition) */

	u_int state;			/* processing state */
	u_int args[SUN_EMUL_NARGS];	/* command args, if CONTROL */
	int nargs;			/* number of args */

	int flags;			/* current processing flags */
#define	SUNFL_LASTCHAR	0x0001		/* printed last char on line */

	u_int scrolldist;		/* distance to scroll */
	long curattr, bkgdattr;		/* currently used attribute */
	long kernattr;			/* attribute for kernel output */
	int attrflags, fgcol, bgcol;	/* properties of curattr */

#ifdef DIAGNOSTIC
	int console;
#endif
};

void wsemul_sun_init(struct wsemul_sun_emuldata *,
    const struct wsscreen_descr *, void *, int, int, long);
void wsemul_sun_reset(struct wsemul_sun_emuldata *);
void wsemul_sun_output_lowchars(struct wsemul_sun_emuldata *, u_char, int);
void wsemul_sun_output_normal(struct wsemul_sun_emuldata *, u_char, int);
u_int wsemul_sun_output_haveesc(struct wsemul_sun_emuldata *, u_char);
u_int wsemul_sun_output_control(struct wsemul_sun_emuldata *, u_char);
void wsemul_sun_control(struct wsemul_sun_emuldata *, u_char);
int wsemul_sun_selectattribute(struct wsemul_sun_emuldata *, int, int, int,
    long *, long *);
void wsemul_sun_scrollup(struct wsemul_sun_emuldata *);

struct wsemul_sun_emuldata wsemul_sun_console_emuldata;

/* some useful utility macros */
#define	ARG(n)			(edp->args[(n)])
#define	NORMALIZE_ARG(n)	(ARG(n) ? ARG(n) : 1)
#define	COLS_LEFT		(edp->ncols - edp->ccol - 1)
#define	ROWS_LEFT		(edp->nrows - edp->crow - 1)

/*
 * wscons color codes
 * To compensate for Sun color choices on older framebuffers, these need to
 * be variables.
 */
int	wscol_white = 0;	/* 0 */
int	wscol_black = 7;	/* 255 */
int	wskernel_bg = 7;	/* 0 */
int	wskernel_fg = 0;	/* 255 */

void
wsemul_sun_init(edp, type, cookie, ccol, crow, defattr)
	struct wsemul_sun_emuldata *edp;
	const struct wsscreen_descr *type;
	void *cookie;
	int ccol, crow;
	long defattr;
{
	edp->emulops = type->textops;
	edp->emulcookie = cookie;
	edp->scrcapabilities = type->capabilities;
	edp->nrows = type->nrows;
	edp->ncols = type->ncols;
	edp->crow = crow;
	edp->ccol = ccol;
	edp->defattr = defattr;
}

void
wsemul_sun_reset(edp)
	struct wsemul_sun_emuldata *edp;
{
	edp->state = SUN_EMUL_STATE_NORMAL;
	edp->flags = 0;
	edp->bkgdattr = edp->curattr = edp->defattr;
	edp->attrflags = 0;
	edp->fgcol = WSCOL_BLACK;
	edp->bgcol = WSCOL_WHITE;
	edp->scrolldist = 1;
}

void *
wsemul_sun_cnattach(type, cookie, ccol, crow, defattr)
	const struct wsscreen_descr *type;
	void *cookie;
	int ccol, crow;
	long defattr;
{
	struct wsemul_sun_emuldata *edp;
	int res;

	edp = &wsemul_sun_console_emuldata;
	wsemul_sun_init(edp, type, cookie, ccol, crow, defattr);

#ifndef WS_KERNEL_FG
#define WS_KERNEL_FG wskernel_bg
#endif
#ifndef WS_KERNEL_BG
#define WS_KERNEL_BG wskernel_fg
#endif
#ifndef WS_KERNEL_COLATTR
#define WS_KERNEL_COLATTR 0
#endif
#ifndef WS_KERNEL_MONOATTR
#define WS_KERNEL_MONOATTR 0
#endif
	if (type->capabilities & WSSCREEN_WSCOLORS)
		res = (*edp->emulops->alloc_attr)(cookie,
					    WS_KERNEL_FG, WS_KERNEL_BG,
					    WS_KERNEL_COLATTR | WSATTR_WSCOLORS,
					    &edp->kernattr);
	else
		res = (*edp->emulops->alloc_attr)(cookie, 0, 0,
					    WS_KERNEL_MONOATTR,
					    &edp->kernattr);
	if (res)
		edp->kernattr = defattr;

	edp->cbcookie = NULL;

#ifdef DIAGNOSTIC
	edp->console = 1;
#endif

	wsemul_sun_reset(edp);
	return (edp);
}

void *
wsemul_sun_attach(console, type, cookie, ccol, crow, cbcookie, defattr)
	int console;
	const struct wsscreen_descr *type;
	void *cookie;
	int ccol, crow;
	void *cbcookie;
	long defattr;
{
	struct wsemul_sun_emuldata *edp;

	if (console) {
		edp = &wsemul_sun_console_emuldata;
#ifdef DIAGNOSTIC
		KASSERT(edp->console == 1);
#endif
	} else {
		edp = malloc(sizeof *edp, M_DEVBUF, M_WAITOK);
		wsemul_sun_init(edp, type, cookie, ccol, crow, defattr);

#ifdef DIAGNOSTIC
		edp->console = 0;
#endif
	}

	edp->cbcookie = cbcookie;

	wsemul_sun_reset(edp);
	return (edp);
}

void
wsemul_sun_output_lowchars(edp, c, kernel)
	struct wsemul_sun_emuldata *edp;
	u_char c;
	int kernel;
{
	u_int n;

	switch (c) {
	case ASCII_NUL:
	default:
		/* ignore */
		break;

	case ASCII_BEL:		/* "Bell (BEL)" */
		wsdisplay_emulbell(edp->cbcookie);
		break;

	case ASCII_BS:		/* "Backspace (BS)" */
		if (edp->ccol > 0) {
			edp->ccol--;
			CLR(edp->flags, SUNFL_LASTCHAR);
		}
		break;

	case ASCII_CR:		/* "Return (CR)" */
		edp->ccol = 0;
		CLR(edp->flags, SUNFL_LASTCHAR);
		break;

	case ASCII_HT:		/* "Tab (TAB)" */
		n = min(8 - (edp->ccol & 7), COLS_LEFT);
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
				edp->ccol, n,
				kernel ? edp->kernattr : edp->bkgdattr);
		edp->ccol += n;
		if (COLS_LEFT == 0)
			SET(edp->flags, SUNFL_LASTCHAR);
		break;

	case ASCII_FF:		/* "Form Feed (FF)" */
		wsemul_sun_resetop(edp, WSEMUL_CLEARSCREEN);
		break;

	case ASCII_VT:		/* "Reverse Line Feed" */
		if (edp->crow > 0)
			edp->crow--;
		break;

	case ASCII_ESC:		/* "Escape (ESC)" */
		if (kernel) {
			printf("wsemul_sun_output_lowchars: ESC in kernel "
			    "output ignored\n");
			break;	/* ignore the ESC */
		}

		edp->state = SUN_EMUL_STATE_HAVEESC;
		break;

	case ASCII_LF:		/* "Line Feed (LF)" */
                /* if the cur line isn't the last, incr and leave. */
		if (ROWS_LEFT > 0)
			edp->crow++;
		else
			wsemul_sun_scrollup(edp);
		break;
	}
}

void
wsemul_sun_output_normal(edp, c, kernel)
	struct wsemul_sun_emuldata *edp;
	u_char c;
	int kernel;
{
	if (ISSET(edp->flags, SUNFL_LASTCHAR)) {
                /* if the cur line isn't the last, incr and leave. */
		if (ROWS_LEFT > 0)
			edp->crow++;
		else
			wsemul_sun_scrollup(edp);
		edp->ccol = 0;
		CLR(edp->flags, SUNFL_LASTCHAR);
	}

	(*edp->emulops->putchar)(edp->emulcookie, edp->crow, edp->ccol,
	    c, kernel ? edp->kernattr : edp->curattr);

	if (COLS_LEFT)
		edp->ccol++;
	else
		SET(edp->flags, SUNFL_LASTCHAR);
}

u_int
wsemul_sun_output_haveesc(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int newstate;

	switch (c) {
	case '[':		/* continuation of multi-char sequence */
		edp->nargs = 0;
		bzero(edp->args, sizeof (edp->args));
		newstate = SUN_EMUL_STATE_CONTROL;
		break;

	default:
#ifdef DEBUG
		printf("ESC%c unknown\n", c);
#endif
		newstate = SUN_EMUL_STATE_NORMAL;	/* XXX is this wise? */
		break;
	}

	return (newstate);
}

void
wsemul_sun_control(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int n, src, dst;
	int flags, fgcol, bgcol;
	long attr, bkgdattr;

	switch (c) {
	case '@':		/* "Insert Character (ICH)" */
		n = min(NORMALIZE_ARG(0), COLS_LEFT + 1);
		src = edp->ccol;
		dst = edp->ccol + n;
		if (dst < edp->ncols) {
			(*edp->emulops->copycols)(edp->emulcookie, edp->crow,
			    src, dst, edp->ncols - dst);
		}
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
		    src, n, edp->bkgdattr);
		break;

	case 'A':		/* "Cursor Up (CUU)" */
		edp->crow -= min(NORMALIZE_ARG(0), edp->crow);
		break;

	case 'E':		/* "Cursor Next Line (CNL)" */
		edp->ccol = 0;
		/* FALLTHRU */
	case 'B':		/* "Cursor Down (CUD)" */
		edp->crow += min(NORMALIZE_ARG(0), ROWS_LEFT);
		break;

	case 'C':		/* "Cursor Forward (CUF)" */
		edp->ccol += min(NORMALIZE_ARG(0), COLS_LEFT);
		break;

	case 'D':		/* "Cursor Backward (CUB)" */
		edp->ccol -= min(NORMALIZE_ARG(0), edp->ccol);
		break;

	case 'f':		/* "Horizontal And Vertical Position (HVP)" */
	case 'H':		/* "Cursor Position (CUP)" */
		edp->crow = min(NORMALIZE_ARG(0), edp->nrows) - 1;
		edp->ccol = min(NORMALIZE_ARG(1), edp->ncols) - 1;
		break;

	case 'J':		/* "Erase in Display (ED)" */
		if (ROWS_LEFT > 0) {
			(*edp->emulops->eraserows)(edp->emulcookie,
			     edp->crow + 1, ROWS_LEFT, edp->bkgdattr);
		}
		/* FALLTHRU */
	case 'K':		/* "Erase in Line (EL)" */
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
		    edp->ccol, COLS_LEFT + 1, edp->bkgdattr);
		break;

	case 'L':		/* "Insert Line (IL)" */
		n = min(NORMALIZE_ARG(0), ROWS_LEFT + 1);
		src = edp->crow;
		dst = edp->crow + n;
		if (dst < edp->nrows) {
			(*edp->emulops->copyrows)(edp->emulcookie,
			    src, dst, edp->nrows - dst);
		}
		(*edp->emulops->eraserows)(edp->emulcookie,
		    src, dst - src, edp->bkgdattr);
		break;

	case 'M':		/* "Delete Line (DL)" */
		n = min(NORMALIZE_ARG(0), ROWS_LEFT + 1);
		src = edp->crow + n;
		dst = edp->crow;
		if (src < edp->nrows) {
			(*edp->emulops->copyrows)(edp->emulcookie,
			    src, dst, edp->nrows - src);
		}
		(*edp->emulops->eraserows)(edp->emulcookie,
		    dst + edp->nrows - src, src - dst, edp->bkgdattr);
		break;

	case 'P':		/* "Delete Character (DCH)" */
		n = min(NORMALIZE_ARG(0), COLS_LEFT + 1);
		src = edp->ccol + n;
		dst = edp->ccol;
		if (src < edp->ncols) {
			(*edp->emulops->copycols)(edp->emulcookie, edp->crow,
			    src, dst, edp->ncols - src);
		}
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
		    edp->ncols - n, n, edp->bkgdattr);
		break;

	case 'm':		/* "Select Graphic Rendition (SGR)" */
		flags = edp->attrflags;
		fgcol = edp->fgcol;
		bgcol = edp->bgcol;

		for (n = 0; n < edp->nargs; n++) {
			switch (ARG(n)) {
			/* Clear all attributes || End underline */
			case 0:
				if (n == edp->nargs - 1) {
					edp->bkgdattr =
					    edp->curattr = edp->defattr;
					edp->attrflags = 0;
					edp->fgcol = WSCOL_BLACK;
					edp->bgcol = WSCOL_WHITE;
					return;
				}
				flags = 0;
				fgcol = WSCOL_BLACK;
				bgcol = WSCOL_WHITE;
				break;
			/* Begin bold */
			case 1:
				flags |= WSATTR_HILIT;
				break;
			/* Begin underline */
			case 4:
				flags |= WSATTR_UNDERLINE;
				break;
			/* Begin reverse */
			case 7:
				flags |= WSATTR_REVERSE;
				break;
			/* ANSI foreground color */
			case 30: case 31: case 32: case 33:
			case 34: case 35: case 36: case 37:
				fgcol = ARG(n) - 30;
				break;
			/* ANSI background color */
			case 40: case 41: case 42: case 43:
			case 44: case 45: case 46: case 47:
				bgcol = ARG(n) - 40;
				break;
			}
		}
setattr:
		if (wsemul_sun_selectattribute(edp, flags, fgcol, bgcol, &attr,
		    &bkgdattr)) {
#ifdef DEBUG
			printf("error allocating attr %d/%d/%x\n",
			    fgcol, bgcol, flags);
#endif
		} else {
			edp->curattr = attr;
			edp->bkgdattr = bkgdattr;
			edp->attrflags = flags;
			edp->fgcol = fgcol;
			edp->bgcol = bgcol;
		}
		break;

	case 'p':		/* "Black On White (SUNBOW)" */
		fgcol = WSCOL_BLACK;
		bgcol = WSCOL_WHITE;
		goto setattr;

	case 'q':		/* "White On Black (SUNWOB)" */
		fgcol = WSCOL_WHITE;
		bgcol = WSCOL_BLACK;
		goto setattr;

	case 'r':		/* "Set Scrolling (SUNSCRL)" */
		edp->scrolldist = min(ARG(0), edp->nrows);
		break;

	case 's':		/* "Reset Terminal Emulator (SUNRESET)" */
		wsemul_sun_reset(edp);
		break;
	}

	if (COLS_LEFT)
		CLR(edp->flags, SUNFL_LASTCHAR);
	else
		SET(edp->flags, SUNFL_LASTCHAR);
}

u_int
wsemul_sun_output_control(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int newstate = SUN_EMUL_STATE_CONTROL;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4': /* argument digit */
	case '5': case '6': case '7': case '8': case '9':
		if (edp->nargs > SUN_EMUL_NARGS - 1)
			break;
		edp->args[edp->nargs] = (edp->args[edp->nargs] * 10) +
		    (c - '0');
                break;

	case ';':		/* argument terminator */
		edp->nargs++;
		break;

	default:		/* end of escape sequence */
		edp->nargs++;
		if (edp->nargs > SUN_EMUL_NARGS)
			edp->nargs = SUN_EMUL_NARGS;
		wsemul_sun_control(edp, c);
		newstate = SUN_EMUL_STATE_NORMAL;
		break;
	}
	return (newstate);
}

void
wsemul_sun_output(cookie, data, count, kernel)
	void *cookie;
	const u_char *data;
	u_int count;
	int kernel;
{
	struct wsemul_sun_emuldata *edp = cookie;
	u_int newstate;

#ifdef DIAGNOSTIC
	if (kernel && !edp->console)
		panic("wsemul_sun_output: kernel output, not console");
#endif

	/* XXX */
	(*edp->emulops->cursor)(edp->emulcookie, 0, edp->crow, edp->ccol);
	for (; count > 0; data++, count--) {
		if (*data < ' ') {
			wsemul_sun_output_lowchars(edp, *data, kernel);
			continue;
		}

		if (kernel) {
			wsemul_sun_output_normal(edp, *data, 1);
			continue;
		}

		switch (newstate = edp->state) {
		case SUN_EMUL_STATE_NORMAL:
			wsemul_sun_output_normal(edp, *data, 0);
			break;
		case SUN_EMUL_STATE_HAVEESC:
			newstate = wsemul_sun_output_haveesc(edp, *data);
			break;
		case SUN_EMUL_STATE_CONTROL:
			newstate = wsemul_sun_output_control(edp, *data);
			break;
		default:
#ifdef DIAGNOSTIC
			panic("wsemul_sun: invalid state %d", edp->state);
#else
                        /* try to recover, if things get screwed up... */
			newstate = SUN_EMUL_STATE_NORMAL;
			wsemul_sun_output_normal(edp, *data, 0);
#endif
                        break;
		}
		edp->state = newstate;
	}
	/* XXX */
	(*edp->emulops->cursor)(edp->emulcookie, 1, edp->crow, edp->ccol);
}


/*
 * Get an attribute from the graphics driver.
 * Try to find replacements if the desired appearance is not supported.
 */
int
wsemul_sun_selectattribute(edp, flags, fgcol, bgcol, attr, bkgdattr)
	struct wsemul_sun_emuldata *edp;
	int flags, fgcol, bgcol;
	long *attr, *bkgdattr;
{
	int error;

	/*
	 * Rasops will force white on black as normal output colors, unless
	 * WSATTR_WSCOLORS is specified. Since Sun console is black on white,
	 * always use WSATTR_WSCOLORS and our colors, as we know better.
	 */
	if (!(edp->scrcapabilities & WSSCREEN_WSCOLORS)) {
		flags &= ~WSATTR_WSCOLORS;
	} else {
		flags |= WSATTR_WSCOLORS;
	}

	error = (*edp->emulops->alloc_attr)(edp->emulcookie, fgcol, bgcol,
					    flags & WSATTR_WSCOLORS, bkgdattr);
	if (error)
		return (error);

	if ((flags & WSATTR_HILIT) &&
	    !(edp->scrcapabilities & WSSCREEN_HILIT)) {
		flags &= ~WSATTR_HILIT;
		if (edp->scrcapabilities & WSSCREEN_WSCOLORS) {
			fgcol = WSCOL_RED;
			flags |= WSATTR_WSCOLORS;
		}
	}
	if ((flags & WSATTR_UNDERLINE) &&
	    !(edp->scrcapabilities & WSSCREEN_UNDERLINE)) {
		flags &= ~WSATTR_UNDERLINE;
		if (edp->scrcapabilities & WSSCREEN_WSCOLORS) {
			fgcol = WSCOL_CYAN;
			flags &= ~WSATTR_UNDERLINE;
			flags |= WSATTR_WSCOLORS;
		}
	}
	if ((flags & WSATTR_BLINK) &&
	    !(edp->scrcapabilities & WSSCREEN_BLINK)) {
		flags &= ~WSATTR_BLINK;
	}
	if ((flags & WSATTR_REVERSE) &&
	    !(edp->scrcapabilities & WSSCREEN_REVERSE)) {
		flags &= ~WSATTR_REVERSE;
		if (edp->scrcapabilities & WSSCREEN_WSCOLORS) {
			int help;
			help = bgcol;
			bgcol = fgcol;
			fgcol = help;
			flags |= WSATTR_WSCOLORS;
		}
	}
	error = (*edp->emulops->alloc_attr)(edp->emulcookie, fgcol, bgcol,
					    flags, attr);
	if (error)
		return (error);

	return (0);
}

static char *sun_fkeys[] = {
	"\033[224z",	/* F1 */
	"\033[225z",
	"\033[226z",
	"\033[227z",
	"\033[228z",
	"\033[229z",
	"\033[230z",
	"\033[231z",
	"\033[232z",
	"\033[233z",
	"\033[234z",
	"\033[235z",	/* F12 */
};

static char *sun_lkeys[] = {
	"\033[207z",	/* KS_Help */
	NULL,		/* KS_Execute */
	"\033[200z",	/* KS_Find */
	NULL,		/* KS_Select */
	"\033[193z",	/* KS_Again */
	"\033[194z",	/* KS_Props */
	"\033[195z",	/* KS_Undo */
	"\033[196z",	/* KS_Front */
	"\033[197z",	/* KS_Copy */
	"\033[198z",	/* KS_Open */
	"\033[199z",	/* KS_Paste */
	"\033[201z",	/* KS_Cut */
};

int
wsemul_sun_translate(cookie, in, out)
	void *cookie;
	keysym_t in;
	char **out;
{
	static char c;

	if (KS_GROUP(in) == KS_GROUP_Keypad && (in & 0x80) == 0) {
		c = in & 0xff; /* turn into ASCII */
		*out = &c;
		return (1);
	}

	if (in >= KS_f1 && in <= KS_f12) {
		*out = sun_fkeys[in - KS_f1];
		return (6);
	}
	if (in >= KS_F1 && in <= KS_F12) {
		*out = sun_fkeys[in - KS_F1];
		return (6);
	}
	if (in >= KS_KP_F1 && in <= KS_KP_F4) {
		*out = sun_fkeys[in - KS_KP_F1];
		return (6);
	}
	if (in >= KS_Help && in <= KS_Cut && sun_lkeys[in - KS_Help] != NULL) {
		*out = sun_lkeys[in - KS_Help];
		return (6);
	}

	switch (in) {
	case KS_Home:
	case KS_KP_Home:
	case KS_KP_Begin:
		*out = "\033[214z";
		return (6);
	case KS_End:
	case KS_KP_End:
		*out = "\033[220z";
		return (6);
	case KS_Insert:
	case KS_KP_Insert:
		*out = "\033[247z";
		return (6);
	case KS_Prior:
	case KS_KP_Prior:
		*out = "\033[216z";
		return (6);
	case KS_Next:
	case KS_KP_Next:
		*out = "\033[222z";
		return (6);
	case KS_Up:
	case KS_KP_Up:
		*out = "\033[A";
		return (3);
	case KS_Down:
	case KS_KP_Down:
		*out = "\033[B";
		return (3);
	case KS_Left:
	case KS_KP_Left:
		*out = "\033[D";
		return (3);
	case KS_Right:
	case KS_KP_Right:
		*out = "\033[C";
		return (3);
	case KS_KP_Delete:
		*out = "\177";
		return (1);
	}
	return (0);
}

void
wsemul_sun_detach(cookie, crowp, ccolp)
	void *cookie;
	u_int *crowp, *ccolp;
{
	struct wsemul_sun_emuldata *edp = cookie;

	*crowp = edp->crow;
	*ccolp = edp->ccol;
	if (edp != &wsemul_sun_console_emuldata)
		free(edp, M_DEVBUF);
}

void
wsemul_sun_resetop(cookie, op)
	void *cookie;
	enum wsemul_resetops op;
{
	struct wsemul_sun_emuldata *edp = cookie;

	switch (op) {
	case WSEMUL_RESET:
		wsemul_sun_reset(edp);
		break;
	case WSEMUL_CLEARSCREEN:
		(*edp->emulops->eraserows)(edp->emulcookie, 0, edp->nrows,
					   edp->bkgdattr);
		edp->ccol = edp->crow = 0;
		CLR(edp->flags, SUNFL_LASTCHAR);
		(*edp->emulops->cursor)(edp->emulcookie, 1, 0, 0);
		break;
	default:
		break;
	}
}

void
wsemul_sun_scrollup(edp)
	struct wsemul_sun_emuldata *edp;
{
	/*
	 * if we're in wrap-around mode, go to the first
	 * line and clear it.
	 */
	if (edp->scrolldist == 0) {
		edp->crow = 0;
		(*edp->emulops->eraserows)(edp->emulcookie, 0, 1,
		    edp->bkgdattr);
		return;
	}

	/* scroll by the scrolling distance. */
	(*edp->emulops->copyrows)(edp->emulcookie, edp->scrolldist, 0,
	    edp->nrows - edp->scrolldist);
	(*edp->emulops->eraserows)(edp->emulcookie,
	    edp->nrows - edp->scrolldist, edp->scrolldist, edp->bkgdattr);
	edp->crow -= edp->scrolldist - 1;
}

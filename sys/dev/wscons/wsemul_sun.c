/* $OpenBSD: wsemul_sun.c,v 1.6 2002/07/25 19:03:25 miod Exp $ */
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

/* XXX DESCRIPTION/SOURCE OF INFORMATION */

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
void	*wsemul_sun_attach(int console, const struct wsscreen_descr *,
				void *, int, int, void *, long);
void	wsemul_sun_output(void *cookie, const u_char *data, u_int count,
			       int);
int	wsemul_sun_translate(void *cookie, keysym_t, char **);
void	wsemul_sun_detach(void *cookie, u_int *crowp, u_int *ccolp);
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

	u_int state;			/* processing state */
	u_int args[SUN_EMUL_NARGS];	/* command args, if CONTROL */
	u_int scrolldist;		/* distance to scroll */
	long defattr;			/* default attribute (rendition) */
	long bowattr;			/* attribute for reversed mode */
	int rendflags;
#define REND_BOW 1
#define REND_SO 2
	long curattr;			/* currently used attribute */
	long kernattr;			/* attribute for kernel output */
#ifdef DIAGNOSTIC
	int console;
#endif
};

u_int wsemul_sun_output_normal(struct wsemul_sun_emuldata *, u_char,
				    int);
u_int wsemul_sun_output_haveesc(struct wsemul_sun_emuldata *, u_char);
u_int wsemul_sun_output_control(struct wsemul_sun_emuldata *, u_char);
void wsemul_sun_control(struct wsemul_sun_emuldata *, u_char);

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

	edp->emulops = type->textops;
	edp->emulcookie = cookie;
	edp->scrcapabilities = type->capabilities;
	edp->nrows = type->nrows;
	edp->ncols = type->ncols;
	edp->crow = crow;
	edp->ccol = ccol;
	edp->curattr = edp->defattr = defattr;

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

	edp->state = SUN_EMUL_STATE_NORMAL;
	edp->scrolldist = 1;
#ifdef DIAGNOSTIC
	edp->console = 1;
#endif
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

		edp->emulops = type->textops;
		edp->emulcookie = cookie;
		edp->scrcapabilities = type->capabilities;
		edp->nrows = type->nrows;
		edp->ncols = type->ncols;
		edp->crow = crow;
		edp->ccol = ccol;
		edp->defattr = defattr;

		edp->state = SUN_EMUL_STATE_NORMAL;
		edp->scrolldist = 1;
#ifdef DIAGNOSTIC
		edp->console = 0;
#endif
	}

	edp->cbcookie = cbcookie;

	/* XXX This assumes that the default attribute is wob. */
	if ((!(edp->scrcapabilities & WSSCREEN_WSCOLORS) ||
		(*edp->emulops->alloc_attr)(edp->emulcookie,
					    WSCOL_WHITE, WSCOL_BLACK,
					    WSATTR_WSCOLORS,
					    &edp->bowattr)) &&
	    (!(edp->scrcapabilities & WSSCREEN_REVERSE) ||
		(*edp->emulops->alloc_attr)(edp->emulcookie, 0, 0,
					    WSATTR_REVERSE,
					    &edp->bowattr)))
		edp->bowattr = edp->defattr;

	edp->curattr = edp->defattr;
	edp->rendflags = 0;

	return (edp);
}

inline u_int
wsemul_sun_output_normal(edp, c, kernel)
	struct wsemul_sun_emuldata *edp;
	u_char c;
	int kernel;
{
	u_int newstate = SUN_EMUL_STATE_NORMAL;
	u_int n;

	switch (c) {
	case ASCII_BEL:		/* "Bell (BEL)" */
		wsdisplay_emulbell(edp->cbcookie);
		break;

	case ASCII_BS:		/* "Backspace (BS)" */
		if (edp->ccol > 0)
			edp->ccol--;
		break;

	case ASCII_CR:		/* "Return (CR)" */
		edp->ccol = 0;
		break;

	case ASCII_HT:		/* "Tab (TAB)" */
		n = min(8 - (edp->ccol & 7), COLS_LEFT);
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
				edp->ccol, n,
				kernel ? edp->kernattr : edp->curattr);
		edp->ccol += n;
		break;

	case ASCII_FF:		/* "Form Feed (FF)" */
		(*edp->emulops->eraserows)(edp->emulcookie, 0, edp->nrows,
				kernel ? edp->kernattr : edp->curattr);
				/* XXX possible in kernel output? */
		edp->ccol = 0;
		edp->crow = 0;
		break;

	case ASCII_VT:		/* "Reverse Line Feed" */
		if (edp->crow > 0)
			edp->crow--;
		break;

	case ASCII_ESC:		/* "Escape (ESC)" */
		if (kernel) {
			printf("wsemul_sun_output_normal: ESC in kernel "
			    "output ignored\n");
			break;	/* ignore the ESC */
		}

		if (edp->state == SUN_EMUL_STATE_NORMAL) {
			newstate = SUN_EMUL_STATE_HAVEESC;
			break;
		}
		/* special case: fall through, we're printing one out */
		/* FALLTHRU */

	default:		/* normal character */
		(*edp->emulops->putchar)(edp->emulcookie, edp->crow, edp->ccol,
		    c, kernel ? edp->kernattr : edp->curattr);
		edp->ccol++;

		/* if cur col is still on cur line, done. */
		if (edp->ccol < edp->ncols)
			break;

		/* wrap the column around. */
		edp->ccol = 0;

               	/* FALLTHRU */

	case ASCII_LF:		/* "Line Feed (LF)" */
                /* if the cur line isn't the last, incr and leave. */
		if (edp->crow < edp->nrows - 1) {
			edp->crow++;
			break;
		}

		/*
		 * if we're in wrap-around mode, go to the first
		 * line and clear it.
		 */
		if (edp->scrolldist == 0) {
			edp->crow = 0;
			(*edp->emulops->eraserows)(edp->emulcookie, 0, 1,
						   edp->curattr);
			break;
		}

		/* scroll by the scrolling distance. */
		(*edp->emulops->copyrows)(edp->emulcookie, edp->scrolldist, 0,
		    edp->nrows - edp->scrolldist);
		(*edp->emulops->eraserows)(edp->emulcookie,
		    edp->nrows - edp->scrolldist, edp->scrolldist,
					   edp->curattr);
		edp->crow -= edp->scrolldist - 1;
		break;
	}

	return (newstate);
}

inline u_int
wsemul_sun_output_haveesc(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int newstate;

	switch (c) {
	case '[':		/* continuation of multi-char sequence */
		bzero(edp->args, sizeof (edp->args));
		newstate = SUN_EMUL_STATE_CONTROL;
		break;

	default:
		/* spit out the escape char (???), then the new character */
		wsemul_sun_output_normal(edp, ASCII_ESC, 0);	/* ??? */
		newstate = wsemul_sun_output_normal(edp, c, 0);
		break;
	}

	return (newstate);
}

inline void
wsemul_sun_control(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int n, src, dst;

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
		    src, n, edp->curattr);
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
		edp->crow = min(NORMALIZE_ARG(1), edp->nrows) - 1;
		edp->ccol = min(NORMALIZE_ARG(0), edp->ncols) - 1;
		break;

	case 'J':		/* "Erase in Display (ED)" */
		if (ROWS_LEFT > 0) {
			(*edp->emulops->eraserows)(edp->emulcookie,
			     edp->crow + 1, ROWS_LEFT, edp->curattr);
		}
		/* FALLTHRU */
	case 'K':		/* "Erase in Line (EL)" */
		(*edp->emulops->erasecols)(edp->emulcookie, edp->crow,
		    edp->ccol, COLS_LEFT + 1, edp->curattr);
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
		    src, dst - src, edp->curattr);
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
		    dst + edp->nrows - src, src - dst, edp->curattr);
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
		    edp->ncols - n, n, edp->curattr);
		break;

	case 'm':		/* "Select Graphic Rendition (SGR)" */
		if (ARG(0))
			edp->rendflags |= REND_SO;
		else
			edp->rendflags &= ~REND_SO;
		goto setattr;

	case 'p':		/* "Black On White (SUNBOW)" */
		edp->rendflags |= REND_BOW;
		goto setattr;

	case 'q':		/* "White On Black (SUNWOB)" */
		edp->rendflags &= ~REND_BOW;
		goto setattr;

	case 'r':		/* "Set Scrolling (SUNSCRL)" */
		edp->scrolldist = min(ARG(0), edp->nrows);
		break;

	case 's':		/* "Reset Terminal Emulator (SUNRESET)" */
		edp->scrolldist = 1;
		edp->rendflags = 0;
setattr:
		if (((edp->rendflags & REND_BOW) != 0) ^
		    ((edp->rendflags & REND_SO) != 0))
			edp->curattr = edp->bowattr;
		else
			edp->curattr = edp->defattr;
		break;
	}
}

inline u_int
wsemul_sun_output_control(edp, c)
	struct wsemul_sun_emuldata *edp;
	u_char c;
{
	u_int newstate = SUN_EMUL_STATE_CONTROL;
	u_int i;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4': /* argument digit */
	case '5': case '6': case '7': case '8': case '9':
		edp->args[0] = (edp->args[0] * 10) + (c - '0');
                break;

	case ';':		/* argument terminator */
		for (i = 1; i < SUN_EMUL_NARGS; i++)
			edp->args[i] = edp->args[i - 1];
		edp->args[0] = 0;
		break;

	default:		/* end of escape sequence */
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
		if (kernel) {
			wsemul_sun_output_normal(edp, *data, 1);
			continue;
		}
		switch (edp->state) {
		case SUN_EMUL_STATE_NORMAL:
			/* XXX SCAN INPUT FOR NEWLINES, DO PRESCROLLING */
			newstate = wsemul_sun_output_normal(edp, *data, 0);
			break;
		case SUN_EMUL_STATE_HAVEESC:
			newstate = wsemul_sun_output_haveesc(edp, *data);
			break;
		case SUN_EMUL_STATE_CONTROL:
			newstate = wsemul_sun_output_control(edp, *data);
			break;
		default:
#ifdef DIAGNOSTIC
			panic("wsemul_sun: invalid state %d\n", edp->state);
#endif
                        /* try to recover, if things get screwed up... */
			newstate = wsemul_sun_output_normal(edp, *data, 0);
                        break;
		}
		edp->state = newstate;
	}
	/* XXX */
	(*edp->emulops->cursor)(edp->emulcookie, 1, edp->crow, edp->ccol);
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
	"\033[233z",	/* F10 */
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

	if (in >= KS_f1 && in <= KS_f10) {
		*out = sun_fkeys[in - KS_f1];
		return (6);
	}
	if (in >= KS_F1 && in <= KS_F10) {
		*out = sun_fkeys[in - KS_F1];
		return (6);
	}
	if (in >= KS_KP_F1 && in <= KS_KP_F4) {
		*out = sun_fkeys[in - KS_KP_F1];
		return (6);
	}

	switch (in) {
	    case KS_Home:
	    case KS_KP_Home:
	    case KS_KP_Begin:
		*out = "\033[214z";
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
		edp->state = SUN_EMUL_STATE_NORMAL;
		edp->scrolldist = 1;
		edp->rendflags = 0;
		edp->curattr = edp->defattr;
		break;
	case WSEMUL_CLEARSCREEN:
		(*edp->emulops->eraserows)(edp->emulcookie, 0, edp->nrows,
					   edp->defattr);
		edp->ccol = edp->crow = 0;
		(*edp->emulops->cursor)(edp->emulcookie, 1, 0, 0);
		break;
	default:
		break;
	}
}

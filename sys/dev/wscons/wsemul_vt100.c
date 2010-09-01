/* $OpenBSD: wsemul_vt100.c,v 1.27 2010/09/01 21:17:16 nicm Exp $ */
/* $NetBSD: wsemul_vt100.c,v 1.13 2000/04/28 21:56:16 mycroft Exp $ */

/*
 * Copyright (c) 1998
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

#ifndef	SMALL_KERNEL
#define	JUMP_SCROLL
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsemulvar.h>
#include <dev/wscons/wsemul_vt100var.h>
#include <dev/wscons/ascii.h>

void	*wsemul_vt100_cnattach(const struct wsscreen_descr *, void *,
				  int, int, long);
void	*wsemul_vt100_attach(int, const struct wsscreen_descr *,
				  void *, int, int, void *, long);
u_int	wsemul_vt100_output(void *, const u_char *, u_int, int);
void	wsemul_vt100_detach(void *, u_int *, u_int *);
void	wsemul_vt100_resetop(void *, enum wsemul_resetops);

const struct wsemul_ops wsemul_vt100_ops = {
	"vt100",
	wsemul_vt100_cnattach,
	wsemul_vt100_attach,
	wsemul_vt100_output,
	wsemul_vt100_translate,
	wsemul_vt100_detach,
	wsemul_vt100_resetop
};

struct wsemul_vt100_emuldata wsemul_vt100_console_emuldata;

void	wsemul_vt100_init(struct wsemul_vt100_emuldata *,
	    const struct wsscreen_descr *, void *, int, int, long);
int	wsemul_vt100_jump_scroll(struct wsemul_vt100_emuldata *,
	    const u_char *, u_int, int);
int	wsemul_vt100_output_normal(struct wsemul_vt100_emuldata *, u_char, int);
int	wsemul_vt100_output_c0c1(struct wsemul_vt100_emuldata *, u_char, int);
int	wsemul_vt100_nextline(struct wsemul_vt100_emuldata *);
typedef int vt100_handler(struct wsemul_vt100_emuldata *, u_char);
vt100_handler
	wsemul_vt100_output_esc,
	wsemul_vt100_output_csi,
	wsemul_vt100_output_scs94,
	wsemul_vt100_output_scs94_percent,
	wsemul_vt100_output_scs96,
	wsemul_vt100_output_scs96_percent,
	wsemul_vt100_output_esc_hash,
	wsemul_vt100_output_esc_spc,
	wsemul_vt100_output_string,
	wsemul_vt100_output_string_esc,
	wsemul_vt100_output_dcs,
	wsemul_vt100_output_dcs_dollar;

#define	VT100_EMUL_STATE_NORMAL		0	/* normal processing */
#define	VT100_EMUL_STATE_ESC		1	/* got ESC */
#define	VT100_EMUL_STATE_CSI		2	/* got CSI (ESC[) */
#define	VT100_EMUL_STATE_SCS94		3	/* got ESC{()*+} */
#define	VT100_EMUL_STATE_SCS94_PERCENT	4	/* got ESC{()*+}% */
#define	VT100_EMUL_STATE_SCS96		5	/* got ESC{-./} */
#define	VT100_EMUL_STATE_SCS96_PERCENT	6	/* got ESC{-./}% */
#define	VT100_EMUL_STATE_ESC_HASH	7	/* got ESC# */
#define	VT100_EMUL_STATE_ESC_SPC	8	/* got ESC<SPC> */
#define	VT100_EMUL_STATE_STRING		9	/* waiting for ST (ESC\) */
#define	VT100_EMUL_STATE_STRING_ESC	10	/* waiting for ST, got ESC */
#define	VT100_EMUL_STATE_DCS		11	/* got DCS (ESC P) */
#define	VT100_EMUL_STATE_DCS_DOLLAR	12	/* got DCS<p>$ */

vt100_handler *vt100_output[] = {
	wsemul_vt100_output_esc,
	wsemul_vt100_output_csi,
	wsemul_vt100_output_scs94,
	wsemul_vt100_output_scs94_percent,
	wsemul_vt100_output_scs96,
	wsemul_vt100_output_scs96_percent,
	wsemul_vt100_output_esc_hash,
	wsemul_vt100_output_esc_spc,
	wsemul_vt100_output_string,
	wsemul_vt100_output_string_esc,
	wsemul_vt100_output_dcs,
	wsemul_vt100_output_dcs_dollar,
};

void
wsemul_vt100_init(struct wsemul_vt100_emuldata *edp,
    const struct wsscreen_descr *type, void *cookie, int ccol, int crow,
    long defattr)
{
	edp->emulops = type->textops;
	edp->emulcookie = cookie;
	edp->scrcapabilities = type->capabilities;
	edp->nrows = type->nrows;
	edp->ncols = type->ncols;
	edp->crow = crow;
	edp->ccol = ccol;
	edp->defattr = defattr;
	wsemul_reset_abortstate(&edp->abortstate);
}

void *
wsemul_vt100_cnattach(const struct wsscreen_descr *type, void *cookie, int ccol,
    int crow, long defattr)
{
	struct wsemul_vt100_emuldata *edp;
	int res;

	edp = &wsemul_vt100_console_emuldata;
	wsemul_vt100_init(edp, type, cookie, ccol, crow, defattr);
#ifdef DIAGNOSTIC
	edp->console = 1;
#endif
	edp->cbcookie = NULL;

#ifndef WS_KERNEL_FG
#define WS_KERNEL_FG WSCOL_WHITE
#endif
#ifndef WS_KERNEL_BG
#define WS_KERNEL_BG WSCOL_BLUE
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
		    WS_KERNEL_COLATTR | WSATTR_WSCOLORS, &edp->kernattr);
	else
		res = (*edp->emulops->alloc_attr)(cookie, 0, 0,
		    WS_KERNEL_MONOATTR, &edp->kernattr);
	if (res)
		edp->kernattr = defattr;

	edp->tabs = NULL;
	edp->dblwid = NULL;
	edp->dw = 0;
	edp->dcsarg = 0;
	edp->isolatin1tab = edp->decgraphtab = edp->dectechtab = NULL;
	edp->nrctab = NULL;
	wsemul_vt100_reset(edp);
	return (edp);
}

void *
wsemul_vt100_attach(int console, const struct wsscreen_descr *type,
    void *cookie, int ccol, int crow, void *cbcookie, long defattr)
{
	struct wsemul_vt100_emuldata *edp;

	if (console) {
		edp = &wsemul_vt100_console_emuldata;
#ifdef DIAGNOSTIC
		KASSERT(edp->console == 1);
#endif
	} else {
		edp = malloc(sizeof *edp, M_DEVBUF, M_NOWAIT);
		if (edp == NULL)
			return (NULL);
		wsemul_vt100_init(edp, type, cookie, ccol, crow, defattr);
#ifdef DIAGNOSTIC
		edp->console = 0;
#endif
	}
	edp->cbcookie = cbcookie;

	edp->tabs = malloc(edp->ncols, M_DEVBUF, M_NOWAIT);
	edp->dblwid = malloc(edp->nrows, M_DEVBUF, M_NOWAIT | M_ZERO);
	edp->dw = 0;
	edp->dcsarg = malloc(DCS_MAXLEN, M_DEVBUF, M_NOWAIT);
	edp->isolatin1tab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->decgraphtab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->dectechtab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	edp->nrctab = malloc(128 * sizeof(int), M_DEVBUF, M_NOWAIT);
	vt100_initchartables(edp);
	wsemul_vt100_reset(edp);
	return (edp);
}

void
wsemul_vt100_detach(void *cookie, u_int *crowp, u_int *ccolp)
{
	struct wsemul_vt100_emuldata *edp = cookie;

	*crowp = edp->crow;
	*ccolp = edp->ccol;
#define f(ptr) if (ptr) {free(ptr, M_DEVBUF); ptr = NULL;}
	f(edp->tabs)
	f(edp->dblwid)
	f(edp->dcsarg)
	f(edp->isolatin1tab)
	f(edp->decgraphtab)
	f(edp->dectechtab)
	f(edp->nrctab)
#undef f
	if (edp != &wsemul_vt100_console_emuldata)
		free(edp, M_DEVBUF);
}

void
wsemul_vt100_resetop(void *cookie, enum wsemul_resetops op)
{
	struct wsemul_vt100_emuldata *edp = cookie;

	switch (op) {
	case WSEMUL_RESET:
		wsemul_vt100_reset(edp);
		break;
	case WSEMUL_SYNCFONT:
		vt100_initchartables(edp);
		break;
	case WSEMUL_CLEARSCREEN:
		(void)wsemul_vt100_ed(edp, 2);
		edp->ccol = edp->crow = 0;
		(*edp->emulops->cursor)(edp->emulcookie,
		    edp->flags & VTFL_CURSORON, 0, 0);
		break;
	case WSEMUL_CLEARCURSOR:
		(*edp->emulops->cursor)(edp->emulcookie, 0, edp->crow,
		    edp->ccol);
		break;
	default:
		break;
	}
}

void
wsemul_vt100_reset(struct wsemul_vt100_emuldata *edp)
{
	int i;

	edp->state = VT100_EMUL_STATE_NORMAL;
	edp->flags = VTFL_DECAWM | VTFL_CURSORON;
	edp->bkgdattr = edp->curattr = edp->defattr;
	edp->attrflags = 0;
	edp->fgcol = WSCOL_WHITE;
	edp->bgcol = WSCOL_BLACK;
	edp->scrreg_startrow = 0;
	edp->scrreg_nrows = edp->nrows;
	if (edp->tabs) {
		memset(edp->tabs, 0, edp->ncols);
		for (i = 8; i < edp->ncols; i += 8)
			edp->tabs[i] = 1;
	}
	edp->dcspos = 0;
	edp->dcstype = 0;
	edp->chartab_G[0] = NULL;
	edp->chartab_G[1] = edp->nrctab; /* ??? */
	edp->chartab_G[2] = edp->isolatin1tab;
	edp->chartab_G[3] = edp->isolatin1tab;
	edp->chartab0 = 0;
	edp->chartab1 = 2;
	edp->sschartab = 0;
}

/*
 * Move the cursor to the next line if possible. If the cursor is at
 * the bottom of the scroll area, then scroll it up. If the cursor is
 * at the bottom of the screen then don't move it down.
 */
int
wsemul_vt100_nextline(struct wsemul_vt100_emuldata *edp)
{
	int rc;

	if (ROWS_BELOW == 0) {
		/* Bottom of the scroll region. */
	  	rc = wsemul_vt100_scrollup(edp, 1);
	} else {
		if ((edp->crow+1) < edp->nrows)
			/* Cursor not at the bottom of the screen. */
			edp->crow++;
		CHECK_DW;
		rc = 0;
	}

	return rc;
}

/*
 * now all the state machine bits
 */

int
wsemul_vt100_output_normal(struct wsemul_vt100_emuldata *edp, u_char c,
    int kernel)
{
	u_int *ct, dc;
	int oldsschartab = edp->sschartab;
	int rc = 0;

	if ((edp->flags & (VTFL_LASTCHAR | VTFL_DECAWM)) ==
	    (VTFL_LASTCHAR | VTFL_DECAWM)) {
		rc = wsemul_vt100_nextline(edp);
		if (rc != 0)
			return rc;
		edp->ccol = 0;
		edp->flags &= ~VTFL_LASTCHAR;
	}

	if (c & 0x80) {
		c &= 0x7f;
		ct = edp->chartab_G[edp->chartab1];
	} else {
		if (edp->sschartab) {
			ct = edp->chartab_G[edp->sschartab];
			edp->sschartab = 0;
		} else
			ct = edp->chartab_G[edp->chartab0];
	}
	dc = (ct ? ct[c] : c);

	if ((edp->flags & VTFL_INSERTMODE) && COLS_LEFT) {
		WSEMULOP(rc, edp, &edp->abortstate, copycols,
		    COPYCOLS(edp->ccol, edp->ccol + 1, COLS_LEFT));
		if (rc != 0) {
			/* undo potential sschartab update */
			edp->sschartab = oldsschartab;

			return rc;
		}
	}

	WSEMULOP(rc, edp, &edp->abortstate, putchar,
	    (edp->emulcookie, edp->crow, edp->ccol << edp->dw, dc,
	     kernel ? edp->kernattr : edp->curattr));
	if (rc != 0) {
		/* undo potential sschartab update */
		edp->sschartab = oldsschartab;

		return rc;
	}

	if (COLS_LEFT)
		edp->ccol++;
	else
		edp->flags |= VTFL_LASTCHAR;

	return 0;
}

int
wsemul_vt100_output_c0c1(struct wsemul_vt100_emuldata *edp, u_char c,
    int kernel)
{
	u_int n;
	int rc = 0;

	switch (c) {
	case ASCII_NUL:
	default:
		/* ignore */
		break;
	case ASCII_BEL:
		wsdisplay_emulbell(edp->cbcookie);
		break;
	case ASCII_BS:
		if (edp->ccol > 0) {
			edp->ccol--;
			edp->flags &= ~VTFL_LASTCHAR;
		}
		break;
	case ASCII_CR:
		edp->ccol = 0;
		break;
	case ASCII_HT:
		if (edp->tabs) {
			if (!COLS_LEFT)
				break;
			for (n = edp->ccol + 1; n < NCOLS - 1; n++)
				if (edp->tabs[n])
					break;
		} else {
			n = edp->ccol + min(8 - (edp->ccol & 7), COLS_LEFT);
		}
		edp->ccol = n;
		break;
	case ASCII_SO: /* LS1 */
		edp->chartab0 = 1;
		break;
	case ASCII_SI: /* LS0 */
		edp->chartab0 = 0;
		break;
	case ASCII_ESC:
		if (kernel) {
			printf("wsemul_vt100_output_c0c1: ESC in kernel "
			    "output ignored\n");
			break;	/* ignore the ESC */
		}

		if (edp->state == VT100_EMUL_STATE_STRING) {
			/* might be a string end */
			edp->state = VT100_EMUL_STATE_STRING_ESC;
		} else {
			/* XXX cancel current escape sequence */
			edp->state = VT100_EMUL_STATE_ESC;
		}
		break;
	case ASCII_CAN:
	case ASCII_SUB:
		/* cancel current escape sequence */
		edp->state = VT100_EMUL_STATE_NORMAL;
		break;
#if 0
	case CSI: /* 8-bit */
		/* XXX cancel current escape sequence */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		edp->modif1 = edp->modif2 = '\0';
		edp->state = VT100_EMUL_STATE_CSI;
		break;
	case DCS: /* 8-bit */
		/* XXX cancel current escape sequence */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		edp->state = VT100_EMUL_STATE_DCS;
		break;
	case ST: /* string end 8-bit */
		/* XXX only in VT100_EMUL_STATE_STRING */
		wsemul_vt100_handle_dcs(edp);
		return (VT100_EMUL_STATE_NORMAL);
#endif
	case ASCII_LF:
	case ASCII_VT:
	case ASCII_FF:
		rc = wsemul_vt100_nextline(edp);
		break;
	}

	if (COLS_LEFT != 0)
		edp->flags &= ~VTFL_LASTCHAR;

	return rc;
}

int
wsemul_vt100_output_esc(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_NORMAL;
	int rc = 0;
	int i;

	switch (c) {
	case '[': /* CSI */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		edp->modif1 = edp->modif2 = '\0';
		newstate = VT100_EMUL_STATE_CSI;
		break;
	case '7': /* DECSC */
		edp->flags |= VTFL_SAVEDCURS;
		edp->savedcursor_row = edp->crow;
		edp->savedcursor_col = edp->ccol;
		edp->savedattr = edp->curattr;
		edp->savedbkgdattr = edp->bkgdattr;
		edp->savedattrflags = edp->attrflags;
		edp->savedfgcol = edp->fgcol;
		edp->savedbgcol = edp->bgcol;
		for (i = 0; i < 4; i++)
			edp->savedchartab_G[i] = edp->chartab_G[i];
		edp->savedchartab0 = edp->chartab0;
		edp->savedchartab1 = edp->chartab1;
		break;
	case '8': /* DECRC */
		if ((edp->flags & VTFL_SAVEDCURS) == 0)
			break;
		edp->crow = edp->savedcursor_row;
		edp->ccol = edp->savedcursor_col;
		edp->curattr = edp->savedattr;
		edp->bkgdattr = edp->savedbkgdattr;
		edp->attrflags = edp->savedattrflags;
		edp->fgcol = edp->savedfgcol;
		edp->bgcol = edp->savedbgcol;
		for (i = 0; i < 4; i++)
			edp->chartab_G[i] = edp->savedchartab_G[i];
		edp->chartab0 = edp->savedchartab0;
		edp->chartab1 = edp->savedchartab1;
		break;
	case '=': /* DECKPAM application mode */
		edp->flags |= VTFL_APPLKEYPAD;
		break;
	case '>': /* DECKPNM numeric mode */
		edp->flags &= ~VTFL_APPLKEYPAD;
		break;
	case 'E': /* NEL */
		edp->ccol = 0;
		/* FALLTHROUGH */
	case 'D': /* IND */
		rc = wsemul_vt100_nextline(edp);
		break;
	case 'H': /* HTS */
		if (edp->tabs != NULL)
			edp->tabs[edp->ccol] = 1;
		break;
	case '~': /* LS1R */
		edp->chartab1 = 1;
		break;
	case 'n': /* LS2 */
		edp->chartab0 = 2;
		break;
	case '}': /* LS2R */
		edp->chartab1 = 2;
		break;
	case 'o': /* LS3 */
		edp->chartab0 = 3;
		break;
	case '|': /* LS3R */
		edp->chartab1 = 3;
		break;
	case 'N': /* SS2 */
		edp->sschartab = 2;
		break;
	case 'O': /* SS3 */
		edp->sschartab = 3;
		break;
	case 'M': /* RI */
		if (ROWS_ABOVE > 0) {
			edp->crow--;
			CHECK_DW;
			break;
		}
		rc = wsemul_vt100_scrolldown(edp, 1);
		break;
	case 'P': /* DCS */
		edp->nargs = 0;
		memset(edp->args, 0, sizeof (edp->args));
		newstate = VT100_EMUL_STATE_DCS;
		break;
	case 'c': /* RIS */
		wsemul_vt100_reset(edp);
		rc = wsemul_vt100_ed(edp, 2);
		if (rc != 0)
			break;
		edp->ccol = edp->crow = 0;
		break;
	case '(': case ')': case '*': case '+': /* SCS */
		edp->designating = c - '(';
		newstate = VT100_EMUL_STATE_SCS94;
		break;
	case '-': case '.': case '/': /* SCS */
		edp->designating = c - '-' + 1;
		newstate = VT100_EMUL_STATE_SCS96;
		break;
	case '#':
		newstate = VT100_EMUL_STATE_ESC_HASH;
		break;
	case ' ': /* 7/8 bit */
		newstate = VT100_EMUL_STATE_ESC_SPC;
		break;
	case ']': /* OSC operating system command */
	case '^': /* PM privacy message */
	case '_': /* APC application program command */
		/* ignored */
		newstate = VT100_EMUL_STATE_STRING;
		break;
	case '<': /* exit VT52 mode - ignored */
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c unknown\n", c);
#endif
		break;
	}

	if (COLS_LEFT != 0)
		edp->flags &= ~VTFL_LASTCHAR;

	if (rc != 0)
		return rc;

	edp->state = newstate;
	return 0;
}

int
wsemul_vt100_output_scs94(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_NORMAL;

	switch (c) {
	case '%': /* probably DEC supplemental graphic */
		newstate = VT100_EMUL_STATE_SCS94_PERCENT;
		break;
	case 'A': /* british / national */
		edp->chartab_G[edp->designating] = edp->nrctab;
		break;
	case 'B': /* ASCII */
		edp->chartab_G[edp->designating] = 0;
		break;
	case '<': /* user preferred supplemental */
		/* XXX not really "user" preferred */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	case '0': /* DEC special graphic */
		edp->chartab_G[edp->designating] = edp->decgraphtab;
		break;
	case '>': /* DEC tech */
		edp->chartab_G[edp->designating] = edp->dectechtab;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%c unknown\n", edp->designating + '(', c);
#endif
		break;
	}

	edp->state = newstate;
	return 0;
}

int
wsemul_vt100_output_scs94_percent(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case '5': /* DEC supplemental graphic */
		/* XXX there are differences */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%%%c unknown\n", edp->designating + '(', c);
#endif
		break;
	}

	edp->state = VT100_EMUL_STATE_NORMAL;
	return 0;
}

int
wsemul_vt100_output_scs96(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_NORMAL;
	int nrc;

	switch (c) {
	case '%': /* probably portuguese */
		newstate = VT100_EMUL_STATE_SCS96_PERCENT;
		break;
	case 'A': /* ISO-latin-1 supplemental */
		edp->chartab_G[edp->designating] = edp->isolatin1tab;
		break;
	case '4': /* dutch */
		nrc = 1;
		goto setnrc;
	case '5': case 'C': /* finnish */
		nrc = 2;
		goto setnrc;
	case 'R': /* french */
		nrc = 3;
		goto setnrc;
	case 'Q': /* french canadian */
		nrc = 4;
		goto setnrc;
	case 'K': /* german */
		nrc = 5;
		goto setnrc;
	case 'Y': /* italian */
		nrc = 6;
		goto setnrc;
	case 'E': case '6': /* norwegian / danish */
		nrc = 7;
		goto setnrc;
	case 'Z': /* spanish */
		nrc = 9;
		goto setnrc;
	case '7': case 'H': /* swedish */
		nrc = 10;
		goto setnrc;
	case '=': /* swiss */
		nrc = 11;
setnrc:
		if (vt100_setnrc(edp, nrc) == 0) /* what table ??? */
			break;
		/* else FALLTHROUGH */
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%c unknown\n", edp->designating + '-' - 1, c);
#endif
		break;
	}

	edp->state = newstate;
	return 0;
}

int
wsemul_vt100_output_scs96_percent(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case '6': /* portuguese */
		if (vt100_setnrc(edp, 8) == 0)
			break;
		/* else FALLTHROUGH */
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC%c%%%c unknown\n", edp->designating + '-' - 1, c);
#endif
		break;
	}

	edp->state = VT100_EMUL_STATE_NORMAL;
	return 0;
}

int
wsemul_vt100_output_esc_spc(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case 'F': /* 7-bit controls */
	case 'G': /* 8-bit controls */
#ifdef VT100_PRINTNOTIMPL
		printf("ESC<SPC>%c ignored\n", c);
#endif
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC<SPC>%c unknown\n", c);
#endif
		break;
	}

	edp->state = VT100_EMUL_STATE_NORMAL;
	return 0;
}

int
wsemul_vt100_output_string(struct wsemul_vt100_emuldata *edp, u_char c)
{
	if (edp->dcstype && edp->dcspos < DCS_MAXLEN)
		edp->dcsarg[edp->dcspos++] = c;

	edp->state = VT100_EMUL_STATE_STRING;
	return 0;
}

int
wsemul_vt100_output_string_esc(struct wsemul_vt100_emuldata *edp, u_char c)
{
	if (c == '\\') { /* ST complete */
		wsemul_vt100_handle_dcs(edp);
		edp->state = VT100_EMUL_STATE_NORMAL;
	} else
		edp->state = VT100_EMUL_STATE_STRING;

	return 0;
}

int
wsemul_vt100_output_dcs(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_DCS;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		/* argument digit */
		if (edp->nargs > VT100_EMUL_NARGS - 1)
			break;
		edp->args[edp->nargs] = (edp->args[edp->nargs] * 10) +
		    (c - '0');
		break;
	case ';': /* argument terminator */
		edp->nargs++;
		break;
	default:
		edp->nargs++;
		if (edp->nargs > VT100_EMUL_NARGS) {
#ifdef VT100_DEBUG
			printf("vt100: too many arguments\n");
#endif
			edp->nargs = VT100_EMUL_NARGS;
		}
		newstate = VT100_EMUL_STATE_STRING;
		switch (c) {
		case '$':
			newstate = VT100_EMUL_STATE_DCS_DOLLAR;
			break;
		case '{': /* DECDLD soft charset */	/* } */
		case '!': /* DECRQUPSS user preferred supplemental set */
			/* 'u' must follow - need another state */
		case '|': /* DECUDK program F6..F20 */
#ifdef VT100_PRINTNOTIMPL
			printf("DCS%c ignored\n", c);
#endif
			break;
		default:
#ifdef VT100_PRINTUNKNOWN
			printf("DCS%c (%d, %d) unknown\n", c, ARG(0), ARG(1));
#endif
			break;
		}
	}

	edp->state = newstate;
	return 0;
}

int
wsemul_vt100_output_dcs_dollar(struct wsemul_vt100_emuldata *edp, u_char c)
{
	switch (c) {
	case 'p': /* DECRSTS terminal state restore */
	case 'q': /* DECRQSS control function request */
#ifdef VT100_PRINTNOTIMPL
		printf("DCS$%c ignored\n", c);
#endif
		break;
	case 't': /* DECRSPS restore presentation state */
		switch (ARG(0)) {
		case 0: /* error */
			break;
		case 1: /* cursor information restore */
#ifdef VT100_PRINTNOTIMPL
			printf("DCS1$t ignored\n");
#endif
			break;
		case 2: /* tab stop restore */
			edp->dcspos = 0;
			edp->dcstype = DCSTYPE_TABRESTORE;
			break;
		default:
#ifdef VT100_PRINTUNKNOWN
			printf("DCS%d$t unknown\n", ARG(0));
#endif
			break;
		}
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("DCS$%c (%d, %d) unknown\n", c, ARG(0), ARG(1));
#endif
		break;
	}

	edp->state = VT100_EMUL_STATE_STRING;
	return 0;
}

int
wsemul_vt100_output_esc_hash(struct wsemul_vt100_emuldata *edp, u_char c)
{
	int i;
	int rc = 0;

	switch (c) {
	case '5': /*  DECSWL single width, single height */
		if (edp->dblwid != NULL && edp->dw != 0) {
			for (i = 0; i < edp->ncols / 2; i++) {
				WSEMULOP(rc, edp, &edp->abortstate, copycols,
				    (edp->emulcookie, edp->crow, 2 * i, i, 1));
				if (rc != 0)
					return rc;
			}
			WSEMULOP(rc, edp, &edp->abortstate, erasecols,
			    (edp->emulcookie, edp->crow, i, edp->ncols - i,
			     edp->bkgdattr));
			if (rc != 0)
				return rc;
			edp->dblwid[edp->crow] = 0;
			edp->dw = 0;
		}
		break;
	case '6': /*  DECDWL double width, single height */
	case '3': /*  DECDHL double width, double height, top half */
	case '4': /*  DECDHL double width, double height, bottom half */
		if (edp->dblwid != NULL && edp->dw == 0) {
			for (i = edp->ncols / 2 - 1; i >= 0; i--) {
				WSEMULOP(rc, edp, &edp->abortstate, copycols,
				    (edp->emulcookie, edp->crow, i, 2 * i, 1));
				if (rc != 0)
					return rc;
			}
			for (i = 0; i < edp->ncols / 2; i++) {
				WSEMULOP(rc, edp, &edp->abortstate, erasecols,
				    (edp->emulcookie, edp->crow, 2 * i + 1, 1,
				     edp->bkgdattr));
				if (rc != 0)
					return rc;
			}
			edp->dblwid[edp->crow] = 1;
			edp->dw = 1;
			if (edp->ccol > (edp->ncols >> 1) - 1)
				edp->ccol = (edp->ncols >> 1) - 1;
		}
		break;
	case '8': { /* DECALN */
		int i, j;
		for (i = 0; i < edp->nrows; i++)
			for (j = 0; j < edp->ncols; j++) {
				WSEMULOP(rc, edp, &edp->abortstate, putchar,
				    (edp->emulcookie, i, j, 'E', edp->curattr));
				if (rc != 0)
					return rc;
			}
		}
		edp->ccol = 0;
		edp->crow = 0;
		break;
	default:
#ifdef VT100_PRINTUNKNOWN
		printf("ESC#%c unknown\n", c);
#endif
		break;
	}

	if (COLS_LEFT != 0)
		edp->flags &= ~VTFL_LASTCHAR;

	edp->state = VT100_EMUL_STATE_NORMAL;
	return 0;
}

int
wsemul_vt100_output_csi(struct wsemul_vt100_emuldata *edp, u_char c)
{
	u_int newstate = VT100_EMUL_STATE_CSI;
	int oargs;
	int rc = 0;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		/* argument digit */
		if (edp->nargs > VT100_EMUL_NARGS - 1)
			break;
		edp->args[edp->nargs] = (edp->args[edp->nargs] * 10) +
		    (c - '0');
		break;
	case ';': /* argument terminator */
		edp->nargs++;
		break;
	case '?': /* DEC specific */
	case '>': /* DA query */
		edp->modif1 = c;
		break;
	case '!':
	case '"':
	case '$':
	case '&':
		edp->modif2 = c;
		break;
	default: /* end of escape sequence */
		oargs = edp->nargs++;
		if (edp->nargs > VT100_EMUL_NARGS) {
#ifdef VT100_DEBUG
			printf("vt100: too many arguments\n");
#endif
			edp->nargs = VT100_EMUL_NARGS;
		}
		rc = wsemul_vt100_handle_csi(edp, c);
		if (rc != 0) {
			edp->nargs = oargs;
			return rc;
		}
		newstate = VT100_EMUL_STATE_NORMAL;
		break;
	}

	if (COLS_LEFT != 0)
		edp->flags &= ~VTFL_LASTCHAR;

	edp->state = newstate;
	return 0;
}

u_int
wsemul_vt100_output(void *cookie, const u_char *data, u_int count, int kernel)
{
	struct wsemul_vt100_emuldata *edp = cookie;
	u_int processed = 0;
	u_char c;
#ifdef JUMP_SCROLL
	int lines;
#endif
	int rc = 0;

#ifdef DIAGNOSTIC
	if (kernel && !edp->console)
		panic("wsemul_vt100_output: kernel output, not console");
#endif

	switch (edp->abortstate.state) {
	case ABORT_FAILED_CURSOR:
		/*
		 * If we could not display the cursor back, we pretended not
		 * having been able to display the last character. But this
		 * is a lie, so compensate here.
		 */
		data++, count--;
		processed++;
		wsemul_reset_abortstate(&edp->abortstate);
		break;
	case ABORT_OK:
		/* remove cursor image if visible */
		if (edp->flags & VTFL_CURSORON) {
			rc = (*edp->emulops->cursor)
			    (edp->emulcookie, 0, edp->crow,
			     edp->ccol << edp->dw);
			if (rc != 0)
				return 0;
		}
		break;
	default:
		break;
	}

	for (; count > 0; data++, count--) {
#ifdef JUMP_SCROLL
		switch (edp->abortstate.state) {
		case ABORT_FAILED_JUMP_SCROLL:
			/*
			 * If we failed a previous jump scroll attempt, we
			 * need to try to resume it with the same distance.
			 * We can not recompute it since there might be more
			 * bytes in the tty ring, causing a different result.
			 */
			lines = edp->abortstate.lines;
			break;
		case ABORT_OK:
			/*
			 * If we are at the bottom of the scrolling area, count
			 * newlines until an escape sequence appears.
			 */
			if ((edp->state == VT100_EMUL_STATE_NORMAL || kernel) &&
			    ROWS_BELOW == 0)
				lines = wsemul_vt100_jump_scroll(edp, data,
				    count, kernel);
			else
				lines = 0;
			break;
		default:
			/*
			 * If we are recovering a non-scrolling failure,
			 * do not try to scroll yet.
			 */
			lines = 0;
			break;
		}

		if (lines > 1) {
			wsemul_resume_abort(&edp->abortstate);
			rc = wsemul_vt100_scrollup(edp, lines);
			if (rc != 0) {
				wsemul_abort_jump_scroll(&edp->abortstate,
				    lines);
				return processed;
			}
			wsemul_reset_abortstate(&edp->abortstate);
			edp->crow -= lines;
		}
#endif

		wsemul_resume_abort(&edp->abortstate);

		c = *data;
		if ((c & 0x7f) < 0x20) {
			rc = wsemul_vt100_output_c0c1(edp, c, kernel);
			if (rc != 0)
				break;
			processed++;
			continue;
		}

		if (edp->state == VT100_EMUL_STATE_NORMAL || kernel) {
			rc = wsemul_vt100_output_normal(edp, c, kernel);
			if (rc != 0)
				break;
			processed++;
			continue;
		}
#ifdef DIAGNOSTIC
		if (edp->state > nitems(vt100_output))
			panic("wsemul_vt100: invalid state %d", edp->state);
#endif
		rc = vt100_output[edp->state - 1](edp, c);
		if (rc != 0)
			break;
		processed++;
	}

	if (rc != 0)
		wsemul_abort_other(&edp->abortstate);
	else {
		/* put cursor image back if visible */
		if (edp->flags & VTFL_CURSORON) {
			rc = (*edp->emulops->cursor)
			    (edp->emulcookie, 1, edp->crow,
			     edp->ccol << edp->dw);
			if (rc != 0) {
				/*
				 * Fail the last character output, remembering
				 * that only the cursor operation really needs
				 * to be done.
				 */
				wsemul_abort_cursor(&edp->abortstate);
				processed--;
			}
		}
	}

	if (rc == 0)
		wsemul_reset_abortstate(&edp->abortstate);

	return processed;
}

#ifdef JUMP_SCROLL
int
wsemul_vt100_jump_scroll(struct wsemul_vt100_emuldata *edp, const u_char *data,
    u_int count, int kernel)
{
	u_char curchar;
	u_int pos, lines;

	lines = 0;
	pos = edp->ccol;
	for (; count != 0; data++, count--) {
		curchar = *data;
		/*
		 * Only char causing a transition from
		 * VT100_EMUL_STATE_NORMAL to another state, for now.
		 * Revisit this if this changes...
		 */
		if (curchar == ASCII_ESC)
			break;

		if (ISSET(edp->flags, VTFL_DECAWM))
			switch (curchar) {
			case ASCII_BS:
				if (pos > 0)
					pos--;
				break;
			case ASCII_CR:
				pos = 0;
				break;
			case ASCII_HT:
				if (edp->tabs) {
					pos++;
					while (pos < NCOLS - 1 &&
					    edp->tabs[pos] == 0)
						pos++;
				} else {
					pos = (pos + 7) & ~7;
					if (pos >= NCOLS)
						pos = NCOLS - 1;
				}
				break;
			default:
				if ((curchar & 0x7f) < 0x20)
					break;
				if (pos++ >= NCOLS) {
					pos = 0;
					curchar = ASCII_LF;
				}
				break;
			}

		if (curchar == ASCII_LF ||
		    curchar == ASCII_VT ||
		    curchar == ASCII_FF) {
			if (++lines >= edp->scrreg_nrows - 1)
				break;
		}
	}

	return lines;
}
#endif

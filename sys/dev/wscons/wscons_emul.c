/*	$OpenBSD: wscons_emul.c,v 1.13 1999/01/11 05:12:20 millert Exp $	*/
/*	$NetBSD: wscons_emul.c,v 1.7 1996/11/19 05:23:13 cgd Exp $	*/

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

/*
 * Console emulator for a 'generic' ANSI X3.64 console.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <dev/wscons/wsconsvar.h>
#include <dev/wscons/wscons_emul.h>
#include <dev/wscons/kbd.h>
#include <dev/wscons/ascii.h>

static __inline int wscons_emul_input_normal
    __P((struct wscons_emul_data *, char));
static __inline int wscons_emul_input_haveesc
    __P((struct wscons_emul_data *, char));
static __inline void wscons_emul_docontrol
    __P((struct wscons_emul_data *, char));
static __inline int wscons_emul_input_control
    __P((struct wscons_emul_data *, char));

static int wscons_emul_input_normal __P((struct wscons_emul_data *, char));
static int wscons_emul_input_haveesc __P((struct wscons_emul_data *, char));
static void wscons_emul_docontrol __P((struct wscons_emul_data *, char));
static int wscons_emul_input_control __P((struct wscons_emul_data *, char));

void
wscons_emul_attach(we, wo)
	struct wscons_emul_data *we;
	const struct wscons_odev_spec *wo;
{
	int i;

#ifdef DIAGNOSTIC
	if (we == NULL || wo == NULL)
		panic("wscons_emul_attach: bogus args");
	if (wo->wo_emulfuncs == NULL)
		panic("wscons_emul_attach: bogus emul functions");
#endif
	if (wo->wo_nrows <= 0 || wo->wo_ncols <= 0)
		panic("wscons_emul_attach: bogus size (%d/%d)",
		    wo->wo_nrows, wo->wo_ncols);
	if (wo->wo_crow < 0 || wo->wo_ccol < 0 ||
	    wo->wo_crow >= wo->wo_nrows || wo->wo_ccol >= wo->wo_ncols)
		panic("wscons_emul_attach: bogus location (n: %d/%d, c: %d/%d",
		    wo->wo_nrows, wo->wo_ncols, wo->wo_crow, wo->wo_ccol);

	we->ac_state = ANSICONS_STATE_NORMAL;
	we->ac_ef = wo->wo_emulfuncs;
	we->ac_efa = wo->wo_emulfuncs_cookie;

	we->ac_nrow = wo->wo_nrows;
	we->ac_ncol = wo->wo_ncols;

	we->ac_crow = wo->wo_crow;
	we->ac_ccol = wo->wo_ccol;

	for (i = 0; i < ANSICONS_NARGS; i++)
		we->ac_args[i] = 0;

	(*we->ac_ef->wef_cursor)(we->ac_efa, 1, we->ac_crow, we->ac_ccol);
}

static __inline int
wscons_emul_input_normal(we, c)
	struct wscons_emul_data *we;
	char c;
{
	int newstate = ANSICONS_STATE_NORMAL;
	int n;

	switch (c) {
	case ASCII_BEL:
		wscons_kbd_bell();
		break;

	case ASCII_BS:
		if (we->ac_ccol > 0)
			we->ac_ccol--;
		break;

	case ASCII_HT:
		n = 8 - (we->ac_ccol & 7);
		if (we->ac_ccol + n >= we->ac_ncol)
			n = we->ac_ncol - we->ac_ccol - 1;

		(*we->ac_ef->wef_erasecols)(we->ac_efa, we->ac_crow,
		    we->ac_ccol, we->ac_ccol + n);

		we->ac_ccol += n;
		break;

	case ASCII_LF:
		if (we->ac_crow < we->ac_nrow - 1) {
			we->ac_crow++;
			break;
		}

#ifdef DIAGNOSTIC
		if (we->ac_crow >= we->ac_nrow)
			panic("wscons_emul: didn't scroll (1)");
#endif
		(*we->ac_ef->wef_copyrows)(we->ac_efa, JUMPSCROLL, 0,
		    we->ac_nrow - JUMPSCROLL);
		(*we->ac_ef->wef_eraserows)(we->ac_efa,
		    we->ac_nrow - JUMPSCROLL, JUMPSCROLL);
		we->ac_crow -= JUMPSCROLL - 1;
		break;

	case ASCII_VT:
		if (we->ac_crow > 0)
			we->ac_crow--;
		break;

	case ASCII_NP:
		(*we->ac_ef->wef_eraserows)(we->ac_efa, 0, we->ac_nrow);
		we->ac_ccol = 0;
		we->ac_crow = 0;
		break;

	case ASCII_CR:
		we->ac_ccol = 0;
		break;

	case ASCII_ESC:
		if (we->ac_state == ANSICONS_STATE_NORMAL) {
			newstate = ANSICONS_STATE_HAVEESC;
			break;
		}
		/* else fall through; we're printing one out */

	default:
		if (c == '\0')
			break;
		(*we->ac_ef->wef_putstr)(we->ac_efa,
		    we->ac_crow, we->ac_ccol, &c, 1);
		we->ac_ccol++;

		/* if the current column is still on the current line, done. */
		if (we->ac_ccol < we->ac_ncol)
			break;

		/* wrap the column around. */
		we->ac_ccol = 0;

		/* if the current row isn't the last, increment and leave. */
                if (we->ac_crow < we->ac_nrow - 1) {
                        we->ac_crow++;
                        break;
                }

#ifdef DIAGNOSTIC
		/* check against row overflow */
		if (we->ac_crow >= we->ac_nrow)
			panic("wscons_emul: didn't scroll (2)");
#endif

		(*we->ac_ef->wef_copyrows)(we->ac_efa, JUMPSCROLL, 0,
		    we->ac_nrow - JUMPSCROLL);
		(*we->ac_ef->wef_eraserows)(we->ac_efa,
		    we->ac_nrow - JUMPSCROLL, JUMPSCROLL);
		we->ac_crow -= JUMPSCROLL - 1;
		break;
	}

	return newstate;
}

static __inline int
wscons_emul_input_haveesc(we, c)
	struct wscons_emul_data *we;
	char c;
{
	int newstate = ANSICONS_STATE_NORMAL;
	int i;

	switch (c) {
	case '[':
		for (i = 0; i < ANSICONS_NARGS; i++)
			we->ac_args[i] = 0;
		newstate = ANSICONS_STATE_CONTROL;
		break;

	default:
		wscons_emul_input_normal(we, ASCII_ESC);	/* special cased */
		newstate = wscons_emul_input_normal(we, c);
		break;
	}

	return newstate;
}

static __inline void
wscons_emul_docontrol(we, c)
	struct wscons_emul_data *we;
	char c;
{
	int n, m;

#if 0
	printf("control: %c: %d, %d\n", c, we->ac_args[0], we->ac_args[1]);
#endif
	switch (c) {
	case 'A':	/* Cursor Up */
		n = we->ac_args[0] ? we->ac_args[0] : 1;
		n = min(n, we->ac_crow);
		we->ac_crow -= n;
		break;

	case 'B':	/* Cursor Down */
		n = we->ac_args[0] ? we->ac_args[0] : 1;
		n = min(n, we->ac_nrow - we->ac_crow - 1);
		we->ac_crow += n;
		break;

	case 'C':	/* Cursor Forward */
		n = we->ac_args[0] ? we->ac_args[0] : 1;
		n = min(n, we->ac_ncol - we->ac_ccol - 1);
		we->ac_ccol += n;
		break;

	case 'D':	/* Cursor Backward */
		n = we->ac_args[0] ? we->ac_args[0] : 1;
		n = min(n, we->ac_ccol);
		we->ac_ccol -= n;
		break;

	case 'E':	/* Cursor Next Line */
		n = we->ac_args[0] ? we->ac_args[0] : 1;
		n = min(n, we->ac_nrow - we->ac_crow - 1);
		we->ac_crow += n;
		we->ac_ccol = 0;
		break;

	case 'f':	/* Horizontal and Vertical Position */
	case 'H':	/* Cursor Position */
		m = we->ac_args[1] ? we->ac_args[1] : 1;	/* arg 1 */
		m = min(m, we->ac_nrow);

		n = we->ac_args[0] ? we->ac_args[0] : 1;	/* arg 2 */
		n = min(n, we->ac_ncol);

		we->ac_crow = m - 1;
		we->ac_ccol = n - 1;
		break;

	case 'J':	/* Erase in Display */
		(*we->ac_ef->wef_erasecols)(we->ac_efa, we->ac_crow,
		    we->ac_ccol, we->ac_ncol - we->ac_ccol);
		if (we->ac_crow + 1 < we->ac_nrow)
			(*we->ac_ef->wef_eraserows)(we->ac_efa,
			    we->ac_crow + 1, we->ac_nrow - we->ac_crow - 1);
		break;

	case 'K':	/* Erase in Line */
		(*we->ac_ef->wef_erasecols)(we->ac_efa, we->ac_crow,
		    we->ac_ccol, we->ac_ncol - we->ac_ccol);
		break;

	case 'L':	/* Insert Line */
		{
			int copy_src, copy_dst, copy_nlines;

			n = we->ac_args[0] ? we->ac_args[0] : 1;
			n = min(n, we->ac_nrow - we->ac_crow);

			copy_src = we->ac_crow;
			copy_dst = we->ac_crow + n;
			copy_nlines = we->ac_nrow - copy_dst;
			if (copy_nlines > 0)
				(*we->ac_ef->wef_copyrows)(we->ac_efa,
				    copy_src, copy_dst, copy_nlines);

			(*we->ac_ef->wef_eraserows)(we->ac_efa,
			    we->ac_crow, n);
		}
		break;

	case 'M':	/* Delete Line */
		{
			int copy_src, copy_dst, copy_nlines;

			n = we->ac_args[0] ? we->ac_args[0] : 1;
			n = min(n, we->ac_nrow - we->ac_crow);

			copy_src = we->ac_crow + n;
			copy_dst = we->ac_crow;
			copy_nlines = we->ac_nrow - copy_src;
			if (copy_nlines > 0)
				(*we->ac_ef->wef_copyrows)(we->ac_efa,
				    copy_src, copy_dst, copy_nlines);

			(*we->ac_ef->wef_eraserows)(we->ac_efa,
			    we->ac_crow + copy_nlines,
			    we->ac_nrow - (we->ac_crow + copy_nlines));
		}
		break;

	case 'P':	/* Delete Character */
		{
			int copy_src, copy_dst, copy_ncols;

			n = we->ac_args[0] ? we->ac_args[0] : 1;
			n = min(n, we->ac_ncol - we->ac_ccol);

			copy_src = we->ac_ccol + n;
			copy_dst = we->ac_ccol;
			copy_ncols = we->ac_ncol - copy_src;
			if (copy_ncols > 0)
				(*we->ac_ef->wef_copycols)(we->ac_efa,
				    we->ac_crow, copy_src, copy_dst,
				    copy_ncols);

			(*we->ac_ef->wef_erasecols)(we->ac_efa,
			    we->ac_crow, we->ac_ccol + copy_ncols,
			    we->ac_ncol - (we->ac_ccol + copy_ncols));
			break;
		}
		break;
	case '@':		/* Insert Char */
		{
			int copy_src, copy_dst, copy_ncols;

			n = we->ac_args[0] ? we->ac_args[0] : 1;
			n = min(n, we->ac_ncol - we->ac_ccol);

			copy_src = we->ac_ccol;
			copy_dst = we->ac_ccol + n;
			copy_ncols = we->ac_ncol - copy_dst;

			if (copy_ncols > 0)
				(*we->ac_ef->wef_copycols)(we->ac_efa,
				    we->ac_crow, copy_src, copy_dst,
				    copy_ncols);

			(*we->ac_ef->wef_erasecols)(we->ac_efa,
			    we->ac_crow, we->ac_ccol,
			    copy_dst - we->ac_ccol);
		}
		break;
	case 'm':		/* video attributes */
		/* 7 for so; 0 for se */
		switch (we->ac_args[0]) {
		case 7:
			(we->ac_ef->wef_set_attr)(we->ac_efa, 1);
			break;
		case 0:
			(we->ac_ef->wef_set_attr)(we->ac_efa, 0);
			break;
		}
		break;
	}
}

static __inline int
wscons_emul_input_control(we, c)
	struct wscons_emul_data *we;
	char c;
{
	int newstate = ANSICONS_STATE_CONTROL;
	int i;

	switch (c) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		we->ac_args[0] *= 10;
		we->ac_args[0] += c - '0';
		break;

	case ';':
		for (i = 0; i < ANSICONS_NARGS - 1; i++)
			we->ac_args[i + 1] = we->ac_args[i];
		we->ac_args[0] = 0;
		break;

	default:
		wscons_emul_docontrol(we, c);
		newstate = ANSICONS_STATE_NORMAL;
		break;
	}

	return newstate;
}

void
wscons_emul_input(we, cp, n)
	struct wscons_emul_data *we;
	char *cp;
	int n;
{
	int newstate;

	(*we->ac_ef->wef_cursor)(we->ac_efa, 0, we->ac_crow, we->ac_ccol);
	for (; n; n--, cp++) {
		switch (we->ac_state) {
		case ANSICONS_STATE_NORMAL:
			newstate = wscons_emul_input_normal(we, *cp);
			break;

		case ANSICONS_STATE_HAVEESC:
			newstate = wscons_emul_input_haveesc(we, *cp);
			break;
	
		case ANSICONS_STATE_CONTROL:
			newstate = wscons_emul_input_control(we, *cp);
			break;

		default:
#ifdef DIAGNOSTIC
			panic("wscons_emul: invalid state %d", we->ac_state);
#endif
			/* try to recover, if things get screwed up... */
			newstate = ANSICONS_STATE_NORMAL;
			break;
		}

		we->ac_state = newstate;
	}
	(*we->ac_ef->wef_cursor)(we->ac_efa, 1, we->ac_crow, we->ac_ccol);
}

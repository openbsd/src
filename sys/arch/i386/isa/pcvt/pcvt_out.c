/*	$OpenBSD: pcvt_out.c,v 1.19 2000/03/30 21:02:10 aaron Exp $	*/

/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
 *
 * Copyright (c) 1992, 1993 Brian Dunford-Shore.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by Hellmuth Michaelis,
 *	Brian Dunford-Shore and Joerg Wunsch.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @(#)pcvt_out.c, 3.32, Last Edit-Date: [Tue Oct  3 11:19:49 1995]
 *
 */

/*---------------------------------------------------------------------------*
 *
 *	pcvt_out.c	VT220 Terminal Emulator
 *	---------------------------------------
 *	-hm	------------ Release 3.00 --------------
 *	-hm	integrating NetBSD-current patches
 *	-hm	integrating patch from Thomas Gellekum
 *	-hm	bugfix: clear last line when hpmode 28lines and force 24
 *	-hm	right fkey labels after soft/hard reset
 *	-hm	patch from Joerg for comconsole operation
 *	-hm	patch from Lon Willet to preserve the initial cursor shape
 *	-hm	if FAT_CURSOR is defined, you get the old cursor type back ..
 *	-hm	patch from Lon Willett regarding winsize settings
 *	-hm	applying patch from Joerg fixing Crtat bug, non VGA startup bug
 *	-hm	setting variable color for CGA and MDA/HGC in coldinit
 *	-hm	fixing bug initializing cursor position on startup
 *	-hm	fixing support for EGA boards in vt_coldinit()
 *	-hm	bugfix from Joerg: check for svsp->vs_tty before using it
 *	-hm	patches from Michael for NetBSD-current (Apr/21/95) support
 *	-hm	---------------- Release 3.30 -----------------------
 *	-hm	patch from Thomas Gellekum to support C1 controls
 *	-hm	patch from Frank van der Linden for keyboard state per VT
 *	-hm	---------------- Release 3.32 -----------------------
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#define PCVT_INCLUDE_VT_SELATTR	/* get inline function from pcvt_hdr.h */

#include "pcvt_hdr.h"		/* global include */

static void check_scroll ( struct video_state *svsp );
static __inline void write_char (struct	video_state *, u_short, u_short ch);

static int check_scrollback ( struct video_state *svsp );

/*---------------------------------------------------------------------------*
 *	do character set transformation and write to display memory (inline)
 *---------------------------------------------------------------------------*/

#define video (svsp->Crtat + svsp->cur_offset)

static __inline void write_char (svsp, attrib, ch)
struct	video_state *svsp;
u_short	attrib, ch;
{
	if ((ch >= 0x20) && (ch <= 0x7f)) {	/* use GL if ch >= 0x20 */
		if(!svsp->ss)		/* single shift G2/G3 -> GL ? */
			*video = attrib | svsp->GL[ch-0x20];
		else {
			*video = attrib | svsp->Gs[ch-0x20];
			svsp->ss = 0;
		}
	}
	else {
		if (pcdisp) {
			*video = attrib | ch;
			return;
		}

		svsp->ss = 0;

		if(ch >= 0x80) {		/* display controls C1 */
			if(ch >= 0xA0)		/* use GR if ch >= 0xA0 */
				*video = attrib | svsp->GR[ch-0xA0];
			else {
				if(vgacs[svsp->vga_charset].secondloaded)
					*video = attrib | ((ch-0x60) | CSH);
				else	/* use normal ibm charset  */
					*video = attrib | ch;
			}
		}
		else {				/* display controls C0 */
			if(vgacs[svsp->vga_charset].secondloaded)
				*video = attrib | (ch | CSH);
			else	/* use normal ibm charset for control display*/
				*video = attrib | ch;
		}
	}
}

/*---------------------------------------------------------------------------*
 *	emulator main entry
 *---------------------------------------------------------------------------*/
void
sput (u_char *s, U_char kernel, int len, int page)
{
    register struct video_state *svsp;
    int		extra;
    u_short	attrib;
    u_short	ch;

    if(page >= PCVT_NSCREENS)		/* failsafe */
	page = 0;

    svsp = &vs[page];			/* pointer to current screen state */

    if(do_initialization)		/* first time called ? */
	vt_coldinit();			/*   yes, we have to init ourselves */

    if(svsp == vsp)			/* on current displayed page ?	*/
    {
	cursor_pos_valid = 0;			/* do not update cursor */

#if PCVT_SCREENSAVER
	if(scrnsv_active)			/* screen blanked ?	*/
		pcvt_scrnsv_reset();		/* unblank NOW !	*/
	else
		reset_screen_saver = 1;		/* do it asynchronously	*/
#endif /* PCVT_SCREENSAVER */

    }

    attrib = kernel ? kern_attr : svsp->c_attr;

    while (len-- > 0)
    if ((ch = (*(s++))) != 0)
    {
	if(svsp->sevenbit)
		ch &= 0x7f;

	if((svsp->transparent == 0)
	    && ((ch <= 0x1f)
		|| (svsp->C1_ctls && (ch > 0x7f) && (ch < 0xa0))))
	{

	/* always process control-chars in the range 0x00..0x1f !!! */

	/* also process the C1 control chars a VT220 recognizes and
	 * ignore the others.
	 */
		switch(ch) {
		case 0x00:	/* NUL */
		case 0x01:	/* SOH */
		case 0x02:	/* STX */
		case 0x03:	/* ETX */
		case 0x04:	/* EOT */
		case 0x05:	/* ENQ */
		case 0x06:	/* ACK */
			break;

		case 0x07:	/* BEL */
			if(svsp->bell_on)
				sysbeep(PCVT_SYSBEEPF/1500, hz/4);
			break;

		case 0x08:	/* BS */
			if(svsp->col > 0) {
				svsp->cur_offset--;
				svsp->col--;
			}
			break;

		case 0x09:	/* TAB */
			while (svsp->col < ((svsp->maxcol)-1)) {
				svsp->cur_offset++;
				if(svsp->tab_stops[++svsp->col])
					break;
			}
			break;

		case 0x0a:	/* LF */
		case 0x0b:	/* VT */
		case 0x0c:	/* FF */
			if (check_scrollback(svsp)) {
				extra = (svsp->cur_offset % svsp->maxcol) ?
				    svsp->col : 0;
				bcopy(svsp->Crtat + svsp->cur_offset - extra,
				    svsp->Scrollback + (svsp->scr_offset *
				    svsp->maxcol), svsp->maxcol * CHR);
			}
			if(svsp->lnm) {
				svsp->cur_offset -= svsp->col;
				svsp->cur_offset += svsp->maxcol;
				svsp->col = 0;
			}
			else {
				svsp->cur_offset += svsp->maxcol;
			}
			check_scroll(svsp);
			break;

		case 0x0d:	/* CR */
			svsp->cur_offset -= svsp->col;
			svsp->col = 0;
			break;

		case 0x0e:	/* SO */
			svsp->GL = svsp->G1;
			break;

		case 0x0f:	/* SI */
			svsp->GL = svsp->G0;
			break;

		case 0x10:	/* DLE */
		case 0x11:	/* DC1/XON */
		case 0x12:	/* DC2 */
		case 0x13:	/* DC3/XOFF */
		case 0x14:	/* DC4 */
		case 0x15:	/* NAK */
		case 0x16:	/* SYN */
		case 0x17:	/* ETB */
			break;

		case 0x18:	/* CAN */
			svsp->state = STATE_INIT;
			clr_parms(svsp);
			break;

		case 0x19:	/* EM */
			break;

		case 0x1a:	/* SUB */
			svsp->state = STATE_INIT;
			clr_parms(svsp);
			break;

		case 0x1b:	/* ESC */
			svsp->state = STATE_ESC;
			clr_parms(svsp);
			break;

		case 0x1c:	/* FS */
		case 0x1d:	/* GS */
		case 0x1e:	/* RS */
		case 0x1f:	/* US */
			break;

		case 0x80:	/* */
		case 0x81:	/* */
		case 0x82:	/* */
		case 0x83:	/* */
			break;

		case 0x84:      /* IND */
			vt_ind(svsp);
			break;

		case 0x85:      /* NEL */
			vt_nel(svsp);
			break;

		case 0x86:      /* SSA */
		case 0x87:      /* ESA */
			break;

		case 0x88:      /* HTS */
			svsp->tab_stops[svsp->col] = 1;
			break;

		case 0x89:      /* HTJ */
		case 0x8a:      /* VTS */
		case 0x8b:      /* PLD */
		case 0x8c:      /* PLU */
			break;

		case 0x8d:      /* RI */
			vt_ri(svsp);
			break;

		case 0x8e:      /* SS2 */
			svsp->Gs = svsp->G2;
			svsp->ss = 1;
			break;

		case 0x8f:      /* SS3 */
			svsp->Gs = svsp->G3;
			svsp->ss = 1;
			break;

		case 0x90:      /* DCS */
			svsp->dcs_state = DCS_INIT;
			svsp->state = STATE_DCS;
			break;

		case 0x91:      /* PU1 */
		case 0x92:      /* PU2 */
		case 0x93:      /* STS */
		case 0x94:      /* CCH */
		case 0x95:      /* MW */
		case 0x96:      /* SPA */
		case 0x97:      /* EPA */
		case 0x98:      /* */
		case 0x99:      /* */
		case 0x9a:      /* */
			break;

		case 0x9b:      /* CSI */
			clr_parms(svsp);
			svsp->state = STATE_CSI;
			break;

		case 0x9c:      /* ST */
			svsp->state = STATE_INIT;
			break;

		case 0x9d:      /* OSC */
		case 0x9e:      /* PM */
		case 0x9f:      /* APC */
			/* only in VT320's */
			break;
		}
	}
	else
	{

	/* char range 0x20...0xff processing depends on current state */

		if (svsp->lastchar && svsp->m_awm && svsp->lastrow == svsp->row)
		{
			svsp->cur_offset++;
			svsp->col = 0;
			svsp->lastchar = 0;
			if (check_scrollback(svsp)) {
				bcopy(svsp->Crtat + svsp->cur_offset -
			      	      svsp->maxcol, svsp->Scrollback +
			      	      (svsp->scr_offset * svsp->maxcol),
		      	      	      svsp->maxcol * CHR);
			}
			check_scroll(svsp);
		}

		switch(svsp->state) {
		case STATE_INIT:
			if(svsp->irm)
				bcopy((svsp->Crtat + svsp->cur_offset),
				    (svsp->Crtat + svsp->cur_offset) + 1,
				    (((svsp->maxcol)-1) - svsp->col * CHR));

			write_char(svsp, attrib, ch);
			vt_selattr(svsp);

			if(svsp->col >= ((svsp->maxcol)-1)) {
				svsp->lastchar = 1;
				svsp->lastrow = svsp->row;
			}
			else {
				svsp->lastchar = 0;
				svsp->cur_offset++;
				svsp->col++;
			}
			break;

		case STATE_ESC:
			switch(ch) {
			case ' ':	/* ESC sp family */
				svsp->state = STATE_BLANK;
				break;

			case '#':	/* ESC # family */
				svsp->state = STATE_HASH;
				break;

			case '(':	/* ESC ( family */
				svsp->state = STATE_BROPN;
				break;

			case ')':	/* ESC ) family */
				svsp->state = STATE_BRCLO;
				break;

			case '*':	/* ESC * family */
				svsp->state = STATE_STAR;
				break;

			case '+':	/* ESC + family */
				svsp->state = STATE_PLUS;
				break;

			case '-':	/* ESC - family */
				svsp->state = STATE_MINUS;
				break;

			case '.':	/* ESC . family */
				svsp->state = STATE_DOT;
				break;

			case '/':	/* ESC / family */
				svsp->state = STATE_SLASH;
				break;

			case '7':	/* SAVE CURSOR */
				vt_sc(svsp);
				svsp->state = STATE_INIT;
				break;

			case '8':	/* RESTORE CURSOR */
				vt_rc(svsp);
				if (!kernel)
					attrib = svsp->c_attr;
				svsp->state = STATE_INIT;
				break;

			case '=': /* keypad application mode */
#if !PCVT_INHIBIT_NUMLOCK
				vt_keyappl(svsp);
#endif
				svsp->state = STATE_INIT;
				break;

			case '>': /* keypad numeric mode */
#if !PCVT_INHIBIT_NUMLOCK
				vt_keynum(svsp);
#endif
				svsp->state = STATE_INIT;
				break;

			case 'D':	/* INDEX */
				vt_ind(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'E':	/* NEXT LINE */
				vt_nel(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'H': /* set TAB at current col */
				svsp->tab_stops[svsp->col] = 1;
				svsp->state = STATE_INIT;
				break;

			case 'M':	/* REVERSE INDEX */
				vt_ri(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'N':	/* SINGLE SHIFT G2 */
				svsp->Gs = svsp->G2;
				svsp->ss = 1;
				svsp->state = STATE_INIT;
				break;

			case 'O':	/* SINGLE SHIFT G3 */
				svsp->Gs = svsp->G3;
				svsp->ss = 1;
				svsp->state = STATE_INIT;
				break;

			case 'P':	/* DCS detected */
				svsp->dcs_state = DCS_INIT;
				svsp->state = STATE_DCS;
				break;

			case 'Z': /* What are you = ESC [ c */
				vt_da(svsp);
				svsp->state = STATE_INIT;
				break;

			case '[':	/* CSI detected */
				clr_parms(svsp);
				svsp->state = STATE_CSI;
				break;

			case '\\':	/* String Terminator */
				svsp->state = STATE_INIT;
				break;

			case 'c':	/* hard reset */
				vt_ris(svsp);
				if (!kernel)
					attrib = svsp->c_attr;
				svsp->state = STATE_INIT;
				break;
#if PCVT_SETCOLOR
			case 'd':	/* set color sgr */
				if (color) {
					/* set shiftwidth=4 */
					sgr_tab_color[svsp->vtsgr] =
					    svsp->c_attr >> 8;
					user_attr = sgr_tab_color[0] << 8;
				}
				svsp->state = STATE_INIT;
				break;
#endif /* PCVT_SETCOLOR */
			case 'n': /* Lock Shift G2 -> GL */
				svsp->GL = svsp->G2;
				svsp->state = STATE_INIT;
				break;

			case 'o': /* Lock Shift G3 -> GL */
				svsp->GL = svsp->G3;
				svsp->state = STATE_INIT;
				break;

			case '}': /* Lock Shift G2 -> GR */
				svsp->GR = svsp->G2;
				svsp->state = STATE_INIT;
				break;

			case '|': /* Lock Shift G3 -> GR */
				svsp->GR = svsp->G3;
				svsp->state = STATE_INIT;
				break;

			case '~': /* Lock Shift G1 -> GR */
				svsp->GR = svsp->G1;
				svsp->state = STATE_INIT;
				break;

			default:
				svsp->state = STATE_INIT;
				break;
			}
			break;

		case STATE_BLANK:	/* ESC space [FG], which are */
			svsp->state = STATE_INIT; /* currently ignored*/
			break;

		case STATE_HASH:
			switch(ch) {
			case '3': /* double height top half */
			case '4': /* double height bottom half */
			case '5': /* single width sngle height */
			case '6': /* double width sngle height */
				svsp->state = STATE_INIT;
				break;

			case '8': /* fill sceen with 'E's */
				vt_aln(svsp);
				svsp->state = STATE_INIT;
				break;

			default: /* anything else */
				svsp->state = STATE_INIT;
				break;
			}
			break;

		case STATE_BROPN:	/* designate G0 */
		case STATE_BRCLO:	/* designate G1 */
		case STATE_STAR:	/* designate G2 */
		case STATE_PLUS:	/* designate G3 */
		case STATE_MINUS:	/* designate G1 (96) */
		case STATE_DOT:		/* designate G2 (96) */
		case STATE_SLASH:	/* designate G3 (96) */
			svsp->which[svsp->whichi++] = ch;
			if (ch >= 0x20 && ch <= 0x2f && svsp->whichi <= 2) {
				break;
			} else if (ch >=0x30 && ch <= 0x7e) {
				 svsp->which[svsp->whichi] = '\0';
				 vt_designate(svsp);
			}
			svsp->whichi = 0;
			svsp->state = STATE_INIT;
			break;

		case STATE_CSIQM:	/* DEC private modes */
			switch(ch) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':	/* parameters */
				svsp->parms[svsp->parmi] *= 10;
				svsp->parms[svsp->parmi] += (ch - '0');
				break;

			case ';':	/* next parameter */
				svsp->parmi = (svsp->parmi+1 < MAXPARMS) ?
				    svsp->parmi+1 : svsp->parmi;
				break;

			case 'h':	/* set mode */
				vt_set_dec_priv_qm(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'l':	/* reset mode */
				vt_reset_dec_priv_qm(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'n':	/* Reports */
				vt_dsr(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'K': /* selective erase in line */
				vt_sel(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'J':/*selective erase in display*/
				vt_sed(svsp);
				svsp->state = STATE_INIT;
				break;

			default:
				svsp->state = STATE_INIT;
				break;
			}
			break;

		case STATE_CSI:
			switch(ch) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':	/* parameters */
				svsp->parms[svsp->parmi] *= 10;
				svsp->parms[svsp->parmi] += (ch - '0');
				break;

			case ';':	/* next parameter */
				svsp->parmi = (svsp->parmi+1 < MAXPARMS) ?
				    svsp->parmi+1 : svsp->parmi;
				break;

			case '?':	/* ESC [ ? family */
				svsp->state = STATE_CSIQM;
				break;

			case '@':	/* insert char */
				vt_ic(svsp);
				svsp->state = STATE_INIT;
				break;

			case '"':  /* select char attribute */
				svsp->state = STATE_DQUOTE;
				break;

			case '\'': /* for DECELR/DECSLE */
/* XXX */			/* another state needed -hm */
				break;

			case '!': /* soft terminal reset */
				svsp->state = STATE_STR;
				break;

			case 'A':	/* cursor up */
				vt_cuu(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'B':	/* cursor down */
				vt_cud(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'C':	/* cursor forward */
				vt_cuf(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'D':	/* cursor backward */
				vt_cub(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'H': /* direct cursor addressing*/
				vt_curadr(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'J':	/* erase screen */
				vt_clreos(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'K':	/* erase line */
				vt_clreol(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'L':	/* insert line */
				vt_il(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'M':	/* delete line */
				vt_dl(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'P':	/* delete character */
				vt_dch(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'S':	/* scroll up */
				vt_su(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'T':	/* scroll down */
				vt_sd(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'X':	/* erase character */
				vt_ech(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'c':	/* device attributes */
				vt_da(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'f': /* direct cursor addressing*/
				vt_curadr(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'g':	/* clear tabs */
				vt_clrtab(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'h':	/* set mode(s) */
				vt_set_ansi(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'i':	/* media copy */
				vt_mc(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'l':	/* reset mode(s) */
				vt_reset_ansi(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'm': /* select graphic rendition*/
				vt_sgr(svsp);
				if (!kernel)
					attrib = svsp->c_attr;
				svsp->state = STATE_INIT;
				break;

			case 'n':	/* reports */
				vt_dsr(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'r': /* set scrolling region */
				vt_stbm(svsp);
				svsp->state = STATE_INIT;
				break;

			case 's': /* save cursor position */
				vt_sc(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'u': /* restore cursor position */
				vt_rc(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'x': /*request/report parameters*/
				vt_reqtparm(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'y': /* invoke selftest(s) */
				vt_tst(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'z': /* DECELR, ignored */
			case '{': /* DECSLE, ignored */
				svsp->state = STATE_INIT;
				break;

			default:
				svsp->state = STATE_INIT;
				break;
			}
			break;

		case STATE_DCS:
			vt_dcsentry(ch,svsp);
			break;

		case STATE_DQUOTE:
			switch(ch) {
			case 'p': /* compatibility level */
				vt_scl(svsp);
				svsp->state = STATE_INIT;
				break;

			case 'q': /* select char attributes */
				vt_sca(svsp);
				svsp->state = STATE_INIT;
				break;

			default:
				svsp->state = STATE_INIT;
				break;
			}
			break;

		case STATE_STR:
			switch(ch) {
			case 'p': /* soft terminal reset */
				vt_str(svsp);
				if (!kernel)
					attrib = svsp->c_attr;
				svsp->state = STATE_INIT;
				break;

			default:
				svsp->state = STATE_INIT;
				break;
			}
			break;

		default:		/* failsafe */
			svsp->state = STATE_INIT;
			break;
		}
	}

	svsp->row = svsp->cur_offset / svsp->maxcol;	/* current row update */

	/* take care of last character on line behaviour */
	if (svsp->lastchar && (svsp->col < ((svsp->maxcol)-1)))
		svsp->lastchar = 0;
    }

    if (svsp == vsp)			/* on current displayed page ?	*/
	cursor_pos_valid = 1;		/* position is valid now */
}

/*---------------------------------------------------------------------------*
 *	this is the absolute cold initialization of the emulator
 *---------------------------------------------------------------------------*/
void
vt_coldinit(void)
{
	extern u_short csd_ascii[];		/* pcvt_tbl.h */
	extern u_short csd_supplemental[];

	u_short was;
	int nscr, charset;
	int equipment;
	struct video_state *svsp;

	do_initialization = 0;		/* reset init necessary flag */
	pcdisp = 1;			/* turn on traditional pc colors/font */

	/* get the equipment byte from the RTC chip */

	equipment = mc146818_read(NULL, NVRAM_EQUIPMENT);
	switch(equipment & NVRAM_EQUIPMENT_MONITOR)
	{
		default:
			panic("vt_coldinit: impossible equipment");

  		case EQ_EGAVGA:
			/* set memory start to CGA == B8000 */

			Crtat = ISA_HOLE_VADDR(CGA_BUF);

			/* find out, what monitor is connected */

			was = *Crtat;
			*Crtat = (u_short) 0xA55A;
			if (*Crtat != 0xA55A)
			{
				Crtat = ISA_HOLE_VADDR(MONO_BUF);
				addr_6845 = MONO_BASE;
				color = 0;
			}
			else
			{
				*Crtat = was;
				addr_6845 = CGA_BASE;
				color = 1;
			}

			if(vga_test())		/* EGA or VGA ? */
			{
				adaptor_type = VGA_ADAPTOR;
				totalfonts = 8;

				if(color == 0)
				{
					mda2egaorvga();
				}

				/* find out which chipset we are running on */
				vga_type = vga_chipset();
			}
			else
			{
				adaptor_type = EGA_ADAPTOR;
				totalfonts = 4;

				if(color == 0)
				{
					mda2egaorvga();
				}
			}

			/* decouple ega/vga charsets and intensity */
			set_2ndcharset();

			break;

		case EQ_40COLOR:	/* XXX should panic in 40 col mode ! */
		case EQ_80COLOR:

			Crtat = ISA_HOLE_VADDR (CGA_BUF);

			addr_6845 = CGA_BASE;
			adaptor_type = CGA_ADAPTOR;
			color = 1;
			totalfonts = 0;
			break;

		case EQ_80MONO:

			Crtat = ISA_HOLE_VADDR (MONO_BUF);

			addr_6845 = MONO_BASE;
			adaptor_type = MDA_ADAPTOR;
			color = 0;
			totalfonts = 0;
			break;
	}

	/* establish default colors */

	if(color)
	{
		kern_attr = (COLOR_KERNEL_FG | COLOR_KERNEL_BG) << 8;
		user_attr = sgr_tab_color[0] << 8;
	}
	else
	{
		kern_attr = (MONO_KERNEL_FG | MONO_KERNEL_BG) << 8;
		if(adaptor_type == MDA_ADAPTOR)
			user_attr = sgr_tab_imono[0] << 8;
		else
			user_attr = sgr_tab_mono[0] << 8;
	}

	totalscreens = 1;	/* for now until malloced */

	for(nscr = 0, svsp = vs; nscr < PCVT_NSCREENS; nscr++, svsp++)
	{
		svsp->Crtat = Crtat;		/* all same until malloc'ed */
		svsp->Memory = Crtat;		/* until malloc'ed */
		svsp->Scrollback = 0;		/* until malloc'ed */
		svsp->scr_offset = -1;		/* scrollback offset (lines) */
		svsp->scrolling = 0;		/* current scrollback page */
		svsp->cur_offset = 0;		/* cursor offset */
		svsp->c_attr = user_attr;	/* non-kernel attributes */
		svsp->bell_on = 1;		/* enable bell */
		svsp->sevenbit = 0;		/* set to 8-bit path */
		svsp->transparent = 0;		/* disable internal tranparency */
		svsp->C1_ctls = 0;	    	/* process only C0 ctls */
		svsp->lastchar = 0;		/* VTxxx behaviour of last */
						/*            char on line */
		svsp->report_chars = NULL;	/* VTxxx reports init */
		svsp->report_count = 0;		/* VTxxx reports init */
		svsp->state = STATE_INIT;	/* main state machine init */
		svsp->m_awm = 1;		/* enable auto wrap mode */
		svsp->m_om = 0;			/* origin mode = absolute */
		svsp->sc_flag = 0;		/* init saved cursor flag */
		svsp->vga_charset = CH_SET0;	/* use bios default charset */

#if PCVT_24LINESDEF				/* true compatibility */
		svsp->screen_rows = 24;		/* default 24 rows on screen */
#else						/* full screen */
		svsp->screen_rows = 25;		/* default 25 rows on screen */
#endif /* PCVT_24LINESDEF */

		svsp->screen_rowsize = 25;	/* default 25 rows on screen */
		svsp->max_off =  svsp->screen_rowsize * SCROLLBACK_PAGES - 1;
		svsp->scrr_beg = 0;		/* scrolling region begin row*/
		svsp->scrr_len = svsp->screen_rows; /* scrolling region length*/
		svsp->scrr_end = svsp->scrr_len - 1;/* scrolling region end */

		if(nscr == 0)
		{
			if(adaptor_type == VGA_ADAPTOR)
			{
				/* only VGA can read cursor shape registers ! */
				/* Preserve initial cursor shape */
				outb(addr_6845,CRTC_CURSTART);
				svsp->cursor_start = inb(addr_6845+1);
				outb(addr_6845,CRTC_CUREND);
				svsp->cursor_end = inb(addr_6845+1);
			}
			else
			{
				/* MDA,HGC,CGA,EGA registers are write-only */
				svsp->cursor_start = 0;
				svsp->cursor_end = 15;
			}
		}
		else
		{
			svsp->cursor_start = vs[0].cursor_start;
			svsp->cursor_end = vs[0].cursor_end;
		}

#ifdef FAT_CURSOR
		svsp->cursor_end = 15;		/* cursor lower scanline */
#endif

		svsp->cursor_on = 1;		/* cursor is on */
		svsp->ckm = 1;			/* normal cursor key mode */
		svsp->irm = 0;			/* replace mode */
		svsp->lnm = 0;			/* CR only */
		svsp->selchar = 0;		/* selective attribute off */
		svsp->G0 = csd_ascii;		/* G0 = ascii	*/
		svsp->G1 = csd_ascii;		/* G1 = ascii	*/
		svsp->G2 = csd_supplemental;	/* G2 = supplemental */
		svsp->G3 = csd_supplemental;	/* G3 = supplemental */
		svsp->GL = svsp->G0;		/* GL = G0 */
		svsp->GR = svsp->G2;		/* GR = G2 */
		svsp->whichi = 0;		/* char set designate init */
		svsp->which[0] = '\0';		/* char set designate init */
		svsp->ss  = 0;			/* init single shift 2/3 */
		svsp->Gs  = NULL;		/* Gs single shift 2/3 */
		svsp->maxcol = SCR_COL80;	/* 80 columns now (MUST!!!) */
		svsp->wd132col = 0;		/* help good old WD .. */
		svsp->scroll_lock = 0;		/* scrollock off */

#if PCVT_INHIBIT_NUMLOCK
		svsp->num_lock = 0; 		/* numlock off */
#else
		svsp->num_lock = 1;		/* numlock on */
#endif

		svsp->caps_lock = 0;		/* capslock off */
		svsp->shift_lock = 0;		/* shiftlock off */

#if PCVT_24LINESDEF				/* true compatibility */
		svsp->force24 = 1;		/* force 24 lines */
#else						/* maximum screen size */
		svsp->force24 = 0;		/* no 24 lines force yet */
#endif /* PCVT_24LINESDEF */

		vt_clearudk(svsp);		/* clear vt220 udk's */

		vt_str(svsp);			/* init emulator */

		if(nscr == 0)
		{
			/*
			 * Preserve data on the startup screen that
			 * precedes the cursor position.  Leave the
			 * cursor where it was found.
			 */
			unsigned cursorat;
			int filllen;

			/* CRTC regs 0x0e and 0x0f are r/w everywhere */

			outb(addr_6845, CRTC_CURSORH);
			cursorat = inb(addr_6845+1) << 8;
			outb(addr_6845, CRTC_CURSORL);
			cursorat |= inb(addr_6845+1);

			svsp->cur_offset = cursorat;
			svsp->row = cursorat / svsp->maxcol;
			svsp->col = cursorat % svsp->maxcol;

			if (svsp->row >= svsp->screen_rows)
			{

			/*
			 * Scroll up; this should only happen when
			 * PCVT_24LINESDEF is set
			 */
				int nscroll =
					svsp->row + 1
					- svsp->screen_rows;
				bcopy (svsp->Crtat
				       + nscroll*svsp->maxcol,
				       svsp->Crtat,
				       svsp->screen_rows
				       * svsp->maxcol * CHR);
				svsp->row -= nscroll;
				svsp->cur_offset -=
					nscroll * svsp->maxcol;
			}

			filllen = (svsp->maxcol * svsp->screen_rowsize)
				- svsp->cur_offset;

			if (filllen > 0)
				fillw(user_attr | ' ',
				      (caddr_t)(svsp->Crtat+svsp->cur_offset),
				      filllen);
		}
		svsp->smode.mode = VT_AUTO;
		svsp->smode.relsig = svsp->smode.acqsig =
			svsp->smode.frsig = 0;
		svsp->proc = 0;
		svsp->kbd_state = K_XLATE;
		svsp->pid = svsp->vt_status = 0;
	}

 	for(charset = 0;charset < NVGAFONTS;charset++)
	{
		vgacs[charset].loaded = 0;		/* not populated yet */
		vgacs[charset].secondloaded = 0;	/* not populated yet */

		switch(adaptor_type)
		{
			case VGA_ADAPTOR:

				/*
				 * for a VGA, do not assume any
				 * constant - instead, read the actual
				 * values. This avoid problems with
				 * LCD displays that apparently happen
				 * to use font matrices up to 19
				 * scan lines and 475 scan lines
				 * total in order to make use of the
				 * whole screen area
				 */

				outb(addr_6845, CRTC_VDE);
				vgacs[charset].scr_scanlines =
					inb(addr_6845 + 1);
				outb(addr_6845, CRTC_MAXROW);
				vgacs[charset].char_scanlines =
					inb(addr_6845 + 1);
				break;

			case EGA_ADAPTOR:
				/* 0x5D for 25 lines */
				vgacs[charset].scr_scanlines = 0x5D;
				/* 0x4D for 25 lines */
				vgacs[charset].char_scanlines = 0x4D;
				break;

			case CGA_ADAPTOR:
			case MDA_ADAPTOR:
			default:
				/* These shouldn't be used for CGA/MDA */
				vgacs[charset].scr_scanlines = 0;
				vgacs[charset].char_scanlines = 0;
				break;
		}
		vgacs[charset].screen_size = SIZ_25ROWS; /* set screen size */
 	}

 	vgacs[0].loaded = 1; /* The BIOS loaded this at boot */

	/* set cursor for first screen */

	outb(addr_6845,CRTC_CURSTART);	/* cursor start reg */
	outb(addr_6845+1,vs[0].cursor_start);
	outb(addr_6845,CRTC_CUREND);	/* cursor end reg */
	outb(addr_6845+1,vs[0].cursor_end);

	/* this is to satisfy ddb */

	if(!keyboard_is_initialized)
		kbd_code_init1();

	/* update keyboard led's */

	update_led();
}

/*---------------------------------------------------------------------------*
 *	get kernel memory for virtual screens
 *
 *	CAUTION: depends on "can_do_132col" being set properly, or
 *	depends on vga_type() being run before calling this !!!
 *
 *---------------------------------------------------------------------------*/
void
vt_coldmalloc(void)
{
	int nscr;
	int screen_max_size;

	/* we need to initialize in case we are not the console */

	if(do_initialization)
		vt_coldinit();

	switch(adaptor_type)
	{
		default:
		case MDA_ADAPTOR:
		case CGA_ADAPTOR:
			screen_max_size = MAXROW_MDACGA * MAXCOL_MDACGA * CHR;
			break;

		case EGA_ADAPTOR:
			screen_max_size = MAXROW_EGA * MAXCOL_EGA * CHR;
			break;

		case VGA_ADAPTOR:
			if(can_do_132col)
				screen_max_size =
					MAXROW_VGA * MAXCOL_SVGA * CHR;
			else
				screen_max_size =
					MAXROW_VGA * MAXCOL_VGA * CHR;
	}

	scrollback_pages = SCROLLBACK_PAGES;
	if ((Scrollbuffer = (u_short *)malloc(vs[0].maxcol *
	     vs[0].screen_rows * scrollback_pages * CHR, M_DEVBUF,
	     M_WAITOK)) == NULL)
	{
		printf("pcvt: scrollback memory malloc failed\n");
	}

	for(nscr = 0; nscr < PCVT_NSCREENS; nscr++)
	{
		if((vs[nscr].Memory =
		    (u_short *)malloc(screen_max_size * 2, M_DEVBUF, M_WAITOK))
		   == NULL)
		{
			printf("pcvt: screen memory malloc failed, "
			       "NSCREEN=%d, nscr=%d\n",
			       PCVT_NSCREENS, nscr);
			break;
		}
		vs[nscr].Scrollback = Scrollbuffer;
		if(nscr != 0)
		{
			vs[nscr].Crtat = vs[nscr].Memory;
			fillw(user_attr | ' ',
				(caddr_t)(vs[nscr].Crtat),
				vs[nscr].maxcol * vs[nscr].screen_rowsize);
			totalscreens++;
		}
	}
}

/*---------------------------------------------------------------------------*
 *	check if we must scroll up screen
 *---------------------------------------------------------------------------*/
static void
check_scroll(struct video_state *svsp)
{
	if(!svsp->abs_write)
	{
		/* we write within scroll region */

		if(svsp->cur_offset >= ((svsp->scrr_end + 1) * svsp->maxcol))
		{
			/* the following piece of code has to be protected */
			/* from trying to switch to another virtual screen */
			/* while being in there ...                        */

			critical_scroll = 1;		/* flag protect ON */

			roll_up(svsp, 1);		/* rolling up .. */

			svsp->cur_offset -= svsp->maxcol;/* update position */

			if(switch_page != -1)	/* someone wanted to switch ? */
			{
				vgapage(switch_page);	/* yes, then switch ! */
				switch_page = -1;	/* reset switch flag  */
			}

			critical_scroll = 0;		/* flag protect OFF */
	  	}
	}
	else
	{
		/* clip, if outside of screen */

		if (svsp->cur_offset >= svsp->screen_rows * svsp->maxcol)
			svsp->cur_offset -= svsp->maxcol;
	}
}

static int
check_scrollback(struct video_state *svsp)
{
	/* still waiting for scrollback memory or not on current page */
	if (!svsp->Scrollback || svsp != vsp)
		return 0;

	/* remove first line of scrollback buffer to make room for new line */
	if (svsp->scr_offset == svsp->max_off)
	{
		bcopy(svsp->Scrollback + svsp->maxcol, svsp->Scrollback,
		      svsp->maxcol * svsp->max_off * CHR);
	}
	else
	{
		/* still room left, increase scroll offset (lines) */
		svsp->scr_offset++;
	}
	return 1;
}

/*---------------------------------------------------------------------------*
 *	toggle vga 80/132 column operation
 *---------------------------------------------------------------------------*/
int
vt_col(struct video_state *svsp, int cols)
{
	if(vga_col(svsp, cols) == 0)
		return(0);

	fillw(user_attr | ' ',
		(caddr_t)(svsp->Crtat),
		svsp->maxcol * svsp->screen_rowsize);

	clr_parms(svsp);		/* escape parameter init */
	svsp->state = STATE_INIT;	/* initial state */
	svsp->col = 0;			/* init col */
	svsp->row = 0;			/* init row */
	svsp->cur_offset = 0;		/* cursor offset init */
	svsp->sc_flag = 0;		/* invalidate saved cursor position */
	svsp->scrr_beg = 0;		/* reset scrolling region */
	svsp->scrr_len = svsp->screen_rows; /*reset scrolling region legnth */
	svsp->scrr_end = svsp->scrr_len - 1;
	svsp->transparent = 0;		/* disable control code processing */
	svsp->selchar = 0;		/* selective attr off */
	vt_initsel(svsp);		/* re-init sel attr */

	/* Update winsize struct to reflect screen size */

	if(svsp->vs_tty)
	{
		svsp->vs_tty->t_winsize.ws_row = svsp->screen_rows;
		svsp->vs_tty->t_winsize.ws_col = svsp->maxcol;
		svsp->vs_tty->t_winsize.ws_xpixel =
			(cols == SCR_COL80)? 720: 1056;
		svsp->vs_tty->t_winsize.ws_ypixel = 400;

#if PCVT_SIGWINCH
		if(svsp->vs_tty->t_pgrp)
			pgsignal(svsp->vs_tty->t_pgrp, SIGWINCH, 1);
#endif /* PCVT_SIGWINCH */

	}
	reallocate_scrollbuffer(svsp, scrollback_pages);
	return(1);
}

/*---------------------------------------------------------------------------*
 *	initialize ANSI escape sequence parameter buffers
 *---------------------------------------------------------------------------*/
void
clr_parms(struct video_state *svsp)
{
	register int i;
	for(i=0; i < MAXPARMS; i++)
		svsp->parms[i] = 0;
	svsp->parmi = 0;
}

#endif	/* NVT > 0 */

/* ------------------------- E O F ------------------------------------------*/


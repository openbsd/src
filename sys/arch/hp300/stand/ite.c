/*	$NetBSD: ite.c,v 1.11 1996/03/03 04:23:33 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: ite.c 1.24 93/06/25$
 *
 *	@(#)ite.c	8.1 (Berkeley) 7/8/93
 */

/*
 * Standalone Internal Terminal Emulator (CRT and keyboard)
 */

#ifdef ITECONSOLE

#include <sys/param.h>
#include <dev/cons.h>
#include <hp300/dev/device.h>
#include <hp300/stand/itevar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/stand/consdefs.h>
#include <hp300/stand/samachdep.h>

void	ite_deinit_noop __P((struct ite_data *));

struct itesw itesw[] = {
	{ GID_TOPCAT,
	topcat_init,	ite_deinit_noop, topcat_clear,	topcat_putc,
	topcat_cursor,	topcat_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_GATORBOX,
	gbox_init,	ite_deinit_noop, gbox_clear,	gbox_putc,
	gbox_cursor,	gbox_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_RENAISSANCE,
	rbox_init,	ite_deinit_noop, rbox_clear,	rbox_putc,
	rbox_cursor,	rbox_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_LRCATSEYE,
	topcat_init,	ite_deinit_noop, topcat_clear,	topcat_putc,
	topcat_cursor,	topcat_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_HRCCATSEYE,
	topcat_init,	ite_deinit_noop, topcat_clear,	topcat_putc,
	topcat_cursor,	topcat_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_HRMCATSEYE,
	topcat_init,	ite_deinit_noop, topcat_clear,	topcat_putc,
	topcat_cursor,	topcat_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_DAVINCI,
      	dvbox_init,	ite_deinit_noop, dvbox_clear,	dvbox_putc,
	dvbox_cursor,	dvbox_scroll,	ite_readbyte,	ite_writeglyph },

	{ GID_HYPERION,
	hyper_init,	ite_deinit_noop, hyper_clear,	hyper_putc,
	hyper_cursor,	hyper_scroll,	ite_readbyte,	ite_writeglyph },
};
int	nitesw = sizeof(itesw) / sizeof(itesw[0]);

/* these guys need to be in initialized data */
int itecons = -1;
struct  ite_data ite_data[NITE] = { 0 };
int	ite_scode[NITE] = { 0 };

/*
 * Locate all bitmapped displays
 */
iteconfig()
{
	extern struct hp_hw sc_table[];
	int dtype, fboff, i;
	struct hp_hw *hw;
	struct grfreg *gr;
	struct ite_data *ip;

	i = 0;
	for (hw = sc_table; hw < &sc_table[MAXCTLRS]; hw++) {
	        if (!HW_ISDEV(hw, D_BITMAP))
			continue;
		gr = (struct grfreg *) hw->hw_kva;
		/* XXX: redundent but safe */
		if (badaddr((caddr_t)gr) || gr->gr_id != GRFHWID)
			continue;
		for (dtype = 0; dtype < nitesw; dtype++)
			if (itesw[dtype].ite_hwid == gr->gr_id2)
				break;
		if (dtype == nitesw)
			continue;
		if (i >= NITE)
			break;
		ite_scode[i] = hw->hw_sc;
		ip = &ite_data[i];
		ip->isw = &itesw[dtype];
		ip->regbase = (caddr_t) gr;
		fboff = (gr->gr_fbomsb << 8) | gr->gr_fbolsb;
		ip->fbbase = (caddr_t) (*((u_char *)ip->regbase+fboff) << 16);
		/* DIO II: FB offset is relative to select code space */
		if (ip->regbase >= (caddr_t)DIOIIBASE)
			ip->fbbase += (int)ip->regbase;
		ip->fbwidth  = gr->gr_fbwidth_h << 8 | gr->gr_fbwidth_l;
		ip->fbheight = gr->gr_fbheight_h << 8 | gr->gr_fbheight_l;
		ip->dwidth   = gr->gr_dwidth_h << 8 | gr->gr_dwidth_l;
		ip->dheight  = gr->gr_dheight_h << 8 | gr->gr_dheight_l;
		/*
		 * XXX some displays (e.g. the davinci) appear
		 * to return a display height greater than the
		 * returned FB height.  Guess we should go back
		 * to getting the display dimensions from the
		 * fontrom...
		 */
		if (ip->dwidth > ip->fbwidth)
			ip->dwidth = ip->fbwidth;
		if (ip->dheight > ip->fbheight)
			ip->dheight = ip->fbheight;
		ip->flags = ITE_ALIVE|ITE_CONSOLE;
		i++;
	}
}

#ifdef CONSDEBUG
/*
 * Allows us to cycle through all possible consoles (NITE ites and serial port)
 * by using SHIFT-RESET on the keyboard.
 */
int	whichconsole = -1;
#endif

void
iteprobe(cp)
	struct consdev *cp;
{
	register int ite;
	register struct ite_data *ip;
	int unit, pri;

#ifdef CONSDEBUG
	whichconsole = ++whichconsole % (NITE+1);
#endif

	if (itecons != -1)
		return;

	iteconfig();
	unit = -1;
	pri = CN_DEAD;
	for (ite = 0; ite < NITE; ite++) {
#ifdef CONSDEBUG
		if (ite < whichconsole)
			continue;
#endif
		ip = &ite_data[ite];
		if ((ip->flags & (ITE_ALIVE|ITE_CONSOLE))
		    != (ITE_ALIVE|ITE_CONSOLE))
			continue;
		if ((int)ip->regbase == GRFIADDR) {
			pri = CN_INTERNAL;
			unit = ite;
		} else if (unit < 0) {
			pri = CN_NORMAL;
			unit = ite;
		}
	}
	curcons_scode = ite_scode[unit];
	cp->cn_dev = unit;
	cp->cn_pri = pri;
}

void
iteinit(cp)
	struct consdev *cp;
{
	int ite = cp->cn_dev;
	struct ite_data *ip;

	if (itecons != -1)
		return;

	ip = &ite_data[ite];

	ip->curx = 0;
	ip->cury = 0;
	ip->cursorx = 0;
	ip->cursory = 0;

	(*ip->isw->ite_init)(ip);
	(*ip->isw->ite_cursor)(ip, DRAW_CURSOR);

	itecons = ite;
	kbdinit();
}

/* ARGSUSED */
void
iteputchar(dev, c)
	dev_t dev;
	register int c;
{
	register struct ite_data *ip = &ite_data[itecons];
	register struct itesw *sp = ip->isw;

	c &= 0x7F;
	switch (c) {

	case '\n':
		if (++ip->cury == ip->rows) {
			ip->cury--;
			(*sp->ite_scroll)(ip, 1, 0, 1, SCROLL_UP);
			ite_clrtoeol(ip, sp, ip->cury, 0);
		}
		else
			(*sp->ite_cursor)(ip, MOVE_CURSOR);
		break;

	case '\r':
		ip->curx = 0;
		(*sp->ite_cursor)(ip, MOVE_CURSOR);
		break;

	case '\b':
		if (--ip->curx < 0)
			ip->curx = 0;
		else
			(*sp->ite_cursor)(ip, MOVE_CURSOR);
		break;

	default:
		if (c < ' ' || c == 0177)
			break;
		(*sp->ite_putc)(ip, c, ip->cury, ip->curx, ATTR_NOR);
		(*sp->ite_cursor)(ip, DRAW_CURSOR);
		itecheckwrap(ip, sp);
		break;
	}
}

itecheckwrap(ip, sp)
     register struct ite_data *ip;
     register struct itesw *sp;
{
	if (++ip->curx == ip->cols) {
		ip->curx = 0;
		if (++ip->cury == ip->rows) {
			--ip->cury;
			(*sp->ite_scroll)(ip, 1, 0, 1, SCROLL_UP);
			ite_clrtoeol(ip, sp, ip->cury, 0);
			return;
		}
	}
	(*sp->ite_cursor)(ip, MOVE_CURSOR);
}

ite_clrtoeol(ip, sp, y, x)
     register struct ite_data *ip;
     register struct itesw *sp;
     register int y, x;
{
	(*sp->ite_clear)(ip, y, x, 1, ip->cols - x);
	(*sp->ite_cursor)(ip, DRAW_CURSOR);
}

/* ARGSUSED */
int
itegetchar(dev)
	dev_t dev;
{
#ifdef SMALL
	return (0);
#else
	return (kbdgetc());
#endif
}
#endif

/* ARGSUSED */
void
ite_deinit_noop(ip)
	struct ite_data *ip;
{
}

/*	$NetBSD: rcons_kern.c,v 1.3 1995/11/29 22:09:23 pk Exp $ */

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)rcons_kern.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <dev/rcons/raster.h>
#include <dev/rcons/rcons.h>

extern struct tty *fbconstty;

static void rcons_belltmr(void *);

#include "rcons_subr.h"

static struct rconsole *mydevicep;

void
rcons_cnputc(c)
	int c;
{
	char buf[1];

	if (c == '\n')
		rcons_puts(mydevicep, "\r\n", 2);
	else {
		buf[0] = c;
		rcons_puts(mydevicep, buf, 1);
	}
}

static void
rcons_output(tp)
	register struct tty *tp;
{
	register int s, n, i;
	char buf[OBUFSIZ];

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);
	n = q_to_b(&tp->t_outq, buf, sizeof(buf));
	rcons_puts(mydevicep, buf, n);

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (tp->t_outq.c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout(ttrstrt, tp, 1);
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	splx(s);
}

/* Ring the console bell */
void
rcons_bell(rc)
	register struct rconsole *rc;
{
	register int i, s;

	if (rc->rc_bits & FB_VISBELL) {
		/* invert the screen twice */
		for (i = 0; i < 2; ++i)
			raster_op(rc->rc_sp, 0, 0,
			    rc->rc_sp->width, rc->rc_sp->height,
			    RAS_INVERT, (struct raster *) 0, 0, 0);
	}

	s = splhigh();
	if (rc->rc_belldepth++) {
		if (rc->rc_belldepth > 3)
			rc->rc_belldepth = 3;
		splx(s);
	} else {
		rc->rc_ringing = 1;
		splx(s);
		(*rc->rc_bell)(1);
		/* XXX Chris doesn't like the following divide */
		timeout(rcons_belltmr, rc, hz/10);
	}
}

/* Bell timer service routine */
static void
rcons_belltmr(p)
	void *p;
{
	register struct rconsole *rc = p;
	register int s = splhigh(), i;

	if (rc->rc_ringing) {
		rc->rc_ringing = 0;
		i = --rc->rc_belldepth;
		splx(s);
		(*rc->rc_bell)(0);
		if (i != 0)
			/* XXX Chris doesn't like the following divide */
			timeout(rcons_belltmr, rc, hz/30);
	} else {
		rc->rc_ringing = 1;
		splx(s);
		(*rc->rc_bell)(1);
		timeout(rcons_belltmr, rc, hz/10);
	}
}

void
rcons_init(rc)
	register struct rconsole *rc;
{
	/* XXX this should go away */
	static struct raster xxxraster;
	register struct raster *rp = rc->rc_sp = &xxxraster;
	register struct winsize *ws;
	register int i;
	static int row, col;

	mydevicep = rc;

	/* XXX mostly duplicates of data in other places */
	rp->width = rc->rc_width;
	rp->height = rc->rc_height;
	rp->depth = rc->rc_depth;
	if (rc->rc_linebytes & 0x3) {
		printf("rcons_init: linebytes assumption botched (0x%x)\n",
		    rc->rc_linebytes);
		return;
	}
	rp->linelongs = rc->rc_linebytes >> 2;
	rp->pixels = (u_int32_t *)rc->rc_pixels;

	rc->rc_ras_blank = RAS_CLEAR;

	/* Impose upper bounds on rc_max{row,col} */
	i = rc->rc_height / rc->rc_font->height;
	if (rc->rc_maxrow > i)
		rc->rc_maxrow = i;
	i = rc->rc_width / rc->rc_font->width;
	if (rc->rc_maxcol > i)
		rc->rc_maxcol = i;

	/* Let the system know how big the console is */
	ws = &fbconstty->t_winsize;
	ws->ws_row = rc->rc_maxrow;
	ws->ws_col = rc->rc_maxcol;
	ws->ws_xpixel = rc->rc_width;
	ws->ws_ypixel = rc->rc_height;

	/* Center emulator screen (but align x origin to 32 bits) */
	rc->rc_xorigin =
	    ((rc->rc_width - rc->rc_maxcol * rc->rc_font->width) / 2) & ~0x1f;
	rc->rc_yorigin =
	    (rc->rc_height - rc->rc_maxrow * rc->rc_font->height) / 2;

	/* Emulator width and height used for scrolling */
	rc->rc_emuwidth = rc->rc_maxcol * rc->rc_font->width;
	if (rc->rc_emuwidth & 0x1f) {
		/* Pad to 32 bits */
		i = (rc->rc_emuwidth + 0x1f) & ~0x1f;
		/* Make sure emulator width isn't too wide */
		if (rc->rc_xorigin + i <= rc->rc_width)
			rc->rc_emuwidth = i;
	}
	rc->rc_emuheight = rc->rc_maxrow * rc->rc_font->height;

#ifdef RASTERCONS_WONB
	rc->rc_ras_blank = RAS_NOT(rc->rc_ras_blank);
	rc->rc_bits |= FB_INVERT;
#endif

	if (rc->rc_row == NULL || rc->rc_col == NULL) {
		/*
		 * No address passed; use private copies
		 * go to LL corner and scroll.
		 */
		rc->rc_row = &row;
		rc->rc_col = &col;
		row = rc->rc_maxrow;
		col = 0;
#if 0
		rcons_clear2eop(rc);	/* clear the display */
#endif
		rcons_scroll(rc, 1);
		rcons_cursor(rc);	/* and draw the initial cursor */
	} else {
		/* Prom emulator cursor is currently visible */
		rc->rc_bits |= FB_CURSOR;
	}

	/* Initialization done; hook us up */
	fbconstty->t_oproc = rcons_output;
	/*fbconstty->t_stop = (void (*)()) nullop;*/
}

/*	$OpenBSD: ite_cv.c,v 1.2 1996/05/02 06:44:12 niklas Exp $	*/
/*	$NetBSD: ite_cv.c,v 1.2 1996/04/21 21:11:59 veego Exp $	*/

/*
 * Copyright (c) 1995 Michael Teske
 * All rights reserved.
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
 *      This product includes software developed by Christian E. Hopps,
 *      Ezra Story, Kari Mettinen, Markus Wild, Lutz Vieweg
 *      and Michael Teske.
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
 * The text console is based on ite_cl.c and ite_rh.c by
 * Ezra Story, Kari Mettinen, Markus Wild, Lutz Vieweg.
 * The gfx console is based on ite_cc.c from Christian E. Hopps.
 */

#include "grfcv.h"
#if NGRFCV > 0

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/termios.h>
#include <sys/malloc.h>
#include <dev/cons.h>
#include <machine/cpu.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/iteioctl.h>
#include <amiga/amiga/device.h> 
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_cvreg.h>

void cv_ite_init __P((struct ite_softc *));
void cv_ite_deinit __P((struct ite_softc *));
static void cv_cursor __P((struct ite_softc *, int));
static void cv_putc __P((struct ite_softc *, int, int, int, int));
static void cv_clear __P((struct ite_softc *, int, int, int, int));
static void cv_scroll __P((struct ite_softc *, int, int, int, int));

#define MAXROWS 200
#define MAXCOLS 200
static unsigned short cv_rowc[MAXROWS];

#ifndef CV_DONT_USE_CONBUFFER

/*
 * Console buffer to avoid the slow reading from gfx mem.
 * this takes up 40k but it makes scrolling 3 times faster.
 * I'd like to alocate it dynamically.
 */
static unsigned short console_buffer[MAXCOLS*MAXROWS];
#endif

/*
 * called from grf_cv to return console priority
 */
int
grfcv_cnprobe()
{
	static int done;
	int rv;

	if (done == 0)
#ifdef CV64CONSOLE
		rv = CN_INTERNAL;
#else
		rv = CN_DEAD;
#endif
	else
#ifdef CV64CONSOLE
		rv = CN_NORMAL;
#else
		rv = CN_DEAD;
#endif
	done = 1;
	return(rv);
}


/*
 * called from grf_cv to init ite portion of
 * grf_softc struct
 */
void
grfcv_iteinit(gp)
	struct grf_softc *gp;
{
	gp->g_itecursor = cv_cursor;
	gp->g_iteputc = cv_putc;
	gp->g_iteclear = cv_clear;
	gp->g_itescroll = cv_scroll;
	gp->g_iteinit = cv_ite_init;
	gp->g_itedeinit = cv_ite_deinit;
}


void
cv_ite_deinit(ip)
	struct ite_softc *ip;
{
	ip->flags &= ~ITE_INITED;
}


void
cv_ite_init(ip)
	register struct ite_softc *ip;
{
	struct grfcvtext_mode *md;
	int i;

	ip->priv = ip->grf->g_data;
	md = (struct grfcvtext_mode *) ip->grf->g_data;

	ip->cols = md->cols;
	ip->rows = md->rows;
	if (ip->rows > MAXROWS)
		panic ("ite_cv.c: Too many rows!");

	for (i = 0; i < ip->rows; i++)
		cv_rowc[i] = i * ip->cols;
#ifndef CV_DONT_USE_CONBUFFER
	for (i = 0; i < MAXCOLS*MAXROWS; i++)
		console_buffer[i] = 0x2007;
#endif
}


void
cv_cursor(ip, flag)
	struct ite_softc *ip;
	int flag;
{
	volatile caddr_t ba = ip->grf->g_regkva;

	switch (flag) {
	    case DRAW_CURSOR:
		/*WCrt(ba, CRT_ID_CURSOR_START, & ~0x20); */
	    case MOVE_CURSOR:
		flag = ip->curx + ip->cury * ip->cols;
		WCrt(ba, CRT_ID_CURSOR_LOC_LOW, flag & 0xff);
		WCrt(ba, CRT_ID_CURSOR_LOC_HIGH, flag >> 8);
		ip->cursorx = ip->curx;
		ip->cursory = ip->cury;
		break;
	    case ERASE_CURSOR:
		/*WCrt(ba, CRT_ID_CURSOR_START, | 0x20); */
	    case START_CURSOROPT:
	    case END_CURSOROPT:
	    default:
		break;
	}
}


void
cv_putc(ip, c, dy, dx, mode)
	struct ite_softc *ip;
	int c;
	int dy;
	int dx;
	int mode;
{
	caddr_t fb = ip->grf->g_fbkva;
	unsigned char attr;
	unsigned char *cp;

	attr = (unsigned char) ((mode & ATTR_INV) ? (0x70) : (0x07));
	if (mode & ATTR_UL)     attr  = 0x01;
	if (mode & ATTR_BOLD)   attr |= 0x08;
	if (mode & ATTR_BLINK)  attr |= 0x80;

	cp = fb + ((cv_rowc[dy] + dx) << 2); /* *4 */
	*cp++ = (unsigned char) c;
	*cp = (unsigned char) attr;
#ifndef CV_DONT_USE_CONBUFFER
	cp = (unsigned char *) &console_buffer[cv_rowc[dy]+dx];
	*cp++ = (unsigned char) c;
	*cp = (unsigned char) attr;
#endif
}


void
cv_clear(ip, sy, sx, h, w)
	struct ite_softc *ip;
	int sy;
	int sx;
	int h;
	int w;
{
	/* cv_clear and cv_scroll both rely on ite passing arguments
	 * which describe continuous regions.  For a VT200 terminal,
	 * this is safe behavior.
	 */
	unsigned short  *dst;
	int len;

	dst = (unsigned short *) (ip->grf->g_fbkva + (((sy * ip->cols) + sx) << 2));

	for (len = w*h; len > 0 ; len--) {
		*dst = 0x2007;
		dst +=2;
	}

#ifndef CV_DONT_USE_CONBUFFER
	dst = &console_buffer[(sy * ip->cols) + sx];
	for (len = w*h; len > 0 ; len--) {
		*dst++ = 0x2007;
	}
#endif
}

void
cv_scroll(ip, sy, sx, count, dir)
	struct ite_softc *ip;
	int sy;
	int sx;
	int count;
	int dir;
{
	unsigned short *src, *dst, *dst2;
	int i;
	int len;

	src = (unsigned short *)(ip->grf->g_fbkva + ((sy * ip->cols) << 2));

	switch (dir) {
	    case SCROLL_UP:
		dst = src - ((count * ip->cols)<<1);
#ifdef CV_DONT_USE_CONBUFFER
		for (i = 0; i < (ip->bottom_margin + 1 - sy) * ip->cols; i++) {
			*dst++ = *src++; /* copy only plane 0 and 1 */
			dst++; src++;
		}
#else
		len = (ip->bottom_margin + 1 - sy) * ip->cols;
		src = &console_buffer[sy*ip->cols];
#if 0
		if (count > sy) { /* boundary checks */
			dst2 = console_buffer;
			len -= (count - sy) * ip->cols;
			src += (count - sy) * ip->cols;
		} else 
#endif
			dst2 = &console_buffer[(sy-count)*ip->cols];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
#endif
		break;
	    case SCROLL_DOWN:
		dst = src + ((count * ip->cols)<<1);
#ifdef CV_DONT_USE_CONBUFFER
		len= (ip->bottom_margin + 1 - (sy + count)) * ip->cols;
		dst += len << 1;
		src += len << 1;
		for (i = 0; i < len; i++) {
			*dst-- = *src--;
			dst--; src--;
		}
#else
		len = (ip->bottom_margin + 1 - (sy + count)) * ip->cols;
		src = &console_buffer[sy*ip->cols];
		dst2 = &console_buffer[(sy+count)*ip->cols];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
#endif
		break;
	    case SCROLL_RIGHT:
		dst = src + ((sx+count)<<1);
#ifdef CV_DONT_USE_CONBUFFER
		src += sx << 1;
		len = (ip->cols - (sx + count));
		dst += (len-1) << 1;
		src += (len-1) << 1;

		for (i = 0; i < len ; i++) {
			*dst-- = *src--;
			dst--; src--;
		}
#else
		src = &console_buffer[sy*ip->cols + sx];
		len = ip->cols - (sx + count);
		dst2 = &console_buffer[sy*ip->cols + sx + count];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
#endif
		break;
	    case SCROLL_LEFT:
		dst = src + ((sx - count)<<1);
#ifdef CV_DONT_USE_CONBUFFER
		src += sx << 1;
		for (i = 0; i < (ip->cols - sx) ; i++) {
			*dst++ = *src++;
			dst++; src++;
		}
#else
		src = &console_buffer[sy*ip->cols + sx];
		len = ip->cols - sx;
		dst2 = &console_buffer[sy*ip->cols + sx - count];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
#endif
	}
}

#endif /* NGRFCV */

/*	$OpenBSD: ite_cv.c,v 1.3 1996/05/29 10:15:28 niklas Exp $	*/
/*	$NetBSD: ite_cv.c,v 1.3 1996/05/19 21:05:58 veego Exp $	*/

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
 * This code is based on ite_cl.c and ite_rh.c by
 * Ezra Story, Kari Mettinen, Markus Wild, Lutz Vieweg.
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


static unsigned short cv_rowc[MAXCOLS*(MAXROWS+1)];

/*
 * Console buffer to avoid the slow reading from gfx mem.
 */

static unsigned short *console_buffer;

void
cv_ite_init(ip)
	register struct ite_softc *ip;
{
	struct grfcvtext_mode *md;
	int i;
	static char first = 1;
	volatile unsigned short *fb = (volatile unsigned short *)ip->grf->g_fbkva;
	unsigned short *buffer;


	ip->priv = ip->grf->g_data;
	md = (struct grfcvtext_mode *) ip->grf->g_data;

	ip->cols = md->cols;
	ip->rows = md->rows;

	/* alloc buffers */

#if 0  /* XXX malloc seems not to work in early init :( */
	if (cv_rowc)
		free(cv_rowc, M_DEVBUF);
 
	/* alloc all in one */
	cv_rowc = malloc(sizeof(short) * (ip->rows + 1) * (ip->cols + 2),
		M_DEVBUF, M_WAITOK);
	if (!cv_rowc)
		panic("No buffers for ite_cv!");
#endif
 
	console_buffer = cv_rowc + ip->rows + 1;


	for (i = 0; i < ip->rows; i++)
		cv_rowc[i] = i * ip->cols;

	if (first) {
		for (i = 0; i < ip->rows * ip->cols; i++)
			console_buffer[i] = 0x2007;
		first = 0;
	} else { /* restore console */
		buffer = console_buffer;
		for (i = 0; i < ip->rows * ip->cols; i++) {
			*fb++ = *buffer++;
			*fb++;
		}
	}
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

	cp = (unsigned char *) &console_buffer[cv_rowc[dy]+dx];
	*cp++ = (unsigned char) c;
	*cp = (unsigned char) attr;
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

	for (len = w * h; len > 0 ; len--) {
		*dst = 0x2007;
		dst +=2;
	}

	dst = &console_buffer[(sy * ip->cols) + sx];
	for (len = w * h; len > 0 ; len--) {
		*dst++ = 0x2007;
	}
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

	src = (unsigned short *)(ip->grf->g_fbkva + (cv_rowc[sy] << 2));

	switch (dir) {
	    case SCROLL_UP:
		dst = src - ((cv_rowc[count])<<1);

		len = cv_rowc[(ip->bottom_margin + 1 - sy)];
		src = &console_buffer[cv_rowc[sy]];

		if (count > sy) { /* boundary checks */
			dst2 = console_buffer;
			dst = (unsigned short *)(ip->grf->g_fbkva);
			len -= cv_rowc[(count - sy)];
			src += cv_rowc[(count - sy)];
		} else
			dst2 = &console_buffer[cv_rowc[(sy-count)]];

		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
		break;
	    case SCROLL_DOWN:
		dst = src + ((cv_rowc[count]) << 1);

		len = cv_rowc[(ip->bottom_margin + 1 - (sy + count))];
		src = &console_buffer[cv_rowc[sy]];
		dst2 = &console_buffer[cv_rowc[(sy + count)]];

		if (len < 0)
			return;  /* do some boundary check */

		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
		break;
	    case SCROLL_RIGHT:
		dst = src + ((sx+count)<<1);
		src = &console_buffer[cv_rowc[sy] + sx];
		len = ip->cols - (sx + count);
		dst2 = &console_buffer[cv_rowc[sy] + sx + count];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
		break;
	    case SCROLL_LEFT:
		dst = src + ((sx - count)<<1);
		src = &console_buffer[cv_rowc[sy] + sx];
		len = ip->cols - sx;
		dst2 = &console_buffer[cv_rowc[sy] + sx - count];
		bcopy (src, dst2, len << 1);

		for (i = 0; i < len; i++) {
			*dst++ = *dst2++;
			dst++;
		}
	}
}

#endif /* NGRFCV */


/*
 * Copyright (c) 1995 Ezra Story
 * Copyright (c) 1995 Kari Mettinen
 * Copyright (c) 1994 Markus Wild
 * Copyright (c) 1994 Lutz Vieweg
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
 *      This product includes software developed by Lutz Vieweg.
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

#include "grfcl.h"
#if NGRFCL > 0

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <dev/cons.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_clreg.h>
#include <amiga/dev/itevar.h>

#ifdef CL5426CONSOLE
int cl_console = 1;
#else
int cl_console = 0;
#endif

void cl_init __P((struct ite_softc *ip));
void cl_cursor __P((struct ite_softc *ip, int flag));
void cl_deinit __P((struct ite_softc *ip));
void cl_putc __P((struct ite_softc *ip, int c, int dy, int dx, int mode));
void cl_clear __P((struct ite_softc *ip, int sy, int sx, int h, int w));
void cl_scroll __P((struct ite_softc *ip, int sy, int sx, int count,
    int dir));


/*
 * Called to determine ite status.  Because the connection between the
 * console & ite in this driver is rather intimate, we return CN_DEAD
 * if the cl_console is not active.
 */
int
grfcl_cnprobe(void)
{
	static int done;
	int rv;

	if (cl_console && (done == 0))
		rv = CN_INTERNAL;
	else
		rv = CN_DEAD;

	done = 1;
	return(rv);
}

void
grfcl_iteinit(gp)
	struct grf_softc *gp;
{
	gp->g_iteinit = cl_init;
	gp->g_itedeinit = cl_deinit;
	gp->g_iteclear = cl_clear;
	gp->g_iteputc = cl_putc;
	gp->g_itescroll = cl_scroll;
	gp->g_itecursor = cl_cursor;
}

void
cl_init(ip)
	struct ite_softc *ip;
{
	struct grfcltext_mode *md;

	ip->priv = ip->grf->g_data;
	md = (struct grfcltext_mode *) ip->priv;

	ip->cols = md->cols;
	ip->rows = md->rows;
}


void
cl_cursor(ip, flag)
	struct ite_softc *ip;
	int flag;
{
	volatile u_char *ba = ip->grf->g_regkva;

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
cl_deinit(ip)
	struct ite_softc *ip;
{
	ip->flags &= ~ITE_INITED;
}


void
cl_putc(ip, c, dy, dx, mode)
	struct ite_softc *ip;
	int c;
	int dy;
	int dx;
	int mode;
{
	volatile unsigned char *ba = ip->grf->g_regkva;
	unsigned char *fb = ip->grf->g_fbkva;
	unsigned char attr;
	unsigned char *cp;

	attr =(unsigned char) ((mode & ATTR_INV) ? (0x70) : (0x07));
	if (mode & ATTR_UL)     attr  = 0x01;	/* ???????? */
	if (mode & ATTR_BOLD)   attr |= 0x08;
	if (mode & ATTR_BLINK)  attr |= 0x80;

	cp = fb + ((dy * ip->cols) + dx);
	SetTextPlane(ba,0x00);
	*cp = (unsigned char) c;
	SetTextPlane(ba,0x01);
	*cp = (unsigned char) attr;
}

void
cl_clear(ip, sy, sx, h, w)
	struct ite_softc *ip;
	int sy;
	int sx;
	int h;
	int w;
{
    	/* cl_clear and cl_scroll both rely on ite passing arguments
         * which describe continuous regions.  For a VT200 terminal,
         * this is safe behavior.
         */
    	unsigned char *src, *dst;
    	volatile unsigned char *ba = ip->grf->g_regkva;
    	int len;

    	dst = ip->grf->g_fbkva + (sy * ip->cols) + sx;
    	src = dst + (ip->rows*ip->cols); 
    	len = w*h;

    	SetTextPlane(ba, 0x00);
    	bcopy(src, dst, len);
    	SetTextPlane(ba, 0x01);
    	bcopy(src, dst, len);
}

void
cl_scroll(ip, sy, sx, count, dir)
	struct ite_softc *ip;
	int sy;
	int sx;
	int count;
	int dir;
{
    	unsigned char *fb;
    	volatile unsigned char *ba = ip->grf->g_regkva;

    	fb = ip->grf->g_fbkva + sy * ip->cols;
    	SetTextPlane(ba, 0x00);

    	switch (dir) {
    	case SCROLL_UP:
    	    	bcopy(fb, fb - (count * ip->cols), 
    	    	    (ip->bottom_margin + 1 - sy) * ip->cols);
    	    	break;
    	case SCROLL_DOWN:
    	    	bcopy(fb, fb + (count * ip->cols),
    	    	    (ip->bottom_margin + 1 - (sy + count)) * ip->cols);
    	    	break;
    	case SCROLL_RIGHT:
    	    	bcopy(fb+sx, fb+sx+count, ip->cols - (sx + count));
    	    	break;
    	case SCROLL_LEFT:
    	    	bcopy(fb+sx, fb+sx-count, ip->cols - sx);
    	    	break;
    	}

    	SetTextPlane(ba, 0x01);

    	switch (dir) {
    	case SCROLL_UP:
    	    	bcopy(fb, fb - (count * ip->cols), 
    	    	    (ip->bottom_margin + 1 - sy) * ip->cols);
    	    	break;
    	case SCROLL_DOWN:
    	    	bcopy(fb, fb + (count * ip->cols),
    	    	    (ip->bottom_margin + 1 - (sy + count)) * ip->cols);
    	    	break;
    	case SCROLL_RIGHT:
    	    	bcopy(fb+sx, fb+sx+count, ip->cols - (sx + count));
    	    	break;
    	case SCROLL_LEFT:
    	    	bcopy(fb+sx, fb+sx-count, ip->cols - sx);
    	    	break;
    	}
}
#endif /* NGRFCL */

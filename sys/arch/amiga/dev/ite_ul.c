/*	$NetBSD: ite_ul.c,v 1.2 1995/12/27 08:09:51 chopps Exp $	*/

/*
 * Copyright (c) 1995 Ignatios Souvatzis
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
#include "grful.h"
#if NGRFUL > 0

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
#include <amiga/amiga/isr.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_ulreg.h>

#ifndef KFONT_CUSTOM  
#ifdef KFONT_8X11
#define kernel_font_width       kernel_font_width_8x11
#define kernel_font_height      kernel_font_height_8x11
#define kernel_font_baseline    kernel_font_baseline_8x11
#define kernel_font_boldsmear   kernel_font_boldsmear_8x11
#define kernel_font_lo  kernel_font_lo_8x11
#define kernel_font_hi  kernel_font_hi_8x11
#define kernel_font     kernel_font_8x11
#define kernel_cursor   kernel_cursor_8x11
#else
#define kernel_font_width       kernel_font_width_8x8
#define kernel_font_height      kernel_font_height_8x8
#define kernel_font_baseline    kernel_font_baseline_8x8
#define kernel_font_boldsmear   kernel_font_boldsmear_8x8
#define kernel_font_lo  kernel_font_lo_8x8
#define kernel_font_hi  kernel_font_hi_8x8
#define kernel_font     kernel_font_8x8
#define kernel_cursor   kernel_cursor_8x8
#endif
#endif

extern u_int8_t kernel_font_width, kernel_font_height, kernel_font_baseline;
extern short  kernel_font_boldsmear;
extern u_int8_t kernel_font_lo, kernel_font_hi;
extern u_int8_t kernel_font[], kernel_cursor[];


#ifdef DEBUG_UL
#define gsp_out(ba,cmd,len) gsp_dump(cmd,len); gsp_write(ba,cmd,len)
#else
#define gsp_out(ba,cmd,len) gsp_write(ba,cmd,len)
#endif

int ulowell_console = 1;

void ulowell_cursor __P((struct ite_softc *,int));
void ulowell_scroll __P((struct ite_softc *,int,int,int,int));
void ulowell_deinit __P((struct ite_softc *));
void ulowell_clear __P((struct ite_softc *,int,int,int,int));
void ulowell_putc __P((struct ite_softc *,int,int,int,int));
void ulowell_init __P((struct ite_softc *));

#ifdef DEBUG_UL
void gsp_dump __P((u_int16_t *,int));
#endif

/* Text always on overlay plane, so: */

#define UL_FG(ip)  0xFFFF
#define UL_BG(ip)  0x0000

/*
 * this function is called from grf_ul to init the grf_softc->g_conpri
 * field each time a ulowell board is attached.
 */
int
grful_cnprobe()
{
	static int done;
	int uv;

	if (ulowell_console && done == 0)
		uv = CN_INTERNAL;
	else
		uv = CN_NORMAL;
	done = 1;
	return(uv);
}

/* 
 * init the required fields in the grf_softc struct for a
 * grf to function as an ite.
 */
void
grful_iteinit(gp)
	struct grf_softc *gp;
{
	gp->g_iteinit = ulowell_init;
	gp->g_itedeinit = ulowell_deinit;
	gp->g_iteclear = ulowell_clear;
	gp->g_iteputc = ulowell_putc;
	gp->g_itescroll = ulowell_scroll;
	gp->g_itecursor = ulowell_cursor;
}

void
ulowell_init(ip)
	struct ite_softc *ip;
{
	struct gspregs *ba;

	u_int16_t *sp;
	u_int16_t cmd[8];

	int i;

	ba = (struct gspregs *) ip->grf->g_regkva;

	ip->font     = kernel_font;
	ip->font_lo  = kernel_font_lo;
	ip->font_hi  = kernel_font_hi;
	ip->ftwidth  = kernel_font_width;
	ip->ftheight = kernel_font_height;
	ip->ftbaseline = kernel_font_baseline;
	ip->ftboldsmear = kernel_font_boldsmear;

	/* upload font data */

	ba->ctrl = LBL|INCW;
	ba->hstadrh = 0xFFA2; 
	ba->hstadrl = 0x0200; 

	ba->data = 0x0000; 
	ba->data = 0xFFA3; 
	ba->data = ip->ftwidth; 
	ba->data = ip->ftheight; 
	ba->data = ip->ftbaseline;
	ba->data = 1;
	ba->data = ip->font_lo;
	ba->data = ip->font_hi;
	ba->data = ip->ftboldsmear;
    
	ba->hstadrh = 0xFFA3; 
	ba->hstadrl = 0x0000; 
    
	/*      
	 * font has to be word aligned and padded to word boundary.
	 * 8 bit wide fonts will be byte swapped in the bit swap
	 * routine.
	 */      

	i = (ip->font_hi - ip->font_lo + 1) * ip->ftheight;
	if (ip->ftwidth <= 8)
		i /= 2;
	for (sp = (u_int16_t *)ip->font; i>0; --i,++sp) {
		ba->data = *sp;
	}       

	/* bitwise mirror the font: */

	cmd[0] = GCMD_FNTMIR;
	gsp_out(ba, cmd, 1);

	ip->priv = NULL;	
	ip->cursor_opt = 0;

	if (ip->ftwidth >0 && ip->ftheight > 0) {
		ip->cols = ip->grf->g_display.gd_dwidth  / ip->ftwidth;
		ip->rows = ip->grf->g_display.gd_dheight / ip->ftheight;
	}

	ulowell_clear(ip, 0, 0, ip->rows, ip->cols);

	/* 
	 * switch overlay plane 0 on again, in case s.b. did a GM_GRFOVOFF
	 * XXX maybe this should be done on each output, by the TMS code?
	 * what happens on panic?

	ba->ctrl = LBL;
	GSPSETHADRS(ba, 0xFE800000);
	ba->data = 0;
	ba->hstadrl = 0x0020;
	gup->gus_ovslct |= 1;
	ba->data = gup->gus_ovslct;
	 */

#ifdef UL_DEBUG
	printf("ulowell_init: %d %d %d %d %d %d\n", ip->ftwidth, ip->ftheight,
		ip->ftbaseline, ip->font_lo, ip->font_hi, ip->ftboldsmear);
#endif
}


void ulowell_cursor(struct ite_softc *ip, int flag)
{
	struct gspregs *ba;
	u_int16_t cmd[7];

	ba = (struct gspregs *)ip->grf->g_regkva;

	if (flag == END_CURSOROPT)
		--ip->cursor_opt;
	else if (flag == START_CURSOROPT) {
		if (!ip->cursor_opt)
			ulowell_cursor(ip, ERASE_CURSOR);
		++ip->cursor_opt;
		return;		/* if we are already opted */
	}

	if (ip->cursor_opt)
		return;		/* if we are still nested. */

	/* else we draw the cursor */

	if (flag != DRAW_CURSOR && flag != END_CURSOROPT) {
		/* erase cursor */
#if 0
		cmd[0] = GCMD_PIXBLT;
		cmd[1] = 1024 - ip->ftwidth;
		cmd[2] = 1024 - ip->ftheight;
		cmd[3] = ip->ftwidth;
		cmd[4] = ip->ftheight;
		cmd[5] = ip->cursorx * ip->ftwidth;
		cmd[6] = ip->cursory * ip->ftheight;
		gsp_out(ba, cmd, 7);
#endif
		cmd[0] = GCMD_FILL;
		cmd[1] = UL_FG(ip);
		cmd[2] = ip->cursorx * ip->ftwidth;
		cmd[3] = ip->cursory * ip->ftheight;
		cmd[4] = ip->ftwidth;
		cmd[5] = ip->ftheight;
		cmd[6] = 10;	/* thats src xor dst */
		gsp_out(ba, cmd, 7);
	}
		
	if (flag != DRAW_CURSOR && flag != MOVE_CURSOR &&
	    flag != END_CURSOROPT)
		return;

	/* draw cursor */

	ip->cursorx = min(ip->curx, ip->cols-1);
	ip->cursory = ip->cury;
#if 0
	cmd[0] = GCMD_PIXBLT;
	cmd[1] = ip->cursorx * ip->ftwidth;
	cmd[2] = ip->cursory * ip->ftheight;
	cmd[3] = ip->ftwidth;
	cmd[4] = ip->ftheight;
	cmd[5] = 1024 - ip->ftwidth;
	cmd[6] = 1024 - ip->ftheight;
	gsp_out(ba, cmd, 7);
#endif
	cmd[0] = GCMD_FILL;
	cmd[1] = UL_FG(ip);
	cmd[2] = ip->cursorx * ip->ftwidth;
	cmd[3] = ip->cursory * ip->ftheight;
	cmd[4] = ip->ftwidth;
	cmd[5] = ip->ftheight;
	cmd[6] = 10;	/* thats src xor dst */
	gsp_out(ba, cmd, 7);

}



static void screen_up (struct ite_softc *ip, int top, int bottom, int lines)
{	
	struct gspregs *ba;

	u_int16_t cmd[7];

	ba = (struct gspregs *)ip->grf->g_regkva;

#ifdef DEBUG_UL
	printf("screen_up %d %d %d ->",top,bottom,lines);
#endif
	/* do some bounds-checking here.. */

	if (top >= bottom)
		return;
	  
	if (top + lines >= bottom)
	{
		ulowell_clear (ip, top, 0, bottom - top, ip->cols);
		return;
	}

	cmd[0] = GCMD_PIXBLT;
	cmd[1] = 0;					/* x */
	cmd[2] = top			* ip->ftheight;	/* y */
	cmd[3] = ip->cols		* ip->ftwidth;	/* w */
	cmd[4] = (bottom-top+1)		* ip->ftheight;	/* h */
	cmd[5] = 0;					/* dst x */
	cmd[6] = (top-lines)  		* ip->ftheight;	/* dst y */
	gsp_out(ba, cmd, 7);

	ulowell_clear(ip, bottom-lines+1, 0, lines-1, ip->cols);
};

static void screen_down (struct ite_softc *ip, int top, int bottom, int lines)
{	
	struct gspregs *ba;

	u_int16_t cmd[7];

	ba = (struct gspregs *)ip->grf->g_regkva;

#ifdef DEBUG_UL
	printf("screen_down %d %d %d ->",top,bottom,lines);
#endif

	/* do some bounds-checking here.. */

	if (top >= bottom)
		return;
	  
	if (top + lines >= bottom)
	{
	    ulowell_clear (ip, top, 0, bottom - top, ip->cols);
	    return;
	}

	cmd[0] = GCMD_PIXBLT;
	cmd[1] = 0;					/* x */
	cmd[2] = top			* ip->ftheight;	/* y */
	cmd[3] = ip->cols		* ip->ftwidth;	/* w */
	cmd[4] = (bottom - top - lines)	* ip->ftheight;	/* h */
	cmd[5] = 0;					/* dst x */
	cmd[6] = (top + lines)		* ip->ftheight;	/* dst y */
	gsp_out(ba, cmd, 7);

	ulowell_clear(ip, top, 0, lines, ip->cols);
};

void ulowell_deinit(struct ite_softc *ip)
{
	ip->flags &= ~ITE_INITED;
}


void ulowell_putc(struct ite_softc *ip, int c, int dy, int dx, int mode)
{
	struct gspregs *ba;
	u_int16_t cmd[8];
	
	ba = (struct gspregs *)ip->grf->g_regkva;

	cmd[0] = GCMD_CHAR;
	cmd[1] = c & 0xff;
	cmd[2] = 0x0;
	cmd[3] = UL_FG(ip);
	cmd[4] = dx * ip->ftwidth;
	cmd[5] = dy * ip->ftheight;
	cmd[6] = mode;
	gsp_write(ba, cmd, 7);
}

void ulowell_clear(struct ite_softc *ip, int sy, int sx, int h, int w)
{
	/* XXX TBD */
	struct gspregs * ba;

	u_int16_t cmd[7];

#ifdef	DEBUG_UL
	printf("ulowell_clear %d %d %d %d ->",sy,sx,h,w);
#endif
	ba = (struct gspregs *)ip->grf->g_regkva;

	cmd[0] = GCMD_FILL;
	cmd[1] = 0x0; /* XXX */
	cmd[2] = sx * ip->ftwidth;
	cmd[3] = sy * ip->ftheight;
	cmd[4] = w * ip->ftwidth;
	cmd[5] = h * ip->ftheight;
	cmd[6] = 0;

	gsp_out(ba, cmd, 7);
}

void ulowell_scroll(struct ite_softc *ip, int sy, int sx, int count, int dir)
{
	struct gspregs *ba;
	u_int16_t cmd[7];

	ba = (struct gspregs *)ip->grf->g_regkva;

#ifdef DEBUG_UL
	printf("ulowell_scroll %d %d %d %d ->",sy,sx,count,dir);
#endif

	ulowell_cursor(ip, ERASE_CURSOR);

	if (dir == SCROLL_UP) {
		screen_up (ip, sy, ip->bottom_margin, count);
	} else if (dir == SCROLL_DOWN) {
		screen_down (ip, sy, ip->bottom_margin, count);
	} else if (dir == SCROLL_RIGHT) {
		cmd[0] = GCMD_PIXBLT;
		cmd[1] = sx * ip->ftwidth;
		cmd[2] = sy * ip->ftheight;
		cmd[3] = (ip->cols - sx - count) * ip->ftwidth;
		cmd[4] = ip->ftheight;
		cmd[5] = (sx + count) * ip->ftwidth;
		cmd[6] = sy * ip->ftheight;
		gsp_out(ba,cmd,7);
		ulowell_clear (ip, sy, sx, 1, count);
	} else {
		cmd[0] = GCMD_PIXBLT;
		cmd[1] = sx * ip->ftwidth;
		cmd[2] = sy * ip->ftheight;
		cmd[3] = (ip->cols - sx) * ip->ftwidth;
		cmd[4] = ip->ftheight;
		cmd[5] = (sx - count) * ip->ftwidth;
		cmd[6] = sy * ip->ftheight;
		gsp_out(ba,cmd,7);
		ulowell_clear (ip, sy, ip->cols - count, 1, count);
	}		
}

#ifdef DEBUG_UL
void
gsp_dump(cmd,len)
	u_int16_t *cmd;
	int len;
{
	printf("gsp");
	while (len-- > 0)
		printf(" %lx",*cmd++);
	printf("\n");
}
#endif
#endif /* NGRFUL */

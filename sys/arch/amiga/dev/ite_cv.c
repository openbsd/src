/*	$OpenBSD: ite_cv.c,v 1.1 1996/03/30 22:18:21 niklas Exp $	*/
/*	$NetBSD: ite_cv.c,v 1.1 1996/03/02 14:28:51 veego Exp $	*/

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
 *      This product includes software developed by Christian E. Hopps
 *	and Michael Teske.
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
 * This code is based on ite_cc.c from Christian E. Hopps.
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
 
extern u_char kernel_font_width, kernel_font_height, kernel_font_baseline;
extern short  kernel_font_boldsmear;
extern u_char kernel_font_lo, kernel_font_hi;
extern u_char kernel_font[], kernel_cursor[];
 
 
/*
 * This is what ip->priv points to;
 * it contains local variables for CV64 ite in gfx mode.
 */
#define MAXCOLS 200 /* Does someone use more than this? */
#define MAXROWS 200

struct ite_priv {
	u_int rowc[MAXROWS];		/* row coordinates  */
	u_int colc[MAXCOLS];		/* column coods */
	u_long row_bytes;
	u_long cursor_opt;
	u_int  row_offset;	/* the row offset */
	u_short width;		/* the framebuffer width */
	u_short height;
	u_short underline;	/* where the underline goes */
	u_short ft_x;		/* the font width */
	u_short ft_y;		/* the font height */
	u_short font_srcx[256]; /* the font source */
	u_short font_srcy[256];
};
typedef struct ite_priv ipriv_t;
 
void cv_load_font __P((struct ite_softc *));
void cv_ite_init __P((struct ite_softc *));
void cv_ite_deinit __P((struct ite_softc *));
void cv_inv_rect __P((volatile caddr_t, u_short, u_short, u_short, u_short));
static void cv_cursor __P((struct ite_softc *, int));
static inline void cv_blt_font __P((volatile caddr_t, int, int, int,
	int, int, int, int, int, int, int));
void cv_uline __P((volatile caddr_t, int, int, int, int));
void cv_putc_nm __P((struct ite_softc *, int, int, int)); 
void cv_putc_ul __P((struct ite_softc *, int, int, int)); 
void cv_putc_bd __P((struct ite_softc *, int, int, int)); 
void cv_putc_bd_ul __P((struct ite_softc *, int, int, int)); 
void cv_putc_in __P((struct ite_softc *, int, int, int)); 
void cv_putc_ul_in __P((struct ite_softc *, int, int, int)); 
void cv_putc_bd_in __P((struct ite_softc *, int, int, int)); 
void cv_putc_bd_ul_in __P((struct ite_softc *, int, int, int)); 
static void cv_putc __P((struct ite_softc *, int, int, int, int));
void cv_clr_rect __P((volatile caddr_t, u_short, u_short, u_short, u_short));
static void cv_clear __P((struct ite_softc *, int, int, int, int));
void cv_bitblt __P((volatile caddr_t, int, int, int, int, int, int));
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
 

/*
 * Load font into display memory (behind visible area!)
 * Could be faster, but it's called only on ite init, so who cares?
 */
void
cv_load_font(ip)
	register struct ite_softc *ip;
{
	int w, h;
	int i,j,k,l;
	int ncol;
	volatile caddr_t a, font_loc, ba, fb;
	ipriv_t *cvi;
 
	ba = (volatile caddr_t)ip->grf->g_regkva;
	fb = (volatile caddr_t)ip->grf->g_fbkva;
 
	cvi = ip->priv;
	w = cvi->width;
	h = cvi->height + 30; /* 30 lines space, to be sure:-) */
 
	font_loc = (volatile caddr_t)ip->grf->g_fbkva + w*h;
	ncol = ip->cols;
 
	if (ncol == 0)
              return;
	a = font_loc;
	j=0;
	for (i = 0 ;i <= ip->font_hi - ip->font_lo; i++) {
 
		/* Font location in memory */
		a = font_loc+(i%ncol)*ip->ftwidth+(i/ncol)*w*ip->ftheight;
 
		/* P2C conversion */
		for (k=0; k < ip->ftheight; k++) {
			for (l = ip->ftwidth - 1; l >= 0; l--) {
				/* XXX depends on fwidth = 8 !!!!! */
				if (ip->font[i*ip->ftheight+k] & (1 << l))
					*a++ = 1;
				else
					*a++ = 0;
			}
			a += w - ip->ftwidth;  /* next line */
		}
	}
 
	/* set up the font source */
        for (i = 0; i <= ip->font_hi; i++) {
		j = i - ip->font_lo;
		if (i < ip->font_lo) {
			cvi->font_srcx[i] = 0;
			cvi->font_srcy[i] = h ;
		} else {
			cvi->font_srcx[i] = (j%ncol) * ip->ftwidth;
			cvi->font_srcy[i] = h + (j/ncol)*ip->ftheight;
		}
	}
 
	/* set up column and row coordinates */
 
	if (ip->cols > MAXCOLS)
		panic ("ite_cv: too many columns");
	if (ip->rows > MAXROWS)
		panic ("ite_cv: too many rows");
 
	for (i = 0; i <= ip->cols; i++)
		cvi->colc[i] = i * ip->ftwidth;
	for (i = 0; i <= ip->rows; i++)
		cvi->rowc[i] = i * ip->ftheight;
}
 
 
void
cv_ite_init(ip)
	register struct ite_softc *ip;
{
        struct grfcvtext_mode *md;
	static ipriv_t cv_priv;
	volatile caddr_t vgaba, fb;
 
	ipriv_t *cvi;
	int i;
 
	cvi = ip->priv ;
	if (cvi == NULL) {    /* first time */
		cvi = &cv_priv;
		ip->priv = cvi;
	}
 
	md = (struct grfcvtext_mode *) ip->grf->g_data;
 
	ip->font     = md->fdata; /*kernel_font;*/
	ip->font_lo  = md->fdstart; /*kernel_font_lo;*/
	ip->font_hi  = md->fdend; /* kernel_font_hi;*/
	ip->ftwidth  = md->fx; /*kernel_font_width;*/
	ip->ftheight = md->fy; /*kernel_font_height;*/
 
	ip->ftbaseline = kernel_font_baseline;
	ip->ftboldsmear = kernel_font_boldsmear;
 
	/* Find the correct set of rendering routines for this font.  */
	if (ip->ftwidth > 8)
		panic("kernel font size not supported");
 
	cvi->cursor_opt = 0;
 
	ip->cols = md->cols;
	ip->rows = md->rows;
 
	cvi->width = md->gv.disp_width;
	cvi->height = md->gv.disp_height;
	cvi->underline = ip->ftbaseline + 1;
	cvi->row_offset = md->gv.disp_width;
	cvi->ft_x = ip->ftwidth;
	cvi->ft_y = ip->ftheight;
	cvi->row_bytes = cvi->row_offset * ip->ftheight;
 
	vgaba = (volatile caddr_t)ip->grf->g_regkva;
 
	vgaw16(vgaba, ECR_READ_REG_DATA, 0x1000);
	delay(200000);
	vgaw16(vgaba, ECR_READ_REG_DATA, 0x2000);
	GfxBusyWait(vgaba);
	vgaw16(vgaba, ECR_READ_REG_DATA, 0x3fff);
	GfxBusyWait(vgaba);
	delay(200000);
	vgaw16(vgaba, ECR_READ_REG_DATA, 0x4fff);
	vgaw16(vgaba, ECR_READ_REG_DATA, 0xe000);
	vgaw16(vgaba, ECR_CURRENT_Y_POS2, 0x0);
	vgaw16(vgaba, ECR_CURRENT_X_POS2, 0x0);
	vgaw16(vgaba, ECR_DEST_Y__AX_STEP, 0x0);
	vgaw16(vgaba, ECR_DEST_Y2__AX_STEP2, 0x0);
	vgaw16(vgaba, ECR_DEST_X__DIA_STEP, 0x0);
	vgaw16(vgaba, ECR_DEST_X2__DIA_STEP2, 0x0);
	vgaw16(vgaba, ECR_SHORT_STROKE, 0x0);
	vgaw16(vgaba, ECR_DRAW_CMD, 0x01);
 
	/* It ain't easy to write here, so let's do it again */
	vgaw16(vgaba, ECR_READ_REG_DATA, 0x4fff);
 
	/* Clear with brute force... */
	fb = (volatile caddr_t) ip->grf->g_fbkva;
	for (i = 0; i < cvi->width * cvi->height; i++)
		*fb++=0;
	cv_clr_rect (vgaba, 0, 0,  cvi->width,  cvi->height);
	cv_load_font(ip);
}
 
 
void
cv_ite_deinit(ip)
	struct ite_softc *ip;
{
	ip->flags &= ~ITE_INITED;
}
 

/* Draws inverted rectangle (cursor) */
void
cv_inv_rect(vgaba, x, y, w, h)
	volatile caddr_t vgaba;
	u_short x, y, w, h;
{
	GfxBusyWait(vgaba);
	vgaw16 (vgaba, ECR_FRGD_MIX, 0x0025);
	vgaw32 (vgaba, ECR_FRGD_COLOR, 0x02);
 
	vgaw16 (vgaba, ECR_READ_REG_DATA, 0xA000);
	vgaw16 (vgaba, ECR_CURRENT_Y_POS , y);
	vgaw16 (vgaba, ECR_CURRENT_X_POS , x);
	vgaw16 (vgaba, ECR_READ_REG_DATA, h - 1);
	vgaw16 (vgaba, ECR_MAJ_AXIS_PIX_CNT, w - 1);
	vgaw16 (vgaba, ECR_DRAW_CMD, 0x40B1);
 
}
 

/*** (M<8)-by-N routines ***/
static void
cv_cursor(ip, flag)
	struct ite_softc *ip;
	int flag;
{
	int cend, cstart;
	struct grfcvtext_mode *md;
	volatile caddr_t ba, fb;
        ipriv_t *cvi;
 
	ba = ip->grf->g_regkva;
        fb = ip->grf->g_fbkva;
 
	md = (struct grfcvtext_mode *) ip->grf->g_data;
 
	cvi = ip->priv;
 
	if (flag == END_CURSOROPT)
		cvi->cursor_opt--;
	else if (flag == START_CURSOROPT) {
		if (!cvi->cursor_opt)
			cv_cursor (ip, ERASE_CURSOR);
		cvi->cursor_opt++;
		return;		  /* if we are already opted. */
	}
 
	if (cvi->cursor_opt)
		return;		  /* if we are still nested. */
				  /* else we draw the cursor. */
	cstart = 0;
	cend = ip->ftheight;
 
	if (flag != DRAW_CURSOR && flag != END_CURSOROPT) {
		/*
		 * erase the cursor by drawing again
		 */
		cv_inv_rect (ba, ip->cursorx * ip->ftwidth,
			ip->cursory * ip->ftheight + cstart,
			ip->ftwidth, cend);
	}
 
	if (flag != DRAW_CURSOR && flag != MOVE_CURSOR &&
	    flag != END_CURSOROPT)
		return;
 
	/*
	 * draw the cursor
	 */
 
	ip->cursorx = min(ip->curx, ip->cols-1);
	ip->cursory = ip->cury;
	cstart = 0;
	cend = ip->ftheight;
 
	cv_inv_rect (ba, ip->cursorx * ip->ftwidth,
		ip->cursory * ip->ftheight + cstart,
		ip->ftwidth, cend);
 
}
 
 
static inline void
cv_blt_font(vgaba, sx, sy, dx, dy, fw, fh, fg, bg, fmix, bmix)
	register volatile caddr_t vgaba;
	register int sx, sy, dx, dy, fw, fh;
	int fg, bg, fmix, bmix;
{
 
	GfxBusyWait(vgaba);
 
	vgaw16 (vgaba, ECR_READ_REG_DATA, 0xA0C0);
	vgaw16 (vgaba, ECR_BKGD_MIX, bmix);
	vgaw16 (vgaba, ECR_FRGD_MIX, fmix);
	vgaw16 (vgaba, ECR_BKGD_COLOR, bg);
	vgaw16 (vgaba, ECR_FRGD_COLOR, fg);
	vgaw16 (vgaba, ECR_BITPLANE_READ_MASK, 0x1);
	vgaw16 (vgaba, ECR_BITPLANE_WRITE_MASK, 0xfff);
	vgaw16 (vgaba, ECR_CURRENT_Y_POS , sy);
	vgaw16 (vgaba, ECR_CURRENT_X_POS , sx);
	vgaw16 (vgaba, ECR_DEST_Y__AX_STEP, dy);
	vgaw16 (vgaba, ECR_DEST_X__DIA_STEP, dx);
	vgaw16 (vgaba, ECR_READ_REG_DATA, fh);
	vgaw16 (vgaba, ECR_MAJ_AXIS_PIX_CNT, fw);
	vgaw16 (vgaba, ECR_DRAW_CMD, 0xc0f1);
}
 

/* Draws horizontal line */
void
cv_uline(vgaba, x, y, x2, fgcol)
	volatile caddr_t vgaba;
	int x, y, x2, fgcol;
{

	GfxBusyWait(vgaba);
	vgaw16 (vgaba, ECR_FRGD_MIX, 0x27);
	vgaw16 (vgaba, ECR_FRGD_COLOR, fgcol);
	vgaw16 (vgaba, ECR_READ_REG_DATA, 0xA000);
	vgaw16 (vgaba, ECR_CURRENT_Y_POS , y);
	vgaw16 (vgaba, ECR_CURRENT_X_POS , x);
	vgaw16 (vgaba, ECR_DEST_Y__AX_STEP, y);
	vgaw16 (vgaba, ECR_DEST_X__DIA_STEP, x2);
	GfxBusyWait(vgaba);
	vgaw16 (vgaba, ECR_DRAW_CMD, 0x2811);
}
 
 
void
cv_putc_nm(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 1, 0, 0x27, 0x7);
}
 
 
void
cv_putc_ul(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 1, 0, 0x27, 0x7);
 
	cv_uline (ip->grf->g_regkva,cvi->colc[dx], cvi->rowc[dy] +
                 cvi->underline, cvi->colc[dx] + cvi->ft_x-1, 1);
}
 
 
void
cv_putc_bd(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 1, 0, 0x27,0x7);
	/* smear bold with OR mix */
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx]+1, cvi->rowc[dy],
		cvi->ft_x-2, cvi->ft_y-1, 1, 0, 0x2b,0x5);
}
 

void
cv_putc_bd_ul(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_putc_bd(ip, c, dy, dx);
	cv_uline (ip->grf->g_regkva,cvi->colc[dx], cvi->rowc[dy] +
		cvi->underline, cvi->colc[dx] + cvi->ft_x-1, 1);
}
 
 
void
cv_putc_in(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 0, 1, 0x27,0x7);
}
 

void
cv_putc_ul_in(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 0, 1, 0x27,0x7);
 
	cv_uline (ip->grf->g_regkva,cvi->colc[dx], cvi->rowc[dy] +
		cvi->underline, cvi->colc[dx] + cvi->ft_x-1, 0);
}
 
 
void
cv_putc_bd_in(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx], cvi->rowc[dy],
		cvi->ft_x-1, cvi->ft_y-1, 0, 1, 0x27,0x7);
 
	/* smear bold with AND mix */
	cv_blt_font (ip->grf->g_regkva, cvi->font_srcx[c],
		cvi->font_srcy[c], cvi->colc[dx]+1, cvi->rowc[dy],
		cvi->ft_x-2, cvi->ft_y-1, 0, 1, 0x27, 0xc);
}
 
 
void
cv_putc_bd_ul_in(ip, c, dy, dx)
	struct ite_softc *ip;
	int c, dy, dx;
{
	ipriv_t *cvi = (ipriv_t *)ip->priv;
 
	cv_putc_bd_in(ip, c, dy, dx);
 
	cv_uline(ip->grf->g_regkva,cvi->colc[dx], cvi->rowc[dy] +
		cvi->underline, cvi->colc[dx] + cvi->ft_x-1, 0);
}
 
 
typedef void cv_putc_func ();
 
cv_putc_func *cv_put_func[ATTR_ALL+1] = {
    cv_putc_nm,
    cv_putc_in,
    cv_putc_ul,
    cv_putc_ul_in,
    cv_putc_bd,
    cv_putc_bd_in,
    cv_putc_bd_ul,
    cv_putc_bd_ul_in,
/* no support for blink */
    cv_putc_nm,
    cv_putc_in,
    cv_putc_ul,
    cv_putc_ul_in,
    cv_putc_bd,
    cv_putc_bd_in,
    cv_putc_bd_ul,
    cv_putc_bd_ul_in
};
 

static void
cv_putc(ip, c, dy, dx, mode)
	struct ite_softc *ip;
	int c, dy, dx, mode;
{

	c = (u_char)c;
	if (c < ip->font_lo || c > ip->font_hi)
		c = ip->font_hi;
	cv_put_func[mode](ip, c, dy, dx);
}
 
 
void
cv_clr_rect (vgaba, x, y, w, h)
	volatile caddr_t vgaba;
	u_short x, y, w, h;
{

	GfxBusyWait(vgaba);
	vgaw16 (vgaba, ECR_FRGD_MIX, 0x0027);
	vgaw32 (vgaba, ECR_FRGD_COLOR, 0x00);
	vgaw16 (vgaba, ECR_READ_REG_DATA, 0xA000);
	vgaw16 (vgaba, ECR_CURRENT_Y_POS , y);
	vgaw16 (vgaba, ECR_CURRENT_X_POS , x);
	vgaw16 (vgaba, ECR_READ_REG_DATA, h - 1);
	vgaw16 (vgaba, ECR_MAJ_AXIS_PIX_CNT, w - 1);
	vgaw16 (vgaba, ECR_DRAW_CMD, 0x40B1);
}
 

static void
cv_clear(ip, sy, sx, h, w)
	struct ite_softc *ip;
	int sy, sx, h, w;
{ 
 
	cv_clr_rect (ip->grf->g_regkva, sx*ip->ftwidth,
			sy*ip->ftheight, w*ip->ftwidth,
			h*ip->ftheight);
}
 

void
cv_bitblt(vgaba, sx, sy, dx, dy, fw, fh)
	volatile caddr_t vgaba;
	int sx, sy, dx, dy, fw, fh;
{
	unsigned short drawdir = 0;

	/* Assume overlap */
	if (sx > dx)
		drawdir |=1<<5; /* X positive */
	else {
		sx += fw -1;
		dx += fw -1;
	}
	if (sy > dy)
		drawdir |=1<<7; /* Y positive */
	else {
		sy += fh - 1;
		dy += fh - 1;
	}
	GfxBusyWait (vgaba);
	vgaw16 (vgaba, ECR_READ_REG_DATA, 0xA000);
	vgaw16 (vgaba, ECR_BKGD_MIX, 0x7);
	vgaw16 (vgaba, ECR_FRGD_MIX, 0x67);
	vgaw16 (vgaba, ECR_BKGD_COLOR, 0x0);
	vgaw16 (vgaba, ECR_FRGD_COLOR, 0x1);
 
	vgaw16 (vgaba, ECR_BITPLANE_READ_MASK, 0x1);
	vgaw16 (vgaba, ECR_BITPLANE_WRITE_MASK, 0xfff);
	vgaw16 (vgaba, ECR_CURRENT_Y_POS , sy);
	vgaw16 (vgaba, ECR_CURRENT_X_POS , sx);
	vgaw16 (vgaba, ECR_DEST_Y__AX_STEP, dy);
	vgaw16 (vgaba, ECR_DEST_X__DIA_STEP, dx);
	vgaw16 (vgaba, ECR_READ_REG_DATA, fh - 1);
	vgaw16 (vgaba, ECR_MAJ_AXIS_PIX_CNT, fw - 1);
	vgaw16 (vgaba, ECR_DRAW_CMD, 0xc051 | drawdir);
}
 
 
/* Note: sx is only relevant for SCROLL_LEFT or SCROLL_RIGHT.  */
static void
cv_scroll(ip, sy, sx, count, dir)
        register struct ite_softc *ip;
        register int sy;
        int dir, sx, count;
 
{
	int dy, dx;
	ipriv_t *cvi = (ipriv_t *) ip->priv;
 
	cv_cursor(ip, ERASE_CURSOR);
	switch (dir) {
	case SCROLL_UP:
		dy = sy - count;
 
		cv_bitblt(ip->grf->g_regkva,
			0,sy*ip->ftheight,
			0, dy*ip->ftheight,
                        cvi->width,
			(ip->bottom_margin-dy+1)*ip->ftheight);
 
		break;
	case SCROLL_DOWN:
		dy = sy + count;
 
		cv_bitblt(ip->grf->g_regkva,
			0,sy*ip->ftheight,
			0, dy*ip->ftheight,
			cvi->width,
			(ip->bottom_margin-dy+1)*ip->ftheight);
		break;
	case SCROLL_RIGHT:   /* one line */
		dx = sx + count;
		cv_bitblt(ip->grf->g_regkva,
			sx*ip->ftwidth, sy*ip->ftheight,
			dx*ip->ftwidth, sy*ip->ftheight,
			(ip->cols-dx-1)*ip->ftwidth,
			ip->ftheight);
		break;
	case SCROLL_LEFT:
		dx = sx - count;
		cv_bitblt(ip->grf->g_regkva,
			sx*ip->ftwidth, sy*ip->ftheight,
			dx*ip->ftwidth, sy*ip->ftheight,
			(ip->cols-dx-1)*ip->ftwidth,
			ip->ftheight);
		break;
	}
}
 
#endif /* NGRFCV */

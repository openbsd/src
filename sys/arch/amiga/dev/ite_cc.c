/*	$NetBSD: ite_cc.c,v 1.24 1995/05/07 15:37:08 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
#include "grfcc.h"
#if NGRFCC > 0

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/termios.h>
#include <dev/cons.h>
#include <machine/cpu.h>
#include <amiga/dev/itevar.h>
#include <amiga/dev/iteioctl.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/grfabs_reg.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_ccreg.h>
#include <amiga/dev/viewioctl.h>
#include <amiga/dev/viewvar.h>

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
 * it contains local variables for custom-chip ites.
 */
struct ite_priv {
	view_t *view;		/* the view for this ite. */
	u_char **row_ptr;	/* array of pointers into the bitmap  */
	u_long row_bytes;
	u_long cursor_opt;
	u_int  *column_offset;	/* array of offsets for columns */
	u_int  row_offset;	/* the row offset */
	u_short width;		/* the bitmap width */
	u_short underline;	/* where the underline goes */
	u_short ft_x;		/* the font width */
	u_short ft_y;		/* the font height */
	u_char *font_cell[256];	/* the font pointer */
};
typedef struct ite_priv ipriv_t;

void view_deinit __P((struct ite_softc *));
void view_init __P((struct ite_softc *));

static void putc8 __P((struct ite_softc *, int, int, int, int));
static void clear8 __P((struct ite_softc *, int, int, int, int));
static void scroll8 __P((struct ite_softc *, int, int, int, int));
static void cursor32 __P((struct ite_softc *, int));
static void scrollbmap __P((bmap_t *, u_short, u_short, u_short, u_short,
    short, short, u_char));

/* patchable */
int ite_default_x = 0;		/* def leftedge offset */
int ite_default_y = 0;		/* def topedge offset */
int ite_default_width = 640;	/* def width */
int ite_default_depth = 2;	/* def depth */
#if defined (GRF_NTSC)
int ite_default_height = 400;	/* def NTSC height */
#elif defined (GRF_PAL)
int ite_default_height = 512;	/* def PAL height */
#else
int ite_default_height = 400;	/* def NON-PAL/NTSC height (?) */
#endif

/*
 * called from grf_cc to return console priority
 */
int
grfcc_cnprobe()
{
	return(CN_INTERNAL);
}

/*
 * called from grf_cc to init ite portion of 
 * grf_softc struct
 */
void
grfcc_iteinit(gp)
	struct grf_softc *gp;
{
	gp->g_itecursor = cursor32;
	gp->g_iteputc = putc8;
	gp->g_iteclear = clear8;
	gp->g_itescroll = scroll8;
	gp->g_iteinit = view_init;
	gp->g_itedeinit = view_deinit;
}

int 
ite_newsize(ip, winsz)
	struct ite_softc *ip;
	struct itewinsize *winsz;
{
	extern struct view_softc views[];
	struct view_size vs;
	ipriv_t *cci = ip->priv;    
	u_long fbp, i;
	int error;

	vs.x = winsz->x;
	vs.y = winsz->y;
	vs.width = winsz->width;
	vs.height = winsz->height;
	vs.depth = winsz->depth;
	error = viewioctl(0, VIOCSSIZE, &vs, 0, -1);

	/*
	 * Reinitialize our structs
	 */
	cci->view = views[0].view; 

	/* -1 for bold. */
	ip->cols = (cci->view->display.width - 1) / ip->ftwidth; 
	ip->rows = cci->view->display.height / ip->ftheight;

	/*
	 * save new values so that future opens use them
	 * this may not be correct when we implement Virtual Consoles
	 */
	ite_default_height = cci->view->display.height;
	ite_default_width = cci->view->display.width;
	ite_default_x = cci->view->display.x;
	ite_default_y = cci->view->display.y;
	ite_default_depth = cci->view->bitmap->depth;

	if (cci->row_ptr) 
		free_chipmem(cci->row_ptr);
	if (cci->column_offset)
		free_chipmem(cci->column_offset);

	cci->row_ptr = alloc_chipmem(sizeof(u_char *) * ip->rows);
	cci->column_offset = alloc_chipmem(sizeof(u_int) * ip->cols);
    
	if (cci->row_ptr == NULL || cci->column_offset == NULL)
		panic("no chipmem for itecc data");
 

	cci->width = cci->view->bitmap->bytes_per_row << 3;
	cci->underline = ip->ftbaseline + 1;
	cci->row_offset = cci->view->bitmap->bytes_per_row 
	    + cci->view->bitmap->row_mod;
	cci->ft_x = ip->ftwidth;
	cci->ft_y = ip->ftheight;
 
	cci->row_bytes = cci->row_offset * ip->ftheight;

	cci->row_ptr[0] = VDISPLAY_LINE (cci->view, 0, 0);
	for (i = 1; i < ip->rows; i++) 
		cci->row_ptr[i] = cci->row_ptr[i-1] + cci->row_bytes;

	/* initialize the column offsets */
	cci->column_offset[0] = 0;
	for (i = 1; i < ip->cols; i++) 
		cci->column_offset[i] = cci->column_offset[i - 1] + cci->ft_x;

	/* initialize the font cell pointers */
	cci->font_cell[ip->font_lo] = ip->font;
	for (i=ip->font_lo+1; i<=ip->font_hi; i++)
		cci->font_cell[i] = cci->font_cell[i-1] + ip->ftheight;
	    
	return (error);
}

void
view_init(ip)
	register struct ite_softc *ip;
{
	struct itewinsize wsz;
	ipriv_t *cci;

	cci = ip->priv;

	if (cci)
		return;

	ip->font     = kernel_font;
	ip->font_lo  = kernel_font_lo;
	ip->font_hi  = kernel_font_hi;
	ip->ftwidth  = kernel_font_width;
	ip->ftheight = kernel_font_height;
	ip->ftbaseline = kernel_font_baseline;
	ip->ftboldsmear = kernel_font_boldsmear;

	/* Find the correct set of rendering routines for this font.  */
	if (ip->ftwidth > 8)
		panic("kernel font size not supported");
	cci = alloc_chipmem(sizeof (*cci));
	if (cci == NULL)
		panic("no memory for console device.");

	ip->priv = cci;
	cci->cursor_opt = 0;
	cci->view = NULL;
	cci->row_ptr = NULL;
	cci->column_offset = NULL;

	wsz.x = ite_default_x;
	wsz.y = ite_default_y;
	wsz.width = ite_default_width;
	wsz.height = ite_default_height;
	wsz.depth = ite_default_depth;

	ite_newsize (ip, &wsz);
	cc_mode(ip->grf, GM_GRFON, NULL, 0, 0);
}

int
ite_grf_ioctl (ip, cmd, addr, flag, p)
	struct ite_softc *ip;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct winsize ws;
	struct itewinsize *is;
	ipriv_t *cci;
	int error;

	cci = ip->priv;
	error = 0;

	switch (cmd) {
	case ITEIOCGWINSZ:
		is = (struct itewinsize *)addr;
		is->x = cci->view->display.x;
		is->y = cci->view->display.y;
		is->width = cci->view->display.width;
		is->height = cci->view->display.height;
		is->depth = cci->view->bitmap->depth;
		break;
	case ITEIOCSWINSZ:
		is = (struct itewinsize *)addr;

		if (ite_newsize(ip, is))
			error = ENOMEM;
		else {
			ws.ws_row = ip->rows;
			ws.ws_col = ip->cols;
			ws.ws_xpixel = cci->view->display.width;
			ws.ws_ypixel = cci->view->display.height;
			ite_reset (ip);
			/*
			 * XXX tell tty about the change 
			 * XXX this is messy, but works 
			 */
			iteioctl(0, TIOCSWINSZ, (caddr_t)&ws, 0, p);
		}
		break;
	case ITEIOCDSPWIN:
		cc_mode(ip->grf, GM_GRFON, NULL, 0, 0);
		break;
	case ITEIOCREMWIN:
		cc_mode(ip->grf, GM_GRFOFF, NULL, 0, 0);
		break;
	case VIOCSCMAP:
	case VIOCGCMAP:
		/*
		 * XXX needs to be fixed when multiple console implemented
		 * XXX watchout for that -1 its not really the kernel talking
		 * XXX these two commands don't use the proc pointer though
		 */
		error = viewioctl(0, cmd, addr, flag, -1);
		break;
	default:
		error = -1;
		break;
	}
	return (error);
}

void
view_deinit(ip)
	struct ite_softc *ip;
{
	ip->flags &= ~ITE_INITED;
}

/*** (M<8)-by-N routines ***/

static void
cursor32(struct ite_softc *ip, int flag)
{
	int cend, ofs, h, cstart, dr_plane;
	u_char *pl, opclr, opset;
	ipriv_t *cci;
	bmap_t *bm;
	view_t *v;

	cci = ip->priv;
	v = cci->view;
   	bm = v->bitmap;
	dr_plane = (bm->depth > 1 ? bm->depth-1 : 0);

	if (flag == END_CURSOROPT)
		cci->cursor_opt--;
	else if (flag == START_CURSOROPT) {
		if (!cci->cursor_opt)
			cursor32 (ip, ERASE_CURSOR);
		cci->cursor_opt++;
		return;		  /* if we are already opted. */
	}
    
	if (cci->cursor_opt) 
		return;		  /* if we are still nested. */
				  /* else we draw the cursor. */
	cstart = 0;
	cend = ip->ftheight-1; 
	pl = VDISPLAY_LINE(v, dr_plane, (ip->cursory * ip->ftheight + cstart));
	ofs = (ip->cursorx * ip->ftwidth);
    
	if (flag != DRAW_CURSOR && flag != END_CURSOROPT) {
		/*
		 * erase the cursor
		 */
		int h;

		if (dr_plane) {
			for (h = cend; h >= 0; h--) {
				asm("bfclr %0@{%1:%2}" : : "a" (pl),
				    "d" (ofs), "d" (ip->ftwidth));
				pl += cci->row_offset;
			}
		} else {
			for (h = cend; h >= 0; h--) {
				asm("bfchg %0@{%1:%2}" : : "a" (pl),
				    "d" (ofs), "d" (ip->ftwidth));
				pl += cci->row_offset;
			}
		}
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
	cend = ip->ftheight-1; 
	pl = VDISPLAY_LINE(v, dr_plane, ip->cursory * ip->ftheight + cstart);
	ofs = ip->cursorx * ip->ftwidth;

	if (dr_plane) {
		for (h = cend; h >= 0; h--) {
			asm("bfset %0@{%1:%2}" : : "a" (pl),
			    "d" (ofs), "d" (ip->ftwidth));
			pl += cci->row_offset;
		}
	} else {
		for (h = cend; h >= 0; h--) {
			asm("bfchg %0@{%1:%2}" : : "a" (pl),
			    "d" (ofs), "d" (ip->ftwidth));
			pl += cci->row_offset;
		}
	}
}


static inline
int expbits (int data)
{
	int i, nd = 0;

	if (data & 1)
		nd |= 0x02;
	for (i=1; i < 32; i++) {
		if (data & (1 << i))
			nd |= 0x5 << (i-1);
	}
	nd &= ~data;
	return(~nd);
}


/* Notes: optimizations given the kernel_font_(width|height) #define'd.
 *        the dbra loops could be elminated and unrolled using height,
 *        the :width in the bfxxx instruction could be made immediate instead
 *        of a data register as it now is.
 *        the underline could be added when the loop is unrolled
 *
 *        It would look like hell but be very fast.*/
 
static void 
putc_nm (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    while (fh--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (*f++), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }
}

static void 
putc_in (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    while (fh--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(*f++)), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }
}


static void 
putc_ul (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    int underline = cci->underline;
    while (underline--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (*f++), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }

    asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	 "d" (expbits(*f++)), "a" (p), "d" (co), "d" (fw));
    p += ro;

    underline = fh - cci->underline - 1;
    while (underline--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (*f++), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }
}


static void 
putc_ul_in (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    int underline = cci->underline;
    while (underline--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(*f++)), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }

    asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	 "d" (~expbits(*f++)), "a" (p), "d" (co), "d" (fw));
    p += ro;

    underline = fh - cci->underline - 1;
    while (underline--) {
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(*f++)), "a" (p), "d" (co), "d" (fw));
	p += ro;
    }
}

/* bold */
static void 
putc_bd (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    u_short ch;
    
    while (fh--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (ch), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }
}

static void 
putc_bd_in (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    u_short ch;
    
    while (fh--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(ch)), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }
}


static void 
putc_bd_ul (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    int underline = cci->underline;
    u_short ch;

    while (underline--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (ch), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }

    ch = *f++;
    ch |= ch << 1;
    asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	 "d" (expbits(ch)), "a" (p), "d" (co), "d" (fw+1));
    p += ro;

    underline = fh - cci->underline - 1;
    while (underline--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (ch), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }
}


static void 
putc_bd_ul_in (cci,p,f,co,ro,fw,fh)
    register ipriv_t *cci;
    register u_char  *p;
    register u_char  *f;
    register u_int    co;
    register u_int    ro;
    register u_int    fw;
    register u_int    fh;
{
    int underline = cci->underline;
    u_short ch;
    
    while (underline--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(ch)), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }

    ch = *f++;
    ch |= ch << 1;
    asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	 "d" (~expbits(ch)), "a" (p), "d" (co), "d" (fw+1));
    p += ro;

    underline = fh - cci->underline - 1;
    while (underline--) {
	ch = *f++;
	ch |= ch << 1;
	asm ("bfins %0,%1@{%2:%3}" : /* no output */ :
	     "d" (~(ch)), "a" (p), "d" (co), "d" (fw+1));
	p += ro;
    }
}


typedef void cc_putc_func ();

cc_putc_func *put_func[ATTR_ALL+1] = {
    putc_nm,
    putc_in,
    putc_ul,
    putc_ul_in,
    putc_bd,
    putc_bd_in,
    putc_bd_ul,
    putc_bd_ul_in,
/* no support for blink */
    putc_nm,
    putc_in,
    putc_ul,
    putc_ul_in,
    putc_bd,
    putc_bd_in,
    putc_bd_ul,
    putc_bd_ul_in
};


/* FIX: shouldn't this advance the cursor even if the character to
        be output is not available in the font? -ch */

static void
putc8(ip, c, dy, dx, mode)
	struct ite_softc *ip;
	int c, dy, dx, mode;
{
	ipriv_t *cci = (ipriv_t *) ip->priv;
	/*
	 * if character is higher than font has glyphs, substitute
	 * highest glyph.
	 */
	c = (u_char)c;
	if (c < ip->font_lo || c > ip->font_hi)
		c = ip->font_hi;
	put_func[mode](cci, cci->row_ptr[dy], cci->font_cell[c],
	    cci->column_offset[dx], cci->row_offset, cci->ft_x, cci->ft_y);
}

static void
clear8(struct ite_softc *ip, int sy, int sx, int h, int w)
{
  ipriv_t *cci = (ipriv_t *) ip->priv;
  view_t *v = cci->view;
  bmap_t *bm = cci->view->bitmap;

  if ((sx == 0) && (w == ip->cols))
    {
      /* common case: clearing whole lines */
      while (h--)
	{
	  int i;
	  u_char *ptr = cci->row_ptr[sy]; 
	  for (i=0; i < ip->ftheight; i++) {
            bzero(ptr, bm->bytes_per_row);
            ptr += bm->bytes_per_row + bm->row_mod;			/* don't get any smart
                                                   ideas, becuase this is for
                                                   interleaved bitmaps */
          }
	  sy++;
	}
    }
  else
    {
      /* clearing only part of a line */
      /* XXX could be optimized MUCH better, but is it worth the trouble? */
      while (h--)
	{
	  u_char *pl = cci->row_ptr[sy];
          int ofs = sx * ip->ftwidth;
	  int i, j;
	  for (i = w-1; i >= 0; i--)
	    {
	      u_char *ppl = pl;
              for (j = ip->ftheight-1; j >= 0; j--)
	        {
	          asm("bfclr %0@{%1:%2}"
	              : : "a" (ppl), "d" (ofs), "d" (ip->ftwidth));
	          ppl += bm->row_mod + bm->bytes_per_row; 
	        }
	      ofs += ip->ftwidth;
	    }
	  sy++;
	}
    }
}

/* Note: sx is only relevant for SCROLL_LEFT or SCROLL_RIGHT.  */
static void
scroll8(ip, sy, sx, count, dir)
        register struct ite_softc *ip;
        register int sy;
        int dir, sx, count;
{
  bmap_t *bm = ((ipriv_t *)ip->priv)->view->bitmap;
  u_char *pl = ((ipriv_t *)ip->priv)->row_ptr[sy];

  if (dir == SCROLL_UP) 
    {
      int dy = sy - count;
      int height = ip->bottom_margin - sy + 1;
      int i;

      /*FIX: add scroll bitmap call */
        cursor32(ip, ERASE_CURSOR);
	scrollbmap (bm, 0, dy*ip->ftheight,
		       bm->bytes_per_row >> 3, (ip->bottom_margin-dy+1)*ip->ftheight,
		       0, -(count*ip->ftheight), 0x1);
/*	if (ip->cursory <= bot || ip->cursory >= dy) {
	    ip->cursory -= count;
	} */
    }
  else if (dir == SCROLL_DOWN) 
    {
      int dy = sy + count;
      int height = ip->bottom_margin - dy + 1;
      int i;

      /* FIX: add scroll bitmap call */
        cursor32(ip, ERASE_CURSOR);
	scrollbmap (bm, 0, sy*ip->ftheight,
		       bm->bytes_per_row >> 3, (ip->bottom_margin-sy+1)*ip->ftheight,
		       0, count*ip->ftheight, 0x1);
/*	if (ip->cursory <= bot || ip->cursory >= sy) {
	    ip->cursory += count;
	} */
    }
  else if (dir == SCROLL_RIGHT) 
    {
      int sofs = (ip->cols - count) * ip->ftwidth;
      int dofs = (ip->cols) * ip->ftwidth;
      int i, j;

      cursor32(ip, ERASE_CURSOR);
      for (j = ip->ftheight-1; j >= 0; j--)
	{
	  int sofs2 = sofs, dofs2 = dofs;
	  for (i = (ip->cols - (sx + count))-1; i >= 0; i--)
	    {
	      int t;
	      sofs2 -= ip->ftwidth;
	      dofs2 -= ip->ftwidth;
	      asm("bfextu %1@{%2:%3},%0"
	          : "=d" (t)
		  : "a" (pl), "d" (sofs2), "d" (ip->ftwidth));
	      asm("bfins %3,%0@{%1:%2}"
	          : : "a" (pl), "d" (dofs2), "d" (ip->ftwidth), "d" (t));
	    }
	  pl += bm->row_mod + bm->bytes_per_row; 
	}
    }
  else /* SCROLL_LEFT */
    {
      int sofs = (sx) * ip->ftwidth;
      int dofs = (sx - count) * ip->ftwidth;
      int i, j;

      cursor32(ip, ERASE_CURSOR);
      for (j = ip->ftheight-1; j >= 0; j--)
	{
	  int sofs2 = sofs, dofs2 = dofs;
	  for (i = (ip->cols - sx)-1; i >= 0; i--)
	    {
	      int t;
	      asm("bfextu %1@{%2:%3},%0"
	          : "=d" (t)
		  : "a" (pl), "d" (sofs2), "d" (ip->ftwidth));
	      asm("bfins %3,%0@{%1:%2}"
	          : : "a" (pl), "d" (dofs2), "d" (ip->ftwidth), "d" (t));
	      sofs2 += ip->ftwidth;
	      dofs2 += ip->ftwidth;
	    }
	  pl += bm->row_mod + bm->bytes_per_row; 
	}
    }		
}

void 
scrollbmap (bmap_t *bm, u_short x, u_short y, u_short width, u_short height, short dx, short dy, u_char mask)
{
    u_short depth = bm->depth; 
    u_short lwpr = bm->bytes_per_row >> 2;
    if (dx) {
    	/* FIX: */ panic ("delta x not supported in scroll bitmap yet.");
    } 
    if (bm->flags & BMF_INTERLEAVED) {
	height *= depth;
	depth = 1;
    }
    if (dy == 0) {
        return;
    }
    if (dy > 0) {
    	int i;
    	for (i=0; i < depth && mask; i++, mask >>= 1) {
    	    if (0x1 & mask) {
	    	u_long *pl = (u_long *)bm->plane[i];
		u_long *src_y = pl + (lwpr*y);
		u_long *dest_y = pl + (lwpr*(y+dy));
		u_long count = lwpr*(height-dy);
		u_long *clr_y = src_y;
		u_long clr_count = dest_y - src_y;
		u_long bc, cbc;
		
		src_y += count - 1;
		dest_y += count - 1;

		bc = count >> 4;
		count &= 0xf;
		
		while (bc--) {
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		    *dest_y-- = *src_y--; *dest_y-- = *src_y--;
		}
		while (count--) {
		    *dest_y-- = *src_y--;
		}

		cbc = clr_count >> 4;
		clr_count &= 0xf;

		while (cbc--) {
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		}
		while (clr_count--) {
		    *clr_y++ = 0;
		}
    	    }
	}
    } else if (dy < 0) {
    	int i;
    	for (i=0; i < depth && mask; i++, mask >>= 1) {
    	    if (0x1 & mask) {
    		u_long *pl = (u_long *)bm->plane[i];
    		u_long *src_y = pl + (lwpr*(y-dy));
    		u_long *dest_y = pl + (lwpr*y); 
		long count = lwpr*(height + dy);
		u_long *clr_y = dest_y + count;
		u_long clr_count = src_y - dest_y;
		u_long bc, cbc;

		bc = count >> 4;
		count &= 0xf;
		
		while (bc--) {
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		    *dest_y++ = *src_y++; *dest_y++ = *src_y++;
		}
		while (count--) {
		    *dest_y++ = *src_y++;
		}

		cbc = clr_count >> 4;
		clr_count &= 0xf;

		while (cbc--) {
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		    *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0; *clr_y++ = 0;
		}
		while (clr_count--) {
		    *clr_y++ = 0;
		}
	    }
	}
    }
}

#endif /* NGRFCC */

/*	$NetBSD: ite_rt.c,v 1.14 1995/04/08 05:30:58 chopps Exp $	*/

/*
 * Copyright (c) 1993 Markus Wild
 * Copyright (c) 1993 Lutz Vieweg
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
#include "grfrt.h"
#if NGRFRT > 0

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
#include <amiga/dev/itevar.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_rtreg.h>

int retina_console = 1;

void retina_cursor __P((struct ite_softc *,int));
void retina_scroll __P((struct ite_softc *,int,int,int,int));
void retina_deinit __P((struct ite_softc *));
void retina_clear __P((struct ite_softc *,int,int,int,int));
void retina_putc __P((struct ite_softc *,int,int,int,int));
void retina_init __P((struct ite_softc *));

/*
 * this function is called from grf_rt to init the grf_softc->g_conpri
 * field each time a retina is attached.
 */
int
grfrt_cnprobe()
{
	static int done;
	int rv;

	if (retina_console && done == 0)
		rv = CN_INTERNAL;
	else
		rv = CN_NORMAL;
	done = 1;
	return(rv);
}

/* 
 * init the required fields in the grf_softc struct for a
 * grf to function as an ite.
 */
void
grfrt_iteinit(gp)
	struct grf_softc *gp;
{
	gp->g_iteinit = retina_init;
	gp->g_itedeinit = retina_deinit;
	gp->g_iteclear = retina_clear;
	gp->g_iteputc = retina_putc;
	gp->g_itescroll = retina_scroll;
	gp->g_itecursor = retina_cursor;
}

void
retina_init(ip)
	struct ite_softc *ip;
{
	struct MonDef *md;

	ip->priv = ip->grf->g_data;
	md = (struct MonDef *) ip->priv;
  
	ip->cols = md->TX;
	ip->rows = md->TY;
}


void retina_cursor(struct ite_softc *ip, int flag)
{
      volatile u_char *ba = ip->grf->g_regkva;
      
      if (flag == ERASE_CURSOR)
        {
	  /* disable cursor */
          WCrt (ba, CRT_ID_CURSOR_START, RCrt (ba, CRT_ID_CURSOR_START) | 0x20);
        }
      else
	{
	  int pos = ip->curx + ip->cury * ip->cols;

	  /* make sure to enable cursor */
          WCrt (ba, CRT_ID_CURSOR_START, RCrt (ba, CRT_ID_CURSOR_START) & ~0x20);

	  /* and position it */
	  WCrt (ba, CRT_ID_CURSOR_LOC_HIGH, (u_char) (pos >> 8));
	  WCrt (ba, CRT_ID_CURSOR_LOC_LOW,  (u_char) pos);

	  ip->cursorx = ip->curx;
	  ip->cursory = ip->cury;
	}
}



static void screen_up (struct ite_softc *ip, int top, int bottom, int lines)
{	
	volatile u_char * ba = ip->grf->g_regkva;
	volatile u_char * fb = ip->grf->g_fbkva;
	const struct MonDef * md = (struct MonDef *) ip->priv;
#ifdef BANKEDDEVPAGER
	int bank;
#endif

	/* do some bounds-checking here.. */
	if (top >= bottom)
	  return;
	  
	if (top + lines >= bottom)
	  {
	    retina_clear (ip, top, 0, bottom - top, ip->cols);
	    return;
	  }


#ifdef BANKEDDEVPAGER
	/* make sure to save/restore active bank (and if it's only
	   for tests of the feature in text-mode..) */
	bank = (RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO) 
		| (RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI) << 8));
#endif

	/* the trick here is to use a feature of the NCR chip. It can
	   optimize data access in various read/write modes. One of
	   the modes is able to read/write from/to different zones.

	   Thus, by setting the read-offset to lineN, and the write-offset
	   to line0, we just cause read/write cycles for all characters
	   up to the last line, and have the chip transfer the data. The
	   `addqb' are the cheapest way to cause read/write cycles (DONT
	   use `tas' on the Amiga!), their results are completely ignored
	   by the NCR chip, it just replicates what it just read. */
	
		/* write to primary, read from secondary */
	WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA, (RSeq(ba, SEQ_ID_EXTENDED_MEM_ENA) & 0x1f) | 0 ); 
		/* clear extended chain4 mode */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR, RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) & ~0x02);  
	
		/* set write mode 1, "[...] data in the read latches is written
		   to memory during CPU memory write cycles. [...]" */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 1); 
	
	{
		/* write to line TOP */	
		long toploc = top * (md->TX / 16);
		WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, ((unsigned char)toploc)); 
		WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, ((unsigned char)(toploc >> 8))); 
	}
	{
		/* read from line TOP + LINES */
		long fromloc = (top+lines) * (md->TX / 16);
		WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO, ((unsigned char)fromloc)) ; 
		WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI, ((unsigned char)(fromloc >> 8))) ; 
	}
	{
		unsigned char * p = (unsigned char *) fb;
		/* transfer all characters but LINES lines, unroll by 16 */
		short x = (1 + bottom - (top + lines)) * (md->TX / 16) - 1;
		do {
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@+" : "=a" (p) : "0" (p)); 
		} while (x--);
	}
	
		/* reset to default values */
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI, 0); 
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO, 0); 
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, 0); 
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, 0); 
		/* write mode 0 */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 0);
		/* extended chain4 enable */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR , RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) | 0x02);  
		/* read/write to primary on A0, secondary on B0 */
	WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA, (RSeq(ba, SEQ_ID_EXTENDED_MEM_ENA) & 0x1f) | 0x40 ); 
	
	
	/* fill the free lines with spaces */
	
	{  /* feed latches with value */
		unsigned short * f = (unsigned short *) fb;
		
		f += (1 + bottom - lines) * md->TX * 2;
		*f = 0x2010;
		{
			volatile unsigned short dummy = *((volatile unsigned short *)f);
		}
	}
	
	   /* clear extended chain4 mode */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR, RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) & ~0x02);  
	   /* set write mode 1, "[...] data in the read latches is written
	      to memory during CPU memory write cycles. [...]" */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 1); 
	
	{
		unsigned long * p = (unsigned long *) fb;
		short x = (lines * (md->TX/16)) - 1;
		const unsigned long dummyval = 0;
		
		p += (1 + bottom - lines) * (md->TX/4);
		
		do {
			*p++ = dummyval;
			*p++ = dummyval;
			*p++ = dummyval;
			*p++ = dummyval;
		} while (x--);
	}
	
	   /* write mode 0 */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 0);
	   /* extended chain4 enable */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR , RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) | 0x02);  

#ifdef BANKEDDEVPAGER
	/* restore former bank */
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, (unsigned char) bank);
	bank >>= 8;
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, (unsigned char) bank);
#endif
};

static void screen_down (struct ite_softc *ip, int top, int bottom, int lines)
{	
	volatile u_char * ba = ip->grf->g_regkva;
	volatile u_char * fb = ip->grf->g_fbkva;
	const struct MonDef * md = (struct MonDef *) ip->priv;
#ifdef BANKEDDEVPAGER
	int bank;
#endif

	/* do some bounds-checking here.. */
	if (top >= bottom)
	  return;
	  
	if (top + lines >= bottom)
	  {
	    retina_clear (ip, top, 0, bottom - top, ip->cols);
	    return;
	  }

#ifdef BANKEDDEVPAGER
	/* make sure to save/restore active bank (and if it's only
	   for tests of the feature in text-mode..) */
	bank = (RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO) 
		| (RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI) << 8));
#endif
	/* see screen_up() for explanation of chip-tricks */

		/* write to primary, read from secondary */
	WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA, (RSeq(ba, SEQ_ID_EXTENDED_MEM_ENA) & 0x1f) | 0 ); 
		/* clear extended chain4 mode */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR, RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) & ~0x02);  
	
		/* set write mode 1, "[...] data in the read latches is written
		   to memory during CPU memory write cycles. [...]" */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 1); 
	
	{
		/* write to line TOP + LINES */	
		long toloc = (top + lines) * (md->TX / 16);
		WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, ((unsigned char)toloc)); 
		WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, ((unsigned char)(toloc >> 8))); 
	}
	{
		/* read from line TOP */
		long fromloc = top * (md->TX / 16);
		WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO, ((unsigned char)fromloc)); 
		WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI, ((unsigned char)(fromloc >> 8))) ; 
	}
	
	{
		unsigned char * p = (unsigned char *) fb;
		short x = (1 + bottom - (top + lines)) * (md->TX / 16) - 1;
		p += (1 + bottom - (top + lines)) * md->TX;
		do {
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
			asm volatile("addqb #1,%0@-" : "=a" (p) : "0" (p)); 
		} while (x--);
	}
	
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, 0); 
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, 0); 
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI, 0); 
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO, 0); 
	
		/* write mode 0 */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 0);
		/* extended chain4 enable */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR , RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) | 0x02);  
		/* read/write to primary on A0, secondary on B0 */
	WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA, (RSeq(ba, SEQ_ID_EXTENDED_MEM_ENA) & 0x1f) | 0x40 ); 
	
	/* fill the free lines with spaces */
	
	{  /* feed latches with value */
		unsigned short * f = (unsigned short *) fb;
		
		f += top * md->TX * 2;
		*f = 0x2010;
		{
			volatile unsigned short dummy = *((volatile unsigned short *)f);
		}
	}
	
	   /* clear extended chain4 mode */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR, RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) & ~0x02);  
	   /* set write mode 1, "[...] data in the read latches is written
	      to memory during CPU memory write cycles. [...]" */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 1); 
	
	{
		unsigned long * p = (unsigned long *) fb;
		short x = (lines * (md->TX/16)) - 1;
		const unsigned long dummyval = 0;
		
		p += top * (md->TX/4);
		
		do {
			*p++ = dummyval;
			*p++ = dummyval;
			*p++ = dummyval;
			*p++ = dummyval;
		} while (x--);
	}
	
	   /* write mode 0 */
	WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 0);
	   /* extended chain4 enable */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR , RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) | 0x02);  
	
#ifdef BANKEDDEVPAGER
	/* restore former bank */
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, (unsigned char) bank);
	bank >>= 8;
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, (unsigned char) bank);
#endif
};

void retina_deinit(struct ite_softc *ip)
{
  ip->flags &= ~ITE_INITED;
}


void retina_putc(struct ite_softc *ip, int c, int dy, int dx, int mode)
{
	volatile u_char * ba = ip->grf->g_regkva;
	volatile u_char * fb = ip->grf->g_fbkva;
	register u_char attr;
	
	attr = (mode & ATTR_INV) ? 0x21 : 0x10;
	if (mode & ATTR_UL)     attr  = 0x01;	/* ???????? */
	if (mode & ATTR_BOLD)   attr |= 0x08;
	if (mode & ATTR_BLINK)	attr |= 0x80;
	
	fb += 4 * (dy * ip->cols + dx);
	*fb++ = c; *fb = attr;
}

void retina_clear(struct ite_softc *ip, int sy, int sx, int h, int w)
{
	volatile u_char * ba = ip->grf->g_regkva;
	u_short * fb = (u_short *) ip->grf->g_fbkva;
	short x;
	const u_short fillval = 0x2010;
	/* could probably be optimized just like the scrolling functions !! */
	fb += 2 * (sy * ip->cols + sx);
	while (h--)
	  {
	    for (x = 2 * (w - 1); x >= 0; x -= 2)
	      fb[x] = fillval;
	    fb += 2 * ip->cols;
	  }
}

/*
 * RETINA_SPEED_HACK code seems to work on some boards and on others
 * it causes text to smear horizontally
 */
void
retina_scroll(struct ite_softc *ip, int sy, int sx, int count, int dir)
{
	register int height, dy, i;
	volatile u_char *ba;
	u_long *fb;

	ba = ip->grf->g_regkva;
	fb = (u_long *)ip->grf->g_fbkva;
	
	retina_cursor(ip, ERASE_CURSOR);

	if (dir == SCROLL_UP) {
#ifdef	RETINA_SPEED_HACK
		screen_up(ip, sy - count, ip->bottom_margin, count);
#else
		bcopy(fb + sy * ip->cols, fb + (sy - count) * ip->cols,
		    4 * (ip->bottom_margin - sy + 1) * ip->cols);
		retina_clear(ip, ip->bottom_margin + 1 - count, 0, count,
		    ip->cols);
#endif
	} else if (dir == SCROLL_DOWN) {
#ifdef	RETINA_SPEED_HACK
		screen_down(ip, sy, ip->bottom_margin, count);
#else
		bcopy(fb + sy * ip->cols, fb + (sy + count) * ip->cols,
		    4 * (ip->bottom_margin - sy - count + 1) * ip->cols);
		retina_clear(ip, sy, 0, count, ip->cols);
#endif
	} else if (dir == SCROLL_RIGHT) {
		bcopy(fb + sx + sy * ip->cols, fb + sx + sy * ip->cols + count,
		    4 * (ip->cols - (sx + count)));
		retina_clear(ip, sy, sx, 1, count);
	} else {
		bcopy(fb + sx + sy * ip->cols, fb + sx - count + sy * ip->cols,
		    4 * (ip->cols - sx));
		retina_clear(ip, sy, ip->cols - count, 1, count);
	}
#ifndef	RETINA_SPEED_HACK
	retina_cursor(ip, !ERASE_CURSOR);
#endif
}

#endif /* NGRFRT */

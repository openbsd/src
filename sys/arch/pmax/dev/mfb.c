/*	$NetBSD: mfb.c,v 1.23 1997/04/19 08:25:31 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)mfb.c	8.1 (Berkeley) 6/10/93
 */

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *  devGraphics.c --
 *
 *     	This file contains machine-dependent routines for the graphics device.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.  
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devGraphics.c,
 *	v 9.2 90/02/13 22:16:24 shirriff Exp  SPRITE (DECWRL)";
 */

#include "fb.h"
#include "mfb.h"
#if NMFB > 0
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>

#include <machine/cpuregs.h>		/* mips cached->uncached */
#include <machine/pmioctl.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>
#include <pmax/dev/cfbvar.h>		/* XXX dev/tc ? */ 

#include <pmax/pmax/pmaxtype.h>

#include <pmax/dev/mfbreg.h>
#include <pmax/dev/fbreg.h>


/*
 * These need to be mapped into user space.
 */
struct fbuaccess mfbu;
struct pmax_fbtty mfbfb;
struct fbinfo	mfbfi;	/*XXX*/

extern int pmax_boardtype;

/*
 * Forward references.
 */
#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [CMAP_BITS];		/* colormap for console... */


void mfbPosCursor  __P((struct fbinfo *fi, int x, int y));



int	mfbinit __P((struct fbinfo *fi, caddr_t mfbaddr, int unit, int silent));

#if 1	/* these  go away when we use the abstracted-out chip drivers */
static void mfbLoadCursor __P((struct fbinfo *fi, u_short *ptr));
static void mfbRestoreCursorColor __P((struct fbinfo *fi));
static void mfbCursorColor  __P((struct fbinfo *fi, u_int *color));

static void mfbInitColorMapBlack __P((struct fbinfo *fi, int blackpix));
static void mfbInitColorMap __P((struct fbinfo *fi));
static int mfbLoadColorMap __P((struct fbinfo *fi, caddr_t mapbits,
				int index, int count));
static int mfbLoadColorMapNoop __P((struct fbinfo *fi, caddr_t mapbits,
				int index, int count));
#endif /* 0 */

/* new-style raster-cons "driver" methods */

int mfbGetColorMap __P((struct fbinfo *fi, caddr_t, int, int));


static int bt455_video_on __P((struct fbinfo *));
static int bt455_video_off __P((struct fbinfo *));

static void  bt431_init __P((bt431_regmap_t *regs));
static void  bt431_select_reg __P((bt431_regmap_t *regs, int regno));
static void  bt431_write_reg __P((bt431_regmap_t *regs, int regno, int val));

#ifdef notused
static u_char bt431_read_reg __P((bt431_regmap_t *regs, int regno));
#endif



/*
 * old pmax-framebuffer hackery
 */
extern u_short defCursor[32];


/*
 * "driver" (member functions) for the raster-console (rcons) pseudo-device.
 */
struct fbdriver mfb_driver = {
	bt455_video_on,
	bt455_video_off,
	mfbInitColorMap,
	mfbGetColorMap,
	mfbLoadColorMapNoop, /*LoadColorMap,*/
	mfbPosCursor,
	mfbLoadCursor,
	mfbCursorColor
};


/*
 * Register offsets
 */
#define	MFB_OFFSET_VRAM		0x200000	/* from module's base */
#define MFB_OFFSET_BT431	0x180000	/* Bt431 registers */
#define MFB_OFFSET_BT455	0x100000	/* Bt455 registers */
#define MFB_OFFSET_IREQ		0x080000	/* Interrupt req. control */
#define MFB_OFFSET_ROM		0x0		/* Diagnostic ROM */
#define MFB_FB_SIZE		0x200000	/* frame buffer size */


/*
 * Autoconfiguration data for config.new.
 * Use static-sized softc until config.old and old autoconfig
 * code is completely gone.
 */

int mfbmatch __P((struct device *, void *, void *));
void mfbattach __P((struct device *, struct device *, void *));
int mfb_intr __P((void *sc));

struct cfattach mfb_ca = {
	sizeof(struct fbinfo), mfbmatch, mfbattach
};

struct cfdriver mfb_cd = {
	NULL, "mfb", DV_DULL
};

int
mfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

#ifdef FBDRIVER_DOES_ATTACH
	/* leave configuration  to the fb driver */
	return 0;
#endif

	/* make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "PMAG-AA "))
		return (0);

	return (1);
}

void
mfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_attach_args *ta = aux;
	caddr_t mfbaddr = (caddr_t) ta->ta_addr;
	int unit = self->dv_unit;
	struct fbinfo *fi = (struct fbinfo *) self;

#ifdef notyet
	struct fbinfo *fi = &mfbfi;

	/* if this is the console, it's already configured. */
	if (ta->ta_cookie == cons_slot)
		return;	/* XXX patch up f softc pointer */
#endif

	if (!mfbinit(fi, mfbaddr, unit, 0))
		return;

	/*
	 * 3MIN does not mask un-established TC option interrupts,
	 * so establish a handler.
	 * XXX Should store cmap updates in softc and apply in the
	 * interrupt handler, which interrupts during vertical-retrace.
	 */
	if (pmax_boardtype == DS_3MIN) {
		tc_intr_establish(parent, (void*)ta->ta_cookie, TC_IPL_NONE,
					mfb_intr, fi);
	}
	printf("\n");
}


/*
 * Initialization
 */
int
mfbinit(fi, mfbaddr, unit, silent)
	struct fbinfo *fi;
	caddr_t mfbaddr;
	int unit;
	int silent;
{

	register int isconsole = 0;

	/*
	 * If this device is being intialized as the console, malloc()
	 * is not yet up and we must use statically-allocated space.
	 */
	if (fi == NULL) {
		fi = &mfbfi;	/* XXX */
		fi->fi_cmap_bits = (caddr_t)cmap_bits;
		isconsole = 1;
	}
	else {
		fi->fi_cmap_bits = malloc(CMAP_BITS, M_DEVBUF, M_NOWAIT);
		if (fi->fi_cmap_bits == NULL) {
			printf("mfb%d: no memory for cmap\n", unit);
			return (0);
		}
	}

	/* check for no frame buffer */
	if (badaddr(mfbaddr, 4)) {
		printf("mfb: bad address 0x%p\n", mfbaddr);
		return (0);
	}

	/* Fill in main frame buffer info struct. */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)(mfbaddr + MFB_OFFSET_VRAM);
	fi->fi_pixelsize = 2048 * 1024;
	fi->fi_base = (caddr_t)(mfbaddr + MFB_OFFSET_BT431);
	fi->fi_vdac = (caddr_t)(mfbaddr + MFB_OFFSET_BT455);
	fi->fi_size = (fi->fi_pixels + MFB_FB_SIZE) - fi->fi_base;
	fi->fi_linebytes = 2048;	/* inter-line stride (blitting) */
	fi->fi_driver = &mfb_driver;
	fi->fi_blanked = 0;

	/* Fill in Frame Buffer Type struct. */
	fi->fi_type.fb_boardtype = PMAX_FBTYPE_MFB;
	fi->fi_type.fb_width = 1280;	/* visible screen pixels */
	fi->fi_type.fb_height = 1024;
	fi->fi_type.fb_depth = 8;
	fi->fi_type.fb_cmsize = 0;
	fi->fi_type.fb_size = MFB_FB_SIZE;

	/*
	 * Reset the video chip.
	 */
	bt431_init ((bt431_regmap_t *) fi->fi_base);
	mfbLoadCursor(fi, defCursor); /*XXX*/ /* Is this necessary? */


	/*
	 * qvss/pm-style mmap()ed event queue compatibility glue
	 */

	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MIPS_PHYS_TO_KSEG1(MIPS_KSEG0_TO_PHYS(&mfbu));

	/* This is glass-tty state but it's in the shared structure. Ick. */
	fi->fi_fbu->scrInfo.max_row = 67;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/*
	 * Initialize old-style pmax glass-tty screen info.
	 */
	fi->fi_glasstty = &mfbfb;

	/*
	 * Initialize the color map, the screen, and the mouse.
	 */
	if (tb_kbdmouseconfig(fi))
		return (0);

	/* white-on-black if console, black-on-black otherwise. */
	mfbInitColorMapBlack(fi, isconsole);


	/*
	 * Connect to the raster-console pseudo-driver.
	 */
	fbconnect("PMAG-AA", fi, silent);

#ifdef 	fpinitialized
	fp->initialized = 1;
#endif
	return (1);
}

static u_char	cursor_RGB[6];	/* cursor color 2 & 3 */

/*
 * There are actually 2 Bt431 cursor sprite chips that each generate 1 bit
 * of each cursor pixel for a 2bit 64x64 cursor sprite. The corresponding
 * registers for these two chips live in adjacent bytes of the shorts that
 * are defined in bt431_regmap_t.
 */
static void
mfbLoadCursor(fi, cursor)
	struct fbinfo *fi;
	u_short *cursor;
{
	register int i, j, k, pos;
	register u_short ap, bp, out;
	register bt431_regmap_t *regs;

	regs = (bt431_regmap_t *)(fi -> fi_base);

	/*
	 * Fill in the cursor sprite using the A and B planes, as provided
	 * for the pmax.
	 * XXX This will have to change when the X server knows that this
	 * is not a pmax display. (ie. Not the Xcfbpmax server.)
	 */
	pos = 0;
	bt431_select_reg(regs, BT431_REG_CRAM_BASE);
	for (k = 0; k < 16; k++) {
		ap = *cursor;
		bp = *(cursor + 16);
		j = 0;
		while (j < 2) {
			out = 0;
			for (i = 0; i < 8; i++) {
				out = (out << 1) | ((bp & 0x1) << 8) |
					(ap & 0x1);
				ap >>= 1;
				bp >>= 1;
			}
			BT431_WRITE_CMAP_AUTOI(regs, out);
			pos++;
			j++;
		}
		while (j < 8) {
			BT431_WRITE_CMAP_AUTOI(regs, 0);
			pos++;
			j++;
		}
		cursor++;
	}
	while (pos < 512) {
		BT431_WRITE_CMAP_AUTOI(regs, 0);
		pos++;
	}
}

/* Restore the color of the cursor. */

void
mfbRestoreCursorColor (fi)
	struct fbinfo *fi;
{
	bt455_regmap_t *regs = (bt455_regmap_t *)(fi -> fi_vdac);
	u_char cm [3];
	u_char fg;

	if (cursor_RGB[0] || cursor_RGB[1] || cursor_RGB[2])
		cm [0] = cm [1] = cm [2] = 0xff;
	else
		cm [0] = cm [1] = cm [2] = 0;
	mfbLoadColorMap(fi, cm, 8, 1);
	mfbLoadColorMap(fi, cm, 9, 1);

	if (cursor_RGB[3] || cursor_RGB[4] || cursor_RGB[5])
		fg = 0xf;
	else
		fg = 0;
	regs->addr_ovly = fg;
	wbflush();
	regs->addr_ovly = fg;
	wbflush();
	regs->addr_ovly = fg;
	wbflush();
}

/* Set the color of the cursor. */

void
mfbCursorColor(fi, color)
	struct fbinfo *fi;
	unsigned int color[];
{
	register int i;

	for (i = 0; i < 6; i++)
		cursor_RGB[i] = (u_char)(color[i] >> 8);

	mfbRestoreCursorColor (fi);
}

/* Position the cursor. */

void
mfbPosCursor(fi, x, y)
	struct fbinfo *fi;
	register int x, y;
{
	bt431_regmap_t *regs = (bt431_regmap_t *)(fi -> fi_base);

#ifdef MELLON
	if (y < 0)
	  y = 0;
	else if (y > fi -> fi_type.fb_width - fi -> fi_cursor.width - 1)
	  y = fi -> fi_type.fb_width - fi -> fi_cursor.width - 1;
	if (x < 0)
	  x = 0;
	else if (x > fi -> fi_type.fb_height - fi -> fi_cursor.height - 1)
	  x = fi -> fi_type.fb_height - fi -> fi_cursor.height - 1;

#else /* old-style pmax glass tty */
 	if (y < fi->fi_fbu->scrInfo.min_cur_y ||
	    y > fi->fi_fbu->scrInfo.max_cur_y)
		y = fi->fi_fbu->scrInfo.max_cur_y;
	if (x < fi->fi_fbu->scrInfo.min_cur_x ||
	    x > fi->fi_fbu->scrInfo.max_cur_x)
		x = fi->fi_fbu->scrInfo.max_cur_x;
#endif /* old-style pmax glass tty */

	fi -> fi_cursor.x = x;
	fi -> fi_cursor.y = y;

#define lo(v)	((v)&0xff)
#define hi(v)	(((v)&0xf00)>>8)

	/*
	 * Cx = x + D + H - P
	 *  P = 37 if 1:1, 52 if 4:1, 57 if 5:1
	 *  D = pixel skew between outdata and external data
	 *  H = pixels between HSYNCH falling and active video
	 *
	 * Cy = y + V - 32
	 *  V = scanlines between HSYNCH falling, two or more
	 *	clocks after VSYNCH falling, and active video
	 */

	bt431_write_reg(regs, BT431_REG_CXLO, lo(x + 360));
	BT431_WRITE_REG_AUTOI(regs, hi(x + 360));
	BT431_WRITE_REG_AUTOI(regs, lo(y + 36));
	BT431_WRITE_REG_AUTOI(regs, hi(y + 36));
}

/* Initialize the color map. */

static void
mfbInitColorMapBlack(fi, blackpix)
	struct fbinfo *fi;
	int blackpix;
{
	u_char rgb [3];
	register int i;

	blackpix = 1; /* XXX XXX XXX defeat screensave bug */

	if (blackpix)
		rgb [0] = rgb [1] = rgb [2] = 0xff;
	else
		rgb [0] = rgb [1] = rgb [2] = 0;

	mfbLoadColorMap(fi, (caddr_t)rgb, 0, 1);

	if (blackpix)
		rgb [0] = rgb [1] = rgb [2] = 0;
	else
		rgb [0] = rgb [1] = rgb [2] = 0xff;

	for (i = 1; i < 16; i++) {
		mfbLoadColorMap(fi, (caddr_t)rgb, i, 1);
	}

	for (i = 0; i < 3; i++) {
		cursor_RGB[i] = 0;
		cursor_RGB[i + 3] = 0xff;
	}
	mfbRestoreCursorColor (fi);
}

/* set colormap for open/close */
static void
mfbInitColorMap(fi)
	struct fbinfo *fi;
{
	mfbInitColorMapBlack(fi, fi->fi_open);
}

/* Load the color map. */

int
mfbLoadColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	bt455_regmap_t *regs = (bt455_regmap_t *)(fi -> fi_vdac);
	u_char *cmap_bits;
	u_char *cmap;
	int i;

	if (index > 15 || index < 0 || index + count > 15)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	BT455_SELECT_ENTRY(regs, index);
	for (i = 0; i < count; i++) {
		cmap [(i + index) * 3]
			= regs->addr_cmap_data = cmap_bits [i * 3] >> 4;
		wbflush();
		cmap [(i + index) * 3 + 1]
			= regs->addr_cmap_data = cmap_bits [i * 3 + 1] >> 4;
		wbflush();
		cmap [(i + index) * 3 + 2]
			= regs->addr_cmap_data = cmap_bits [i * 3 + 2] >> 4;
		wbflush();
	}
	return 0;
}

/* stub for driver */

int
mfbLoadColorMapNoop(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	return 0;
}

/* Get the color map. */

int
mfbGetColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	/*bt455_regmap_t *regs = (bt455_regmap_t *)(fi -> fi_vdac);*/
	u_char *cmap_bits;
	u_char *cmap;

	if (index > 15 || index < 0 || index + count > 15)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	bcopy (cmap, cmap_bits, count * 3);
	return 0;
}

/* Enable the video display. */

static int
bt455_video_on(fi)
	struct fbinfo *fi;
{
	register int i;
	bt455_regmap_t *regs = (bt455_regmap_t *)(fi -> fi_vdac);
	u_char *cmap;

	if (!fi -> fi_blanked)
		return 0;

	cmap = (u_char *)(fi -> fi_cmap_bits);

	/* restore old color map entry zero */
	BT455_SELECT_ENTRY(regs, 0);
	for (i = 0; i < 6; i++) {
		regs->addr_cmap_data = cmap [i];
		wbflush();
	}
	mfbRestoreCursorColor (fi);
	fi -> fi_blanked = 0;

	return 0;
}

/*
 * ----------------------------------------------------------------------------
 *
 * bt455_video_off
 *
 *	Disable the video display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The display is disabled.
 *
 * ----------------------------------------------------------------------------
 */
static int
bt455_video_off(fi)
	struct fbinfo *fi;
{
	register int i;
	u_char cursor_save [6];
	bt455_regmap_t *regs = (bt455_regmap_t *)(fi -> fi_vdac);
	u_char *cmap;

	if (fi -> fi_blanked)
		return 0;

	cmap = (u_char *)(fi -> fi_cmap_bits);

	bcopy (cursor_RGB, cursor_save, 6);

	/* Zap the cursor and the colormap... */
	BT455_SELECT_ENTRY(regs, 0);
	for (i = 0; i < 6; i++) {
		cursor_RGB[i] = 0;
		regs->addr_cmap_data = 0;
		wbflush();
	}
		
	mfbRestoreCursorColor (fi);

	bcopy (cursor_save, cursor_RGB, 6);
	fi -> fi_blanked = 1;

	return 0;
}

/*
 * Generic register access
 */
static void
bt431_select_reg(regs, regno)
	bt431_regmap_t *regs;
{
	regs->addr_lo = SET_VALUE(regno & 0xff);
	regs->addr_hi = SET_VALUE((regno >> 8) & 0xff);
	wbflush();
}

static void 
bt431_write_reg(regs, regno, val)
	bt431_regmap_t *regs;
{
	bt431_select_reg(regs, regno);
	regs->addr_reg = SET_VALUE(val);
	wbflush();
}

#ifdef notused
static u_char
bt431_read_reg(regs, regno)
	bt431_regmap_t *regs;
{
	bt431_select_reg(regs, regno);
	return (GET_VALUE(regs->addr_reg));
}
#endif


static void
bt431_init(regs)
	bt431_regmap_t *regs;
{

	/* use 4:1 input mux */
	bt431_write_reg(regs, BT431_REG_CMD,
			 BT431_CMD_CURS_ENABLE|BT431_CMD_OR_CURSORS|
			 BT431_CMD_4_1_MUX|BT431_CMD_THICK_1);

	/* home cursor */
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);

	/* no crosshair window */
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
	BT431_WRITE_REG_AUTOI(regs, 0x00);
}

/*
 * copied from cfb_intr
 */
int
mfb_intr(sc)
	void *sc;
{
	struct fbinfo *fi = /* XXX (struct fbinfo *)sc */ &mfbfi;
	volatile int junk;
	char *slot_addr = (((char *)fi->fi_base) - MFB_OFFSET_BT431);

	/* reset vertical-retrace interrupt by writing a dont-care */
	junk = *(volatile int*) (slot_addr + MFB_OFFSET_IREQ);
	*(volatile int*) (slot_addr + MFB_OFFSET_IREQ) = 0;

	return (0);
}

#endif /* NMFB */


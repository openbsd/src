/*	$NetBSD: pm.c,v 1.8.2.1 1995/10/17 00:19:23 jonathan Exp $	*/

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
 *	@(#)pm.c	8.1 (Berkeley) 6/10/93
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


#include <fb.h>
#include <pm.h>
#include <dc.h>
#if NPM > 0
#if NDC == 0
pm needs dc device
#else

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mman.h>

#include <vm/vm.h>

#include <sys/device.h>
#include <machine/autoconf.h>

#include <machine/machConst.h>
#include <machine/dc7085cons.h>
#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/pmax/kn01.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/cons.h>

#include <pmax/dev/fbreg.h>

#include <pmax/dev/pmreg.h>
#include <pmax/dev/bt478.h>

/*
 * These need to be mapped into user space.
 */
struct fbuaccess pmu;
static u_short curReg;		/* copy of PCCRegs.cmdr since it's read only */

/*
 * rcons methods and globals.
 */
struct pmax_fbtty pmfb;
struct fbinfo	pmfi;		/*XXX*/

/*
 * Forward references.
 */
extern void pmScreenInit __P((struct fbinfo *fi));
static void pmLoadCursor __P((struct fbinfo *fi, u_short *ptr));
void pmPosCursor __P((struct fbinfo *fi, int x, int y));

#ifdef notyet /* these should be removed */
static void pmRestoreCursorColor __P(());

#endif
void bt478CursorColor __P((struct fbinfo *fi, u_int *color));
void bt478InitColorMap __P((struct fbinfo *fi));

static void pmLoadColorMap __P ((ColorMap *ptr));	/*XXX*/


int pminit __P((struct fbinfo *fi, int unit, int silent));

static int pm_video_on __P ((struct fbinfo *));
static int pm_video_off __P ((struct fbinfo *));
int bt478LoadColorMap __P ((struct fbinfo *, caddr_t, int, int));
int bt478GetColorMap __P ((struct fbinfo *, caddr_t, int, int));


#if 0
static void pmVDACInit();
void pmKbdEvent(), pmMouseEvent(), pmMouseButtons();
#endif

/* pm framebuffers are only found in {dec,vax}station 3100s with dc7085s */

extern void dcPutc();
extern void (*dcDivertXInput)();
extern void (*dcMouseEvent)();
extern void (*dcMouseButtons)();
extern int pmax_boardtype;
extern u_short defCursor[32];

void genConfigMouse(), genDeconfigMouse();
void genKbdEvent(), genMouseEvent(), genMouseButtons();

extern void pmEventQueueInit __P((pmEventQueue *qe));

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [NPM * CMAP_BITS];	/* One colormap per pm... */


/*
 * Autoconfiguration data for config.new.
 * Use static-sized softc until config.old and old autoconfig
 * code is completely gone.
 */

int pmmatch __P((struct device *, void *, void *));
void pmattach __P((struct device *, struct device *, void *));

struct cfdriver pmcd = {
	NULL, "pm", pmmatch, pmattach, DV_DULL, sizeof(struct device), 0
};

/* new-style raster-cons "driver" methods */

struct fbdriver pm_driver = {
	pm_video_on,
	pm_video_off,
	bt478InitColorMap,
	bt478GetColorMap,
	bt478LoadColorMap,
	pmPosCursor,
	pmLoadCursor,
	bt478CursorColor,
};

int
pmmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	static int npms = 1;
	caddr_t pmaddr = BUS_CVTADDR(ca);


	/* make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "pm"))
		return (0);

	if (badaddr(pmaddr, 4))
		return (0);

#ifdef notyet
	/* if it can't have the one mentioned, reject it */
	if (cf->cf_unit >= npms)
		return (0);
#endif
	return (1);
}

void
pmattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	caddr_t pmaddr = BUS_CVTADDR(ca);

	if (!pminit(&pmfi, 0, 0))
		return;

	/* no interrupts for PM */
	/*BUS_INTR_ESTABLISH(ca, sccintr, self->dv_unit);*/
	return;
}


/*
 * Test to see if device is present.
 * Return true if found and initialized ok.
 */
pminit(fi, unit, silent)
	struct fbinfo *fi;
	int unit;
	int silent;
{
	register PCCRegs *pcc = (PCCRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_PCC);

	if (fi == 0) fi = &pmfi;

	/* Set address of frame buffer... */
	fi->fi_pixels = (caddr_t)MACH_PHYS_TO_UNCACHED(KN01_PHYS_FBUF_START);

	/* Fill in the stuff that differs from monochrome to color. */
	if (*(volatile u_short *)MACH_PHYS_TO_UNCACHED(KN01_SYS_CSR) &
	    KN01_CSR_MONO) {
		/* check for no frame buffer */
		if (badaddr((char *)fi->fi_pixels, 4))
			return (0);

		fi->fi_type.fb_depth = 1;
		fi->fi_type.fb_cmsize = 0;
		fi->fi_type.fb_boardtype = PMAX_FBTYPE_PM_MONO;
		fi->fi_type.fb_size = 0x40000;
		fi->fi_linebytes = 256;
	} else {
		fi->fi_type.fb_depth = 8;
		fi->fi_type.fb_cmsize = 256;
		fi->fi_type.fb_boardtype = PMAX_FBTYPE_PM_COLOR;
		fi->fi_type.fb_size = 0x100000;
		fi->fi_linebytes = 1024;
	}

	/* Fill in main frame buffer info struct. */

	fi->fi_driver = &pm_driver;
	fi->fi_pixelsize =
		((fi->fi_type.fb_depth == 1) ? 1024 / 8 : 1024) * 864;
	fi->fi_unit = unit;
	fi->fi_base = (caddr_t)pcc;
	fi->fi_vdac = (caddr_t)MACH_PHYS_TO_UNCACHED(KN01_SYS_VDAC);
	fi->fi_blanked = 0;
	fi->fi_cmap_bits = (caddr_t)&cmap_bits [CMAP_BITS * unit];

	fi->fi_type.fb_width = 1024;
	fi->fi_type.fb_height = 864;


	/*
	 * compatibility glue
	 */
	fi->fi_glasstty = &pmfb;


	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MACH_PHYS_TO_UNCACHED(MACH_CACHED_TO_PHYS(&pmu));
	fi->fi_glasstty->KBDPutc = dcPutc;
	fi->fi_glasstty->kbddev = makedev(DCDEV, DCKBD_PORT);

	if (fi->fi_type.fb_depth == 1) {
		/* check for no frame buffer */
		if (badaddr((char *)fi->fi_pixels, 4))
			return (0);
	}

	/*
	 * Initialize the screen.
	 */
	pcc->cmdr = PCC_FOPB | PCC_VBHI;

	/*
	 * Initialize the cursor register.
	 */
	pcc->cmdr = curReg = PCC_ENPA | PCC_ENPB;

	/*
	 * Initialize the color map, the screen, and the mouse.
	 */
	bt478init(&pmfi);

	/*
	 * Initialize old-style pmax screen info.
	 */
	fi->fi_fbu->scrInfo.max_row = 56;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/* These are non-zero on the kn01 framebuffer. Why? */
	fi->fi_fbu->scrInfo.min_cur_x = -15;
	fi->fi_fbu->scrInfo.min_cur_y = -15;


#ifdef notanymore
	bt478InitColorMap(&pmfi);	/* done inside bt478init() */
#endif

	/*
	 * Connect to the raster-console pseudo-driver.
	 */
	fi->fi_glasstty = &pmfb; /*XXX*/
	fbconnect((fi->fi_type.fb_depth == 1) ? "KN01 mfb" : "KN01 cfb",
		  fi, silent);


#ifdef fpinitialized
	fp->initialized = 1;
#endif
	return (1);
}


static u_char	bg_RGB[3];	/* background color for the cursor */
static u_char	fg_RGB[3];	/* foreground color for the cursor */


/*
 * ----------------------------------------------------------------------------
 *
 * pmLoadCursor --
 *
 *	Routine to load the cursor Sprite pattern.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor is loaded into the hardware cursor.
 *
 * ----------------------------------------------------------------------------
 */
static void
pmLoadCursor(fi, cur)
	struct fbinfo *fi;
	unsigned short *cur;
{
	register PCCRegs *pcc = (PCCRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_PCC);
	register int i;

	curReg |= PCC_LODSA;
	pcc->cmdr = curReg;
	for (i = 0; i < 32; i++) {
		pcc->memory = cur[i];
		MachEmptyWriteBuffer();
	}
	curReg &= ~PCC_LODSA;
	pcc->cmdr = curReg;
}

#if 0
/*
 * ----------------------------------------------------------------------------
 *
 * pmRestoreCursorColor --
 *
 *	Routine to restore the color of the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
static void
pmRestoreCursorColor(fi)
	struct fbinfo *fi;
{
	register VDACRegs *vdac = (VDACRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_VDAC);
	register int i;

	vdac->overWA = 0x04;
	MachEmptyWriteBuffer();
	for (i = 0; i < 3; i++) {  
		vdac->over = bg_RGB[i];
		MachEmptyWriteBuffer();
	}

	vdac->overWA = 0x08;
	MachEmptyWriteBuffer();
	vdac->over = 0x00;
	MachEmptyWriteBuffer();
	vdac->over = 0x00;
	MachEmptyWriteBuffer();
	vdac->over = 0x7f;
	MachEmptyWriteBuffer();

	vdac->overWA = 0x0c;
	MachEmptyWriteBuffer();
	for (i = 0; i < 3; i++) {
		vdac->over = fg_RGB[i];
		MachEmptyWriteBuffer();
	}
}

/*
 * ----------------------------------------------------------------------------
 *
 * pmCursorColor --
 *
 *	Set the color of the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
static void
pmCursorColor(color)
	unsigned int color[];
{
	register int i, j;

	for (i = 0; i < 3; i++)
		bg_RGB[i] = (u_char)(color[i] >> 8);

	for (i = 3, j = 0; i < 6; i++, j++)
		fg_RGB[j] = (u_char)(color[i] >> 8);

	pmRestoreCursorColor();
}

/*
 * ----------------------------------------------------------------------------
 *
 * pmInitColorMap --
 *
 *	Initialize the color map.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The colormap is initialized appropriately whether it is color or 
 *	monochrome.
 *
 * ----------------------------------------------------------------------------
 */
static void
pmInitColorMap()
{
	register VDACRegs *vdac = (VDACRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_VDAC);
	register int i;

	*(volatile char *)MACH_PHYS_TO_UNCACHED(KN01_PHYS_COLMASK_START) = 0xff;
	MachEmptyWriteBuffer();

	if (fi->fi_type.fb_depth == 1) {
		vdac->mapWA = 0; MachEmptyWriteBuffer();
		for (i = 0; i < 256; i++) {
			vdac->map = (i < 128) ? 0x00 : 0xff;
			MachEmptyWriteBuffer();
			vdac->map = (i < 128) ? 0x00 : 0xff;
			MachEmptyWriteBuffer();
			vdac->map = (i < 128) ? 0x00 : 0xff;
			MachEmptyWriteBuffer();
		}
	} else {
		vdac->mapWA = 0; MachEmptyWriteBuffer();
		vdac->map = 0; MachEmptyWriteBuffer();
		vdac->map = 0; MachEmptyWriteBuffer();
		vdac->map = 0; MachEmptyWriteBuffer();

		for (i = 1; i < 256; i++) {
			vdac->map = 0xff; MachEmptyWriteBuffer();
			vdac->map = 0xff; MachEmptyWriteBuffer();
			vdac->map = 0xff; MachEmptyWriteBuffer();
		}
	}

	for (i = 0; i < 3; i++) {
		bg_RGB[i] = 0x00;
		fg_RGB[i] = 0xff;
	}
	pmRestoreCursorColor();
}


#endif /* 0 */

/* should zap pmloadcolormap too, but i haven't fixed the callers yet */

/*
 * ----------------------------------------------------------------------------
 *
 * pmLoadColorMap --
 *
 *	Load the color map.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The color map is loaded.
 *
 * ----------------------------------------------------------------------------
 */
static void
pmLoadColorMap(ptr)
	ColorMap *ptr;
{
	register VDACRegs *vdac = (VDACRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_VDAC);

	if (ptr->index > 256)
		return;

	vdac->mapWA = ptr->index; MachEmptyWriteBuffer();
	vdac->map = ptr->Entry.red; MachEmptyWriteBuffer();
	vdac->map = ptr->Entry.green; MachEmptyWriteBuffer();
	vdac->map = ptr->Entry.blue; MachEmptyWriteBuffer();
}



/*
 *----------------------------------------------------------------------
 *
 * pmPosCursor --
 *
 *	Postion the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
pmPosCursor(fi, x, y)
	register struct fbinfo *fi;
	register int x, y;
{
	register PCCRegs *pcc = (PCCRegs *)MACH_PHYS_TO_UNCACHED(KN01_SYS_PCC);

	if (y < fi->fi_fbu->scrInfo.min_cur_y ||
	    y > fi->fi_fbu->scrInfo.max_cur_y)
		y = fi->fi_fbu->scrInfo.max_cur_y;
	if (x < fi->fi_fbu->scrInfo.min_cur_x ||
	    x > fi->fi_fbu->scrInfo.max_cur_x)
		x = fi->fi_fbu->scrInfo.max_cur_x;
	fi->fi_fbu->scrInfo.cursor.x = x;	/* keep track of real cursor */
	fi->fi_fbu->scrInfo.cursor.y = y;	/* position, indep. of mouse */
	pcc->xpos = PCC_X_OFFSET + x;
	pcc->ypos = PCC_Y_OFFSET + y;
}

/* enable the video display. */

static int pm_video_on (fi)
	struct fbinfo *fi;
{
	register PCCRegs *pcc = (PCCRegs *)fi -> fi_base;

	if (!fi -> fi_blanked)
		return 0;
	
	pcc -> cmdr = curReg & ~(PCC_FOPA | PCC_FOPB);
	bt478RestoreCursorColor (fi);
	fi -> fi_blanked = 0;
	return 0;
}

/* disable the video display. */

static int pm_video_off (fi)
	struct fbinfo *fi;
{
	register PCCRegs *pcc = (PCCRegs *)fi -> fi_base;

	if (fi -> fi_blanked)
		return 0;
	
	bt478BlankCursor (fi);
	pcc -> cmdr = curReg | PCC_FOPA | PCC_FOPB;
	fi -> fi_blanked = 1;
	return 0;
}

#endif /* NDC */
#endif /* NPM */

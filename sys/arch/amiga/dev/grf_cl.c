/*	$OpenBSD: grf_cl.c,v 1.13 1999/03/08 15:26:09 espie Exp $	*/
/*	$NetBSD: grf_cl.c,v 1.20 1997/07/29 17:46:24 veego Exp $	*/

/*
 * Copyright (c) 1997 Klaus Burkert
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

/*
 * Graphics routines for Cirrus CL GD 5426 boards,
 *
 * This code offers low-level routines to access Cirrus Cl GD 5426
 * graphics-boards from within OpenBSD for the Amiga.
 * No warranties for any kind of function at all - this
 * code may crash your hardware and scratch your harddisk.  Use at your
 * own risk.  Freely distributable.
 *
 * Modified for Cirrus CL GD 5426 from
 * Lutz Vieweg's retina driver by Kari Mettinen 08/94
 * Contributions by Ill, ScottE, MiL
 * Extensively hacked and rewritten by Ezra Story (Ezy) 01/95
 * Picasso/040 patches (wee!) by crest 01/96
 *
 * PicassoIV support bz Klaus "crest" Burkert.
 * Fixed interlace and doublescan, added clockdoubling and
 * HiColor&TrueColor suuport by crest 01/97
 *
 * Thanks to Village Tronic Marketing Gmbh for providing me with
 * a Picasso-II board.
 * Thanks for Integrated Electronics Oy Ab for providing me with
 * Cirrus CL GD 542x family documentation.
 *
 * TODO:
 *    Mouse support (almost there! :-))
 *    Blitter support
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <dev/cons.h>
#include <amiga/dev/itevar.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_clreg.h>
#include <amiga/dev/zbusvar.h>

int	cl_mondefok __P((struct grfvideo_mode *));
void	cl_boardinit __P((struct grf_softc *));
void	cl_CompFQ __P((u_int, u_char *, u_char *, u_char *));
int	cl_getvmode __P((struct grf_softc *, struct grfvideo_mode *));
int	cl_setvmode __P((struct grf_softc *, unsigned int));
int	cl_toggle __P((struct grf_softc *, unsigned short));
int	cl_getcmap __P((struct grf_softc *, struct grf_colormap *));
int	cl_putcmap __P((struct grf_softc *, struct grf_colormap *));
#ifndef CL5426CONSOLE
void	cl_off __P((struct grf_softc *));
#endif
void	cl_inittextmode __P((struct grf_softc *));
int	cl_ioctl __P((register struct grf_softc *, u_long, void *));
int	cl_getmousepos __P((struct grf_softc *, struct grf_position *));
int	cl_setmousepos __P((struct grf_softc *, struct grf_position *));
int	cl_setspriteinfo __P((struct grf_softc *, struct grf_spriteinfo *));
int	cl_getspriteinfo __P((struct grf_softc *, struct grf_spriteinfo *));
int	cl_getspritemax __P((struct grf_softc *, struct grf_position *));
int	cl_blank __P((struct grf_softc *, int *));
int	cl_setmonitor __P((struct grf_softc *, struct grfvideo_mode *));
void	cl_writesprpos __P((volatile char *, short, short));
void	writeshifted __P((volatile char *, char, char));

void	RegWakeup __P((volatile caddr_t));
void	RegOnpass __P((volatile caddr_t));
void	RegOffpass __P((volatile caddr_t));

void	grfclattach __P((struct device *, struct device *, void *));
int	grfclprint __P((void *, const char *));
int	grfclmatch __P((struct device *, void *, void *));
void	cl_memset __P((unsigned char *, unsigned char, int));

/* Graphics display definitions.
 * These are filled by 'grfconfig' using GRFIOCSETMON.
 */
#define monitor_def_max 24
static struct grfvideo_mode monitor_def[24] = {
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}
};
static struct grfvideo_mode *monitor_current = &monitor_def[0];

/* Patchable maximum pixel clock */
unsigned long cl_maxpixelclock = 86000000;

/* Console display definition.
 *   Default hardcoded text mode.  This grf_cl is set up to
 *   use one text mode only, and this is it.  You may use
 *   grfconfig to change the mode after boot.
 */
/* Console font */
#ifdef KFONT_8X11
#define CIRRUSFONT kernel_font_8x11
#define CIRRUSFONTY 11
#else
#define CIRRUSFONT kernel_font_8x8
#define CIRRUSFONTY 8
#endif
extern unsigned char CIRRUSFONT[];

struct grfcltext_mode clconsole_mode = {
	{255, "", 28200000, 640, 480, 4, 640/8, 752/8, 792/8, 832/8,
	 481, 490, 498, 522, 0},
	8, CIRRUSFONTY, 80, 480 / CIRRUSFONTY, CIRRUSFONT, 32, 255
};
/* Console colors */
unsigned char clconscolors[3][3] = {	/* background, foreground, hilite */
	{0, 0x40, 0x50}, {152, 152, 152}, {255, 255, 255}
};

int	cltype = 0;		/* Picasso, Spectrum or Piccolo */
int	cl_64bit = 0;		/* PiccoloSD64 or PicassoIV */
unsigned char pass_toggle;	/* passthru status tracker */

/*
 * because all 542x-boards have 2 configdev entries, one for
 * framebuffer mem and the other for regs, we have to hold onto
 * the pointers globally until we match on both.  This and 'cltype'
 * are the primary obsticles to multiple board support, but if you
 * have multiple boards you have bigger problems than grf_cl.
 */
static void *cl_fbaddr = 0;	/* framebuffer */
static void *cl_regaddr = 0;	/* registers */
static int cl_fbsize;		/* framebuffer size */
static int cl_fbautosize;	/* framebuffer autoconfig size */

/*
 * current sprite info, if you add support for multiple boards
 * make this an array or something
 */
struct grf_spriteinfo cl_cursprite;

/* sprite bitmaps in kernel stack, you'll need to arrayize these too if
 * you add multiple board support
 */
static unsigned char cl_imageptr[8 * 64], cl_maskptr[8 * 64];
static unsigned char cl_sprred[2], cl_sprgreen[2], cl_sprblue[2];

/* standard driver stuff */
struct cfattach grfcl_ca = {
	sizeof(struct grf_softc), grfclmatch, grfclattach
};

struct cfdriver grfcl_cd = {
	NULL, "grfcl", DV_DULL, NULL, 0
};
static struct cfdata *grfcl_cfdata;

int
grfclmatch(pdp, match, auxp)
	struct device *pdp;
	void   *match, *auxp;
{
#ifdef CL5426CONSOLE
	struct cfdata *cfp = match;
#endif
	struct zbus_args *zap;
	static int regprod, fbprod, fbprod2;
	int error;

	fbprod2 = 0;
	zap = auxp;

#ifndef CL5426CONSOLE
	if (amiga_realconfig == 0)
		return (0);
#endif

	/* Grab the first board we encounter as the preferred one.  This will
	 * allow one board to work in a multiple 5426 board system, but not
	 * multiple boards at the same time.  */
	if (cltype == 0) {
		switch (zap->manid) {
		    case PICASSO:
			switch (zap->prodid) {
			    case 11:
			    case 12:
				regprod = 12;
				fbprod = 11;
				error = 0;
				break;
			    case 22:
				fbprod2 = 22;
				error = 0;
				break;
			    case 21:
			    case 23:
				regprod = 23;
				fbprod = 21;
				cl_64bit = 1;
				error = 0;
				break;
			    case 24:
				regprod = 24;
				fbprod = 24;
				cl_64bit = 1;
				error = 0;
				break;
		    	    default:
				error = 1;
				break;
			}
			if (error == 1)
			    return (0);
			else
			    break;
		    case SPECTRUM:
			if (zap->prodid != 2 && zap->prodid != 1)
				return (0);
			regprod = 2;
			fbprod = 1;
			break;
		    case PICCOLO:
			switch (zap->prodid) {
			    case 5:
			    case 6:
				regprod = 6;
				fbprod = 5;
				error = 0;
				break;
			    case 10:
			    case 11:
				regprod = 11;
				fbprod = 10;
				cl_64bit = 1;
				error = 0;
				break;
		    	    default:
				error = 1;
				break;
			}
			if (error == 1)
			    return (0);
			else
			    break;
		    default:
			return (0);
		}
		cltype = zap->manid;
	} else {
		if (cltype != zap->manid) {
			return (0);
		}
	}

	/* Configure either registers or framebuffer in any order */
	if ((cltype == PICASSO) && (cl_64bit == 1)) {
		switch (zap->prodid) {
		    case 21:
			cl_fbaddr = zap->va;
			cl_fbautosize = zap->size;
			break;
		    case 22: 
			cl_fbautosize += zap->size;
			break;
		    case 23:
			cl_regaddr = (void *)((unsigned long)(zap->va) +
			    0x10000);
			break;
		    case 24:
			cl_regaddr = (void *)((unsigned long)(zap->va) +
			    0x600000);
			cl_fbaddr = (void *)((unsigned long)(zap->va) +
			    0x01000000);
			cl_fbautosize = 0x400000;
			break;
		    default:
			return (0);
		}
	} else {
		if (zap->prodid == regprod)
			cl_regaddr = zap->va;
		else
			if (zap->prodid == fbprod) {
				cl_fbaddr = zap->va;
				cl_fbautosize = zap->size;
			} else
				return (0);
	}

#ifdef CL5426CONSOLE
	if (amiga_realconfig == 0) {
		grfcl_cfdata = cfp;
	}
#endif

	return (1);
}

void
grfclattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void   *auxp;
{
	static struct grf_softc congrf;
	struct zbus_args *zap;
	struct grf_softc *gp;
	static char attachflag = 0;

	zap = auxp;

	printf("\n");

	/* make sure both halves have matched */
	if (!cl_regaddr || !cl_fbaddr)
		return;

	/* do all that messy console/grf stuff */
	if (dp == NULL)
		gp = &congrf;
	else
		gp = (struct grf_softc *) dp;

	if (dp != NULL && congrf.g_regkva != 0) {
		/*
		 * inited earlier, just copy (not device struct)
		 */
		bcopy(&congrf.g_display, &gp->g_display,
		    (char *) &gp[1] - (char *) &gp->g_display);
	} else {
		gp->g_regkva = (volatile caddr_t) cl_regaddr;
		gp->g_fbkva = (volatile caddr_t) cl_fbaddr;

		gp->g_unit = GRF_CL5426_UNIT;
		gp->g_mode = cl_mode;
		gp->g_conpri = grfcl_cnprobe();
		gp->g_flags = GF_ALIVE;

		/* wakeup the board */
		cl_boardinit(gp);
#ifdef CL5426CONSOLE
		grfcl_iteinit(gp);
		(void) cl_load_mon(gp, &clconsole_mode);
#endif

	}

	/*
	 * attach grf (once)
	 */
	if (amiga_config_found(grfcl_cfdata, &gp->g_device, gp, grfclprint)) {
		attachflag = 1;
		printf("grfcl: %dMB ", cl_fbsize / 0x100000);
		switch (cltype) {
		    case PICASSO:
			if (cl_64bit == 1) {
				printf("Picasso IV");
				/* 135MHz will be supported if we
				 * have a palette doubling mode.
				 */
				cl_maxpixelclock = 86000000;
			} else {
				printf("Picasso II");

				/* check for PicassoII+ (crest) */
				if (zap->serno == 0x00100000)
					printf("+");

				/* determine used Gfx/chipset (crest) */
				vgaw(gp->g_regkva, CRT_ADDRESS, 0x27);
				switch (vgar(gp->g_regkva, CRT_ADDRESS_R)>>2) {
				    case 0x24:
					printf(" (with CL-GD5426)");
					break;
				    case 0x26:
					printf(" (with CL-GD5428)");
					break;
				    case 0x27:
					printf(" (with CL-GD5429)");
					break;
				}
	                        cl_maxpixelclock = 86000000;
			}
			break;
		    case SPECTRUM:
			printf("Spectrum");
                        cl_maxpixelclock = 90000000;
			break;
		    case PICCOLO:
			if (cl_64bit == 1) {
				printf("Piccolo SD64");
				/* 110MHz will be supported if we
				 * have a palette doubling mode.
				 */
				cl_maxpixelclock = 90000000;
			} else {
				printf("Piccolo");
				cl_maxpixelclock = 90000000;
			}
			break;
		}
		printf(" being used\n");
#ifdef CL_OVERCLOCK
                cl_maxpixelclock = 115000000;
#endif
	} else {
		if (!attachflag)
			printf("grfcl unattached!!\n");
	}
}

int
grfclprint(auxp, pnp)
	void	*auxp;
	const char *pnp;
{
	if (pnp)
		printf("ite at %s: ", pnp);
	return (UNCONF);
}

void
cl_boardinit(gp)
	struct grf_softc *gp;
{
	unsigned char *ba = gp->g_regkva;
	int     x;

	if ((cltype == PICASSO) && (cl_64bit == 1)) { /* PicassoIV */
		/* disable capture (FlickerFixer) */
		WCrt(ba, 0x51, 0x00);
		/* wait some time (two frames as of now) */
		delay(200000);
		/* get Blitter into 542x  */
		WGfx(ba, 0x2f, 0x00);
		/* compatibility mode     */
		WGfx(ba, GCT_ID_RESERVED, 0x00);
		/* or at least, try so... */
		WGfx(ba, GCT_ID_BLT_STAT_START, 0x00);
		cl_fbsize = cl_fbautosize;
	} else {

		/* wakeup board and flip passthru OFF */
		RegWakeup(ba);
		RegOnpass(ba);

		vgaw(ba, 0x46e8, 0x16);
		vgaw(ba, 0x102, 1);
		vgaw(ba, 0x46e8, 0x0e);
		if (cl_64bit != 1)
			vgaw(ba, 0x3c3, 1);

		cl_fbsize = cl_fbautosize;

		/* setup initial unchanging parameters */

		/* 8 dot - display off */
		WSeq(ba, SEQ_ID_CLOCKING_MODE, 0x21);
		vgaw(ba, GREG_MISC_OUTPUT_W, 0xed);	/* mem disable */

		WGfx(ba, GCT_ID_OFFSET_1, 0xec);	/* magic cookie */
		WSeq(ba, SEQ_ID_UNLOCK_EXT, 0x12);	/* yum! cookies! */

		if (cl_64bit == 1) {
			WSeq(ba, SEQ_ID_CONF_RBACK, 0x00);
			WSeq(ba, SEQ_ID_DRAM_CNTL,
			    (cl_fbsize / 0x100000 == 2) ? 0x38 : 0xb8);
		} else {
			WSeq(ba, SEQ_ID_DRAM_CNTL, 0xb0);
		}
		WSeq(ba, SEQ_ID_RESET, 0x03);
		WSeq(ba, SEQ_ID_MAP_MASK, 0xff);
		WSeq(ba, SEQ_ID_CHAR_MAP_SELECT, 0x00);
		WSeq(ba, SEQ_ID_MEMORY_MODE, 0x0e);	/* a or 6? */
		WSeq(ba, SEQ_ID_EXT_SEQ_MODE,
		    (cltype == PICASSO) ? 0x21 : 0x81);
		WSeq(ba, SEQ_ID_EEPROM_CNTL, 0x00);
		if (cl_64bit == 1)
			WSeq(ba, SEQ_ID_PERF_TUNE, 0x5a);
		else
			/* mouse 0a fa */
			WSeq(ba, SEQ_ID_PERF_TUNE, 0x0a);
		WSeq(ba, SEQ_ID_SIG_CNTL, 0x02);
		WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x04);
		if (cl_64bit == 1)
			WSeq(ba, SEQ_ID_MCLK_SELECT, 0x1c);
		else
			WSeq(ba, SEQ_ID_MCLK_SELECT, 0x22);

		WCrt(ba, CRT_ID_PRESET_ROW_SCAN, 0x00);
		WCrt(ba, CRT_ID_CURSOR_START, 0x00);
		WCrt(ba, CRT_ID_CURSOR_END, 0x08);
		WCrt(ba, CRT_ID_START_ADDR_HIGH, 0x00);
		WCrt(ba, CRT_ID_START_ADDR_LOW, 0x00);
		WCrt(ba, CRT_ID_CURSOR_LOC_HIGH, 0x00);
		WCrt(ba, CRT_ID_CURSOR_LOC_LOW, 0x00);

		WCrt(ba, CRT_ID_UNDERLINE_LOC, 0x07);
		WCrt(ba, CRT_ID_MODE_CONTROL, 0xe3);
		WCrt(ba, CRT_ID_LINE_COMPARE, 0xff);	/* ff */
		WCrt(ba, CRT_ID_EXT_DISP_CNTL, 0x22);
		if (cl_64bit == 1) {
			WCrt(ba, CRT_ID_SYNC_ADJ_GENLOCK, 0x00);
			WCrt(ba, CRT_ID_OVERLAY_EXT_CTRL_REG, 0x40);
		}
		WSeq(ba, SEQ_ID_CURSOR_STORE, 0x3c);	/* mouse 0x00 */

		WGfx(ba, GCT_ID_SET_RESET, 0x00);
		WGfx(ba, GCT_ID_ENABLE_SET_RESET, 0x00);
		WGfx(ba, GCT_ID_DATA_ROTATE, 0x00);
		WGfx(ba, GCT_ID_READ_MAP_SELECT, 0x00);
		WGfx(ba, GCT_ID_GRAPHICS_MODE, 0x00);
		WGfx(ba, GCT_ID_MISC, 0x01);
		WGfx(ba, GCT_ID_COLOR_XCARE, 0x0f);
		WGfx(ba, GCT_ID_BITMASK, 0xff);
		WGfx(ba, GCT_ID_MODE_EXT, 0x28);

		for (x = 0; x < 0x10; x++)
			WAttr(ba, x, x);
		WAttr(ba, ACT_ID_ATTR_MODE_CNTL, 0x01);
		WAttr(ba, ACT_ID_OVERSCAN_COLOR, 0x00);
		WAttr(ba, ACT_ID_COLOR_PLANE_ENA, 0x0f);
		WAttr(ba, ACT_ID_HOR_PEL_PANNING, 0x00);
		WAttr(ba, ACT_ID_COLOR_SELECT, 0x00);
		WAttr(ba, 0x34, 0x00);

		vgaw(ba, VDAC_MASK, 0xff);
		vgaw(ba, GREG_MISC_OUTPUT_W, 0xef);

		WGfx(ba, GCT_ID_BLT_STAT_START, 0x04);
		WGfx(ba, GCT_ID_BLT_STAT_START, 0x00);
	}

	/* colors initially set to greyscale */
	vgaw(ba, VDAC_ADDRESS_W, 0);
	for (x = 255; x >= 0; x--) {
		vgaw(ba, VDAC_DATA, x);
		vgaw(ba, VDAC_DATA, x);
		vgaw(ba, VDAC_DATA, x);
	}
	/* set sprite bitmap pointers */
	cl_cursprite.image = cl_imageptr;
	cl_cursprite.mask = cl_maskptr;
	cl_cursprite.cmap.red = cl_sprred;
	cl_cursprite.cmap.green = cl_sprgreen;
	cl_cursprite.cmap.blue = cl_sprblue;

	if (cl_64bit == 0) {
		/* check for 1MB or 2MB board (crest) */
		volatile unsigned long *cl_fbtestaddr;
		cl_fbtestaddr = (volatile unsigned long *)gp->g_fbkva;

		WGfx(ba, GCT_ID_OFFSET_0, 0x40);
		*cl_fbtestaddr = 0x12345678;

		if (*cl_fbtestaddr != 0x12345678) {
			WSeq(ba, SEQ_ID_DRAM_CNTL, 0x30);
			cl_fbsize = 0x100000;
		} else {
			cl_fbsize = 0x200000;
		}
	}
	WGfx(ba, GCT_ID_OFFSET_0, 0x00);
}


int
cl_getvmode(gp, vm)
	struct grf_softc *gp;
	struct grfvideo_mode *vm;
{
	struct grfvideo_mode *gv;

#ifdef CL5426CONSOLE
	/* Handle grabbing console mode */
	if (vm->mode_num == 255) {
		bcopy(&clconsole_mode, vm, sizeof(struct grfvideo_mode));
		/* XXX so grfconfig can tell us the correct text dimensions. */
		vm->depth = clconsole_mode.fy;
	} else 
#endif
        {
                if (vm->mode_num == 0)
                        vm->mode_num = (monitor_current - monitor_def) + 1;
                if (vm->mode_num < 1 || vm->mode_num > monitor_def_max)
                        return (EINVAL);
                gv = monitor_def + (vm->mode_num - 1);
                if (gv->mode_num == 0)
                        return (EINVAL);

                bcopy(gv, vm, sizeof(struct grfvideo_mode));
        }

        /* adjust internal values to pixel values */

        vm->hblank_start *= 8;
        vm->hsync_start *= 8;
        vm->hsync_stop *= 8;
        vm->htotal *= 8;
        
	return (0);
}


int
cl_setvmode(gp, mode)
	struct grf_softc *gp;
	unsigned mode;
{
	if (!mode || (mode > monitor_def_max) ||
	    monitor_def[mode - 1].mode_num == 0)
		return (EINVAL);

	monitor_current = monitor_def + (mode - 1);

	return (0);
}

#ifndef CL5426CONSOLE
void
cl_off(gp)
	struct grf_softc *gp;
{
	char   *ba = gp->g_regkva;

	/*
	 * we'll put the pass-through on for cc ite and set Full Bandwidth bit
	 * on just in case it didn't work...but then it doesn't matter does
	 * it? =)
	 */
	RegOnpass(ba);
	vgaw(ba, SEQ_ADDRESS, SEQ_ID_CLOCKING_MODE);
	vgaw(ba, SEQ_ADDRESS_W, vgar(ba, SEQ_ADDRESS_W) | 0x20);
}
#endif

int
cl_blank(gp, on)
        struct grf_softc *gp;
        int *on;
{
        WSeq(gp->g_regkva, SEQ_ID_CLOCKING_MODE, *on > 0 ? 0x01 : 0x21);
        return(0);
}
        
/*
 * Change the mode of the display.
 * Return a UNIX error number or 0 for success.
 */
int
cl_mode(gp, cmd, arg, a2, a3)
	register struct grf_softc *gp;
	u_long cmd;
	void *arg;
	u_long a2;
	int a3;
{
	int     error;

	switch (cmd) {
	    case GM_GRFON:
		error = cl_load_mon(gp,
		    (struct grfcltext_mode *) monitor_current) ? 0 : EINVAL;
		return (error);

	    case GM_GRFOFF:
#ifndef CL5426CONSOLE
		cl_off(gp);
#else
		cl_load_mon(gp, &clconsole_mode);
#endif
		return (0);

	    case GM_GRFCONFIG:
		return (0);

	    case GM_GRFGETVMODE:
		return (cl_getvmode(gp, (struct grfvideo_mode *) arg));

	    case GM_GRFSETVMODE:
		error = cl_setvmode(gp, *(unsigned *) arg);
		if (!error && (gp->g_flags & GF_GRFON))
			cl_load_mon(gp,
			    (struct grfcltext_mode *) monitor_current);
		return (error);

	    case GM_GRFGETNUMVM:
		*(int *) arg = monitor_def_max;
		return (0);

	    case GM_GRFIOCTL:
		return (cl_ioctl(gp, a2, arg));

	    default:
		break;
	}

	return (EINVAL);
}

int
cl_ioctl(gp, cmd, data)
	register struct grf_softc *gp;
	u_long cmd;
	void   *data;
{
	switch (cmd) {
	    case GRFIOCGSPRITEPOS:
		return (cl_getmousepos(gp, (struct grf_position *) data));

	    case GRFIOCSSPRITEPOS:
		return (cl_setmousepos(gp, (struct grf_position *) data));

	    case GRFIOCSSPRITEINF:
		return (cl_setspriteinfo(gp, (struct grf_spriteinfo *) data));

	    case GRFIOCGSPRITEINF:
		return (cl_getspriteinfo(gp, (struct grf_spriteinfo *) data));

	    case GRFIOCGSPRITEMAX:
		return (cl_getspritemax(gp, (struct grf_position *) data));

	    case GRFIOCGETCMAP:
		return (cl_getcmap(gp, (struct grf_colormap *) data));

	    case GRFIOCPUTCMAP:
		return (cl_putcmap(gp, (struct grf_colormap *) data));

	    case GRFIOCBITBLT:
		break;

	    case GRFTOGGLE:
		return (cl_toggle(gp, 0));

	    case GRFIOCSETMON:
		return (cl_setmonitor(gp, (struct grfvideo_mode *) data));

            case GRFIOCBLANK:
                return (cl_blank(gp, (int *)data));

	}
	return (EINVAL);
}

int
cl_getmousepos(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{
	data->x = cl_cursprite.pos.x;
	data->y = cl_cursprite.pos.y;
	return (0);
}

void
cl_writesprpos(ba, x, y)
	volatile char *ba;
	short   x;
	short   y;
{
	/* we want to use a 16-bit write to 3c4 so no macros used */
	volatile unsigned char *cwp;
        volatile unsigned short *wp;

	cwp = ba + 0x3c4;
        wp = (unsigned short *)cwp;

	/*
	 * don't ask me why, but apparently you can't do a 16-bit write with
	 * x-position like with y-position below (dagge)
	 */
        cwp[0] = 0x10 | ((x << 5) & 0xff);
        cwp[1] = (x >> 3) & 0xff;

        *wp = 0x1100 | ((y & 7) << 13) | ((y >> 3) & 0xff);
}

void
writeshifted(to, shiftx, shifty)
	volatile char *to;
	char    shiftx;
	char    shifty;
{
	register char y;
	unsigned long long *tptr, *iptr, *mptr, line;

	tptr = (unsigned long long *) to;
        iptr = (unsigned long long *) cl_cursprite.image;
        mptr = (unsigned long long *) cl_cursprite.mask;

        shiftx = shiftx < 0 ? 0 : shiftx;
        shifty = shifty < 0 ? 0 : shifty;

        /*
	 * start reading shifty lines down, and
         * shift each line in by shiftx
         */
        for (y = shifty; y < 64; y++) {

                /* image */
                line = iptr[(int)y];
		*tptr++ = line << shiftx;

                /* mask */
                line = mptr[(int)y];
		*tptr++ = line << shiftx;
	}

        /* clear the remainder */
        for (y = shifty; y > 0; y--) {
                *tptr++ = 0;
                *tptr++ = 0;
        }
}

int
cl_setmousepos(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{
	volatile char *ba = gp->g_regkva;
        short rx, ry, prx, pry;
#ifdef CL_SHIFTSPRITE
	volatile char *fb = gp->g_fbkva;
        volatile char *sprite = fb + (cl_fbsize - 1024);
#endif

        /* no movement */
	if (cl_cursprite.pos.x == data->x && cl_cursprite.pos.y == data->y)
		return (0);

        /* current and previous real coordinates */
	rx = data->x - cl_cursprite.hot.x;
	ry = data->y - cl_cursprite.hot.y;
	prx = cl_cursprite.pos.x - cl_cursprite.hot.x;
	pry = cl_cursprite.pos.y - cl_cursprite.hot.y;

        /*
	 * if we are/were on an edge, create (un)shifted bitmap --
         * ripped out optimization (not extremely worthwhile,
         * and kind of buggy anyhow).
         */
#ifdef CL_SHIFTSPRITE
        if (rx < 0 || ry < 0 || prx < 0 || pry < 0) {
                writeshifted(sprite, rx < 0 ? -rx : 0, ry < 0 ? -ry : 0);
        }
#endif

        /* do movement, save position */
        cl_writesprpos(ba, rx < 0 ? 0 : rx, ry < 0 ? 0 : ry);
	cl_cursprite.pos.x = data->x;
	cl_cursprite.pos.y = data->y;

	return (0);
}

int
cl_getspriteinfo(gp, data)
	struct grf_softc *gp;
	struct grf_spriteinfo *data;
{
	copyout(&cl_cursprite, data, sizeof(struct grf_spriteinfo));
	copyout(cl_cursprite.image, data->image, 64 * 8);
	copyout(cl_cursprite.mask, data->mask, 64 * 8);
	return (0);
}

int
cl_setspriteinfo(gp, data)
	struct grf_softc *gp;
	struct grf_spriteinfo *data;
{
	volatile unsigned char *ba = gp->g_regkva, *fb = gp->g_fbkva;
        volatile char *sprite = fb + (cl_fbsize - 1024);

	if (data->set & GRFSPRSET_SHAPE) {

                short dsx, dsy, i;
                unsigned long *di, *dm, *si, *sm;
                unsigned long ssi[128], ssm[128];
                struct grf_position gpos;

               
                /* check for a too large sprite (no clipping!) */ 
                dsy = data->size.y;
                dsx = data->size.x;
                if (dsy > 64 || dsx > 64)
                        return(EINVAL);

                /* prepare destination */
                di = (unsigned long *)cl_cursprite.image;
                dm = (unsigned long *)cl_cursprite.mask;
                cl_memset((unsigned char *)di, 0, 8*64);
                cl_memset((unsigned char *)dm, 0, 8*64);

                /* two alternatives:  64 across, then it's
                 * the same format we use, just copy.  Otherwise,
                 * copy into tmp buf and recopy skipping the
                 * unused 32 bits.
                 */
                if ((dsx - 1) / 32) {
                        copyin(data->image, di, 8 * dsy);
                        copyin(data->mask, dm, 8 * dsy);
                } else {
                        si = ssi; sm = ssm;
                        copyin(data->image, si, 4 * dsy);
                        copyin(data->mask, sm, 4 * dsy);
                        for (i = 0; i < dsy; i++) {
                                *di = *si++;
                                *dm = *sm++;
                                di += 2;
                                dm += 2;
                        }
                }

                /* set size */
		cl_cursprite.size.x = data->size.x;
		cl_cursprite.size.y = data->size.y;

                /* forcably load into board */
                gpos.x = cl_cursprite.pos.x;
                gpos.y = cl_cursprite.pos.y;
                cl_cursprite.pos.x = -1;
                cl_cursprite.pos.y = -1;
                writeshifted(sprite, 0, 0);
                cl_setmousepos(gp, &gpos);

	}
	if (data->set & GRFSPRSET_HOT) {

		cl_cursprite.hot = data->hot;

	}
	if (data->set & GRFSPRSET_CMAP) {

		u_char  red[2], green[2], blue[2];

		copyin(data->cmap.red, red, 2);
		copyin(data->cmap.green, green, 2);
		copyin(data->cmap.blue, blue, 2);
		bcopy(red, cl_cursprite.cmap.red, 2);
		bcopy(green, cl_cursprite.cmap.green, 2);
		bcopy(blue, cl_cursprite.cmap.blue, 2);

                /* enable and load colors 256 & 257 */
		WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x06);

                /* 256 */
		vgaw(ba, VDAC_ADDRESS_W, 0x00);
		if (cltype == PICASSO) {
			vgaw(ba, VDAC_DATA, (u_char) (red[0] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (green[0] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (blue[0] >> 2));
		} else {
			vgaw(ba, VDAC_DATA, (u_char) (blue[0] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (green[0] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (red[0] >> 2));
		}

                /* 257 */
		vgaw(ba, VDAC_ADDRESS_W, 0x0f);
		if (cltype == PICASSO) {
			vgaw(ba, VDAC_DATA, (u_char) (red[1] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (green[1] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (blue[1] >> 2));
		} else {
			vgaw(ba, VDAC_DATA, (u_char) (blue[1] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (green[1] >> 2));
			vgaw(ba, VDAC_DATA, (u_char) (red[1] >> 2));
		}
                
                /* turn on/off sprite */
		if (cl_cursprite.enable) {
			WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x05);
		} else {
			WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x04);
		}

	}
	if (data->set & GRFSPRSET_ENABLE) {

		if (data->enable == 1) {
			WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x05);
			cl_cursprite.enable = 1;
		} else {
			WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x04);
			cl_cursprite.enable = 0;
		}

	}
	if (data->set & GRFSPRSET_POS) {

                /* force placement */
                cl_cursprite.pos.x = -1;
                cl_cursprite.pos.y = -1;

                /* do it */
                cl_setmousepos(gp, &data->pos);
                
	}
	return (0);
}

int
cl_getspritemax(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{
	if (gp->g_display.gd_planes == 24)
		return (EINVAL);
	data->x = 64;
	data->y = 64;
	return (0);
}

int
cl_setmonitor(gp, gv)
	struct grf_softc *gp;
	struct grfvideo_mode *gv;
{
	struct grfvideo_mode *md;

        if (!cl_mondefok(gv))
                return(EINVAL);

#ifdef CL5426CONSOLE
	/* handle interactive setting of console mode */
	if (gv->mode_num == 255) {
		bcopy(gv, &clconsole_mode.gv, sizeof(struct grfvideo_mode));
		clconsole_mode.gv.hblank_start /= 8;
		clconsole_mode.gv.hsync_start /= 8;
		clconsole_mode.gv.hsync_stop /= 8;
		clconsole_mode.gv.htotal /= 8;
		clconsole_mode.rows = gv->disp_height / clconsole_mode.fy;
		clconsole_mode.cols = gv->disp_width / clconsole_mode.fx;
		if (!(gp->g_flags & GF_GRFON))
			cl_load_mon(gp, &clconsole_mode);
		ite_reinit(gp->g_itedev);
		return (0);
	}
#endif

	md = monitor_def + (gv->mode_num - 1);
	bcopy(gv, md, sizeof(struct grfvideo_mode));

	/* adjust pixel oriented values to internal rep. */

	md->hblank_start /= 8;
	md->hsync_start /= 8;
	md->hsync_stop /= 8;
	md->htotal /= 8;

	return (0);
}

int
cl_getcmap(gfp, cmap)
	struct grf_softc *gfp;
	struct grf_colormap *cmap;
{
	volatile unsigned char *ba;
	u_char  red[256], green[256], blue[256], *rp, *gp, *bp;
	short   x;
	int     error;

	if (cmap->count == 0 || cmap->index >= 256)
		return 0;

	if (cmap->index + cmap->count > 256)
		cmap->count = 256 - cmap->index;

	ba = gfp->g_regkva;
	/* first read colors out of the chip, then copyout to userspace */
	vgaw(ba, VDAC_ADDRESS_R, cmap->index);
	x = cmap->count - 1;

/*
 * Some sort 'o Magic. Spectrum has some changes on the board to speed
 * up 15 and 16Bit modes. They can access these modes with easy-to-programm
 * rgbrgbrgb instead of rrrgggbbb. Side effect: when in 8Bit mode, rgb
 * is swapped to bgr. I wonder if we need to check for 8Bit though, ill
 */

/*
 * The source for the above comment is somewhat unknow to me.
 * The Spectrum, Piccolo and PiccoloSD64 have the analog Red and Blue
 * lines swapped. In 24BPP this provides RGB instead of BGR as it would
 * be native to the chipset. This requires special programming for the
 * CLUT in 8BPP to compensate and avoid false colors.
 * I didn't find any special stuff for 15 and 16BPP though, crest.
 */

	switch (cltype) {
	    case SPECTRUM:
	    case PICCOLO:
		rp = blue + cmap->index;
		gp = green + cmap->index;
		bp = red + cmap->index;
		break;
	    case PICASSO:
		rp = red + cmap->index;
		gp = green + cmap->index;
		bp = blue + cmap->index;
		break;
	    default:
		rp = gp = bp = 0;
		break;
	}

	do {
		*rp++ = vgar(ba, VDAC_DATA) << 2;
		*gp++ = vgar(ba, VDAC_DATA) << 2;
		*bp++ = vgar(ba, VDAC_DATA) << 2;
	} while (x-- > 0);

	if (!(error = copyout(red + cmap->index, cmap->red, cmap->count))
	    && !(error = copyout(green + cmap->index, cmap->green, cmap->count))
	    && !(error = copyout(blue + cmap->index, cmap->blue, cmap->count)))
		return (0);

	return (error);
}

int
cl_putcmap(gfp, cmap)
	struct grf_softc *gfp;
	struct grf_colormap *cmap;
{
	volatile unsigned char *ba;
	u_char  red[256], green[256], blue[256], *rp, *gp, *bp;
	short   x;
	int     error;

	if (cmap->count == 0 || cmap->index >= 256)
		return (0);

	if (cmap->index + cmap->count > 256)
		cmap->count = 256 - cmap->index;

	/* first copy the colors into kernelspace */
	if (!(error = copyin(cmap->red, red + cmap->index, cmap->count))
	    && !(error = copyin(cmap->green, green + cmap->index, cmap->count))
	    && !(error = copyin(cmap->blue, blue + cmap->index, cmap->count))) {
		ba = gfp->g_regkva;
		vgaw(ba, VDAC_ADDRESS_W, cmap->index);
		x = cmap->count - 1;

		switch (cltype) {
		    case SPECTRUM:
		    case PICCOLO:
			rp = blue + cmap->index;
			gp = green + cmap->index;
			bp = red + cmap->index;
			break;
		    case PICASSO:
			rp = red + cmap->index;
			gp = green + cmap->index;
			bp = blue + cmap->index;
			break;
		    default:
			rp = gp = bp = 0;
			break;
		}

		do {
			vgaw(ba, VDAC_DATA, *rp++ >> 2);
			vgaw(ba, VDAC_DATA, *gp++ >> 2);
			vgaw(ba, VDAC_DATA, *bp++ >> 2);
		} while (x-- > 0);
		return (0);
	} else
		return (error);
}


int
cl_toggle(gp, wopp)
	struct grf_softc *gp;
	unsigned short wopp;	/* don't need that one yet, ill */
{
	volatile caddr_t ba;

	ba = gp->g_regkva;

	if (pass_toggle) {
		RegOffpass(ba);
	} else {
		RegOnpass(ba);
	}
	return (0);
}

void
cl_CompFQ(fq, num, denom, clkdoub)
	u_int   fq;
	u_char *num;
	u_char *denom;
	u_char *clkdoub;
{
#define OSC     14318180
/* OK, here's what we're doing here:
 *
 *             OSC * NUMERATOR
 *      VCLK = -------------------  Hz
 *             DENOMINATOR * (1+P)
 *
 * so we're given VCLK and we should give out some useful
 * values....
 *
 * NUMERATOR is 7 bits wide
 * DENOMINATOR is 5 bits wide with bit P in the same char as bit 0.
 *
 * We run through all the possible combinations and
 * return the values which deviate the least from the chosen frequency.
 *
 */
#define OSC     14318180
#define count(n,d,p)    ((OSC * n)/(d * (1+p)))

	unsigned char n, d, p, minn, mind, minp = 0;
	unsigned long err, minerr;

/*
numer = 0x00 - 0x7f 
denom = 0x00 - 0x1f (1) 0x20 - 0x3e (even)
*/

	/* find lowest error in 6144 iterations. */
	minerr = fq;
	minn = 0;
	mind = 0;
	p = 0;

	if ((cl_64bit == 1) && (fq >= 86000000)) {
		for (d = 1; d < 0x20; d++) {
			for (n = 1; n < 0x80; n++) {
				err = abs(count(n, d, 0) - fq);
				if (err < minerr) {
					minerr = err;
					minn = n;
					mind = d;
					minp = 1;
				}
			}
		}
		*clkdoub = 1;
	} else {
		for (d = 1; d < 0x20; d++) {
			for (n = 1; n < 0x80; n++) {
				err = abs(count(n, d, p) - fq);
				if (err < minerr) {
					minerr = err;
					minn = n;
					mind = d;
					minp = p;
				}
			}
			if (d == 0x1f && p == 0) {
				p = 1;
				d = 0x0f;
			}
		}
		*clkdoub = 0;
	}

	*num = minn;
	*denom = (mind << 1) | minp;
	if (minerr > 500000)
		printf("Warning: CompFQ minimum error = %ld\n", minerr);
	return;
}

int
cl_mondefok(gv)
	struct grfvideo_mode *gv;
{
        unsigned long maxpix;
        
	if (gv->mode_num < 1 || gv->mode_num > monitor_def_max)
                if (gv->mode_num != 255 || gv->depth != 4)
                        return(0);

	switch (gv->depth) {
	case 4:
                if (gv->mode_num != 255)
                        return(0);
	case 1:
	case 8:
		maxpix = cl_maxpixelclock;
		if (cl_64bit == 1)
		{
			if (cltype == PICASSO) /* Picasso IV */
				maxpix = 135000000;
			else                   /* Piccolo SD64 */
				maxpix = 110000000;
		}
                break;
	case 15:
	case 16:
		if (cl_64bit == 1)
	                maxpix = 85000000;
		else
	                maxpix = cl_maxpixelclock - (cl_maxpixelclock / 3);
                break;
	case 24:
		if ((cltype == PICASSO) && (cl_64bit == 1))
	                maxpix = 85000000;
		else
	                maxpix = cl_maxpixelclock / 3;
                break;
	case 32:
		if ((cltype == PICCOLO) && (cl_64bit == 1))
	                maxpix = 50000000;
		else
	                maxpix = 0;
                break;
	default:
		printf("grfcl: Illegal depth in mode %d\n",
			(int) gv->mode_num);
		return (0);
	}

	if (gv->pixel_clock > maxpix) {
		printf("grfcl: Pixelclock too high in mode %d\n",
			(int) gv->mode_num);
                return (0);
	}

	if (gv->disp_flags & GRF_FLAGS_SYNC_ON_GREEN) {
		printf("grfcl: sync-on-green is not supported\n");
		return (0);
	}

        return (1);
}

int
cl_load_mon(gp, md)
	struct grf_softc *gp;
	struct grfcltext_mode *md;
{
	struct grfvideo_mode *gv;
	struct grfinfo *gi;
	volatile caddr_t ba, fb;
	unsigned char num0, denom0, clkdoub;
	unsigned short HT, HDE, HBS, HBE, HSS, HSE, VDE, VBS, VBE, VSS,
	        VSE, VT;
	int	clkmul, offsmul, clkmode;
	int	vmul;
	int	sr15;
	unsigned char hvsync_pulse;
	char    TEXT;

	/* identity */
	gv = &md->gv;
	TEXT = (gv->depth == 4);

	if (!cl_mondefok(gv)) {
		printf("grfcl: Monitor definition not ok\n");
		return (0);
	}

	ba = gp->g_regkva;
	fb = gp->g_fbkva;

	/* 
	 * provide all needed information in grf device-independant
	 * locations
	 */
	gp->g_data = (caddr_t)gv;
	gi = &gp->g_display;
	gi->gd_regaddr = (caddr_t)kvtop(ba);
	gi->gd_regsize = 64 * 1024;
	gi->gd_fbaddr = (caddr_t)kvtop(fb);
	gi->gd_fbsize = cl_fbsize;
	gi->gd_colors = 1 << gv->depth;
	gi->gd_planes = gv->depth;
	gi->gd_fbwidth = gv->disp_width;
	gi->gd_fbheight = gv->disp_height;
	gi->gd_fbx = 0;
	gi->gd_fby = 0;
	if (TEXT) {
		gi->gd_dwidth = md->fx * md->cols;
		gi->gd_dheight = md->fy * md->rows;
	} else {
		gi->gd_dwidth = gv->disp_width;
		gi->gd_dheight = gv->disp_height;
	}
	gi->gd_dx = 0;
	gi->gd_dy = 0;

	/* get display mode parameters */

	HBS = gv->hblank_start;
	HSS = gv->hsync_start;
	HSE = gv->hsync_stop;
	HBE = gv->htotal - 1;
	HT = gv->htotal;
	VBS = gv->vblank_start;
	VSS = gv->vsync_start;
	VSE = gv->vsync_stop;
	VBE = gv->vtotal - 1;
	VT = gv->vtotal;

	if (TEXT)
		HDE = ((gv->disp_width + md->fx - 1) / md->fx) - 1;
	else
		HDE = (gv->disp_width + 3) / 8 - 1;	/* HBS; */
	VDE = gv->disp_height - 1;

	/* adjustments */
	switch (gv->depth) {
	case 8:
		clkmul = 1;
		offsmul = 1;
		clkmode = 0x0;
		break;
	case 15:
	case 16:
		clkmul = 1;
		offsmul = 2;
		clkmode = 0x6;
		break;
	case 24:
		/* Picasso IV? */
		if ((cltype == PICASSO) && (cl_64bit == 1))
			clkmul = 1;
		else
			clkmul = 3;
		offsmul = 3;
		clkmode = 0x4;
		break;
	case 32:
		clkmul = 1;
		offsmul = 2;
		clkmode = 0x8;
		break;
	default:
		clkmul = 1;
		offsmul = 1;
		clkmode = 0x0;
		break;
	}

	if ((VT > 1023) && (!(gv->disp_flags & GRF_FLAGS_LACE))) {
		WCrt(ba, CRT_ID_MODE_CONTROL, 0xe7);
	} else
		WCrt(ba, CRT_ID_MODE_CONTROL, 0xe3);

	vmul = 2;
	if ((VT > 1023) || (gv->disp_flags & GRF_FLAGS_LACE))
		vmul = 1;
	if (gv->disp_flags & GRF_FLAGS_DBLSCAN)
		vmul = 4;

	VDE = VDE * vmul / 2;
	VBS = VBS * vmul / 2;
	VSS = VSS * vmul / 2;
	VSE = VSE * vmul / 2;
	VBE = VBE * vmul / 2;
	VT  = VT * vmul / 2;

	WSeq(ba, SEQ_ID_MEMORY_MODE, (TEXT || (gv->depth == 1)) ? 0x06 : 0x0e);
	if (cl_64bit == 1) {
	    if (TEXT || (gv->depth == 1))
		sr15 = 0xd0;
	    else
		sr15 = ((cl_fbsize / 0x100000 == 2) ? 0x38 : 0xb8);
	    WSeq(ba, SEQ_ID_CONF_RBACK, 0x00);
	} else {
		sr15 = (TEXT || (gv->depth == 1)) ? 0xd0 : 0xb0;
		sr15 &= ((cl_fbsize / 0x100000) == 2) ? 0xff : 0x7f;
	}
	WSeq(ba, SEQ_ID_DRAM_CNTL, sr15);
	WGfx(ba, GCT_ID_READ_MAP_SELECT, 0x00);
	WSeq(ba, SEQ_ID_MAP_MASK, (gv->depth == 1) ? 0x01 : 0xff);
	WSeq(ba, SEQ_ID_CHAR_MAP_SELECT, 0x00);

	/* Set clock */

	cl_CompFQ(gv->pixel_clock * clkmul, &num0, &denom0, &clkdoub);

	/* Horizontal/Vertical Sync Pulse */
	hvsync_pulse = vgar(ba, GREG_MISC_OUTPUT_R);
	if (gv->disp_flags & GRF_FLAGS_PHSYNC)
		hvsync_pulse &= ~0x40;
	else
		hvsync_pulse |= 0x40;
	if (gv->disp_flags & GRF_FLAGS_PVSYNC)
		hvsync_pulse &= ~0x80;
	else
		hvsync_pulse |= 0x80;
	vgaw(ba, GREG_MISC_OUTPUT_W, hvsync_pulse);

	if (clkdoub) {
		HDE /= 2;
		HBS /= 2;
		HSS /= 2;
		HSE /= 2;
		HBE /= 2;
		HT  /= 2;
		clkmode = 0x6;
	}

	WSeq(ba, SEQ_ID_VCLK_3_NUM, num0);
	WSeq(ba, SEQ_ID_VCLK_3_DENOM, denom0);

	/* load display parameters into board */

	WCrt(ba, CRT_ID_HOR_TOTAL, HT);
	WCrt(ba, CRT_ID_HOR_DISP_ENA_END, ((HDE >= HBS) ? HBS - 1 : HDE));
	WCrt(ba, CRT_ID_START_HOR_BLANK, HBS);
	WCrt(ba, CRT_ID_END_HOR_BLANK, (HBE & 0x1f) | 0x80);	/* | 0x80? */
	WCrt(ba, CRT_ID_START_HOR_RETR, HSS);
	WCrt(ba, CRT_ID_END_HOR_RETR,
	    (HSE & 0x1f) |
	    ((HBE & 0x20) ? 0x80 : 0x00));
	WCrt(ba, CRT_ID_VER_TOTAL, VT);
	WCrt(ba, CRT_ID_OVERFLOW,
	    0x10 |
	    ((VT & 0x100) ? 0x01 : 0x00) |
	    ((VDE & 0x100) ? 0x02 : 0x00) |
	    ((VSS & 0x100) ? 0x04 : 0x00) |
	    ((VBS & 0x100) ? 0x08 : 0x00) |
	    ((VT & 0x200) ? 0x20 : 0x00) |
	    ((VDE & 0x200) ? 0x40 : 0x00) |
	    ((VSS & 0x200) ? 0x80 : 0x00));

	WCrt(ba, CRT_ID_CHAR_HEIGHT,
	    0x40 |		/* TEXT ? 0x00 ??? */
	    ((gv->disp_flags & GRF_FLAGS_DBLSCAN) ? 0x80 : 0x00) |
	    ((VBS & 0x200) ? 0x20 : 0x00) |
	    (TEXT ? ((md->fy - 1) & 0x1f) : 0x00));

	/* text cursor */

	if (TEXT) {
#if CL_ULCURSOR
		WCrt(ba, CRT_ID_CURSOR_START, (md->fy & 0x1f) - 2);
		WCrt(ba, CRT_ID_CURSOR_END, (md->fy & 0x1f) - 1);
#else
		WCrt(ba, CRT_ID_CURSOR_START, 0x00);
		WCrt(ba, CRT_ID_CURSOR_END, md->fy & 0x1f);
#endif
		WCrt(ba, CRT_ID_UNDERLINE_LOC, (md->fy - 1) & 0x1f);

		WCrt(ba, CRT_ID_CURSOR_LOC_HIGH, 0x00);
		WCrt(ba, CRT_ID_CURSOR_LOC_LOW, 0x00);
	}
	WCrt(ba, CRT_ID_START_ADDR_HIGH, 0x00);
	WCrt(ba, CRT_ID_START_ADDR_LOW, 0x00);

	WCrt(ba, CRT_ID_START_VER_RETR, VSS);
	WCrt(ba, CRT_ID_END_VER_RETR, (VSE & 0x0f) | 0x20);
	WCrt(ba, CRT_ID_VER_DISP_ENA_END, VDE);
	WCrt(ba, CRT_ID_START_VER_BLANK, VBS);
	WCrt(ba, CRT_ID_END_VER_BLANK, VBE);

	WCrt(ba, CRT_ID_LINE_COMPARE, 0xff);
	WCrt(ba, CRT_ID_LACE_END, HT / 2);	/* MW/16 */
	WCrt(ba, CRT_ID_LACE_CNTL,
	    ((gv->disp_flags & GRF_FLAGS_LACE) ? 0x01 : 0x00) |
	    ((HBE & 0x40) ? 0x10 : 0x00) |
	    ((HBE & 0x80) ? 0x20 : 0x00) |
	    ((VBE & 0x100) ? 0x40 : 0x00) |
	    ((VBE & 0x200) ? 0x80 : 0x00));

	WGfx(ba, GCT_ID_GRAPHICS_MODE,
	    ((TEXT || (gv->depth == 1)) ? 0x00 : 0x40));
	WGfx(ba, GCT_ID_MISC, (TEXT ? 0x04 : 0x01));

	WSeq(ba, SEQ_ID_EXT_SEQ_MODE,
	    ((TEXT || (gv->depth == 1)) ? 0x00 : 0x01) |
	    ((cltype == PICASSO) ? 0x20 : 0x80) | clkmode);

	/* write 0x00 to VDAC_MASK before accessing HDR this helps
	   sometimes, out of "secret" application note (crest) */
	vgaw(ba, VDAC_MASK, 0);
	/* reset HDR "magic" access counter (crest) */
	vgar(ba, VDAC_ADDRESS);

	delay(200000);
	vgar(ba, VDAC_MASK);
	delay(200000);
	vgar(ba, VDAC_MASK);
	delay(200000);
	vgar(ba, VDAC_MASK);
	delay(200000);
	vgar(ba, VDAC_MASK);
	delay(200000);
	switch (gv->depth) {
	case 1:
	case 4:		/* text */
		vgaw(ba, VDAC_MASK, 0);
		HDE = gv->disp_width / 16;
		break;
	case 8:
		if (clkdoub) 
			vgaw(ba, VDAC_MASK, 0x4a); /* Clockdouble Magic */
		else
			vgaw(ba, VDAC_MASK, 0);
		HDE = gv->disp_width / 8;
		break;
	case 15:
		vgaw(ba, VDAC_MASK, 0xd0);
		HDE = gv->disp_width / 4;
		break;
	case 16:
		vgaw(ba, VDAC_MASK, 0xc1);
		HDE = gv->disp_width / 4;
		break;
	case 24:
		vgaw(ba, VDAC_MASK, 0xc5);
		HDE = (gv->disp_width / 8) * 3;
		break;
	case 32:
		vgaw(ba, VDAC_MASK, 0xc5);
		HDE = (gv->disp_width / 4);
		break;
	}

	/* reset HDR "magic" access counter (crest) */
	vgar(ba, VDAC_ADDRESS);
	/* then enable all bit in VDAC_MASK afterwards (crest) */
	vgaw(ba, VDAC_MASK, 0xff);

	WCrt(ba, CRT_ID_OFFSET, HDE);
	if (cl_64bit == 1) {
		WCrt(ba, CRT_ID_SYNC_ADJ_GENLOCK, 0x00);
		WCrt(ba, CRT_ID_OVERLAY_EXT_CTRL_REG, 0x40);
	}
	WCrt(ba, CRT_ID_EXT_DISP_CNTL,
	    ((TEXT && gv->pixel_clock > 29000000) ? 0x40 : 0x00) |
	    0x22 |
	    ((HDE > 0xff) ? 0x10 : 0x00));

	WAttr(ba, ACT_ID_ATTR_MODE_CNTL, (TEXT ? 0x0a : 0x01));
	WAttr(ba, 0x20 | ACT_ID_COLOR_PLANE_ENA,
	    (gv->depth == 1) ? 0x01 : 0x0f);

	/* text initialization */

	if (TEXT) {
		cl_inittextmode(gp);
	}
	WSeq(ba, SEQ_ID_CURSOR_ATTR, 0x14);
	WSeq(ba, SEQ_ID_CLOCKING_MODE, 0x01);

	/* Pass-through */

	RegOffpass(ba);

	return (1);
}

void
cl_inittextmode(gp)
	struct grf_softc *gp;
{
	struct grfcltext_mode *tm = (struct grfcltext_mode *) gp->g_data;
	volatile unsigned char *ba = gp->g_regkva;
	unsigned char *fb = gp->g_fbkva;
	unsigned char *c, *f, y;
	unsigned short z;


	/* load text font into beginning of display memory. Each character
	 * cell is 32 bytes long (enough for 4 planes) */

	SetTextPlane(ba, 0x02);
        cl_memset(fb, 0, 256 * 32);
	c = (unsigned char *) (fb) + (32 * tm->fdstart);
	f = tm->fdata;
	for (z = tm->fdstart; z <= tm->fdend; z++, c += (32 - tm->fy))
		for (y = 0; y < tm->fy; y++)
			*c++ = *f++;

	/* clear out text/attr planes (three screens worth) */

	SetTextPlane(ba, 0x01);
	cl_memset(fb, 0x07, tm->cols * tm->rows * 3);
	SetTextPlane(ba, 0x00);
	cl_memset(fb, 0x20, tm->cols * tm->rows * 3);

	/* print out a little init msg */

	c = (unsigned char *) (fb) + (tm->cols - 16);
	strcpy(c, "CIRRUS");
	c[6] = 0x20;

	/* set colors (B&W) */

	vgaw(ba, VDAC_ADDRESS_W, 0);
	for (z = 0; z < 256; z++) {
		unsigned char r, g, b;

		y = (z & 1) ? ((z > 7) ? 2 : 1) : 0;

		if (cltype == PICASSO) {
			r = clconscolors[y][0];
			g = clconscolors[y][1];
			b = clconscolors[y][2];
		} else {
			b = clconscolors[y][0];
			g = clconscolors[y][1];
			r = clconscolors[y][2];
		}
		vgaw(ba, VDAC_DATA, r >> 2);
		vgaw(ba, VDAC_DATA, g >> 2);
		vgaw(ba, VDAC_DATA, b >> 2);
	}
}

void
cl_memset(d, c, l)
	unsigned char *d;
	unsigned char c;
	int     l;
{
	for (; l > 0; l--)
		*d++ = c;
}

/*
 * Special wakeup/passthrough registers on graphics boards
 *
 * The methods have diverged a bit for each board, so
 * WPass(P) has been converted into a set of specific
 * inline functions.
 */
void
RegWakeup(ba)
	volatile caddr_t ba;
{

	switch (cltype) {
	    case SPECTRUM:
		vgaw(ba, PASS_ADDRESS_W, 0x1f);
		break;
	    case PICASSO:
		/* Picasso needs no wakeup */
		break;
	    case PICCOLO:
		if (cl_64bit == 1)
			vgaw(ba, PASS_ADDRESS_W, 0x1f);
		else
			vgaw(ba, PASS_ADDRESS_W,
			    vgar(ba, PASS_ADDRESS) | 0x10);
		break;
	}
	delay(200000);
}

void
RegOnpass(ba)
	volatile caddr_t ba;
{

	switch (cltype) {
	    case SPECTRUM:
		vgaw(ba, PASS_ADDRESS_W, 0x4f);
		break;
	    case PICASSO:
		vgaw(ba, PASS_ADDRESS_WP, 0x01);
		break;
	    case PICCOLO:
		if (cl_64bit == 1)
			vgaw(ba, PASS_ADDRESS_W, 0x4f);
		else
			vgaw(ba, PASS_ADDRESS_W, vgar(ba, PASS_ADDRESS) & 0xdf);
		break;
	}
	pass_toggle = 1;
	delay(200000);
}

void
RegOffpass(ba)
	volatile caddr_t ba;
{

	switch (cltype) {
	    case SPECTRUM:
		vgaw(ba, PASS_ADDRESS_W, 0x6f);
		break;
	    case PICASSO:
		if (cl_64bit == 0)
			vgaw(ba, PASS_ADDRESS_W, 0xff);
		break;
	    case PICCOLO:
		if (cl_64bit == 1)
			vgaw(ba, PASS_ADDRESS_W, 0x6f);
		else
			vgaw(ba, PASS_ADDRESS_W,
			    vgar(ba, PASS_ADDRESS) | 0x20);
		break;
	}
	pass_toggle = 0;
	delay(200000);
}

#endif /* NGRFCL */

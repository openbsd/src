/*	$OpenBSD: grf_et.c,v 1.9 2002/08/02 16:13:07 millert Exp $	*/
/*	$NetBSD: grf_et.c,v 1.10 1997/07/29 17:46:31 veego Exp $	*/

/*
 * Copyright (c) 1997 Klaus Burkert
 * Copyright (c) 1996 Tobias Abt
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
#include "grfet.h"
#if NGRFET > 0

/*
 * Graphics routines for Tseng ET4000 (&W32) boards,
 *
 * This code offers low-level routines to access Tseng ET4000
 * graphics-boards from within Net- & OpenBSD for the Amiga.
 * No warranties for any kind of function at all - this
 * code may crash your hardware and scratch your harddisk.  Use at your
 * own risk.  Freely distributable.
 *
 * Modified for Tseng ET4000 from
 * Kari Mettinen's Cirrus driver by Tobias Abt
 *
 * Fixed Merlin in Z-III, fixed LACE and DBLSCAN, added Domino16M proto
 * and AT&T ATT20c491 DAC, added memory-size detection by Klaus Burkert. 
 *
 * TODO:
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
#ifdef TSENGCONSOLE
#include <amiga/dev/itevar.h>
#endif
#include <amiga/amiga/device.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_etreg.h>
#include <amiga/dev/zbusvar.h>

int	et_mondefok(struct grfvideo_mode *gv);
void	et_boardinit(struct grf_softc *gp);
void	et_CompFQ(u_int fq, u_char *num, u_char *denom);
int	et_getvmode(struct grf_softc *gp, struct grfvideo_mode *vm);
int	et_setvmode(struct grf_softc *gp, unsigned int mode);
int	et_toggle(struct grf_softc *gp, unsigned short);
int	et_getcmap(struct grf_softc *gfp, struct grf_colormap *cmap);
int	et_putcmap(struct grf_softc *gfp, struct grf_colormap *cmap);
#ifndef TSENGCONSOLE
void	et_off(struct grf_softc *gp);
#endif
void	et_inittextmode(struct grf_softc *gp);
int	et_ioctl(register struct grf_softc *gp, u_long cmd, void *data);
int	et_getmousepos(struct grf_softc *gp, struct grf_position *data);
void	et_writesprpos(volatile char *ba, short x, short y);
int	et_setmousepos(struct grf_softc *gp, struct grf_position *data);
int	et_setspriteinfo(struct grf_softc *gp,
	    struct grf_spriteinfo *data);
int	et_getspriteinfo(struct grf_softc *gp,
	    struct grf_spriteinfo *data);
int	et_getspritemax(struct grf_softc *gp, struct grf_position *data);
int	et_setmonitor(struct grf_softc *gp, struct grfvideo_mode *gv);
int	et_blank(struct grf_softc *gp, int *on);
int	et_getControllerType(struct grf_softc *gp);
int	et_getDACType(struct grf_softc *gp);

int	grfetmatch(struct device *, void *, void *);
void	grfetattach(struct device *, struct device *, void *);
int	grfetprint(void *, const char *);
void	et_memset(unsigned char *d, unsigned char c, int l);

/*
 * Graphics display definitions.
 * These are filled by 'grfconfig' using GRFIOCSETMON.
 */
#define monitor_def_max 24
static struct grfvideo_mode monitor_def[24] = {
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
};
static struct grfvideo_mode *monitor_current = &monitor_def[0];

/* Console display definition.
 *   Default hardcoded text mode.  This grf_et is set up to
 *   use one text mode only, and this is it.  You may use
 *   grfconfig to change the mode after boot.
 */
/* Console font */
#ifdef KFONT_8X11
#define TSENGFONT kernel_font_8x11
#define TSENGFONTY 11
#else
#define TSENGFONT kernel_font_8x8
#define TSENGFONTY 8
#endif
extern unsigned char TSENGFONT[];

struct grfettext_mode etconsole_mode = {
	{ 255, "", 25000000, 640, 480, 4, 640/8, 680/8, 768/8, 800/8,
	  481, 491, 493, 525, 0 },
	8, TSENGFONTY, 640 / 8, 480 / TSENGFONTY, TSENGFONT, 32, 255
};

/* Console colors */
unsigned char etconscolors[3][3] = {	/* background, foreground, hilite */
	{0, 0x40, 0x50}, {152, 152, 152}, {255, 255, 255}
};

int ettype = 0;		/* oMniBus, Domino or Merlin */
int etctype = 0;	/* ET4000 or ETW32 */
int etdtype = 0;	/* Type of DAC (see grf_etregs.h) */

char etcmap_shift = 0;	/* 6 or 8 bit cmap entries */
unsigned char pass_toggle;	/* passthru status tracker */

unsigned char Merlin_switch = 0;

/*
 * Because all Tseng-boards have 2 configdev entries, one for
 * framebuffer mem and the other for regs, we have to hold onto
 * the pointers globally until we match on both.  This and 'ettype'
 * are the primary obsticles to multiple board support, but if you
 * have multiple boards you have bigger problems than grf_et.
 */
static void *et_fbaddr = 0;	/* framebuffer */
static void *et_regaddr = 0;	/* registers */
static int et_fbsize;		/* framebuffer size */

/* current sprite info, if you add support for multiple boards
 * make this an array or something
 */
struct grf_spriteinfo et_cursprite;

/* sprite bitmaps in kernel stack, you'll need to arrayize these too if
 * you add multiple board support
 */
static unsigned char et_imageptr[8 * 64], et_maskptr[8 * 64];
static unsigned char et_sprred[2], et_sprgreen[2], et_sprblue[2];

/* standard driver stuff */
struct cfattach grfet_ca = {
	sizeof(struct grf_softc), grfetmatch, grfetattach
};

struct cfdriver grfet_cd = {
	NULL, "grfet", DV_DULL, NULL, 0
};

static struct cfdata *grfet_cfdata;

int
grfetmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
#ifdef TSENGCONSOLE
	struct cfdata *cfp = match;
#endif
	struct zbus_args *zap;
	static int regprod, regprod2 = 0, fbprod;

	zap = auxp;

#ifndef TSENGCONSOLE
	if (amiga_realconfig == 0)
		return (0);
#endif

	/* Grab the first board we encounter as the preferred one.  This will
	 * allow one board to work in a multiple Tseng board system, but not
	 * multiple boards at the same time.  */
	if (ettype == 0) {
		switch (zap->manid) {
		    case OMNIBUS:
			if (zap->prodid != 0)
				return (0);
			regprod = 0;
			fbprod = 0;
			break;
		    case DOMINO:
			/* 2167/3 is Domino16M proto (crest) */
			if (zap->prodid != 3 && zap->prodid != 2 &&
			    zap->prodid != 1)
				return (0);
			regprod = 2;
			regprod2 = 3;
			fbprod = 1;
			break;
		    case MERLIN:
			if (zap->prodid != 3 && zap->prodid != 4)
				return (0);
			regprod = 4;
			fbprod = 3;
			break;
		    default:
			return (0);
		}
		ettype = zap->manid;
	} else {
		if (ettype != zap->manid) {
			return (0);
		}
	}

	/* Configure either registers or framebuffer in any order */
	/* as said before, oMniBus does not support ProdID */
	if (ettype == OMNIBUS) {
		if (zap->size == 64 * 1024) {
			/* register area */
			et_regaddr = zap->va;
		} else {
			/* memory area */
			et_fbaddr = zap->va;
			et_fbsize = zap->size;
		}
	} else {
		if (zap->prodid == regprod || zap->prodid == regprod2) {
			et_regaddr = zap->va;
		} else {
			if (zap->prodid == fbprod) {
				et_fbaddr = zap->va;
				et_fbsize = zap->size;
			} else {
				return (0);
			}
		}
	}

#ifdef TSENGCONSOLE
	if (amiga_realconfig == 0) {
		grfet_cfdata = cfp;
	}
#endif

	return (1);
}


void
grfetattach(pdp, dp, auxp)
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
	if (!et_regaddr || !et_fbaddr)
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
		gp->g_regkva = (volatile caddr_t) et_regaddr;
		gp->g_fbkva = (volatile caddr_t) et_fbaddr;

		gp->g_unit = GRF_ET4000_UNIT;
		gp->g_mode = et_mode;
		gp->g_conpri = grfet_cnprobe();
		gp->g_flags = GF_ALIVE;

		/* wakeup the board */
		et_boardinit(gp);

#ifdef TSENGCONSOLE
		grfet_iteinit(gp);
		(void) et_load_mon(gp, &etconsole_mode);
#endif
	}

	/*
	 * attach grf (once)
	 */
	if (amiga_config_found(grfet_cfdata, &gp->g_device, gp, grfetprint)) {
		attachflag = 1;
		printf("grfet: %dMB ", et_fbsize / 0x100000);
		switch (ettype) {
		    case OMNIBUS:
			printf("oMniBus");
			break;
		    case DOMINO:
			printf("Domino");
			break;
		    case MERLIN:
			printf("Merlin");
			break;
		}
		printf(" with ");
		switch (etctype) {
		    case ET4000:
			printf("Tseng ET4000");
			break;
		    case ETW32:
			printf("Tseng ETW32");
			break;
		}
		printf(" and ");
		switch (etdtype) {
		    case SIERRA11483:
			printf("Sierra SC11483 DAC");
			break;
		    case SIERRA15025:
			printf("Sierra SC15025 DAC");
			break;
		    case MUSICDAC:
			printf("MUSIC DAC");
			break;
		    case MERLINDAC:
			printf("BrookTree Bt482 DAC");
			break;
		    case ATT20C491:
			printf("AT&T ATT20c491 DAC");
			break;
		}
		printf(" being used\n");
	} else {
		if (!attachflag)
			printf("grfet unattached!!\n");
	}
}


int
grfetprint(auxp, pnp)
	void	*auxp;
	const char *pnp;
{
	if (pnp)
		printf("ite at %s: ", pnp);
	return (UNCONF);
}


void
et_boardinit(gp)
	struct grf_softc *gp;
{
	unsigned char *ba = gp->g_regkva;
	int     x;

	/* wakeup board and flip passthru OFF */

	RegWakeup(ba);
	RegOnpass(ba);

	if (ettype == MERLIN) {
		/* Merlin needs some special initialisations */
		vgaw(ba, MERLIN_SWITCH_REG, 0);
		delay(20000);
		vgaw(ba, MERLIN_SWITCH_REG, 8);
		delay(20000);
		vgaw(ba, MERLIN_SWITCH_REG, 0);
		delay(20000);
		vgaw(ba, MERLIN_VDAC_DATA, 1);

		vgaw(ba, MERLIN_VDAC_INDEX, 0x00);
		vgaw(ba, MERLIN_VDAC_SPRITE,  0xff);
		vgaw(ba, MERLIN_VDAC_INDEX, 0x01);
		vgaw(ba, MERLIN_VDAC_SPRITE,  0x0f);
		vgaw(ba, MERLIN_VDAC_INDEX, 0x02);
		vgaw(ba, MERLIN_VDAC_SPRITE,  0x42);
		vgaw(ba, MERLIN_VDAC_INDEX, 0x03);
		vgaw(ba, MERLIN_VDAC_SPRITE,  0x00);

		vgaw(ba, MERLIN_VDAC_DATA, 0);
	}

	
	/* setup initial unchanging parameters */

	vgaw(ba, GREG_HERCULESCOMPAT + ((ettype == DOMINO) ? 0x0fff : 0), 0x03);
	vgaw(ba, GREG_DISPMODECONTROL, 0xa0);
	vgaw(ba, GREG_MISC_OUTPUT_W, 0x63);

	if (ettype == DOMINO)
	{
		vgaw(ba, CRT_ADDRESS, CRT_ID_VIDEO_CONFIG1);
		vgaw(ba, CRT_ADDRESS_W + 0x0fff,
		    0xc0 | vgar(ba, CRT_ADDRESS_R + 0x0fff));
	}

	WSeq(ba, SEQ_ID_RESET, 0x03);
	WSeq(ba, SEQ_ID_CLOCKING_MODE, 0x21);	/* 8 dot, Display off */
	WSeq(ba, SEQ_ID_MAP_MASK, 0x0f);
	WSeq(ba, SEQ_ID_CHAR_MAP_SELECT, 0x00);
	WSeq(ba, SEQ_ID_MEMORY_MODE, 0x0e);
	WSeq(ba, SEQ_ID_STATE_CONTROL, 0x00);
	WSeq(ba, SEQ_ID_AUXILIARY_MODE, 0xf4);

	WCrt(ba, CRT_ID_PRESET_ROW_SCAN, 0x00);
	WCrt(ba, CRT_ID_CURSOR_START, 0x00);
	WCrt(ba, CRT_ID_CURSOR_END, 0x08);
	WCrt(ba, CRT_ID_START_ADDR_HIGH, 0x00);
	WCrt(ba, CRT_ID_START_ADDR_LOW, 0x00);
	WCrt(ba, CRT_ID_CURSOR_LOC_HIGH, 0x00);
	WCrt(ba, CRT_ID_CURSOR_LOC_LOW, 0x00);

	WCrt(ba, CRT_ID_UNDERLINE_LOC, 0x67);
	WCrt(ba, CRT_ID_MODE_CONTROL, 0xc3);
	WCrt(ba, CRT_ID_LINE_COMPARE, 0xff);

	/* ET4000 special */
	WCrt(ba, CRT_ID_RASCAS_CONFIG, 0x28);
	WCrt(ba, CRT_ID_EXT_START, 0x00);
	WCrt(ba, CRT_ID_6845_COMPAT, 0x08);

	/* ET4000/W32 special (currently only for Merlin (crest) */
	if (ettype == MERLIN) {
		WCrt(ba, CRT_ID_SEGMENT_COMP, 0x1c);
		WCrt(ba, CRT_ID_GENERAL_PURPOSE, 0x00);
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0x93);
	}
	else {
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0xd3);
	}

	WCrt(ba, CRT_ID_VIDEO_CONFIG2, 0x0f);
	WCrt(ba, CRT_ID_HOR_OVERFLOW, 0x00);

	vgaw(ba, GREG_SEGMENTSELECT, 0x00);

	WGfx(ba, GCT_ID_SET_RESET, 0x00);
	WGfx(ba, GCT_ID_ENABLE_SET_RESET, 0x00);
	WGfx(ba, GCT_ID_COLOR_COMPARE, 0x00);
	WGfx(ba, GCT_ID_DATA_ROTATE, 0x00);
	WGfx(ba, GCT_ID_READ_MAP_SELECT, 0x00);
	WGfx(ba, GCT_ID_GRAPHICS_MODE, 0x40);
	WGfx(ba, GCT_ID_MISC, 0x01);
	WGfx(ba, GCT_ID_COLOR_XCARE, 0x0f);
	WGfx(ba, GCT_ID_BITMASK, 0xff);

	for (x = 0; x < 0x10; x++)
		WAttr(ba, x, x);
	WAttr(ba, ACT_ID_ATTR_MODE_CNTL, 0x01);
	WAttr(ba, ACT_ID_OVERSCAN_COLOR, 0x00);
	WAttr(ba, ACT_ID_COLOR_PLANE_ENA, 0x0f);
	WAttr(ba, ACT_ID_HOR_PEL_PANNING, 0x00);
	WAttr(ba, ACT_ID_COLOR_SELECT, 0x00);
	WAttr(ba, ACT_ID_MISCELLANEOUS, 0x00);

	vgaw(ba, VDAC_MASK, 0xff);
	delay(200000);
	vgaw(ba, GREG_MISC_OUTPUT_W, 0xe3);

	/* colors initially set to greyscale */
	switch(ettype) {
	    case MERLIN:
		vgaw(ba, MERLIN_VDAC_INDEX, 0);
		for (x = 255; x >= 0; x--) {
			vgaw(ba, MERLIN_VDAC_COLORS, x);
			vgaw(ba, MERLIN_VDAC_COLORS, x);
			vgaw(ba, MERLIN_VDAC_COLORS, x);
		}
		break;
	    default:
		vgaw(ba, VDAC_ADDRESS_W, 0);
		for (x = 255; x >= 0; x--) {
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0), x);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0), x);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0), x);
		}
		break;
	}
	/* set sprite bitmap pointers */
	/* should work like that */
	et_cursprite.image = et_imageptr;
	et_cursprite.mask = et_maskptr;
	et_cursprite.cmap.red = et_sprred;
	et_cursprite.cmap.green = et_sprgreen;
	et_cursprite.cmap.blue = et_sprblue;

	/* card specific initialisations */
	switch(ettype) {
	    case OMNIBUS:
		etctype = et_getControllerType(gp);
		etdtype = et_getDACType(gp);
		break;
		vgaw(ba, GREG_SEGMENTSELECT2, 0x00);
		if (((vgar(ba, GREG_FEATURE_CONTROL_R) & 12) |
		     (vgar(ba, GREG_STATUS0_R) & 0x60)) == 0x24) {
			WCrt(ba, CRT_ID_VIDEO_CONFIG2, 0x07);	/* 1Mx4 RAM */
			et_fbsize = 0x400000;			/* 4 MB */
		}
		else {
			/* check for 1MB or 2MB board (crest) */
			/* has there a 1MB Merlin ever been sold ??? */
			volatile unsigned long *et_fbtestaddr;
			et_fbtestaddr = (volatile unsigned long *)gp->g_fbkva;
			*et_fbtestaddr = 0x0;
			vgaw(ba, GREG_SEGMENTSELECT2, 0x11); /* 1MB offset */
			*et_fbtestaddr = 0x12345678;
			vgaw(ba, GREG_SEGMENTSELECT2, 0x00);
			if (*et_fbtestaddr == 0x0) 
				et_fbsize = 0x200000;		/* 2 MB */
			else
				et_fbsize = 0x100000;		/* 1 MB */
		}
		/* ZorroII can map 2 MB max ... */
		if (!iszthreepa(gp->g_fbkva) && et_fbsize == 0x400000)
			et_fbsize = 0x200000;
	    case MERLIN:
		etctype = ETW32;
		etdtype = MERLINDAC;
		break;
	    case DOMINO:
		etctype = ET4000;
		etdtype = et_getDACType(gp);
		break;
	}
}


int
et_getvmode(gp, vm)
	struct grf_softc *gp;
	struct grfvideo_mode *vm;
{
	struct grfvideo_mode *gv;

#ifdef TSENGCONSOLE
	/* Handle grabbing console mode */
	if (vm->mode_num == 255) {
		bcopy(&etconsole_mode, vm, sizeof(struct grfvideo_mode));
		/* XXX so grfconfig can tell us the correct text dimensions. */
		vm->depth = etconsole_mode.fy;
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
et_setvmode(gp, mode)
	struct grf_softc *gp;
	unsigned mode;
{
	if (!mode || (mode > monitor_def_max) ||
	    monitor_def[mode - 1].mode_num == 0)
		return (EINVAL);

	monitor_current = monitor_def + (mode - 1);

	return (0);
}


#ifndef TSENGCONSOLE
void
et_off(gp)
	struct grf_softc *gp;
{
	char   *ba = gp->g_regkva;

	RegOnpass(ba);
	WSeq(ba, SEQ_ID_CLOCKING_MODE, 0x21);
}
#endif


int
et_blank(gp, on)
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
et_mode(gp, cmd, arg, a2, a3)
	register struct grf_softc *gp;
	u_long cmd;
	void *arg;
	u_long a2;
	int a3;
{
	int error;

	switch (cmd) {
	    case GM_GRFON:
		error = et_load_mon(gp,
		    (struct grfettext_mode *) monitor_current) ? 0 : EINVAL;
		return (error);

	    case GM_GRFOFF:
#ifndef TSENGCONSOLE
		et_off(gp);
#else
		et_load_mon(gp, &etconsole_mode);
#endif
		return (0);

	    case GM_GRFCONFIG:
		return (0);

	    case GM_GRFGETVMODE:
		return (et_getvmode(gp, (struct grfvideo_mode *) arg));

	    case GM_GRFSETVMODE:
		error = et_setvmode(gp, *(unsigned *) arg);
		if (!error && (gp->g_flags & GF_GRFON))
			et_load_mon(gp,
			    (struct grfettext_mode *) monitor_current);
		return (error);

	    case GM_GRFGETNUMVM:
		*(int *) arg = monitor_def_max;
		return (0);

	    case GM_GRFIOCTL:
		return (et_ioctl(gp, a2, arg));

	    default:
		break;
	}

	return (EINVAL);
}

int
et_ioctl(gp, cmd, data)
	register struct grf_softc *gp;
	u_long cmd;
	void   *data;
{
	switch (cmd) {
	    case GRFIOCGSPRITEPOS:
		return (et_getmousepos(gp, (struct grf_position *) data));

	    case GRFIOCSSPRITEPOS:
		return (et_setmousepos(gp, (struct grf_position *) data));

	    case GRFIOCSSPRITEINF:
		return (et_setspriteinfo(gp, (struct grf_spriteinfo *) data));

	    case GRFIOCGSPRITEINF:
		return (et_getspriteinfo(gp, (struct grf_spriteinfo *) data));

	    case GRFIOCGSPRITEMAX:
		return (et_getspritemax(gp, (struct grf_position *) data));

	    case GRFIOCGETCMAP:
		return (et_getcmap(gp, (struct grf_colormap *) data));

	    case GRFIOCPUTCMAP:
		return (et_putcmap(gp, (struct grf_colormap *) data));

	    case GRFIOCBITBLT:
		break;

	    case GRFTOGGLE:
		return (et_toggle(gp, 0));

	    case GRFIOCSETMON:
		return (et_setmonitor(gp, (struct grfvideo_mode *) data));

	    case GRFIOCBLANK:
		return (et_blank(gp, (int *)data));
	}
	return (EINVAL);
}


int
et_getmousepos(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{
	data->x = et_cursprite.pos.x;
	data->y = et_cursprite.pos.y;

	return (0);
}


void
et_writesprpos(ba, x, y)
	volatile char *ba;
	short   x;
	short   y;
{
}


int
et_setmousepos(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{
	volatile char *ba = gp->g_regkva;
	short rx, ry, prx, pry;

	/* no movement */
	if (et_cursprite.pos.x == data->x && et_cursprite.pos.y == data->y)
		return (0);

	/* current and previous real coordinates */
	rx = data->x - et_cursprite.hot.x;
	ry = data->y - et_cursprite.hot.y;
	prx = et_cursprite.pos.x - et_cursprite.hot.x;
	pry = et_cursprite.pos.y - et_cursprite.hot.y;

	/* if we are/were on an edge, create (un)shifted bitmap --
	 * ripped out optimization (not extremely worthwhile,
	 * and kind of buggy anyhow).
	 */

	/* do movement, save position */
	et_writesprpos(ba, rx < 0 ? 0 : rx, ry < 0 ? 0 : ry);
	et_cursprite.pos.x = data->x;
	et_cursprite.pos.y = data->y;

	return (0);
}


int
et_getspriteinfo(gp, data)
	struct grf_softc *gp;
	struct grf_spriteinfo *data;
{

	return(EINVAL);
}


int
et_setspriteinfo(gp, data)
	struct grf_softc *gp;
	struct grf_spriteinfo *data;
{

	return(EINVAL);
}


int
et_getspritemax(gp, data)
	struct grf_softc *gp;
	struct grf_position *data;
{

	return(EINVAL);
}


int
et_setmonitor(gp, gv)
	struct grf_softc *gp;
	struct grfvideo_mode *gv;
{
	struct grfvideo_mode *md;

	if (!et_mondefok(gv))
		return(EINVAL);

#ifdef TSENGCONSOLE
	/* handle interactive setting of console mode */
	if (gv->mode_num == 255) {
		bcopy(gv, &etconsole_mode.gv, sizeof(struct grfvideo_mode));
		etconsole_mode.gv.hblank_start /= 8;
		etconsole_mode.gv.hsync_start /= 8;
		etconsole_mode.gv.hsync_stop /= 8;
		etconsole_mode.gv.htotal /= 8;
		etconsole_mode.rows = gv->disp_height / etconsole_mode.fy;
		etconsole_mode.cols = gv->disp_width / etconsole_mode.fx;
		if (!(gp->g_flags & GF_GRFON))
			et_load_mon(gp, &etconsole_mode);
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
et_getcmap(gfp, cmap)
	struct grf_softc *gfp;
	struct grf_colormap *cmap;
{
	volatile unsigned char *ba;
	u_char	red[256], green[256], blue[256], *rp, *gp, *bp;
	short	x;
	int	error;

	if (cmap->count == 0 || cmap->index >= 256)
		return 0;

	if (cmap->count > 256 - cmap->index)
		cmap->count = 256 - cmap->index;

	ba = gfp->g_regkva;
	/* first read colors out of the chip, then copyout to userspace */
	x = cmap->count - 1;

	rp = red + cmap->index;
	gp = green + cmap->index;
	bp = blue + cmap->index;

	switch(ettype) {
	    case MERLIN:
		vgaw(ba, MERLIN_VDAC_INDEX, cmap->index);
		do {
			*rp++ = vgar(ba, MERLIN_VDAC_COLORS);
			*gp++ = vgar(ba, MERLIN_VDAC_COLORS);
			*bp++ = vgar(ba, MERLIN_VDAC_COLORS);
		} while (x-- > 0);
		break;
	    default:
		vgaw(ba, VDAC_ADDRESS_R+((ettype==DOMINO)?0x0fff:0), cmap->index);
		do {
			*rp++ = vgar(ba, VDAC_DATA+((ettype==DOMINO)?0x0fff:0)) << etcmap_shift;
			*gp++ = vgar(ba, VDAC_DATA+((ettype==DOMINO)?0x0fff:0)) << etcmap_shift;
			*bp++ = vgar(ba, VDAC_DATA+((ettype==DOMINO)?0x0fff:0)) << etcmap_shift;
		} while (x-- > 0);
		break;
	}

	error = copyout(red + cmap->index, cmap->red, cmap->count);
	if (!error)
		error = copyout(green + cmap->index, cmap->green, cmap->count);
	if (!error)
		error = copyout(blue + cmap->index, cmap->blue, cmap->count);

	return (error);
}


int
et_putcmap(gfp, cmap)
	struct grf_softc *gfp;
	struct grf_colormap *cmap;
{
	volatile unsigned char *ba;
	u_char	red[256], green[256], blue[256], *rp, *gp, *bp;
	short	x;
	int	error;

	if (cmap->count == 0 || cmap->index >= 256)
		return (0);

	if (cmap->count > 256 - cmap->index)
		cmap->count = 256 - cmap->index;

	/* first copy the colors into kernelspace */
	if ((error = copyin(cmap->red, red + cmap->index, cmap->count)))
		return (error);

	if ((error = copyin(cmap->green, green + cmap->index, cmap->count)))
		return (error);

	if ((error = copyin(cmap->blue, blue + cmap->index, cmap->count)))
		return (error);

	ba = gfp->g_regkva;
	x = cmap->count - 1;

	rp = red + cmap->index;
	gp = green + cmap->index;
	bp = blue + cmap->index;

	switch(ettype){
	    case MERLIN:
		vgaw(ba, MERLIN_VDAC_INDEX, cmap->index);
		do {
			vgaw(ba, MERLIN_VDAC_COLORS, *rp++);
			vgaw(ba, MERLIN_VDAC_COLORS, *gp++);
			vgaw(ba, MERLIN_VDAC_COLORS, *bp++);
		} while (x-- > 0);
		break;
	    default:
		vgaw(ba, VDAC_ADDRESS_W, cmap->index);
		do {
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    *rp++ >> etcmap_shift);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    *gp++ >> etcmap_shift);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    *bp++ >> etcmap_shift);
		} while (x-- > 0);
		break;
	}

	return (0);
}


int
et_toggle(gp, wopp)
	struct grf_softc *gp;
	unsigned short wopp;	/* don't need that one yet, ill */
{
	volatile unsigned char *ba;

	ba = gp->g_regkva;

	if (pass_toggle) {
		RegOffpass(ba);
	} else {
		RegOnpass(ba);
	}
	return (0);
}

#define ET_NUMCLOCKS 32

static u_char et_clocks[ET_NUMCLOCKS] = {
	0, 1, 6, 2, 3, 7, 4, 5,
	0, 1, 6, 2, 3, 7, 4, 5,
	0, 1, 6, 2, 3, 7, 4, 5,
	0, 1, 6, 2, 3, 7, 4, 5
};

static u_char et_clockdividers[ET_NUMCLOCKS] = {
	3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0
};

static u_int et_clockfreqs[ET_NUMCLOCKS] = {
	 6293750,  7080500,  7875000,  8125000,
	 9000000,  9375000, 10000000, 11225000,
	12587500, 14161000, 15750000, 16250000,
	18000000, 18750000, 20000000, 22450000,
	25175000, 28322000, 31500000, 32500000,
	36000000, 37500000, 40000000, 44900000,
	50350000, 56644000, 63000000, 65000000,
	72000000, 75000000, 80000000, 89800000
};


void
et_CompFQ(fq, num, denom)
	u_int   fq;
	u_char *num;
	u_char *denom;
{
	int i;

	for (i=0; i < ET_NUMCLOCKS;) {
		if (fq <= et_clockfreqs[i++]) {
			break;
		}
	}

	*num = et_clocks[--i];
	*denom = et_clockdividers[i];

	return;
}


int
et_mondefok(gv)
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
		maxpix = 85000000;
		break;
	case 15:
	case 16:
		maxpix = 45000000;
		break;
	case 24:
		maxpix = 28000000;
		break;
	case 32:
		maxpix = 21000000;
		break;
	default:
		printf("grfet: Illegal depth in mode %d\n", (int)gv->mode_num);
		return (0);
	}

        if (gv->pixel_clock > maxpix) {
		printf("grfet: Pixelclock too high in mode %d\n",
		    (int)gv->mode_num);
		return (0);
	}

	if (gv->disp_flags & GRF_FLAGS_SYNC_ON_GREEN) {
		printf("grfet: sync-on-green is not supported\n");
		return (0);
	}

	return (1);
}


int
et_load_mon(gp, md)
	struct grf_softc *gp;
	struct grfettext_mode *md;
{
	struct grfvideo_mode *gv;
	struct grfinfo *gi;
	volatile unsigned char *ba;
	unsigned char num0, denom0;
	unsigned short HT, HDE, HBS, HBE, HSS, HSE, VDE, VBS, VBE, VSS,
	        VSE, VT;
	unsigned char hvsync_pulse, seq;
	char    TEXT;
	int	hmul;

	/* identity */
	gv = &md->gv;
	TEXT = (gv->depth == 4);

	if (!et_mondefok(gv)) {
		printf("grfet: Monitor definition not ok\n");
		return (0);
	}

	ba = gp->g_regkva;

	/*
	 * Provide all needed information in grf device-independant
	 * locations.
	 */
	gp->g_data = (caddr_t) gv;
	gi = &gp->g_display;
	gi->gd_regaddr = (caddr_t) ztwopa(ba);
	gi->gd_regsize = 64 * 1024;
	gi->gd_fbaddr = (caddr_t) kvtop(gp->g_fbkva);
	gi->gd_fbsize = et_fbsize;
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
	HT  = gv->htotal;
	VBS = gv->vblank_start;
	VSS = gv->vsync_start;
	VSE = gv->vsync_stop;
	VBE = gv->vtotal - 1;
	VT  = gv->vtotal;

	if (TEXT)
		HDE = ((gv->disp_width + md->fx - 1) / md->fx) - 1;
	else
		HDE = (gv->disp_width + 3) / 8 - 1;	/* HBS; */
	VDE = gv->disp_height - 1;

	/* adjustments (crest) */
	switch (gv->depth) {
	case 15:
	case 16:
		hmul = 2;
		break;
	case 24:
		hmul = 3;
		break;
	case 32:
		hmul = 4;
		break;
	default:
		hmul = 1;
		break;
	}

	HDE *= hmul;
	HBS *= hmul;
	HSS *= hmul;
	HSE *= hmul;
	HBE *= hmul;
	HT  *= hmul;

	if (gv->disp_flags & GRF_FLAGS_LACE) {
		VDE /= 2;
		VT = VT + 1;
	}

	if (gv->disp_flags & GRF_FLAGS_DBLSCAN) {
		VDE *= 2;
		VBS *= 2;
		VSS *= 2;
		VSE *= 2;
		VBE *= 2;
		VT  *= 2;
	}

	WSeq(ba, SEQ_ID_MEMORY_MODE, (TEXT || (gv->depth == 1)) ? 0x06 : 0x0e);

	WGfx(ba, GCT_ID_READ_MAP_SELECT, 0x00);
	WSeq(ba, SEQ_ID_MAP_MASK, (gv->depth == 1) ? 0x01 : 0xff);
	WSeq(ba, SEQ_ID_CHAR_MAP_SELECT, 0x00);

	/* Set clock */
	et_CompFQ(gv->pixel_clock * hmul, &num0, &denom0);

	/* Horizontal/Vertical Sync Pulse */
	hvsync_pulse = 0xe3;
	if (gv->disp_flags & GRF_FLAGS_PHSYNC)
		hvsync_pulse &= ~0x40;
	else
		hvsync_pulse |= 0x40;
	if (gv->disp_flags & GRF_FLAGS_PVSYNC)
		hvsync_pulse &= ~0x80;
	else
		hvsync_pulse |= 0x80;

	vgaw(ba, GREG_MISC_OUTPUT_W, hvsync_pulse | ((num0 & 3) << 2));
	WCrt(ba, CRT_ID_6845_COMPAT, (num0 & 4) ? 0x0a : 0x08);
	seq = RSeq(ba, SEQ_ID_CLOCKING_MODE);
	switch(denom0) {
	case 0:
		WSeq(ba, SEQ_ID_AUXILIARY_MODE, 0xb4);
		WSeq(ba, SEQ_ID_CLOCKING_MODE, seq & 0xf7);
 		break;
	case 1:
		WSeq(ba, SEQ_ID_AUXILIARY_MODE, 0xf4);
		WSeq(ba, SEQ_ID_CLOCKING_MODE, seq & 0xf7);
		break;
	case 2:
		WSeq(ba, SEQ_ID_AUXILIARY_MODE, 0xf5);
		WSeq(ba, SEQ_ID_CLOCKING_MODE, seq & 0xf7);
		break;
	case 3:
		WSeq(ba, SEQ_ID_AUXILIARY_MODE, 0xf5);
		WSeq(ba, SEQ_ID_CLOCKING_MODE, seq | 0x08);
		break;
	}
 
	/* load display parameters into board */
	WCrt(ba, CRT_ID_HOR_TOTAL, HT);
	WCrt(ba, CRT_ID_HOR_DISP_ENA_END, ((HDE >= HBS) ? HBS - 1 : HDE));
	WCrt(ba, CRT_ID_START_HOR_BLANK, HBS);
	WCrt(ba, CRT_ID_END_HOR_BLANK, (HBE & 0x1f) | 0x80);
	WCrt(ba, CRT_ID_START_HOR_RETR, HSS);
	WCrt(ba, CRT_ID_END_HOR_RETR,
	    (HSE & 0x1f) |
	    ((HBE & 0x20) ? 0x80 : 0x00));
	WCrt(ba, CRT_ID_VER_TOTAL, VT);
	WCrt(ba, CRT_ID_OVERFLOW,
	    0x10 |
	    ((VT  & 0x100) ? 0x01 : 0x00) |
	    ((VDE & 0x100) ? 0x02 : 0x00) |
	    ((VSS & 0x100) ? 0x04 : 0x00) |
	    ((VBS & 0x100) ? 0x08 : 0x00) |
	    ((VT  & 0x200) ? 0x20 : 0x00) |
	    ((VDE & 0x200) ? 0x40 : 0x00) |
	    ((VSS & 0x200) ? 0x80 : 0x00));

	WCrt(ba, CRT_ID_MAX_ROW_ADDRESS,
	    0x40 |		/* splitscreen not visible */
	    ((gv->disp_flags & GRF_FLAGS_DBLSCAN) ? 0x80 : 0x00) |
	    ((VBS & 0x200) ? 0x20 : 0x00) |
	    (TEXT ? ((md->fy - 1) & 0x1f) : 0x00));

	WCrt(ba, CRT_ID_MODE_CONTROL,
	    ((TEXT || (gv->depth == 1)) ? 0xc3 : 0xab));

	/* text cursor */
	if (TEXT) {
#if ET_ULCURSOR
		WCrt(ba, CRT_ID_CURSOR_START, (md->fy & 0x1f) - 2);
		WCrt(ba, CRT_ID_CURSOR_END, (md->fy & 0x1f) - 1);
#else
		WCrt(ba, CRT_ID_CURSOR_START, 0x00);
		WCrt(ba, CRT_ID_CURSOR_END, md->fy & 0x1f);
#endif
		WCrt(ba, CRT_ID_CURSOR_LOC_HIGH, 0x00);
		WCrt(ba, CRT_ID_CURSOR_LOC_LOW, 0x00);
	}

	WCrt(ba, CRT_ID_UNDERLINE_LOC, ((md->fy - 1) & 0x1f)
		| ((TEXT || (gv->depth == 1)) ? 0x00 : 0x60));

	WCrt(ba, CRT_ID_START_ADDR_HIGH, 0x00);
	WCrt(ba, CRT_ID_START_ADDR_LOW, 0x00);

	WCrt(ba, CRT_ID_START_VER_RETR, VSS);
	WCrt(ba, CRT_ID_END_VER_RETR, (VSE & 0x0f) | 0x30);
	WCrt(ba, CRT_ID_VER_DISP_ENA_END, VDE);
	WCrt(ba, CRT_ID_START_VER_BLANK, VBS);
	WCrt(ba, CRT_ID_END_VER_BLANK, VBE);

	WCrt(ba, CRT_ID_LINE_COMPARE, 0xff);

	WCrt(ba, CRT_ID_OVERFLOW_HIGH,
	    ((VBS & 0x400) ? 0x01 : 0x00) |
	    ((VT  & 0x400) ? 0x02 : 0x00) |
	    ((VDE & 0x400) ? 0x04 : 0x00) |
	    ((VSS & 0x400) ? 0x08 : 0x00) |
	    0x10 |
	    ((gv->disp_flags & GRF_FLAGS_LACE) ? 0x80 : 0x00));

	WCrt(ba, CRT_ID_HOR_OVERFLOW,
	    ((HT  & 0x100) ? 0x01 : 0x00) |
	    ((HBS & 0x100) ? 0x04 : 0x00) |
	    ((HSS & 0x100) ? 0x10 : 0x00)
	);

	/* depth dependent stuff */

	WGfx(ba, GCT_ID_GRAPHICS_MODE,
	    ((TEXT || (gv->depth == 1)) ? 0x00 : 0x40));
	WGfx(ba, GCT_ID_MISC, (TEXT ? 0x04 : 0x01));

	vgaw(ba, VDAC_MASK, 0xff);
	vgar(ba, VDAC_MASK);
	vgar(ba, VDAC_MASK);
	vgar(ba, VDAC_MASK);
	vgar(ba, VDAC_MASK);
	switch (gv->depth) {
	case 1:
	case 4:	/* text */
		switch(etdtype) {
		case SIERRA11483:
		case SIERRA15025:
		case MUSICDAC:
			vgaw(ba, VDAC_MASK, 0);
			break;
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0x02);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0);
			break;
		}
		HDE = gv->disp_width / 16;
		break;
	case 8:
		switch(etdtype) {
		case SIERRA11483:
		case SIERRA15025:
		case MUSICDAC:
			vgaw(ba, VDAC_MASK, 0);
			break;
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0x02);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0);
			break;
		}
		HDE = gv->disp_width / 8;
		break;
	case 15:
		switch(etdtype) {
		case SIERRA11483:
		case SIERRA15025:
		case MUSICDAC:
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0xa0);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0xa0);
			break;
		}
		HDE = gv->disp_width / 4;
		break;
	case 16:
		switch(etdtype) {
		case SIERRA11483:
			vgaw(ba, VDAC_MASK, 0);	/* illegal mode! */
			break;
		case SIERRA15025:
			vgaw(ba, VDAC_MASK, 0xe0);
			break;
		case MUSICDAC:
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0xc0);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0xe0);
			break;
		}
		HDE = gv->disp_width / 4;
		break;
	case 24:
		switch(etdtype) {
		case SIERRA11483:
			vgaw(ba, VDAC_MASK, 0);	/* illegal mode! */
			break;
		case SIERRA15025:
			vgaw(ba, VDAC_MASK, 0xe1);
			break;
		case MUSICDAC:
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0xe0);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0xf0);
			break;
		}
		HDE = (gv->disp_width / 8) * 3;
		break;
	case 32:
		switch(etdtype) {
		case SIERRA11483:
		case MUSICDAC:
		case ATT20C491:
			vgaw(ba, VDAC_MASK, 0);	/* illegal mode! */
			break;
		case SIERRA15025:
			vgaw(ba, VDAC_MASK, 0x61);
			break;
		case MERLINDAC:
			setMerlinDACmode(ba, 0xb0);
			break;
		}
		HDE = gv->disp_width / 2;
		break;
	}
	WAttr(ba, ACT_ID_ATTR_MODE_CNTL, (TEXT ? 0x0a : 0x01));
	WAttr(ba, 0x20 | ACT_ID_COLOR_PLANE_ENA,
	    (gv->depth == 1) ? 0x01 : 0x0f);

	WCrt(ba, CRT_ID_OFFSET, HDE);
	vgaw(ba, CRT_ADDRESS, CRT_ID_HOR_OVERFLOW);
	vgaw(ba, CRT_ADDRESS_W,
	    (vgar(ba, CRT_ADDRESS_R) & 0x7f) | ((HDE & 0x100) ? 0x80: 0x00));

	/* text initialization */
	if (TEXT) {
		et_inittextmode(gp);
	}

	WSeq(ba, SEQ_ID_CLOCKING_MODE, 0x01);

	/* Pass-through */
	RegOffpass(ba);

	return (1);
}


void
et_inittextmode(gp)
	struct grf_softc *gp;
{
	struct grfettext_mode *tm = (struct grfettext_mode *) gp->g_data;
	volatile unsigned char *ba = gp->g_regkva;
	unsigned char *fb = gp->g_fbkva;
	unsigned char *c, *f, y;
	unsigned short z;


	/*
	 * load text font into beginning of display memory. Each character
	 * cell is 32 bytes long (enough for 4 planes)
	 */

	SetTextPlane(ba, 0x02);
        et_memset(fb, 0, 256 * 32);
	c = (unsigned char *) (fb) + (32 * tm->fdstart);
	f = tm->fdata;
	for (z = tm->fdstart; z <= tm->fdend; z++, c += (32 - tm->fy))
		for (y = 0; y < tm->fy; y++)
			*c++ = *f++;

	/* clear out text/attr planes (three screens worth) */

	SetTextPlane(ba, 0x01);
	et_memset(fb, 0x07, tm->cols * tm->rows * 3);
	SetTextPlane(ba, 0x00);
	et_memset(fb, 0x20, tm->cols * tm->rows * 3);

	/* print out a little init msg */

	c = (unsigned char *) (fb) + (tm->cols - 16);
	strcpy(c, "TSENG");
	c[5] = 0x20;

	/* set colors (B&W) */
	switch(ettype) {
	    case MERLIN:
		vgaw(ba, MERLIN_VDAC_INDEX, 0);
		for (z = 0; z < 256; z++) {
			y = (z & 1) ? ((z > 7) ? 2 : 1) : 0;

			vgaw(ba, MERLIN_VDAC_COLORS, etconscolors[y][0]);
			vgaw(ba, MERLIN_VDAC_COLORS, etconscolors[y][1]);
			vgaw(ba, MERLIN_VDAC_COLORS, etconscolors[y][2]);
		}
		break;
	    default:
		vgaw(ba, VDAC_ADDRESS_W, 0);
		for (z = 0; z < 256; z++) {
			y = (z & 1) ? ((z > 7) ? 2 : 1) : 0;

			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    etconscolors[y][0] >> etcmap_shift);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    etconscolors[y][1] >> etcmap_shift);
			vgaw(ba, VDAC_DATA + ((ettype == DOMINO) ? 0x0fff : 0),
			    etconscolors[y][2] >> etcmap_shift);
		}
		break;
	}
}


void
et_memset(d, c, l)
	unsigned char *d;
	unsigned char c;
	int     l;
{
	for (; l > 0; l--)
		*d++ = c;
}


int
et_getControllerType(gp)
	struct grf_softc * gp;
{
	unsigned char *ba = gp->g_regkva; /* register base */
	unsigned char *mem = gp->g_fbkva; /* memory base */
	unsigned char *mmu = mem + MMU_APERTURE0; /* MMU aperture 0 base */

	*mem = 0;

	/* make ACL visible */
	if (ettype == MERLIN) {
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0xbb);
	} else {
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0xfb);
	}

	WIma(ba, IMA_PORTCONTROL, 0x01);

	*((unsigned long *)mmu) = 0;
	*(mem + 0x13) = 0x38;

	*mmu = 0xff;

	/* hide ACL */
	WIma(ba, IMA_PORTCONTROL, 0x00);

	if (ettype == MERLIN) {
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0x93);
	} else {
		WCrt(ba, CRT_ID_VIDEO_CONFIG1, 0xd3);
	}
	return ((*mem == 0xff) ? ETW32 : ET4000);
}


int
et_getDACType(gp)
	struct grf_softc * gp;
{
	unsigned char *ba = gp->g_regkva;
	union {
		int  tt;
		char cc[4];
	} check;

	/* check for Sierra SC 15025 */

	/* We MUST do 4 HW reads to switch into command mode */
	if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR));
		vgaw(ba, VDAC_COMMAND, 0x10); /* set ERPF */

	vgaw(ba, VDAC_XINDEX, 9);
	check.cc[0] = vgar(ba, VDAC_XDATA);
	vgaw(ba, VDAC_XINDEX, 10);
	check.cc[1] = vgar(ba, VDAC_XDATA);
	vgaw(ba, VDAC_XINDEX, 11);
	check.cc[2] = vgar(ba, VDAC_XDATA);
	vgaw(ba, VDAC_XINDEX, 12);
	check.cc[3] = vgar(ba, VDAC_XDATA);

	if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR));
		vgaw(ba, VDAC_COMMAND, 0x00); /* clear ERPF */

	if (check.tt == 0x533ab141) {
		if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR));
			vgaw(ba, VDAC_COMMAND, 0x10); /* set ERPF */

		/* switch to 8 bits per color */
		vgaw(ba, VDAC_XINDEX, 8);
		vgaw(ba, VDAC_XDATA, 1);
		/* do not shift color values */
		etcmap_shift = 0;

		if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR));
			vgaw(ba, VDAC_COMMAND, 0x00); /* clear ERPF */

		vgaw(ba, VDAC_MASK, 0xff);
		return (SIERRA15025);
	}

	/* check for MUSIC DAC */

	if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR)); if (vgar(ba, HDR));
		vgaw(ba, VDAC_COMMAND, 0x02);	/* set some strange MUSIC mode (???) */

	vgaw(ba, VDAC_XINDEX, 0x01);
	if (vgar(ba, VDAC_XDATA) == 0x01) {
		/* shift color values by 2 */
		etcmap_shift = 2;

		vgaw(ba, VDAC_MASK, 0xff);
		return (MUSICDAC);
	}

	/* check for AT&T ATT20c491 DAC (crest) */
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	vgaw(ba, HDR, 0xff);
	vgaw(ba, VDAC_MASK, 0x01);
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	if (vgar(ba, HDR) == 0xff) {
		/* do not shift color values */
		etcmap_shift = 0;

		vgaw(ba, VDAC_MASK, 0xff);
		return (ATT20C491);
	}

	/* restore PowerUp settings (crest) */
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	if (vgar(ba, HDR)); if (vgar(ba, HDR));
	vgaw(ba, HDR, 0x00);

	/*
	 * nothing else found, so let us pretend it is a stupid
	 * Sierra SC 11483
	 */

	/* shift color values by 2 */
	etcmap_shift = 2;

	vgaw(ba, VDAC_MASK, 0xff);
	return (SIERRA11483);
}

#endif /* NGRFET */

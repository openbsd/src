/*	$NetBSD: cfb.c,v 1.24 1996/10/13 13:13:52 jonathan Exp $	*/

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
 *	from: @(#)sfb.c	8.1 (Berkeley) 6/10/93
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

#include "fb.h"
#include "cfb.h"

#if NCFB > 0
#include <sys/param.h>
#include <sys/systm.h>					/* printf() */
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>

#include <machine/cpuregs.h>		/* mips cached->uncached */
#include <machine/pmioctl.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>
#include <pmax/dev/cfbvar.h>		/* XXX dev/tc ? */

#include <pmax/pmax/pmaxtype.h>

#include <pmax/dev/bt459.h>
#include <pmax/dev/fbreg.h>

#include <machine/autoconf.h>


#define PMAX	/* enable /dev/pm compatibility */

/*
 * These need to be mapped into user space.
 */
struct fbuaccess cfbu;
struct pmax_fbtty cfbfb;
struct fbinfo	cfbfi;	/*XXX*/ /* should be softc */


/*
 * Forward references.
 */

extern struct cfdriver cfb_cd;

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [CMAP_BITS];		/* colormap for console... */

/*
 * Method table for standard framebuffer operations on a CFB.
 * The  CFB uses a Brooktree bt479 ramdac.
 */
struct fbdriver cfb_driver = {
	bt459_video_on,
	bt459_video_off,
	bt459InitColorMap,
	bt459GetColorMap,
	bt459LoadColorMap,
	bt459PosCursor,
	bt459LoadCursor,
	bt459CursorColor
};

int cfbinit __P((struct fbinfo *fi, caddr_t cfbaddr, int unit, int silent));
extern void fbScreenInit __P((struct fbinfo *fi));


extern int pmax_boardtype;


#define	CFB_OFFSET_VRAM		0x0		/* from module's base */
#define CFB_OFFSET_BT459	0x200000	/* Bt459 registers */
#define CFB_OFFSET_IREQ		0x300000	/* Interrupt req. control */
#define CFB_OFFSET_ROM		0x380000	/* Diagnostic ROM */
#define CFB_OFFSET_RESET	0x3c0000	/* Bt459 resets on writes */
#define CFB_FB_SIZE		0x100000	/* frame buffer size */

/*
 * Autoconfiguration data for config.new.
 * Use static-sized softc until config.old and old autoconfig
 * code is completely gone.
 */

int cfbmatch __P((struct device *, void *, void *));
void cfbattach __P((struct device *, struct device *, void *));
int cfb_intr __P((void *sc));

struct cfattach cfb_ca = {
	sizeof(struct fbinfo), cfbmatch, cfbattach
};

struct cfdriver cfb_cd = {
	NULL, "cfb", DV_DULL
};



int
cfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	/*struct cfdata *cf = match;*/
	struct tc_attach_args *ta = aux;

#ifdef FBDRIVER_DOES_ATTACH
	/* leave configuration  to the fb driver */
	return 0;
#endif

	/* make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "PMAG-BA "))
		return (0);

	return (1);
}

/*
 * Attach a device.  Hand off all the work to cfbinit(),
 * so console-config code can attach cfb devices very early in boot,
 * to use as system console.
 */
void
cfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_attach_args *ta = aux;
	caddr_t base = 	(caddr_t)(ta->ta_addr);
	int unit = self->dv_unit;
	struct fbinfo *fi = (struct fbinfo *) self;

#ifdef notyet
	/* if this is the console, it's already configured. */
	if (ca->ca_slotpri == cons_slot)
		return;	/* XXX patch up softc pointer */
#endif

	if (!cfbinit(fi, base, unit, 0))
		return;

	/*
	 * The only interrupt on the CFB proper is the vertical-blank
	 * interrupt, which cannot be disabled. The CFB always requests
	 * an interrupt during every vertical-retrace period.
	 * We never enable interrupts from CFB cards, except on the
	 * 3MIN, where TC options interrupt at spl0 through spl2, and
	 * disabling of TC option interrupts doesn't work.
	 */
	if (pmax_boardtype == DS_3MIN) {
		tc_intr_establish(parent, (void*)ta->ta_cookie, TC_IPL_NONE,
				  cfb_intr, fi);
	}
	printf("\n");
}



/*
 * CFB initialization.  This is divorced from cfbattch() so that
 * a console framebuffer can be initialized early during boot.
 */
int
cfbinit(fi, cfbaddr, unit, silent)
	struct fbinfo *fi;
	caddr_t cfbaddr;
	int unit;
	int silent;
{
	/*
	 * If this device is being intialized as the console, malloc()
	 * is not yet up and we must use statically-allocated space.
	 */
	if (fi == NULL) {
		fi = &cfbfi;	/* XXX */
  		fi->fi_cmap_bits = (caddr_t)cmap_bits;
	}
	else {
    		fi->fi_cmap_bits = malloc(CMAP_BITS, M_DEVBUF, M_NOWAIT);
		if (fi->fi_cmap_bits == NULL) {
			printf("cfb%d: no memory for cmap\n", unit);
			return (0);
		}
	}

	/* check for no frame buffer */
	if (badaddr(cfbaddr, 4)) {
		printf("cfb: bad address 0x%p\n", cfbaddr);
		return (0);
	}

	/* XXX  fi should be a pointer to a field in the softc */

	/* Fill in main frame buffer info struct. */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)(cfbaddr + CFB_OFFSET_VRAM);
	fi->fi_pixelsize = 1024 * 864;
	fi->fi_base = (caddr_t)(cfbaddr + CFB_OFFSET_VRAM);
	fi->fi_vdac = (caddr_t)(cfbaddr + CFB_OFFSET_BT459);
	fi->fi_size = (fi->fi_pixels + CFB_FB_SIZE) - fi->fi_base;
	fi->fi_linebytes = 1024;
	fi->fi_driver = &cfb_driver;
	fi->fi_blanked = 0;

	/* Fill in Frame Buffer Type struct. */
	fi->fi_type.fb_boardtype = PMAX_FBTYPE_CFB;
	fi->fi_type.fb_width = 1024;
	fi->fi_type.fb_height = 864;
	fi->fi_type.fb_depth = 8;
	fi->fi_type.fb_cmsize = 256;
	fi->fi_type.fb_size = CFB_FB_SIZE;


	/*
	 * Reset the chip.   (Initializes colormap for us as a
	 * "helpful"  side-effect.)
	 */
	if (!bt459init(fi)) {
		printf("cfb%d: vdac init failed.\n", unit);
		return (0);
	}
	/*cfbInitColorMap();*/  /* done by bt459init() */


	/*
	 * qvss/pm-style mmap()ed event queue compatibility glue
	 */

	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MACH_PHYS_TO_UNCACHED(MACH_CACHED_TO_PHYS(&cfbu));

	/* This is glass-tty state but it's in the shared structure. Ick. */
	fi->fi_fbu->scrInfo.max_row = 56;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/*
	 * Initialize old-style pmax glass-tty screen info.
	 */
	fi->fi_glasstty = &cfbfb;


	/*
	 * Initialize the color map, the screen, and the mouse.
	 */
	if (tb_kbdmouseconfig(fi)) {
		printf(" (mouse/keyboard config failed)");
		return (0);
	}


	/*
	 * Connect to the raster-console pseudo-driver
	 */
	fbconnect("PMAG-BA", fi, silent);


#ifdef fpinitialized
	fp->initialized = 1;
#endif
	return (1);
}


/*
 * The original TURBOChannel cfb interrupts on every vertical
 * retrace, and we can't disable the board from requesting those
 * interrupts.  The 4.4BSD kernel never enabled those interrupts;
 * but there's a kernel design bug on the 3MIN, where disabling
 * (or enabling) TC option interrupts has no effect the interrupts
 * are mapped to R3000 interrupts and always seem to be taken.
 * This function simply dismisses CFB interrupts, or the interrupt
 * request from the card will still be active.
 */
int
cfb_intr(sc)
	void *sc;
{
	struct fbinfo *fi = /* XXX (struct fbinfo *)sc */ &cfbfi;
	
	char *slot_addr = (((char *)fi->fi_base) - CFB_OFFSET_VRAM);
	
	/* reset vertical-retrace interrupt by writing a dont-care */
	*(int*) (slot_addr+CFB_OFFSET_IREQ) = 0;

	return (0);
}

#endif /* NCFB */

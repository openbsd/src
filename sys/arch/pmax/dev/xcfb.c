/*	$NetBSD: xcfb.c,v 1.20 1996/10/14 01:39:57 mhitch Exp $	*/

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
 *	@(#)xcfb.c	8.1 (Berkeley) 6/10/93
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

#include "xcfb.h"
#include "dtop.h"
#if NXCFB > 0
#if NDTOP == 0
xcfb needs dtop device
#else

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <machine/autoconf.h>
#include <machine/cpuregs.h>		/* mips cached->uncached */

#include <machine/pmioctl.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/pmax/cons.h>

#include <pmax/dev/xcfbreg.h>
#include <pmax/dev/xcfbvar.h>
#include <pmax/dev/ims332.h>
#include <pmax/pmax/maxine.h>

#include <pmax/dev/dtopreg.h>

#include <pmax/dev/fbreg.h>

/*
 * These need to be mapped into user space.
 */
struct fbuaccess xcfbu;


/*
 * rcons methods and globals.
 */
struct pmax_fbtty xcfbfb;

struct fbinfo	xcfbfi;	/*XXX*/
extern struct cfdriver cfb_cd;

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [CMAP_BITS];		/* colormap for console... */

#define XCFB_FB_SIZE 0x100000	/* size of raster (mapped into userspace) */


struct fbdriver xcfb_driver = {
	ims332_video_on,
	ims332_video_off,
	ims332InitColorMap,
	ims332GetColorMap,
	ims332LoadColorMap,
	ims332PosCursor,
	ims332LoadCursor,
	ims332CursorColor
};


/*
 * Forward references.
 */
extern u_short defCursor[32];


/*
 * Autoconfiguration data for config.new.
 * Use static-sized softc until config.old and old autoconfig
 * code is completely gone.
 */

int xcfbmatch __P((struct device *, void *, void *));
void xcfbattach __P((struct device *, struct device *, void *));

struct cfattach xcfb_ca = {
	sizeof(struct device), xcfbmatch, xcfbattach
};

struct cfdriver xcfb_cd = {
	NULL, "xcfb", DV_DULL
};

int
xcfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	/*struct cfdata *cf = match;*/
	struct tc_attach_args *ta = aux;

	/* Make sure that it's an xcfb. */
	if (!TC_BUS_MATCHNAME(ta, "PMAG-DV ")  &&
	    strcmp(ta->ta_modname, "xcfb") != 0)
		return (0);

	return (1);
}

void
xcfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (!xcfbinit(NULL, (caddr_t)ta->ta_addr, self->dv_unit, 0))
		return;

	/* no interrupts for XCFB */
	/*BUS_INTR_ESTABLISH(ca, xcfbintr, self->dv_unit);*/
	printf("\n");
}


/*
 * Initialization
 */
int
xcfbinit(fi, base, unit, silent)
	struct fbinfo *fi;
	caddr_t base;
	int unit;
	int silent;
{
	/*XXX*/
	/*
	 * If this device is being intialized as the console, malloc()
	 * is not yet up and we must use statically-allocated space.
	 */
	if (fi == NULL) {
		fi = &xcfbfi;	/* XXX */
  		fi->fi_cmap_bits = (caddr_t)cmap_bits;
	} else {
    		fi->fi_cmap_bits = malloc(CMAP_BITS, M_DEVBUF, M_NOWAIT);
		if (fi->fi_cmap_bits == NULL) {
			printf("cfb%d: no memory for cmap\n", unit);
			return (0);
		}
	}

	/*XXX*/
	/*
	 * Or Cached? A comment in the Mach driver suggests that the X server
	 * runs faster in cached address space, but the X server is going
	 * to blow away the data cache whenever it updates the screen, so..
	 */
	base = (char *) MIPS_PHYS_TO_KSEG1(XINE_PHYS_CFB_START);
	
	/* Fill in main frame buffer info struct. */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)(base + VRAM_OFFSET);
	fi->fi_pixelsize = 1024 * 768;
	fi->fi_base = (caddr_t)IMS332_RESET_ADDRESS;
	fi->fi_vdac = (caddr_t)IMS332_ADDRESS;
	fi->fi_size = 0x100000;
	fi->fi_linebytes = 1024;
	fi->fi_driver = &xcfb_driver;
	fi->fi_blanked = 0;

	/* Fill in Frame Buffer Type struct. */
	fi->fi_type.fb_boardtype = PMAX_FBTYPE_XCFB;
	fi->fi_type.fb_width = 1024;
	fi->fi_type.fb_height = 768;
	fi->fi_type.fb_depth = 8;
	fi->fi_type.fb_cmsize = 256;
	fi->fi_type.fb_size = XCFB_FB_SIZE;

	/*
	 * qvss/pm-style mmap()ed event queue compatibility glue
	 */
	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MIPS_PHYS_TO_KSEG1(MIPS_KSEG0_TO_PHYS(&xcfbu));

	/* This is glass-tty state but it's in the shared structure. Ick. */
	fi->fi_fbu->scrInfo.max_row = 50;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/*
	 * Initialize old-style pmax glass-tty screen info.
	 */
	fi->fi_glasstty = &xcfbfb;

	/*XXX*/
	/* dimensions translated -15 pixels, for sprite origin? */
	fi->fi_fbu->scrInfo.max_cur_x = 1008;
	fi->fi_fbu->scrInfo.max_cur_y = 752;

	fi->fi_fbu->scrInfo.min_cur_x = -15;
	fi->fi_fbu->scrInfo.min_cur_y = -15;

	/* Initialize the RAMDAC. */
	ims332init (fi);


	/* Connect serial device(s) */
	if (tb_kbdmouseconfig(fi)) {
		printf(" (mouse/keyboard config failed)");
		return (0);
	}

	/*
	 * Connect to the raster-console pseudo-driver
	 */
	fbconnect("PMAG-DV", fi, silent);

#ifdef	fpinitialized
	fp->initialized = 1;
#endif
	return (1);
}

#endif /* NDTOP */
#endif /* NXCFB */

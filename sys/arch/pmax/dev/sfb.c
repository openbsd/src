/*	$NetBSD: sfb.c,v 1.4 1995/09/12 07:30:45 jonathan Exp $	*/

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
 *      $Id: sfb.c,v 1.1.1.1 1995/10/18 08:51:28 deraadt Exp $
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

#include <fb.h>
#include <sfb.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/fcntl.h>

#include <machine/autoconf.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>


#include <pmax/dev/bt459.h>
#include <pmax/dev/sfbreg.h>

#include <machine/machConst.h>
#include <pmax/pmax/pmaxtype.h>
#include <machine/pmioctl.h>
#include <pmax/dev/fbreg.h>

/*
 * These need to be mapped into user space.
 */
struct fbuaccess sfbu;
struct pmax_fbtty sfbfb;
struct fbinfo	sfbfi;	/*XXX*/ /* should be softc */


extern int pmax_boardtype;

/*
 * Forward references.
 */

int sfbinit (char *, int, int);

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [NSFB * CMAP_BITS];	/* One colormap per sfb... */

int sfbmatch __P((struct device *, void *, void *));
void sfbattach __P((struct device *, struct device *, void *));

struct cfdriver sfbcd = {
	NULL, "sfb", sfbmatch, sfbattach, DV_DULL, sizeof(struct device), 0
};

struct fbdriver sfb_driver = {
	bt459_video_on,
	bt459_video_off,
	bt459InitColorMap,
	bt459GetColorMap,
	bt459LoadColorMap,
	bt459PosCursor,
	bt459LoadCursor,
	bt459CursorColor
};


/* match and attach routines cut-and-pasted from cfb */

int
sfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	static int nsfbs = 1;
	caddr_t sfbaddr = BUS_CVTADDR(ca);

	/* make sure that we're looking for this type of device. */
	/*if (!sfbprobe(sfbaddr)) return 0;*/
	if (!BUS_MATCHNAME(ca, "PMAGB-BA"))
		return (0);


#ifdef notyet
	/* if it can't have the one mentioned, reject it */
	if (cf->cf_unit >= nsfbs)
		return (0);
#endif
	return (1);
}

/*
 * Attach a device.  Hand off all the work to sfbinit(),
 * so console-config cod can attach sfbs early in boot.
 */
void
sfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	caddr_t base = 	BUS_CVTADDR(ca);
	int unit = self->dv_unit;

#ifdef notyet
	/* if this is the console, it's already configured. */
	if (ca->ca_slotpri == cons_slot)
		return;	/* XXX patch up f softc pointer */
#endif

	if (!sfbinit(base, unit, 0))
		return;

#if 0 /*XXX*/
	*(base + SFB_INTERRUPT_ENABLE) = 0;
#endif

}

/*
 * Test to see if device is present.
 * Return true if found and initialized ok.
 */
/*ARGSUSED*/
sfbprobe(cp)
	struct pmax_ctlr *cp;
{
}

/*
 * Initialization
 */
int
sfbinit(base, unit, silent)
	char *base;
	int unit;
	int silent;
{
	struct fbinfo *fi;
	u_char foo;

	fi = &sfbfi;	/* XXX use softc */

	if (unit > NSFB)
		return (0);

	/* check for no frame buffer */
	if (badaddr(base + SFB_OFFSET_VRAM, 4))
		return (0);

	/* Fill in main frame buffer info struct. */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)(base + SFB_OFFSET_VRAM);
	fi->fi_pixelsize = 1280 * 1024;
	fi->fi_base = (caddr_t)(base + SFB_ASIC_OFFSET);
	fi->fi_vdac = (caddr_t)(base + SFB_OFFSET_BT459);
	fi->fi_size = (fi->fi_pixels + SFB_FB_SIZE) - fi->fi_base;
	fi->fi_linebytes = 1280;
	fi->fi_driver = &sfb_driver;
	fi->fi_blanked = 0;
	fi->fi_cmap_bits = (caddr_t)&cmap_bits [CMAP_BITS * unit];

	/* Fill in Frame Buffer Type struct. */
	fi->fi_type.fb_boardtype = PMAX_FBTYPE_SFB;
	fi->fi_type.fb_height = 1024;
	fi->fi_type.fb_width = 1280;
	fi->fi_type.fb_depth = 8;
	fi->fi_type.fb_cmsize = 256;
	fi->fi_type.fb_size = SFB_FB_SIZE;

	/* Initialize the RAMDAC. */
	bt459init (fi);

	/* Initialize the SFB ASIC... */
	*((int *)(base + SFB_MODE)) = 0;
	*((int *)(base + SFB_PLANEMASK)) = 0xFFFFFFFF;

/* XXX below, up to fbconnect(), cut-and-pasted from cfb */
	/*
	 * qvss/pm-style mmap()ed event queue compatibility glue
	 */

	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MACH_PHYS_TO_UNCACHED(MACH_CACHED_TO_PHYS(&sfbu));

	/* This is glass-tty state but it's in the shared structure. Ick. */
	fi->fi_fbu->scrInfo.max_row = 67;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/*
	 * Initialize old-style pmax glass-tty screen info.
	 */
	fi->fi_glasstty = &sfbfb;


	/*
	 * Initialize the color map, the screen, and the mouse.
	 */
	if (tb_kbdmouseconfig(fi)) {
		printf(" (mouse/keyboard config failed)");
		return (0);
	}


	/*sfbInitColorMap();*/  /* done by bt459init() */

	/*
	 * Connect to the raster-console pseudo-driver
	 */

	fbconnect ("PMAGB-BA", fi, silent);
	return (1);
}

/* old bt459 code used to be here */

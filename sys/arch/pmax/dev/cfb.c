/*	$NetBSD: cfb.c,v 1.11 1995/09/12 22:36:09 jonathan Exp $	*/

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
 *      $Id: cfb.c,v 1.1.1.1 1995/10/18 08:51:26 deraadt Exp $
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
#include <cfb.h>
#if NCFB > 0
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <machine/machConst.h>
#include <machine/pmioctl.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>

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

extern struct cfdriver cfb;

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [NCFB * CMAP_BITS];	/* One colormap per cfb... */

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

extern void fbScreenInit __P((struct fbinfo *fi));

void genConfigMouse(), genDeconfigMouse();
void genKbdEvent(), genMouseEvent(), genMouseButtons();

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

struct cfdriver cfbcd = {
	NULL, "cfb", cfbmatch, cfbattach, DV_DULL, sizeof(struct fbinfo), 0
};

#if 0
/*
 * Look for a cfb. Separated out from cfbmatch() so consinit() can call it.
 */
int
cfbprobe(cfbaddr)
	caddr_t cfbaddr;
{
	/* check for no frame buffer */
	if (badaddr(cfbaddr, 4))
		return (0);

	/* make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "PMAG-BA "))
		return (0);

	return 1;
}
#endif


int
cfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	static int ncfbs = 1;
	caddr_t cfbaddr = BUS_CVTADDR(ca);

#ifdef FBDRIVER_DOES_ATTACH
	/* leave configuration  to the fb driver */
	return 0;
#endif

	/* make sure that we're looking for this type of device. */
	/*if (!cfbprobe(cfbaddr)) return 0;*/
	if (!BUS_MATCHNAME(ca, "PMAG-BA "))
		return (0);


#ifdef notyet
	/* if it can't have the one mentioned, reject it */
	if (cf->cf_unit >= ncfbs)
		return (0);
#endif
	return (1);
}

/*
 * Attach a device.  Hand off all the work to cfbinit(),
 * so console-config cod can attach cfbs early in boot.
 */
void
cfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	caddr_t base = 	BUS_CVTADDR(ca);
	int unit = self->dv_unit;
	struct fbinfo *fi = (struct fbinfo *) self;

#ifdef notyet
	/* if this is the console, it's already configured. */
	if (ca->ca_slotpri == cons_slot)
		return;	/* XXX patch up f softc pointer */
#endif

	if (!cfbinit(fi, base, unit, 0))
		return;

	/*
	 * The only interrupt on the CFB proper is the vertical-blank
	 * interrupt, which cannot be disabled. The CFB always requests it.
	 * We never enable interrupts from CFB cards, except on the
	 * 3MIN, where TC options interrupt at spl0 through spl2, and
	 * disabling those interrupts isn't currently honoured.
	 */
	if (pmax_boardtype == DS_3MIN) {
		BUS_INTR_ESTABLISH(ca, cfb_intr, self);
	}
}




/*
 * Initialization
 */
int
cfbinit(fi, cfbaddr, unit, silent)
	struct fbinfo *fi;
	caddr_t cfbaddr;
	int unit;
	int silent;
{
	if (fi == NULL) fi = &cfbfi;	/* XXX */

	/* check for no frame buffer */
	if (badaddr(cfbaddr, 4)) {
		printf("cfb: bad address 0x%x\n", cfbaddr);
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
	fi->fi_cmap_bits = (caddr_t)&cmap_bits [CMAP_BITS * unit];

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


	/*cfbInitColorMap();*/  /* done by bt459init() */

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
 * but there's a kernel design bug on the 3MIN, where the cfb
 * interrupts at spl0, spl1, or spl2.
 */
int
cfb_intr(sc)
	void *sc;
{
	struct fbinfo *fi = /* XXX (struct fbinfo *)sc */ &cfbfi;
	
	char *slot_addr = (((char *)fi->fi_base) - CFB_OFFSET_VRAM);
	
	/* reset vertical-retrace interrupt by writing a dont-care */
	*(int*) (slot_addr+CFB_OFFSET_IREQ) = 0;
}

#endif /* NCFB */

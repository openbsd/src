/*	$NetBSD: fb.c,v 1.19 1997/05/24 08:19:50 jonathan Exp $	*/

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
 *	@(#)fb.c	8.1 (Berkeley) 6/10/93
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
 * This file has all the routines common to the various frame buffer drivers
 * including a generic ioctl routine. The pmax_fb structure is passed into the
 * routines and has device specifics stored in it.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/poll.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/mman.h>
#include <sys/syslog.h>

#include <vm/vm.h>
#include <miscfs/specfs/specdev.h>

#include <machine/autoconf.h>
#include <sys/conf.h>
#include <machine/conf.h>

#include <machine/cpuregs.h>		/* mips cached->uncached */
#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>
#include <pmax/dev/fbreg.h>
#include <pmax/dev/qvssvar.h>


#include <pmax/stand/dec_prom.h>

#include <pmax/pmax/cons.h>
#include <pmax/pmax/pmaxtype.h>

#include "rasterconsole.h"

#include "dc_ioasic.h"
#include "dc_ds.h"
#include "scc.h"
#include "dtop.h"

/*
 * This framebuffer driver is a generic driver for all supported
 * framebuffers on NetBSD/pmax.  The match and attach functions call
 * out to probe/init functions in subdrivers for each specific baseboard or
 * expansion  bus framebuffers. The driver softc are maintained here, as
 * are the handlers for user-level requests (open/ioctl/close).
 *
 * Hardware dependencies are handled by calls through the "fbdriver"
 * method table, which the subdriver 
 */

/* qvss/pm compatible and old 4.4bsd/pmax driver functions */

extern int pmax_boardtype;

extern void fbScreenInit __P (( struct fbinfo *fi));


#if (NDC_DS > 0) || (NDC_IOASIC > 0)
#include <machine/dc7085cons.h>
#include <pmax/dev/dcvar.h>
#endif

#if NDTOP > 0
#include <pmax/dev/dtopvar.h>
#endif

#if NSCC > 0
#include <pmax/tc/sccvar.h>
#endif

/*
 * LK-201 and successor keycode mapping 
*/
#include <pmax/dev/lk201var.h>

extern void rcons_connect __P((struct fbinfo *info));	/* XXX */

/*
 * The "blessed" framebuffer; the fb that gets
 * the qvss-style ring buffer of mouse/kbd events, and is used
 * for glass-tty fb console output.
 */
struct fbinfo *firstfi = NULL;

/*
 * The default cursor.
 */
u_short defCursor[32] = { 
/* plane A */ 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
	      0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
/* plane B */ 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
              0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF

};

 
/*
 * Pro-tem framebuffer pseudo-device driver
 */

#include <sys/device.h>
#include "fb.h"


static struct {
	struct fbinfo *cd_devs[NFB];
	int cd_ndevs;
} fbcd =   { {NULL}, 0} ;

void fbattach __P((int n));



/*
 * attach routine: required for pseudo-device
 */
void
fbattach(n)
	int n;
{
	/* allocate space  for n framebuffers... */
}

/*
 * Connect a framebuffer, described by a struct fbinfo, to the 
 * raster-console pseudo-device subsystem. )This would be done
 * with BStreams, if only we had them.)
 */
void
fbconnect (name, info, silent)
	char *name;
	struct fbinfo *info;
	int silent;
{
	int fbix;
	static int first = 1;

#ifndef FBDRIVER_DOES_ATTACH
	/*
	 * See if we've already configured this frame buffer;
	 * if not, find an "fb" pseudo-device entry for it.
	 */
	for (fbix = 0; fbix < fbcd.cd_ndevs; fbix++)
		if ((fbcd.cd_devs [fbix]->fi_type.fb_boardtype
		     == info -> fi_type.fb_boardtype)
		    && fbcd.cd_devs [fbix]->fi_unit == info -> fi_unit)
			goto got_it;
			
	if (fbcd.cd_ndevs >= NFB) {
		printf ("fb: more frame buffers probed than configured!\n");
		return;
	}

	fbix = fbcd.cd_ndevs++;
	fbcd.cd_devs [fbix] = info;
#endif /* FBDRIVER_DOES_ATTACH */

	/*
	 * If this is the first frame buffer we've seen, pass it to rcons.
	 */
	if (first) {
		extern dev_t cn_in_dev;	/* XXX rcons hackery */

		/* Only the first fb gets 4.4bsd/pmax style event ringbuffer */
		firstfi = info;
#if NRASTERCONSOLE > 0
		/*XXX*/ cn_in_dev = cn_tab->cn_dev; /*XXX*/ /* FIXME */
		rcons_connect (info);
#endif  /* NRASTERCONSOLE */
		first = 0;
	}

got_it:
	if (!silent)
		printf (" (%dx%dx%d)%s",
			info -> fi_type.fb_width,
			info -> fi_type.fb_height,
			info -> fi_type.fb_depth,
			(fbix ? ""
			 : ((cn_tab -> cn_pri == CN_REMOTE)
			    ? "" : " (console)")));
	return;
}


#include "fb_usrreq.c"	/* old pm-compatblie driver that supports X11R5 */


/*
 * Configure the keyboard/mouse based on machine type for turbochannel
 * display boards.
 */
int
tb_kbdmouseconfig(fi)
	struct fbinfo *fi;
{

	if (fi == NULL || fi->fi_glasstty == NULL) {
#if defined(DEBUG) || defined(DIAGNOSTIC)
		printf("tb_kbdmouseconfig: given non-console framebuffer\n");
#endif
		return 1;
	}

	switch (pmax_boardtype) {

#if (NDC_DS > 0) || (NDC_IOASIC > 0)
	case DS_PMAX:
	case DS_3MAX:
		fi->fi_glasstty->KBDPutc = dcPutc;
		fi->fi_glasstty->kbddev = makedev(DCDEV, DCKBD_PORT);
		break;
#endif	/* NDC_DS || NDC_IOASIC */

#if NSCC > 0
	case DS_3MIN:
	case DS_3MAXPLUS:
		fi->fi_glasstty->KBDPutc = sccPutc;
		fi->fi_glasstty->kbddev = makedev(SCCDEV, SCCKBD_PORT);
		break;
#endif /* NSCC */

#if NDTOP > 0
	case DS_MAXINE:
		fi->fi_glasstty->KBDPutc = dtopKBDPutc;
		fi->fi_glasstty->kbddev = makedev(DTOPDEV, DTOPKBD_PORT);
		break;
#endif	/* NDTOP */

	default:
		printf("Can't configure keyboard/mouse\n");
		return (1);
	};

	return (0);
}

/*
 * pre-rcons glass-tty emulator (stub)
 */
void
fbScreenInit(fi)
	struct fbinfo *fi;
{
	/* how to do this on rcons ? */
}

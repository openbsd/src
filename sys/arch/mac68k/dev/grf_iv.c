/*	$NetBSD: grf_iv.c,v 1.10 1995/08/11 17:48:19 briggs Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
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
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * Graphics display driver for the Macintosh internal video for machines
 * that don't map it into a fake nubus card.
 */

#include <sys/param.h>

#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>

#include <machine/grfioctl.h>
#include <machine/cpu.h>

#include "nubus.h"
#include "grfvar.h"

extern int	grfiv_probe __P((struct grf_softc *sc, nubus_slot *ignore));
extern int	grfiv_init __P((struct grf_softc *sc));
extern int	grfiv_mode __P((struct grf_softc *sc, int cmd, void *arg));

extern u_int32_t	mac68k_vidlog;
extern u_int32_t	mac68k_vidphys;
extern long		videorowbytes;
extern long		videobitdepth;
extern unsigned long	videosize;

extern int
grfiv_probe(sc, slotinfo)
	struct grf_softc *sc;
	nubus_slot *slotinfo;
{
	static int	internal_video_found = 0;

	if (internal_video_found || (mac68k_vidlog == 0)) {
		return 0;
	}

	if (   (NUBUS_SLOT_TO_BASE(slotinfo->slot) <= mac68k_vidlog)
	    && (mac68k_vidlog < NUBUS_SLOT_TO_BASE(slotinfo->slot + 1))) {
		internal_video_found++;
		return 1;
	}

	if (slotinfo->slot == NUBUS_INT_VIDEO_PSUEDO_SLOT) {
		internal_video_found++;
		return 1;
	}

	return 0;
}

extern int
grfiv_init(sc)
	struct	grf_softc *sc;
{
	struct grfmode *gm;
	int i, j;

	sc->card_id = 0;

	strcpy(sc->card_name, "Internal video");

	gm = &(sc->curr_mode);
	gm->mode_id = 0;
	gm->psize = videobitdepth;
	gm->ptype = 0;
	gm->width = videosize & 0xffff;
	gm->height = (videosize >> 16) & 0xffff;
	gm->rowbytes = videorowbytes;
	gm->hres = 80;		/* XXX Hack */
	gm->vres = 80;		/* XXX Hack */
	gm->fbsize = gm->rowbytes * gm->height;
	gm->fbbase = (caddr_t) mac68k_vidlog;
	gm->fboff = 0;

	return 1;
}

extern int
grfiv_mode(sc, cmd, arg)
	struct grf_softc *sc;
	int cmd;
	void *arg;
{
	switch (cmd) {
	case GM_GRFON:
	case GM_GRFOFF:
		return 0;
	case GM_CURRMODE:
		break;
	case GM_NEWMODE:
		break;
	case GM_LISTMODES:
		break;
	}
	return EINVAL;
}

extern caddr_t
grfiv_phys(gp, addr)
	struct grf_softc *gp;
	vm_offset_t addr;
{
	/*
	 * If we're using IIsi or similar, this will be 0.
	 * If we're using IIvx or similar, this will be correct.
	 */
	return (caddr_t) mac68k_vidphys;
}

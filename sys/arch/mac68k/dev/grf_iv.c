/*	$NetBSD: grf_iv.c,v 1.12 1996/05/19 22:27:06 scottr Exp $	*/

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
#include <sys/systm.h>

#include <machine/grfioctl.h>
#include <machine/cpu.h>

#include "nubus.h"
#include "grfvar.h"

extern u_int32_t	mac68k_vidlog;
extern u_int32_t	mac68k_vidphys;
extern long		videorowbytes;
extern long		videobitdepth;
extern unsigned long	videosize;

static int	grfiv_mode __P((struct grf_softc *gp, int cmd, void *arg));
static caddr_t	grfiv_phys __P((struct grf_softc *gp, vm_offset_t addr));
static int	grfiv_match __P((struct device *, void *, void *));
static void	grfiv_attach __P((struct device *, struct device *, void *));

struct cfdriver intvid_cd = {
	NULL, "intvid", DV_DULL
};

struct cfattach intvid_ca = {
	sizeof(struct grfbus_softc), grfiv_match, grfiv_attach
};

static int
grfiv_match(pdp, match, auxp)
	struct device	*pdp;
	void	*match, *auxp;
{
	static int	internal_video_found = 0;

	if (internal_video_found || (mac68k_vidlog == 0)) {
		return 0;
	}

	return 1;
}

static void
grfiv_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct grfbus_softc	*sc;
	struct grfmode		*gm;

	sc = (struct grfbus_softc *) self;

	sc->card_id = 0;

	printf(": Internal Video\n");

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

	/* Perform common video attachment. */
	grf_establish(sc, grfiv_mode, grfiv_phys);
}

static int
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

static caddr_t
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

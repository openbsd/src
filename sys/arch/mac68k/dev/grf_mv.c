/*	$OpenBSD: grf_mv.c,v 1.8 1997/03/12 13:36:57 briggs Exp $	*/
/*	$NetBSD: grf_mv.c,v 1.17 1997/02/24 06:20:06 scottr Exp $	*/

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
 * Device-specific routines for handling Nubus-based video cards.
 */

#include <sys/param.h>

#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/grfioctl.h>
#include <machine/viareg.h>

#include "nubus.h"
#include "grfvar.h"

static void	load_image_data __P((caddr_t data, struct image_data *image));
static void	grfmv_intr __P((void *vsc, int slot));

#ifndef MYSTERY
static char zero = 0;
#endif

static int	grfmv_mode __P((struct grf_softc *gp, int cmd, void *arg));
static caddr_t	grfmv_phys __P((struct grf_softc *gp, vm_offset_t addr));
static int	grfmv_match __P((struct device *, void *, void *));
static void	grfmv_attach __P((struct device *, struct device *, void *));

struct cfdriver macvid_cd = {
	NULL, "macvid", DV_DULL
};

struct cfattach macvid_ca = {
	sizeof(struct grfbus_softc), grfmv_match, grfmv_attach
};

static void
load_image_data(data, image)
	caddr_t	data;
	struct	image_data *image;
{
	bcopy(data     , &image->size,       4);
	bcopy(data +  4, &image->offset,     4);
	bcopy(data +  8, &image->rowbytes,   2);
	bcopy(data + 10, &image->top,        2);
	bcopy(data + 12, &image->left,       2);
	bcopy(data + 14, &image->bottom,     2);
	bcopy(data + 16, &image->right,      2);
	bcopy(data + 18, &image->version,    2);
	bcopy(data + 20, &image->packType,   2);
	bcopy(data + 22, &image->packSize,   4);
	bcopy(data + 26, &image->hRes,       4);
	bcopy(data + 30, &image->vRes,       4);
	bcopy(data + 34, &image->pixelType,  2);
	bcopy(data + 36, &image->pixelSize,  2);
	bcopy(data + 38, &image->cmpCount,   2);
	bcopy(data + 40, &image->cmpSize,    2);
	bcopy(data + 42, &image->planeBytes, 4);
}

/*ARGSUSED*/
static void
grfmv_intr(vsc, slot)
	void	*vsc;
	int	slot;
{
#ifdef MYSTERY
	struct grfbus_softc *sc;
	caddr_t slotbase;

	sc = (struct grfbus_softc *) vsc;
	slotbase = (caddr_t) sc->sc_slot.virtual_base;
	asm volatile("	movl	%0,a0
			movl	a0@(0xff6028),d0
			andl	#0x2,d0
			beq	_mv_intr0
			movql	#0x3,d0
		_mv_intr0:
			movl	a0@(0xff600c),d1
			andl	#0x3,d1
			cmpl	d1,d0
			beq	_mv_intr_fin
			movl	d0,a0@(0xff600c)
			nop
			tstb	d0
			beq	_mv_intr1
			movl	#0x0002,a0@(0xff6040)
			movl	#0x0102,a0@(0xff6044)
			movl	#0x0105,a0@(0xff6048)
			movl	#0x000e,a0@(0xff604c)
			movl	#0x001c,a0@(0xff6050)
			movl	#0x00bc,a0@(0xff6054)
			movl	#0x00c3,a0@(0xff6058)
			movl	#0x0061,a0@(0xff605c)
			movl	#0x0012,a0@(0xff6060)
			bra	_mv_intr_fin
		_mv_intr1:
			movl	#0x0002,a0@(0xff6040)
			movl	#0x0209,a0@(0xff6044)
			movl	#0x020c,a0@(0xff6048)
			movl	#0x000f,a0@(0xff604c)
			movl	#0x0027,a0@(0xff6050)
			movl	#0x00c7,a0@(0xff6054)
			movl	#0x00d7,a0@(0xff6058)
			movl	#0x006b,a0@(0xff605c)
			movl	#0x0029,a0@(0xff6060)
		_mv_intr_fin:
			movl	#0x1,a0@(0xff6014)"
		: : "g" (slotbase) : "a0","d0","d1");
#else
	caddr_t			 slotbase;
	struct grfbus_softc	*sc;

	sc = (struct grfbus_softc *) vsc;
	slotbase = (caddr_t) sc->sc_slot.virtual_base;
	switch (sc->card_id) {
	case NUBUS_DRHW_WVC:
		slotbase[0xa00000] = zero;
		break;
	default:
		slotbase[0xa0000] = zero;
		break;
	}
#endif
}

static int
grfmv_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;

	if (na->category != NUBUS_CATEGORY_DISPLAY)
		return 0;

	if (na->type != NUBUS_TYPE_VIDEO)
		return 0;

	if (na->drsw != NUBUS_DRSW_APPLE)
		return 0;

	/*
	 * If we've gotten this far, then we're dealing with a real-live
	 * Apple QuickDraw-compatible display card resource.  Now, how to
	 * determine that this is an active resource???  Dunno.  But we'll
	 * proceed like it is.
	 */

	return 1;
}

static void
grfmv_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfbus_softc *sc = (struct grfbus_softc *) self;
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
	struct image_data image_store, image;
	struct grfmode *gm;
	char cardname[CARD_NAME_LEN];
	nubus_dirent dirent;
	nubus_dir dir, mode_dir;
	int mode;

	sc->card_id = na->drhw;

	bcopy(na->fmt, &sc->sc_slot, sizeof(nubus_slot));

	nubus_get_main_dir(&sc->sc_slot, &dir);

	if (nubus_find_rsrc(&sc->sc_slot, &dir, na->rsrcid, &dirent) <= 0)
		return;

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &sc->board_dir);

	if (nubus_find_rsrc(&sc->sc_slot, &sc->board_dir,
	    NUBUS_RSRC_TYPE, &dirent) <= 0)
		if ((na->rsrcid != 128) ||
		    (nubus_find_rsrc(&sc->sc_slot, &dir, 129, &dirent) <= 0))
			return;

	mode = NUBUS_RSRC_FIRSTMODE;
	if (nubus_find_rsrc(&sc->sc_slot, &sc->board_dir, mode, &dirent) <= 0) {
		printf("\n%s: probe failed to get board rsrc.\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &mode_dir);

	if (nubus_find_rsrc(&sc->sc_slot, &mode_dir, VID_PARAMS, &dirent)
	    <= 0) {
		printf("\n%s: probe failed to get mode dir.\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (nubus_get_ind_data(&sc->sc_slot, &dirent, (caddr_t) &image_store,
				sizeof(struct image_data)) <= 0) {
		printf("\n%s: probe failed to get indirect mode data.\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Need to load display info (and driver?), etc... (?) */

	load_image_data((caddr_t) &image_store, &image);

	gm = &sc->curr_mode;
	gm->mode_id = mode;
	gm->fbbase = (caddr_t) (sc->sc_slot.virtual_base + image.offset);
	gm->fboff = image.offset;
	gm->rowbytes = image.rowbytes;
	gm->width = image.right - image.left;
	gm->height = image.bottom - image.top;
	gm->fbsize = sc->curr_mode.height * sc->curr_mode.rowbytes;
	gm->hres = image.hRes;
	gm->vres = image.vRes;
	gm->ptype = image.pixelType;
	gm->psize = image.pixelSize;

	strncpy(cardname, nubus_get_card_name(&sc->sc_slot),
		CARD_NAME_LEN);
	cardname[CARD_NAME_LEN-1] = '\0';

	printf(": %s\n", cardname);

	add_nubus_intr(sc->sc_slot.slot, grfmv_intr, sc);

	/* Perform common video attachment. */
	grf_establish(sc, &sc->sc_slot, grfmv_mode, grfmv_phys);
}

static int
grfmv_mode(gp, cmd, arg)
	struct grf_softc *gp;
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
grfmv_phys(gp, addr)
	struct grf_softc *gp;
	vm_offset_t addr;
{
	return (caddr_t) (NUBUS_SLOT2PA(gp->sc_slot->slot) +
				(addr - gp->sc_slot->virtual_base));
}

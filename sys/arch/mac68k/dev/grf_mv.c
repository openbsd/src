/*	$OpenBSD: grf_mv.c,v 1.30 2006/01/10 21:19:14 miod Exp $	*/
/*	$NetBSD: grf_nubus.c,v 1.62 2001/01/22 20:27:02 briggs Exp $	*/

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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

#include <mac68k/dev/nubus.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <mac68k/dev/macfbvar.h>

int	macfb_nubus_match(struct device *, void *, void *);
void	macfb_nubus_attach(struct device *, struct device *, void *);

struct cfattach macfb_nubus_ca = {
	sizeof(struct macfb_softc), macfb_nubus_match, macfb_nubus_attach
};

void	load_image_data(caddr_t data, struct image_data *image);

int	grfmv_intr_generic_write1(void *vsc);
int	grfmv_intr_generic_write4(void *vsc);
int	grfmv_intr_generic_or4(void *vsc);

int	grfmv_intr_cb264(void *vsc);
int	grfmv_intr_cb364(void *vsc);
int	grfmv_intr_cmax(void *vsc);
int	grfmv_intr_cti(void *vsc);
int	grfmv_intr_radius(void *vsc);
int	grfmv_intr_radius24(void *vsc);
int	grfmv_intr_supermacgfx(void *vsc);
int	grfmv_intr_lapis(void *vsc);
int	grfmv_intr_formac(void *vsc);
int	grfmv_intr_vimage(void *vsc);
int	grfmv_intr_gvimage(void *vsc);
int	grfmv_intr_radius_gsc(void *vsc);
int	grfmv_intr_radius_gx(void *vsc);

#define CARD_NAME_LEN	64

void
load_image_data(caddr_t data, struct image_data *image)
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

int
macfb_nubus_match(struct device *parent, void *vcf, void *aux)
{
	struct nubus_attach_args *na = (struct nubus_attach_args *)aux;

	if (na->category != NUBUS_CATEGORY_DISPLAY)
		return (0);

	if (na->type != NUBUS_TYPE_VIDEO)
		return (0);

	if (na->drsw != NUBUS_DRSW_APPLE)
		return (0);

	/*
	 * If we've gotten this far, then we're dealing with a real-live
	 * Apple QuickDraw-compatible display card resource.  Now, how to
	 * determine that this is an active resource???  Dunno.  But we'll
	 * proceed like it is.
	 */
	return (1);
}

void
macfb_nubus_attach(struct device *parent, struct device *self, void *aux)
{
	struct nubus_attach_args *na = (struct nubus_attach_args *)aux;
	struct macfb_softc *sc = (struct macfb_softc *)self;
	struct image_data image_store, image;
	char cardname[CARD_NAME_LEN];
	nubus_dirent dirent;
	nubus_dir dir, mode_dir, board_dir;
	int mode;
	struct macfb_devconfig *dc;

	bcopy(na->fmt, &sc->sc_slot, sizeof(nubus_slot));

	sc->sc_tag = na->na_tag;
	sc->card_id = na->drhw;
	sc->sc_basepa = (bus_addr_t)NUBUS_SLOT2PA(na->slot);
	sc->sc_fbofs = 0;

	if (bus_space_map(sc->sc_tag, sc->sc_basepa, NBMEMSIZE,
	    0, &sc->sc_regh)) {
		printf(": failed to map slot %d\n", na->slot);
		return;
	}

	nubus_get_main_dir(&sc->sc_slot, &dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_regh,
	    &sc->sc_slot, &dir, na->rsrcid, &dirent) <= 0) {
		printf(": failed to get board rsrc.\n");
		goto bad;
	}

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &board_dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_regh,
	    &sc->sc_slot, &board_dir, NUBUS_RSRC_TYPE, &dirent) <= 0)
		if ((na->rsrcid != 128) ||
		    (nubus_find_rsrc(sc->sc_tag, sc->sc_regh,
		    &sc->sc_slot, &dir, 129, &dirent) <= 0)) {
			printf(": failed to get board rsrc.\n");
			goto bad;
		}

	mode = NUBUS_RSRC_FIRSTMODE;
	if (nubus_find_rsrc(sc->sc_tag, sc->sc_regh,
	    &sc->sc_slot, &board_dir, mode, &dirent) <= 0) {
		printf(": probe failed to get board rsrc.\n");
		goto bad;
	}

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &mode_dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_regh,
	    &sc->sc_slot, &mode_dir, VID_PARAMS, &dirent) <= 0) {
		printf(": probe failed to get mode dir.\n");
		goto bad;
	}

	if (nubus_get_ind_data(sc->sc_tag, sc->sc_regh, &sc->sc_slot,
	    &dirent, (caddr_t)&image_store, sizeof(struct image_data)) <= 0) {
		printf(": probe failed to get indirect mode data.\n");
		goto bad;
	}

	/* Need to load display info (and driver?), etc... (?) */

	load_image_data((caddr_t)&image_store, &image);

	dc = malloc(sizeof(*dc), M_DEVBUF, M_WAITOK);
	bzero(dc, sizeof(*dc));
	
	dc->dc_vaddr = (vaddr_t)sc->sc_handle.base; /* XXX evil hack */
	dc->dc_paddr = sc->sc_basepa;
	dc->dc_offset = image.offset;
	dc->dc_wid = image.right - image.left;
	dc->dc_ht = image.bottom - image.top;
	dc->dc_depth = image.pixelSize;
	dc->dc_rowbytes = image.rowbytes;
	dc->dc_size = dc->dc_ht * dc->dc_rowbytes;

	/* Perform common video attachment. */

	strlcpy(cardname, nubus_get_card_name(sc->sc_tag, sc->sc_regh,
	    &sc->sc_slot), sizeof cardname);
	printf(": %s\n", cardname);

	if (sc->card_id == NUBUS_DRHW_TFB) {
		/*
		 * This is the Toby card, but apparently some manufacturers
		 * (like Cornerstone) didn't bother to get/use their own
		 * value here, even though the cards are different, so we
		 * so we try to differentiate here.
		 */
		if (strncmp(cardname, "Samsung 768", 11) == 0)
			sc->card_id = NUBUS_DRHW_SAM768;
#ifdef DEBUG
		else if (strncmp(cardname, "Toby frame", 10) != 0)
			printf("%s: This display card pretends to be a TFB!\n",
			    sc->sc_dev.dv_xname);
#endif
	}

	switch (sc->card_id) {
	case NUBUS_DRHW_TFB:
	case NUBUS_DRHW_M2HRVC:
	case NUBUS_DRHW_PVC:
		sc->cli_offset = 0xa0000;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_WVC:
		sc->cli_offset = 0xa00000;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_COLORMAX:
		add_nubus_intr(na->slot, grfmv_intr_cmax, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_SE30:
		/* Do nothing--SE/30 interrupts are disabled */
		break;
	case NUBUS_DRHW_MDC:
		sc->cli_offset = 0x200148;
		sc->cli_value = 1;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc,
		    sc->sc_dev.dv_xname);

		/* Enable interrupts; to disable, write 0x7 to this location */
		bus_space_write_4(sc->sc_tag, sc->sc_regh, 0x20013C, 5);
		break;
	case NUBUS_DRHW_CB264:
		add_nubus_intr(na->slot, grfmv_intr_cb264, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_CB364:
		add_nubus_intr(na->slot, grfmv_intr_cb364, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RPC8:
		sc->cli_offset = 0xfdff8f;
		sc->cli_value = 0xff;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RPC8XJ:
		sc->cli_value = 0x66;
		add_nubus_intr(na->slot, grfmv_intr_radius, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RPC24X:
	case NUBUS_DRHW_BOOGIE:
		sc->cli_value = 0x64;
		add_nubus_intr(na->slot, grfmv_intr_radius, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RPC24XP:
		add_nubus_intr(na->slot, grfmv_intr_radius24, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RADGSC:
		add_nubus_intr(na->slot, grfmv_intr_radius_gsc, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_RDCGX:
	case NUBUS_DRHW_MPGX:
		add_nubus_intr(na->slot, grfmv_intr_radius_gx, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_FIILX:
	case NUBUS_DRHW_FIISXDSP:
	case NUBUS_DRHW_FUTURASX:
		sc->cli_offset = 0xf05000;
		sc->cli_value = 0x80;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_SAM768:
		add_nubus_intr(na->slot, grfmv_intr_cti, sc,
		sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_SUPRGFX:
		add_nubus_intr(na->slot, grfmv_intr_supermacgfx, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_SPECTRM8:
		sc->cli_offset = 0x0de178;
		sc->cli_value = 0x80;
		add_nubus_intr(na->slot, grfmv_intr_generic_or4, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_LAPIS:
		add_nubus_intr(na->slot, grfmv_intr_lapis, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_FORMAC:
		add_nubus_intr(na->slot, grfmv_intr_formac, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_ROPS24LXI:
	case NUBUS_DRHW_ROPS24XLTV:
	case NUBUS_DRHW_ROPS24MXTV:
	case NUBUS_DRHW_THUNDER24:
		sc->cli_offset = 0xfb0010;
		sc->cli_value = 0x00;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_ROPSPPGT:
		sc->cli_offset = 0xf50010;
		sc->cli_value = 0x02;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_VIMAGE:
		add_nubus_intr(na->slot, grfmv_intr_vimage, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_GVIMAGE:
		add_nubus_intr(na->slot, grfmv_intr_gvimage, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_MC2124NB:
		sc->cli_offset = 0xfd1000;
		sc->cli_value = 0x00;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc,
		    sc->sc_dev.dv_xname);
		break;
	case NUBUS_DRHW_MICRON:
		sc->cli_offset = 0xa00014;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc,
		    sc->sc_dev.dv_xname);
		break;
	default:
		printf("%s: Unknown video card ID 0x%x --",
		    sc->sc_dev.dv_xname, sc->card_id);
		printf(" Not installing interrupt routine.\n");
		break;
	}

	/* Perform common video attachment. */
	macfb_attach_common(sc, dc);
	return;

bad:
	bus_space_unmap(sc->sc_tag, sc->sc_regh, NBMEMSIZE);
}

/* Interrupt handlers... */
/*
 * Generic routine to clear interrupts for cards where it simply takes
 * a MOV.B to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
int
grfmv_intr_generic_write1(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_regh,
	    sc->cli_offset, (u_int8_t)sc->cli_value);
	return (1);
}

/*
 * Generic routine to clear interrupts for cards where it simply takes
 * a MOV.L to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
int
grfmv_intr_generic_write4(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;

	bus_space_write_4(sc->sc_tag, sc->sc_regh,
	    sc->cli_offset, sc->cli_value);
	return (1);
}

/*
 * Generic routine to clear interrupts for cards where it simply takes
 * an OR.L to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
int
grfmv_intr_generic_or4(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	unsigned long	scratch;

	scratch = bus_space_read_4(sc->sc_tag, sc->sc_regh, sc->cli_offset);
	scratch |= 0x80;
	bus_space_write_4(sc->sc_tag, sc->sc_regh, sc->cli_offset, scratch);
	return (1);
}

/*
 * Routine to clear interrupts for the Radius PrecisionColor 8xj card.
 */
/*ARGSUSED*/
int
grfmv_intr_radius(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t c;

	c = sc->cli_value;

	c |= 0x80;
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xd00403, c);
	c &= 0x7f;
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xd00403, c);
	return (1);
}

/*
 * Routine to clear interrupts for the Radius PrecisionColor 24Xp card.
 * Is this what the 8xj routine is doing, too?
 */
/*ARGSUSED*/
int
grfmv_intr_radius24(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t c;

	c = 0x80 | bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xfffd8);
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xd00403, c);
	c &= 0x7f;
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xd00403, c);
	return (1);
}

/*
 * Routine to clear interrupts on Samsung 768x1006 video controller.
 * This controller was manufactured by Cornerstone Technology, Inc.,
 * now known as Cornerstone Imaging.
 *
 * To clear this interrupt, we apparently have to set, then clear,
 * bit 2 at byte offset 0x80000 from the card's base.
 *	Information for this provided by Brad Salai <bsalai@servtech.com>
 */
/*ARGSUSED*/
int
grfmv_intr_cti(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t c;

	c = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0x80000);
	c |= 0x02;
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x80000, c);
	c &= 0xfd;
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x80000, c);
	return (1);
}

/*ARGSUSED*/
int
grfmv_intr_cb264(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	volatile char *slotbase;

	slotbase = (volatile char *)(sc->sc_handle.base); /* XXX evil hack */
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
	return (1);
}

/*
 * Support for the Colorboard 364 might be more complex than it needs to
 * be.  If we can find more information about this card, this might be
 * significantly simplified.  Contributions welcome...  :-)
 */
/*ARGSUSED*/
int
grfmv_intr_cb364(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	volatile char *slotbase;

	slotbase = (volatile char *)(sc->sc_handle.base); /* XXX evil hack */
	asm volatile("	movl	%0,a0
			movl	a0@(0xfe6028),d0
			andl	#0x2,d0
			beq	_cb364_intr4
			movql	#0x3,d0
			movl	a0@(0xfe6018),d1
			movl	#0x3,a0@(0xfe6018)
			movw	a0@(0xfe7010),d2
			movl	d1,a0@(0xfe6018)
			movl	a0@(0xfe6020),d1
			btst	#0x06,d2
			beq	_cb364_intr0
			btst	#0x00,d1
			beq	_cb364_intr5
			bsr	_cb364_intr1
			bra	_cb364_intr_out
		_cb364_intr0:
			btst	#0x00,d1
			bne	_cb364_intr5
			bsr	_cb364_intr1
			bra	_cb364_intr_out
		_cb364_intr1:
			movl	d0,a0@(0xfe600c)
			nop
			tstb	d0
			beq	_cb364_intr3
			movl	#0x0002,a0@(0xfe6040)
			movl	#0x0105,a0@(0xfe6048)
			movl	#0x000e,a0@(0xfe604c)
			movl	#0x00c3,a0@(0xfe6058)
			movl	#0x0061,a0@(0xfe605c)
			btst	#0x06,d2
			beq	_cb364_intr2
			movl	#0x001c,a0@(0xfe6050)
			movl	#0x00bc,a0@(0xfe6054)
			movl	#0x0012,a0@(0xfe6060)
			movl	#0x000e,a0@(0xfe6044)
			movl	#0x00c3,a0@(0xfe6064)
			movl	#0x0061,a0@(0xfe6020)
			rts
		_cb364_intr2:
			movl	#0x0016,a0@(0xfe6050)
			movl	#0x00b6,a0@(0xfe6054)
			movl	#0x0011,a0@(0xfe6060)
			movl	#0x0101,a0@(0xfe6044)
			movl	#0x00bf,a0@(0xfe6064)
			movl	#0x0001,a0@(0xfe6020)
			rts
		_cb364_intr3:
			movl	#0x0002,a0@(0xfe6040)
			movl	#0x0209,a0@(0xfe6044)
			movl	#0x020c,a0@(0xfe6048)
			movl	#0x000f,a0@(0xfe604c)
			movl	#0x0027,a0@(0xfe6050)
			movl	#0x00c7,a0@(0xfe6054)
			movl	#0x00d7,a0@(0xfe6058)
			movl	#0x006b,a0@(0xfe605c)
			movl	#0x0029,a0@(0xfe6060)
			oril	#0x0040,a0@(0xfe6064)
			movl	#0x0000,a0@(0xfe6020)
			rts
		_cb364_intr4:
			movq	#0x00,d0
		_cb364_intr5:
			movl	a0@(0xfe600c),d1
			andl	#0x3,d1
			cmpl	d1,d0
			beq	_cb364_intr_out
			bsr	_cb364_intr1
		_cb364_intr_out:
			movl	#0x1,a0@(0xfe6014)
		_cb364_intr_quit:
		" : : "g" (slotbase) : "a0","d0","d1","d2");
	return (1);
}

/*
 * Interrupt clearing routine for SuperMac GFX card.
 */
/*ARGSUSED*/
int
grfmv_intr_supermacgfx(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xE70D3);
	return (1);
}

/*
 * Routine to clear interrupts for the Sigma Designs ColorMax card.
 */
/*ARGSUSED*/
int
grfmv_intr_cmax(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int32_t dummy;

	dummy = bus_space_read_4(sc->sc_tag, sc->sc_regh, 0xf501c);
	dummy = bus_space_read_4(sc->sc_tag, sc->sc_regh, 0xf5018);
	return (1);
}

/*
 * Routine to clear interrupts for the Lapis ProColorServer 8 PDS card
 * (for the SE/30).
 */
/*ARGSUSED*/
int
grfmv_intr_lapis(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xff7000, 0x08);
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xff7000, 0x0C);
	return (1);
}

/*
 * Routine to clear interrupts for the Formac Color Card II
 */
/*ARGSUSED*/
int
grfmv_intr_formac(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xde80db);
	dummy = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xde80d3);
	return (1);
}

/*
 * Routine to clear interrupts for the Vimage by Interware Co., Ltd.
 */
/*ARGSUSED*/
int
grfmv_intr_vimage(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x800000, 0x67);
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x800000, 0xE7);
	return (1);
}

/*
 * Routine to clear interrupts for the Grand Vimage by Interware Co., Ltd.
 */
/*ARGSUSED*/
int
grfmv_intr_gvimage(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xf00000);
	return (1);
}

/*
 * Routine to clear interrupts for the Radius GS/C
 */
/*ARGSUSED*/
int
grfmv_intr_radius_gsc(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_regh, 0xfb802);
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0xfb802, 0xff);
	return (1);
}

/*
 * Routine to clear interrupts for the Radius GS/C
 */
/*ARGSUSED*/
int
grfmv_intr_radius_gx(void *vsc)
{
	struct macfb_softc *sc = (struct macfb_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x600000, 0x00);
	bus_space_write_1(sc->sc_tag, sc->sc_regh, 0x600000, 0x20);
	return (1);
}

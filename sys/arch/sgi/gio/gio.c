/*	$OpenBSD: gio.c,v 1.17 2014/03/27 21:24:22 miod Exp $	*/
/*	$NetBSD: gio.c,v 1.32 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2000 Soren S. Jorvang
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <sgi/gio/gioreg.h>
#include <sgi/gio/giovar.h>
#include <sgi/gio/giodevs.h>
#include <sgi/gio/giodevs_data.h>

#include <sgi/gio/grtworeg.h>
#include <sgi/gio/lightreg.h>
#include <sgi/gio/newportreg.h>

#include <sgi/localbus/imcvar.h>
#include <sgi/localbus/intreg.h>
#include <sgi/localbus/intvar.h>
#include <sgi/sgi/ip22.h>

#include "grtwo.h"
#include "impact.h"
#include "light.h"
#include "newport.h"

#if NGRTWO > 0
#include <sgi/gio/grtwovar.h>
#endif
#if NIMPACT_GIO > 0
#include <sgi/dev/impactvar.h>
#endif
#if NLIGHT > 0
#include <sgi/gio/lightvar.h>
#endif
#if NNEWPORT > 0
#include <sgi/gio/newportvar.h>
#endif

int	 gio_match(struct device *, void *, void *);
void	 gio_attach(struct device *, struct device *, void *);
int	 gio_print(void *, const char *);
int	 gio_print_fb(void *, const char *);
int	 gio_search(struct device *, void *, void *);
int	 gio_submatch(struct device *, void *, void *);
uint32_t gio_id(vaddr_t, paddr_t, int);
int	 gio_is_framebuffer_id(uint32_t);

struct gio_softc {
	struct device	sc_dev;

	bus_space_tag_t	sc_iot;
	bus_dma_tag_t	sc_dmat;
};

const struct cfattach gio_ca = {
	sizeof(struct gio_softc), gio_match, gio_attach
};

struct cfdriver gio_cd = {
	NULL, "gio", DV_DULL
};

/* Address of the console frame buffer registers, if applicable */
paddr_t		giofb_consaddr;
/* Id of the console frame buffer */
uint32_t	giofb_consid;
/* Names of the frame buffers, as obtained by ARCBios */
const char	*giofb_names[GIO_MAX_FB];

struct gio_probe {
	uint32_t slot;
	uint64_t base;
	uint32_t mach_type;
	uint32_t mach_subtype;
};

/*
 * Expansion Slot Base Addresses
 *
 * IP20 and IP24 have two GIO connectors: GIO_SLOT_EXP0 and
 * GIO_SLOT_EXP1.
 *
 * On IP24 these slots exist on the graphics board or the IOPLUS
 * "mezzanine" on Indy and Challenge S, respectively. The IOPLUS or
 * graphics board connects to the mainboard via a single GIO64 connector.
 *
 * IP22 has either three or four physical connectors, but only two
 * electrically distinct slots: GIO_SLOT_GFX and GIO_SLOT_EXP0.
 *
 * It should also be noted that DMA is (mostly) not supported in Challenge S's
 * GIO_SLOT_EXP1. See gio(4) for the story.
 */
static const struct gio_probe slot_bases[] = {
	/* GFX is only a slot on Indigo 2 */
	{ GIO_SLOT_GFX, GIO_ADDR_GFX, SGI_IP22, IP22_INDIGO2 },

	/* EXP0 is available on all systems */
	{ GIO_SLOT_EXP0, GIO_ADDR_EXP0, SGI_IP20, -1 },
	{ GIO_SLOT_EXP0, GIO_ADDR_EXP0, SGI_IP22, -1 },

	/* EXP1 does not exist on Indigo 2 */
	{ GIO_SLOT_EXP1, GIO_ADDR_EXP1, SGI_IP20, -1 },
	{ GIO_SLOT_EXP1, GIO_ADDR_EXP1, SGI_IP22, IP22_INDY },
	{ GIO_SLOT_EXP1, GIO_ADDR_EXP1, SGI_IP22, IP22_CHALLS },

	{ 0, 0, 0, 0 }
};

/*
 * Graphic Board Base Addresses
 *
 * Graphics boards are not treated like expansion slot cards. Their base
 * addresses do not necessarily correspond to GIO slot addresses and they
 * do not contain product identification words.
 *
 * This list needs to be sorted in address order, to match the descriptions
 * obtained from ARCBios.
 */
static const struct gio_probe gfx_bases[] = {
	{ -1, GIO_ADDR_GFX, SGI_IP20, -1 },
	{ -1, GIO_ADDR_GFX, SGI_IP22, -1 },

	/* IP20 LG1/LG2 */
	{ -1, GIO_ADDR_GFX + 0x003f0000, SGI_IP20, -1 },
	{ -1, GIO_ADDR_GFX + 0x003f8000, SGI_IP20, -1 }, /* second head */

	{ -1, GIO_ADDR_EXP0, SGI_IP22, -1 },
	{ -1, GIO_ADDR_EXP1, SGI_IP22, -1 },

	{ 0, 0, 0, 0 }
};

int
gio_match(struct device *parent, void *match, void *aux)
{
	struct imc_attach_args *iaa = aux;

	if (strcmp(iaa->iaa_name, gio_cd.cd_name) != 0)
		return 0;

	return 1;
}

void
gio_attach(struct device *parent, struct device *self, void *aux)
{
	struct gio_softc *sc = (struct gio_softc *)self;
	struct imc_attach_args *iaa = (struct imc_attach_args *)aux;
	struct gio_attach_args ga;
	uint32_t gfx[GIO_MAX_FB], id;
	uint i, j, ngfx;
	int sys_type;

	printf("\n");

	sc->sc_iot = iaa->iaa_st;
	sc->sc_dmat = iaa->iaa_dmat;

	switch (sys_config.system_type) {
	case SGI_IP20:
		sys_type = SGI_IP20;
		break;
	default:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		sys_type = SGI_IP22;
		break;
	}

	ngfx = 0;
	memset(gfx, 0, sizeof(gfx));

	/*
	 * Try and attach graphics devices first.
	 * Unfortunately, they - not being GIO devices after all (except for
	 * Impact) - do not contain a Product Identification Word, nor have
	 * a slot number.
	 *
	 * Record addresses to which graphics devices attach so that
	 * we do not confuse them with expansion slots, should the
	 * addresses coincide.
	 *
	 * If only the ARCBios component tree would be so kind as to give
	 * us the address of the frame buffer components...
	 */
	if (sys_type != SGI_IP22 ||
	    sys_config.system_subtype != IP22_CHALLS) {
		for (i = 0; gfx_bases[i].base != 0; i++) {
			/* skip slots that don't apply to us */
			if (gfx_bases[i].mach_type != sys_type)
				continue;

			if (gfx_bases[i].mach_subtype != -1 &&
			    gfx_bases[i].mach_subtype !=
			      sys_config.system_subtype)
				continue;

			ga.ga_addr = gfx_bases[i].base;
			ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);

			/* no need to probe a glass console again */
			if (ga.ga_addr == giofb_consaddr && giofb_consid != 0)
				id = giofb_consid;
			else {
				id = gio_id(ga.ga_ioh, ga.ga_addr, 1);
				if (!gio_is_framebuffer_id(id))
					continue;
			}

			ga.ga_iot = sc->sc_iot;
			ga.ga_dmat = sc->sc_dmat;
			ga.ga_slot = -1;
			ga.ga_product = id;
			/*
			 * Note that this relies upon ARCBios listing frame
			 * buffers in ascending address order, which seems
			 * to be the case so far on multihead Indigo2 systems.
			 */
			if (ngfx < GIO_MAX_FB)
				ga.ga_descr = giofb_names[ngfx];
			else
				ga.ga_descr = NULL;	/* shouldn't happen */

			if (config_found_sm(self, &ga, gio_print_fb,
			    gio_submatch))
				gfx[ngfx] = gfx_bases[i].base;

			ngfx++;
		}
	}

	/*
	 * Now attach any GIO expansion cards.
	 *
	 * Be sure to skip any addresses to which a graphics device has
	 * already been attached.
	 */
	for (i = 0; slot_bases[i].base != 0; i++) {
		int skip = 0;

		/* skip slots that don't apply to us */
		if (slot_bases[i].mach_type != sys_type)
			continue;

		if (slot_bases[i].mach_subtype != -1 &&
		    slot_bases[i].mach_subtype != sys_config.system_subtype)
			continue;

		for (j = 0; j < ngfx; j++) {
			if (slot_bases[i].base == gfx[j]) {
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;

		ga.ga_addr = slot_bases[i].base;
		ga.ga_iot = sc->sc_iot;
		ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);

		id = gio_id(ga.ga_ioh, ga.ga_addr, 0);
		if (id == 0)
			continue;

		ga.ga_dmat = sc->sc_dmat;
		ga.ga_slot = slot_bases[i].slot;
		ga.ga_product = id;
		ga.ga_descr = NULL;

		config_found_sm(self, &ga, gio_print, gio_submatch);
	}

	config_search(gio_search, self, aux);
}

/*
 * Try and figure out whether there is a device at the given slot address.
 */
uint32_t
gio_id(vaddr_t va, paddr_t pa, int maybe_gfx)
{
	uint32_t id32, mystery;
	uint16_t id16 = 0;
	uint8_t id8 = 0;

	/*
	 * First, attempt to read the address with various sizes.
	 *
	 * - GIO32 devices will only support reads from 32-bit aligned
	 *   addresses, in all sizes (at least for the ID register).
	 * - frame buffers will support aligned reads from any size at
	 *   any address, but will actually return the access width if
	 *   the slot is pipelined.
	 */

	if (guarded_read_4(va, &id32) != 0)
		return 0;

	/*
	 * If the address doesn't match a base slot address, then we are
	 * only probing for a light(4) frame buffer.
	 */

	if (pa != GIO_ADDR_GFX && pa != GIO_ADDR_EXP0 && pa != GIO_ADDR_EXP1) {
		if (maybe_gfx == 0)
			return 0;
		else {
			if (pa == LIGHT_ADDR_0 || pa == LIGHT_ADDR_1) {
				if (guarded_read_4(va + REX_PAGE1_SET +
				    REX_P1REG_XYOFFSET, &id32) != 0)
					return 0;
				if (id32 == 0x08000800)
					return GIO_PRODUCT_FAKEID_LIGHT;
			}
			return 0;
		}
	}

	/*
	 * GIO32 devices with a 32-bit ID register will not necessarily
	 * answer to addresses not aligned on 32 bit boundaries.
	 */

	if (guarded_read_2(va | 2, &id16) != 0 ||
	    guarded_read_1(va | 3, &id8) != 0) {
		if (GIO_PRODUCT_32BIT_ID(id32))
			return id32;
		else /* not a frame buffer anyway */
			return GIO_PRODUCT_PRODUCTID(id32);
	}

	/*
	 * Of course, GIO32 devices with a 8-bit ID register can use the
	 * other bytes in the first 32-bit word for other purposes.
	 */

	if ((id32 & 0xffff) == id16 && (id32 & 0xff) == id8) {
		if (GIO_PRODUCT_32BIT_ID(id32))
			return id32;
		else if (!GIO_PRODUCT_32BIT_ID(id8) && id8 != 0x00)
			return /*GIO_PRODUCT_PRODUCTID*/(id8);
	}

	/*
	 * If there is a frame buffer device, then either we have hit a
	 * device register (grtwo), or we did not fault because the slot
	 * is pipelined (newport).
	 * In the latter case, we attempt to probe a known register offset.
	 */

	if (maybe_gfx) {
		/*
		 * On (at least) Indy systems with newport graphics, the
		 * presence of a SCSI Expansion board (030-8133) in either
		 * slot will cause extra bits to be set in the topmost byte
		 * of the 32-bit access to the pipelined slot (i.e. the
		 * value of id32 is 0x18000004, not 0x00000004).
		 *
		 * This would prevent newport from being recognized
		 * properly.
		 *
		 * This behaviour seems to be specific to the SCSI board,
		 * since the E++ board does not trigger it. This would
		 * rule out an HPC1.x-specific cause.
		 * 
		 * We work around this by ignoring the topmost byte of id32
		 * from this point on, but it's ugly and isaish...
		 *
		 * Note that this is not necessary on Indigo 2 since this
		 * troublesome board can not be installed on such a system.
		 * Indigo are probably safe from this issues, for they can't
		 * use newport graphics; but the issue at hand might be
		 * HPC 1.x related, so better play safe.
		 */
		if (sys_config.system_type == SGI_IP20 ||
		    (sys_config.system_type == SGI_IP22 &&
		     sys_config.system_subtype != IP22_INDIGO2))
			id32 &= ~0xff000000;

		if (id32 != 4 || id16 != 2 || id8 != 1) {
			if (guarded_read_4(va + HQ2_MYSTERY, &mystery) == 0 &&
			    mystery == HQ2_MYSTERY_VALUE)
				return GIO_PRODUCT_FAKEID_GRTWO;
			else
				return 0;
		}

		/* could be newport(4) */
		if (pa == GIO_ADDR_GFX || pa == GIO_ADDR_EXP0) {
			va += NEWPORT_REX3_OFFSET;
			if (guarded_read_4(va, &id32) == 0 &&
			    guarded_read_2(va | 2, &id16) == 0 &&
			    guarded_read_1(va | 3, &id8) == 0) {
				if (id32 != 4 || id16 != 2 || id8 != 1)
					return GIO_PRODUCT_FAKEID_NEWPORT;
			}
		}

		return 0;
	}

	return 0;
}

int
gio_print(void *aux, const char *pnp)
{
	struct gio_attach_args *ga = aux;
	const char *descr;
	int product, revision;
	uint i;

	product = GIO_PRODUCT_PRODUCTID(ga->ga_product);
	if (GIO_PRODUCT_32BIT_ID(ga->ga_product))
		revision = GIO_PRODUCT_REVISION(ga->ga_product);
	else
		revision = 0;

	descr = "unknown GIO card";
	for (i = 0; gio_knowndevs[i].productid != 0; i++) {
		if (gio_knowndevs[i].productid == product) {
			descr = gio_knowndevs[i].product;
			break;
		}
	}

	if (pnp != NULL) {
		printf("%s", descr);
		if (ga->ga_product != -1)
			printf(" (product 0x%02x revision 0x%02x)",
			    product, revision);
		printf(" at %s", pnp);
	}

	if (ga->ga_slot != -1)
		printf(" slot %d", ga->ga_slot);
	printf(" addr 0x%lx", ga->ga_addr);

	return UNCONF;
}

int
gio_print_fb(void *aux, const char *pnp)
{
	struct gio_attach_args *ga = aux;
	const char *fbname;

	if (pnp != NULL) {
		switch (ga->ga_product) {
		case GIO_PRODUCT_FAKEID_GRTWO:
			fbname = "grtwo";
			break;
		case GIO_PRODUCT_FAKEID_LIGHT:
			fbname = "light";
			break;
		case GIO_PRODUCT_FAKEID_NEWPORT:
			fbname = "newport";
			break;
		default:
			if (GIO_PRODUCT_32BIT_ID(ga->ga_product) &&
			    GIO_PRODUCT_PRODUCTID(ga->ga_product) ==
			      GIO_PRODUCT_IMPACT)
				fbname = "impact";
			else	/* should never happen */
				fbname = "framebuffer";
			break;
		}
		printf("%s at %s", fbname, pnp);
	}

	printf(" addr 0x%lx", ga->ga_addr);

	return UNCONF;
}

int
gio_search(struct device *parent, void *vcf, void *aux)
{
	struct gio_softc *sc = (struct gio_softc *)parent;
	struct cfdata *cf = (struct cfdata *)vcf;
	struct gio_attach_args ga;

	/* Handled by direct configuration, so skip here */
	if (cf->cf_loc[1 /*GIOCF_ADDR*/] == -1)
		return 0;

	ga.ga_addr = (uint64_t)cf->cf_loc[1 /*GIOCF_ADDR*/];
	ga.ga_iot = sc->sc_iot;
	ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
	ga.ga_dmat = sc->sc_dmat;
	ga.ga_slot = cf->cf_loc[0 /*GIOCF_SLOT*/];
	ga.ga_product = -1;
	ga.ga_descr = NULL;

	if ((*cf->cf_attach->ca_match)(parent, cf, &ga) == 0)
		return 0;

	config_attach(parent, cf, &ga, gio_print);

	return 1;
}

int
gio_submatch(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = (struct cfdata *)vcf;
	struct gio_attach_args *ga = (struct gio_attach_args *)aux;

	if (cf->cf_loc[0 /*GIOCF_SLOT*/] != -1 &&
	    cf->cf_loc[0 /*GIOCF_SLOT*/] != ga->ga_slot)
		return 0;

	if (cf->cf_loc[1 /*GIOCF_ADDR*/] != -1 &&
	    (uint64_t)cf->cf_loc[1 /*GIOCF_ADDR*/] != ga->ga_addr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, aux);
}

int
giofb_cnprobe()
{
	struct gio_attach_args ga;
	uint32_t id;
	int i;
	int sys_type;

	switch (sys_config.system_type) {
	case SGI_IP20:
		sys_type = SGI_IP20;
		break;
	default:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		sys_type = SGI_IP22;
		break;
	}

	for (i = 0; gfx_bases[i].base != 0; i++) {
		if (giofb_consaddr != 0 &&
		    gfx_bases[i].base != giofb_consaddr)
			continue;

		/* skip bases that don't apply to us */
		if (gfx_bases[i].mach_type != sys_type)
			continue;

		if (gfx_bases[i].mach_subtype != -1 &&
		    gfx_bases[i].mach_subtype != sys_config.system_subtype)
			continue;

		ga.ga_addr = gfx_bases[i].base;
		ga.ga_iot = &imcbus_tag;
		ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
		ga.ga_dmat = &imc_bus_dma_tag;
		ga.ga_slot = -1;
		ga.ga_descr = NULL;

		id = gio_id(ga.ga_ioh, ga.ga_addr, 1);
		if (!gio_is_framebuffer_id(id))
			continue;

		ga.ga_product = giofb_consid = id;
		switch (id) {
		default:
#if NIMPACT_GIO > 0
			if (GIO_PRODUCT_32BIT_ID(id) &&
			    GIO_PRODUCT_PRODUCTID(id) == GIO_PRODUCT_IMPACT) {
				if (impact_gio_cnprobe(&ga) != 0)
					return 0;
			}
#endif
			break;
		case GIO_PRODUCT_FAKEID_GRTWO:
#if NGRTWO > 0
			if (grtwo_cnprobe(&ga) != 0)
				return 0;
#endif
			break;
		case GIO_PRODUCT_FAKEID_LIGHT:
#if NLIGHT > 0
			if (light_cnprobe(&ga) != 0)
				return 0;
#endif
			break;
		case GIO_PRODUCT_FAKEID_NEWPORT:
#if NNEWPORT > 0
			if (newport_cnprobe(&ga) != 0)
				return 0;
#endif
			break;
		}
	}

	return ENXIO;
}

int
giofb_cnattach()
{
	struct gio_attach_args ga;

	ga.ga_addr = giofb_consaddr;
	ga.ga_iot = &imcbus_tag;
	ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
	ga.ga_dmat = &imc_bus_dma_tag;
	ga.ga_slot = -1;
	ga.ga_product = giofb_consid;
	ga.ga_descr = NULL;

	switch (giofb_consid) {
	default:
#if NIMPACT_GIO > 0
		if (GIO_PRODUCT_32BIT_ID(giofb_consid) &&
		    GIO_PRODUCT_PRODUCTID(giofb_consid) == GIO_PRODUCT_IMPACT) {
			if (impact_gio_cnattach(&ga) == 0)
				return 0;
		}
#endif
		break;
	case GIO_PRODUCT_FAKEID_GRTWO:
#if NGRTWO > 0
		if (grtwo_cnattach(&ga) == 0)
			return 0;
#endif
		break;
	case GIO_PRODUCT_FAKEID_LIGHT:
#if NLIGHT > 0
		if (light_cnattach(&ga) == 0)
			return 0;
#endif
		break;
	case GIO_PRODUCT_FAKEID_NEWPORT:
#if NNEWPORT > 0
		if (newport_cnattach(&ga) == 0)
			return 0;
#endif
		break;
	}

	giofb_consaddr = 0;
	return ENXIO;
}

int
gio_is_framebuffer_id(uint32_t id)
{
	switch (id) {
	case GIO_PRODUCT_FAKEID_GRTWO:
	case GIO_PRODUCT_FAKEID_LIGHT:
	case GIO_PRODUCT_FAKEID_NEWPORT:
		return 1;
	default:
		if (GIO_PRODUCT_32BIT_ID(id) &&
		    GIO_PRODUCT_PRODUCTID(id) == GIO_PRODUCT_IMPACT)
			return 1;
		else
			return 0;
	}
}

/*
 * Devices living in the expansion slots must enable or disable some
 * GIO arbiter settings. This is accomplished via imc(4) registers.
 */
int
gio_arb_config(int slot, uint32_t flags)
{
	if (flags == 0)
		return (EINVAL);

	if (flags & ~(GIO_ARB_RT | GIO_ARB_LB | GIO_ARB_MST | GIO_ARB_SLV |
	    GIO_ARB_PIPE | GIO_ARB_NOPIPE | GIO_ARB_32BIT | GIO_ARB_64BIT |
	    GIO_ARB_HPC2_32BIT | GIO_ARB_HPC2_64BIT))
		return (EINVAL);

	if (((flags & GIO_ARB_RT)   && (flags & GIO_ARB_LB))  ||
	    ((flags & GIO_ARB_MST)  && (flags & GIO_ARB_SLV)) ||
	    ((flags & GIO_ARB_PIPE) && (flags & GIO_ARB_NOPIPE)) ||
	    ((flags & GIO_ARB_32BIT) && (flags & GIO_ARB_64BIT)) ||
	    ((flags & GIO_ARB_HPC2_32BIT) && (flags & GIO_ARB_HPC2_64BIT)))
		return (EINVAL);

	return (imc_gio64_arb_config(slot, flags));
}

/*
 * Return the logical interrupt number for an expansion board (not a frame
 * buffer!) in the specified slot.
 *
 * Indy and Challenge S have a single GIO interrupt per GIO slot, but
 * distinct slot interrups. Indigo and Indigo2 have three GIO interrupts per
 * slot, but at a given GIO interrupt level, all slots share the same
 * interrupt on the interrupt controller.
 *
 * Expansion boards appear to always use the intermediate level.
 */
int
gio_intr_map(int slot)
{
	switch (sys_config.system_type) {
	case SGI_IP20:
		if (slot == GIO_SLOT_GFX)
			return -1;
		return INT2_L0_INTR(INT2_L0_GIO_LVL1);
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		if (sys_config.system_subtype == IP22_INDIGO2) {
			if (slot == GIO_SLOT_EXP1)
				return -1;
			return INT2_L0_INTR(INT2_L0_GIO_LVL1);
		} else {
			if (slot == GIO_SLOT_GFX)
				return -1;
			return INT2_MAP1_INTR(slot == GIO_SLOT_EXP0 ?
			    INT2_MAP_GIO_SLOT0 : INT2_MAP_GIO_SLOT1);
		}
	default:
		return -1;
	}
}

void *
gio_intr_establish(int intr, int level, int (*func)(void *), void *arg,
    const char *what)
{
	return int2_intr_establish(intr, level, func, arg, what);
}

const char *
gio_product_string(int prid)
{
	int i;

	for (i = 0; gio_knowndevs[i].product != NULL; i++)
		if (gio_knowndevs[i].productid == prid)
			return (gio_knowndevs[i].product);

	return (NULL);
}

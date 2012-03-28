/*	$OpenBSD: gio.c,v 1.1 2012/03/28 20:44:23 miod Exp $	*/
/*	$NetBSD: gio.c,v 1.32 2011/07/01 18:53:46 dyoung Exp $	*/

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
#include <sgi/gio/giodevs_data.h>

#include <sgi/localbus/imcvar.h>
#include <sgi/localbus/intvar.h>
#include <sgi/sgi/ip22.h>

int	 gio_match(struct device *, void *, void *);
void	 gio_attach(struct device *, struct device *, void *);
int	 gio_print(void *, const char *);
int	 gio_print_fb(void *, const char *);
int	 gio_search(struct device *, void *, void *);
int	 gio_submatch(struct device *, void *, void *);
uint32_t gio_id(paddr_t, int);

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

struct gio_probe {
	uint32_t slot;
	uint64_t base;
	uint32_t mach_type;
	uint32_t mach_subtype;
};

/*
 * Expansion Slot Base Addresses
 *
 * IP12, IP20 and IP24 have two GIO connectors: GIO_SLOT_EXP0 and
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
	{ GIO_SLOT_GFX,  0x1f000000, SGI_IP22, IP22_INDIGO2 },

	{ GIO_SLOT_EXP0, 0x1f400000, SGI_IP20, -1 },
	{ GIO_SLOT_EXP0, 0x1f400000, SGI_IP22, -1 },

	{ GIO_SLOT_EXP1, 0x1f600000, SGI_IP20, -1 },
	{ GIO_SLOT_EXP1, 0x1f600000, SGI_IP22, IP22_INDY },

	{ 0, 0, 0, 0 }
};

/*
 * Graphic Board Base Addresses
 *
 * Graphics boards are not treated like expansion slot cards. Their base
 * addresses do not necessarily correspond to GIO slot addresses and they
 * do not contain product identification words. 
 */
static const struct gio_probe gfx_bases[] = {
	/* grtwo, and newport on IP22 */
	{ -1, 0x1f000000, SGI_IP20, -1 },
	{ -1, 0x1f000000, SGI_IP22, -1 },

	/* light */
	{ -1, 0x1f3f0000, SGI_IP20, -1 },

	/* light (dual headed) */
	{ -1, 0x1f3f8000, SGI_IP20, -1 },

	/* grtwo, and newport on IP22 */
	{ -1, 0x1f400000, SGI_IP20, -1 },
	{ -1, 0x1f400000, SGI_IP22, -1 },

	/* grtwo */
	{ -1, 0x1f600000, SGI_IP20, -1 },
	{ -1, 0x1f600000, SGI_IP22, -1 },

	/* newport */
	{ -1, 0x1f800000, SGI_IP22, IP22_INDIGO2 },

	/* newport */
	{ -1, 0x1fc00000, SGI_IP22, IP22_INDIGO2 },

	{ 0, 0, 0, 0 }
};

/* maximum number of graphics boards possible (arbitrarily large estimate) */
#define MAXGFX (nitems(gfx_bases) - 1)

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
	uint32_t gfx[MAXGFX];
	uint i, j, ngfx;

	printf("\n");

	sc->sc_iot = iaa->iaa_st;
	sc->sc_dmat = iaa->iaa_dmat;

	ngfx = 0;
	memset(gfx, 0, sizeof(gfx));

	/*
	 * Try and attach graphics devices first.
	 * Unfortunately, they - not being GIO devices after all - do not
	 * contain a Product Identification Word, nor have a slot number.
	 *
	 * Record addresses to which graphics devices attach so that
	 * we do not confuse them with expansion slots, should the
	 * addresses coincide.
	 *
	 * Unfortunately graphics devices for which we have no configured
	 * driver, which address matches a regular slot number, will show
	 * up as rogue devices attached to real slots.
	 *
	 * If only the ARCBios component tree would be so kind as to give
	 * us the address of the frame buffer components...
	 */
	for (i = 0; gfx_bases[i].base != 0; i++) {
		/* skip slots that don't apply to us */
		if (gfx_bases[i].mach_type != sys_config.system_type)
			continue;

		if (gfx_bases[i].mach_subtype != -1 &&
		    gfx_bases[i].mach_subtype != sys_config.system_subtype)
			continue;

		ga.ga_slot = -1;
		ga.ga_addr = gfx_bases[i].base;
		ga.ga_iot = sc->sc_iot;
		ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
		ga.ga_dmat = sc->sc_dmat;
		ga.ga_product = -1;

		if (gio_id(ga.ga_ioh, 1) == 0)
			continue;
		
		if (config_found_sm(self, &ga, gio_print_fb, gio_submatch)) {
			gfx[ngfx++] = gfx_bases[i].base;
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
		if (slot_bases[i].mach_type != sys_config.system_type)
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

		ga.ga_slot = slot_bases[i].slot;
		ga.ga_addr = slot_bases[i].base;
		ga.ga_iot = sc->sc_iot;
		ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
		ga.ga_dmat = sc->sc_dmat;

		if (gio_id(ga.ga_ioh, 0) == 0)
			continue;

		ga.ga_product = bus_space_read_4(ga.ga_iot, ga.ga_ioh, 0);

		config_found_sm(self, &ga, gio_print, gio_submatch);
	}

	config_search(gio_search, self, aux);
}

/*
 * Try and figure out whether there is a device at the given slot address.
 */
uint32_t
gio_id(paddr_t pa, int maybe_gfx)
{
	uint32_t id32;
	uint16_t id16 = 0;
	uint8_t id8 = 0;

	if (guarded_read_4(pa, &id32) != 0)
		return 0;

	id16 = id32 ^ 0xffff;
	(void)guarded_read_2(pa | 2, &id16);
	id8 = id16 ^ 0xff;
	(void)guarded_read_1(pa | 3, &id8);

	/*
	 * If there is a real GIO device at this address (as opposed to
	 * a graphics card), then the low-order 8 bits of each read need
	 * to be consistent...
	 */
	if (id8 == (id16 & 0xff) && id8 == (id32 & 0xff)) {
		if (GIO_PRODUCT_32BIT_ID(id8)) {
			if (id16 == (id32 & 0xffff))
				return id32;
		} else {
			if (id8 != 0)
				return id32;
		}
	}

	if (maybe_gfx)
		return 1;

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
#if 0 /* once we can know for sure there really is a frame buffer here */
	if (pnp != NULL)
		printf("framebuffer at %s", pnp);

	if (ga->ga_addr != (uint64_t)-1)
		printf(" addr 0x%lx", ga->ga_addr);

	return UNCONF;
#else
	return QUIET;
#endif
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

	ga.ga_product = -1;
	ga.ga_slot = cf->cf_loc[0 /*GIOCF_SLOT*/];
	ga.ga_addr = (uint64_t)cf->cf_loc[1 /*GIOCF_ADDR*/];
	ga.ga_iot = sc->sc_iot;
	ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
	ga.ga_dmat = sc->sc_dmat;

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

#if 0	/* XXX involve wscons_machdep somehow */
int
gio_cnattach(void)
{
	extern struct machine_bus_dma_tag imc_bus_dma_tag;	/* XXX */
	extern bus_space_t imcbus_tag;				/* XXX */
	struct gio_attach_args ga;
	uint32_t dummy;
	int i;

	for (i = 0; gfx_bases[i].base != 0; i++) {
		/* skip bases that don't apply to us */
		if (gfx_bases[i].mach_type != sys_config.system_type)
			continue;

		if (gfx_bases[i].mach_subtype != -1 &&
		    gfx_bases[i].mach_subtype != sys_config.system_subtype)
			continue;

		ga.ga_slot = -1;
		ga.ga_addr = gfx_bases[i].base;
		ga.ga_iot = &imcbus_tag;			/* XXX */
		ga.ga_ioh = PHYS_TO_XKPHYS(ga.ga_addr, CCA_NC);
		ga.ga_dmat = &imc_bus_dma_tag;			/* XXX */
		ga.ga_product = -1;
		
		if (gio_id(ga.ga_ioh, 1) == 0)
			continue;

#if NGRTWO > 0
		if (grtwo_cnattach(&ga) == 0)
			return 0;
#endif

#if NLIGHT > 0
		if (light_cnattach(&ga) == 0)
			return 0;
#endif

#if NNEWPORT > 0
		if (newport_cnattach(&ga) == 0)
			return 0;
#endif

	}

	return ENXIO;
}
#endif

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
 * Establish an interrupt handler for the specified slot.
 *
 * Indy and Challenge S have an interrupt per GIO slot. Indigo and Indigo2
 * share a single interrupt, however.
 */
void *
gio_intr_establish(int slot, int level, int (*func)(void *), void *arg,
    const char *what)
{
	int intr;

	switch (sys_config.system_type) {
	case SGI_IP20:
		if (slot == GIO_SLOT_GFX)
			return NULL;
		intr = 6;
		break;
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		if (sys_config.system_subtype == IP22_INDIGO2) {
			if (slot == GIO_SLOT_EXP1)
				return NULL;
			intr = 6;
		} else {
			if (slot == GIO_SLOT_GFX)
				return NULL;
			intr = (slot == GIO_SLOT_EXP0) ? 22 : 23;
		}
		break;
	default:
		return NULL;
	}

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

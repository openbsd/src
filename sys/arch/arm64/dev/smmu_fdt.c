/* $OpenBSD: smmu_fdt.c,v 1.5 2022/04/06 18:59:26 naddy Exp $ */
/*
 * Copyright (c) 2021 Patrick Wildt <patrick@blueri.se>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/pool.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <dev/pci/pcivar.h>
#include <arm64/dev/smmuvar.h>
#include <arm64/dev/smmureg.h>

struct smmu_fdt_softc {
	struct smmu_softc	 sc_smmu;

	struct iommu_device	 sc_id;
};

int smmu_fdt_match(struct device *, void *, void *);
void smmu_fdt_attach(struct device *, struct device *, void *);

bus_dma_tag_t smmu_fdt_map(void *, uint32_t *, bus_dma_tag_t);
void smmu_fdt_reserve(void *, uint32_t *, bus_addr_t, bus_size_t);

const struct cfattach smmu_fdt_ca = {
	sizeof(struct smmu_fdt_softc), smmu_fdt_match, smmu_fdt_attach
};

int
smmu_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "arm,smmu-v2") ||
	    OF_is_compatible(faa->fa_node, "arm,mmu-500"));
}

void
smmu_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct smmu_fdt_softc *fsc = (struct smmu_fdt_softc *)self;
	struct smmu_softc *sc = &fsc->sc_smmu;
	struct fdt_attach_args *faa = aux;
	uint32_t ngirq;
	int i;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_dmat = faa->fa_dmat;
	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	if (OF_is_compatible(faa->fa_node, "arm,mmu-500"))
		sc->sc_is_mmu500 = 1;
	if (OF_is_compatible(faa->fa_node, "marvell,ap806-smmu-500"))
		sc->sc_is_ap806 = 1;
	if (OF_getproplen(faa->fa_node, "dma-coherent") == 0)
		sc->sc_coherent = 1;

	if (smmu_attach(sc) != 0)
		return;

	ngirq = OF_getpropint(faa->fa_node, "#global-interrupts", 1);
	for (i = 0; i < ngirq; i++) {
		fdt_intr_establish_idx(faa->fa_node, i, IPL_TTY,
		    smmu_global_irq, sc, sc->sc_dev.dv_xname);
	}
	for (i = ngirq; ; i++) {
		struct smmu_cb_irq *cbi = malloc(sizeof(*cbi),
		    M_DEVBUF, M_WAITOK);
		cbi->cbi_sc = sc;
		cbi->cbi_idx = i - ngirq;
		if (fdt_intr_establish_idx(faa->fa_node, i, IPL_TTY,
		    smmu_context_irq, cbi, sc->sc_dev.dv_xname) == NULL) {
			free(cbi, M_DEVBUF, sizeof(*cbi));
			break;
		}
	}

	fsc->sc_id.id_node = faa->fa_node;
	fsc->sc_id.id_cookie = fsc;
	fsc->sc_id.id_map = smmu_fdt_map;
	fsc->sc_id.id_reserve = smmu_fdt_reserve;
	iommu_device_register(&fsc->sc_id);
}

bus_dma_tag_t
smmu_fdt_map(void *cookie, uint32_t *cells, bus_dma_tag_t dmat)
{
	struct smmu_fdt_softc *fsc = (struct smmu_fdt_softc *)cookie;
	struct smmu_softc *sc = &fsc->sc_smmu;

	return smmu_device_map(sc, cells[0], dmat);
}

void
smmu_fdt_reserve(void *cookie, uint32_t *cells, bus_addr_t addr,
    bus_size_t size)
{
	struct smmu_fdt_softc *fsc = (struct smmu_fdt_softc *)cookie;
	struct smmu_softc *sc = &fsc->sc_smmu;

	return smmu_reserve_region(sc, cells[0], addr, size);
}

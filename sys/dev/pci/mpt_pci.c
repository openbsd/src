/*	$OpenBSD: mpt_pci.c,v 1.6 2004/08/23 21:00:42 marco Exp $	*/
/*	$NetBSD: mpt_pci.c,v 1.2 2003/07/14 15:47:26 lukem Exp $	*/

/*
 * Copyright (c) 2004 Milos Urbanek
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * mpt_pci.c:
 *
 * OpenBSD PCI-specific routines for LSI Fusion adapters.
 */     

#include <sys/cdefs.h>
/* __KERNEL_RCSID(0, "$NetBSD: mpt_pci.c,v 1.2 2003/07/14 15:47:26 lukem Exp $"); */

#include <dev/ic/mpt.h>			/* pulls in all headers */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define	MPT_PCI_MMBA		(PCI_MAPREG_START+0x04)
#define PCI_MAPREG_ROM	0x30

void mpt_pci_attach(struct device *, struct device *, void *);
int mpt_pci_match(struct device *, void *, void *);
const struct mpt_pci_product *mpt_pci_lookup(const struct pci_attach_args *);

struct mpt_pci_softc {
	mpt_softc_t sc_mpt;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;

	void *sc_ih;

	/* Saved volatile PCI configuration registers. */
	pcireg_t sc_pci_csr;
	pcireg_t sc_pci_bhlc;
	pcireg_t sc_pci_io_bar;
	pcireg_t sc_pci_mem0_bar[2];
	pcireg_t sc_pci_mem1_bar[2];
	pcireg_t sc_pci_rom_bar;
	pcireg_t sc_pci_int;
	pcireg_t sc_pci_pmcsr;
};

void	mpt_pci_link_peer(mpt_softc_t *);
void	mpt_pci_read_config_regs(mpt_softc_t *);
void	mpt_pci_set_config_regs(mpt_softc_t *);

#define	MPP_F_FC	0x01	/* Fibre Channel adapter */
#define	MPP_F_DUAL	0x02	/* Dual port adapter */

static const struct mpt_pci_product {
	pci_vendor_id_t		mpp_vendor;
	pci_product_id_t	mpp_product;
	int			mpp_flags;
} mpt_pci_products[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_1030,
	  MPP_F_DUAL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC909,
	  MPP_F_FC },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC909A,
	  MPP_F_FC },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC929,
	  MPP_F_FC | MPP_F_DUAL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC929_1,
	  MPP_F_FC | MPP_F_DUAL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC919,
	  MPP_F_FC },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC919_1,
	  MPP_F_FC },
	{ 0,			0,
	  0 },
};

const struct mpt_pci_product *
mpt_pci_lookup(const struct pci_attach_args *pa)
{
	const struct mpt_pci_product *mpp;

	for (mpp = mpt_pci_products; mpp->mpp_vendor != 0; mpp++) {
		if (PCI_VENDOR(pa->pa_id) == mpp->mpp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == mpp->mpp_product)
			return (mpp);
	}
	return (NULL);
}

/* probe for mpt controller */
int
mpt_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (mpt_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
mpt_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct mpt_pci_softc *psc = (void *) self;
	mpt_softc_t *mpt = &psc->sc_mpt;
	struct pci_attach_args *pa = aux;
	const struct mpt_pci_product *mpp;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg, memtype;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	int memh_valid;

	mpp = mpt_pci_lookup(pa);
	if (mpp == NULL) {
		printf(": mpt_pci_lookup failed\n");
		return;
	}

	psc->sc_pc = pa->pa_pc;
	psc->sc_tag = pa->pa_tag;

	mpt->sc_dmat = pa->pa_dmat;
	mpt->sc_set_config_regs = mpt_pci_set_config_regs;

	/*
	 * Map the device.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MPT_PCI_MMBA);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, MPT_PCI_MMBA,
		    memtype, 0, &memt, &memh, NULL, NULL, 0) == 0);
		break;

	default:
		memh_valid = 0;
	}

	if (memh_valid) {
		mpt->sc_st = memt;
		mpt->sc_sh = memh;
	} else {
		printf(": unable to map device registers\n");
		return;
	}

	/*
	 * Make sure the PCI command register is properly configured.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	/* XXX PCI_COMMAND_INVALIDATE_ENABLE */
	/* XXX PCI_COMMAND_PARITY_ENABLE */
	/* XXX PCI_COMMAND_SERR_ENABLE */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Ensure that the ROM is diabled.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM);
	reg &= ~1;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM, reg);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, mpt_intr, mpt,
	    mpt->mpt_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s\n", intrstr ? intrstr : "?");

	/* Disable interrupts on the part. */
	mpt_disable_ints(mpt);

	/* Allocate DMA memory. */
	if (mpt_dma_mem_alloc(mpt) != 0) {
		printf(": unable to allocate DMA memory\n");
		return;
	}

	/*
	 * Save the PCI config register values.
	 *
	 * Hard resets are known to screw up the BAR for diagnostic
	 * memory accesses (Mem1).
	 *
	 * Using Mem1 is know to make the chip stop responding to
	 * configuration cycles, so we need to save it now.
	 */
	mpt_pci_read_config_regs(mpt);

	/*
	 * If we're a dual-port adapter, try to find our peer.  We
	 * need to fix his PCI config registers, too.
	 */
	if (mpp->mpp_flags & MPP_F_DUAL) {
		mpt_pci_link_peer(mpt);
	}
	/* Initialize the hardware. */
	if (mpt_init(mpt, MPT_DB_INIT_HOST) != 0) {
		/* Error message already printed. */
		return;
	}

	/* Complete attachment of hardware, include subdevices. */
	mpt_attach(mpt);
}

#if defined(__NetBSD__)
CFATTACH_DECL(mpt_pci, sizeof(struct mpt_pci_softc),
    mpt_pci_match, mpt_pci_attach, NULL, NULL);
#else
struct cfattach mpt_pci_ca = {
	sizeof (struct mpt_pci_softc), mpt_pci_match, mpt_pci_attach,
};
#endif

/*
 * Find and remember our peer PCI function on a dual-port device.
 */
void
mpt_pci_link_peer(mpt_softc_t *mpt)
{
	extern struct cfdriver mpt_cd;

	struct mpt_pci_softc *peer_psc, *psc = (void *) mpt;
	struct device *dev;
	int unit, b, d, f, peer_b, peer_d, peer_f;

	pci_decompose_tag(psc->sc_pc, psc->sc_tag, &b, &d, &f);

	for (unit = 0; unit < mpt_cd.cd_ndevs; unit++) {
		if (unit == mpt->mpt_dev.dv_unit)
			continue;
		dev = device_lookup(&mpt_cd, unit);
		if (dev == NULL)
			continue;
		if (dev->dv_cfdata == NULL)
			continue;
		if (dev->dv_cfdata->cf_attach != &mpt_pci_ca)
			continue;
		peer_psc = (void *) dev;
		if (peer_psc->sc_pc != psc->sc_pc)
			continue;
		pci_decompose_tag(peer_psc->sc_pc, peer_psc->sc_tag,
		    &peer_b, &peer_d, &peer_f);
		if (peer_b == b && peer_d == d) {
			if (mpt->verbose)
				mpt_prt(mpt, "linking with peer: %s",
				    peer_psc->sc_mpt.mpt_dev.dv_xname);
			mpt->mpt2 = (mpt_softc_t *) peer_psc;
			peer_psc->sc_mpt.mpt2 = mpt;
			return;
		}
	}
}

/*
 * Save the volatile PCI configuration registers.
 */
void
mpt_pci_read_config_regs(mpt_softc_t *mpt)
{
	struct mpt_pci_softc *psc = (void *) mpt;

	psc->sc_pci_csr = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_COMMAND_STATUS_REG);
	psc->sc_pci_bhlc = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_BHLC_REG);
	psc->sc_pci_io_bar = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_START);
	psc->sc_pci_mem0_bar[0] = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_START+0x04);
	psc->sc_pci_mem0_bar[1] = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_START+0x08);
	psc->sc_pci_mem1_bar[0] = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_START+0x0c);
	psc->sc_pci_mem1_bar[1] = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_START+0x10);
	psc->sc_pci_rom_bar = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_MAPREG_ROM);
	psc->sc_pci_int = pci_conf_read(psc->sc_pc, psc->sc_tag,
	    PCI_INTERRUPT_REG);
	psc->sc_pci_pmcsr = pci_conf_read(psc->sc_pc, psc->sc_tag, 0x44);
}

/*
 * Restore the volatile PCI configuration registers.
 */
void
mpt_pci_set_config_regs(mpt_softc_t *mpt)
{
	struct mpt_pci_softc *psc = (void *) mpt;

	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_COMMAND_STATUS_REG,
	    psc->sc_pci_csr);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_BHLC_REG,
	    psc->sc_pci_bhlc);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_START,
	    psc->sc_pci_io_bar);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_START+0x04,
	    psc->sc_pci_mem0_bar[0]);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_START+0x08,
	    psc->sc_pci_mem0_bar[1]);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_START+0x0c,
	    psc->sc_pci_mem1_bar[0]);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_START+0x10,
	    psc->sc_pci_mem1_bar[1]);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_MAPREG_ROM,
	    psc->sc_pci_rom_bar);
	pci_conf_write(psc->sc_pc, psc->sc_tag, PCI_INTERRUPT_REG,
	    psc->sc_pci_int);
	pci_conf_write(psc->sc_pc, psc->sc_tag, 0x44, psc->sc_pci_pmcsr);
}

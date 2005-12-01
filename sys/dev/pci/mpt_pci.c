/*	$OpenBSD: mpt_pci.c,v 1.11 2005/12/01 22:10:06 dlg Exp $	*/
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

#include <dev/ic/mpt.h>			/* pulls in all headers */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define MPT_PCI_MMBA	(PCI_MAPREG_START+0x04)
#define PCI_MAPREG_ROM	0x30

void	mpt_pci_attach(struct device *, struct device *, void *);
int	mpt_pci_match(struct device *, void *, void *);
const struct mpt_pci_product *mpt_pci_lookup(const struct pci_attach_args *);

struct mpt_pci_softc {
	struct mpt_softc	sc_mpt;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	void			*sc_ih;

	/* Saved volatile PCI configuration registers. */
	pcireg_t		sc_pci_csr;
	pcireg_t		sc_pci_bhlc;
	pcireg_t		sc_pci_io_bar;
	pcireg_t		sc_pci_mem0_bar[2];
	pcireg_t		sc_pci_mem1_bar[2];
	pcireg_t		sc_pci_rom_bar;
	pcireg_t		sc_pci_int;
	pcireg_t		sc_pci_pmcsr;
};

void	mpt_pci_link_peer(struct mpt_softc *);
void	mpt_pci_read_config_regs(struct mpt_softc *);
void	mpt_pci_set_config_regs(struct mpt_softc *);

struct cfattach mpt_pci_ca = {
	sizeof (struct mpt_pci_softc), mpt_pci_match, mpt_pci_attach,
};

#define PREAD(s, r)	pci_conf_read((s)->sc_pc, (s)->sc_tag, (r))
#define PWRITE(s, r, v)	pci_conf_write((s)->sc_pc, (s)->sc_tag, (r), (v))

#define MPP_F_FC	0x01	/* Fibre Channel adapter */
#define MPP_F_DUAL	0x02	/* Dual port adapter */

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
	  0 }
};

const struct mpt_pci_product *
mpt_pci_lookup(const struct pci_attach_args *pa)
{
	const struct mpt_pci_product	*mpp;

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
	struct pci_attach_args		*pa = aux;

	if (mpt_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
mpt_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct mpt_pci_softc		*psc = (void *)self;
	struct mpt_softc		*mpt = &psc->sc_mpt;
	struct pci_attach_args		*pa = aux;
	const struct mpt_pci_product	*mpp;
	pci_intr_handle_t		ih;
	const char			*intrstr;
	pcireg_t			memtype;

	mpp = mpt_pci_lookup(pa);
	if (mpp == NULL) {
		printf(": mpt_pci_lookup failed\n");
		return;
	}

	psc->sc_pc = pa->pa_pc;
	psc->sc_tag = pa->pa_tag;

	mpt->sc_dmat = pa->pa_dmat;
	mpt->sc_set_config_regs = mpt_pci_set_config_regs;

	/* Map the device. */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MPT_PCI_MMBA);
	if (memtype != (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT) &&
	    memtype != (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT)) {
		printf(": incorrect memory map type\n");
		return;
	}

	if (pci_mapreg_map(pa, MPT_PCI_MMBA, memtype, 0, &mpt->sc_st,
	    &mpt->sc_sh, NULL, NULL, 0) != 0) {
		printf(": unable to map device registers\n");
		return;
	}

	/* Ensure that the ROM is disabled.  */
	PWRITE(psc, PCI_MAPREG_ROM, PREAD(psc, PCI_MAPREG_ROM & ~1));

	/* Map and establish our interrupt. */
	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, mpt_intr, mpt,
	    mpt->mpt_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf(": unable to establish interrupt%s%s\n",
		    intrstr != NULL ? " at " : "",
		    intrstr != NULL ? intrstr : "");
		return;
	}

	/* Disable interrupts on the part. */
	mpt_disable_ints(mpt);

	/* Allocate DMA memory. */
	if (mpt_dma_mem_alloc(mpt) != 0) {
		printf(": unable to allocate DMA memory\n");
		return;
	}

	printf(": %s\n", intrstr ? intrstr : "?");

	/*
	 * Save the PCI config register values.
	 *
	 * Hard resets are known to screw up the BAR for diagnostic
	 * memory accesses (Mem1).
	 *
	 * Using Mem1 is known to make the chip stop responding to
	 * configuration cycles, so we need to save it now.
	 */
	mpt_pci_read_config_regs(mpt);

	/*
	 * If we're a dual-port adapter, try to find our peer.  We
	 * need to fix its PCI config registers too.
	 */
	if (mpp->mpp_flags & MPP_F_DUAL)
		mpt_pci_link_peer(mpt);

	/* Initialize the hardware. */
	if (mpt_init(mpt, MPT_DB_INIT_HOST) != 0) {
		/* Error message already printed. */
		return;
	}

	/* Complete attachment of hardware, include subdevices. */
	mpt_attach(mpt);
}

/*
 * Find and remember our peer PCI function on a dual-port device.
 */
void
mpt_pci_link_peer(struct mpt_softc *mpt)
{
	extern struct cfdriver		mpt_cd;

	struct mpt_pci_softc		*peer_psc, *psc = (void *)mpt;
	struct device			*dev;
	int				unit, b, d, f, peer_b, peer_d, peer_f;

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
		peer_psc = (void *)dev;
		if (peer_psc->sc_pc != psc->sc_pc)
			continue;
		pci_decompose_tag(peer_psc->sc_pc, peer_psc->sc_tag,
		    &peer_b, &peer_d, &peer_f);
		if (peer_b == b && peer_d == d) {
			if (mpt->verbose)
				mpt_prt(mpt, "linking with peer: %s",
				    peer_psc->sc_mpt.mpt_dev.dv_xname);
			mpt->mpt2 = (struct mpt_softc *)peer_psc;
			peer_psc->sc_mpt.mpt2 = mpt;
			return;
		}
	}
}

/*
 * Save the volatile PCI configuration registers.
 */
void
mpt_pci_read_config_regs(struct mpt_softc *mpt)
{
	struct mpt_pci_softc		*psc = (void *)mpt;

	psc->sc_pci_csr = PREAD(psc, PCI_COMMAND_STATUS_REG);
	psc->sc_pci_bhlc = PREAD(psc, PCI_BHLC_REG);
	psc->sc_pci_io_bar = PREAD(psc, PCI_MAPREG_START);
	psc->sc_pci_mem0_bar[0] = PREAD(psc, PCI_MAPREG_START+0x04);
	psc->sc_pci_mem0_bar[1] = PREAD(psc, PCI_MAPREG_START+0x08);
	psc->sc_pci_mem1_bar[0] = PREAD(psc, PCI_MAPREG_START+0x0c);
	psc->sc_pci_mem1_bar[1] = PREAD(psc, PCI_MAPREG_START+0x10);
	psc->sc_pci_rom_bar = PREAD(psc, PCI_MAPREG_ROM);
	psc->sc_pci_int = PREAD(psc, PCI_INTERRUPT_REG);
	psc->sc_pci_pmcsr = PREAD(psc, 0x44);
}

/*
 * Restore the volatile PCI configuration registers.
 */
void
mpt_pci_set_config_regs(struct mpt_softc *mpt)
{
	struct mpt_pci_softc		*psc = (void *)mpt;

	PWRITE(psc, PCI_COMMAND_STATUS_REG, psc->sc_pci_csr);
	PWRITE(psc, PCI_BHLC_REG, psc->sc_pci_bhlc);
	PWRITE(psc, PCI_MAPREG_START, psc->sc_pci_io_bar);
	PWRITE(psc, PCI_MAPREG_START+0x04, psc->sc_pci_mem0_bar[0]);
	PWRITE(psc, PCI_MAPREG_START+0x08, psc->sc_pci_mem0_bar[1]);
	PWRITE(psc, PCI_MAPREG_START+0x0c, psc->sc_pci_mem1_bar[0]);
	PWRITE(psc, PCI_MAPREG_START+0x10, psc->sc_pci_mem1_bar[1]);
	PWRITE(psc, PCI_MAPREG_ROM, psc->sc_pci_rom_bar);
	PWRITE(psc, PCI_INTERRUPT_REG, psc->sc_pci_int);
	PWRITE(psc, 0x44, psc->sc_pci_pmcsr);
}

/*	$OpenBSD: i80321.c,v 1.6 2009/03/30 09:28:12 kettenis Exp $	*/
/*	$NetBSD: i80321.c,v 1.18 2006/02/25 02:28:56 wiz Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
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
 * Autoconfiguration support for the Intel i80321 I/O Processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

/*
 * Statically-allocated bus_space structure used to access the
 * i80321's own registers.
 */
struct bus_space i80321_bs_tag;

/*
 * There can be only one i80321, so we keep a global pointer to
 * the softc, so board-specific code can use features of the
 * i80321 without having to have a handle on the softc itself.
 */
struct i80321_softc *i80321_softc;

static int i80321_iopxs_print(void *, const char *);

/* Built-in devices. */
static const struct iopxs_device {
	const char *id_name;
	bus_addr_t id_offset;
	bus_size_t id_size;
} iopxs_devices[] = {
	{ "iopaau",	VERDE_AAU_BASE,		VERDE_AAU_SIZE },
/*	{ "iopdma",	VERDE_DMA_BASE0,	VERDE_DMA_CHSIZE },	*/
/*	{ "iopdma",	VERDE_DMA_BASE1,	VERDE_DMA_CHSIZE },	*/
	{ "iopiic",	VERDE_I2C_BASE0,	VERDE_I2C_CHSIZE },
	{ "iopiic",	VERDE_I2C_BASE1,	VERDE_I2C_CHSIZE },
/*	{ "iopssp",	VERDE_SSP_BASE,		VERDE_SSP_SIZE },	*/
	{ "iopmu",	VERDE_MU_BASE,		VERDE_MU_SIZE },
	{ "iopwdog",	0,			0 },
	{ NULL,		0,			0 }
};

static void i80321_pci_dma_init(struct i80321_softc *);

/* XXX - debug */
/*
 * i80321_attach:
 *
 *	Board-independent attach routine for the i80321.
 */
void
i80321_attach(struct i80321_softc *sc)
{
	struct pcibus_attach_args pba;
	const struct iopxs_device *id;
	struct iopxs_attach_args ia;
	pcireg_t preg;

	i80321_softc = sc;

	/*
	 * Program the Inbound windows.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR0,
	    (0xffffffff - (sc->sc_iwin[0].iwin_size - 1)) & 0xffffffc0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IATVR0,
	    sc->sc_iwin[0].iwin_xlate);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START, sc->sc_iwin[0].iwin_base_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x04, sc->sc_iwin[0].iwin_base_hi);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR1,
	    (0xffffffff - (sc->sc_iwin[1].iwin_size - 1)) & 0xffffffc0);
	/* no xlate for window 1 */

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x08, sc->sc_iwin[1].iwin_base_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x0c, sc->sc_iwin[1].iwin_base_hi);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR2,
	    (0xffffffff - (sc->sc_iwin[2].iwin_size - 1)) & 0xffffffc0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IATVR2,
	    sc->sc_iwin[2].iwin_xlate);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x10, sc->sc_iwin[2].iwin_base_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x14, sc->sc_iwin[2].iwin_base_hi);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IALR3,
	    (0xffffffff - (sc->sc_iwin[3].iwin_size - 1)) & 0xffffffc0);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_IATVR3,
	    sc->sc_iwin[3].iwin_xlate);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_IABAR3, sc->sc_iwin[3].iwin_base_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_IAUBAR3, sc->sc_iwin[3].iwin_base_hi);

	/*
	 * Mask (disable) the ATU interrupt sources.
	 * XXX May want to revisit this if we encounter
	 * XXX an application that wants it.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_ATUIMR,
	    ATUIMR_IMW1BU|ATUIMR_ISCEM|ATUIMR_RSCEM|ATUIMR_PST|
	    ATUIMR_DPE|ATUIMR_P_SERR_ASRT|ATUIMR_PMA|ATUIMR_PTAM|
	    ATUIMR_PTAT|ATUIMR_PMPE);

	/*
	 * Program the outbound windows.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OIOWTVR, sc->sc_ioout_xlate);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OMWTVR0, sc->sc_owin[0].owin_xlate_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OUMWTVR0, sc->sc_owin[0].owin_xlate_hi);

	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OMWTVR1, sc->sc_owin[1].owin_xlate_lo);
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OUMWTVR1, sc->sc_owin[1].owin_xlate_hi);

	/*
	 * Set up the ATU configuration register.  All we do
	 * right now is enable Outbound Windows.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh, ATU_ATUCR,
	    ATUCR_OUT_EN);

	/*
	 * Enable bus mastering, memory access, SERR, and parity
	 * checking on the ATU.
	 */
	preg = bus_space_read_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_COMMAND_STATUS_REG);
	preg |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE |
	    PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE;
	bus_space_write_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_COMMAND_STATUS_REG, preg);

	/* Initialize the bus space tags. */
	i80321_io_bs_init(&sc->sc_pci_iot, sc);
	i80321_mem_bs_init(&sc->sc_pci_memt, sc);

	/* Initialize the DMA tags. */
	i80321_pci_dma_init(sc);
	i80321_local_dma_init(sc);

	/*
	 * Attach all the IOP built-ins.
	 */
	for (id = iopxs_devices; id->id_name != NULL; id++) {
		ia.ia_name = id->id_name;
		ia.ia_st = sc->sc_st;
		ia.ia_sh = sc->sc_sh;
		ia.ia_dmat = &sc->sc_local_dmat;
		ia.ia_offset = id->id_offset;
		ia.ia_size = id->id_size;

		config_found(&sc->sc_dev, &ia, i80321_iopxs_print);
	}

	/*
	 * Attach the PCI bus.
	 */
	preg = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	preg = PCIXSR_BUSNO(preg);
	if (preg == 0xff)
		preg = 0;
	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_pci_iot;
	pba.pba_memt = &sc->sc_pci_memt;
	pba.pba_dmat = &sc->sc_pci_dmat;
	pba.pba_pc = &sc->sc_pci_chipset;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = preg;

	config_found((struct device *)sc, &pba, i80321_iopxs_print);
}

/*
 * i80321_iopxs_print:
 *
 *	Autoconfiguration cfprint routine when attaching
 *	to the "iopxs" device.
 */
static int
i80321_iopxs_print(void *aux, const char *pnp)
{

	return (QUIET);
}

/*
 * i80321_pci_dma_init:
 *
 *	Initialize the PCI DMA tag.
 */
static void
i80321_pci_dma_init(struct i80321_softc *sc)
{
	bus_dma_tag_t dmat = &sc->sc_pci_dmat;
	struct arm32_dma_range *dr = &sc->sc_pci_dma_range;

	dr->dr_sysbase = sc->sc_iwin[2].iwin_xlate;
	dr->dr_busbase = PCI_MAPREG_MEM_ADDR(sc->sc_iwin[2].iwin_base_lo);
	dr->dr_len = sc->sc_iwin[2].iwin_size;

	dmat->_ranges = dr;
	dmat->_nranges = 1;

	dmat->_dmamap_create = _bus_dmamap_create;
	dmat->_dmamap_destroy = _bus_dmamap_destroy;
	dmat->_dmamap_load = _bus_dmamap_load;
	dmat->_dmamap_load_mbuf = _bus_dmamap_load_mbuf;
	dmat->_dmamap_load_uio = _bus_dmamap_load_uio;
	dmat->_dmamap_load_raw = _bus_dmamap_load_raw;
	dmat->_dmamap_unload = _bus_dmamap_unload;
	dmat->_dmamap_sync = _bus_dmamap_sync;

	dmat->_dmamem_alloc = _bus_dmamem_alloc;
	dmat->_dmamem_free = _bus_dmamem_free;
	dmat->_dmamem_map = _bus_dmamem_map;
	dmat->_dmamem_unmap = _bus_dmamem_unmap;
	dmat->_dmamem_mmap = _bus_dmamem_mmap;
}

/*
 * i80321_local_dma_init:
 *
 *	Initialize the local DMA tag.
 */
void
i80321_local_dma_init(struct i80321_softc *sc)
{
	bus_dma_tag_t dmat = &sc->sc_local_dmat;

	dmat->_ranges = NULL;
	dmat->_nranges = 0;

	dmat->_dmamap_create = _bus_dmamap_create;
	dmat->_dmamap_destroy = _bus_dmamap_destroy;
	dmat->_dmamap_load = _bus_dmamap_load;
	dmat->_dmamap_load_mbuf = _bus_dmamap_load_mbuf;
	dmat->_dmamap_load_uio = _bus_dmamap_load_uio;
	dmat->_dmamap_load_raw = _bus_dmamap_load_raw;
	dmat->_dmamap_unload = _bus_dmamap_unload;
	dmat->_dmamap_sync = _bus_dmamap_sync;

	dmat->_dmamem_alloc = _bus_dmamem_alloc;
	dmat->_dmamem_free = _bus_dmamem_free;
	dmat->_dmamem_map = _bus_dmamem_map;
	dmat->_dmamem_unmap = _bus_dmamem_unmap;
	dmat->_dmamem_mmap = _bus_dmamem_mmap;
}

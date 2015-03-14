/*	$OpenBSD: if_lmc_obsd.c,v 1.25 2015/03/14 03:38:48 jsg Exp $ */
/*	$NetBSD: if_lmc_nbsd.c,v 1.1 1999/03/25 03:32:43 explorer Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include <net/if_sppp.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>

#include <dev/pci/if_lmc_types.h>
#include <dev/pci/if_lmcioctl.h>
#include <dev/pci/if_lmcvar.h>

/*
 * This file is INCLUDED (gross, I know, but...)
 */

static int lmc_busdma_init(lmc_softc_t * const sc);
static int lmc_busdma_allocmem(lmc_softc_t * const sc, size_t size,
	bus_dmamap_t *map_p, lmc_desc_t **desc_p);

static int
lmc_pci_probe(struct device *parent,
	       void *match,
	       void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	u_int32_t id;

	/*
	 * check first for the DEC chip we expect to find.  We expect
	 * 21140A, pass 2.2 or higher.
	 */
	if (PCI_VENDORID(pa->pa_id) != PCI_VENDOR_DEC)
		return 0;
	if (PCI_CHIPID(pa->pa_id) != PCI_PRODUCT_DEC_21140)
		return 0;
	id = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFRV) & 0xff;
	if (id < 0x22)
		return 0;

	/*
	 * Next, check the subsystem ID and see if it matches what we
	 * expect.
	 */
	id = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SSID);
	if (PCI_VENDORID(id) != PCI_VENDOR_LMC)
		return 0;
	if ((PCI_CHIPID(id) != PCI_PRODUCT_LMC_HSSI)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_HSSIC)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_DS3)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_SSI)
	    && (PCI_CHIPID(id) != PCI_PRODUCT_LMC_DS1))
		return 0;

	return 20; /* must be > than any other tulip driver */
}

static void  lmc_pci_attach(struct device * const parent,
			     struct device * const self, void * const aux);

struct cfattach lmc_ca = {
    sizeof(lmc_softc_t), lmc_pci_probe, lmc_pci_attach
};

struct cfdriver lmc_cd = {
	NULL, "lmc", DV_IFNET
};

static void
lmc_pci_attach(struct device * const parent,
		struct device * const self, void * const aux)
{
	u_int32_t revinfo, cfdainfo, id, ssid;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	unsigned csroffset = LMC_PCI_CSROFFSET;
	unsigned csrsize = LMC_PCI_CSRSIZE;
	lmc_csrptr_t csr_base;
	lmc_spl_t s;
	lmc_intrfunc_t (*intr_rtn)(void *) = lmc_intr_normal;
	lmc_softc_t * const sc = (lmc_softc_t *) self;
	struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
	extern lmc_media_t lmc_hssi_media;
	extern lmc_media_t lmc_ds3_media;
	extern lmc_media_t lmc_t1_media;
	extern lmc_media_t lmc_ssi_media;

	revinfo  = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFRV) & 0xFF;
	id       = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFID);
	cfdainfo = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CFDA);
	ssid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SSID);

	switch (PCI_CHIPID(ssid)) {
	case PCI_PRODUCT_LMC_HSSI:
		printf(": HSSI\n");
		sc->lmc_media = &lmc_hssi_media;
		break;
	case PCI_PRODUCT_LMC_HSSIC:
		printf(": HSSIc\n");
		sc->lmc_media = &lmc_hssi_media;
		break;
	case PCI_PRODUCT_LMC_DS3:
		printf(": DS3\n");
		sc->lmc_media = &lmc_ds3_media;
		break;
	case PCI_PRODUCT_LMC_SSI:
		printf(": SSI\n");
		sc->lmc_media = &lmc_ssi_media;
		break;
	case PCI_PRODUCT_LMC_DS1:
		printf(": T1\n");
		sc->lmc_media = &lmc_t1_media;
		break;
	}

        sc->lmc_pci_busno = parent;
        sc->lmc_pci_devno = pa->pa_device;

	sc->lmc_chipid = LMC_21140A;
	sc->lmc_features |= LMC_HAVE_STOREFWD;
	if (sc->lmc_chipid == LMC_21140A && revinfo <= 0x22)
		sc->lmc_features |= LMC_HAVE_RXBADOVRFLW;

	if (cfdainfo & (TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE)) {
		cfdainfo &= ~(TULIP_CFDA_SLEEP | TULIP_CFDA_SNOOZE);
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_CFDA, cfdainfo);
		DELAY(11 * 1000);
	}

	bcopy(self->dv_xname, sc->lmc_if.if_xname, IFNAMSIZ);
	sc->lmc_if.if_softc = sc;
	sc->lmc_pc = pa->pa_pc;

	sc->lmc_revinfo = revinfo;
	sc->lmc_if.if_softc = sc;

	csr_base = 0;
	{
		bus_space_tag_t iot, memt;
		bus_space_handle_t ioh, memh;
		int ioh_valid, memh_valid;

		ioh_valid = (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO,
		    0, &iot, &ioh, NULL, NULL, 0) == 0);
		memh_valid = (pci_mapreg_map(pa, PCI_CBMA,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt,
		    &memh, NULL, NULL, 0) == 0);

		if (memh_valid) {
			sc->lmc_bustag = memt;
			sc->lmc_bushandle = memh;
		} else if (ioh_valid) {
			sc->lmc_bustag = iot;
			sc->lmc_bushandle = ioh;
		} else {
			printf("%s: unable to map device registers\n",
			       sc->lmc_dev.dv_xname);
			return;
		}
	}

	sc->lmc_dmatag = pa->pa_dmat;
	if ((lmc_busdma_init(sc)) != 0) {
		printf("error initing bus_dma\n");
		return;
	}

	lmc_initcsrs(sc, csr_base + csroffset, csrsize);
	lmc_initring(sc, &sc->lmc_rxinfo, sc->lmc_rxdescs,
		       LMC_RXDESCS);
	lmc_initring(sc, &sc->lmc_txinfo, sc->lmc_txdescs,
		       LMC_TXDESCS);

	lmc_gpio_mkinput(sc, 0xff);
	sc->lmc_gpio = 0;  /* drive no signals yet */

	sc->lmc_media->defaults(sc);

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN); /* down */

	/*
	 * Make sure there won't be any interrupts or such...
	 */
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);

	/*
	 * Wait 10 microseconds (actually 50 PCI cycles but at 
	 * 33MHz that comes to two microseconds but wait a
	 * bit longer anyways)
	 */
	DELAY(100);

	lmc_read_macaddr(sc);

        if (pci_intr_map(pa, &intrhandle)) {
		 printf("%s: couldn't map interrupt\n",
			sc->lmc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle);

	sc->lmc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
						intr_rtn, sc, self->dv_xname);

	if (sc->lmc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		       sc->lmc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

        printf("%s: pass %d.%d, serial " LMC_EADDR_FMT ", %s\n",
               sc->lmc_dev.dv_xname,
               (sc->lmc_revinfo & 0xF0) >> 4, sc->lmc_revinfo & 0x0F,
               LMC_EADDR_ARGS(sc->lmc_enaddr), intrstr);

	s = LMC_RAISESPL();
	lmc_dec_reset(sc);
	lmc_reset(sc);
	lmc_attach(sc);
	LMC_RESTORESPL(s);
}

static int
lmc_busdma_allocmem(
    lmc_softc_t * const sc,
    size_t size,
    bus_dmamap_t *map_p,
    lmc_desc_t **desc_p)
{
    bus_dma_segment_t segs[1];
    int nsegs, error;
    error = bus_dmamem_alloc(sc->lmc_dmatag, size, 1, NBPG,
			     segs, nitems(segs), &nsegs, BUS_DMA_NOWAIT);
    if (error == 0) {
	void *desc;
	error = bus_dmamem_map(sc->lmc_dmatag, segs, nsegs, size,
			       (void *) &desc, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error == 0) {
	    bus_dmamap_t map;
	    error = bus_dmamap_create(sc->lmc_dmatag, size, 1, size, 0,
				      BUS_DMA_NOWAIT, &map);
	    if (error == 0) {
		error = bus_dmamap_load(sc->lmc_dmatag, map, desc,
					size, NULL, BUS_DMA_NOWAIT);
		if (error)
		    bus_dmamap_destroy(sc->lmc_dmatag, map);
		else
		    *map_p = map;
	    }
	    if (error)
		bus_dmamem_unmap(sc->lmc_dmatag, desc, size);
	}
	if (error)
	    bus_dmamem_free(sc->lmc_dmatag, segs, nsegs);
	else
	    *desc_p = desc;
    }
    return error;
}

static int
lmc_busdma_init(
    lmc_softc_t * const sc)
{
    int error = 0;

    /*
     * Allocate space and dmamap for transmit ring
     */
    if (error == 0) {
	error = lmc_busdma_allocmem(sc, sizeof(lmc_desc_t) * LMC_TXDESCS,
				      &sc->lmc_txdescmap,
				      &sc->lmc_txdescs);
    }

    /*
     * Allocate dmamaps for each transmit descriptors
     */
    if (error == 0) {
	while (error == 0 && sc->lmc_txmaps_free < LMC_TXDESCS) {
	    bus_dmamap_t map;
	    if ((error = LMC_TXMAP_CREATE(sc, &map)) == 0)
		sc->lmc_txmaps[sc->lmc_txmaps_free++] = map;
	}
	if (error) {
	    while (sc->lmc_txmaps_free > 0) 
		bus_dmamap_destroy(sc->lmc_dmatag,
				   sc->lmc_txmaps[--sc->lmc_txmaps_free]);
	}
    }

    /*
     * Allocate space and dmamap for receive ring
     */
    if (error == 0) {
	error = lmc_busdma_allocmem(sc, sizeof(lmc_desc_t) * LMC_RXDESCS,
				      &sc->lmc_rxdescmap,
				      &sc->lmc_rxdescs);
    }

    /*
     * Allocate dmamaps for each receive descriptors
     */
    if (error == 0) {
	while (error == 0 && sc->lmc_rxmaps_free < LMC_RXDESCS) {
	    bus_dmamap_t map;
	    if ((error = LMC_RXMAP_CREATE(sc, &map)) == 0)
		sc->lmc_rxmaps[sc->lmc_rxmaps_free++] = map;
	}
	if (error) {
	    while (sc->lmc_rxmaps_free > 0) 
		bus_dmamap_destroy(sc->lmc_dmatag,
				   sc->lmc_rxmaps[--sc->lmc_rxmaps_free]);
	}
    }

    return error;
}

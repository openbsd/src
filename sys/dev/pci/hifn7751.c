/*	$OpenBSD: hifn7751.c,v 1.178 2018/04/28 15:44:59 jasper Exp $	*/

/*
 * Invertex AEON / Hifn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000-2001 Network Security Technologies, Inc.
 *			http://www.netsec.net
 * Copyright (c) 2003 Hifn Inc.

 * This driver is based on a previous driver by Invertex, for which they
 * requested:  Please send any comments, feedback, bug-fixes, or feature
 * requests to software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

/*
 * Driver for various Hifn encryption processors.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <crypto/cryptodev.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/hifn7751reg.h>
#include <dev/pci/hifn7751var.h>

#undef HIFN_DEBUG

/*
 * Prototypes and count for the pci_device structure
 */
int hifn_probe(struct device *, void *, void *);
void hifn_attach(struct device *, struct device *, void *);

struct cfattach hifn_ca = {
	sizeof(struct hifn_softc), hifn_probe, hifn_attach,
};

struct cfdriver hifn_cd = {
	0, "hifn", DV_DULL
};

void	hifn_reset_board(struct hifn_softc *, int);
void	hifn_reset_puc(struct hifn_softc *);
void	hifn_puc_wait(struct hifn_softc *);
int	hifn_enable_crypto(struct hifn_softc *, pcireg_t);
void	hifn_set_retry(struct hifn_softc *);
void	hifn_init_dma(struct hifn_softc *);
void	hifn_init_pci_registers(struct hifn_softc *);
int	hifn_sramsize(struct hifn_softc *);
int	hifn_dramsize(struct hifn_softc *);
int	hifn_ramtype(struct hifn_softc *);
void	hifn_sessions(struct hifn_softc *);
int	hifn_intr(void *);
u_int	hifn_write_command(struct hifn_command *, u_int8_t *);
u_int32_t hifn_next_signature(u_int32_t a, u_int cnt);
int	hifn_newsession(u_int32_t *, struct cryptoini *);
int	hifn_freesession(u_int64_t);
int	hifn_process(struct cryptop *);
void	hifn_callback(struct hifn_softc *, struct hifn_command *, u_int8_t *);
int	hifn_crypto(struct hifn_softc *, struct hifn_command *,
    struct cryptop *);
int	hifn_readramaddr(struct hifn_softc *, int, u_int8_t *);
int	hifn_writeramaddr(struct hifn_softc *, int, u_int8_t *);
int	hifn_dmamap_aligned(bus_dmamap_t);
int	hifn_dmamap_load_src(struct hifn_softc *, struct hifn_command *);
int	hifn_dmamap_load_dst(struct hifn_softc *, struct hifn_command *);
int	hifn_init_pubrng(struct hifn_softc *);
void	hifn_rng(void *);
void	hifn_tick(void *);
void	hifn_abort(struct hifn_softc *);
void	hifn_alloc_slot(struct hifn_softc *, int *, int *, int *, int *);
void	hifn_write_4(struct hifn_softc *, int, bus_size_t, u_int32_t);
u_int32_t hifn_read_4(struct hifn_softc *, int, bus_size_t);
int	hifn_compression(struct hifn_softc *, struct cryptop *,
    struct hifn_command *);
struct mbuf *hifn_mkmbuf_chain(int, struct mbuf *);
int	hifn_compress_enter(struct hifn_softc *, struct hifn_command *);
void	hifn_callback_comp(struct hifn_softc *, struct hifn_command *,
    u_int8_t *);

struct hifn_stats hifnstats;

const struct pci_matchid hifn_devices[] = {
	{ PCI_VENDOR_INVERTEX, PCI_PRODUCT_INVERTEX_AEON },
	{ PCI_VENDOR_HIFN, PCI_PRODUCT_HIFN_7751 },
	{ PCI_VENDOR_HIFN, PCI_PRODUCT_HIFN_7811 },
	{ PCI_VENDOR_HIFN, PCI_PRODUCT_HIFN_7951 },
	{ PCI_VENDOR_HIFN, PCI_PRODUCT_HIFN_7955 },
	{ PCI_VENDOR_HIFN, PCI_PRODUCT_HIFN_7956 },
	{ PCI_VENDOR_NETSEC, PCI_PRODUCT_NETSEC_7751 },
};

int
hifn_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, hifn_devices,
	    nitems(hifn_devices)));
}

void 
hifn_attach(struct device *parent, struct device *self, void *aux)
{
	struct hifn_softc *sc = (struct hifn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	char rbase;
	bus_size_t iosize0, iosize1;
	u_int16_t ena;
	int rseg;
	caddr_t kva;
	int algs[CRYPTO_ALGORITHM_MAX + 1];

	sc->sc_pci_pc = pa->pa_pc;
	sc->sc_pci_tag = pa->pa_tag;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7951))
		sc->sc_flags = HIFN_HAS_RNG | HIFN_HAS_PUBLIC;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7955 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7956))
		sc->sc_flags = HIFN_IS_7956 | HIFN_HAS_AES | HIFN_HAS_RNG |
		    HIFN_HAS_PUBLIC;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7811)
		sc->sc_flags |= HIFN_IS_7811 | HIFN_HAS_RNG | HIFN_HAS_LEDS |
		    HIFN_NO_BURSTWRITE;

	if (pci_mapreg_map(pa, HIFN_BAR0, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st0, &sc->sc_sh0, NULL, &iosize0, 0)) {
		printf(": can't find mem space %d\n", 0);
		return;
	}

	if (pci_mapreg_map(pa, HIFN_BAR1, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st1, &sc->sc_sh1, NULL, &iosize1, 0)) {
		printf(": can't find mem space %d\n", 1);
		goto fail_io0;
	}

	hifn_set_retry(sc);

	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		sc->sc_waw_lastgroup = -1;
		sc->sc_waw_lastreg = 1;
	}

	sc->sc_dmat = pa->pa_dmat;
	if (bus_dmamap_create(sc->sc_dmat, sizeof(*sc->sc_dma), 1,
	    sizeof(*sc->sc_dma), 0, BUS_DMA_NOWAIT, &sc->sc_dmamap)) {
		printf(": can't create dma map\n");
		goto fail_io1;
	}
	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(*sc->sc_dma), PAGE_SIZE, 0,
	    sc->sc_dmasegs, 1, &sc->sc_dmansegs,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO)) {
		printf(": can't alloc dma buffer\n");
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
		goto fail_io1;
	}
	if (bus_dmamem_map(sc->sc_dmat, sc->sc_dmasegs, sc->sc_dmansegs,
	    sizeof(*sc->sc_dma), &kva, BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%lu bytes)\n",
		    (u_long)sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, sc->sc_dmasegs, sc->sc_dmansegs);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
		goto fail_io1;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, kva,
	    sizeof(*sc->sc_dma), NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load dma map\n");
		bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, sc->sc_dmasegs, sc->sc_dmansegs);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
		goto fail_io1;
	}
	sc->sc_dma = (struct hifn_dma *)kva;

	hifn_reset_board(sc, 0);

	if (hifn_enable_crypto(sc, pa->pa_id) != 0) {
		printf("%s: crypto enabling failed\n", sc->sc_dv.dv_xname);
		goto fail_mem;
	}
	hifn_reset_puc(sc);

	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	if (sc->sc_flags & HIFN_IS_7956)
		sc->sc_drammodel = 1;
	else if (hifn_ramtype(sc))
		goto fail_mem;

	if (sc->sc_drammodel == 0)
		hifn_sramsize(sc);
	else
		hifn_dramsize(sc);

	/*
	 * Workaround for NetSec 7751 rev A: half ram size because two
	 * of the address lines were left floating
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NETSEC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NETSEC_7751 &&
	    PCI_REVISION(pa->pa_class) == 0x61)
		sc->sc_ramsize >>= 1;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail_mem;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, hifn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_mem;
	}

	hifn_sessions(sc);

	rseg = sc->sc_ramsize / 1024;
	rbase = 'K';
	if (sc->sc_ramsize >= (1024 * 1024)) {
		rbase = 'M';
		rseg /= 1024;
	}
	printf("%d%cB %cram, %s\n", rseg, rbase,
	    sc->sc_drammodel ? 'd' : 's', intrstr);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0)
		goto fail_intr;

	WRITE_REG_0(sc, HIFN_0_PUCNFG,
	    READ_REG_0(sc, HIFN_0_PUCNFG) | HIFN_PUCNFG_CHIPID);
	ena = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	bzero(algs, sizeof(algs));

	algs[CRYPTO_LZS_COMP] = CRYPTO_ALG_FLAG_SUPPORTED;
	switch (ena) {
	case HIFN_PUSTAT_ENA_2:
		algs[CRYPTO_3DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
		/*FALLTHROUGH*/
	case HIFN_PUSTAT_ENA_1:
		algs[CRYPTO_MD5_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	}
	if (sc->sc_flags & HIFN_HAS_AES)
		algs[CRYPTO_AES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;

	crypto_register(sc->sc_cid, algs, hifn_newsession,
	    hifn_freesession, hifn_process);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
	    sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (sc->sc_flags & (HIFN_HAS_PUBLIC | HIFN_HAS_RNG))
		hifn_init_pubrng(sc);

	timeout_set(&sc->sc_tickto, hifn_tick, sc);
	timeout_add_sec(&sc->sc_tickto, 1);

	return;

fail_intr:
	pci_intr_disestablish(pc, sc->sc_ih);
fail_mem:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
	bus_dmamem_free(sc->sc_dmat, sc->sc_dmasegs, sc->sc_dmansegs);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);

	/* Turn off DMA polling */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

fail_io1:
	bus_space_unmap(sc->sc_st1, sc->sc_sh1, iosize1);
fail_io0:
	bus_space_unmap(sc->sc_st0, sc->sc_sh0, iosize0);
}

int
hifn_init_pubrng(struct hifn_softc *sc)
{
	u_int32_t r;
	int i;

	if ((sc->sc_flags & HIFN_IS_7811) == 0) {
		/* Reset 7951 public key/rng engine */
		WRITE_REG_1(sc, HIFN_1_PUB_RESET,
		    READ_REG_1(sc, HIFN_1_PUB_RESET) | HIFN_PUBRST_RESET);

		for (i = 0; i < 100; i++) {
			DELAY(1000);
			if ((READ_REG_1(sc, HIFN_1_PUB_RESET) &
			    HIFN_PUBRST_RESET) == 0)
				break;
		}

		if (i == 100) {
			printf("%s: public key init failed\n",
			    sc->sc_dv.dv_xname);
			return (1);
		}
	}

	/* Enable the rng, if available */
	if (sc->sc_flags & HIFN_HAS_RNG) {
		if (sc->sc_flags & HIFN_IS_7811) {
			r = READ_REG_1(sc, HIFN_1_7811_RNGENA);
			if (r & HIFN_7811_RNGENA_ENA) {
				r &= ~HIFN_7811_RNGENA_ENA;
				WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
			}
			WRITE_REG_1(sc, HIFN_1_7811_RNGCFG,
			    HIFN_7811_RNGCFG_DEFL);
			r |= HIFN_7811_RNGENA_ENA;
			WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
		} else
			WRITE_REG_1(sc, HIFN_1_RNG_CONFIG,
			    READ_REG_1(sc, HIFN_1_RNG_CONFIG) |
			    HIFN_RNGCFG_ENA);

		sc->sc_rngfirst = 1;
		if (hz >= 100)
			sc->sc_rnghz = hz / 100;
		else
			sc->sc_rnghz = 1;
		timeout_set(&sc->sc_rngto, hifn_rng, sc);
		timeout_add(&sc->sc_rngto, sc->sc_rnghz);
	}

	/* Enable public key engine, if available */
	if (sc->sc_flags & HIFN_HAS_PUBLIC) {
		WRITE_REG_1(sc, HIFN_1_PUB_IEN, HIFN_PUBIEN_DONE);
		sc->sc_dmaier |= HIFN_DMAIER_PUBDONE;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	return (0);
}

void
hifn_rng(void *vsc)
{
	struct hifn_softc *sc = vsc;
	u_int32_t num1, sts, num2;
	int i;

	if (sc->sc_flags & HIFN_IS_7811) {
		for (i = 0; i < 5; i++) {
			sts = READ_REG_1(sc, HIFN_1_7811_RNGSTS);
			if (sts & HIFN_7811_RNGSTS_UFL) {
				printf("%s: RNG underflow: disabling\n",
				    sc->sc_dv.dv_xname);
				return;
			}
			if ((sts & HIFN_7811_RNGSTS_RDY) == 0)
				break;

			/*
			 * There are at least two words in the RNG FIFO
			 * at this point.
			 */
			num1 = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			num2 = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			if (sc->sc_rngfirst)
				sc->sc_rngfirst = 0;
			else {
				enqueue_randomness(num1);
				enqueue_randomness(num2);
			}
		}
	} else {
		num1 = READ_REG_1(sc, HIFN_1_RNG_DATA);

		if (sc->sc_rngfirst)
			sc->sc_rngfirst = 0;
		else
			enqueue_randomness(num1);
	}

	timeout_add(&sc->sc_rngto, sc->sc_rnghz);
}

void
hifn_puc_wait(struct hifn_softc *sc)
{
	int i;

	for (i = 5000; i > 0; i--) {
		DELAY(1);
		if (!(READ_REG_0(sc, HIFN_0_PUCTRL) & HIFN_PUCTRL_RESET))
			break;
	}
	if (!i)
		printf("%s: proc unit did not reset\n", sc->sc_dv.dv_xname);
}

/*
 * Reset the processing unit.
 */
void
hifn_reset_puc(struct hifn_softc *sc)
{
	/* Reset processing unit */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	hifn_puc_wait(sc);
}

void
hifn_set_retry(struct hifn_softc *sc)
{
	u_int32_t r;

	r = pci_conf_read(sc->sc_pci_pc, sc->sc_pci_tag, HIFN_TRDY_TIMEOUT);
	r &= 0xffff0000;
	pci_conf_write(sc->sc_pci_pc, sc->sc_pci_tag, HIFN_TRDY_TIMEOUT, r);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
void
hifn_reset_board(struct hifn_softc *sc, int full)
{
	u_int32_t reg;

	/*
	 * Set polling in the DMA configuration register to zero.  0x7 avoids
	 * resetting the board and zeros out the other fields.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	/*
	 * Now that polling has been disabled, we have to wait 1 ms
	 * before resetting the board.
	 */
	DELAY(1000);

	/* Reset the DMA unit */
	if (full) {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MODE);
		DELAY(1000);
	} else {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG,
		    HIFN_DMACNFG_MODE | HIFN_DMACNFG_MSTRESET);
		hifn_reset_puc(sc);
	}

	bzero(sc->sc_dma, sizeof(*sc->sc_dma));

	/* Bring dma unit out of reset */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	hifn_puc_wait(sc);

	hifn_set_retry(sc);

	if (sc->sc_flags & HIFN_IS_7811) {
		for (reg = 0; reg < 1000; reg++) {
			if (READ_REG_1(sc, HIFN_1_7811_MIPSRST) &
			    HIFN_MIPSRST_CRAMINIT)
				break;
			DELAY(1000);
		}
		if (reg == 1000)
			printf(": cram init timeout\n");
	}
}

u_int32_t
hifn_next_signature(u_int32_t a, u_int cnt)
{
	int i;
	u_int32_t v;

	for (i = 0; i < cnt; i++) {

		/* get the parity */
		v = a & 0x80080125;
		v ^= v >> 16;
		v ^= v >> 8;
		v ^= v >> 4;
		v ^= v >> 2;
		v ^= v >> 1;

		a = (v & 1) ^ (a << 1);
	}

	return a;
}

struct pci2id {
	u_short		pci_vendor;
	u_short		pci_prod;
	char		card_id[13];
} pci2id[] = {
	{
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7951,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7955,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7956,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_NETSEC,
		PCI_PRODUCT_NETSEC_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_INVERTEX,
		PCI_PRODUCT_INVERTEX_AEON,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7811,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		/*
		 * Other vendors share this PCI ID as well, such as
		 * powercrypt, and obviously they also
		 * use the same key.
		 */
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	},
};

/*
 * Checks to see if crypto is already enabled.  If crypto isn't enable,
 * "hifn_enable_crypto" is called to enable it.  The check is important,
 * as enabling crypto twice will lock the board.
 */
int 
hifn_enable_crypto(struct hifn_softc *sc, pcireg_t pciid)
{
	u_int32_t dmacfg, ramcfg, encl, addr, i;
	char *offtbl = NULL;

	for (i = 0; i < nitems(pci2id); i++) {
		if (pci2id[i].pci_vendor == PCI_VENDOR(pciid) &&
		    pci2id[i].pci_prod == PCI_PRODUCT(pciid)) {
			offtbl = pci2id[i].card_id;
			break;
		}
	}

	if (offtbl == NULL) {
#ifdef HIFN_DEBUG
		printf(": Unknown card!\n");
#endif
		return (1);
	}

	ramcfg = READ_REG_0(sc, HIFN_0_PUCNFG);
	dmacfg = READ_REG_1(sc, HIFN_1_DMA_CNFG);

	/*
	 * The RAM config register's encrypt level bit needs to be set before
	 * every read performed on the encryption level register.
	 */
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);

	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	/*
	 * Make sure we don't re-unlock.  Two unlocks kills chip until the
	 * next reboot.
	 */
	if (encl == HIFN_PUSTAT_ENA_1 || encl == HIFN_PUSTAT_ENA_2) {
#ifdef HIFN_DEBUG
		printf(": Strong Crypto already enabled!\n");
#endif
		goto report;
	}

	if (encl != 0 && encl != HIFN_PUSTAT_ENA_0) {
#ifdef HIFN_DEBUG
		printf(": Unknown encryption level\n");
#endif
		return 1;
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_UNLOCK |
	    HIFN_DMACNFG_MSTRESET | HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
	DELAY(1000);
	addr = READ_REG_1(sc, HIFN_1_UNLOCK_SECRET1);
	DELAY(1000);
	WRITE_REG_1(sc, HIFN_1_UNLOCK_SECRET2, 0);
	DELAY(1000);

	for (i = 0; i <= 12; i++) {
		addr = hifn_next_signature(addr, offtbl[i] + 0x101);
		WRITE_REG_1(sc, HIFN_1_UNLOCK_SECRET2, addr);

		DELAY(1000);
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);
	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

#ifdef HIFN_DEBUG
	if (encl != HIFN_PUSTAT_ENA_1 && encl != HIFN_PUSTAT_ENA_2)
		printf(": engine is permanently locked until next system reset");
	else
		printf(": engine enabled successfully!");
#endif

report:
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, dmacfg);

	switch (encl) {
	case HIFN_PUSTAT_ENA_0:
		offtbl = "LZS";
		break;
	case HIFN_PUSTAT_ENA_1:
		offtbl = "LZS DES";
		break;
	case HIFN_PUSTAT_ENA_2:
		offtbl = "LZS 3DES ARC4 MD5 SHA1";
		break;
	default:
		offtbl = "disabled";
		break;
	}
	printf(": %s", offtbl);
	if (sc->sc_flags & HIFN_HAS_RNG)
		printf(" RNG");
	if (sc->sc_flags & HIFN_HAS_AES)
		printf(" AES");
	if (sc->sc_flags & HIFN_HAS_PUBLIC)
		printf(" PK");
	printf(", ");

	return (0);
}

/*
 * Give initial values to the registers listed in the "Register Space"
 * section of the HIFN Software Development reference manual.
 */
void 
hifn_init_pci_registers(struct hifn_softc *sc)
{
	/* write fixed values needed by the Initialization registers */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	WRITE_REG_0(sc, HIFN_0_FIFOCNFG, HIFN_FIFOCNFG_THRESHOLD);
	WRITE_REG_0(sc, HIFN_0_PUIER, HIFN_PUIER_DSTOVER);

	/* write all 4 ring address registers */
	WRITE_REG_1(sc, HIFN_1_DMA_CRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, cmdr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_SRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, srcr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_DRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, dstr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_RRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, resr[0]));

	DELAY(2000);

	/* write status register */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS |
	    HIFN_DMACSR_S_CTRL_DIS | HIFN_DMACSR_C_CTRL_DIS |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_D_DONE | HIFN_DMACSR_D_LAST |
	    HIFN_DMACSR_D_WAIT | HIFN_DMACSR_D_OVER |
	    HIFN_DMACSR_R_ABORT | HIFN_DMACSR_R_DONE | HIFN_DMACSR_R_LAST |
	    HIFN_DMACSR_R_WAIT | HIFN_DMACSR_R_OVER |
	    HIFN_DMACSR_S_ABORT | HIFN_DMACSR_S_DONE | HIFN_DMACSR_S_LAST |
	    HIFN_DMACSR_S_WAIT |
	    HIFN_DMACSR_C_ABORT | HIFN_DMACSR_C_DONE | HIFN_DMACSR_C_LAST |
	    HIFN_DMACSR_C_WAIT |
	    HIFN_DMACSR_ENGINE |
	    ((sc->sc_flags & HIFN_HAS_PUBLIC) ?
		HIFN_DMACSR_PUBDONE : 0) |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMACSR_ILLW | HIFN_DMACSR_ILLR : 0));

	sc->sc_d_busy = sc->sc_r_busy = sc->sc_s_busy = sc->sc_c_busy = 0;
	sc->sc_dmaier |= HIFN_DMAIER_R_DONE | HIFN_DMAIER_C_ABORT |
	    HIFN_DMAIER_D_OVER | HIFN_DMAIER_R_OVER |
	    HIFN_DMAIER_S_ABORT | HIFN_DMAIER_D_ABORT | HIFN_DMAIER_R_ABORT |
	    HIFN_DMAIER_ENGINE |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMAIER_ILLW | HIFN_DMAIER_ILLR : 0);
	sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
	WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	CLR_LED(sc, HIFN_MIPSRST_LED0 | HIFN_MIPSRST_LED1 | HIFN_MIPSRST_LED2);

	if (sc->sc_flags & HIFN_IS_7956) {
		WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
		    HIFN_PUCNFG_TCALLPHASES |
		    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32);
		WRITE_REG_1(sc, HIFN_1_PLL, HIFN_PLL_7956);
	} else {
		WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
		    HIFN_PUCNFG_DRFR_128 | HIFN_PUCNFG_TCALLPHASES |
		    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32 |
		    (sc->sc_drammodel ? HIFN_PUCNFG_DRAM : HIFN_PUCNFG_SRAM));
	}

	WRITE_REG_0(sc, HIFN_0_PUISR, HIFN_PUISR_DSTOVER);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE | HIFN_DMACNFG_LAST |
	    ((HIFN_POLL_FREQUENCY << 16 ) & HIFN_DMACNFG_POLLFREQ) |
	    ((HIFN_POLL_SCALAR << 8) & HIFN_DMACNFG_POLLINVAL));
}

/*
 * The maximum number of sessions supported by the card
 * is dependent on the amount of context ram, which
 * encryption algorithms are enabled, and how compression
 * is configured.  This should be configured before this
 * routine is called.
 */
void
hifn_sessions(struct hifn_softc *sc)
{
	u_int32_t pucnfg;
	int ctxsize;

	pucnfg = READ_REG_0(sc, HIFN_0_PUCNFG);

	if (pucnfg & HIFN_PUCNFG_COMPSING) {
		if (pucnfg & HIFN_PUCNFG_ENCCNFG)
			ctxsize = 128;
		else
			ctxsize = 512;
		/*
		 * 7955/7956 has internal context memory of 32K
		 */
		if (sc->sc_flags & HIFN_IS_7956)
			sc->sc_maxses = 32768 / ctxsize;
		else
			sc->sc_maxses = 1 +
			    ((sc->sc_ramsize - 32768) / ctxsize);
	}
	else
		sc->sc_maxses = sc->sc_ramsize / 16384;

	if (sc->sc_maxses > 2048)
		sc->sc_maxses = 2048;
}

/*
 * Determine ram type (sram or dram).  Board should be just out of a reset
 * state when this is called.
 */
int
hifn_ramtype(struct hifn_softc *sc)
{
	u_int8_t data[8], dataexpect[8];
	int i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0x55;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (bcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0xaa;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (bcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	return (0);
}

#define	HIFN_SRAM_MAX		(32 << 20)
#define	HIFN_SRAM_STEP_SIZE	16384
#define	HIFN_SRAM_GRANULARITY	(HIFN_SRAM_MAX / HIFN_SRAM_STEP_SIZE)

int
hifn_sramsize(struct hifn_softc *sc)
{
	u_int32_t a;
	u_int8_t data[8];
	u_int8_t dataexpect[sizeof(data)];
	int32_t i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = i ^ 0x5a;

	for (i = HIFN_SRAM_GRANULARITY - 1; i >= 0; i--) {
		a = i * HIFN_SRAM_STEP_SIZE;
		bcopy(&i, data, sizeof(i));
		hifn_writeramaddr(sc, a, data);
	}

	for (i = 0; i < HIFN_SRAM_GRANULARITY; i++) {
		a = i * HIFN_SRAM_STEP_SIZE;
		bcopy(&i, dataexpect, sizeof(i));
		if (hifn_readramaddr(sc, a, data) < 0)
			return (0);
		if (bcmp(data, dataexpect, sizeof(data)) != 0)
			return (0);
		sc->sc_ramsize = a + HIFN_SRAM_STEP_SIZE;
	}

	return (0);
}

/*
 * XXX For dram boards, one should really try all of the
 * HIFN_PUCNFG_DSZ_*'s.  This just assumes that PUCNFG
 * is already set up correctly.
 */
int
hifn_dramsize(struct hifn_softc *sc)
{
	u_int32_t cnfg;

	if (sc->sc_flags & HIFN_IS_7956) {
		/*
		 * 7956/7956 have a fixed internal ram of only 32K.
		 */
		sc->sc_ramsize = 32768;
	} else {
		cnfg = READ_REG_0(sc, HIFN_0_PUCNFG) &
		    HIFN_PUCNFG_DRAMMASK;
		sc->sc_ramsize = 1 << ((cnfg >> 13) + 18);
	}
	return (0);
}

void
hifn_alloc_slot(struct hifn_softc *sc, int *cmdp, int *srcp,
    int *dstp, int *resp)
{
	struct hifn_dma *dma = sc->sc_dma;

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*cmdp = dma->cmdi++;
	dma->cmdk = dma->cmdi;

	if (dma->srci == HIFN_D_SRC_RSIZE) {
		dma->srci = 0;
		dma->srcr[HIFN_D_SRC_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*srcp = dma->srci++;
	dma->srck = dma->srci;

	if (dma->dsti == HIFN_D_DST_RSIZE) {
		dma->dsti = 0;
		dma->dstr[HIFN_D_DST_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, HIFN_D_DST_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*dstp = dma->dsti++;
	dma->dstk = dma->dsti;

	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*resp = dma->resi++;
	dma->resk = dma->resi;
}

int
hifn_writeramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_base_command wc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, resi, srci, dsti;

	wc.masks = htole16(3 << 13);
	wc.session_num = htole16(addr >> 14);
	wc.total_source_count = htole16(8);
	wc.total_dest_count = htole16(addr & 0x3fff);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	/* build write command */
	bzero(dma->command_bufs[cmdi], HIFN_MAX_COMMAND);
	*(struct hifn_base_command *)dma->command_bufs[cmdi] = wc;
	bcopy(data, &dma->test_src, sizeof(dma->test_src));

	dma->srcr[srci].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr
	    + offsetof(struct hifn_dma, test_src));
	dma->dstr[dsti].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr
	    + offsetof(struct hifn_dma, test_dst));

	dma->cmdr[cmdi].l = htole32(16 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(4 | masks);
	dma->resr[resi].l = htole32(4 | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    0, sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		printf("%s: writeramaddr -- "
		    "result[%d](addr %d) still valid\n",
		    sc->sc_dv.dv_xname, resi, addr);

		return (-1);
	} else
		r = 0;

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

int
hifn_readramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_base_command rc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, srci, dsti, resi;

	rc.masks = htole16(2 << 13);
	rc.session_num = htole16(addr >> 14);
	rc.total_source_count = htole16(addr & 0x3fff);
	rc.total_dest_count = htole16(8);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	bzero(dma->command_bufs[cmdi], HIFN_MAX_COMMAND);
	*(struct hifn_base_command *)dma->command_bufs[cmdi] = rc;

	dma->srcr[srci].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, test_src));
	dma->test_src = 0;
	dma->dstr[dsti].p =  htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, test_dst));
	dma->test_dst = 0;
	dma->cmdr[cmdi].l = htole32(8 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(8 | masks);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    0, sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		printf("%s: readramaddr -- "
		    "result[%d](addr %d) still valid\n",
		    sc->sc_dv.dv_xname, resi, addr);
		r = -1;
	} else {
		r = 0;
		bcopy(&dma->test_dst, data, sizeof(dma->test_dst));
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

/*
 * Initialize the descriptor rings.
 */
void 
hifn_init_dma(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	int i;

	hifn_set_retry(sc);

	/* initialize static pointer values */
	for (i = 0; i < HIFN_D_CMD_RSIZE; i++)
		dma->cmdr[i].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, command_bufs[i][0]));
	for (i = 0; i < HIFN_D_RES_RSIZE; i++)
		dma->resr[i].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, result_bufs[i][0]));

	dma->cmdr[HIFN_D_CMD_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, cmdr[0]));
	dma->srcr[HIFN_D_SRC_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, srcr[0]));
	dma->dstr[HIFN_D_DST_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, dstr[0]));
	dma->resr[HIFN_D_RES_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, resr[0]));

	dma->cmdu = dma->srcu = dma->dstu = dma->resu = 0;
	dma->cmdi = dma->srci = dma->dsti = dma->resi = 0;
	dma->cmdk = dma->srck = dma->dstk = dma->resk = 0;
}

/*
 * Writes out the raw command buffer space.  Returns the
 * command buffer size.
 */
u_int
hifn_write_command(struct hifn_command *cmd, u_int8_t *buf)
{
	u_int8_t *buf_pos;
	struct hifn_base_command *base_cmd;
	struct hifn_mac_command *mac_cmd;
	struct hifn_crypt_command *cry_cmd;
	struct hifn_comp_command *comp_cmd;
	int using_mac, using_crypt, using_comp, len, ivlen;
	u_int32_t dlen, slen;

	buf_pos = buf;
	using_mac = cmd->base_masks & HIFN_BASE_CMD_MAC;
	using_crypt = cmd->base_masks & HIFN_BASE_CMD_CRYPT;
	using_comp = cmd->base_masks & HIFN_BASE_CMD_COMP;

	base_cmd = (struct hifn_base_command *)buf_pos;
	base_cmd->masks = htole16(cmd->base_masks);
	slen = cmd->src_map->dm_mapsize;
	if (cmd->sloplen)
		dlen = cmd->dst_map->dm_mapsize - cmd->sloplen +
		    sizeof(u_int32_t);
	else
		dlen = cmd->dst_map->dm_mapsize;
	base_cmd->total_source_count = htole16(slen & HIFN_BASE_CMD_LENMASK_LO);
	base_cmd->total_dest_count = htole16(dlen & HIFN_BASE_CMD_LENMASK_LO);
	dlen >>= 16;
	slen >>= 16;
	base_cmd->session_num = htole16(
	    ((slen << HIFN_BASE_CMD_SRCLEN_S) & HIFN_BASE_CMD_SRCLEN_M) |
	    ((dlen << HIFN_BASE_CMD_DSTLEN_S) & HIFN_BASE_CMD_DSTLEN_M));
	buf_pos += sizeof(struct hifn_base_command);

	if (using_comp) {
		comp_cmd = (struct hifn_comp_command *)buf_pos;
		dlen = cmd->compcrd->crd_len;
		comp_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		comp_cmd->masks = htole16(cmd->comp_masks |
		    ((dlen << HIFN_COMP_CMD_SRCLEN_S) & HIFN_COMP_CMD_SRCLEN_M));
		comp_cmd->header_skip = htole16(cmd->compcrd->crd_skip);
		comp_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_comp_command);
	}

	if (using_mac) {
		mac_cmd = (struct hifn_mac_command *)buf_pos;
		dlen = cmd->maccrd->crd_len;
		mac_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		mac_cmd->masks = htole16(cmd->mac_masks |
		    ((dlen << HIFN_MAC_CMD_SRCLEN_S) & HIFN_MAC_CMD_SRCLEN_M));
		mac_cmd->header_skip = htole16(cmd->maccrd->crd_skip);
		mac_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_mac_command);
	}

	if (using_crypt) {
		cry_cmd = (struct hifn_crypt_command *)buf_pos;
		dlen = cmd->enccrd->crd_len;
		cry_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		cry_cmd->masks = htole16(cmd->cry_masks |
		    ((dlen << HIFN_CRYPT_CMD_SRCLEN_S) & HIFN_CRYPT_CMD_SRCLEN_M));
		cry_cmd->header_skip = htole16(cmd->enccrd->crd_skip);
		cry_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_crypt_command);
	}

	if (using_mac && cmd->mac_masks & HIFN_MAC_CMD_NEW_KEY) {
		bcopy(cmd->mac, buf_pos, HIFN_MAC_KEY_LENGTH);
		buf_pos += HIFN_MAC_KEY_LENGTH;
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_KEY) {
		switch (cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) {
		case HIFN_CRYPT_CMD_ALG_3DES:
			bcopy(cmd->ck, buf_pos, HIFN_3DES_KEY_LENGTH);
			buf_pos += HIFN_3DES_KEY_LENGTH;
			break;
		case HIFN_CRYPT_CMD_ALG_DES:
			bcopy(cmd->ck, buf_pos, HIFN_DES_KEY_LENGTH);
			buf_pos += HIFN_DES_KEY_LENGTH;
			break;
		case HIFN_CRYPT_CMD_ALG_RC4:
			len = 256;
			do {
				int clen;

				clen = MIN(cmd->cklen, len);
				bcopy(cmd->ck, buf_pos, clen);
				len -= clen;
				buf_pos += clen;
			} while (len > 0);
			bzero(buf_pos, 4);
			buf_pos += 4;
			break;
		case HIFN_CRYPT_CMD_ALG_AES:
			/*
			 * AES key are variable 128, 192 and
			 * 256 bits (16, 24 and 32 bytes).
			 */
			bcopy(cmd->ck, buf_pos, cmd->cklen);
			buf_pos += cmd->cklen;
			break;
		}
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_IV) {
		if ((cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) ==
		    HIFN_CRYPT_CMD_ALG_AES)
			ivlen = HIFN_AES_IV_LENGTH;
		else
			ivlen = HIFN_IV_LENGTH;
		bcopy(cmd->iv, buf_pos, ivlen);
		buf_pos += ivlen;
	}

	if ((cmd->base_masks & (HIFN_BASE_CMD_MAC | HIFN_BASE_CMD_CRYPT |
	    HIFN_BASE_CMD_COMP)) == 0) {
		bzero(buf_pos, 8);
		buf_pos += 8;
	}

	return (buf_pos - buf);
}

int
hifn_dmamap_aligned(bus_dmamap_t map)
{
	int i;

	for (i = 0; i < map->dm_nsegs; i++) {
		if (map->dm_segs[i].ds_addr & 3)
			return (0);
		if ((i != (map->dm_nsegs - 1)) &&
		    (map->dm_segs[i].ds_len & 3))
			return (0);
	}
	return (1);
}

int
hifn_dmamap_load_dst(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	bus_dmamap_t map = cmd->dst_map;
	u_int32_t p, l;
	int idx, used = 0, i;

	idx = dma->dsti;
	for (i = 0; i < map->dm_nsegs - 1; i++) {
		dma->dstr[idx].p = htole32(map->dm_segs[i].ds_addr);
		dma->dstr[idx].l = htole32(HIFN_D_VALID |
		    HIFN_D_MASKDONEIRQ | map->dm_segs[i].ds_len);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		used++;

		if (++idx == HIFN_D_DST_RSIZE) {
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			idx = 0;
		}
	}

	if (cmd->sloplen == 0) {
		p = map->dm_segs[i].ds_addr;
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    map->dm_segs[i].ds_len;
	} else {
		p = sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, slop[cmd->slopidx]);
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    sizeof(u_int32_t);

		if ((map->dm_segs[i].ds_len - cmd->sloplen) != 0) {
			dma->dstr[idx].p = htole32(map->dm_segs[i].ds_addr);
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_MASKDONEIRQ |
			    (map->dm_segs[i].ds_len - cmd->sloplen));
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			used++;

			if (++idx == HIFN_D_DST_RSIZE) {
				dma->dstr[idx].l = htole32(HIFN_D_VALID |
				    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
				HIFN_DSTR_SYNC(sc, idx,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				idx = 0;
			}
		}
	}
	dma->dstr[idx].p = htole32(p);
	dma->dstr[idx].l = htole32(l);
	HIFN_DSTR_SYNC(sc, idx, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	used++;

	if (++idx == HIFN_D_DST_RSIZE) {
		dma->dstr[idx].l = htole32(HIFN_D_VALID | HIFN_D_JUMP |
		    HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		idx = 0;
	}

	dma->dsti = idx;
	dma->dstu += used;
	return (idx);
}

int
hifn_dmamap_load_src(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	bus_dmamap_t map = cmd->src_map;
	int idx, i;
	u_int32_t last = 0;

	idx = dma->srci;
	for (i = 0; i < map->dm_nsegs; i++) {
		if (i == map->dm_nsegs - 1)
			last = HIFN_D_LAST;

		dma->srcr[idx].p = htole32(map->dm_segs[i].ds_addr);
		dma->srcr[idx].l = htole32(map->dm_segs[i].ds_len |
		    HIFN_D_VALID | HIFN_D_MASKDONEIRQ | last);
		HIFN_SRCR_SYNC(sc, idx,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

		if (++idx == HIFN_D_SRC_RSIZE) {
			dma->srcr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
			    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
			idx = 0;
		}
	}
	dma->srci = idx;
	dma->srcu += map->dm_nsegs;
	return (idx);
}

int 
hifn_crypto(struct hifn_softc *sc, struct hifn_command *cmd,
    struct cryptop *crp)
{
	struct	hifn_dma *dma = sc->sc_dma;
	u_int32_t cmdlen;
	int cmdi, resi, s, err = 0;

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->src_map))
		return (ENOMEM);

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else {
		err = EINVAL;
		goto err_srcmap1;
	}

	if (hifn_dmamap_aligned(cmd->src_map)) {
		cmd->sloplen = cmd->src_map->dm_mapsize & 3;
		if (crp->crp_flags & CRYPTO_F_IOV)
			cmd->dstu.dst_io = cmd->srcu.src_io;
		else if (crp->crp_flags & CRYPTO_F_IMBUF)
			cmd->dstu.dst_m = cmd->srcu.src_m;
		cmd->dst_map = cmd->src_map;
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			err = EINVAL;
			goto err_srcmap;
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			int totlen, len;
			struct mbuf *m, *m0, *mlast;

			totlen = cmd->src_map->dm_mapsize;
			if (cmd->srcu.src_m->m_flags & M_PKTHDR) {
				len = MHLEN;
				MGETHDR(m0, M_DONTWAIT, MT_DATA);
			} else {
				len = MLEN;
				MGET(m0, M_DONTWAIT, MT_DATA);
			}
			if (m0 == NULL) {
				err = ENOMEM;
				goto err_srcmap;
			}
			if (len == MHLEN) {
				err = m_dup_pkthdr(m0, cmd->srcu.src_m,
				    M_DONTWAIT);
				if (err) {
					m_free(m0);
					goto err_srcmap;
				}
			}
			if (totlen >= MINCLSIZE) {
				MCLGET(m0, M_DONTWAIT);
				if (m0->m_flags & M_EXT)
					len = MCLBYTES;
			}
			totlen -= len;
			m0->m_pkthdr.len = m0->m_len = len;
			mlast = m0;

			while (totlen > 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					err = ENOMEM;
					m_freem(m0);
					goto err_srcmap;
				}
				len = MLEN;
				if (totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
						len = MCLBYTES;
				}

				m->m_len = len;
				if (m0->m_flags & M_PKTHDR)
					m0->m_pkthdr.len += len;
				totlen -= len;

				mlast->m_next = m;
				mlast = m;
			}
			cmd->dstu.dst_m = m0;
		}
	}

	if (cmd->dst_map == NULL) {
		if (bus_dmamap_create(sc->sc_dmat,
		    HIFN_MAX_SEGLEN * MAX_SCATTER, MAX_SCATTER,
		    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->dst_map)) {
			err = ENOMEM;
			goto err_srcmap;
		}
		if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
			    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
				err = ENOMEM;
				goto err_dstmap1;
			}
		} else if (crp->crp_flags & CRYPTO_F_IOV) {
			if (bus_dmamap_load_uio(sc->sc_dmat, cmd->dst_map,
			    cmd->dstu.dst_io, BUS_DMA_NOWAIT)) {
				err = ENOMEM;
				goto err_dstmap1;
			}
		}
	}

#ifdef HIFN_DEBUG
	printf("%s: Entering cmd: stat %8x ien %8x u %d/%d/%d/%d n %d/%d\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER),
	    dma->cmdu, dma->srcu, dma->dstu, dma->resu,
	    cmd->src_map->dm_nsegs, cmd->dst_map->dm_nsegs);
#endif

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	}

	s = splnet();

	/*
	 * need 1 cmd, and 1 res
	 * need N src, and N dst
	 */
	if ((dma->cmdu + 1) > HIFN_D_CMD_RSIZE ||
	    (dma->resu + 1) > HIFN_D_RES_RSIZE) {
		splx(s);
		err = ENOMEM;
		goto err_dstmap;
	}
	if ((dma->srcu + cmd->src_map->dm_nsegs) > HIFN_D_SRC_RSIZE ||
	    (dma->dstu + cmd->dst_map->dm_nsegs + 1) > HIFN_D_DST_RSIZE) {
		splx(s);
		err = ENOMEM;
		goto err_dstmap;
	}

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	cmdi = dma->cmdi++;
	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
	HIFN_CMD_SYNC(sc, cmdi, BUS_DMASYNC_PREWRITE);

	/* .p for command/result already set */
	dma->cmdr[cmdi].l = htole32(cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ);
	HIFN_CMDR_SYNC(sc, cmdi,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	dma->cmdu++;
	if (sc->sc_c_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_C_CTRL_ENA);
		sc->sc_c_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED0);
	}

	/*
	 * Always enable the command wait interrupt.  We are obviously
	 * missing an interrupt or two somewhere. Enabling the command wait
	 * interrupt will guarantee we get called periodically until all
	 * of the queues are drained and thus work around this.
	 */
	sc->sc_dmaier |= HIFN_DMAIER_C_WAIT;
	WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);

	hifnstats.hst_ipackets++;
	hifnstats.hst_ibytes += cmd->src_map->dm_mapsize;

	hifn_dmamap_load_src(sc, cmd);
	if (sc->sc_s_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_S_CTRL_ENA);
		sc->sc_s_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED1);
	}

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
#ifdef HIFN_DEBUG
	printf("load res\n");
#endif
	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	resi = dma->resi++;
	dma->hifn_commands[resi] = cmd;
	HIFN_RES_SYNC(sc, resi, BUS_DMASYNC_PREREAD);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
	    HIFN_D_VALID | HIFN_D_LAST);
	HIFN_RESR_SYNC(sc, resi,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dma->resu++;
	if (sc->sc_r_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_CTRL_ENA);
		sc->sc_r_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED2);
	}

	if (cmd->sloplen)
		cmd->slopidx = resi;

	hifn_dmamap_load_dst(sc, cmd);

	if (sc->sc_d_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA);
		sc->sc_d_busy = 1;
	}

#ifdef HIFN_DEBUG
	printf("%s: command: stat %8x ier %8x\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER));
#endif

	sc->sc_active = 5;
	cmd->cmd_callback = hifn_callback;
	splx(s);
	return (err);		/* success */

err_dstmap:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
err_dstmap1:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
err_srcmap:
	if (crp->crp_flags & CRYPTO_F_IMBUF &&
	    cmd->srcu.src_m != cmd->dstu.dst_m)
		m_freem(cmd->dstu.dst_m);
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
err_srcmap1:
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	return (err);
}

void
hifn_tick(void *vsc)
{
	struct hifn_softc *sc = vsc;
	int s;

	s = splnet();
	if (sc->sc_active == 0) {
		struct hifn_dma *dma = sc->sc_dma;
		u_int32_t r = 0;

		if (dma->cmdu == 0 && sc->sc_c_busy) {
			sc->sc_c_busy = 0;
			r |= HIFN_DMACSR_C_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED0);
		}
		if (dma->srcu == 0 && sc->sc_s_busy) {
			sc->sc_s_busy = 0;
			r |= HIFN_DMACSR_S_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED1);
		}
		if (dma->dstu == 0 && sc->sc_d_busy) {
			sc->sc_d_busy = 0;
			r |= HIFN_DMACSR_D_CTRL_DIS;
		}
		if (dma->resu == 0 && sc->sc_r_busy) {
			sc->sc_r_busy = 0;
			r |= HIFN_DMACSR_R_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED2);
		}
		if (r)
			WRITE_REG_1(sc, HIFN_1_DMA_CSR, r);
	}
	else
		sc->sc_active--;
	splx(s);
	timeout_add_sec(&sc->sc_tickto, 1);
}

int 
hifn_intr(void *arg)
{
	struct hifn_softc *sc = arg;
	struct hifn_dma *dma = sc->sc_dma;
	u_int32_t dmacsr, restart;
	int i, u;

	dmacsr = READ_REG_1(sc, HIFN_1_DMA_CSR);

#ifdef HIFN_DEBUG
	printf("%s: irq: stat %08x ien %08x u %d/%d/%d/%d\n",
	    sc->sc_dv.dv_xname,
	    dmacsr, READ_REG_1(sc, HIFN_1_DMA_IER),
	    dma->cmdu, dma->srcu, dma->dstu, dma->resu);
#endif

	/* Nothing in the DMA unit interrupted */
	if ((dmacsr & sc->sc_dmaier) == 0)
		return (0);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR, dmacsr & sc->sc_dmaier);

	if (dmacsr & HIFN_DMACSR_ENGINE)
		WRITE_REG_0(sc, HIFN_0_PUISR, READ_REG_0(sc, HIFN_0_PUISR));

	if ((sc->sc_flags & HIFN_HAS_PUBLIC) &&
	    (dmacsr & HIFN_DMACSR_PUBDONE))
		WRITE_REG_1(sc, HIFN_1_PUB_STATUS,
		    READ_REG_1(sc, HIFN_1_PUB_STATUS) | HIFN_PUBSTS_DONE);

	restart = dmacsr & (HIFN_DMACSR_R_OVER | HIFN_DMACSR_D_OVER);
	if (restart)
		printf("%s: overrun %x\n", sc->sc_dv.dv_xname, dmacsr);

	if (sc->sc_flags & HIFN_IS_7811) {
		if (dmacsr & HIFN_DMACSR_ILLR)
			printf("%s: illegal read\n", sc->sc_dv.dv_xname);
		if (dmacsr & HIFN_DMACSR_ILLW)
			printf("%s: illegal write\n", sc->sc_dv.dv_xname);
	}

	restart = dmacsr & (HIFN_DMACSR_C_ABORT | HIFN_DMACSR_S_ABORT |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_R_ABORT);
	if (restart) {
		printf("%s: abort, resetting.\n", sc->sc_dv.dv_xname);
		hifnstats.hst_abort++;
		hifn_abort(sc);
		return (1);
	}

	if ((dmacsr & HIFN_DMACSR_C_WAIT) && (dma->resu == 0)) {
		/*
		 * If no slots to process and we receive a "waiting on
		 * command" interrupt, we disable the "waiting on command"
		 * (by clearing it).
		 */
		sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	/* clear the rings */
	i = dma->resk;
	while (dma->resu != 0) {
		HIFN_RESR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->resr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_RESR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}

		if (i != HIFN_D_RES_RSIZE) {
			struct hifn_command *cmd;

			HIFN_RES_SYNC(sc, i, BUS_DMASYNC_POSTREAD);
			cmd = dma->hifn_commands[i];

			(*cmd->cmd_callback)(sc, cmd, dma->result_bufs[i]);
			hifnstats.hst_opackets++;
		}

		if (++i == (HIFN_D_RES_RSIZE + 1))
			i = 0;
		else
			dma->resu--;
	}
	dma->resk = i;

	i = dma->srck; u = dma->srcu;
	while (u != 0) {
		HIFN_SRCR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->srcr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_SRCR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_SRC_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->srck = i; dma->srcu = u;

	i = dma->cmdk; u = dma->cmdu;
	while (u != 0) {
		HIFN_CMDR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->cmdr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_CMDR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (i != HIFN_D_CMD_RSIZE) {
			u--;
			HIFN_CMD_SYNC(sc, i, BUS_DMASYNC_POSTWRITE);
		}
		if (++i == (HIFN_D_CMD_RSIZE + 1))
			i = 0;
	}
	dma->cmdk = i; dma->cmdu = u;

	return (1);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
int
hifn_newsession(u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c;
	struct hifn_softc *sc = NULL;
	int i, mac = 0, cry = 0, comp = 0, sesn;
	struct hifn_session *ses = NULL;

	if (sidp == NULL || cri == NULL)
		return (EINVAL);

	for (i = 0; i < hifn_cd.cd_ndevs; i++) {
		sc = hifn_cd.cd_devs[i];
		if (sc == NULL)
			break;
		if (sc->sc_cid == (*sidp))
			break;
	}
	if (sc == NULL)
		return (EINVAL);

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct hifn_session *)malloc(
		    sizeof(*ses), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (!sc->sc_sessions[sesn].hs_used) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			ses = mallocarray((sesn + 1), sizeof(*ses),
			    M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			bcopy(sc->sc_sessions, ses, sesn * sizeof(*ses));
			explicit_bzero(sc->sc_sessions, sesn * sizeof(*ses));
			free(sc->sc_sessions, M_DEVBUF, 0);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}
	bzero(ses, sizeof(*ses));

	for (c = cri; c != NULL; c = c->cri_next) {
		switch (c->cri_alg) {
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
			if (mac)
				return (EINVAL);
			mac = 1;
			break;
		case CRYPTO_3DES_CBC:
		case CRYPTO_AES_CBC:
			if (cry)
				return (EINVAL);
			cry = 1;
			break;
		case CRYPTO_LZS_COMP:
			if (comp)
				return (EINVAL);
			comp = 1;
			break;
		default:
			return (EINVAL);
		}
	}
	if (mac == 0 && cry == 0 && comp == 0)
		return (EINVAL);

	/*
	 * XXX only want to support compression without chaining to
	 * MAC/crypt engine right now
	 */
	if ((comp && mac) || (comp && cry))
		return (EINVAL);

	*sidp = HIFN_SID(sc->sc_dv.dv_unit, sesn);
	ses->hs_used = 1;

	return (0);
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
int
hifn_freesession(u_int64_t tid)
{
	struct hifn_softc *sc;
	int card, session;
	u_int32_t sid = ((u_int32_t)tid) & 0xffffffff;

	card = HIFN_CARD(sid);
	if (card >= hifn_cd.cd_ndevs || hifn_cd.cd_devs[card] == NULL)
		return (EINVAL);

	sc = hifn_cd.cd_devs[card];
	session = HIFN_SESSION(sid);
	if (session >= sc->sc_nsessions)
		return (EINVAL);

	bzero(&sc->sc_sessions[session], sizeof(sc->sc_sessions[session]));
	return (0);
}

int
hifn_process(struct cryptop *crp)
{
	struct hifn_command *cmd = NULL;
	int card, session, err = 0, ivlen;
	struct hifn_softc *sc;
	struct cryptodesc *crd1, *crd2 = NULL, *maccrd, *enccrd;

	if (crp == NULL || crp->crp_callback == NULL) {
		hifnstats.hst_invalid++;
		return (EINVAL);
	}

	if (crp->crp_ilen == 0) {
		err = EINVAL;
		goto errout;
	}

	card = HIFN_CARD(crp->crp_sid);
	if (card >= hifn_cd.cd_ndevs || hifn_cd.cd_devs[card] == NULL) {
		err = EINVAL;
		goto errout;
	}

	sc = hifn_cd.cd_devs[card];
	session = HIFN_SESSION(crp->crp_sid);
	if (session >= sc->sc_nsessions) {
		err = EINVAL;
		goto errout;
	}

	cmd = malloc(sizeof(*cmd), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cmd == NULL) {
		err = ENOMEM;
		goto errout;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		cmd->srcu.src_m = (struct mbuf *)crp->crp_buf;
		cmd->dstu.dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		cmd->srcu.src_io = (struct uio *)crp->crp_buf;
		cmd->dstu.dst_io = (struct uio *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous buffers! */
	}

	if (crp->crp_ndesc < 1) {
		err = EINVAL;
		goto errout;
	}
	crd1 = &crp->crp_desc[0];
	if (crp->crp_ndesc >= 2)
		crd2 = &crp->crp_desc[1];

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_AES_CBC) {
			if ((crd1->crd_flags & CRD_F_ENCRYPT) == 0)
				cmd->base_masks |= HIFN_BASE_CMD_DECODE;
			maccrd = NULL;
			enccrd = crd1;
		} else if (crd1->crd_alg == CRYPTO_LZS_COMP) {
			return (hifn_compression(sc, crp, cmd));
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
		     crd1->crd_alg == CRYPTO_SHA1_HMAC) &&
		    (crd2->crd_alg == CRYPTO_3DES_CBC ||
		     crd2->crd_alg == CRYPTO_AES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			cmd->base_masks = HIFN_BASE_CMD_DECODE;
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_3DES_CBC ||
		     crd1->crd_alg == CRYPTO_AES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
		     crd2->crd_alg == CRYPTO_SHA1_HMAC) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the 7751 as requested
			 */
			err = EINVAL;
			goto errout;
		}
	}

	if (enccrd) {
		cmd->enccrd = enccrd;
		cmd->base_masks |= HIFN_BASE_CMD_CRYPT;
		switch (enccrd->crd_alg) {
		case CRYPTO_3DES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_3DES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		case CRYPTO_AES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_AES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		default:
			err = EINVAL;
			goto errout;
		}
		ivlen = ((enccrd->crd_alg == CRYPTO_AES_CBC) ?
		    HIFN_AES_IV_LENGTH : HIFN_IV_LENGTH);
		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, cmd->iv, ivlen);
			else
				arc4random_buf(cmd->iv, ivlen);

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					err = m_copyback(cmd->srcu.src_m,
					    enccrd->crd_inject,
					    ivlen, cmd->iv, M_NOWAIT);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copyback(cmd->srcu.src_io,
					    enccrd->crd_inject,
					    ivlen, cmd->iv);
				if (err)
					goto errout;
			}
		} else {
			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, cmd->iv, ivlen);
			else if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata(cmd->srcu.src_m,
				    enccrd->crd_inject, ivlen, cmd->iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata(cmd->srcu.src_io,
				    enccrd->crd_inject, ivlen, cmd->iv);
		}

		cmd->ck = enccrd->crd_key;
		cmd->cklen = enccrd->crd_klen >> 3;
		cmd->cry_masks |= HIFN_CRYPT_CMD_NEW_KEY;

		/*
		 * Need to specify the size for the AES key in the masks.
		 */
		if ((cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) ==
		    HIFN_CRYPT_CMD_ALG_AES) {
			switch (cmd->cklen) {
			case 16:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_128;
				break;
			case 24:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_192;
				break;
			case 32:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_256;
				break;
			default:
				err = EINVAL;
				goto errout;
			}
		}
	}

	if (maccrd) {
		cmd->maccrd = maccrd;
		cmd->base_masks |= HIFN_BASE_CMD_MAC;

		switch (maccrd->crd_alg) {
		case CRYPTO_MD5_HMAC:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		case CRYPTO_SHA1_HMAC:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		}

		if (maccrd->crd_alg == CRYPTO_SHA1_HMAC ||
		     maccrd->crd_alg == CRYPTO_MD5_HMAC) {
			cmd->mac_masks |= HIFN_MAC_CMD_NEW_KEY;
			bcopy(maccrd->crd_key, cmd->mac, maccrd->crd_klen >> 3);
			bzero(cmd->mac + (maccrd->crd_klen >> 3),
			    HIFN_MAC_KEY_LENGTH - (maccrd->crd_klen >> 3));
		}
	}

	cmd->crp = crp;
	cmd->session_num = session;
	cmd->softc = sc;

	err = hifn_crypto(sc, cmd, crp);
	if (!err)
		return 0;

errout:
	if (cmd != NULL) {
		explicit_bzero(cmd, sizeof(*cmd));
		free(cmd, M_DEVBUF, sizeof *cmd);
	}
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	crypto_done(crp);
	return (0);
}

void
hifn_abort(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_command *cmd;
	struct cryptop *crp;
	int i, u;

	i = dma->resk; u = dma->resu;
	while (u != 0) {
		cmd = dma->hifn_commands[i];
		crp = cmd->crp;

		if ((dma->resr[i].l & htole32(HIFN_D_VALID)) == 0) {
			/* Salvage what we can. */
			hifnstats.hst_opackets++;
			(*cmd->cmd_callback)(sc, cmd, dma->result_bufs[i]);
		} else {
			if (cmd->src_map == cmd->dst_map)
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
				    0, cmd->src_map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			else {
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
				    0, cmd->src_map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
				    0, cmd->dst_map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
			}

			if (cmd->srcu.src_m != cmd->dstu.dst_m) {
				m_freem(cmd->srcu.src_m);
				crp->crp_buf = (caddr_t)cmd->dstu.dst_m;
			}

			/* non-shared buffers cannot be restarted */
			if (cmd->src_map != cmd->dst_map) {
				/*
				 * XXX should be EAGAIN, delayed until
				 * after the reset.
				 */
				crp->crp_etype = ENOMEM;
				bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
				bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
			} else
				crp->crp_etype = ENOMEM;

			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
			bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);

			explicit_bzero(cmd, sizeof(*cmd));
			free(cmd, M_DEVBUF, sizeof *cmd);
			if (crp->crp_etype != EAGAIN)
				crypto_done(crp);
		}

		if (++i == HIFN_D_RES_RSIZE)
			i = 0;
		u--;
	}
	dma->resk = i; dma->resu = u;

	hifn_reset_board(sc, 1);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);
}

void
hifn_callback(struct hifn_softc *sc, struct hifn_command *cmd,
    u_int8_t *resbuf)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct cryptop *crp = cmd->crp;
	struct cryptodesc *crd;
	struct mbuf *m;
	int totlen, i, u;

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (cmd->srcu.src_m != cmd->dstu.dst_m) {
			crp->crp_buf = (caddr_t)cmd->dstu.dst_m;
			totlen = cmd->src_map->dm_mapsize;
			for (m = cmd->dstu.dst_m; m != NULL; m = m->m_next) {
				if (totlen < m->m_len) {
					m->m_len = totlen;
					totlen = 0;
				} else
					totlen -= m->m_len;
			}
			cmd->dstu.dst_m->m_pkthdr.len =
			    cmd->srcu.src_m->m_pkthdr.len;
			m_freem(cmd->srcu.src_m);
		}
	}

	if (cmd->sloplen != 0) {
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			crp->crp_etype =
			    m_copyback((struct mbuf *)crp->crp_buf,
			    cmd->src_map->dm_mapsize - cmd->sloplen,
			    cmd->sloplen, &dma->slop[cmd->slopidx],
			    M_NOWAIT);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			cuio_copyback((struct uio *)crp->crp_buf,
			    cmd->src_map->dm_mapsize - cmd->sloplen,
			    cmd->sloplen, &dma->slop[cmd->slopidx]);
		if (crp->crp_etype)
			goto out;
	}

	i = dma->dstk; u = dma->dstu;
	while (u != 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    offsetof(struct hifn_dma, dstr[i]), sizeof(struct hifn_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->dstr[i].l & htole32(HIFN_D_VALID)) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    offsetof(struct hifn_dma, dstr[i]),
			    sizeof(struct hifn_desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_DST_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->dstk = i; dma->dstu = u;

	hifnstats.hst_obytes += cmd->dst_map->dm_mapsize;

	if (cmd->base_masks & HIFN_BASE_CMD_MAC) {
		u_int8_t *macbuf;

		macbuf = resbuf + sizeof(struct hifn_base_result);
		if (cmd->base_masks & HIFN_BASE_CMD_COMP)
			macbuf += sizeof(struct hifn_comp_result);
		macbuf += sizeof(struct hifn_mac_result);

		for (i = 0; i < crp->crp_ndesc; i++) {
			int len;

			crd = &crp->crp_desc[i];

			if (crd->crd_alg == CRYPTO_MD5_HMAC ||
			    crd->crd_alg == CRYPTO_SHA1_HMAC)
				len = 12;
			else
				continue;

			if (crp->crp_flags & CRYPTO_F_IMBUF)
				crp->crp_etype =
				    m_copyback((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, len, macbuf, M_NOWAIT);
			else if ((crp->crp_flags & CRYPTO_F_IOV) && crp->crp_mac)
				bcopy((caddr_t)macbuf, crp->crp_mac, len);
			break;
		}
	}

out:
	if (cmd->src_map != cmd->dst_map) {
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	explicit_bzero(cmd, sizeof(*cmd));
	free(cmd, M_DEVBUF, sizeof *cmd);
	crypto_done(crp);
}

int
hifn_compression(struct hifn_softc *sc, struct cryptop *crp,
    struct hifn_command *cmd)
{
	struct cryptodesc *crd = &crp->crp_desc[0];
	int s, err = 0;

	cmd->compcrd = crd;
	cmd->base_masks |= HIFN_BASE_CMD_COMP;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) == 0) {
		/*
		 * XXX can only handle mbufs right now since we can
		 * XXX dynamically resize them.
		 */
		err = EINVAL;
		return (ENOMEM);
	}

	if ((crd->crd_flags & CRD_F_COMP) == 0)
		cmd->base_masks |= HIFN_BASE_CMD_DECODE;
	if (crd->crd_alg == CRYPTO_LZS_COMP)
		cmd->comp_masks |= HIFN_COMP_CMD_ALG_LZS |
		    HIFN_COMP_CMD_CLEARHIST;

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->src_map)) {
		err = ENOMEM;
		goto fail;
	}

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->dst_map)) {
		err = ENOMEM;
		goto fail;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		int len;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}

		len = cmd->src_map->dm_mapsize / MCLBYTES;
		if ((cmd->src_map->dm_mapsize % MCLBYTES) != 0)
			len++;
		len *= MCLBYTES;

		if ((crd->crd_flags & CRD_F_COMP) == 0)
			len *= 4;

		if (len > HIFN_MAX_DMALEN)
			len = HIFN_MAX_DMALEN;

		cmd->dstu.dst_m = hifn_mkmbuf_chain(len, cmd->srcu.src_m);
		if (cmd->dstu.dst_m == NULL) {
			err = ENOMEM;
			goto fail;
		}

		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
	}

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	}

	cmd->crp = crp;
	/*
	 * Always use session 0.  The modes of compression we use are
	 * stateless and there is always at least one compression
	 * context, zero.
	 */
	cmd->session_num = 0;
	cmd->softc = sc;

	s = splnet();
	err = hifn_compress_enter(sc, cmd);
	splx(s);

	if (err != 0)
		goto fail;
	return (0);

fail:
	if (cmd->dst_map != NULL) {
		if (cmd->dst_map->dm_nsegs > 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	if (cmd->src_map != NULL) {
		if (cmd->src_map->dm_nsegs > 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	}
	explicit_bzero(cmd, sizeof(*cmd));
	free(cmd, M_DEVBUF, sizeof *cmd);
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	crypto_done(crp);
	return (0);
}

/*
 * must be called at splnet()
 */
int
hifn_compress_enter(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	int cmdi, resi;
	u_int32_t cmdlen;

	if ((dma->cmdu + 1) > HIFN_D_CMD_RSIZE ||
	    (dma->resu + 1) > HIFN_D_CMD_RSIZE)
		return (ENOMEM);

	if ((dma->srcu + cmd->src_map->dm_nsegs) > HIFN_D_SRC_RSIZE ||
	    (dma->dstu + cmd->dst_map->dm_nsegs) > HIFN_D_DST_RSIZE)
		return (ENOMEM);

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	cmdi = dma->cmdi++;
	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
	HIFN_CMD_SYNC(sc, cmdi, BUS_DMASYNC_PREWRITE);

	/* .p for command/result already set */
	dma->cmdr[cmdi].l = htole32(cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ);
	HIFN_CMDR_SYNC(sc, cmdi,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	dma->cmdu++;
	if (sc->sc_c_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_C_CTRL_ENA);
		sc->sc_c_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED0);
	}

	/*
	 * Always enable the command wait interrupt.  We are obviously
	 * missing an interrupt or two somewhere. Enabling the command wait
	 * interrupt will guarantee we get called periodically until all
	 * of the queues are drained and thus work around this.
	 */
	sc->sc_dmaier |= HIFN_DMAIER_C_WAIT;
	WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);

	hifnstats.hst_ipackets++;
	hifnstats.hst_ibytes += cmd->src_map->dm_mapsize;

	hifn_dmamap_load_src(sc, cmd);
	if (sc->sc_s_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_S_CTRL_ENA);
		sc->sc_s_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED1);
	}

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	resi = dma->resi++;
	dma->hifn_commands[resi] = cmd;
	HIFN_RES_SYNC(sc, resi, BUS_DMASYNC_PREREAD);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
	    HIFN_D_VALID | HIFN_D_LAST);
	HIFN_RESR_SYNC(sc, resi,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dma->resu++;
	if (sc->sc_r_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_CTRL_ENA);
		sc->sc_r_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED2);
	}

	if (cmd->sloplen)
		cmd->slopidx = resi;

	hifn_dmamap_load_dst(sc, cmd);

	if (sc->sc_d_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA);
		sc->sc_d_busy = 1;
	}
	sc->sc_active = 5;
	cmd->cmd_callback = hifn_callback_comp;
	return (0);
}

void
hifn_callback_comp(struct hifn_softc *sc, struct hifn_command *cmd,
    u_int8_t *resbuf)
{
	struct hifn_base_result baseres;
	struct cryptop *crp = cmd->crp;
	struct hifn_dma *dma = sc->sc_dma;
	struct mbuf *m;
	int err = 0, i, u;
	u_int32_t olen;
	bus_size_t dstsize;

	bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
	    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
	    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);

	dstsize = cmd->dst_map->dm_mapsize;
	bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);

	bcopy(resbuf, &baseres, sizeof(struct hifn_base_result));

	i = dma->dstk; u = dma->dstu;
	while (u != 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    offsetof(struct hifn_dma, dstr[i]), sizeof(struct hifn_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->dstr[i].l & htole32(HIFN_D_VALID)) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    offsetof(struct hifn_dma, dstr[i]),
			    sizeof(struct hifn_desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_DST_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->dstk = i; dma->dstu = u;

	if (baseres.flags & htole16(HIFN_BASE_RES_DSTOVERRUN)) {
		bus_size_t xlen;

		xlen = dstsize;

		m_freem(cmd->dstu.dst_m);

		if (xlen == HIFN_MAX_DMALEN) {
			/* We've done all we can. */
			err = E2BIG;
			goto out;
		}

		xlen += MCLBYTES;

		if (xlen > HIFN_MAX_DMALEN)
			xlen = HIFN_MAX_DMALEN;

		cmd->dstu.dst_m = hifn_mkmbuf_chain(xlen,
		    cmd->srcu.src_m);
		if (cmd->dstu.dst_m == NULL) {
			err = ENOMEM;
			goto out;
		}
		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto out;
		}

		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);

		/* already at splnet... */
		err = hifn_compress_enter(sc, cmd);
		if (err != 0)
			goto out;
		return;
	}

	olen = dstsize - (letoh16(baseres.dst_cnt) |
	    (((letoh16(baseres.session) & HIFN_BASE_RES_DSTLEN_M) >>
	    HIFN_BASE_RES_DSTLEN_S) << 16));

	crp->crp_olen = olen - cmd->compcrd->crd_skip;

	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);

	m = cmd->dstu.dst_m;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len = olen;
	crp->crp_buf = (caddr_t)m;
	for (; m != NULL; m = m->m_next) {
		if (olen >= m->m_len)
			olen -= m->m_len;
		else {
			m->m_len = olen;
			olen = 0;
		}
	}

	m_freem(cmd->srcu.src_m);
	explicit_bzero(cmd, sizeof(*cmd));
	free(cmd, M_DEVBUF, sizeof *cmd);
	crp->crp_etype = 0;
	crypto_done(crp);
	return;

out:
	if (cmd->dst_map != NULL) {
		if (cmd->src_map->dm_nsegs != 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	if (cmd->src_map != NULL) {
		if (cmd->src_map->dm_nsegs != 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	}
	m_freem(cmd->dstu.dst_m);
	explicit_bzero(cmd, sizeof(*cmd));
	free(cmd, M_DEVBUF, sizeof *cmd);
	crp->crp_etype = err;
	crypto_done(crp);
}

struct mbuf *
hifn_mkmbuf_chain(int totlen, struct mbuf *mtemplate)
{
	int len;
	struct mbuf *m, *m0, *mlast;

	if (mtemplate->m_flags & M_PKTHDR) {
		len = MHLEN;
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
	} else {
		len = MLEN;
		MGET(m0, M_DONTWAIT, MT_DATA);
	}
	if (m0 == NULL)
		return (NULL);
	if (len == MHLEN) {
		if (m_dup_pkthdr(m0, mtemplate, M_DONTWAIT)) {
			m_free(m0);
			return (NULL);
		}
	}
	MCLGET(m0, M_DONTWAIT);
	if (!(m0->m_flags & M_EXT)) {
		m_freem(m0);
		return (NULL);
	}
	len = MCLBYTES;

	totlen -= len;
	m0->m_pkthdr.len = m0->m_len = len;
	mlast = m0;

	while (totlen > 0) {
		MGET(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			m_freem(m0);
			return (NULL);
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_free(m);
			m_freem(m0);
			return (NULL);
		}
		len = MCLBYTES;
		m->m_len = len;
		if (m0->m_flags & M_PKTHDR)
			m0->m_pkthdr.len += len;
		totlen -= len;

		mlast->m_next = m;
		mlast = m;
	}

	return (m0);
}

void
hifn_write_4(struct hifn_softc *sc, int reggrp, bus_size_t reg,
    u_int32_t val)
{
	/*
	 * 7811 PB3 rev/2 parts lock-up on burst writes to Group 0
	 * and Group 1 registers; avoid conditions that could create
	 * burst writes by doing a read in between the writes.
	 */
	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		if (sc->sc_waw_lastgroup == reggrp &&
		    sc->sc_waw_lastreg == reg - 4) {
			bus_space_read_4(sc->sc_st1, sc->sc_sh1, HIFN_1_REVID);
		}
		sc->sc_waw_lastgroup = reggrp;
		sc->sc_waw_lastreg = reg;
	}
	if (reggrp == 0)
		bus_space_write_4(sc->sc_st0, sc->sc_sh0, reg, val);
	else
		bus_space_write_4(sc->sc_st1, sc->sc_sh1, reg, val);

}

u_int32_t
hifn_read_4(struct hifn_softc *sc, int reggrp, bus_size_t reg)
{
	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		sc->sc_waw_lastgroup = -1;
		sc->sc_waw_lastreg = 1;
	}
	if (reggrp == 0)
		return (bus_space_read_4(sc->sc_st0, sc->sc_sh0, reg));
	return (bus_space_read_4(sc->sc_st1, sc->sc_sh1, reg));
}

/*	$OpenBSD: hifn7751.c,v 1.42 2000/06/17 20:34:52 jason Exp $	*/

/*
 * Invertex AEON / Hi/fn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000 Network Security Technologies, Inc.
 *			http://www.netsec.net
 *
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
 */

/*
 * Driver for the Hi/Fn 7751 encryption processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/device.h>

#include <crypto/crypto.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/hifn7751var.h>
#include <dev/pci/hifn7751reg.h>

#undef HIFN_DEBUG

/*
 * Prototypes and count for the pci_device structure
 */
int hifn_probe		__P((struct device *, void *, void *));
void hifn_attach	__P((struct device *, struct device *, void *));

struct cfattach hifn_ca = {
	sizeof(struct hifn_softc), hifn_probe, hifn_attach,
};

struct cfdriver hifn_cd = {
	0, "hifn", DV_DULL
};

void	hifn_reset_board __P((struct hifn_softc *));
int	hifn_enable_crypto __P((struct hifn_softc *, pcireg_t));
void	hifn_init_dma __P((struct hifn_softc *));
void	hifn_init_pci_registers __P((struct hifn_softc *));
int	hifn_sramsize __P((struct hifn_softc *));
int	hifn_dramsize __P((struct hifn_softc *));
void	hifn_ramtype __P((struct hifn_softc *));
void	hifn_sessions __P((struct hifn_softc *));
int	hifn_intr __P((void *));
u_int	hifn_write_command __P((struct hifn_command *, u_int8_t *));
u_int32_t hifn_next_signature __P((u_int a, u_int cnt));
int	hifn_newsession __P((u_int32_t *, struct cryptoini *));
int	hifn_freesession __P((u_int64_t));
int	hifn_process __P((struct cryptop *));
void	hifn_callback __P((struct hifn_softc *, struct hifn_command *, u_int8_t *));
int	hifn_crypto __P((struct hifn_softc *, hifn_command_t *));
int	hifn_readramaddr __P((struct hifn_softc *, int, u_int8_t *, int));
int	hifn_writeramaddr __P((struct hifn_softc *, int, u_int8_t *, int));

struct hifn_stats {
	u_int64_t hst_ibytes;
	u_int64_t hst_obytes;
	u_int32_t hst_ipackets;
	u_int32_t hst_opackets;
	u_int32_t hst_invalid;
	u_int32_t hst_nomem;
} hifnstats;

int
hifn_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INVERTEX &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INVERTEX_AEON)
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_7751)
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NETSEC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NETSEC_7751)
		return (1);
	return (0);
}

void 
hifn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hifn_softc *sc = (struct hifn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	char rbase;
	bus_addr_t iobase;
	bus_size_t iosize;
	u_int32_t cmd;
	u_int16_t ena;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	int rseg;
	caddr_t kva;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (pci_mem_find(pc, pa->pa_tag, HIFN_BAR0, &iobase, &iosize, NULL)) {
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh0)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_st0 = pa->pa_memt;

	if (pci_mem_find(pc, pa->pa_tag, HIFN_BAR1, &iobase, &iosize, NULL)) {
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh1)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_st1 = pa->pa_memt;

	sc->sc_dmat = pa->pa_dmat;
	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(*sc->sc_dma), PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't alloc dma buffer\n");
		return;
        }
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, sizeof(*sc->sc_dma), &kva,
	    BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%d bytes)\n",
		    sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(*sc->sc_dma), 1,
	    sizeof(*sc->sc_dma), 0, BUS_DMA_NOWAIT, &dmamap)) {
		printf(": can't create dma map\n");
		bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(sc->sc_dmat, dmamap, kva, sizeof(*sc->sc_dma),
	    NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load dma map\n");
		bus_dmamap_destroy(sc->sc_dmat, dmamap);
		bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	sc->sc_dma = (struct hifn_dma *)kva;
	bzero(sc->sc_dma, sizeof(*sc->sc_dma));

	hifn_reset_board(sc);

	if (hifn_enable_crypto(sc, pa->pa_id) != 0) {
		printf("%s: crypto enabling failed\n", sc->sc_dv.dv_xname);
		return;
	}

	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	hifn_ramtype(sc);

	if (sc->sc_drammodel == 0)
		hifn_sramsize(sc);
	else
		hifn_dramsize(sc);

	/*
	 * Reinitialize again, since the DRAM/SRAM detection shifted our ring
	 * pointers and may have changed the value we send to the RAM Config
	 * Register.
	 */
	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, hifn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt\n");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	hifn_sessions(sc);

	rseg = sc->sc_ramsize / 1024;
	rbase = 'K';
	if (sc->sc_ramsize >= (1024 * 1024)) {
		rbase = 'M';
		rseg /= 1024;
	}
	printf(", %d%cB %cram, %s\n", rseg, rbase,
	    sc->sc_drammodel ? 'd' : 's', intrstr);

	sc->sc_cid = crypto_get_driverid();
	if (sc->sc_cid < 0)
		return;

	WRITE_REG_0(sc, HIFN_0_PUCNFG,
	    READ_REG_0(sc, HIFN_0_PUCNFG) | HIFN_PUCNFG_CHIPID);
	ena = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	switch (ena) {
	case HIFN_PUSTAT_ENA_2:
		crypto_register(sc->sc_cid, CRYPTO_3DES_CBC,
		    hifn_newsession, hifn_freesession, hifn_process);
		/*FALLTHROUGH*/
	case HIFN_PUSTAT_ENA_1:
		crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC96,
		    hifn_newsession, hifn_freesession, hifn_process);
		crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC96,
		    NULL, NULL, NULL);
		crypto_register(sc->sc_cid, CRYPTO_DES_CBC,
		    NULL, NULL, NULL);
	}
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
void
hifn_reset_board(sc)
	struct hifn_softc *sc;
{
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

	/* Reset the board.  We do this by writing zeros to the DMA reset
	 * field, the BRD reset field, and the manditory 1 at position 2.
	 * Every other field is set to zero.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MODE);

	/*
	 * Wait another millisecond for the board to reset.
	 */
	DELAY(1000);

	/*
	 * Turn off the reset!  (No joke.)
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
}

u_int32_t
hifn_next_signature(a, cnt)
	u_int a, cnt;
{
	int i, v;

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
		/*
		 * Other vendors share this PCI ID as well, such as
		 * http://www.powercrypt.com, and obviously they also
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
hifn_enable_crypto(sc, pciid)
	struct hifn_softc *sc;
	pcireg_t pciid;
{
	u_int32_t dmacfg, ramcfg, encl, addr, i;
	char *offtbl = NULL;

	for (i = 0; i < sizeof(pci2id)/sizeof(pci2id[0]); i++) {
		if (pci2id[i].pci_vendor == PCI_VENDOR(pciid) &&
		    pci2id[i].pci_prod == PCI_PRODUCT(pciid)) {
			offtbl = pci2id[i].card_id;
			break;
		}
	}

	if (offtbl == NULL) {
#ifdef HIFN_DEBUG
		printf("%s: Unknown card!\n", sc->sc_dv.dv_xname);
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
		printf("%s: Strong Crypto already enabled!\n",
		    sc->sc_dv.dv_xname);
#endif
		WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg);
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG, dmacfg);
		return 0;	/* success */
	}

	if (encl != 0 && encl != HIFN_PUSTAT_ENA_0) {
#ifdef HIFN_DEBUG
		printf("%: Unknown encryption level\n",  sc->sc_dv.dv_xname);
#endif
		return 1;
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_UNLOCK |
	    HIFN_DMACNFG_MSTRESET | HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
	addr = READ_REG_1(sc, HIFN_UNLOCK_SECRET1);
	WRITE_REG_1(sc, HIFN_UNLOCK_SECRET2, 0);

	for (i = 0; i <= 12; i++) {
		addr = hifn_next_signature(addr, offtbl[i] + 0x101);
		WRITE_REG_1(sc, HIFN_UNLOCK_SECRET2, addr);

		DELAY(1000);
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);
	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

#ifdef HIFN_DEBUG
	if (encl != HIFN_PUSTAT_ENA_1 && encl != HIFN_PUSTAT_ENA_2)
		printf("Encryption engine is permanently locked until next system reset.");
	else
		printf("Encryption engine enabled successfully!");
#endif

	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, dmacfg);

	switch(encl) {
	case HIFN_PUSTAT_ENA_0:
		printf(": no encr/auth");
		break;
	case HIFN_PUSTAT_ENA_1:
		printf(": DES enabled");
		break;
	case HIFN_PUSTAT_ENA_2:
		printf(": fully enabled");
		break;
	default:
		printf(": disabled");
		break;
	}

	return 0;
}

/*
 * Give initial values to the registers listed in the "Register Space"
 * section of the HIFN Software Development reference manual.
 */
void 
hifn_init_pci_registers(sc)
	struct hifn_softc *sc;
{
	/* write fixed values needed by the Initialization registers */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	WRITE_REG_0(sc, HIFN_0_FIFOCNFG, HIFN_FIFOCNFG_THRESHOLD);
	WRITE_REG_0(sc, HIFN_0_PUIER, HIFN_PUIER_DSTOVER);

	/* write all 4 ring address registers */
	WRITE_REG_1(sc, HIFN_1_DMA_CRAR, vtophys(sc->sc_dma->cmdr));
	WRITE_REG_1(sc, HIFN_1_DMA_SRAR, vtophys(sc->sc_dma->srcr));
	WRITE_REG_1(sc, HIFN_1_DMA_DRAR, vtophys(sc->sc_dma->dstr));
	WRITE_REG_1(sc, HIFN_1_DMA_RRAR, vtophys(sc->sc_dma->resr));

	/* write status register */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA |
	    HIFN_DMACSR_R_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_C_CTRL_ENA);
	WRITE_REG_1(sc, HIFN_1_DMA_IER, HIFN_DMAIER_R_DONE);

#if 0
#if BYTE_ORDER == BIG_ENDIAN
	    (0x1 << 7) |
#endif
#endif
	WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
	    HIFN_PUCNFG_DRFR_128 | HIFN_PUCNFG_TCALLPHASES |
	    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32 |
	    (sc->sc_drammodel ? HIFN_PUCNFG_DRAM : HIFN_PUCNFG_SRAM));

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
hifn_sessions(sc)
	struct hifn_softc *sc;
{
	u_int32_t pucnfg;
	int ctxsize;

	pucnfg = READ_REG_0(sc, HIFN_0_PUCNFG);

	if (pucnfg & HIFN_PUCNFG_COMPSING) {
		if (pucnfg & HIFN_PUCNFG_ENCCNFG)
			ctxsize = 128;
		else
			ctxsize = 512;
		sc->sc_maxses = 1 +
		    ((sc->sc_ramsize - 32768) / ctxsize);
	}
	else
		sc->sc_maxses = sc->sc_ramsize / 16384;

	if (sc->sc_maxses > 2048)
		sc->sc_maxses = 2048;
}

void
hifn_ramtype(sc)
	struct hifn_softc *sc;
{
	u_int8_t data[8], dataexpect[8];
	int i;

	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0x55;
	if (hifn_writeramaddr(sc, 0, data, 0) < 0)
		return;
	if (hifn_readramaddr(sc, 0, data, 1) < 0)
		return;
	if (bcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return;
	}

	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0xaa;
	if (hifn_writeramaddr(sc, 0, data, 0) < 0)
		return;
	if (hifn_readramaddr(sc, 0, data, 1) < 0)
		return;
	if (bcmp(data, dataexpect, sizeof(data)) != 0)
		sc->sc_drammodel = 1;
}

/*
 * For sram boards, just write/read memory until it fails, also check for
 * banking.
 */
int
hifn_sramsize(sc)
	struct hifn_softc *sc;
{
	u_int32_t a = 0, end;
	u_int8_t data[8], dataexpect[8];

	for (a = 0; a < sizeof(data); a++)
		data[a] = dataexpect[a] = 0x5a;

	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);
	end = 1 << 20;	/* 1MB */
	for (a = 0; a < end; a += 16384) {
		if (hifn_writeramaddr(sc, a, data, 0) < 0)
			return (0);
		if (hifn_readramaddr(sc, a, data, 1) < 0)
			return (0);
		if (bcmp(data, dataexpect, sizeof(data)) != 0)
			return (0);
		hifn_reset_board(sc);
		hifn_init_dma(sc);
		hifn_init_pci_registers(sc);
		sc->sc_ramsize = a + 16384;
	}

	for (a = 0; a < sizeof(data); a++)
		data[a] = dataexpect[a] = 0xa5;
	if (hifn_writeramaddr(sc, 0, data, 0) < 0)
		return (0);

	end = sc->sc_ramsize;
	for (a = 0; a < end; a += 16384) {
		hifn_reset_board(sc);
		hifn_init_dma(sc);
		hifn_init_pci_registers(sc);
		if (hifn_readramaddr(sc, a, data, 0) < 0)
			return (0);
		if (a != 0 && bcmp(data, dataexpect, sizeof(data)) == 0)
			return (0);
		sc->sc_ramsize = a + 16384;
	}

	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	return (0);
}

/*
 * XXX For dram boards, one should really try all of the
 * HIFN_PUCNFG_DSZ_*'s.  This just assumes that PUCNFG
 * is already set up correctly.
 */
int
hifn_dramsize(sc)
	struct hifn_softc *sc;
{
	u_int32_t cnfg;

	cnfg = READ_REG_0(sc, HIFN_0_PUCNFG) &
	    HIFN_PUCNFG_DRAMMASK;
	sc->sc_ramsize = 1 << ((cnfg >> 13) + 18);
	return (0);
}

int
hifn_writeramaddr(sc, addr, data, slot)
	struct hifn_softc *sc;
	int addr, slot;
	u_int8_t *data;
{
	struct hifn_dma *dma = sc->sc_dma;
	hifn_base_command_t wc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	u_int64_t src, dst;

	wc.masks = 3 << 13;
	wc.session_num = addr >> 14;
	wc.total_source_count = 8;
	wc.total_dest_count = addr & 0x3fff;;

	/* build write command */
	*(hifn_base_command_t *) sc->sc_dma->command_bufs[slot] = wc;
	bcopy(data, &src, sizeof(src));

	dma->srcr[slot].p = vtophys(&src);
	dma->dstr[slot].p = vtophys(&dst);

	dma->cmdr[slot].l = 16 | masks;
	dma->srcr[slot].l = 8 | masks;
	dma->dstr[slot].l = 8 | masks;
	dma->resr[slot].l = HIFN_MAX_RESULT | masks;

	DELAY(1000);	/* let write command execute */
	if (dma->resr[slot].l & HIFN_D_VALID) {
		printf("%s: SRAM/DRAM detection error -- "
		    "result[%d] valid still set\n", sc->sc_dv.dv_xname, slot);
		return (-1);
	}
	return (0);
}

int
hifn_readramaddr(sc, addr, data, slot)
	struct hifn_softc *sc;
	int addr, slot;
	u_int8_t *data;
{
	struct hifn_dma *dma = sc->sc_dma;
	hifn_base_command_t rc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	u_int64_t src, dst;

	rc.masks = 2 << 13;
	rc.session_num = addr >> 14;
	rc.total_source_count = addr & 0x3fff;
	rc.total_dest_count = 8;

	*(hifn_base_command_t *) sc->sc_dma->command_bufs[slot] = rc;

	dma->srcr[slot].p = vtophys(&src);
	dma->dstr[slot].p = vtophys(&dst);
	dma->cmdr[slot].l = 16 | masks;
	dma->srcr[slot].l = 8 | masks;
	dma->dstr[slot].l = 8 | masks;
	dma->resr[slot].l = HIFN_MAX_RESULT | masks;

	DELAY(1000);	/* let read command execute */
	if (dma->resr[slot].l & HIFN_D_VALID) {
		printf("%s: SRAM/DRAM detection error -- "
		    "result[%d] valid still set\n", sc->sc_dv.dv_xname, slot);
		return (-1);
	}
	bcopy(&dst, data, sizeof(dst));
	return (0);
}

/*
 * Initialize the descriptor rings.
 */
void 
hifn_init_dma(sc)
	struct hifn_softc *sc;
{
	struct hifn_dma *dma = sc->sc_dma;
	int i;

	/* initialize static pointer values */
	for (i = 0; i < HIFN_D_CMD_RSIZE; i++)
		dma->cmdr[i].p = vtophys(dma->command_bufs[i]);
	for (i = 0; i < HIFN_D_RES_RSIZE; i++)
		dma->resr[i].p = vtophys(dma->result_bufs[i]);

	dma->cmdr[HIFN_D_CMD_RSIZE].p = vtophys(dma->cmdr);
	dma->srcr[HIFN_D_SRC_RSIZE].p = vtophys(dma->srcr);
	dma->dstr[HIFN_D_DST_RSIZE].p = vtophys(dma->dstr);
	dma->resr[HIFN_D_RES_RSIZE].p = vtophys(dma->resr);
	dma->cmdu = dma->srcu = dma->dstu = dma->resu = 0;
	dma->cmdi = dma->srci = dma->dsti = dma->resi = 0;
	dma->cmdk = dma->srck = dma->dstk = dma->resk = 0;
}

/*
 * Writes out the raw command buffer space.  Returns the
 * command buffer size.
 */
u_int
hifn_write_command(cmd, buf)
	struct hifn_command *cmd;
	u_int8_t *buf;
{
	u_int8_t *buf_pos;
	hifn_base_command_t *base_cmd;
	hifn_mac_command_t *mac_cmd;
	hifn_crypt_command_t *cry_cmd;
	int using_mac, using_crypt, len;

	buf_pos = buf;
	using_mac = cmd->base_masks & HIFN_BASE_CMD_MAC;
	using_crypt = cmd->base_masks & HIFN_BASE_CMD_CRYPT;

	base_cmd = (hifn_base_command_t *)buf_pos;
	base_cmd->masks = cmd->base_masks;
	base_cmd->total_source_count = cmd->src_l;
	base_cmd->total_dest_count = cmd->dst_l;
	base_cmd->session_num = cmd->session_num;
	buf_pos += sizeof(hifn_base_command_t);

	if (using_mac) {
		mac_cmd = (hifn_mac_command_t *)buf_pos;
		mac_cmd->masks = cmd->mac_masks;
		mac_cmd->header_skip = cmd->mac_header_skip;
		mac_cmd->source_count = cmd->mac_process_len;
		buf_pos += sizeof(hifn_mac_command_t);
	}

	if (using_crypt) {
		cry_cmd = (hifn_crypt_command_t *)buf_pos;
		cry_cmd->masks = cmd->cry_masks;
		cry_cmd->header_skip = cmd->crypt_header_skip;
		cry_cmd->source_count = cmd->crypt_process_len;
		buf_pos += sizeof(hifn_crypt_command_t);
	}

	if (using_mac && mac_cmd->masks & HIFN_MAC_CMD_NEW_KEY) {
		bcopy(cmd->mac, buf_pos, HIFN_MAC_KEY_LENGTH);
		buf_pos += HIFN_MAC_KEY_LENGTH;
	}

	if (using_crypt && cry_cmd->masks & HIFN_CRYPT_CMD_NEW_KEY) {
		len = (cry_cmd->masks & HIFN_CRYPT_CMD_ALG_3DES) ?
		    HIFN_3DES_KEY_LENGTH : HIFN_DES_KEY_LENGTH;
		bcopy(cmd->ck, buf_pos, len);
		buf_pos += len;
	}

	if (using_crypt && cry_cmd->masks & HIFN_CRYPT_CMD_NEW_IV) {
		bcopy(cmd->iv, buf_pos, HIFN_IV_LENGTH);
		buf_pos += HIFN_IV_LENGTH;
	}

	if ((base_cmd->masks & (HIFN_BASE_CMD_MAC | HIFN_BASE_CMD_CRYPT)) == 0) {
		bzero(buf_pos, 8);
		buf_pos += 8;
	}

	return (buf_pos - buf);
}

int 
hifn_crypto(sc, cmd)
	struct hifn_softc *sc;
	struct hifn_command *cmd;
{
	u_int32_t cmdlen;
	struct	hifn_dma *dma = sc->sc_dma;
	int	cmdi, srci, dsti, resi, nicealign = 0;
	int     s, i;

	if (cmd->src_npa == 0 && cmd->src_m)
		cmd->src_l = mbuf2pages(cmd->src_m, &cmd->src_npa,
		    cmd->src_packp, cmd->src_packl, MAX_SCATTER, &nicealign);
	if (cmd->src_l == 0)
		return (-1);

	if (nicealign == 0) {
		int totlen, len;
		struct mbuf *m, *top, **mp;

		totlen = cmd->dst_l = cmd->src_l;
		if (cmd->src_m->m_flags & M_PKTHDR) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			M_COPY_PKTHDR(m, cmd->src_m);
			len = MHLEN;
		} else {
			MGET(m, M_DONTWAIT, MT_DATA);
			len = MLEN;
		}
		if (m == NULL)
			return (-1);
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len;
		top = NULL;
		mp = &top;

		while (totlen > 0) {
			if (top) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					m_freem(top);
					return (-1);
				}
				len = MLEN;
			}
			if (top && totlen >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (m->m_flags & M_EXT)
					len = MCLBYTES;
			}
			m->m_len = len;
			totlen -= len;
			*mp = m;
			mp = &m->m_next;
		}
		cmd->dst_m = top;
	}
	else
		cmd->dst_m = cmd->src_m;

	cmd->dst_l = mbuf2pages(cmd->dst_m, &cmd->dst_npa,
	    cmd->dst_packp, cmd->dst_packl, MAX_SCATTER, NULL);
	if (cmd->dst_l == 0)
		return (-1);

#ifdef HIFN_DEBUG
	printf("%s: Entering cmd: stat %8x ien %8x u %d/%d/%d/%d n %d/%d\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER),
	    dma->cmdu, dma->srcu, dma->dstu, dma->resu, cmd->src_npa,
	    cmd->dst_npa);
#endif

	s = splnet();

	/*
	 * need 1 cmd, and 1 res
	 * need N src, and N dst
	 */
	if (dma->cmdu+1 > HIFN_D_CMD_RSIZE ||
	    dma->srcu+cmd->src_npa > HIFN_D_SRC_RSIZE ||
	    dma->dstu+cmd->dst_npa > HIFN_D_DST_RSIZE ||
	    dma->resu+1 > HIFN_D_RES_RSIZE) {
			splx(s);
			return (HIFN_CRYPTO_RINGS_FULL);
	}

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = HIFN_D_VALID | HIFN_D_LAST |
		    HIFN_D_MASKDONEIRQ | HIFN_D_JUMP;
	}
	cmdi = dma->cmdi++;

	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = HIFN_D_VALID | HIFN_D_LAST |
		    HIFN_D_MASKDONEIRQ | HIFN_D_JUMP;
	}
	resi = dma->resi++;

	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
#ifdef HIFN_DEBUG
	printf("write_command %d (nice %d)\n", cmdlen, nicealign);
#endif
	/* .p for command/result already set */
	dma->cmdr[cmdi].l = cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ;
	dma->cmdu++;

	/*
	 * We don't worry about missing an interrupt (which a "command wait"
	 * interrupt salvages us from), unless there is more than one command
	 * in the queue.
	 */
	if (dma->cmdu > 1)
		WRITE_REG_1(sc, HIFN_1_DMA_IER,
		    HIFN_DMAIER_C_WAIT | HIFN_DMAIER_R_DONE);

	hifnstats.hst_ipackets++;

	for (i = 0; i < cmd->src_npa; i++) {
		int last = 0;

		if (i == cmd->src_npa-1)
			last = HIFN_D_LAST;

		if (dma->srci == HIFN_D_SRC_RSIZE) {
			srci = 0, dma->srci = 1;
			dma->srcr[HIFN_D_SRC_RSIZE].l = HIFN_D_VALID |
			    HIFN_D_MASKDONEIRQ | HIFN_D_JUMP | HIFN_D_LAST;
		} else
			srci = dma->srci++;
		dma->srcr[srci].p = cmd->src_packp[i];
		dma->srcr[srci].l = cmd->src_packl[i] | HIFN_D_VALID |
		    HIFN_D_MASKDONEIRQ | last;
		hifnstats.hst_ibytes += cmd->src_packl[i];
	}
	dma->srcu += cmd->src_npa;

	for (i = 0; i < cmd->dst_npa; i++) {
		int last = 0;

		if (i == cmd->dst_npa-1)
			last = HIFN_D_LAST;

		if (dma->dsti == HIFN_D_DST_RSIZE) {
			dsti = 0, dma->dsti = 1;
			dma->dstr[HIFN_D_DST_RSIZE].l = HIFN_D_VALID |
			    HIFN_D_MASKDONEIRQ | HIFN_D_JUMP | HIFN_D_LAST;
		} else
			dsti = dma->dsti++;
		dma->dstr[dsti].p = cmd->dst_packp[i];
		dma->dstr[dsti].l = cmd->dst_packl[i] | HIFN_D_VALID |
		    HIFN_D_MASKDONEIRQ | last;
	}
	dma->dstu += cmd->dst_npa;

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
#ifdef HIFN_DEBUG
	printf("load res\n");
#endif
	dma->hifn_commands[resi] = cmd;
	dma->resr[resi].l = HIFN_MAX_RESULT | HIFN_D_VALID | HIFN_D_LAST;
	dma->resu++;

#ifdef HIFN_DEBUG
	printf("%s: command: stat %8x ier %8x\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER));
#endif

	splx(s);
	return 0;		/* success */
}

int 
hifn_intr(arg)
	void *arg;
{
	struct hifn_softc *sc = arg;
	struct hifn_dma *dma = sc->sc_dma;
	u_int32_t dmacsr;
	int i, u;

	dmacsr = READ_REG_1(sc, HIFN_1_DMA_CSR);

#ifdef HIFN_DEBUG
	printf("%s: irq: stat %08x ien %08x u %d/%d/%d/%d\n",
	    sc->sc_dv.dv_xname,
	    dmacsr, READ_REG_1(sc, HIFN_1_DMA_IER),
	    dma->cmdu, dma->srcu, dma->dstu, dma->resu);
#endif

	if ((dmacsr & (HIFN_DMACSR_R_DONE | HIFN_DMACSR_C_WAIT)) == 0)
		return (0);

	if (dma->resu > HIFN_D_RES_RSIZE)
		printf("%s: Internal Error -- ring overflow\n",
		    sc->sc_dv.dv_xname);

	if ((dmacsr & HIFN_DMACSR_C_WAIT) && (dma->cmdu == 0)) {
		/*
		 * If no slots to process and we receive a "waiting on
		 * command" interrupt, we disable the "waiting on command"
		 * (by clearing it).
		 */
		WRITE_REG_1(sc, HIFN_1_DMA_IER, HIFN_DMAIER_R_DONE);
	}

	while (dma->resu > 0) {
		struct hifn_command *cmd;
		u_int8_t *macbuf = NULL;

		cmd = dma->hifn_commands[dma->resk];

		/* if still valid, stop processing */
		if (dma->resr[dma->resk].l & HIFN_D_VALID)
			break;

		if (cmd->base_masks & HIFN_BASE_CMD_MAC) {
			macbuf = dma->result_bufs[dma->resk];
			macbuf += 12;
		}

		hifn_callback(sc, cmd, macbuf);
	
		if (++dma->resk == HIFN_D_RES_RSIZE)
			dma->resk = 0;
		dma->resu--;
		hifnstats.hst_opackets++;
	}

	/* clear the rings */

	i = dma->srck; u = dma->srcu;
	while (u != 0 && (dma->srcr[i].l & HIFN_D_VALID) == 0) {
		if (++i == HIFN_D_SRC_RSIZE)
			i = 0;
		u--;
	}
	dma->srck = i; dma->srcu = u;

	i = dma->cmdk; u = dma->cmdu;
	while (u != 0 && (dma->cmdr[i].l & HIFN_D_VALID) == 0) {
		if (++i == HIFN_D_CMD_RSIZE)
			i = 0;
		u--;
	}
	dma->cmdk = i; dma->cmdu = u;

	/*
	 * Clear "result done" and "command wait" flags in status register.
	 * If we still have slots to process and we received a "command wait"
	 * interrupt, this will interupt us again.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_DONE|HIFN_DMACSR_C_WAIT);
	return (1);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
int
hifn_newsession(sidp, cri)
	u_int32_t *sidp;
	struct cryptoini *cri;
{
	struct cryptoini *c;
	struct hifn_softc *sc = NULL;
	int i, mac = 0, cry = 0;

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

	for (i = 0; i < sc->sc_maxses; i++) {
		if (sc->sc_sessions[i].hs_flags == 0)
			break;
	}
	if (i == sc->sc_maxses)
		return (ENOMEM);

	for (c = cri; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5_HMAC96 ||
		    c->cri_alg == CRYPTO_SHA1_HMAC96) {
			if (mac)
				return (EINVAL);
			mac = 1;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
		    c->cri_alg == CRYPTO_3DES_CBC) {
			if (cry)
				return (EINVAL);
			cry = 1;
		}
		else
			return (EINVAL);
	}
	if (mac == 0 && cry == 0)
		return (EINVAL);

	*sidp = HIFN_SID(sc->sc_dv.dv_unit, i);
	sc->sc_sessions[i].hs_flags = 1;
	get_random_bytes(sc->sc_sessions[i].hs_iv, HIFN_IV_LENGTH);

	return (0);
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
int
hifn_freesession(tid)
	u_int64_t tid;
{
	struct hifn_softc *sc;
	int card, session;
	u_int32_t sid = (tid >> 31) & 0xffffffff;

	card = HIFN_CARD(sid);
	if (card >= hifn_cd.cd_ndevs || hifn_cd.cd_devs[card] == NULL)
		return (EINVAL);

	sc = hifn_cd.cd_devs[card];
	session = HIFN_SESSION(sid);
	if (session >= sc->sc_maxses)
		return (EINVAL);

	sc->sc_sessions[session].hs_flags = 0;
	return (0);
}

int
hifn_process(crp)
	struct cryptop *crp;
{
	struct hifn_command *cmd = NULL;
	int card, session, err;
	struct hifn_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;

	if (crp == NULL || crp->crp_callback == NULL) {
		hifnstats.hst_invalid++;
		return (EINVAL);
	}

	card = HIFN_CARD(crp->crp_sid);
	if (card >= hifn_cd.cd_ndevs || hifn_cd.cd_devs[card] == NULL) {
		err = EINVAL;
		goto errout;
	}

	sc = hifn_cd.cd_devs[card];
	session = HIFN_SESSION(crp->crp_sid);
	if (session >= sc->sc_maxses) {
		err = EINVAL;
		goto errout;
	}

	cmd = (struct hifn_command *)malloc(sizeof(struct hifn_command),
	    M_DEVBUF, M_NOWAIT);
	if (cmd == NULL) {
		err = ENOMEM;
		goto errout;
	}
	bzero(cmd, sizeof(struct hifn_command));

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		cmd->src_m = (struct mbuf *)crp->crp_buf;
		cmd->dst_m = (struct mbuf *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX only handle mbufs right now */
	}

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC96) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
			 crd1->crd_alg == CRYPTO_3DES_CBC) {
			if ((crd1->crd_flags & CRD_F_ENCRYPT) == 0)
				cmd->base_masks |= HIFN_BASE_CMD_DECODE;
			maccrd = NULL;
			enccrd = crd1;
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC96) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
			crd2->crd_alg == CRYPTO_3DES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			cmd->base_masks = HIFN_BASE_CMD_DECODE;
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC96 ||
			crd2->crd_alg == CRYPTO_SHA1_HMAC96) &&
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
		cmd->base_masks |= HIFN_BASE_CMD_CRYPT;
		cmd->cry_masks |= HIFN_CRYPT_CMD_MODE_CBC |
		    HIFN_CRYPT_CMD_NEW_IV;
		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, cmd->iv, HIFN_IV_LENGTH);
			else
				bcopy(sc->sc_sessions[session].hs_iv,
				    cmd->iv, HIFN_IV_LENGTH);

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0)
				m_copyback(cmd->src_m, enccrd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);
		} else {
			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, cmd->iv, HIFN_IV_LENGTH);
			else
				m_copydata(cmd->src_m, enccrd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);
		}

		if (enccrd->crd_alg == CRYPTO_DES_CBC)
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_DES;
		else
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_3DES;

		cmd->crypt_header_skip = enccrd->crd_skip;
		cmd->crypt_process_len = enccrd->crd_len;
		cmd->ck = enccrd->crd_key;

		if (sc->sc_sessions[session].hs_flags == 1)
			cmd->cry_masks |= HIFN_CRYPT_CMD_NEW_KEY;
	}

	if (maccrd) {
		cmd->base_masks |= HIFN_BASE_CMD_MAC;
		cmd->mac_masks |= HIFN_MAC_CMD_RESULT |
		    HIFN_MAC_CMD_MODE_HMAC | HIFN_MAC_CMD_RESULT |
		    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;

		if (maccrd->crd_alg == CRYPTO_MD5_HMAC96)
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5;
		else
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1;

		if (sc->sc_sessions[session].hs_flags == 1) {
			cmd->mac_masks |= HIFN_MAC_CMD_NEW_KEY;
			bcopy(maccrd->crd_key, cmd->mac, maccrd->crd_klen >> 3);
			bzero(cmd->mac + (maccrd->crd_klen >> 3),
			    HIFN_MAC_KEY_LENGTH - (maccrd->crd_klen >> 3));
		}

		cmd->mac_header_skip = maccrd->crd_skip;
		cmd->mac_process_len = maccrd->crd_len;
	}

	if (sc->sc_sessions[session].hs_flags == 1)
		sc->sc_sessions[session].hs_flags = 2;

	cmd->private_data = (u_long)crp;
	cmd->session_num = session;
	cmd->softc = sc;

	if (hifn_crypto(sc, cmd) == 0)
		return (0);

	err = ENOMEM;

errout:
	if (cmd != NULL)
		free(cmd, M_DEVBUF);
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	crp->crp_callback(crp);
	return (0);
}

void
hifn_callback(sc, cmd, macbuf)
	struct hifn_softc *sc;
	struct hifn_command *cmd;
	u_int8_t *macbuf;
{
	struct hifn_dma *dma = sc->sc_dma;
	struct cryptop *crp = (struct cryptop *)cmd->private_data;
	struct cryptodesc *crd;
	struct mbuf *m;
	int totlen;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (cmd->src_m != cmd->dst_m)) {
		m_freem(cmd->src_m);
		crp->crp_buf = (caddr_t)cmd->dst_m;
	}

	if ((m = cmd->dst_m) != NULL) {
		totlen = cmd->src_l;
		hifnstats.hst_obytes += totlen;
		while (m) {
			if (totlen < m->m_len) {
				m->m_len = totlen;
				totlen = 0;
			} else
				totlen -= m->m_len;
			m = m->m_next;
			if (++dma->dstk == HIFN_D_DST_RSIZE)
				dma->dstk = 0;
			dma->dstu--;
		}
	} else {
		hifnstats.hst_obytes += dma->dstr[dma->dstk].l & HIFN_D_LENGTH;
		if (++dma->dstk == HIFN_D_DST_RSIZE)
			dma->dstk = 0;
		dma->dstu--;
	}

	if ((cmd->base_masks & (HIFN_BASE_CMD_CRYPT | HIFN_BASE_CMD_DECODE)) ==
	    HIFN_BASE_CMD_CRYPT) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC)
				continue;
			m_copydata((struct mbuf *)crp->crp_buf,
			    crd->crd_skip + crd->crd_len - 8, 8,
			    cmd->softc->sc_sessions[cmd->session_num].hs_iv);
			break;
		}
	}

	if (macbuf != NULL) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_MD5_HMAC96 &&
			    crd->crd_alg != CRYPTO_SHA1_HMAC96)
				continue;
			m_copyback((struct mbuf *)crp->crp_buf,
			    crd->crd_inject, 12, macbuf);
			break;
		}
	}

	free(cmd, M_DEVBUF);
	crp->crp_callback(crp);
}

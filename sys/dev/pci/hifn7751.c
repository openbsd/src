/*	$OpenBSD: hifn7751.c,v 1.23 2000/03/31 05:49:08 jason Exp $	*/

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
int	hifn_checkramaddr __P((struct hifn_softc *, int));
void	hifn_sessions __P((struct hifn_softc *));
int	hifn_intr __P((void *));
u_int	hifn_write_command __P((const struct hifn_command_buf_data *,
    u_int8_t *));
int	hifn_build_command __P((const struct hifn_command * cmd,
    struct hifn_command_buf_data *));
int	hifn_mbuf __P((struct mbuf *, int *, long *, int *, int, int *));
u_int32_t hifn_next_signature __P((u_int a, u_int cnt));
int	hifn_newsession __P((u_int32_t *, struct cryptoini *));
int	hifn_freesession __P((u_int32_t));
int	hifn_process __P((struct cryptop *));
void	hifn_callback __P((struct hifn_command *));
int	hifn_crypto __P((struct hifn_softc *, hifn_command_t *));

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
	cmd |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
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
#ifdef HIFN_DEBUG
	printf(" mem %x %x", sc->sc_sh0, sc->sc_sh1);
#endif

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

	if (hifn_checkramaddr(sc, 0) != 0)
		sc->sc_drammodel = 1;

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
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_IMP, hifn_intr, sc,
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
#if 0
		/* Can't do md5/sha1 yet */
		crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC96,
		    hifn_newsession, hifn_freesession, hifn_process);
		crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC96,
		    NULL, NULL, NULL);
#endif
		crypto_register(sc->sc_cid, CRYPTO_DES_CBC,
		    hifn_newsession, hifn_freesession, hifn_process);
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

/*
 * For sram boards, just write/read memory until it fails.
 */
int
hifn_sramsize(sc)
	struct hifn_softc *sc;
{
	u_int32_t a = 0, end;

	hifn_reset_board(sc);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);
	end = 1 << 21;	/* 2MB */
	for (a = 0; a < end; a += 16384) {
		if (hifn_checkramaddr(sc, a) < 0)
			return (0);
		hifn_reset_board(sc);
		hifn_init_dma(sc);
		hifn_init_pci_registers(sc);
		sc->sc_ramsize = a + 16384;
	}
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


/*
 * There are both DRAM and SRAM models of the hifn board.
 * A bit in the "ram configuration register" needs to be
 * set according to the model.  The driver will guess one
 * way or the other -- and then call this routine to verify.
 *
 * 0: RAM setting okay,  -1: Current RAM setting in error
 */
int 
hifn_checkramaddr(sc, addr)
	struct hifn_softc *sc;
	int addr;
{
	hifn_base_command_t write_command,read_command;
	u_int8_t data[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
	u_int8_t *source_buf, *dest_buf;
	struct hifn_dma *dma = sc->sc_dma;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ;

	write_command.masks = 3 << 13;
	write_command.session_num = addr >> 14;
	write_command.total_source_count = 8;
	write_command.total_dest_count = addr & 0x3fff;;

	read_command.masks = 2 << 13;
	read_command.session_num = addr >> 14;
	read_command.total_source_count = addr & 0x3fff;
	read_command.total_dest_count = 8;

#if (HIFN_D_CMD_RSIZE < 3)
#error "descriptor ring size too small DRAM/SRAM check"
#endif

	/*
	 * We steal the 8 bytes needed for both the source and dest buffers
	 * from the 3rd slot that the DRAM/SRAM test won't use.
	 */
	source_buf = sc->sc_dma->command_bufs[2];
	dest_buf = sc->sc_dma->result_bufs[2];

	/* build write command */
	*(hifn_base_command_t *) sc->sc_dma->command_bufs[0] = write_command;
	bcopy(data, source_buf, sizeof(data));

	dma->srcr[0].p = vtophys(source_buf);
	dma->dstr[0].p = vtophys(dest_buf);

	dma->cmdr[0].l = 16 | masks;
	dma->srcr[0].l = 8 | masks;
	dma->dstr[0].l = 8 | masks;
	dma->resr[0].l = HIFN_MAX_RESULT | masks;

	DELAY(1000);	/* let write command execute */
	if (dma->resr[0].l & HIFN_D_VALID)
		printf("%s: SRAM/DRAM detection error -- result[0] valid still set\n",
		    sc->sc_dv.dv_xname);

	/* Build read command */
	*(hifn_base_command_t *) sc->sc_dma->command_bufs[1] = read_command;

	dma->srcr[1].p = vtophys(source_buf);
	dma->dstr[1].p = vtophys(dest_buf);
	dma->cmdr[1].l = 16 | masks;
	dma->srcr[1].l = 8 | masks;
	dma->dstr[1].l = 8 | masks;
	dma->resr[1].l = HIFN_MAX_RESULT | masks;

	DELAY(1000);	/* let read command execute */
	if (dma->resr[1].l & HIFN_D_VALID)
		printf("%s: SRAM/DRAM detection error -- result[1] valid still set\n",
		    sc->sc_dv.dv_xname);
	return (memcmp(dest_buf, data, sizeof(data)) == 0) ? 0 : -1;
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
hifn_write_command(const struct hifn_command_buf_data *cmd_data,
    u_int8_t *command_buf)
{
	u_int8_t *command_buf_pos = command_buf;
	const hifn_base_command_t *base_cmd = &cmd_data->base_cmd;
	const hifn_mac_command_t *mac_cmd = &cmd_data->mac_cmd;
	const hifn_crypt_command_t *crypt_cmd = &cmd_data->crypt_cmd;
	int     using_mac = base_cmd->masks & HIFN_BASE_CMD_MAC;
	int     using_crypt = base_cmd->masks & HIFN_BASE_CMD_CRYPT;

	/* write base command structure */
	*((hifn_base_command_t *) command_buf_pos) = *base_cmd;
	command_buf_pos += sizeof(hifn_base_command_t);

	/* Write MAC command structure */
	if (using_mac) {
		*((hifn_mac_command_t *) command_buf_pos) = *mac_cmd;
		command_buf_pos += sizeof(hifn_mac_command_t);
	}

	/* Write encryption command structure */
	if (using_crypt) {
		*((hifn_crypt_command_t *) command_buf_pos) = *crypt_cmd;
		command_buf_pos += sizeof(hifn_crypt_command_t);
	}

	/* write MAC key */
	if (mac_cmd->masks & HIFN_MAC_NEW_KEY) {
		bcopy(cmd_data->mac, command_buf_pos, HIFN_MAC_KEY_LENGTH);
		command_buf_pos += HIFN_MAC_KEY_LENGTH;
	}

	/* Write crypto key */
	if (crypt_cmd->masks & HIFN_CRYPT_CMD_NEW_KEY) {
		u_int32_t alg = crypt_cmd->masks & HIFN_CRYPT_CMD_ALG_MASK;
		u_int32_t key_len = (alg == HIFN_CRYPT_CMD_ALG_DES) ?
		HIFN_DES_KEY_LENGTH : HIFN_3DES_KEY_LENGTH;
		bcopy(cmd_data->ck, command_buf_pos, key_len);
		command_buf_pos += key_len;
	}

	/* Write crypto iv */
	if (crypt_cmd->masks & HIFN_CRYPT_CMD_NEW_IV) {
		bcopy(cmd_data->iv, command_buf_pos, HIFN_IV_LENGTH);
		command_buf_pos += HIFN_IV_LENGTH;
	}

	/* Write 8 zero bytes we're not sending crypt or MAC structures */
	if (!(base_cmd->masks & HIFN_BASE_CMD_MAC) &&
	    !(base_cmd->masks & HIFN_BASE_CMD_CRYPT)) {
		*((u_int32_t *) command_buf_pos) = 0;
		command_buf_pos += 4;
		*((u_int32_t *) command_buf_pos) = 0;
		command_buf_pos += 4;
	}

	if ((command_buf_pos - command_buf) > HIFN_MAX_COMMAND)
		printf("hifn: Internal Error -- Command buffer overflow.\n");
	return command_buf_pos - command_buf;
}

/*
 * Check command input and build up structure to write
 * the command buffer later.  Returns 0 on success and
 * -1 if given bad command input was given.
 */
int 
hifn_build_command(const struct hifn_command *cmd,
    struct hifn_command_buf_data * cmd_buf_data)
{
#define HIFN_COMMAND_CHECKING

	u_int32_t flags = cmd->flags;
	hifn_base_command_t *base_cmd = &cmd_buf_data->base_cmd;
	hifn_mac_command_t *mac_cmd = &cmd_buf_data->mac_cmd;
	hifn_crypt_command_t *crypt_cmd = &cmd_buf_data->crypt_cmd;
	u_int   mac_length;
#if defined(HIFN_COMMAND_CHECKING) && 0
	int     dest_diff;
#endif

	bzero(cmd_buf_data, sizeof(struct hifn_command_buf_data));

#ifdef HIFN_COMMAND_CHECKING
	if (!(!!(flags & HIFN_DECODE) ^ !!(flags & HIFN_ENCODE))) {
		printf("hifn: encode/decode setting error\n");
		return -1;
	}
	if ((flags & HIFN_CRYPT_DES) && (flags & HIFN_CRYPT_3DES)) {
		printf("hifn: Too many crypto algorithms set in command\n");
		return -1;
	}
	if ((flags & HIFN_MAC_SHA1) && (flags & HIFN_MAC_MD5)) {
		printf("hifn: Too many MAC algorithms set in command\n");
		return -1;
	}
#endif


	/*
	 * Compute the mac value length -- leave at zero if not MAC'ing
	 */
	mac_length = 0;
	if (HIFN_USING_MAC(flags)) {
		mac_length = (flags & HIFN_MAC_TRUNC) ? HIFN_MAC_TRUNC_LENGTH :
		    ((flags & HIFN_MAC_MD5) ? HIFN_MD5_LENGTH : HIFN_SHA1_LENGTH);
	}
#ifdef HIFN_COMMAND_CHECKING
	/*
	 * Check for valid src/dest buf sizes
	 */

	/*
	 * XXX XXX  We need to include header counts into all these
	 *           checks!!!!
	 * XXX These tests are totally wrong.
	 */
#if 0
	if (cmd->src_npa <= mac_length) {
		printf("hifn: command source buffer has no data: %d <= %d\n",
		    cmd->src_npa, mac_length);
		return -1;
	}
	dest_diff = (flags & HIFN_ENCODE) ? mac_length : -mac_length;
	if (cmd->dst_npa < cmd->dst_npa + dest_diff) {
		printf("hifn:  command dest length %u too short -- needed %u\n",
		    cmd->dst_npa, cmd->dst_npa + dest_diff);
		return -1;
	}
#endif
#endif

	/*
	 * Set MAC bit
	 */
	if (HIFN_USING_MAC(flags))
		base_cmd->masks |= HIFN_BASE_CMD_MAC;

	/* Set Encrypt bit */
	if (HIFN_USING_CRYPT(flags))
		base_cmd->masks |= HIFN_BASE_CMD_CRYPT;

	/*
	 * Set Decode bit
	 */
	if (flags & HIFN_DECODE)
		base_cmd->masks |= HIFN_BASE_CMD_DECODE;

	/*
	 * Set total source and dest counts.  These values are the same as the
	 * values set in the length field of the source and dest descriptor rings.
	 */
	base_cmd->total_source_count = cmd->src_l;
	base_cmd->total_dest_count = cmd->dst_l;

	/*
	 * XXX -- We need session number range checking...
	 */
	base_cmd->session_num = cmd->session_num;

	/**
	 **  Building up mac command
	 **
	 **/
	if (HIFN_USING_MAC(flags)) {

		/*
		 * Set the MAC algorithm and trunc setting
		 */
		mac_cmd->masks |= (flags & HIFN_MAC_MD5) ?
		    HIFN_MAC_CMD_ALG_MD5 : HIFN_MAC_CMD_ALG_SHA1;
		if (flags & HIFN_MAC_TRUNC)
			mac_cmd->masks |= HIFN_MAC_CMD_TRUNC;

		/*
		 * We always use HMAC mode, assume MAC values are appended to the
		 * source buffer on decodes and we append them to the dest buffer
		 * on encodes, and order auth/encryption engines as needed by
		 * IPSEC
		 */
		mac_cmd->masks |= HIFN_MAC_CMD_MODE_HMAC | HIFN_MAC_CMD_APPEND |
		    HIFN_MAC_CMD_POS_IPSEC;

		/*
		 * Setup to send new MAC key if needed.
		 */
		if (flags & HIFN_MAC_NEW_KEY) {
			mac_cmd->masks |= HIFN_MAC_CMD_NEW_KEY;
			cmd_buf_data->mac = cmd->mac;
		}
		/*
		 * Set the mac header skip and source count.
		 */
		mac_cmd->header_skip = cmd->mac_header_skip;
		mac_cmd->source_count = cmd->mac_process_len;
		if (flags & HIFN_DECODE)
			mac_cmd->source_count -= mac_length;
	}

	if (HIFN_USING_CRYPT(flags)) {
		/*
		 * Set the encryption algorithm bits.
		 */
		crypt_cmd->masks |= (flags & HIFN_CRYPT_DES) ?
		    HIFN_CRYPT_CMD_ALG_DES : HIFN_CRYPT_CMD_ALG_3DES;

		/* We always use CBC mode and send a new IV (as needed by
		 * IPSec). */
		crypt_cmd->masks |= HIFN_CRYPT_CMD_MODE_CBC | HIFN_CRYPT_CMD_NEW_IV;

		/*
		 * Setup to send new encrypt key if needed.
		 */
		if (flags & HIFN_CRYPT_NEW_KEY) {
			crypt_cmd->masks |= HIFN_CRYPT_CMD_NEW_KEY;
			cmd_buf_data->ck = cmd->ck;
		}
		/*
		 * Set the encrypt header skip and source count.
		 */
		crypt_cmd->header_skip = cmd->crypt_header_skip;
		crypt_cmd->source_count = cmd->crypt_process_len;
		if (flags & HIFN_DECODE)
			crypt_cmd->source_count -= mac_length;


#ifdef HIFN_COMMAND_CHECKING
		if (crypt_cmd->source_count % 8 != 0) {
			printf("hifn:  Error -- encryption source %u not a multiple of 8!\n",
			    crypt_cmd->source_count);
			return -1;
		}
#endif
	}
	cmd_buf_data->iv = cmd->iv;


#if 0
	printf("hifn: command parameters"
	    " -- session num %u"
	    " -- base t.s.c: %u"
	    " -- base t.d.c: %u"
	    " -- mac h.s. %u  s.c. %u"
	    " -- crypt h.s. %u  s.c. %u\n",
	    base_cmd->session_num, base_cmd->total_source_count,
	    base_cmd->total_dest_count, mac_cmd->header_skip,
	    mac_cmd->source_count, crypt_cmd->header_skip,
	    crypt_cmd->source_count);
#endif

	return 0;		/* success */
}

int
hifn_mbuf(m, np, pp, lp, maxp, nicep)
	struct mbuf *m;
	int *np;
	long *pp;
	int *lp;
	int maxp;
	int *nicep;
{
	struct	mbuf *m0;
	int npa = 0, tlen = 0;

	/* generate a [pa,len] array from an mbuf */
	for (m0 = m; m; m = m->m_next) {
		void *va;
		long pg, npg;
		int len, off;

		if (m->m_len == 0)
			continue;
		len = m->m_len;
		tlen += len;
		va = m->m_data;

		lp[npa] = len;
		pp[npa] = vtophys(va);
		pg = pp[npa] & ~PAGE_MASK;
		off = (long)va & PAGE_MASK;

		while (len + off > PAGE_SIZE) {
			va = va + PAGE_SIZE - off;
			npg = vtophys(va);
			if (npg != pg) {
				/* FUCKED UP condition */
				if (++npa > maxp)
					return (0);
				continue;
			}
			lp[npa] = PAGE_SIZE - off;
			off = 0;

			if (++npa > maxp)
				return (0);

			lp[npa] = len - (PAGE_SIZE - off);
			len -= lp[npa];
			pp[npa] = vtophys(va);
		} 

		if (++npa == maxp)
			return (0);
	}

	if (nicep) {
		int nice = 1;
		int i;

		/* see if each [pa,len] entry is long-word aligned */
		for (i = 0; i < npa; i++)
			if ((lp[i] & 3) || (pp[i] & 3))
				nice = 0;
		*nicep = nice;
	}

	*np = npa;
	return (tlen);
}

int 
hifn_crypto(sc, cmd)
	struct hifn_softc *sc;
	struct hifn_command *cmd;
{
	u_int32_t cmdlen;
	struct	hifn_dma *dma = sc->sc_dma;
	struct	hifn_command_buf_data cmd_buf_data;
	int	cmdi, srci, dsti, resi, nicealign = 0;
	int     s, i;

	if (cmd->src_npa == 0 && cmd->src_m)
		cmd->src_l = hifn_mbuf(cmd->src_m, &cmd->src_npa,
		    cmd->src_packp, cmd->src_packl, MAX_SCATTER, &nicealign);
	if (cmd->src_l == 0)
		return (-1);

	if (nicealign == 0) {
		cmd->dst_l = cmd->src_l;
		MGETHDR(cmd->dst_m, M_DONTWAIT, MT_DATA);
		if (cmd->dst_m == NULL)
			return (-1);
		if (cmd->src_l > MHLEN) {
			MCLGET(cmd->dst_m, M_DONTWAIT);
			if ((cmd->dst_m->m_flags & M_EXT) == 0) {
				m_freem(cmd->dst_m);
				return (-1);
			}
		}
	} else
		cmd->dst_m = cmd->src_m;

	cmd->dst_l = hifn_mbuf(cmd->dst_m, &cmd->dst_npa,
	    cmd->dst_packp, cmd->dst_packl, MAX_SCATTER, NULL);
	if (cmd->dst_l == 0)
		return (-1);

	if (hifn_build_command(cmd, &cmd_buf_data) != 0)
		return HIFN_CRYPTO_BAD_INPUT;

#ifdef HIFN_DEBUG
	printf("%s: Entering cmd: stat %8x ien %8x u %d/%d/%d/%d n %d/%d\n",
	    sc->sc_dv.dv_xname,
	    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER),
	    dma->cmdu, dma->srcu, dma->dstu, dma->resu, cmd->src_npa,
	    cmd->dst_npa);
#endif

	s = splimp();

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

	cmdlen = hifn_write_command(&cmd_buf_data, dma->command_bufs[cmdi]);
#ifdef HIFN_DEBUG
	printf("write_command %d (nice %d)\n", cmdlen, nicealign);
#endif
	/* .p for command/result already set */
	dma->cmdr[cmdi].l = cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ;
	dma->cmdu++;
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

	if ((dmacsr & HIFN_DMACSR_R_DONE) == 0)
		return (0);

	if (dma->resu > HIFN_D_RES_RSIZE)
		printf("%s: Internal Error -- ring overflow\n",
		    sc->sc_dv.dv_xname);

	while (dma->resu > 0) {
		struct hifn_command *cmd;

		cmd = dma->hifn_commands[dma->resk];

		/* if still valid, stop processing */
		if (dma->resr[dma->resk].l & HIFN_D_VALID)
			break;

		if (HIFN_USING_MAC(cmd->flags) && (cmd->flags & HIFN_DECODE)) {
			u_int8_t *result_buf = dma->result_bufs[dma->resk];
	
			cmd->result_flags = (result_buf[8] & 0x2) ?
			    HIFN_MAC_BAD : 0;
		}
	
		/* position is done, notify producer with callback */
		cmd->dest_ready_callback(cmd);
	
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

	i = dma->dstk; u = dma->dstu;
	while (u != 0 && (dma->dstr[i].l & HIFN_D_VALID) == 0) {
		hifnstats.hst_obytes += dma->dstr[i].l & 0xffff;
		if (++i == HIFN_D_DST_RSIZE)
			i = 0;
		u--;
	}
	dma->dstk = i; dma->dstu = u;

	i = dma->cmdk; u = dma->cmdu;
	while (u != 0 && (dma->cmdr[i].l & HIFN_D_VALID) == 0) {
		if (++i == HIFN_D_CMD_RSIZE)
			i = 0;
		u--;
	}
	dma->cmdk = i; dma->cmdu = u;

	/*
	 * Clear "result done" flags in status register.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_DONE);
	return (1);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 * XXX Mac and encrypt keys should be sent to context ram and should
 * XXX maintain some sort of state.
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

	for (c = cri, i = 0; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5_HMAC96 ||
		    c->cri_alg == CRYPTO_SHA1_HMAC96) {
			if (mac)
				return (EINVAL);
			mac = 1;
		}
		else if (c->cri_alg == CRYPTO_DES_CBC ||
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

	*sidp = HIFN_SID(sc->sc_dv.dv_unit, 1);
	return (0);
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
int
hifn_freesession(sid)
	u_int32_t sid;
{
	return (0);
}

int
hifn_process(crp)
	struct cryptop *crp;
{
	struct hifn_command *cmd = NULL;
	int card, session, err;
	struct hifn_softc *sc;
	struct cryptodesc *crd;

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
	}
	else {
		err = EINVAL;
		goto errout;	/* XXX only handle mbufs right now */
	}

	for (crd = crp->crp_desc; crd != NULL; crd = crd->crd_next) {
		if (crd->crd_flags & CRD_F_ENCRYPT)
			cmd->flags |= HIFN_ENCODE;
		else
			cmd->flags |= HIFN_DECODE;

		if (crd->crd_alg == CRYPTO_MD5_HMAC96) {
			/* XXX not right */
			cmd->flags |= HIFN_MAC_MD5 | HIFN_MAC_TRUNC |
			    HIFN_MAC_NEW_KEY;
			cmd->mac_header_skip = crd->crd_skip;
			cmd->mac_process_len = crd->crd_len;
			cmd->mac = crd->crd_key;
			cmd->mac_len = crd->crd_klen >> 3;
		}
		else if (crd->crd_alg == CRYPTO_SHA1_HMAC96) {
			/* XXX not right */
			cmd->flags |= HIFN_MAC_SHA1 | HIFN_MAC_TRUNC |
			    HIFN_MAC_NEW_KEY;
			cmd->mac_header_skip = crd->crd_skip;
			cmd->mac_process_len = crd->crd_len;
			cmd->mac = crd->crd_key;
			cmd->mac_len = crd->crd_klen >> 3;
		}
		else if (crd->crd_alg == CRYPTO_DES_CBC) {
			if ((crd->crd_flags &
			     (CRD_F_ENCRYPT | CRD_F_IV_PRESENT)) ==
			    CRD_F_ENCRYPT) {
				get_random_bytes(cmd->iv, HIFN_IV_LENGTH);
				m_copyback(cmd->src_m, crd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);
			}
			else
				m_copydata(cmd->src_m, crd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);

			cmd->flags |= HIFN_CRYPT_DES | HIFN_CRYPT_NEW_KEY;
			cmd->crypt_header_skip = crd->crd_skip;
			cmd->crypt_process_len = crd->crd_len;
			cmd->ck = crd->crd_key;
			cmd->ck_len = crd->crd_klen >> 3;
		}
		else if (crd->crd_alg == CRYPTO_3DES_CBC) {
			if ((crd->crd_flags &
			     (CRD_F_ENCRYPT | CRD_F_IV_PRESENT)) ==
			    CRD_F_IV_PRESENT) {
				get_random_bytes(cmd->iv, HIFN_IV_LENGTH);
				m_copyback(cmd->src_m, crd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);
			}
			else
				m_copydata(cmd->src_m, crd->crd_inject,
				    HIFN_IV_LENGTH, cmd->iv);

			cmd->flags |= HIFN_CRYPT_3DES | HIFN_CRYPT_NEW_KEY;
			cmd->crypt_header_skip = crd->crd_skip;
			cmd->crypt_process_len = crd->crd_len;
			cmd->ck = crd->crd_key;
			cmd->ck_len = crd->crd_klen >> 3;
		}
		else {
			err = EINVAL;
			goto errout;
		}
	}

	cmd->private_data = (u_long)crp;
	cmd->dest_ready_callback = hifn_callback;

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
	return (crp->crp_callback(crp));
}

void
hifn_callback(cmd)
	struct hifn_command *cmd;
{
	struct cryptop *crp = (struct cryptop *)cmd->private_data;

	if (HIFN_USING_MAC(cmd->flags) && !HIFN_MAC_OK(cmd->result_flags))
		crp->crp_etype = EIO;
	else
		crp->crp_etype = 0;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (cmd->src_m != cmd->dst_m)) {
		m_freem(cmd->src_m);
		crp->crp_buf = (caddr_t)cmd->dst_m;
	}

	free(cmd, M_DEVBUF);
	crp->crp_callback(crp);
}

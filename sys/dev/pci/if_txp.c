/*	$OpenBSD: if_txp.c,v 1.9 2001/04/09 22:04:59 jason Exp $	*/

/*
 * Copyright (c) 2001
 *	Jason L. Wright <jason@thought.net> and
 *	Aaron Campbell <aaron@monkey.org>.  All rights reserved.
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
 *	This product includes software developed by Jason L. Wright and
 *	Aaron Campbell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for 3c990 (Typhoon) Ethernet ASIC
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_txpreg.h>
#include <dev/microcode/typhoon/typhoon_image.h>

int txp_probe		__P((struct device *, void *, void *));
void txp_attach		__P((struct device *, struct device *, void *));
int txp_intr		__P((void *));
void txp_tick		__P((void *));
void txp_shutdown	__P((void *));
int txp_ioctl		__P((struct ifnet *, u_long, caddr_t));
void txp_start		__P((struct ifnet *));
void txp_stop		__P((struct txp_softc *));
void txp_init		__P((struct txp_softc *));
void txp_watchdog	__P((struct ifnet *));

int txp_chip_init __P((struct txp_softc *));
int txp_reset_adapter __P((struct txp_softc *));
int txp_download_fw __P((struct txp_softc *));
int txp_download_fw_wait __P((struct txp_softc *));
int txp_download_fw_section __P((struct txp_softc *,
    struct txp_fw_section_header *, int));
int txp_alloc_rings __P((struct txp_softc *));
void txp_dma_free __P((struct txp_softc *, struct txp_dma_alloc *));
int txp_dma_malloc __P((struct txp_softc *, bus_size_t, struct txp_dma_alloc *));

int txp_cmd_desc_numfree __P((struct txp_softc *));
int txp_command __P((struct txp_softc *, u_int16_t, u_int16_t, u_int32_t,
    u_int32_t, u_int16_t *, u_int32_t *, u_int32_t *, int));
int txp_response __P((struct txp_softc *, u_int32_t, u_int16_t, u_int16_t,
    struct txp_rsp_desc **));
void txp_rsp_fixup __P((struct txp_softc *, struct txp_rsp_desc *));

void txp_ifmedia_sts __P((struct ifnet *, struct ifmediareq *));
int txp_ifmedia_upd __P((struct ifnet *));
void txp_show_descriptor __P((void *));

struct cfattach txp_ca = {
	sizeof(struct txp_softc), txp_probe, txp_attach,
};

struct cfdriver txp_cd = {
	0, "txp", DV_IFNET
};

int
txp_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_3COM)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_3COM_3CR990TX95:
	case PCI_PRODUCT_3COM_3CR990TX97:
	case PCI_PRODUCT_3COM_3CR990SVR95:
	case PCI_PRODUCT_3COM_3CR990SVR97:
		return (1);
	}

	return (0);
}

void
txp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct txp_softc *sc = (struct txp_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_size_t iosize;
	u_int32_t command;
	u_int16_t p1;
	u_int32_t p2;

	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		return;
	}

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}
	if (pci_mapreg_map(pa, TXP_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_bt, &sc->sc_bh, NULL, &iosize)) {
		printf(": can't map mem space %d\n", 0);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, txp_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	if (txp_chip_init(sc))
		return;

	if (txp_download_fw(sc))
		return;

	if (txp_alloc_rings(sc))
		return;

	if (txp_command(sc, TXP_CMD_STATION_ADDRESS_READ, 0, 0, 0,
	    &p1, &p2, NULL, 1))
		return;

	sc->sc_arpcom.ac_enaddr[0] = ((u_int8_t *)&p1)[1];
	sc->sc_arpcom.ac_enaddr[1] = ((u_int8_t *)&p1)[0];
	sc->sc_arpcom.ac_enaddr[2] = ((u_int8_t *)&p2)[3];
	sc->sc_arpcom.ac_enaddr[3] = ((u_int8_t *)&p2)[2];
	sc->sc_arpcom.ac_enaddr[4] = ((u_int8_t *)&p2)[1];
	sc->sc_arpcom.ac_enaddr[5] = ((u_int8_t *)&p2)[0];

	printf(" address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	ifmedia_init(&sc->sc_ifmedia, 0, txp_ifmedia_upd, txp_ifmedia_sts);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO);
	txp_command(sc, TXP_CMD_XCVR_SELECT, TXP_XCVR_AUTO, 0, 0,
	    NULL, NULL, NULL, 0);

	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = txp_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = txp_start;
	ifp->if_watchdog = txp_watchdog;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Attach us everywhere
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	shutdownhook_establish(txp_shutdown, sc);
}

int
txp_chip_init(sc)
	struct txp_softc *sc;
{
	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	if (txp_reset_adapter(sc))
		return (-1);

	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	return (0);
}

int
txp_reset_adapter(sc)
	struct txp_softc *sc;
{
	u_int32_t r;
	int i;

	WRITE_REG(sc, TXP_SRR, TXP_SRR_ALL);
	DELAY(1000);
	WRITE_REG(sc, TXP_SRR, 0);

	/* Should wait max 6 seconds */
	for (i = 0; i < 6000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(1000);
	}

	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		printf(": reset hung\n");
		return (-1);
	}

	return (0);
}

int
txp_download_fw(sc)
	struct txp_softc *sc;
{
	struct txp_fw_file_header *fileheader;
	struct txp_fw_section_header *secthead;
	int sect;
	u_int32_t r, i, ier, imr;

	ier = READ_REG(sc, TXP_IER);
	WRITE_REG(sc, TXP_IER, ier | TXP_INT_A2H_0);

	imr = READ_REG(sc, TXP_IMR);
	WRITE_REG(sc, TXP_IMR, imr | TXP_INT_A2H_0);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		printf(": not waiting for host request\n");
		return (-1);
	}

	/* Ack the status */
	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	fileheader = (struct txp_fw_file_header *)TyphoonImage;
	if (strncmp("TYPHOON", fileheader->magicid, sizeof(fileheader->magicid))) {
		printf(": fw invalid magic\n");
		return (-1);
	}

	/* Tell boot firmware to get ready for image */
	WRITE_REG(sc, TXP_H2A_1, fileheader->addr);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_RUNTIME_IMAGE);

	if (txp_download_fw_wait(sc)) {
		printf(": fw wait failed, initial\n");
		return (-1);
	}

	secthead = (struct txp_fw_section_header *)(TyphoonImage +
	    sizeof(struct txp_fw_file_header));

	for (sect = 0; sect < fileheader->nsections; sect++) {
		if (txp_download_fw_section(sc, secthead, sect))
			return (-1);
		secthead = (struct txp_fw_section_header *)
		    (((u_int8_t *)secthead) + secthead->nbytes + sizeof(*secthead));
	}

	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_DOWNLOAD_COMPLETE);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_BOOT) {
		printf(": not waiting for boot\n");
		return (-1);
	}

	WRITE_REG(sc, TXP_IER, ier);
	WRITE_REG(sc, TXP_IMR, imr);

	return (0);
}

int
txp_download_fw_wait(sc)
	struct txp_softc *sc;
{
	u_int32_t i, r;

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_ISR);
		if (r & TXP_INT_A2H_0)
			break;
		DELAY(50);
	}

	if (!(r & TXP_INT_A2H_0)) {
		printf(": fw wait failed comm0\n", sc->sc_dev.dv_xname);
		return (-1);
	}

	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	r = READ_REG(sc, TXP_A2H_0);
	if (r != STAT_WAITING_FOR_SEGMENT) {
		printf(": fw not waiting for segment\n", sc->sc_dev.dv_xname);
		return (-1);
	}
	return (0);
}

int
txp_download_fw_section(sc, sect, sectnum)
	struct txp_softc *sc;
	struct txp_fw_section_header *sect;
	int sectnum;
{
	struct txp_dma_alloc dma;
	int rseg, err = 0;
	struct mbuf m;
	u_int16_t csum;

	/* Skip zero length sections */
	if (sect->nbytes == 0)
		return (0);

	/* Make sure we aren't past the end of the image */
	rseg = ((u_int8_t *)sect) - ((u_int8_t *)TyphoonImage);
	if (rseg >= sizeof(TyphoonImage)) {
		printf(": fw invalid section address, section %d\n", sectnum);
		return (-1);
	}

	/* Make sure this section doesn't go past the end */
	rseg += sect->nbytes;
	if (rseg >= sizeof(TyphoonImage)) {
		printf(": fw truncated section %d\n", sectnum);
		return (-1);
	}

	/* map a buffer, copy segment to it, get physaddr */
	if (txp_dma_malloc(sc, sect->nbytes, &dma)) {
		printf(": fw dma malloc failed, section %d\n", sectnum);
		return (-1);
	}

	bcopy(((u_int8_t *)sect) + sizeof(*sect), dma.dma_vaddr, sect->nbytes);

	/*
	 * dummy up mbuf and verify section checksum
	 */
	m.m_type = MT_DATA;
	m.m_next = m.m_nextpkt = NULL;
	m.m_len = sect->nbytes;
	m.m_data = dma.dma_vaddr;
	m.m_flags = 0;
	csum = in_cksum(&m, sect->nbytes);
	if (csum != sect->cksum) {
		printf(": fw section %d, bad cksum (expected 0x%x got 0x%x)\n",
		    sectnum, sect->cksum, csum);
		err = -1;
		goto bail;
	}

	bus_dmamap_sync(sc->sc_dmat, dma.dma_map,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	WRITE_REG(sc, TXP_H2A_1, sect->nbytes);
	WRITE_REG(sc, TXP_H2A_2, sect->cksum);
	WRITE_REG(sc, TXP_H2A_3, sect->addr);
	WRITE_REG(sc, TXP_H2A_4, dma.dma_paddr >> 32);
	WRITE_REG(sc, TXP_H2A_5, dma.dma_paddr & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_SEGMENT_AVAILABLE);

	if (txp_download_fw_wait(sc)) {
		printf(": fw wait failed, section %d\n", sectnum);
		err = -1;
		goto bail;
	}

	bus_dmamap_sync(sc->sc_dmat, dma.dma_map,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

bail:
	txp_dma_free(sc, &dma);

	return (err);
}

int
txp_intr(vsc)
	void *vsc;
{
	int claimed = 0;

	return (claimed);
}


void
txp_shutdown(vsc)
	void *vsc;
{
	struct txp_softc *sc = (struct txp_softc *)vsc;

	txp_stop(sc);
}

int
txp_alloc_rings(sc)
	struct txp_softc *sc;
{
	struct txp_boot_record *boot;
	u_int32_t r;
	int i;

	/* boot record */
	if (txp_dma_malloc(sc, sizeof(struct txp_boot_record), &sc->sc_boot_dma)) {
		printf(": can't allocate boot record\n");
		return (-1);
	}
	boot = (struct txp_boot_record *)sc->sc_boot_dma.dma_vaddr;
	bzero(boot, sizeof(*boot));
	sc->sc_boot = boot;

	/* host variables */
	if (txp_dma_malloc(sc, sizeof(struct txp_hostvar), &sc->sc_host_dma)) {
		printf(": can't allocate host ring\n");
		goto bail_boot;
	}
	bzero(sc->sc_host_dma.dma_vaddr, sc->sc_host_dma.dma_siz);
	boot->br_hostvar_lo = sc->sc_host_dma.dma_paddr & 0xffffffff;
	boot->br_hostvar_hi = sc->sc_host_dma.dma_paddr >> 32;
	sc->sc_hostvar = (struct txp_hostvar *)sc->sc_host_dma.dma_vaddr;

	/* high priority tx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_tx_desc) * TX_ENTRIES,
	    &sc->sc_txhiring_dma)) {
		printf(": can't allocate high tx ring\n");
		goto bail_host;
	}
	bzero(sc->sc_txhiring_dma.dma_vaddr, sc->sc_txhiring_dma.dma_siz);
	boot->br_txhipri_lo = sc->sc_txhiring_dma.dma_paddr & 0xffffffff;
	boot->br_txhipri_hi = sc->sc_txhiring_dma.dma_paddr >> 32;
	boot->br_txhipri_siz = TX_ENTRIES * sizeof(struct txp_tx_desc);

	/* low priority tx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_tx_desc) * TX_ENTRIES,
	    &sc->sc_txloring_dma)) {
		printf(": can't allocate low tx ring\n");
		goto bail_txhiring;
	}
	bzero(sc->sc_txloring_dma.dma_vaddr, sc->sc_txloring_dma.dma_siz);
	boot->br_txlopri_lo = sc->sc_txloring_dma.dma_paddr & 0xffffffff;
	boot->br_txlopri_hi = sc->sc_txloring_dma.dma_paddr >> 32;
	boot->br_txlopri_siz = TX_ENTRIES * sizeof(struct txp_tx_desc);

	/* high priority rx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rx_desc) * RX_ENTRIES,
	    &sc->sc_rxhiring_dma)) {
		printf(": can't allocate high rx ring\n");
		goto bail_txloring;
	}
	bzero(sc->sc_rxhiring_dma.dma_vaddr, sc->sc_rxhiring_dma.dma_siz);
	boot->br_rxhipri_lo = sc->sc_rxhiring_dma.dma_paddr & 0xffffffff;
	boot->br_rxhipri_hi = sc->sc_rxhiring_dma.dma_paddr >> 32;
	boot->br_rxhipri_siz = RX_ENTRIES * sizeof(struct txp_rx_desc);

	/* low priority rx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rx_desc) * RX_ENTRIES,
	    &sc->sc_rxloring_dma)) {
		printf(": can't allocate low rx ring\n");
		goto bail_rxhiring;
	}
	bzero(sc->sc_rxloring_dma.dma_vaddr, sc->sc_rxloring_dma.dma_siz);
	boot->br_rxlopri_lo = sc->sc_rxloring_dma.dma_paddr & 0xffffffff;
	boot->br_rxlopri_hi = sc->sc_rxloring_dma.dma_paddr >> 32;
	boot->br_rxlopri_siz = RX_ENTRIES * sizeof(struct txp_rx_desc);

	/* command ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_cmd_desc) * CMD_ENTRIES,
	    &sc->sc_cmdring_dma)) {
		printf(": can't allocate command ring\n");
		goto bail_rxloring;
	}
	bzero(sc->sc_cmdring_dma.dma_vaddr, sc->sc_cmdring_dma.dma_siz);
	boot->br_cmd_lo = sc->sc_cmdring_dma.dma_paddr & 0xffffffff;
	boot->br_cmd_hi = sc->sc_cmdring_dma.dma_paddr >> 32;
	boot->br_cmd_siz = CMD_ENTRIES * sizeof(struct txp_cmd_desc);
	sc->sc_cmdring.base = (struct txp_cmd_desc *)sc->sc_cmdring_dma.dma_vaddr;
	sc->sc_cmdring.size = CMD_ENTRIES * sizeof(struct txp_cmd_desc);
	sc->sc_cmdring.lastwrite = 0;

	/* response ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rsp_desc) * RSP_ENTRIES,
	    &sc->sc_rspring_dma)) {
		printf(": can't allocate response ring\n");
		goto bail_cmdring;
	}
	bzero(sc->sc_rspring_dma.dma_vaddr, sc->sc_rspring_dma.dma_siz);
	boot->br_resp_lo = sc->sc_rspring_dma.dma_paddr & 0xffffffff;
	boot->br_resp_hi = sc->sc_rspring_dma.dma_paddr >> 32;
	boot->br_resp_siz = CMD_ENTRIES * sizeof(struct txp_rsp_desc);
	sc->sc_rspring.base = (struct txp_rsp_desc *)sc->sc_rspring_dma.dma_vaddr;
	sc->sc_rspring.size = RSP_ENTRIES * sizeof(struct txp_rsp_desc);
	sc->sc_rspring.lastwrite = 0;

	/* zero dma */
	if (txp_dma_malloc(sc, sizeof(u_int32_t), &sc->sc_zero_dma)) {
		printf(": can't allocate response ring\n");
		goto bail_rspring;
	}
	bzero(sc->sc_zero_dma.dma_vaddr, sc->sc_zero_dma.dma_siz);
	boot->br_zero_lo = sc->sc_zero_dma.dma_paddr & 0xffffffff;
	boot->br_zero_hi = sc->sc_zero_dma.dma_paddr >> 32;

	/* See if it's waiting for boot, and try to boot it */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_BOOT) {
		printf(": not waiting for boot\n");
		goto bail;
	}
	WRITE_REG(sc, TXP_H2A_2, sc->sc_boot_dma.dma_paddr >> 32);
	WRITE_REG(sc, TXP_H2A_1, sc->sc_boot_dma.dma_paddr & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_REGISTER_BOOT_RECORD);

	/* See if it booted */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_RUNNING)
			break;
		DELAY(50);
	}
	if (r != STAT_RUNNING) {
		printf(": fw not running\n");
		goto bail;
	}

	/* Clear TX and CMD ring write registers */
	WRITE_REG(sc, TXP_H2A_1, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_2, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_3, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_NULL);

	return (0);

bail:
	txp_dma_free(sc, &sc->sc_zero_dma);
bail_rspring:
	txp_dma_free(sc, &sc->sc_rspring_dma);
bail_cmdring:
	txp_dma_free(sc, &sc->sc_cmdring_dma);
bail_rxloring:
	txp_dma_free(sc, &sc->sc_rxloring_dma);
bail_rxhiring:
	txp_dma_free(sc, &sc->sc_rxhiring_dma);
bail_txloring:
	txp_dma_free(sc, &sc->sc_txloring_dma);
bail_txhiring:
	txp_dma_free(sc, &sc->sc_txhiring_dma);
bail_host:
	txp_dma_free(sc, &sc->sc_host_dma);
bail_boot:
	txp_dma_free(sc, &sc->sc_boot_dma);
	return (-1);
}

int
txp_dma_malloc(sc, size, dma)
	struct txp_softc *sc;
	bus_size_t size;
	struct txp_dma_alloc *dma;
{
	int r;

	if ((r = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &dma->dma_seg, 1, &dma->dma_nseg, BUS_DMA_NOWAIT)) != 0) {
		return (r);
	}
	if (dma->dma_nseg != 1) {
		bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
		return (-1);
	}

	if ((r = bus_dmamem_map(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg,
	    size, &dma->dma_vaddr, BUS_DMA_NOWAIT)) != 0) {
		bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
		return (r);
	}

	if ((r = bus_dmamap_create(sc->sc_dmat, size, dma->dma_nseg,
	    size, 0, BUS_DMA_NOWAIT, &dma->dma_map)) != 0) {
		bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, size);
		bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
		return (r);
	}

	if ((r = bus_dmamap_load(sc->sc_dmat, dma->dma_map, dma->dma_vaddr,
	    size, NULL, BUS_DMA_NOWAIT)) != 0) {
		bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
		bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, size);
		bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
		return (r);
	}

	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	dma->dma_siz = size;

	return (0);
}

void
txp_dma_free(sc, dma)
	struct txp_softc *sc;
	struct txp_dma_alloc *dma;
{
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, dma->dma_siz);
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
}

void
txp_tick(vsc)
	void *vsc;
{
	struct txp_softc *sc = vsc;

	timeout_add(&sc->sc_tick_tmo, hz);
}

int
txp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			txp_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
		default:
			txp_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			txp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				txp_stop(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			/* XXX TODO: set multicast list */
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

void
txp_init(sc)
	struct txp_softc *sc;
{
}

void
txp_start(ifp)
	struct ifnet *ifp;
{
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		/* XXX enqueue and send the packet */
		m_freem(m);
	}
}

/*
 * XXX this needs to have a callback mechanism
 */
int
txp_command(sc, id, in1, in2, in3, out1, out2, out3, wait)
	struct txp_softc *sc;
	u_int16_t id, in1, *out1;
	u_int32_t in2, in3, *out2, *out3;
	int wait;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_cmd_desc *cmd;
	struct txp_rsp_desc *rsp = NULL;
	u_int32_t idx, i;
	u_int16_t seq;

	if (txp_cmd_desc_numfree(sc) == 0) {
		printf(": no free cmd descriptors\n");
		return (-1);
	}

	idx = sc->sc_cmdring.lastwrite;
	cmd = (struct txp_cmd_desc *)(((u_int8_t *)sc->sc_cmdring.base) + idx);
	bzero(cmd, sizeof(*cmd));

	cmd->cmd_numdesc = 0;
	cmd->cmd_seq = seq = sc->sc_seq++;
	cmd->cmd_id = id;
	cmd->cmd_par1 = in1;
	cmd->cmd_par2 = in2;
	cmd->cmd_par3 = in3;
	cmd->cmd_flags = CMD_FLAGS_TYPE_CMD |
	    (wait ? CMD_FLAGS_RESP : 0) | CMD_FLAGS_VALID;

	txp_show_descriptor(cmd);

	idx += sizeof(struct txp_cmd_desc);
	if (idx == sc->sc_cmdring.size)
		idx = 0;
	sc->sc_cmdring.lastwrite = idx;

	WRITE_REG(sc, TXP_H2A_2, sc->sc_cmdring.lastwrite);

	if (!wait)
		return (0);

	for (i = 0; i < 10000; i++) {
		idx = hv->hv_resp_read_idx;
		if (idx != hv->hv_resp_write_idx) {
			rsp = NULL;
			if (txp_response(sc, idx, cmd->cmd_id, seq, &rsp))
				return (-1);
			if (rsp != NULL)
				break;
		}
		DELAY(50);
	}
	if (i == 1000 || rsp == NULL) {
		printf(": command failed\n");
		return (-1);
	}

	if (out1 != NULL)
		*out1 = rsp->rsp_par1;
	if (out2 != NULL)
		*out2 = rsp->rsp_par2;
	if (out3 != NULL)
		*out3 = rsp->rsp_par3;

	idx += sizeof(struct txp_rsp_desc);
	if (idx == sc->sc_rspring.size)
		idx = 0;
	sc->sc_rspring.lastwrite = hv->hv_resp_read_idx = idx;

	return (0);
}

int
txp_response(sc, ridx, id, seq, rspp)
	struct txp_softc *sc;
	u_int32_t ridx;
	u_int16_t id;
	u_int16_t seq;
	struct txp_rsp_desc **rspp;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_rsp_desc *rsp;

	while (ridx != hv->hv_resp_write_idx) {
		rsp = (struct txp_rsp_desc *)(((u_int8_t *)sc->sc_rspring.base) + ridx);

		txp_show_descriptor(rsp);

		if (id == rsp->rsp_id && rsp->rsp_seq == seq) {
			*rspp = rsp;
			return (0);
		}

		if (rsp->rsp_flags & RSP_FLAGS_ERROR) {
			printf(": response error!\n");
			txp_rsp_fixup(sc, rsp);
			ridx = hv->hv_resp_read_idx;
			continue;
		}

		switch (rsp->rsp_id) {
		case TXP_CMD_CYCLE_STATISTICS:
			printf(": stats\n");
			break;
		case TXP_CMD_MEDIA_STATUS_READ:
			break;
		case TXP_CMD_HELLO_RESPONSE:
			printf(": hello\n");
			break;
		default:
			printf(": unknown id(0x%x)\n", rsp->rsp_id);
		}

		txp_rsp_fixup(sc, rsp);
		ridx = hv->hv_resp_read_idx;
		hv->hv_resp_read_idx = ridx;
	}

	return (0);
}

void
txp_rsp_fixup(sc, rsp)
	struct txp_softc *sc;
	struct txp_rsp_desc *rsp;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	u_int32_t i, ridx;

	ridx = hv->hv_resp_read_idx;

	for (i = 0; i < rsp->rsp_numdesc + 1; i++) {
		ridx += sizeof(struct txp_rsp_desc);
		if (ridx == sc->sc_rspring.size)
			ridx = 0;
		sc->sc_rspring.lastwrite = hv->hv_resp_read_idx = ridx;
	}
	
	hv->hv_resp_read_idx = ridx;
}

int
txp_cmd_desc_numfree(sc)
	struct txp_softc *sc;
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_boot_record *br = sc->sc_boot;
	u_int32_t widx, ridx, nfree;

	widx = sc->sc_cmdring.lastwrite;
	ridx = hv->hv_cmd_read_idx;

	if (widx == ridx) {
		/* Ring is completely free */
		nfree = br->br_cmd_siz - sizeof(struct txp_cmd_desc);
	} else {
		if (widx > ridx)
			nfree = br->br_cmd_siz -
			    (widx - ridx + sizeof(struct txp_cmd_desc));
		else
			nfree = ridx - widx - sizeof(struct txp_cmd_desc);
	}

	return (nfree / sizeof(struct txp_cmd_desc));
}

void
txp_stop(sc)
	struct txp_softc *sc;
{
}

void
txp_watchdog(ifp)
	struct ifnet *ifp;
{
}

int
txp_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t new_xcvr;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_10_T) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_10_FDX;
		else
			new_xcvr = TXP_XCVR_10_HDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_100_FDX;
		else
			new_xcvr = TXP_XCVR_100_HDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		new_xcvr = TXP_XCVR_AUTO;
	}

	/* nothing to do */
	if (sc->sc_xcvr == new_xcvr)
		return (0);

	txp_command(sc, TXP_CMD_XCVR_SELECT, new_xcvr, 0, 0,
	    NULL, NULL, NULL, 0);
	sc->sc_xcvr = new_xcvr;

	return (0);
}

void
txp_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t bmsr, bmcr, anlpar;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;
	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMCR, 0,
	    &bmcr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_ANLPAR, 0,
	    &anlpar, NULL, NULL, 1))
		goto bail;

	if (bmsr & BMSR_LINK)
		ifmr->ifm_status |= IFM_ACTIVE;

	if (bmcr & BMCR_ISO) {
		ifmr->ifm_active |= IFM_NONE;
		ifmr->ifm_status = 0;
		printf("isolated!\n");
		return;
	}

	if (bmcr & BMCR_LOOP)
		ifmr->ifm_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			ifmr->ifm_active |= IFM_NONE;
			printf("acomp!\n");
			return;
		}

		if (anlpar & ANLPAR_T4)
			ifmr->ifm_active |= IFM_100_T4;
		else if (anlpar & ANLPAR_TX_FD)
			ifmr->ifm_active |= IFM_100_TX|IFM_FDX;
		else if (anlpar & ANLPAR_TX)
			ifmr->ifm_active |= IFM_100_TX;
		else if (anlpar & ANLPAR_10_FD)
			ifmr->ifm_active |= IFM_10_T|IFM_FDX;
		else if (anlpar & ANLPAR_10)
			ifmr->ifm_active |= IFM_10_T;
		else
			ifmr->ifm_active |= IFM_NONE;
	} else
		ifmr->ifm_active = ifm->ifm_cur->ifm_media;
	return;

bail:
	ifmr->ifm_active |= IFM_NONE;
	ifmr->ifm_status &= ~IFM_AVALID;
}

void
txp_show_descriptor(d)
	void *d;
{
#if 0
	struct txp_cmd_desc *cmd = d;
	struct txp_rsp_desc *rsp = d;

	switch (cmd->cmd_flags & CMD_FLAGS_TYPE_M) {
	case CMD_FLAGS_TYPE_CMD:
		/* command descriptor */
		printf("[cmd flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags, cmd->cmd_numdesc, cmd->cmd_id, cmd->cmd_seq,
		    cmd->cmd_par1, cmd->cmd_par2, cmd->cmd_par3);
		break;
	case CMD_FLAGS_TYPE_RESP:
		/* response descriptor */
		printf("[rsp flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    rsp->rsp_flags, rsp->rsp_numdesc, rsp->rsp_id, rsp->rsp_seq,
		    rsp->rsp_par1, rsp->rsp_par2, rsp->rsp_par3);
		break;
	default:
		printf("[unknown(%x) flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags & CMD_FLAGS_TYPE_M,
		    cmd->cmd_flags, cmd->cmd_numdesc, cmd->cmd_id, cmd->cmd_seq,
		    cmd->cmd_par1, cmd->cmd_par2, cmd->cmd_par3);
		break;
	}
#endif
}

/*	$OpenBSD: if_txp.c,v 1.2 2001/04/08 05:28:49 jason Exp $	*/

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
#include <dev/pci/typhoon_image.h>

int txp_probe	__P((struct device *, void *, void *));
void txp_attach	__P((struct device *, struct device *, void *));
int txp_intr	__P((void *));
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

	printf("\n");

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

	WRITE_REG(sc, TXP_SRR, 0x7f);
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

	/* Tell boot firmware to get ready for image */
	WRITE_REG(sc, TXP_H2A_1, fileheader->addr);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_RUNTIME_IMAGE);

	fileheader = (struct txp_fw_file_header *)TyphoonImage;
	if (strncmp("TYPHOON", fileheader->magicid, sizeof(fileheader->magicid))) {
		printf(": fw invalid magic\n");
		return (-1);
	}

	secthead = (struct txp_fw_section_header *)(TyphoonImage +
	    sizeof(struct txp_fw_file_header));

	if (txp_download_fw_wait(sc)) {
		printf(": fw wait failed, initial\n", sect);
		return (-1);
	}

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
	u_int64_t pa;
	bus_dma_tag_t dmat = sc->sc_dmat;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	int rseg, err = 0;
	caddr_t kva;

	/* Skip zero length sections */
	if (sect->nbytes == 0)
		return (0);

	/* map a buffer, copy segment to it, get physaddr */
	if (bus_dmamem_alloc(dmat, sect->nbytes, PAGE_SIZE, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) {
		printf(": fw dmamam alloc fail\n");
		return (-1);
	}
	if (bus_dmamem_map(dmat, &seg, rseg, sect->nbytes, &kva,
	    BUS_DMA_NOWAIT)) {
		printf(": fw dmamem map fail\n");
		err = -1;
		goto bail_free;
	}
	if (bus_dmamap_create(dmat, sect->nbytes, 1, sect->nbytes, 0,
	    BUS_DMA_NOWAIT, &dmamap)) {
		printf(": fw dmamap create fail\n");
		err = -1;
		goto bail_unmap;
	}
	if (bus_dmamap_load(dmat, dmamap, kva, sect->nbytes, NULL,
	    BUS_DMA_NOWAIT)) {
		printf(": fw dmamap load fail\n");
		err = -1;
		goto bail_destroy;
	}

	bcopy(((u_int8_t *)sect) + sizeof(*sect), kva, sect->nbytes);

	bus_dmamap_sync(dmat, dmamap,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	pa = dmamap->dm_segs[0].ds_addr;

	WRITE_REG(sc, TXP_H2A_1, sect->nbytes);
	WRITE_REG(sc, TXP_H2A_2, sect->cksum);
	WRITE_REG(sc, TXP_H2A_3, sect->addr);
	WRITE_REG(sc, TXP_H2A_4, pa >> 32);
	WRITE_REG(sc, TXP_H2A_5, pa & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_SEGMENT_AVAILABLE);

	if (txp_download_fw_wait(sc)) {
		printf(": fw wait failed, section %d\n", sectnum);
		err = -1;
	}


	bus_dmamap_sync(dmat, dmamap,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(dmat, dmamap);
bail_destroy:
	bus_dmamap_destroy(dmat, dmamap);
bail_unmap:
	bus_dmamem_unmap(dmat, kva, sect->nbytes);
bail_free:
	bus_dmamem_free(dmat, &seg, rseg);

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
	struct ifreq *ifr = (struct ifreq *) data;
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

struct cfattach txp_ca = {
	sizeof(struct txp_softc), txp_probe, txp_attach,
};

struct cfdriver txp_cd = {
	0, "txp", DV_IFNET
};

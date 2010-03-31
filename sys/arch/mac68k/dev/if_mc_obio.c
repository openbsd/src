/*	$OpenBSD: if_mc_obio.c,v 1.9 2010/03/31 19:46:27 miod Exp $	*/
/*	$NetBSD: if_mc_obio.c,v 1.13 2004/03/26 12:15:46 wiz Exp $	*/

/*-
 * Copyright (c) 1997 David Huang <khym@azeotrope.org>
 * All rights reserved.
 *
 * Portions of this code are based on code by Denton Gentry <denny1@home.com>
 * and Yanagisawa Takeshi <yanagisw@aa.ap.titech.ac.jp>.
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
 *
 */

/*
 * Bus attachment and DMA routines for the mc driver (Centris/Quadra
 * 660av and Quadra 840av onboard ethernet, based on the AMD Am79C940
 * MACE ethernet chip). Also uses the PSC (Peripheral Subsystem
 * Controller) for DMA to and from the MACE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/psc.h>

#include <mac68k/dev/obiovar.h>
#include <mac68k/dev/if_mcreg.h>
#include <mac68k/dev/if_mcvar.h>

#define MACE_REG_BASE	0x50F1C000
#define MACE_PROM_BASE	0x50F08000

int	mc_obio_match(struct device *, void *, void *);
void	mc_obio_attach(struct device *, struct device *, void *);
void	mc_obio_init(struct mc_softc *sc);
void	mc_obio_put(struct mc_softc *sc, u_int len);
int	mc_dmaintr(void *arg);
void	mc_reset_rxdma(struct mc_softc *sc);
void	mc_reset_rxdma_set(struct mc_softc *, int set);
void	mc_reset_txdma(struct mc_softc *sc);
int	mc_obio_getaddr(struct mc_softc *, u_int8_t *);

struct cfattach mc_obio_ca = {
	sizeof(struct mc_softc), mc_obio_match, mc_obio_attach
};

int
mc_obio_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct obio_attach_args *oa = aux;
	bus_space_handle_t bsh;
	int found = 0;

        if (current_mac_model->class != MACH_CLASSAV)
		return 0;

	if (bus_space_map(oa->oa_tag, MACE_REG_BASE, MC_REGSIZE, 0, &bsh))
		return 0;

	/*
	 * Make sure the MACE's I/O space is readable, and if it is,
	 * try to read the CHIPID register. A MACE will always have
	 * 0x?940, where the ? depends on the chip version.
	 */
	if (mac68k_bus_space_probe(oa->oa_tag, bsh, 0, 1)) {
		if ((bus_space_read_1(
			oa->oa_tag, bsh, MACE_REG(MACE_CHIPIDL)) == 0x40) &&
		    ((bus_space_read_1(
			oa->oa_tag, bsh, MACE_REG(MACE_CHIPIDH)) & 0xf) == 9))
			found = 1;
	}

	bus_space_unmap(oa->oa_tag, bsh, MC_REGSIZE);

	return found;
}

void
mc_obio_attach(parent, self, aux)
	struct device *parent, *self;
	void	*aux;
{
	struct obio_attach_args *oa = (struct obio_attach_args *)aux;
	struct mc_softc *sc = (void *)self;
	struct pglist rxlist, txlist;
	vm_page_t pg;
	vaddr_t va;
	u_int8_t myaddr[ETHER_ADDR_LEN];

	sc->sc_regt = oa->oa_tag;
	sc->sc_biucc = XMTSP_64;
	sc->sc_fifocc = XMTFW_16 | RCVFW_64 | XMTFWU | RCVFWU |
	    XMTBRST | RCVBRST;
	sc->sc_plscc = PORTSEL_AUI;

	if (bus_space_map(sc->sc_regt, MACE_REG_BASE, MC_REGSIZE, 0,
	    &sc->sc_regh)) {
		printf(": failed to map space for MACE regs.\n");
		return;
	}

	if (mc_obio_getaddr(sc, myaddr)) {
		printf(": failed to get MAC address.\n");
		goto out1;
	}

	/* allocate memory for transmit buffer and mark it non-cacheable */
	TAILQ_INIT(&txlist);
	if (uvm_pglistalloc(PAGE_SIZE, 0, -1, PAGE_SIZE, 0,
	    &txlist, 1, UVM_PLA_NOWAIT) != 0) {
		printf(": could not allocate transmit buffer memory\n");
		goto out1;
	}
	sc->sc_txbuf = (u_char *)uvm_km_valloc(kernel_map, PAGE_SIZE);
	if (sc->sc_txbuf == NULL) {
		printf(": can't map transmit buffer memory\n");
		goto out2;
	}
	pg = TAILQ_FIRST(&txlist);
	sc->sc_txbuf_phys = VM_PAGE_TO_PHYS(pg);
	pmap_enter_cache(pmap_kernel(), (vaddr_t)sc->sc_txbuf,
	    sc->sc_txbuf_phys, UVM_PROT_RW, UVM_PROT_RW | PMAP_WIRED, PG_CI);
	pmap_update(pmap_kernel());

	/*
	 * allocate memory for receive buffer and mark it non-cacheable
	 */
	TAILQ_INIT(&rxlist);
	if (uvm_pglistalloc(MC_NPAGES * PAGE_SIZE, 0, -1, PAGE_SIZE, 0,
	    &rxlist, 1, UVM_PLA_NOWAIT) != 0) {
		printf(": could not allocate receive buffer memory\n");
		goto out3;
	}
	sc->sc_rxbuf = (u_char *)(va = uvm_km_valloc(kernel_map,
	    MC_NPAGES * PAGE_SIZE));
	if (sc->sc_rxbuf == NULL) {
		printf(": can't map receive buffer memory\n");
		goto out4;
	}
	pg = TAILQ_FIRST(&rxlist);
	sc->sc_rxbuf_phys = VM_PAGE_TO_PHYS(pg);
	TAILQ_FOREACH(pg, &rxlist, pageq) {
		pmap_enter_cache(pmap_kernel(), va, VM_PAGE_TO_PHYS(pg),
		    UVM_PROT_RW, UVM_PROT_RW | PMAP_WIRED, PG_CI);
		va += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	sc->sc_bus_init = mc_obio_init;
	sc->sc_putpacket = mc_obio_put;

	/* disable receive DMA */
	psc_reg2(PSC_ENETRD_CTL) = 0x8800;
	psc_reg2(PSC_ENETRD_CTL) = 0x1000;
	psc_reg2(PSC_ENETRD_CMD + PSC_SET0) = 0x1100;
	psc_reg2(PSC_ENETRD_CMD + PSC_SET1) = 0x1100;

	/* disable transmit DMA */
	psc_reg2(PSC_ENETWR_CTL) = 0x8800;
	psc_reg2(PSC_ENETWR_CTL) = 0x1000;
	psc_reg2(PSC_ENETWR_CMD + PSC_SET0) = 0x1100;
	psc_reg2(PSC_ENETWR_CMD + PSC_SET1) = 0x1100;

	/* install interrupt handlers */
	add_psc_lev4_intr(PSCINTR_ENET_DMA, mc_dmaintr, sc);
	add_psc_lev3_intr(mcintr, sc);

	/* enable MACE DMA interrupts */
	psc_reg1(PSC_LEV4_IER) = 0x80 | (1 << PSCINTR_ENET_DMA);

	/* don't know what this does */
	psc_reg2(PSC_ENETWR_CTL) = 0x9000;
	psc_reg2(PSC_ENETRD_CTL) = 0x9000;
	psc_reg2(PSC_ENETWR_CTL) = 0x0400;
	psc_reg2(PSC_ENETRD_CTL) = 0x0400;

	/* enable MACE interrupts */
	psc_reg1(PSC_LEV3_IER) = 0x80 | (1 << PSCINTR_ENET);

	/* mcsetup returns 1 if something fails */
	if (mcsetup(sc, myaddr) != 0)
		goto out5;

	return;

out5:
	/* disable interrupts */
	psc_reg1(PSC_LEV4_IER) = (1 << PSCINTR_ENET_DMA);
	psc_reg1(PSC_LEV3_IER) = (1 << PSCINTR_ENET);
	/* remove interrupt handlers */
	remove_psc_lev4_intr(PSCINTR_ENET_DMA);
	remove_psc_lev3_intr();
	pmap_remove(pmap_kernel(), (vaddr_t)sc->sc_rxbuf,
	    (vaddr_t)sc->sc_rxbuf + MC_NPAGES * PAGE_SIZE);
	pmap_update(pmap_kernel());
out4:
	uvm_pglistfree(&rxlist);
out3:
	pmap_remove(pmap_kernel(), (vaddr_t)sc->sc_txbuf,
	    (vaddr_t)sc->sc_txbuf + PAGE_SIZE);
	pmap_update(pmap_kernel());
out2:
	uvm_pglistfree(&txlist);
out1:
	bus_space_unmap(sc->sc_regt, sc->sc_regh, MC_REGSIZE);
}

/* Bus-specific initialization */
void
mc_obio_init(sc)
	struct mc_softc *sc;
{
	mc_reset_rxdma(sc);
	mc_reset_txdma(sc);
}

void
mc_obio_put(sc, len)
	struct mc_softc *sc;
	u_int len;
{
	psc_reg4(PSC_ENETWR_ADDR + sc->sc_txset) = sc->sc_txbuf_phys;
	psc_reg4(PSC_ENETWR_LEN + sc->sc_txset) = len;
	psc_reg2(PSC_ENETWR_CMD + sc->sc_txset) = 0x9800;

	sc->sc_txset ^= 0x10;
}

/*
 * Interrupt handler for the MACE DMA completion interrupts
 */
int
mc_dmaintr(arg)
	void *arg;
{
	struct mc_softc *sc = arg;
	u_int16_t status;
	u_int32_t bufsleft, which;
	int head;

	/*
	 * Not sure what this does... figure out if this interrupt is
	 * really ours?
	 */
	while ((which = psc_reg4(0x804)) != psc_reg4(0x804))
		;
	if ((which & 0x60000000) == 0)
		return 0;

	/* Get the read channel status */
	status = psc_reg2(PSC_ENETRD_CTL);
	if (status & 0x2000) {
		/* I think this is an exceptional condition. Reset the DMA */
		mc_reset_rxdma(sc);
#ifdef MCDEBUG
		printf("%s: resetting receive DMA channel (status 0x%04x)\n",
		    sc->sc_dev.dv_xname, status);
#endif
	} else if (status & 0x100) {
		/* We've received some packets from the MACE */
		int offset;

		/* Clear the interrupt */
		psc_reg2(PSC_ENETRD_CMD + sc->sc_rxset) = 0x1100;

		/* See how may receive buffers are left */
		bufsleft = psc_reg4(PSC_ENETRD_LEN + sc->sc_rxset);
		head = MC_RXDMABUFS - bufsleft;

#if 0 /* I don't think this should ever happen */
		if (head == sc->sc_tail) {
#ifdef MCDEBUG
			printf("%s: head == tail: suspending DMA?\n",
			    sc->sc_dev.dv_xname);
#endif
			psc_reg2(PSC_ENETRD_CMD + sc->sc_rxset) = 0x9000;
		}
#endif

		/* Loop through, processing each of the packets */
		for (; sc->sc_tail < head; sc->sc_tail++) {
			offset = sc->sc_tail * 0x800;
			sc->sc_rxframe.rx_rcvcnt = sc->sc_rxbuf[offset];
			sc->sc_rxframe.rx_rcvsts = sc->sc_rxbuf[offset+2];
			sc->sc_rxframe.rx_rntpc = sc->sc_rxbuf[offset+4];
			sc->sc_rxframe.rx_rcvcc = sc->sc_rxbuf[offset+6];
			sc->sc_rxframe.rx_frame = sc->sc_rxbuf + offset + 16;

			mc_rint(sc);
		}

		/*
		 * If we're out of buffers, reset this register set
		 * and switch to the other one. Otherwise, reactivate
		 * this set.
		 */
		if (bufsleft == 0) {
			mc_reset_rxdma_set(sc, sc->sc_rxset);
			sc->sc_rxset ^= 0x10;
		} else
			psc_reg2(PSC_ENETRD_CMD + sc->sc_rxset) = 0x9800;
	}

	/* Get the write channel status */
	status = psc_reg2(PSC_ENETWR_CTL);
	if (status & 0x2000) {
		/* I think this is an exceptional condition. Reset the DMA */
		mc_reset_txdma(sc);
#ifdef MCDEBUG
		printf("%s: resetting transmit DMA channel (status 0x%04x)\n",
			sc->sc_dev.dv_xname, status);
#endif
	} else if (status & 0x100) {
		/* Clear the interrupt and switch register sets */
		psc_reg2(PSC_ENETWR_CMD + sc->sc_txseti) = 0x100;
		sc->sc_txseti ^= 0x10;
	}

	return 1;
}


void
mc_reset_rxdma(sc)
	struct mc_softc *sc;
{
	u_int8_t maccc;

	/* Disable receiver, reset the DMA channels */
	maccc = NIC_GET(sc, MACE_MACCC);
	NIC_PUT(sc, MACE_MACCC, maccc & ~ENRCV);
	psc_reg2(PSC_ENETRD_CTL) = 0x8800;
	mc_reset_rxdma_set(sc, 0);
	psc_reg2(PSC_ENETRD_CTL) = 0x400;

	psc_reg2(PSC_ENETRD_CTL) = 0x8800;
	mc_reset_rxdma_set(sc, 0x10);
	psc_reg2(PSC_ENETRD_CTL) = 0x400;

	/* Reenable receiver, reenable DMA */
	NIC_PUT(sc, MACE_MACCC, maccc);
	sc->sc_rxset = 0;

	psc_reg2(PSC_ENETRD_CMD + PSC_SET0) = 0x9800;
	psc_reg2(PSC_ENETRD_CMD + PSC_SET1) = 0x9800;
}

void
mc_reset_rxdma_set(sc, set)
	struct mc_softc *sc;
	int set;
{
	/* disable DMA while modifying the registers, then reenable DMA */
	psc_reg2(PSC_ENETRD_CMD + set) = 0x0100;
	psc_reg4(PSC_ENETRD_ADDR + set) = sc->sc_rxbuf_phys;
	psc_reg4(PSC_ENETRD_LEN + set) = MC_RXDMABUFS;
	psc_reg2(PSC_ENETRD_CMD + set) = 0x9800;
	sc->sc_tail = 0;
}

void
mc_reset_txdma(sc)
	struct mc_softc *sc;
{
	u_int8_t maccc;

	psc_reg2(PSC_ENETWR_CTL) = 0x8800;
	maccc = NIC_GET(sc, MACE_MACCC);
	NIC_PUT(sc, MACE_MACCC, maccc & ~ENXMT);
	sc->sc_txset = sc->sc_txseti = 0;
	psc_reg2(PSC_ENETWR_CTL) = 0x400;
	NIC_PUT(sc, MACE_MACCC, maccc);
}

int
mc_obio_getaddr(sc, lladdr)
	struct mc_softc *sc;
	u_int8_t *lladdr;
{
	bus_space_handle_t bsh;
	u_char csum;

	if (bus_space_map(sc->sc_regt, MACE_PROM_BASE, 8*16, 0, &bsh)) {
		printf(": failed to map space to read MACE address.\n%s",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	if (!mac68k_bus_space_probe(sc->sc_regt, bsh, 0, 1)) {
		bus_space_unmap(sc->sc_regt, bsh, 8*16);
		return (-1);
	}

	csum = mc_get_enaddr(sc->sc_regt, bsh, 1, lladdr);
	if (csum != 0xff)
		printf(": ethernet PROM checksum failed (0x%x != 0xff)\n%s",
		    (int)csum, sc->sc_dev.dv_xname);

	bus_space_unmap(sc->sc_regt, bsh, 8*16);

	return (csum == 0xff ? 0 : -1);
}

/*	$OpenBSD: noct.c,v 1.2 2002/06/04 21:25:16 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

/*
 * Driver for the Netoctave NSP2000 security processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <crypto/cryptodev.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/noctreg.h>
#include <dev/pci/noctvar.h>

int noct_probe(struct device *, void *, void *);
void noct_attach(struct device *, struct device *, void *);

int noct_ram_size(struct noct_softc *);
void noct_ram_write(struct noct_softc *, u_int32_t, u_int64_t);
u_int64_t noct_ram_read(struct noct_softc *, u_int32_t);

void noct_rng_enable(struct noct_softc *);
void noct_rng_disable(struct noct_softc *);
void noct_rng_init(struct noct_softc *);
void noct_rng_intr(struct noct_softc *);
void noct_rng_tick(void *);

u_int64_t noct_read_8(struct noct_softc *, u_int32_t);
void noct_write_8(struct noct_softc *, u_int32_t, u_int64_t);

struct cfattach noct_ca = {
	sizeof(struct noct_softc), noct_probe, noct_attach,
};

struct cfdriver noct_cd = {
	0, "noct", DV_DULL
};

int noct_intr(void *);

int
noct_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NETOCTAVE &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NETOCTAVE_NSP2K)
		return (1);
	return (0);
}

void 
noct_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct noct_softc *sc = (struct noct_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize = 0;
	u_int32_t cmd;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		goto fail;
	}

	if (pci_mapreg_map(pa, NOCT_BAR0, PCI_MAPREG_MEM_TYPE_64BIT, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		goto fail;
	}

	/* Before we do anything else, put the chip in little endian mode */
	NOCT_WRITE_4(sc, NOCT_BRDG_ENDIAN, 0);

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, noct_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}

	if (noct_ram_size(sc))
		goto fail;

	printf(": %s, %uMB\n", intrstr, sc->sc_ramsize);

	noct_rng_init(sc);

	return;

fail:
	if (iosize != 0)
		bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
}

int 
noct_intr(vsc)
	void *vsc;
{
	struct noct_softc *sc = vsc;
	u_int32_t reg;
	int r = 0;

	reg = NOCT_READ_4(sc, NOCT_BRDG_STAT);

	if (reg & BRDGSTS_RNG_INT) {
		r = 1;
		noct_rng_intr(sc);
	}

	return (r);
}

int
noct_ram_size(sc)
	struct noct_softc *sc;
{
	u_int64_t t;

	noct_ram_write(sc, 0x000000, 64);
	noct_ram_write(sc, 0x400000, 32);
	t = noct_ram_read(sc, 0x000000);
	noct_ram_write(sc, 0x000000, 128);
	noct_ram_write(sc, 0x800000, t);
	t = noct_ram_read(sc, 0x000000);

	if (t != 32 && t != 64 && t != 128) {
		printf(": invalid ram size %llx\n", (unsigned long long)t);
		return (1);
	}

	sc->sc_ramsize = t;
	return (0);
}

void
noct_ram_write(sc, adr, dat)
	struct noct_softc *sc;
	u_int32_t adr;
	u_int64_t dat;
{
	u_int32_t reg;

	/* wait for pending writes to finish */
	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & CTXADDR_WRITEPEND) == 0)
			break;
	}

	NOCT_WRITE_4(sc, NOCT_EA_CTX_ADDR, adr);
	NOCT_WRITE_4(sc, NOCT_EA_CTX_DAT_1, (dat >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_EA_CTX_DAT_0, (dat >>  0) & 0xffffffff);

	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & CTXADDR_WRITEPEND) == 0)
			break;
	}
}

u_int64_t
noct_ram_read(sc, adr)
	struct noct_softc *sc;
	u_int32_t adr;
{
	u_int64_t dat;
	u_int32_t reg;

	/* wait for pending reads to finish */
	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & CTXADDR_READPEND) == 0)
			break;
	}

	NOCT_WRITE_4(sc, NOCT_EA_CTX_ADDR, adr | CTXADDR_READPEND);

	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & CTXADDR_READPEND) == 0)
			break;
	}

	dat = NOCT_READ_4(sc, NOCT_EA_CTX_DAT_1);
	dat <<= 32;
	dat |= NOCT_READ_4(sc, NOCT_EA_CTX_DAT_0);
	return (dat);
}

void
noct_rng_disable(sc)
	struct noct_softc *sc;
{
	u_int64_t csr;
	u_int32_t r;

	/* Turn off RN irq */
	NOCT_WRITE_4(sc, NOCT_BRDG_CTL,
	    NOCT_READ_4(sc, NOCT_BRDG_CTL) & ~(BRDGCTL_RNIRQ_ENA));

	/* Turn off RNH interrupts */
	r = NOCT_READ_4(sc, NOCT_RNG_CSR);
	r &= ~(RNGCSR_INT_KEY | RNGCSR_INT_DUP |
	    RNGCSR_INT_BUS | RNGCSR_INT_ACCESS);
	NOCT_WRITE_4(sc, NOCT_RNG_CSR, r);

	/* Turn off RN queue */
	r = NOCT_READ_4(sc, NOCT_RNG_CSR);
	r &= ~(RNGCSR_XFER_ENABLE | RNGCSR_INT_KEY | RNGCSR_INT_BUS |
	    RNGCSR_INT_DUP | RNGCSR_INT_ACCESS);
	NOCT_WRITE_4(sc, NOCT_RNG_CSR, r);

	for (;;) {
		r = NOCT_READ_4(sc, NOCT_RNG_CSR);
		if ((r & RNGCSR_XFER_BUSY) == 0)
			break;
	}

	/* Turn off RN generator */
	csr = NOCT_READ_8(sc, NOCT_RNG_CTL);
	csr &= ~RNGCTL_RNG_ENA;
	NOCT_WRITE_8(sc, NOCT_RNG_CTL, csr);
}

void
noct_rng_enable(sc)
	struct noct_softc *sc;
{
	u_int64_t adr;
	u_int32_t r;

	adr = sc->sc_rngmap->dm_segs[0].ds_addr;
	NOCT_WRITE_4(sc, NOCT_RNG_BAR1, (adr >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_RNG_Q_LEN, NOCT_RNG_QLEN);
	NOCT_WRITE_4(sc, NOCT_RNG_BAR0, (adr >> 0 ) & 0xffffffff);

	NOCT_WRITE_8(sc, NOCT_RNG_CTL,
	    RNGCTL_RNG_ENA |
	    RNGCTL_TOD_ENA |
	    RNGCTL_BUFSRC_SEED |
	    RNGCTL_SEEDSRC_INT |
	    RNGCTL_EXTCLK_ENA |
	    RNGCTL_DIAG |
	    (100 & RNGCTL_ITERCNT));

	/* Turn on interrupts and enable xfer */
	r = RNGCSR_XFER_ENABLE | RNGCSR_INT_ACCESS |
	    RNGCSR_INT_KEY | RNGCSR_INT_BUS | RNGCSR_INT_DUP;
	NOCT_WRITE_4(sc, NOCT_RNG_CSR, r);

	/* Turn on bridge/rng interrupts */
	r = NOCT_READ_4(sc, NOCT_BRDG_CTL);
	r |= BRDGCTL_RNIRQ_ENA;
	NOCT_WRITE_4(sc, NOCT_BRDG_CTL, r);
}

void
noct_rng_init(sc)
	struct noct_softc *sc;
{
	bus_dma_segment_t seg;
	int rseg;

	if (bus_dmamem_alloc(sc->sc_dmat, NOCT_RNG_BUFSIZE,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: failed rng buf alloc\n", sc->sc_dv.dv_xname);
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, NOCT_RNG_BUFSIZE,
	    (caddr_t *)&sc->sc_rngbuf, BUS_DMA_NOWAIT)) {
		printf("%s: failed rng buf map\n", sc->sc_dv.dv_xname);
		goto fail_1;
	}
	if (bus_dmamap_create(sc->sc_dmat, NOCT_RNG_BUFSIZE, rseg,
	    NOCT_RNG_BUFSIZE, 0, BUS_DMA_NOWAIT, &sc->sc_rngmap)) {
		printf("%s: failed rng map create\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}
	if (bus_dmamap_load_raw(sc->sc_dmat, sc->sc_rngmap,
	    &seg, rseg, NOCT_RNG_BUFSIZE, BUS_DMA_NOWAIT)) {
		printf("%s: failed rng buf load\n", sc->sc_dv.dv_xname);
		goto fail_3;
	}

	noct_rng_disable(sc);
	noct_rng_enable(sc);

	if (hz > 100)
		sc->sc_rngtick = hz/100;
	else
		sc->sc_rngtick = 1;
	timeout_set(&sc->sc_rngto, noct_rng_tick, sc);
	timeout_add(&sc->sc_rngto, sc->sc_rngtick);

	return;

fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_rngmap);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->sc_rngbuf, NOCT_RNG_BUFSIZE);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
fail:
	sc->sc_rngbuf = NULL;
	sc->sc_rngmap = NULL;
	return;
}

void
noct_rng_intr(sc)
	struct noct_softc *sc;
{
	u_int32_t csr;
	int enable = 1;

	csr = NOCT_READ_4(sc, NOCT_RNG_CSR);
	NOCT_WRITE_4(sc, NOCT_RNG_CSR, csr);

	if (csr & RNGCSR_ERR_KEY) {
		u_int32_t ctl;

		enable = 0;
		ctl = NOCT_READ_4(sc, NOCT_RNG_CTL);
		printf("%s: rng bad key(s)", sc->sc_dv.dv_xname);
		if (ctl & RNGCTL_KEY1PAR_ERR)
			printf(", key1 parity");
		if (ctl & RNGCTL_KEY2PAR_ERR)
			printf(", key2 parity");
		printf("\n");
	}
	if (csr & RNGCSR_ERR_BUS) {
		enable = 0;
		printf("%s: rng bus error\n", sc->sc_dv.dv_xname);
	}
	if (csr & RNGCSR_ERR_DUP) {
		enable = 0;
		printf("%s: rng duplicate block\n", sc->sc_dv.dv_xname);
	}
	if (csr & RNGCSR_ERR_ACCESS) {
		enable = 0;
		printf("%s: rng invalid access\n", sc->sc_dv.dv_xname);
	}

	if (!enable)
		noct_rng_disable(sc);
}

void
noct_rng_tick(vsc)
	void *vsc;
{
	struct noct_softc *sc = vsc;
	u_int64_t val;
	u_int32_t reg, rd, wr;
	int cons = 0;

	reg = NOCT_READ_4(sc, NOCT_RNG_Q_PTR);
	rd = (reg & RNGQPTR_READ_M) >> RNGQPTR_READ_S;
	wr = (reg & RNGQPTR_WRITE_M) >> RNGQPTR_WRITE_S;

	while (rd != wr && cons < 32) {
		val = sc->sc_rngbuf[rd];
		add_true_randomness((val >> 32) & 0xffffffff);
		add_true_randomness((val >> 0) & 0xffffffff);
		if (++rd == NOCT_RNG_ENTRIES)
			rd = 0;
		cons++;
	}

	if (cons != 0)
		NOCT_WRITE_4(sc, NOCT_RNG_Q_PTR, rd);
	timeout_add(&sc->sc_rngto, sc->sc_rngtick);
}

void
noct_write_8(sc, reg, val)
	struct noct_softc *sc;
	u_int32_t reg;
	u_int64_t val;
{
	NOCT_WRITE_4(sc, reg, (val >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, reg + 4, (val >> 0) & 0xffffffff);
}

u_int64_t
noct_read_8(sc, reg)
	struct noct_softc *sc;
	u_int32_t reg;
{
	u_int64_t ret;

	ret = NOCT_READ_4(sc, reg);
	ret <<= 32;
	ret |= NOCT_READ_4(sc, reg + 4);
	return (ret);
}

/*	$OpenBSD: noct.c,v 1.8 2002/07/16 15:51:22 jason Exp $	*/

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
#include <sys/extent.h>
#include <sys/kthread.h>

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

void noct_pkh_enable(struct noct_softc *);
void noct_pkh_disable(struct noct_softc *);
void noct_pkh_init(struct noct_softc *);
void noct_pkh_intr(struct noct_softc *);
void noct_pkh_freedesc(struct noct_softc *, int);
u_int32_t noct_pkh_nfree(struct noct_softc *);
int noct_kload(struct noct_softc *, struct crparam *, u_int32_t);
void noct_kload_cb(struct noct_softc *, u_int32_t, int);
void noct_modmul_cb(struct noct_softc *, u_int32_t, int);

void noct_ea_enable(struct noct_softc *);
void noct_ea_disable(struct noct_softc *);
void noct_ea_init(struct noct_softc *);
void noct_ea_intr(struct noct_softc *);
void noct_ea_create_thread(void *);
void noct_ea_thread(void *);
u_int32_t noct_ea_nfree(struct noct_softc *);
void noct_ea_start(struct noct_softc *, struct noct_workq *);
int noct_newsession(u_int32_t *, struct cryptoini *);
int noct_freesession(u_int64_t);
int noct_process(struct cryptop *);

u_int64_t noct_read_8(struct noct_softc *, u_int32_t);
void noct_write_8(struct noct_softc *, u_int32_t, u_int64_t);

struct noct_softc *noct_kfind(struct cryptkop *);
int noct_ksigbits(struct crparam *);
int noct_kprocess(struct cryptkop *);
int noct_kprocess_modexp(struct noct_softc *, struct cryptkop *);

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

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf(": couldn't register cid\n");
		goto fail;
	}

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
	noct_pkh_init(sc);
	noct_ea_init(sc);

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

	if (reg & BRDGSTS_PKP_INT) {
		r = 1;
		noct_pkh_intr(sc);
	}

	if (reg & BRDGSTS_CCH_INT) {
		r = 1;
		noct_ea_intr(sc);
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
		if ((reg & EACTXADDR_WRITEPEND) == 0)
			break;
	}

	NOCT_WRITE_4(sc, NOCT_EA_CTX_ADDR, adr);
	NOCT_WRITE_4(sc, NOCT_EA_CTX_DAT_1, (dat >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_EA_CTX_DAT_0, (dat >>  0) & 0xffffffff);

	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & EACTXADDR_WRITEPEND) == 0)
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
		if ((reg & EACTXADDR_READPEND) == 0)
			break;
	}

	NOCT_WRITE_4(sc, NOCT_EA_CTX_ADDR, adr | EACTXADDR_READPEND);

	for (;;) {
		reg = NOCT_READ_4(sc, NOCT_EA_CTX_ADDR);
		if ((reg & EACTXADDR_READPEND) == 0)
			break;
	}

	dat = NOCT_READ_4(sc, NOCT_EA_CTX_DAT_1);
	dat <<= 32;
	dat |= NOCT_READ_4(sc, NOCT_EA_CTX_DAT_0);
	return (dat);
}

void
noct_pkh_disable(sc)
	struct noct_softc *sc;
{
	u_int32_t r;

	/* Turn off PK irq */
	NOCT_WRITE_4(sc, NOCT_BRDG_CTL,
	    NOCT_READ_4(sc, NOCT_BRDG_CTL) & ~(BRDGCTL_PKIRQ_ENA));

	/* Turn off PK interrupts */
	r = NOCT_READ_4(sc, NOCT_PKH_IER);
	r &= ~(PKHIER_CMDSI | PKHIER_SKSWR | PKHIER_SKSOFF | PKHIER_PKHLEN |
	    PKHIER_PKHOPCODE | PKHIER_BADQBASE | PKHIER_LOADERR |
	    PKHIER_STOREERR | PKHIER_CMDERR | PKHIER_ILL | PKHIER_PKERESV |
	    PKHIER_PKEWDT | PKHIER_PKENOTPRIME |
	    PKHIER_PKE_B | PKHIER_PKE_A | PKHIER_PKE_M | PKHIER_PKE_R |
	    PKHIER_PKEOPCODE);
	NOCT_WRITE_4(sc, NOCT_PKH_IER, r);

	/* Disable PK unit */
	r = NOCT_READ_4(sc, NOCT_PKH_CSR);
	r &= ~PKHCSR_PKH_ENA;
	NOCT_WRITE_4(sc, NOCT_PKH_CSR, r);
	for (;;) {
		r = NOCT_READ_4(sc, NOCT_PKH_CSR);
		if ((r & PKHCSR_PKH_BUSY) == 0)
			break;
	}

	/* Clear status bits */
	r |= PKHCSR_CMDSI | PKHCSR_SKSWR | PKHCSR_SKSOFF | PKHCSR_PKHLEN |
	    PKHCSR_PKHOPCODE | PKHCSR_BADQBASE | PKHCSR_LOADERR |
	    PKHCSR_STOREERR | PKHCSR_CMDERR | PKHCSR_ILL | PKHCSR_PKERESV |
	    PKHCSR_PKEWDT | PKHCSR_PKENOTPRIME |
	    PKHCSR_PKE_B | PKHCSR_PKE_A | PKHCSR_PKE_M | PKHCSR_PKE_R |
	    PKHCSR_PKEOPCODE;
	NOCT_WRITE_4(sc, NOCT_PKH_CSR, r);
}

void
noct_pkh_enable(sc)
	struct noct_softc *sc;
{
	u_int64_t adr;

	sc->sc_pkhwp = 0;
	sc->sc_pkhrp = 0;

	adr = sc->sc_pkhmap->dm_segs[0].ds_addr;
	NOCT_WRITE_4(sc, NOCT_PKH_Q_BASE_HI, (adr >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_PKH_Q_LEN, NOCT_PKH_QLEN);
	NOCT_WRITE_4(sc, NOCT_PKH_Q_BASE_LO, (adr >> 0) & 0xffffffff);

	NOCT_WRITE_4(sc, NOCT_PKH_IER,
	    PKHIER_CMDSI | PKHIER_SKSWR | PKHIER_SKSOFF | PKHIER_PKHLEN |
	    PKHIER_PKHOPCODE | PKHIER_BADQBASE | PKHIER_LOADERR |
	    PKHIER_STOREERR | PKHIER_CMDERR | PKHIER_ILL | PKHIER_PKERESV |
	    PKHIER_PKEWDT | PKHIER_PKENOTPRIME |
	    PKHIER_PKE_B | PKHIER_PKE_A | PKHIER_PKE_M | PKHIER_PKE_R |
	    PKHIER_PKEOPCODE);

	NOCT_WRITE_4(sc, NOCT_PKH_CSR,
	    NOCT_READ_4(sc, NOCT_PKH_CSR) | PKHCSR_PKH_ENA);

	NOCT_WRITE_4(sc, NOCT_BRDG_CTL,
	    NOCT_READ_4(sc, NOCT_BRDG_CTL) | BRDGCTL_PKIRQ_ENA);
}

void
noct_pkh_init(sc)
	struct noct_softc *sc;
{
	bus_dma_segment_t seg, bnseg;
	int rseg, bnrseg;

	sc->sc_pkh_bn = extent_create("noctbn", 0, 255, M_DEVBUF,
	    NULL, NULL, EX_NOWAIT | EX_NOCOALESCE);
	if (sc->sc_pkh_bn == NULL) {
		printf("%s: failed pkh bn extent\n", sc->sc_dv.dv_xname);
		goto fail;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, NOCT_PKH_BUFSIZE,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: failed pkh buf alloc\n", sc->sc_dv.dv_xname);
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, NOCT_PKH_BUFSIZE,
	    (caddr_t *)&sc->sc_pkhcmd, BUS_DMA_NOWAIT)) {
		printf("%s: failed pkh buf map\n", sc->sc_dv.dv_xname);
		goto fail_1;
	}
	if (bus_dmamap_create(sc->sc_dmat, NOCT_PKH_BUFSIZE, rseg,
	    NOCT_PKH_BUFSIZE, 0, BUS_DMA_NOWAIT, &sc->sc_pkhmap)) {
		printf("%s: failed pkh map create\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}
	if (bus_dmamap_load_raw(sc->sc_dmat, sc->sc_pkhmap,
	    &seg, rseg, NOCT_PKH_BUFSIZE, BUS_DMA_NOWAIT)) {
		printf("%s: failed pkh buf load\n", sc->sc_dv.dv_xname);
		goto fail_3;
	}

	/*
	 * Allocate shadow big number cache.
	 */
	if (bus_dmamem_alloc(sc->sc_dmat, NOCT_BN_CACHE_SIZE, PAGE_SIZE, 0,
	    &bnseg, 1, &bnrseg, BUS_DMA_NOWAIT)) {
		printf("%s: failed bnc buf alloc\n", sc->sc_dv.dv_xname);
		goto fail_4;
	}
	if (bus_dmamem_map(sc->sc_dmat, &bnseg, bnrseg, NOCT_BN_CACHE_SIZE,
	    (caddr_t *)&sc->sc_bncache, BUS_DMA_NOWAIT)) {
		printf("%s: failed bnc buf map\n", sc->sc_dv.dv_xname);
		goto fail_5;
	}
	if (bus_dmamap_create(sc->sc_dmat, NOCT_BN_CACHE_SIZE, bnrseg,
	    NOCT_BN_CACHE_SIZE, 0, BUS_DMA_NOWAIT, &sc->sc_bnmap)) {
		printf("%s: failed bnc map create\n", sc->sc_dv.dv_xname);
		goto fail_6;
	}
	if (bus_dmamap_load_raw(sc->sc_dmat, sc->sc_bnmap,
	    &bnseg, bnrseg, NOCT_BN_CACHE_SIZE, BUS_DMA_NOWAIT)) {
		printf("%s: failed bnc buf load\n", sc->sc_dv.dv_xname);
		goto fail_7;
	}

	noct_pkh_disable(sc);
	noct_pkh_enable(sc);

#if 0
	/*
	 * XXX MODEXP is implemented as MODMUL for debugging, don't
	 * XXX actually register.
	 */
	crypto_kregister(sc->sc_cid, CRK_MOD_EXP, 0, noct_kprocess);
#endif

	return;

fail_7:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_bnmap);
fail_6:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->sc_pkhcmd, NOCT_PKH_BUFSIZE);
fail_5:
	bus_dmamem_free(sc->sc_dmat, &bnseg, bnrseg);
fail_4:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_pkhmap);
fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_pkhmap);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->sc_pkhcmd, NOCT_PKH_BUFSIZE);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
fail:
	if (sc->sc_pkh_bn != NULL) {
		extent_destroy(sc->sc_pkh_bn);
		sc->sc_pkh_bn = NULL;
	}
	sc->sc_pkhcmd = NULL;
	sc->sc_pkhmap = NULL;
}

void
noct_pkh_intr(sc)
	struct noct_softc *sc;
{
	u_int32_t csr;
	u_int32_t rp;

	csr = NOCT_READ_4(sc, NOCT_PKH_CSR);
	NOCT_WRITE_4(sc, NOCT_PKH_CSR, csr |
	    PKHCSR_CMDSI | PKHCSR_SKSWR | PKHCSR_SKSOFF | PKHCSR_PKHLEN |
	    PKHCSR_PKHOPCODE | PKHCSR_BADQBASE | PKHCSR_LOADERR |
	    PKHCSR_STOREERR | PKHCSR_CMDERR | PKHCSR_ILL | PKHCSR_PKERESV |
	    PKHCSR_PKEWDT | PKHCSR_PKENOTPRIME |
	    PKHCSR_PKE_B | PKHCSR_PKE_A | PKHCSR_PKE_M | PKHCSR_PKE_R |
	    PKHCSR_PKEOPCODE);

	rp = (NOCT_READ_4(sc, NOCT_PKH_Q_PTR) & PKHQPTR_READ_M) >>
	    PKHQPTR_READ_S;

	while (sc->sc_pkhrp != rp) {
		if (sc->sc_pkh_bnsw[sc->sc_pkhrp].bn_callback != NULL)
			(*sc->sc_pkh_bnsw[sc->sc_pkhrp].bn_callback)(sc,
			    sc->sc_pkhrp, 0);
		if (++sc->sc_pkhrp == NOCT_PKH_ENTRIES)
			sc->sc_pkhrp = 0;
	}
	sc->sc_pkhrp = rp;

	if (csr & PKHCSR_CMDSI) {
		/* command completed */
	}

	if (csr & PKHCSR_SKSWR)
		printf("%s:%x: sks write error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_SKSOFF)
		printf("%s:%x: sks offset error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKHLEN)
		printf("%s:%x: pkh invalid length\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKHOPCODE)
		printf("%s:%x: pkh bad opcode\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_BADQBASE)
		printf("%s:%x: pkh base qbase\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_LOADERR)
		printf("%s:%x: pkh load error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_STOREERR)
		printf("%s:%x: pkh store error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_CMDERR)
		printf("%s:%x: pkh command error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_ILL)
		printf("%s:%x: pkh illegal access\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKERESV)
		printf("%s:%x: pke reserved error\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKEWDT)
		printf("%s:%x: pke watchdog\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKENOTPRIME)
		printf("%s:%x: pke not prime\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKE_B)
		printf("%s:%x: pke bad 'b'\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKE_A)
		printf("%s:%x: pke bad 'a'\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKE_M)
		printf("%s:%x: pke bad 'm'\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKE_R)
		printf("%s:%x: pke bad 'r'\n", sc->sc_dv.dv_xname, rp);
	if (csr & PKHCSR_PKEOPCODE)
		printf("%s:%x: pke bad opcode\n", sc->sc_dv.dv_xname, rp);
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
	NOCT_WRITE_4(sc, NOCT_RNG_Q_BASE_HI, (adr >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_RNG_Q_LEN, NOCT_RNG_QLEN);
	NOCT_WRITE_4(sc, NOCT_RNG_Q_BASE_LO, (adr >> 0 ) & 0xffffffff);

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

u_int32_t
noct_ea_nfree(sc)
	struct noct_softc *sc;
{
	if (sc->sc_eawp == sc->sc_earp)
		return (NOCT_EA_ENTRIES);
	if (sc->sc_eawp < sc->sc_earp)
		return (sc->sc_earp - sc->sc_eawp - 1);
	return (sc->sc_earp + NOCT_EA_ENTRIES - sc->sc_eawp - 1);
}

void
noct_ea_disable(sc)
	struct noct_softc *sc;
{
	u_int32_t r;

	/* Turn off EA irq */
	NOCT_WRITE_4(sc, NOCT_BRDG_CTL,
	    NOCT_READ_4(sc, NOCT_BRDG_CTL) & ~(BRDGCTL_EAIRQ_ENA));

	/* Turn off EA interrupts */
	r = NOCT_READ_4(sc, NOCT_EA_IER);
	r &= ~(EAIER_QALIGN | EAIER_CMDCMPL | EAIER_OPERR | EAIER_CMDREAD |
	    EAIER_CMDWRITE | EAIER_DATAREAD | EAIER_DATAWRITE |
	    EAIER_INTRNLLEN | EAIER_EXTRNLLEN | EAIER_DESBLOCK |
	    EAIER_DESKEY | EAIER_ILL);
	NOCT_WRITE_4(sc, NOCT_EA_IER, r);

	/* Disable EA unit */
	r = NOCT_READ_4(sc, NOCT_EA_CSR);
	r &= ~EACSR_ENABLE;
	NOCT_WRITE_4(sc, NOCT_EA_CSR, r);
	for (;;) {
		r = NOCT_READ_4(sc, NOCT_EA_CSR);
		if ((r & EACSR_BUSY) == 0)
			break;
	}

	/* Clear status bits */
	r = NOCT_READ_4(sc, NOCT_EA_CSR);
	r |= EACSR_QALIGN | EACSR_CMDCMPL | EACSR_OPERR | EACSR_CMDREAD |
	    EACSR_CMDWRITE | EACSR_DATAREAD | EACSR_DATAWRITE |
	    EACSR_INTRNLLEN | EACSR_EXTRNLLEN | EACSR_DESBLOCK |
	    EACSR_DESKEY | EACSR_ILL;
	NOCT_WRITE_4(sc, NOCT_EA_CSR, r);
}

void
noct_ea_enable(sc)
	struct noct_softc *sc;
{
	u_int64_t adr;

	sc->sc_eawp = 0;
	sc->sc_earp = 0;

	adr = sc->sc_eamap->dm_segs[0].ds_addr;
	NOCT_WRITE_4(sc, NOCT_EA_Q_BASE_HI, (adr >> 32) & 0xffffffff);
	NOCT_WRITE_4(sc, NOCT_EA_Q_LEN, NOCT_EA_QLEN);
	NOCT_WRITE_4(sc, NOCT_EA_Q_BASE_LO, (adr >> 0) & 0xffffffff);

	NOCT_WRITE_4(sc, NOCT_EA_IER,
	    EAIER_QALIGN | EAIER_CMDCMPL | EAIER_OPERR | EAIER_CMDREAD |
	    EAIER_CMDWRITE | EAIER_DATAREAD | EAIER_DATAWRITE |
	    EAIER_INTRNLLEN | EAIER_EXTRNLLEN | EAIER_DESBLOCK |
	    EAIER_DESKEY | EAIER_ILL);

	NOCT_WRITE_4(sc, NOCT_EA_CSR,
	    NOCT_READ_4(sc, NOCT_EA_CSR) | EACSR_ENABLE);

	NOCT_WRITE_4(sc, NOCT_BRDG_CTL,
	    NOCT_READ_4(sc, NOCT_BRDG_CTL) | BRDGCTL_EAIRQ_ENA);
}

void
noct_ea_init(sc)
	struct noct_softc *sc;
{
	bus_dma_segment_t seg;
	int rseg;

	if (bus_dmamem_alloc(sc->sc_dmat, NOCT_EA_BUFSIZE,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: failed ea buf alloc\n", sc->sc_dv.dv_xname);
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, NOCT_EA_BUFSIZE,
	    (caddr_t *)&sc->sc_eacmd, BUS_DMA_NOWAIT)) {
		printf("%s: failed ea buf map\n", sc->sc_dv.dv_xname);
		goto fail_1;
	}
	if (bus_dmamap_create(sc->sc_dmat, NOCT_EA_BUFSIZE, rseg,
	    NOCT_EA_BUFSIZE, 0, BUS_DMA_NOWAIT, &sc->sc_eamap)) {
		printf("%s: failed ea map create\n", sc->sc_dv.dv_xname);
		goto fail_2;
	}
	if (bus_dmamap_load_raw(sc->sc_dmat, sc->sc_eamap,
	    &seg, rseg, NOCT_EA_BUFSIZE, BUS_DMA_NOWAIT)) {
		printf("%s: failed ea buf load\n", sc->sc_dv.dv_xname);
		goto fail_3;
	}

	noct_ea_disable(sc);
	noct_ea_enable(sc);

	SIMPLEQ_INIT(&sc->sc_inq);
	SIMPLEQ_INIT(&sc->sc_chipq);
	SIMPLEQ_INIT(&sc->sc_outq);

	crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0,
	    noct_newsession, noct_freesession, noct_process);
	crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0, NULL, NULL, NULL);

	kthread_create_deferred(noct_ea_create_thread, sc);

	return;

fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_eamap);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->sc_eacmd, NOCT_EA_BUFSIZE);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
fail:
	sc->sc_eacmd = NULL;
	sc->sc_eamap = NULL;
}

void
noct_ea_create_thread(vsc)
	void *vsc;
{
	struct noct_softc *sc = vsc;

	if (kthread_create(noct_ea_thread, sc, NULL,
	    "%s", sc->sc_dv.dv_xname))
		panic("%s: unable to create ea thread", sc->sc_dv.dv_xname);
}

void
noct_ea_thread(vsc)
	void *vsc;
{
	struct noct_softc *sc = vsc;
	struct noct_workq *q;
	struct cryptop *crp;
	struct cryptodesc *crd;
	int s, rseg;
	u_int32_t len;

	for (;;) {
		tsleep(&sc->sc_eawp, PWAIT, "noctea", 0);

		/* Handle output queue */
		s = splnet();
		while (!SIMPLEQ_EMPTY(&sc->sc_outq)) {
			q = SIMPLEQ_FIRST(&sc->sc_outq);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_outq, q, q_next);
			crp = q->q_crp;
			crd = crp->crp_desc;

			if (crd->crd_alg == CRYPTO_MD5)
				len = 16;
			else if (crd->crd_alg == CRYPTO_SHA1)
				len = 20;
			else
				len = 0;

			if (len != 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copyback((struct mbuf *)crp->crp_buf,
					    crd->crd_inject, len,
					    q->q_macbuf);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					bcopy(q->q_macbuf, crp->crp_mac, len);
			}

			splx(s);

			bus_dmamap_sync(sc->sc_dmat, q->q_dmamap,
			    0, q->q_dmamap->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, q->q_dmamap);
			bus_dmamap_destroy(sc->sc_dmat, q->q_dmamap);
			bus_dmamem_unmap(sc->sc_dmat, q->q_buf, crd->crd_len);
			bus_dmamem_free(sc->sc_dmat, &q->q_dmaseg, rseg);
			crp->crp_etype = 0;
			free(q, M_DEVBUF);
			crypto_done(crp);
			s = splnet();
		}

		/* Handle input queue */
		s = splnet();
		while (!SIMPLEQ_EMPTY(&sc->sc_inq)) {
			q = SIMPLEQ_FIRST(&sc->sc_inq);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_inq, q, q_next);
			splx(s);

			noct_ea_start(sc, q);
			s = splnet();
		}
		splx(s);
	}
}

void
noct_ea_start(sc, q)
	struct noct_softc *sc;
	struct noct_workq *q;
{
	struct cryptop *crp;
	struct cryptodesc *crd;
	u_int64_t adr;
	int s, err, i, rseg;
	u_int32_t wp;

	crp = q->q_crp;
	crd = crp->crp_desc;

	/* XXX Can't handle multiple ops yet */
	if (crd->crd_next != NULL) {
		err = EOPNOTSUPP;
		goto errout;
	}

	if (crd->crd_alg != CRYPTO_MD5 &&
	    crd->crd_alg != CRYPTO_SHA1) {
		err = EOPNOTSUPP;
		goto errout;
	}

	if (crd->crd_len > 0x4800) {
		err = ERANGE;
		goto errout;
	}

	if ((err = bus_dmamem_alloc(sc->sc_dmat, crd->crd_len, PAGE_SIZE, 0,
	    &q->q_dmaseg, 1, &rseg, BUS_DMA_WAITOK | BUS_DMA_STREAMING)) != 0)
		goto errout;

	if ((err = bus_dmamem_map(sc->sc_dmat, &q->q_dmaseg, rseg,
	    crd->crd_len, (caddr_t *)&q->q_buf, BUS_DMA_WAITOK)) != 0)
		goto errout_dmafree;

	if ((err = bus_dmamap_create(sc->sc_dmat, crd->crd_len, 1,
	    crd->crd_len, 0, BUS_DMA_WAITOK, &q->q_dmamap)) != 0)
		goto errout_dmaunmap;

	if ((err = bus_dmamap_load_raw(sc->sc_dmat, q->q_dmamap, &q->q_dmaseg,
	    rseg, crd->crd_len, BUS_DMA_WAITOK)) != 0)
		goto errout_dmadestroy;

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m_copydata((struct mbuf *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, q->q_buf);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		cuio_copydata((struct uio *)crp->crp_buf,
		    crd->crd_skip, crd->crd_len, q->q_buf);
	else {
		err = EINVAL;
		goto errout_dmaunload;
	}

	bus_dmamap_sync(sc->sc_dmat, q->q_dmamap, 0, q->q_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	s = splnet();
	if (noct_ea_nfree(sc) < 1) {
		err = ENOMEM;
		goto errout_dmaunload;
	}
	wp = sc->sc_eawp;
	if (++sc->sc_eawp == NOCT_EA_ENTRIES)
		sc->sc_eawp = 0;
	for (i = 0; i < EA_CMD_WORDS; i++)
		sc->sc_eacmd[wp].buf[i] = 0;
	sc->sc_eacmd[wp].buf[0] = EA_0_SI;
	switch (crd->crd_alg) {
	case CRYPTO_MD5:
		sc->sc_eacmd[wp].buf[1] = htole32(EA_OP_MD5);
		break;
	case CRYPTO_SHA1:
		sc->sc_eacmd[wp].buf[1] = htole32(EA_OP_SHA1);
		break;
	}

	/* Source, new buffer just allocated */
	sc->sc_eacmd[wp].buf[1] |= htole32(crd->crd_len);
	adr = q->q_dmamap->dm_segs[0].ds_addr;
	sc->sc_eacmd[wp].buf[2] = htole32(adr >> 32);
	sc->sc_eacmd[wp].buf[3] = htole32(adr & 0xffffffff);

	/* Dest, hide it in the descriptor */
	adr = sc->sc_eamap->dm_segs[0].ds_addr +
	    (wp * sizeof(struct noct_ea_cmd)) +
	    offsetof(struct noct_ea_cmd, buf[6]);
	sc->sc_eacmd[wp].buf[4] = htole32(adr >> 32);
	sc->sc_eacmd[wp].buf[5] = htole32(adr & 0xffffffff);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_eamap,
	    (wp * sizeof(struct noct_ea_cmd)), sizeof(struct noct_ea_cmd),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	if (++wp == NOCT_EA_ENTRIES)
		wp = 0;
	NOCT_WRITE_4(sc, NOCT_EA_Q_PTR, wp);
	sc->sc_eawp = wp;

	SIMPLEQ_INSERT_TAIL(&sc->sc_chipq, q, q_next);
	splx(s);

	return;

errout_dmaunload:
	bus_dmamap_unload(sc->sc_dmat, q->q_dmamap);
errout_dmadestroy:
	bus_dmamap_destroy(sc->sc_dmat, q->q_dmamap);
errout_dmaunmap:
	bus_dmamem_unmap(sc->sc_dmat, q->q_buf, crd->crd_len);
errout_dmafree:
	bus_dmamem_free(sc->sc_dmat, &q->q_dmaseg, rseg);
errout:
	crp->crp_etype = err;
	free(q, M_DEVBUF);
	s = splnet();
	crypto_done(crp);
	splx(s);
}

void
noct_ea_intr(sc)
	struct noct_softc *sc;
{
	struct noct_workq *q;
	u_int32_t csr, rp;

	csr = NOCT_READ_4(sc, NOCT_EA_CSR);
	NOCT_WRITE_4(sc, NOCT_EA_CSR, csr |
	    EACSR_QALIGN | EACSR_CMDCMPL | EACSR_OPERR | EACSR_CMDREAD |
	    EACSR_CMDWRITE | EACSR_DATAREAD | EACSR_DATAWRITE |
	    EACSR_INTRNLLEN | EACSR_EXTRNLLEN | EACSR_DESBLOCK |
	    EACSR_DESKEY | EACSR_ILL);

	rp = (NOCT_READ_4(sc, NOCT_EA_Q_PTR) & EAQPTR_READ_M) >>
	    EAQPTR_READ_S;
	while (sc->sc_earp != rp) {
		if (SIMPLEQ_EMPTY(&sc->sc_chipq))
			panic("%s: empty chipq", sc->sc_dv.dv_xname);
		q = SIMPLEQ_FIRST(&sc->sc_chipq);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_chipq, q, q_next);
		SIMPLEQ_INSERT_TAIL(&sc->sc_outq, q, q_next);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_eamap,
		    (sc->sc_earp * sizeof(struct noct_ea_cmd)),
		    sizeof(struct noct_ea_cmd),
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
		bcopy((u_int8_t *)&sc->sc_eacmd[sc->sc_earp].buf[6],
		    q->q_macbuf, 20);

		NOCT_WAKEUP(sc);
		if (++sc->sc_earp == NOCT_EA_ENTRIES)
			sc->sc_earp = 0;
	}
	sc->sc_earp = rp;

	if (csr & EACSR_QALIGN)
		printf("%s: ea bad queue alignment\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_OPERR)
		printf("%s: ea bad opcode\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_CMDREAD)
		printf("%s: ea command read error\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_CMDWRITE)
		printf("%s: ea command write error\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_DATAREAD)
		printf("%s: ea data read error\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_DATAWRITE)
		printf("%s: ea data write error\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_INTRNLLEN)
		printf("%s: ea bad internal len\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_EXTRNLLEN)
		printf("%s: ea bad external len\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_DESBLOCK)
		printf("%s: ea bad des block\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_DESKEY)
		printf("%s: ea bad des key\n", sc->sc_dv.dv_xname);
	if (csr & EACSR_ILL)
		printf("%s: ea illegal access\n", sc->sc_dv.dv_xname);
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

struct noct_softc *
noct_kfind(krp)
	struct cryptkop *krp;
{
	struct noct_softc *sc;
	int i;

	for (i = 0; i < noct_cd.cd_ndevs; i++) {
		sc = noct_cd.cd_devs[i];
		if (sc == NULL)
			continue;
		if (sc->sc_cid == krp->krp_hid)
			return (sc);
	}
	return (NULL);
}

int
noct_kprocess(krp)
	struct cryptkop *krp;
{
	struct noct_softc *sc;

	if (krp == NULL || krp->krp_callback == NULL)
		return (EINVAL);
	if ((sc = noct_kfind(krp)) == NULL) {
		krp->krp_status = EINVAL;
		crypto_kdone(krp);
		return (0);
	}

	switch (krp->krp_op) {
	case CRK_MOD_EXP:
		noct_kprocess_modexp(sc, krp);
		break;
	default:
		printf("%s: kprocess: invalid op 0x%x\n",
		    sc->sc_dv.dv_xname, krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		break;
	}
	return (0);
}

u_int32_t
noct_pkh_nfree(sc)
	struct noct_softc *sc;
{
	if (sc->sc_pkhwp == sc->sc_pkhrp)
		return (NOCT_PKH_ENTRIES);
	if (sc->sc_pkhwp < sc->sc_pkhrp)
		return (sc->sc_pkhrp - sc->sc_pkhwp - 1);
	return (sc->sc_pkhrp + NOCT_PKH_ENTRIES - sc->sc_pkhwp - 1);
}

int
noct_kprocess_modexp(sc, krp)
	struct noct_softc *sc;
	struct cryptkop *krp;
{
	int s, err;
	u_long roff;
	u_int32_t wp, aidx, bidx, midx;
	u_int64_t adr;
	union noct_pkh_cmd *cmd;
	int i;

	s = splnet();
	if (noct_pkh_nfree(sc) < 5) {
		/* Need 5 entries: 3 loads, 1 store, and an op */
		splx(s);
		return (ENOMEM);
	}

	wp = sc->sc_pkhwp;

	aidx = wp;
	if (noct_kload(sc, &krp->krp_param[0], aidx))
		goto errout;
	if (++wp == NOCT_PKH_ENTRIES)
		wp = 0;

	bidx = wp;
	if (noct_kload(sc, &krp->krp_param[1], bidx))
		goto errout;
	if (++wp == NOCT_PKH_ENTRIES)
		wp = 0;

	midx = wp;
	if (noct_kload(sc, &krp->krp_param[2], midx))
		goto errout;
	if (++wp == NOCT_PKH_ENTRIES)
		wp = 0;

	/* alloc cache for result */
	if (extent_alloc(sc->sc_pkh_bn, sc->sc_pkh_bnsw[midx].bn_siz,
	    EX_NOALIGN, 0, EX_NOBOUNDARY, EX_NOWAIT, &roff)) {
		err = ENOMEM;
		goto errout;
	}

	cmd = &sc->sc_pkhcmd[wp];
	cmd->arith.op = htole32(PKH_OP_CODE_MUL);
	cmd->arith.r = htole32(roff);
	cmd->arith.m = htole32(((sc->sc_pkh_bnsw[midx].bn_siz) << 16) |
	    sc->sc_pkh_bnsw[midx].bn_off);
	cmd->arith.a = htole32(((sc->sc_pkh_bnsw[aidx].bn_siz) << 16) |
	    sc->sc_pkh_bnsw[aidx].bn_off);
	cmd->arith.b = htole32(((sc->sc_pkh_bnsw[bidx].bn_siz) << 16) |
	    sc->sc_pkh_bnsw[bidx].bn_off);
	cmd->arith.c = cmd->arith.unused[0] = cmd->arith.unused[1] = 0;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_pkhmap,
	    wp * sizeof(union noct_pkh_cmd), sizeof(union noct_pkh_cmd),
	    BUS_DMASYNC_PREWRITE);
	sc->sc_pkh_bnsw[wp].bn_callback = NULL;
	if (++wp == NOCT_PKH_ENTRIES)
		wp = 0;

	cmd = &sc->sc_pkhcmd[wp];
	cmd->cache.op = htole32(PKH_OP_CODE_STORE | PKH_OP_SI);
	cmd->cache.r = htole32(roff);
	adr = sc->sc_bnmap->dm_segs[0].ds_addr + (roff * 16);
	cmd->cache.addrhi = htole32((adr >> 32) & 0xffffffff);
	cmd->cache.addrlo = htole32((adr >> 0 ) & 0xffffffff);
	cmd->cache.len = htole32(sc->sc_pkh_bnsw[midx].bn_siz * 16);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_pkhmap,
	    wp * sizeof(union noct_pkh_cmd), sizeof(union noct_pkh_cmd),
	    BUS_DMASYNC_PREWRITE);
	sc->sc_pkh_bnsw[wp].bn_callback = noct_modmul_cb;
	sc->sc_pkh_bnsw[wp].bn_off = roff;
	sc->sc_pkh_bnsw[wp].bn_siz = sc->sc_pkh_bnsw[midx].bn_siz;
	sc->sc_pkh_bnsw[wp].bn_krp = krp;
	if (++wp == NOCT_PKH_ENTRIES)
		wp = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_bnmap,
	    0, sc->sc_bnmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	NOCT_WRITE_4(sc, NOCT_PKH_Q_PTR, wp);
	sc->sc_pkhwp = wp;

	splx(s);

	return (0);

errout:
	i = sc->sc_pkhwp;
	while (i != wp) {
		noct_pkh_freedesc(sc, i);
		if (++i == NOCT_PKH_ENTRIES)
			i = 0;
	}

	splx(s);
	krp->krp_status = err;
	crypto_kdone(krp);
	return (1);
}

void
noct_pkh_freedesc(sc, idx)
	struct noct_softc *sc;
	int idx;
{
	if (sc->sc_pkh_bnsw[idx].bn_callback != NULL)
		(*sc->sc_pkh_bnsw[idx].bn_callback)(sc, idx, 0);
}

/*
 * Return the number of significant bits of a big number.
 */
int
noct_ksigbits(cr)
	struct crparam *cr;
{
	u_int plen = (cr->crp_nbits + 7) / 8;
	int i, sig = plen * 8;
	u_int8_t c, *p = cr->crp_p;

	for (i = plen - 1; i >= 0; i--) {
		c = p[i];
		if (c != 0) {
			while ((c & 0x80) == 0) {
				sig--;
				c <<= 1;
			}
			break;
		}
		sig -= 8;
	}
	return (sig);
}

int
noct_kload(sc, cr, wp)
	struct noct_softc *sc;
	struct crparam *cr;
	u_int32_t wp;
{
	u_int64_t adr;
	union noct_pkh_cmd *cmd;
	u_long off;
	int bits, digits, i;
	u_int32_t wpnext;

	wpnext = wp + 1;
	if (wpnext == NOCT_PKH_ENTRIES)
		wpnext = 0;
	if (wpnext == sc->sc_pkhrp)
		return (ENOMEM);

	bits = noct_ksigbits(cr);
	if (bits > 4096)
		return (E2BIG);

	digits = (bits + 127) / 128;

	if (extent_alloc(sc->sc_pkh_bn, digits, EX_NOALIGN, 0, EX_NOBOUNDARY,
	    EX_NOWAIT, &off))
		return (ENOMEM);

	cmd = &sc->sc_pkhcmd[wp];
	cmd->cache.op = htole32(PKH_OP_CODE_LOAD);
	cmd->cache.r = htole32(off);
	adr = sc->sc_bnmap->dm_segs[0].ds_addr + (off * 16);
	cmd->cache.addrhi = htole32((adr >> 32) & 0xffffffff);
	cmd->cache.addrlo = htole32((adr >> 0 ) & 0xffffffff);
	cmd->cache.len = htole32(digits * 16);
	cmd->cache.unused[0] = cmd->cache.unused[1] = cmd->cache.unused[2] = 0;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_pkhmap,
	    wp * sizeof(union noct_pkh_cmd), sizeof(union noct_pkh_cmd),
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < (digits * 16); i++)
		sc->sc_bncache[(off * 16) + i] = 0;
	for (i = 0; i < ((bits + 7) / 8); i++)
		sc->sc_bncache[(off * 16) + (digits * 16) - 1 - i] =
		    cr->crp_p[i];
	bus_dmamap_sync(sc->sc_dmat, sc->sc_bnmap, off * 16, digits * 16,
	    BUS_DMASYNC_PREWRITE);

	sc->sc_pkh_bnsw[wp].bn_off = off;
	sc->sc_pkh_bnsw[wp].bn_siz = digits;
	sc->sc_pkh_bnsw[wp].bn_callback = noct_kload_cb;
	return (0);
}

void
noct_kload_cb(sc, wp, err)
	struct noct_softc *sc;
	u_int32_t wp;
	int err;
{
	struct noct_bnc_sw *sw = &sc->sc_pkh_bnsw[wp];

	extent_free(sc->sc_pkh_bn, sw->bn_off, sw->bn_siz, EX_NOWAIT);
	bzero(&sc->sc_bncache[sw->bn_off * 16], sw->bn_siz * 16);
}

void
noct_modmul_cb(sc, wp, err)
	struct noct_softc *sc;
	u_int32_t wp;
	int err;
{
	struct noct_bnc_sw *sw = &sc->sc_pkh_bnsw[wp];
	struct cryptkop *krp = sw->bn_krp;
	int i, j;

	if (err)
		goto out;

	i = (sw->bn_off * 16) + (sw->bn_siz * 16) - 1;
	for (j = 0; j < (krp->krp_param[3].crp_nbits + 7) / 8; j++) {
		krp->krp_param[3].crp_p[j] = sc->sc_bncache[i];
		i--;
	}

out:
	extent_free(sc->sc_pkh_bn, sw->bn_off, sw->bn_siz, EX_NOWAIT);
	bzero(&sc->sc_bncache[sw->bn_off * 16], sw->bn_siz * 16);
	krp->krp_status = err;
	crypto_kdone(krp);
}

int
noct_newsession(sidp, cri)
	u_int32_t *sidp;
	struct cryptoini *cri;
{
	struct noct_softc *sc;
	int i;

	for (i = 0; i < noct_cd.cd_ndevs; i++) {
		sc = noct_cd.cd_devs[i];
		if (sc == NULL || sc->sc_cid == (*sidp))
			break;
	}
	if (sc == NULL)
		return (EINVAL);

	/* Can only handle single operations */
	if (cri->cri_next != NULL)
		return (EINVAL);

	*sidp = NOCT_SID(sc->sc_dv.dv_unit, 0);
	return (0);
}

int
noct_freesession(tid)
	u_int64_t tid;
{
	int card;
	u_int32_t sid = ((u_int32_t)tid) & 0xffffffff;

	card = NOCT_CARD(sid);
	if (card >= noct_cd.cd_ndevs || noct_cd.cd_devs[card] == NULL)
		return (EINVAL);
	return (0);
}

int
noct_process(crp)
	struct cryptop *crp;
{
	struct noct_softc *sc;
	struct noct_workq *q = NULL;
	int card, err, s;

	if (crp == NULL || crp->crp_callback == NULL)
		return (EINVAL);

	card = NOCT_CARD(crp->crp_sid);
	if (card >= noct_cd.cd_ndevs || noct_cd.cd_devs[card] == NULL)
		return (EINVAL);
	sc = noct_cd.cd_devs[card];

	q = (struct noct_workq *)malloc(sizeof(struct noct_workq),
	    M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		err = ENOMEM;
		goto errout;
	}
	q->q_crp = crp;

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_inq, q, q_next);
	splx(s);
	NOCT_WAKEUP(sc);
	return (0);

errout:
	if (q != NULL)
		free(q, M_DEVBUF);
	crp->crp_etype = err;
	crypto_done(crp);
	return (0);
}

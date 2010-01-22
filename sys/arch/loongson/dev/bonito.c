/*	$OpenBSD: bonito.c,v 1.2 2010/01/22 21:45:22 miod Exp $	*/
/*	$NetBSD: bonito_mainbus.c,v 1.11 2008/04/28 20:23:10 martin Exp $	*/
/*	$NetBSD: bonito_pci.c,v 1.5 2008/04/28 20:23:28 martin Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI configuration space support for the Loongson PCI and memory controller
 * chip, which is derived from the Algorithmics BONITO chip.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <loongson/dev/bonitoreg.h>
#include <loongson/dev/bonitovar.h>
#include <loongson/dev/glxvar.h>
#include <loongson/dev/lemote_irq.h>

int	bonito_match(struct device *, void *, void *);
void	bonito_attach(struct device *, struct device *, void *);

const struct cfattach bonito_ca = {
	sizeof(struct bonito_softc),
	bonito_match, bonito_attach
};

struct cfdriver bonito_cd = {
	NULL, "bonito", DV_DULL
};

#define	wbflush()	__asm__ __volatile__ ("sync" ::: "memory")

bus_addr_t	bonito_pa_to_device(paddr_t);
paddr_t		bonito_device_to_pa(bus_addr_t);

void	 bonito_intr_makemasks(void);
void	 bonito_splx(int);
uint32_t bonito_intr(uint32_t, struct trap_frame *);
uint32_t bonito_isa_intr(uint32_t, struct trap_frame *);
void	 bonito_setintrmask(int);

void	 bonito_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	 bonito_bus_maxdevs(void *, int);
pcitag_t bonito_make_tag(void *, int, int, int);
void	 bonito_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t bonito_conf_read(void *, pcitag_t, int);
void	 bonito_conf_write(void *, pcitag_t, int, pcireg_t);
int	 bonito_pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *
	 bonito_pci_intr_string(void *, pci_intr_handle_t);
void	*bonito_pci_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	 bonito_pci_intr_disestablish(void *, void *);

int	 bonito_conf_addr(struct bonito_softc *, pcitag_t, int, u_int32_t *,
	    u_int32_t *);

uint	 bonito_get_isa_imr(void);
uint	 bonito_get_isa_isr(void);
void	 bonito_set_isa_imr(uint);
void	 bonito_isa_specific_eoi(int);

/*
 * Bonito interrupt handling declarations: on the Yeelong, we have 14
 * interrupts on Bonito, and 16 (well, 15) ISA interrupts with the usual
 * 8259 pair. Bonito and ISA interrupts happen on two different levels.
 *
 * For simplicity we allocate 16 vectors for direct interrupts, and 16
 * vectors for ISA interrupts as well.
 */

#define	BONITO_NINTS		(16 + 16)
struct intrhand *bonito_intrhand[BONITO_NINTS];

#define	BONITO_ISA_IRQ(i)	((i) + 16)
#define	BONITO_DIRECT_IRQ(i)	(i)
#define	BONITO_IRQ_IS_ISA(i)	((i) >= 16)

#define	INTPRI_BONITO	(INTPRI_CLOCK + 1)
#define	INTPRI_ISA	(INTPRI_BONITO + 1)

uint64_t bonito_intem;
uint64_t bonito_imask[NIPLS];

#define	REGVAL8(x)	*((volatile u_int8_t *)PHYS_TO_XKPHYS(x, CCA_NC))
uint	bonito_isaimr;

struct machine_bus_dma_tag bonito_bus_dma_tag = {
	._dmamap_create = _dmamap_create,
	._dmamap_destroy = _dmamap_destroy,
	._dmamap_load = _dmamap_load,
	._dmamap_load_mbuf = _dmamap_load_mbuf,
	._dmamap_load_uio = _dmamap_load_uio,
	._dmamap_load_raw = _dmamap_load_raw,
	._dmamap_load_buffer = _dmamap_load_buffer,
	._dmamap_unload = _dmamap_unload,
	._dmamap_sync = _dmamap_sync,

	._dmamem_alloc = _dmamem_alloc,
	._dmamem_free = _dmamem_free,
	._dmamem_map = _dmamem_map,
	._dmamem_unmap = _dmamem_unmap,
	._dmamem_mmap = _dmamem_mmap,

	._pa_to_device = bonito_pa_to_device,
	._device_to_pa = bonito_device_to_pa
};

struct mips_bus_space bonito_pci_io_space_tag = {
	.bus_base = PHYS_TO_XKPHYS(BONITO_PCIIO_BASE, CCA_NC),
	._space_read_1 = generic_space_read_1,
	._space_write_1 = generic_space_write_1,
	._space_read_2 = generic_space_read_2,
	._space_write_2 = generic_space_write_2,
	._space_read_4 = generic_space_read_4,
	._space_write_4 = generic_space_write_4,
	._space_read_8 = generic_space_read_8,
	._space_write_8 = generic_space_write_8,
	._space_read_raw_2 = generic_space_read_raw_2,
	._space_write_raw_2 = generic_space_write_raw_2,
	._space_read_raw_4 = generic_space_read_raw_4,
	._space_write_raw_4 = generic_space_write_raw_4,
	._space_read_raw_8 = generic_space_read_raw_8,
	._space_write_raw_8 = generic_space_write_raw_8,
	._space_map = generic_space_map,
	._space_unmap = generic_space_unmap,
	._space_subregion = generic_space_region,
	._space_vaddr = generic_space_vaddr
};

struct mips_bus_space bonito_pci_mem_space_tag = {
	.bus_base = PHYS_TO_XKPHYS(BONITO_PCILO_BASE, CCA_NC),
	._space_read_1 = generic_space_read_1,
	._space_write_1 = generic_space_write_1,
	._space_read_2 = generic_space_read_2,
	._space_write_2 = generic_space_write_2,
	._space_read_4 = generic_space_read_4,
	._space_write_4 = generic_space_write_4,
	._space_read_8 = generic_space_read_8,
	._space_write_8 = generic_space_write_8,
	._space_read_raw_2 = generic_space_read_raw_2,
	._space_write_raw_2 = generic_space_write_raw_2,
	._space_read_raw_4 = generic_space_read_raw_4,
	._space_write_raw_4 = generic_space_write_raw_4,
	._space_read_raw_8 = generic_space_read_raw_8,
	._space_write_raw_8 = generic_space_write_raw_8,
	._space_map = generic_space_map,
	._space_unmap = generic_space_unmap,
	._space_subregion = generic_space_region,
	._space_vaddr = generic_space_vaddr
};

int
bonito_match(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (strcmp(maa->maa_name, bonito_cd.cd_name) == 0)
		return (1);

	return (0);
}

const struct bonito_config yeelong_bonito = {
	.bc_adbase = 11,

	.bc_gpioIE = YEELONG_INTRMASK_GPIO,
	.bc_intEdge = YEELONG_INTRMASK_PCI_SYSERR | YEELONG_INTRMASK_PCI_PARERR,
	.bc_intSteer = 0,
	.bc_intPol = YEELONG_INTRMASK_DRAM_PARERR |
	    YEELONG_INTRMASK_PCI_SYSERR | YEELONG_INTRMASK_PCI_PARERR |
	    YEELONG_INTRMASK_INT0 | YEELONG_INTRMASK_INT1
};

void
bonito_attach(struct device *parent, struct device *self, void *aux)
{
	struct bonito_softc *sc = (struct bonito_softc *)self;
	struct pcibus_attach_args pba;
	pci_chipset_tag_t pc = &sc->sc_pc;
	const struct bonito_config *bc;
	pcireg_t rev;

	rev = PCI_REVISION(REGVAL(BONITO_PCICLASS));

	printf(": memory and PCI controller, %s rev. %d.%d\n",
	    BONITO_REV_FPGA(rev) ? "FPGA" : "ASIC",
	    BONITO_REV_MAJOR(rev), BONITO_REV_MINOR(rev));

	bc = &yeelong_bonito;

	sc->sc_bonito = bc;
	SLIST_INIT(&sc->sc_hook);

	/*
	 * Setup interrupt handling.
	 */

	REGVAL(BONITO_GPIOIE) = bc->bc_gpioIE;
	REGVAL(BONITO_INTEDGE) = bc->bc_intEdge;
	REGVAL(BONITO_INTSTEER) = 0;
	REGVAL(BONITO_INTPOL) = bc->bc_intPol;
	REGVAL(BONITO_INTENCLR) = -1L;
	(void)REGVAL(BONITO_INTENCLR);
	bonito_isaimr = bonito_get_isa_imr();

	set_intr(INTPRI_BONITO, CR_INT_4, bonito_intr);
	set_intr(INTPRI_ISA, CR_INT_0, bonito_isa_intr);
	register_splx_handler(bonito_splx);

	/*
	 * Attach PCI bus.
	 */

	pc->pc_conf_v = sc;
	pc->pc_attach_hook = bonito_attach_hook;
	pc->pc_bus_maxdevs = bonito_bus_maxdevs;
	pc->pc_make_tag = bonito_make_tag;
	pc->pc_decompose_tag = bonito_decompose_tag;
	pc->pc_conf_read = bonito_conf_read;
	pc->pc_conf_write = bonito_conf_write;

	pc->pc_intr_v = sc;
	pc->pc_intr_map = bonito_pci_intr_map;
	pc->pc_intr_string = bonito_pci_intr_string;
	pc->pc_intr_establish = bonito_pci_intr_establish;
	pc->pc_intr_disestablish = bonito_pci_intr_disestablish;

	bzero(&pba, sizeof pba);
	pba.pba_busname = "pci";
	pba.pba_iot = &bonito_pci_io_space_tag;
	pba.pba_memt = &bonito_pci_mem_space_tag;
	pba.pba_dmat = &bonito_bus_dma_tag;
	pba.pba_pc = pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;
	/* XXX setup extents: I/O is only BONITO_PCIIO_SIZE long, memory is
	  BONITO_PCILO_SIZE and BONITO_PCIHI_SIZE */

	config_found(&sc->sc_dev, &pba, bonito_print);
}

bus_addr_t
bonito_pa_to_device(paddr_t pa)
{
	return pa ^ loongson_dma_base;
}

paddr_t
bonito_device_to_pa(bus_addr_t addr)
{
	return addr ^ loongson_dma_base;
}

int
bonito_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);

	return UNCONF;
}

/*
 * Bonito interrupt handling
 */

void *
bonito_intr_establish(int irq, int type, int level, int (*handler)(void *),
    void *arg, const char *name)
{
	struct intrhand **p, *q, *ih;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= BONITO_NINTS || irq == BONITO_ISA_IRQ(2) || irq < 0)
		panic("bonito_intr_establish: illegal irq %d", irq);
#endif

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih_next = NULL;
	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_irq, &evcount_intr);

	s = splhigh();

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &bonito_intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		;
	*p = ih;

	bonito_intem |= 1UL << irq;
	bonito_intr_makemasks();

	splx(s);	/* causes hw mask update */

	return (ih);
}

void
bonito_intr_disestablish(void *ih)
{
	/* XXX */
	panic("%s not implemented", __func__);
}

void
bonito_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	bonito_setintrmask(newipl);
	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Recompute interrupt masks.
 */
void
bonito_intr_makemasks()
{
	int irq, level;
	struct intrhand *q;
	uint intrlevel[BONITO_NINTS];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < BONITO_NINTS; irq++) {
		uint levels = 0;
		for (q = bonito_intrhand[irq]; q != NULL; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/*
	 * Then figure out which IRQs use each level.
	 * Note that we make sure never to overwrite imask[IPL_HIGH], in
	 * case an interrupt occurs during intr_disestablish() and causes
	 * an unfortunate splx() while we are here recomputing the masks.
	 */
	for (level = IPL_NONE; level < IPL_HIGH; level++) {
		uint64_t irqs = 0;
		for (irq = 0; irq < BONITO_NINTS; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1UL << irq;
		bonito_imask[level] = irqs;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	bonito_imask[IPL_NET] |= bonito_imask[IPL_BIO];
	bonito_imask[IPL_TTY] |= bonito_imask[IPL_NET];
	bonito_imask[IPL_VM] |= bonito_imask[IPL_TTY];
	bonito_imask[IPL_CLOCK] |= bonito_imask[IPL_VM];

	/*
	 * These are pseudo-levels.
	 */
	bonito_imask[IPL_NONE] = 0;
	bonito_imask[IPL_HIGH] = -1UL;
}

/*
 * Process direct interrupts
 */

uint32_t
bonito_intr(uint32_t hwpend, struct trap_frame *frame)
{
	uint64_t imr, isr, mask;
	int bit;
	struct intrhand *ih;
	int rc;

	isr = REGVAL(BONITO_INTISR) & YEELONG_INTRMASK_LVL4;
	imr = REGVAL(BONITO_INTEN);
	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

#ifdef DEBUG
	printf("pci interrupt: imr %04x isr %04x\n", imr, isr);
#endif
	/*
	 * Mask all pending interrupts.
	 */
	REGVAL(BONITO_INTENCLR) = isr;
	(void)REGVAL(BONITO_INTENCLR);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & bonito_imask[frame->ipl]) != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process allowed interrupts.
	 */
	if (isr != 0) {
		int lvl, bitno;
		uint64_t tmpisr;

		/* Service higher level interrupts first */
		bit = YEELONG_INTR_DRAM_PARERR;
		for (lvl = IPL_HIGH - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr & (bonito_imask[lvl] ^ bonito_imask[lvl - 1]);
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = bonito_intrhand[bitno]; ih != NULL;
				    ih = ih->ih_next) {
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						ih->ih_count.ec_count++;
					}
				}
				if (rc == 0)
					printf("spurious interrupt %d\n", bit);

				isr ^= mask;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		REGVAL(BONITO_INTENSET) = imr;
		(void)REGVAL(BONITO_INTENSET);
	}

	return hwpend;
}

/*
 * Process ISA interrupts.
 *
 * XXX ISA interrupts only occur on YEELONG_INTR_INT0, but since the other
 * XXX YEELONG_INTR_INT# are unmaskable, bad things will happen if they
 * XXX are triggered...
 */

/*
 * Interrupt dispatcher.
 */
uint32_t
bonito_isa_intr(uint32_t hwpend, struct trap_frame *frame)
{
	uint64_t imr, isr, mask;
	int bit;
	struct intrhand *ih;
	int rc;

	isr = bonito_get_isa_isr();
	imr = bonito_get_isa_imr();

	isr &= imr;
	isr &= ~(1 << 2);	/* cascade */
#ifdef DEBUG
	printf("isa interrupt: imr %04x isr %04x\n", imr, isr);
#endif
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */

	bonito_set_isa_imr(imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & (bonito_imask[frame->ipl] >> 16)) != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process allowed interrupts.
	 */
	if (isr != 0) {
		int lvl, bitno;
		uint64_t tmpisr;

		/* Service higher level interrupts first */
		bit = 15;
		for (lvl = IPL_HIGH - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr &
			    ((bonito_imask[lvl] ^ bonito_imask[lvl - 1]) >> 16);
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = bonito_intrhand[bitno + 16];
				    ih != NULL; ih = ih->ih_next) {
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						ih->ih_count.ec_count++;
					}
				}
				if (rc == 0)
					printf("spurious isa interrupt %d\n",
					    bit);

				bonito_isa_specific_eoi(bitno);

				isr ^= mask;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		bonito_set_isa_imr(imr);
	}

	return hwpend;
}


void
bonito_setintrmask(int level)
{
	uint64_t active = bonito_intem & ~bonito_imask[level];

	REGVAL(BONITO_INTENCLR) = bonito_imask[level] & 0xffff;
	REGVAL(BONITO_INTENSET) = active & 0xffff;
	(void)REGVAL(BONITO_INTENSET);
	bonito_set_isa_imr(active >> 16);
}

/*
 * various PCI helpers
 */

void
bonito_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	pci_chipset_tag_t pc = pba->pba_pc;
	pcireg_t id;
	pcitag_t tag;
	int dev;

	if (pba->pba_bus != 0)
		return;

	/*
	 * Check for an AMD CS5536 chip; if one is found, register
	 * the proper PCI configuration space hooks.
	 */

	for (dev = pci_bus_maxdevs(pc, pba->pba_bus); dev >= 0; dev--) {
		tag = pci_make_tag(pc, pba->pba_bus, dev, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (id == PCI_ID_CODE(PCI_VENDOR_AMD,
		    PCI_PRODUCT_AMD_CS5536_PCISB)) {
			glx_init(pc, tag, dev);
			break;
		}
	}
}

/*
 * PCI configuration space access routines
 */

/* Bonito systems are always single-processor, so this is sufficient. */
#define	PCI_CONF_LOCK(s)	(s) = splhigh()
#define	PCI_CONF_UNLOCK(s)	splx((s))

int
bonito_bus_maxdevs(void *v, int busno)
{
	struct bonito_softc *sc = v;
	const struct bonito_config *bc = sc->sc_bonito;

	return busno == 0 ? 32 - bc->bc_adbase : 32;
}

pcitag_t
bonito_make_tag(void *v, int b, int d, int f)
{
	return (b << 16) | (d << 11) | (f << 8);
}

void
bonito_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
}

int
bonito_conf_addr(struct bonito_softc *sc, pcitag_t tag, int offset,
    u_int32_t *cfgoff, u_int32_t *pcimap_cfg)
{
	int b, d, f;

	bonito_decompose_tag(sc, tag, &b, &d, &f);

	if (b == 0) {
		d += sc->sc_bonito->bc_adbase;
		if (d > 31)
			return 1;
		*cfgoff = (1 << d) | (f << 8) | offset;
		*pcimap_cfg = 0;
	} else {
		*cfgoff = tag | offset;
		*pcimap_cfg = BONITO_PCIMAPCFG_TYPE1;
	}

	return 0;
}

/* PCI Configuration Space access hook structure */
struct bonito_cfg_hook {
	SLIST_ENTRY(bonito_cfg_hook) next;
	int	(*read)(void *, pci_chipset_tag_t, pcitag_t, int, pcireg_t *);
	int	(*write)(void *, pci_chipset_tag_t, pcitag_t, int, pcireg_t);
	void	*cookie;
};

int
bonito_pci_hook(pci_chipset_tag_t pc, void *cookie,
    int (*r)(void *, pci_chipset_tag_t, pcitag_t, int, pcireg_t *),
    int (*w)(void *, pci_chipset_tag_t, pcitag_t, int, pcireg_t))
{
	struct bonito_softc *sc = pc->pc_conf_v;
	struct bonito_cfg_hook *bch;

	bch = malloc(sizeof *bch, M_DEVBUF, M_NOWAIT);
	if (bch == NULL)
		return ENOMEM;

	bch->read = r;
	bch->write = w;
	bch->cookie = cookie;
	SLIST_INSERT_HEAD(&sc->sc_hook, bch, next);
	return 0;
}

pcireg_t
bonito_conf_read(void *v, pcitag_t tag, int offset)
{
	struct bonito_softc *sc = v;
	pcireg_t data;
	u_int32_t cfgoff, dummy, pcimap_cfg;
	struct bonito_cfg_hook *hook;
	int s;

	SLIST_FOREACH(hook, &sc->sc_hook, next) {
		if (hook->read != NULL &&
		    (*hook->read)(hook->cookie, &sc->sc_pc, tag, offset,
		      &data) != 0)
			return data;
	}

	if (bonito_conf_addr(sc, tag, offset, &cfgoff, &pcimap_cfg))
		return (pcireg_t)-1;

	PCI_CONF_LOCK(s);

	/* clear aborts */
	REGVAL(BONITO_PCICMD) |=
	    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

	/* high 16 bits of address go into PciMapCfg register */
	REGVAL(BONITO_PCIMAP_CFG) = (cfgoff >> 16) | pcimap_cfg;

	wbflush();
	/* Issue a read to make sure the write is posted */
	dummy = REGVAL(BONITO_PCIMAP_CFG);

	/* low 16 bits of address are offset into config space */
	data = REGVAL(BONITO_PCICFG_BASE + (cfgoff & 0xfffc));

	/* check for error */
	if (REGVAL(BONITO_PCICMD) &
	    (PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT)) {
		REGVAL(BONITO_PCICMD) |=
		    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;
		data = (pcireg_t) -1;
	}

	PCI_CONF_UNLOCK(s);

	return data;
}

void
bonito_conf_write(void *v, pcitag_t tag, int offset, pcireg_t data)
{
	struct bonito_softc *sc = v;
	u_int32_t cfgoff, dummy, pcimap_cfg;
	struct bonito_cfg_hook *hook;
	int s;

	SLIST_FOREACH(hook, &sc->sc_hook, next) {
		if (hook->write != NULL &&
		    (*hook->write)(hook->cookie, &sc->sc_pc, tag, offset,
		      data) != 0)
			return;
	}

	if (bonito_conf_addr(sc, tag, offset, &cfgoff, &pcimap_cfg))
		panic("bonito_conf_write");

	PCI_CONF_LOCK(s);

	/* clear aborts */
	REGVAL(BONITO_PCICMD) |=
	    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

	/* high 16 bits of address go into PciMapCfg register */
	REGVAL(BONITO_PCIMAP_CFG) = (cfgoff >> 16) | pcimap_cfg;

	wbflush();
	/* Issue a read to make sure the write is posted */
	dummy = REGVAL(BONITO_PCIMAP_CFG);

	/* low 16 bits of address are offset into config space */
	REGVAL(BONITO_PCICFG_BASE + (cfgoff & 0xfffc)) = data;

	PCI_CONF_UNLOCK(s);
}

/*
 * PCI Interrupt handling
 */

int
bonito_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int dev, fn, pin;

	*ihp = -1;

	if (pa->pa_intrpin == 0)	/* no interrupt needed */
		return 1;

#ifdef DIAGNOSTIC
	if (pa->pa_intrpin > 4) {
		printf("%s: bad interrupt pin %d\n", __func__, pa->pa_intrpin);
		return 1;
	}
#endif

	pci_decompose_tag(pa->pa_pc, pa->pa_tag, NULL, &dev, &fn);
	if (pa->pa_bridgetag) {
		pin = PPB_INTERRUPT_SWIZZLE(pa->pa_rawintrpin, dev);
		*ihp = pa->pa_bridgeih[pin - 1];
	} else {
		switch (dev) {
		/* onboard devices, only pin A is wired */
		case 6:
		case 7:
		case 8:
		case 9:
			if (pa->pa_intrpin == PCI_INTERRUPT_PIN_A)
				*ihp = BONITO_DIRECT_IRQ(YEELONG_INTR_PCIA +
				    (dev - 6));
			break;
		/* PCI slot */
		case 10:
			*ihp = BONITO_DIRECT_IRQ(YEELONG_INTR_PCIA +
			    (pa->pa_intrpin - PCI_INTERRUPT_PIN_A));
			break;
		/* Geode chip */
		case 14:
			switch (fn) {
			case 1:	/* Flash */
				*ihp = BONITO_ISA_IRQ(6);
				break;
			case 2:	/* AC97 */
				*ihp = BONITO_ISA_IRQ(9);
				break;
			case 4:	/* OHCI */
			case 5:	/* EHCI */
				*ihp = BONITO_ISA_IRQ(11);
				break;
			}
			break;
		default:
			break;
		}

		if (*ihp < 0)
			return 1;
	}

	return 0;
}

const char *
bonito_pci_intr_string(void *cookie, pci_intr_handle_t ih)
{
	static char irqstr[1 + 12];

	if (BONITO_IRQ_IS_ISA(ih))
		snprintf(irqstr, sizeof irqstr, "isa irq %d", ih - 16);
	else
		snprintf(irqstr, sizeof irqstr, "irq %d", ih);
	return irqstr;
}

void *
bonito_pci_intr_establish(void *cookie, pci_intr_handle_t ih, int level,
    int (*cb)(void *), void *cbarg, char *name)
{
	return bonito_intr_establish(ih, IST_LEVEL, level, cb, cbarg, name);
}

void
bonito_pci_intr_disestablish(void *cookie, void *ihp)
{
	bonito_intr_disestablish(ihp);
}

uint
bonito_get_isa_imr()
{
	uint imr1, imr2;

	imr1 = 0xff & ~REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1);
	imr1 &= ~(1 << 2);	/* hide cascade */
	imr2 = 0xff & ~REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1);

	return (imr2 << 8) | imr1;
}

uint
bonito_get_isa_isr()
{
	uint isr1, isr2;

	isr1 = 0xff & REGVAL8(BONITO_PCIIO_BASE + IO_ICU1);
	isr2 = 0xff & REGVAL8(BONITO_PCIIO_BASE + IO_ICU2);

	return (isr2 << 8) | isr1;
}

void
bonito_set_isa_imr(uint newimr)
{
	uint imr1, imr2;

	imr1 = 0xff & ~newimr;
	imr1 &= ~(1 << 2);	/* enable cascade */
	imr2 = 0xff & ~(newimr >> 8);

	if ((newimr ^ bonito_isaimr) & 0xff00) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1) = imr2;
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1);
	}
	if ((newimr ^ bonito_isaimr) & 0x00ff) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1) = imr1;
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1);
	}
	bonito_isaimr = newimr;
}

void
bonito_isa_specific_eoi(int bit)
{
	if (bit & 8) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 0) = 0x60 | (bit & 7);
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 0);
		bit = 2;
	}
	REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 0) = 0x60 | bit;
	(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 0);
}

void
isa_attach_hook(struct device *parent, struct device *self,
    struct isabus_attach_args *iba)
{
}

void *
isa_intr_establish(isa_chipset_tag_t ic, int irq, int type, int level,
    int (*handler)(void *), void *arg, char *name)
{
	return bonito_intr_establish(BONITO_ISA_IRQ(irq), type, level,
	    handler, arg, name);
}

void
isa_intr_disestablish(void *v, void *ih)
{
	bonito_intr_disestablish(ih);
}

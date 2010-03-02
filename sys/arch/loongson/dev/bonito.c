/*	$OpenBSD: bonito.c,v 1.13 2010/03/02 20:53:12 miod Exp $	*/
/*	$NetBSD: bonito_mainbus.c,v 1.11 2008/04/28 20:23:10 martin Exp $	*/
/*	$NetBSD: bonito_pci.c,v 1.5 2008/04/28 20:23:28 martin Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
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
/*
 * XXX The legacy interrupt scheme is entangled and as many parts as
 * XXX possible should move to pcib.c...
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <mips64/archtype.h>

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
#include <loongson/dev/bonito_irq.h>

#if 0
#define	BONITO_DEBUG
#endif

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
uint32_t bonito_intr(uint32_t, struct trap_frame *);

void	 bonito_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	 bonito_bus_maxdevs(void *, int);
pcitag_t bonito_make_tag(void *, int, int, int);
void	 bonito_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t bonito_conf_read(void *, pcitag_t, int);
pcireg_t bonito_conf_read_internal(const struct bonito_config *, pcitag_t, int);
void	 bonito_conf_write(void *, pcitag_t, int, pcireg_t);
int	 bonito_pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *
	 bonito_pci_intr_string(void *, pci_intr_handle_t);
void	*bonito_pci_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	 bonito_pci_intr_disestablish(void *, void *);

int	 bonito_conf_addr(const struct bonito_config *, pcitag_t, int,
	    u_int32_t *, u_int32_t *);

void	 bonito_splx(int);
void	 bonito_setintrmask(int);

uint	 loongson_get_isa_imr(void);
uint	 loongson_get_isa_isr(void);
void	 loongson_set_isa_imr(uint);
void	 loongson_isa_specific_eoi(int);
uint32_t loongson_isa_intr(uint32_t, struct trap_frame *);
void	 loongson_isa_splx(int);
void	 loongson_isa_setintrmask(int);

/*
 * Bonito interrupt handling declarations.
 * See <loongson/dev/bonito_irq.h> for details.
 */
struct intrhand *bonito_intrhand[BONITO_NINTS];
uint64_t bonito_intem;
uint64_t bonito_imask[NIPLS];

uint	loongson_isaimr;

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

int     bonito_io_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int     bonito_mem_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

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
	._space_map = bonito_io_map,
	._space_unmap = generic_space_unmap,
	._space_subregion = generic_space_region,
	._space_vaddr = generic_space_vaddr
};

struct mips_bus_space bonito_pci_mem_space_tag = {
	.bus_base = PHYS_TO_XKPHYS(0, CCA_NC),
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
	._space_map = bonito_mem_map,
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

void
bonito_attach(struct device *parent, struct device *self, void *aux)
{
	struct bonito_softc *sc = (struct bonito_softc *)self;
	struct pcibus_attach_args pba;
	pci_chipset_tag_t pc = &sc->sc_pc;
	const struct bonito_config *bc;
	uint32_t reg;

	/*
	 * Loongson 2F processors do not use a real Bonito64 chip but
	 * their own derivative, which is no longer 100% compatible.
	 * We need to make sure we never try to access an unimplemented
	 * register...
	 */
	if (curcpu()->ci_hw.type == MIPS_LOONGSON2 &&
	    (curcpu()->ci_hw.c0prid & 0xff) == 0x2f - 0x2c)
		sc->sc_compatible = 0;
	else
		sc->sc_compatible = 1;

	reg = PCI_REVISION(REGVAL(BONITO_PCI_REG(PCI_CLASS_REG)));
	if (sc->sc_compatible) {
		printf(": BONITO Memory and PCI controller, %s rev %d.%d\n",
		    BONITO_REV_FPGA(reg) ? "FPGA" : "ASIC",
		    BONITO_REV_MAJOR(reg), BONITO_REV_MINOR(reg));
	} else {
		printf(": memory and PCI-X controller, rev %d\n",
		    PCI_REVISION(REGVAL(BONITO_PCI_REG(PCI_CLASS_REG))));
	}

	bc = sys_platform->bonito_config;
	sc->sc_bonito = bc;
	SLIST_INIT(&sc->sc_hook);

#ifdef BONITO_DEBUG
	if (!sc->sc_compatible)
		printf("ISR4C: %08x\n", REGVAL(BONITO_PCI_REG(0x4c)));
	printf("PCIMAP: %08x\n", REGVAL(BONITO_PCIMAP));
	printf("MEMWIN: %08x.%08x - %08x.%08x\n",
	    REGVAL(BONITO_MEM_WIN_BASE_H), REGVAL(BONITO_MEM_WIN_BASE_L),
	    REGVAL(BONITO_MEM_WIN_MASK_H), REGVAL(BONITO_MEM_WIN_MASK_L));
	if (!sc->sc_compatible) {
		printf("HITSEL0: %08x.%08x\n",
		    REGVAL(LOONGSON_PCI_HIT0_SEL_H),
		    REGVAL(LOONGSON_PCI_HIT0_SEL_L));
		printf("HITSEL1: %08x.%08x\n",
		    REGVAL(LOONGSON_PCI_HIT1_SEL_H),
		    REGVAL(LOONGSON_PCI_HIT1_SEL_L));
		printf("HITSEL2: %08x.%08x\n",
		    REGVAL(LOONGSON_PCI_HIT2_SEL_H),
		    REGVAL(LOONGSON_PCI_HIT2_SEL_L));
	}
	printf("PCI BAR 0:%08x 1:%08x 2:%08x 3:%08x 4:%08x 5:%08x\n",
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 0 * 4)),
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 1 * 4)),
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 2 * 4)),
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 3 * 4)),
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 4 * 4)),
	    REGVAL(BONITO_PCI_REG(PCI_MAPREG_START + 5 * 4)));
#endif

	/*
	 * Setup proper abitration.
	 */

	if (!sc->sc_compatible) {
		/*
		 * According to Linux, changing the value of this register
		 * ``avoids deadlock of PCI reading/writing lock operation''.
		 *
		 * Unfortunately, documentation for the Implementation
		 * Specific Registers (ISR40 to ISR5C) is only found in the
		 * chinese version of the Loongson 2F documentation.
		 *
		 * The particular bit we set here is ``mas_read_defer''.
		 */
		/* c2000001 -> d2000001 */
		REGVAL(BONITO_PCI_REG(0x4c)) |= 0x10000000;

		/* all pci devices may need to hold the bus */
		reg = REGVAL(LOONGSON_PXARB_CFG);
		reg &= ~LOONGSON_PXARB_RUDE_DEV_MSK;
		reg |= 0xfe << LOONGSON_PXARB_RUDE_DEV_SHFT;
		REGVAL(LOONGSON_PXARB_CFG) = reg;
		(void)REGVAL(LOONGSON_PXARB_CFG);
	}

	/*
	 * Setup interrupt handling.
	 */

	REGVAL(BONITO_GPIOIE) = bc->bc_gpioIE;
	REGVAL(BONITO_INTEDGE) = bc->bc_intEdge;
	if (sc->sc_compatible)
		REGVAL(BONITO_INTSTEER) = bc->bc_intSteer;
	REGVAL(BONITO_INTPOL) = bc->bc_intPol;

	REGVAL(BONITO_INTENCLR) = 0xffffffff;
	(void)REGVAL(BONITO_INTENCLR);
	
	if (bc->bc_legacy_pic) {
		set_intr(INTPRI_BONITO, CR_INT_4, bonito_intr);
		set_intr(INTPRI_ISA, CR_INT_0, loongson_isa_intr);
		loongson_isaimr = loongson_get_isa_imr();
		register_splx_handler(loongson_isa_splx);
	} else {
		set_intr(INTPRI_BONITO, CR_INT_4, bonito_intr);
		register_splx_handler(bonito_splx);
	}

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
#ifdef notyet
	pba.pba_ioex = bonito_get_resource_extent(pc, 1);
	pba.pba_memex = bonito_get_resource_extent(pc, 0);
#endif

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

/*
 * Update interrupt masks. This is for designs without legacy PIC.
 */

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

void
bonito_setintrmask(int level)
{
	uint64_t active;
	uint32_t clear, set;
	uint32_t sr;

	active = bonito_intem & ~bonito_imask[level];
	/* don't bother masking high bits, there are no isa interrupt sources */
	clear = bonito_imask[level];
	set = active;

	sr = disableintr();

	if (clear != 0)
		REGVAL(BONITO_INTENCLR) = clear;
	if (set != 0)
		REGVAL(BONITO_INTENSET) = set;
	(void)REGVAL(BONITO_INTENSET);

	setsr(sr);
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
 * Process native interrupts
 */

uint32_t
bonito_intr(uint32_t hwpend, struct trap_frame *frame)
{
	uint64_t imr, isr, mask;
	int bit;
	struct intrhand *ih;
	int rc;

	isr = REGVAL(BONITO_INTISR) & LOONGSON_INTRMASK_LVL4;
	imr = REGVAL(BONITO_INTEN);
	isr &= imr;
#ifdef DEBUG
	printf("pci interrupt: imr %04x isr %04x\n", imr, isr);
#endif
	if (isr == 0)
		return 0;	/* not for us */

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
		bit = LOONGSON_INTR_DRAM_PARERR; /* skip non-pci interrupts */
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
					printf("spurious interrupt %d\n",
					    bitno);

				if ((isr ^= mask) == 0)
					goto done;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}
done:

		/*
		 * Reenable interrupts which have been serviced.
		 */
		REGVAL(BONITO_INTENSET) = imr;
		(void)REGVAL(BONITO_INTENSET);
	}

	return hwpend;
}

/*
 * various PCI helpers
 */

void
bonito_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	pci_chipset_tag_t pc = pba->pba_pc;
	struct bonito_softc *sc = pc->pc_conf_v;
	const struct bonito_config *bc = sc->sc_bonito;

	if (pba->pba_bus != 0)
		return;

	(*bc->bc_attach_hook)(pc);
}

/*
 * PCI configuration space access routines
 */

int
bonito_bus_maxdevs(void *v, int busno)
{
	struct bonito_softc *sc = v;
	const struct bonito_config *bc = sc->sc_bonito;

	return busno == 0 ? 32 - bc->bc_adbase : 32;
}

pcitag_t
bonito_make_tag(void *unused, int b, int d, int f)
{
	return (b << 16) | (d << 11) | (f << 8);
}

void
bonito_decompose_tag(void *unused, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
}

int
bonito_conf_addr(const struct bonito_config *bc, pcitag_t tag, int offset,
    u_int32_t *cfgoff, u_int32_t *pcimap_cfg)
{
	int b, d, f;

	bonito_decompose_tag(NULL, tag, &b, &d, &f);

	if (b == 0) {
		d += bc->bc_adbase;
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
	struct bonito_cfg_hook *hook;
	pcireg_t data;

	SLIST_FOREACH(hook, &sc->sc_hook, next) {
		if (hook->read != NULL &&
		    (*hook->read)(hook->cookie, &sc->sc_pc, tag, offset,
		      &data) != 0)
			return data;
	}

	return bonito_conf_read_internal(sc->sc_bonito, tag, offset);
}

pcireg_t
bonito_conf_read_internal(const struct bonito_config *bc, pcitag_t tag,
    int offset)
{
	pcireg_t data;
	u_int32_t cfgoff, pcimap_cfg;
	uint32_t sr;
	uint64_t imr;

	if (bonito_conf_addr(bc, tag, offset, &cfgoff, &pcimap_cfg))
		return (pcireg_t)-1;

	sr = disableintr();
	imr = REGVAL(BONITO_INTEN);
	REGVAL(BONITO_INTENCLR) = 0xffffffff;
	(void)REGVAL(BONITO_INTENCLR);

	/* clear aborts */
	REGVAL(BONITO_PCI_REG(PCI_COMMAND_STATUS_REG)) |=
	    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

	/* high 16 bits of address go into PciMapCfg register */
	REGVAL(BONITO_PCIMAP_CFG) = (cfgoff >> 16) | pcimap_cfg;
	(void)REGVAL(BONITO_PCIMAP_CFG);
	wbflush();

	/* low 16 bits of address are offset into config space */
	data = REGVAL(BONITO_PCICFG_BASE + (cfgoff & 0xfffc));

	/* check for error */
	if (REGVAL(BONITO_PCI_REG(PCI_COMMAND_STATUS_REG)) &
	    (PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT)) {
		REGVAL(BONITO_PCI_REG(PCI_COMMAND_STATUS_REG)) |=
		    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;
		data = (pcireg_t) -1;
	}

	REGVAL(BONITO_INTENSET) = imr;
	(void)REGVAL(BONITO_INTENSET);
	setsr(sr);

	return data;
}

void
bonito_conf_write(void *v, pcitag_t tag, int offset, pcireg_t data)
{
	struct bonito_softc *sc = v;
	u_int32_t cfgoff, pcimap_cfg;
	struct bonito_cfg_hook *hook;
	uint32_t sr;
	uint64_t imr;

	SLIST_FOREACH(hook, &sc->sc_hook, next) {
		if (hook->write != NULL &&
		    (*hook->write)(hook->cookie, &sc->sc_pc, tag, offset,
		      data) != 0)
			return;
	}

	if (bonito_conf_addr(sc->sc_bonito, tag, offset, &cfgoff, &pcimap_cfg))
		panic("bonito_conf_write");

	sr = disableintr();
	imr = REGVAL(BONITO_INTEN);
	REGVAL(BONITO_INTENCLR) = 0xffffffff;
	(void)REGVAL(BONITO_INTENCLR);

	/* clear aborts */
	REGVAL(BONITO_PCI_REG(PCI_COMMAND_STATUS_REG)) |=
	    PCI_STATUS_MASTER_ABORT | PCI_STATUS_MASTER_TARGET_ABORT;

	/* high 16 bits of address go into PciMapCfg register */
	REGVAL(BONITO_PCIMAP_CFG) = (cfgoff >> 16) | pcimap_cfg;
	(void)REGVAL(BONITO_PCIMAP_CFG);
	wbflush();

	/* low 16 bits of address are offset into config space */
	REGVAL(BONITO_PCICFG_BASE + (cfgoff & 0xfffc)) = data;

	REGVAL(BONITO_INTENSET) = imr;
	(void)REGVAL(BONITO_INTENSET);
	setsr(sr);
}

/*
 * PCI Interrupt handling
 */

int
bonito_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct bonito_softc *sc = pa->pa_pc->pc_intr_v;
	const struct bonito_config *bc = sc->sc_bonito;
	int bus, dev, fn, pin;

	*ihp = -1;

	if (pa->pa_intrpin == 0)	/* no interrupt needed */
		return 1;

#ifdef DIAGNOSTIC
	if (pa->pa_intrpin > 4) {
		printf("%s: bad interrupt pin %d\n", __func__, pa->pa_intrpin);
		return 1;
	}
#endif

	pci_decompose_tag(pa->pa_pc, pa->pa_tag, &bus, &dev, &fn);
	if (pa->pa_bridgetag) {
		pin = PPB_INTERRUPT_SWIZZLE(pa->pa_rawintrpin, dev);
		*ihp = pa->pa_bridgeih[pin - 1];
	} else {
		if (bus == 0)
			*ihp = (*bc->bc_intr_map)(dev, fn, pa->pa_intrpin);

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
		snprintf(irqstr, sizeof irqstr, "isa irq %d",
		    ih - BONITO_NDIRECT);
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

/*
 * Legacy (ISA) interrupt handling
 */

/*
 * Process legacy interrupts.
 *
 * XXX ISA interrupts only occur on LOONGSON_INTR_INT0, but since the other
 * XXX LOONGSON_INTR_INT# are unmaskable, bad things will happen if they
 * XXX are triggered...
 */

/*
 * Interrupt dispatcher.
 */
uint32_t
loongson_isa_intr(uint32_t hwpend, struct trap_frame *frame)
{
	uint64_t imr, isr, mask;
	int bit;
	struct intrhand *ih;
	int rc;

	isr = loongson_get_isa_isr();
	imr = loongson_get_isa_imr();

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

	loongson_set_isa_imr(imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & (BONITO_ISA_MASK(bonito_imask[frame->ipl]))) != 0) {
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
		bit = BONITO_NISA - 1;
		for (lvl = IPL_HIGH - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr & BONITO_ISA_MASK(bonito_imask[lvl] ^
			    bonito_imask[lvl - 1]);
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = bonito_intrhand[BONITO_ISA_IRQ(bitno)];
				    ih != NULL; ih = ih->ih_next) {
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						ih->ih_count.ec_count++;
					}
				}
				if (rc == 0)
					printf("spurious isa interrupt %d\n",
					    bitno);

				loongson_isa_specific_eoi(bitno);

				if ((isr ^= mask) == 0)
					goto done;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}
done:

		/*
		 * Reenable interrupts which have been serviced.
		 */
		loongson_set_isa_imr(imr);
	}

	return hwpend;
}

#define	REGVAL8(x)	*((volatile u_int8_t *)PHYS_TO_XKPHYS(x, CCA_NC))

uint
loongson_get_isa_imr()
{
	uint imr1, imr2;

	imr1 = 0xff & ~REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1);
	imr1 &= ~(1 << 2);	/* hide cascade */
	imr2 = 0xff & ~REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1);

	return (imr2 << 8) | imr1;
}

uint
loongson_get_isa_isr()
{
	uint isr1, isr2;

	isr1 = 0xff & REGVAL8(BONITO_PCIIO_BASE + IO_ICU1);
	isr2 = 0xff & REGVAL8(BONITO_PCIIO_BASE + IO_ICU2);

	return (isr2 << 8) | isr1;
}

void
loongson_set_isa_imr(uint newimr)
{
	uint imr1, imr2;

	imr1 = 0xff & ~newimr;
	imr1 &= ~(1 << 2);	/* enable cascade */
	imr2 = 0xff & ~(newimr >> 8);

	/*
	 * For some reason, trying to write the same value to the PIC
	 * registers causes an immediate system freeze, so we only do
	 * this if the value changes.
	 * Note that interrupts have been disabled by the caller.
	 */
	if ((newimr ^ loongson_isaimr) & 0xff00) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1) = imr2;
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 1);
	}
	if ((newimr ^ loongson_isaimr) & 0x00ff) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1) = imr1;
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 1);
	}
	loongson_isaimr = newimr;
}

void
loongson_isa_specific_eoi(int bit)
{
	if (bit & 8) {
		REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 0) = 0x60 | (bit & 7);
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 0);
		bit = 2;
	}
	REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 0) = 0x60 | bit;
	(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU1 + 0);
}

/*
 * Update interrupt masks. This is for designs with legacy PIC.
 */

void
loongson_isa_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	loongson_isa_setintrmask(newipl);
	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

void
loongson_isa_setintrmask(int level)
{
	uint64_t active;
	uint32_t clear, set;
	uint32_t sr;

	active = bonito_intem & ~bonito_imask[level];
	clear = BONITO_DIRECT_MASK(bonito_imask[level]);
	set = BONITO_DIRECT_MASK(active);

	sr = disableintr();

	if (clear != 0)
		REGVAL(BONITO_INTENCLR) = clear;
	if (set != 0)
		REGVAL(BONITO_INTENSET) = set;
	(void)REGVAL(BONITO_INTENSET);

	loongson_set_isa_imr(BONITO_ISA_MASK(active));

	setsr(sr);
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

/*
 * Legacy I/O access protection.
 * Since MI ISA code does not expect bus access to cause any failure when
 * accessing missing hardware, but only receive bogus data in return, we
 * force bus_space_map() to fail if there is no hardware there.
 */

int
bonito_io_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	const struct legacy_io_range *r;

	if (offs < BONITO_PCIIO_LEGACY) {
		size--;
		for (r = sys_platform->legacy_io_ranges; r->start != 0; r++)
			if (offs >= r->start && offs + size <= r->end)
				break;

		if (r->start == 0)
			return ENXIO;
	}

	*bshp = t->bus_base + offs;
	return 0;
}

/*
 * PCI memory access.
 * Things are a bit complicated here, as we can either use one of the 64MB
 * windows in PCILO space (making sure ranges spanning multiple windows will
 * turn contiguous), or a direct access within the PCIHI space.
 * Note that, on 2F systems, only the PCIHI range for which CPU->PCI accesses
 * are enabled in the crossbar is usable.
 */

int
bonito_mem_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	uint32_t pcimap;
	bus_addr_t pcilo_w[3];
	bus_addr_t ws, we, w;
	bus_addr_t end = offs + size - 1;
	int is2f, pcilo_window;

	/*
	 * Try a PCIHI mapping first.
	 */

	/* may be used before curcpu() points to valid data */
	if ((cp0_get_prid() & 0xffff) ==
	    ((MIPS_LOONGSON2 << 8) | (0x2f - 0x2c)))
		is2f = 1;
	else
		is2f = 0;

	if (is2f) {
		if (offs >= LS2F_PCIHI_BASE && end <= LS2F_PCIHI_TOP) {
			*bshp = t->bus_base + offs;
			return 0;
		}
	} else {
		/* PCI1.5 */
		if (offs >= BONITO_PCIHI_BASE && end <= BONITO_PCIHI_TOP) {
			*bshp = t->bus_base + offs;
			return 0;
		}

		/* PCI2 */
		w = pcimap & BONITO_PCIMAP_PCIMAP_2 ? 0x80000000UL : 0;
		if (offs >= w && end < (w + 0x80000000UL)) {
			*bshp = t->bus_base + 0x80000000UL + (offs - w);
			return 0;
		}
	}

	/*
	 * No luck, try a PCILO mapping.
	 */

	/*
	 * Decode PCIMAP, and figure out what PCILO mappings are
	 * possible.
	 */

	pcimap = REGVAL(BONITO_PCIMAP);
	pcilo_w[0] = (pcimap & BONITO_PCIMAP_PCIMAP_LO0) >>
	    BONITO_PCIMAP_PCIMAP_LO0_SHIFT;
	pcilo_w[1] = (pcimap & BONITO_PCIMAP_PCIMAP_LO1) >>
	    BONITO_PCIMAP_PCIMAP_LO1_SHIFT;
	pcilo_w[2] = (pcimap & BONITO_PCIMAP_PCIMAP_LO2) >>
	    BONITO_PCIMAP_PCIMAP_LO2_SHIFT;

	/*
	 * Check if the 64MB areas we want to span are all available as
	 * contiguous PCILO mappings.
	 */

	ws = offs >> 26;
	we = end >> 26;

	pcilo_window = -1;
	if (ws == pcilo_w[0])
		pcilo_window = 0;
	else if (ws == pcilo_w[1])
		pcilo_window = 1;
	else if (ws == pcilo_w[2])
		pcilo_window = 2;

	if (pcilo_window >= 0) {
		/* contiguous area test */
		for (w = ws + 1; w <= we; w++) {
			if (pcilo_window + (w - ws) > 2 ||
			    w != pcilo_w[pcilo_window + (w - ws)]) {
				pcilo_window = -1;
				break;
			}
		}
	}

	if (pcilo_window >= 0) {
		*bshp = t->bus_base + BONITO_PCILO_BASE +
		    BONITO_PCIMAP_WINBASE(pcilo_window) +
		    BONITO_PCIMAP_WINOFFSET(offs);
		return 0;
	}

	return EINVAL;
}

/*
 * PCI resource handling
 */

struct extent *
bonito_get_resource_extent(pci_chipset_tag_t pc, int io)
{
	struct bonito_softc *sc = pc->pc_conf_v;
	struct extent *ex;
	char *exname;
	size_t exnamesz;
	uint32_t reg;
	int errors;

	exnamesz = 1 + 16 + 4;
	exname = (char *)malloc(exnamesz, M_DEVBUF, M_NOWAIT);
	if (exname == NULL)
		return NULL;
	snprintf(exname, exnamesz, "%s%s", sc->sc_dev.dv_xname,
	    io ? "_io" : "_mem");

	ex = extent_create(exname, 0, 0xffffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT | EX_FILLED);
	if (ex == NULL)
		goto out;

	errors = 0;
	if (io) {
		/*
		 * Reserve the low 16KB of I/O space to the legacy hardware,
		 * if any.
		 */
		if (extent_free(ex, BONITO_PCIIO_LEGACY, BONITO_PCIIO_SIZE,
		    EX_NOWAIT) != 0)
			errors++;
	} else {
		reg = REGVAL(BONITO_PCIMAP);
		if (extent_free(ex,
		    BONITO_PCIMAP_WINBASE((reg & BONITO_PCIMAP_PCIMAP_LO0) >>
		      BONITO_PCIMAP_PCIMAP_LO0_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT) != 0)
			errors++;
		if (extent_free(ex,
		    BONITO_PCIMAP_WINBASE((reg & BONITO_PCIMAP_PCIMAP_LO1) >>
		      BONITO_PCIMAP_PCIMAP_LO1_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT) != 0)
			errors++;
		if (extent_free(ex,
		    BONITO_PCIMAP_WINBASE((reg & BONITO_PCIMAP_PCIMAP_LO2) >>
		      BONITO_PCIMAP_PCIMAP_LO2_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT) != 0)
			errors++;

		if (sc->sc_compatible) {
			/* XXX make PCIMAP_HI available if PCIMAP_2 set */
		}
	}

	if (errors != 0) {
		extent_destroy(ex);
		ex = NULL;
	}

#ifdef BONITO_DEBUG
	extent_print(ex);
#endif

out:
	if (exname != NULL)
		free(exname, M_DEVBUF);

	return ex;
}

/*
 * Functions used during early system configuration (before bonito attaches).
 */

pcitag_t
pci_make_tag_early(int b, int d, int f)
{
	return bonito_make_tag(NULL, b, d, f);
}

pcireg_t
pci_conf_read_early(pcitag_t tag, int reg)
{
	return bonito_conf_read_internal(sys_platform->bonito_config, tag, reg);
}

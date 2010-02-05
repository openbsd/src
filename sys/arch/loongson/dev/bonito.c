/*	$OpenBSD: bonito.c,v 1.7 2010/02/05 20:53:24 miod Exp $	*/
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
#include <loongson/dev/glxvar.h>

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

uint	 bonito_get_isa_imr(void);
uint	 bonito_get_isa_isr(void);
void	 bonito_set_isa_imr(uint);
void	 bonito_isa_specific_eoi(int);

uint32_t bonito_isa_intr(uint32_t, struct trap_frame *);

void	 bonito_splx(int);
void	 bonito_setintrmask(int);

void	 bonito_isa_splx(int);
void	 bonito_isa_setintrmask(int);

/*
 * Bonito interrupt handling declarations.
 * See <loongson/dev/bonito_irq.h> for details.
 */
struct intrhand *bonito_intrhand[BONITO_NINTS];
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

void
bonito_attach(struct device *parent, struct device *self, void *aux)
{
	struct bonito_softc *sc = (struct bonito_softc *)self;
	struct pcibus_attach_args pba;
	pci_chipset_tag_t pc = &sc->sc_pc;
	const struct bonito_config *bc;
	struct extent *ioex, *memex;
	uint32_t reg;
	int real_bonito;

	/*
	 * Loongson 2F processors do not use a real Bonito64 chip but
	 * their own derivative, which is no longer 100% compatible.
	 * We need to make sure we never try to access an unimplemented
	 * register...
	 */
	if (curcpu()->ci_hw.type == MIPS_LOONGSON2 &&
	    (curcpu()->ci_hw.c0prid & 0xff) == 0x2f - 0x2c)
		real_bonito = 0;
	else
		real_bonito = 1;

	reg = PCI_REVISION(REGVAL(BONITO_PCI_REG(PCI_CLASS_REG)));
	if (real_bonito) {
		printf(": BONITO Memory and PCI controller, %s rev %d.%d\n",
		    BONITO_REV_FPGA(reg) ? "FPGA" : "ASIC",
		    BONITO_REV_MAJOR(reg), BONITO_REV_MINOR(reg));
	} else {
		printf(": memory and PCI-X controller, rev %d\n",
		    PCI_REVISION(REGVAL(BONITO_PCI_REG(PCI_CLASS_REG))));
	}

	bc = sys_config.sys_bc;
	sc->sc_bonito = bc;
	SLIST_INIT(&sc->sc_hook);

	/*
	 * Setup proper abitration.
	 */

	if (!real_bonito) {
		if (sys_config.system_type == LOONGSON_YEELOONG) {
			/*
			 * According to Linux, changing the value of this
			 * undocumented register ``avoids deadlock of PCI
			 * reading/writing lock operation''.
			 * Is this really necessary, and if so, does it
			 * matter on other designs?
			 */
			REGVAL(BONITO_PCI_REG(0x4c)) =  0xd2000001;
							/* was c2000001 */
		}

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
	if (real_bonito)
		REGVAL(BONITO_INTSTEER) = bc->bc_intSteer;
	REGVAL(BONITO_INTPOL) = bc->bc_intPol;

	REGVAL(BONITO_INTENCLR) = 0xffffffff;
	(void)REGVAL(BONITO_INTENCLR);
	
	switch (sys_config.system_type) {
	case LOONGSON_YEELOONG:
		set_intr(INTPRI_BONITO, CR_INT_4, bonito_intr);
		set_intr(INTPRI_ISA, CR_INT_0, bonito_isa_intr);
		bonito_isaimr = bonito_get_isa_imr();
		register_splx_handler(bonito_isa_splx);
		break;
	case LOONGSON_GDIUM:
		set_intr(INTPRI_BONITO, CR_INT_4, bonito_intr);
		register_splx_handler(bonito_splx);
		break;
	default:
		/* we should have died way earlier */
		panic("missing interrupt configuration code for this system");
		break;
	}

	/*
	 * Setup PCI resource extents.
	 */

	ioex = extent_create("pciio", 0, 0xffffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT | EX_FILLED);
	if (ioex != NULL)
		(void)extent_free(ioex, 0, BONITO_PCIIO_SIZE, EX_NOWAIT);

	memex = extent_create("pcimem", 0, 0xffffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT | EX_FILLED);
	if (memex != NULL) {
		reg = REGVAL(BONITO_PCIMAP);
		(void)extent_free(memex, BONITO_PCIMAP_WINBASE((reg &
		    BONITO_PCIMAP_PCIMAP_LO0) >> BONITO_PCIMAP_PCIMAP_LO0_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT);
		(void)extent_free(memex, BONITO_PCIMAP_WINBASE((reg &
		    BONITO_PCIMAP_PCIMAP_LO1) >> BONITO_PCIMAP_PCIMAP_LO1_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT);
		(void)extent_free(memex, BONITO_PCIMAP_WINBASE((reg &
		    BONITO_PCIMAP_PCIMAP_LO2) >> BONITO_PCIMAP_PCIMAP_LO2_SHIFT),
		    BONITO_PCIMAP_WINSIZE, EX_NOWAIT);

		if (real_bonito) {
			/* XXX make PCIMAP_HI available if PCIMAP_2 set */
		}
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
	pba.pba_ioex = ioex;
	pba.pba_memex = memex;

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
 * Update interrupt masks. Two set of routines: one when ISA interrupts
 * are involved, one without.
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
bonito_isa_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	bonito_isa_setintrmask(newipl);
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

void
bonito_isa_setintrmask(int level)
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

	bonito_set_isa_imr(BONITO_ISA_MASK(active));

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
 * Process direct interrupts
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
					printf("spurious interrupt %d\n", bit);

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
 * Process ISA interrupts.
 *
 * XXX ISA interrupts only occur on LOONGSON_INTR_INT0, but since the other
 * XXX LOONGSON_INTR_INT# are unmaskable, bad things will happen if they
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
					    bit);

				bonito_isa_specific_eoi(bitno);

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
		bonito_set_isa_imr(imr);
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
 * ISA Interrupt handling
 */

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

	/*
	 * For some reason, trying to write the same value to the PIC
	 * registers causes an immediate system freeze, so we only do
	 * this if the value changes.
	 * Note that interrupts have been disabled by the caller.
	 */
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
		(void)REGVAL8(BONITO_PCIIO_BASE + IO_ICU2 + 0);
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
	return bonito_conf_read_internal(sys_config.sys_bc, tag, reg);
}

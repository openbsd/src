/*	$OpenBSD: obio.c,v 1.6 2010/11/23 18:46:29 syuu Exp $ */

/*
 * Copyright (c) 2000-2004 Opsycon AB  (www.opsycon.se)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * This is a obio driver.
 * It handles configuration of all devices on the processor bus except UART.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/atomic.h>

#include <octeon/dev/octeonreg.h>
#include <octeon/dev/obiovar.h>

#define OBIO_NINTS 64

int	 obiomatch(struct device *, void *, void *);
void	 obioattach(struct device *, struct device *, void *);
int	 obioprint(void *, const char *);
int	 obiosubmatch(struct device *, void *, void *);

void	 obio_intr_makemasks(void);
void	 obio_splx(int);
uint32_t obio_iointr(uint32_t, struct trap_frame *);
uint32_t obio_aux(uint32_t, struct trap_frame *);
int	 obio_iointr_skip(struct intrhand *, uint64_t, uint64_t);
void	 obio_setintrmask(int);

u_int8_t obio_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t obio_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t obio_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t obio_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 obio_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 obio_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 obio_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 obio_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 obio_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 obio_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 obio_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 obio_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 obio_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 obio_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

int	 obio_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 obio_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 obio_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*obio_space_vaddr(bus_space_tag_t, bus_space_handle_t);

bus_addr_t obio_pa_to_device(paddr_t);
paddr_t	 obio_device_to_pa(bus_addr_t);

struct cfattach obio_ca = {
	sizeof(struct device), obiomatch, obioattach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

bus_space_t obio_tag = {
	PHYS_TO_XKPHYS(0, CCA_NC),
	NULL,
	obio_read_1, obio_write_1,
	obio_read_2, obio_write_2,
	obio_read_4, obio_write_4,
	obio_read_8, obio_write_8,
	obio_read_raw_2, obio_write_raw_2,
	obio_read_raw_4, obio_write_raw_4,
	obio_read_raw_8, obio_write_raw_8,
	obio_space_map, obio_space_unmap, obio_space_region,
	obio_space_vaddr
};

bus_space_handle_t obio_h;

struct machine_bus_dma_tag obio_bus_dma_tag = {
	NULL,			/* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_load_buffer,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	obio_pa_to_device,
	obio_device_to_pa,
	0
};

struct intrhand *obio_intrhand[OBIO_NINTS];

#define	INTPRI_CIU_0	(INTPRI_CLOCK + 1)

uint64_t obio_intem[MAXCPUS];
uint64_t obio_imask[MAXCPUS][NIPLS];

/*
 * List of obio child devices.
 */

#define	OBIODEV(name, addr, i) \
	{ name, &obio_tag, &obio_tag, &obio_bus_dma_tag, addr, i }
struct obio_attach_args obio_children[] = {
	OBIODEV("octcf", OCTEON_CF_BASE, 0),
	OBIODEV("pcibus", 0, 0),
};
#undef	OBIODEV

/*
 * Match bus only to targets which have this bus.
 */
int
obiomatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

int
obioprint(void *aux, const char *obio)
{
	struct obio_attach_args *oba = aux;

	if (obio != NULL)
		printf("%s at %s", oba->oba_name, obio);

	if (oba->oba_baseaddr != 0)
		printf(" base 0x%llx", oba->oba_baseaddr);
	if (oba->oba_intr >= 0)
		printf(" irq %d", oba->oba_intr);

	return (UNCONF);
}

int
obiosubmatch(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct obio_attach_args *oba = args;

	if (strcmp(cf->cf_driver->cd_name, oba->oba_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)oba->oba_baseaddr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, oba);
}

void
obioattach(struct device *parent, struct device *self, void *aux)
{
	uint i;

	/*
	 * Map and setup CRIME control registers.
	 */
	if (bus_space_map(&obio_tag, OCTEON_CIU_BASE, OCTEON_CIU_SIZE, 0,
		&obio_h)) {
		printf(": can't map CIU control registers\n");
		return;
	}

	printf("\n");

	obio_intr_init();

	set_intr(INTPRI_CIU_0, CR_INT_0, obio_iointr);
	register_splx_handler(obio_splx);

	/*
	 * Attach subdevices.
	 */
	for (i = 0; i < nitems(obio_children); i++)
		config_found_sm(self, obio_children + i,
		    obioprint, obiosubmatch);
}

/*
 * Bus access primitives. These are really ugly...
 */

u_int8_t
obio_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (u_int8_t)*(volatile uint8_t *)(h + o);
}

u_int16_t
obio_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (u_int16_t)*(volatile uint16_t *)(h + o);
}

u_int32_t
obio_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return (u_int32_t)*(volatile u_int32_t *)(h + o);
}

u_int64_t
obio_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int64_t *)(h + o);
}

void
obio_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	*(volatile uint8_t *)(h + o) = (volatile uint8_t)v;
}

void
obio_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int16_t v)
{
	*(volatile uint16_t *)(h + o) = (volatile uint16_t)v;
}

void
obio_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int32_t v)
{
	*(volatile u_int32_t *)(h + o) = (volatile uint32_t)v;
}

void
obio_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int64_t v)
{
	*(volatile u_int64_t *)(h + o) = v;
}

void
obio_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = *addr;
		buf += 2;
	}
}

void
obio_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*addr = *(uint16_t *)buf;
		buf += 2;
	}
}

void
obio_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = *addr;
		buf += 4;
	}
}

void
obio_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(uint32_t *)buf;
		buf += 4;
	}
}

void
obio_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = *addr;
		buf += 8;
	}
}

void
obio_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = *(uint64_t *)buf;
		buf += 8;
	}
}

int
obio_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	if (ISSET(flags, BUS_SPACE_MAP_KSEG0)) {
		*bshp = PHYS_TO_CKSEG0(offs);
		return 0;
	}
	if (ISSET(flags, BUS_SPACE_MAP_CACHEABLE))
		offs +=
		    PHYS_TO_XKPHYS(0, CCA_CACHED) - PHYS_TO_XKPHYS(0, CCA_NC);
	*bshp = t->bus_base + offs;
	return 0;
}

void
obio_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
obio_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void *
obio_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

/*
 * Obio bus_dma helpers.
 */

bus_addr_t
obio_pa_to_device(paddr_t pa)
{
	return (bus_addr_t)pa;
}

paddr_t
obio_device_to_pa(bus_addr_t addr)
{
	return (paddr_t)addr;
}

/*
 * Obio interrupt handler driver.
 */

void
obio_intr_init(void)
{
	int cpuid = cpu_number();
	bus_space_write_8(&obio_tag, obio_h, CIU_IP2_EN0(cpuid), 0);
	bus_space_write_8(&obio_tag, obio_h, CIU_IP3_EN0(cpuid), 0);
	bus_space_write_8(&obio_tag, obio_h, CIU_IP2_EN1(cpuid), 0);
	bus_space_write_8(&obio_tag, obio_h, CIU_IP3_EN1(cpuid), 0);
}

/*
 * Establish an interrupt handler called from the dispatcher.
 * The interrupt function established should return zero if there was nothing
 * to serve (no int) and non-zero when an interrupt was serviced.
 *
 * Interrupts are numbered from 1 and up where 1 maps to HW int 0.
 * XXX There is no reason to keep this... except for hardcoded interrupts
 * XXX in kernel configuration files...
 */
void *
obio_intr_establish(int irq, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *ih_what)
{
	int cpuid = cpu_number();
	struct intrhand **p, *q, *ih;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= OBIO_NINTS || irq < 0)
		panic("intr_establish: illegal irq %d", irq);
#endif

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih_next = NULL;
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_level = level;
	ih->ih_irq = irq;
	evcount_attach(&ih->ih_count, ih_what, (void *)&ih->ih_irq);

	s = splhigh();

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &obio_intrhand[irq]; (q = *p) != NULL;
	    p = (struct intrhand **)&q->ih_next)
		;
	*p = ih;

	obio_intem[cpuid] |= 1UL << irq;
	obio_intr_makemasks();

	splx(s);	/* causes hw mask update */

	return (ih);
}

void
obio_intr_disestablish(void *ih)
{
	/* XXX */
	panic("%s not implemented", __func__);
}

void
obio_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	if (CPU_IS_PRIMARY(ci))
		obio_setintrmask(newipl);
	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Recompute interrupt masks.
 */
void
obio_intr_makemasks()
{
	int cpuid = cpu_number();
	int irq, level;
	struct intrhand *q;
	uint intrlevel[OBIO_NINTS];

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < OBIO_NINTS; irq++) {
		uint levels = 0;
		for (q = (struct intrhand *)obio_intrhand[irq]; q != NULL; 
			q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/*
	 * Then figure out which IRQs use each level.
	 * Note that we make sure never to overwrite imask[IPL_HIGH], in
	 * case an interrupt occurs during intr_disestablish() and causes
	 * an unfortunate splx() while we are here recomputing the masks.
	 */
	for (level = IPL_NONE; level < NIPLS; level++) {
		uint64_t irqs = 0;
		for (irq = 0; irq < OBIO_NINTS; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1UL << irq;
		obio_imask[cpuid][level] = irqs;
	}
	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	obio_imask[cpuid][IPL_NET] |= obio_imask[cpuid][IPL_BIO];
	obio_imask[cpuid][IPL_TTY] |= obio_imask[cpuid][IPL_NET];
	obio_imask[cpuid][IPL_VM] |= obio_imask[cpuid][IPL_TTY];
	obio_imask[cpuid][IPL_CLOCK] |= obio_imask[cpuid][IPL_VM];
	obio_imask[cpuid][IPL_HIGH] |= obio_imask[cpuid][IPL_CLOCK];
	obio_imask[cpuid][IPL_IPI] |= obio_imask[cpuid][IPL_HIGH];

	/*
	 * These are pseudo-levels.
	 */
	obio_imask[cpuid][IPL_NONE] = 0;
}

/*
 * Interrupt dispatcher.
 */
uint32_t
obio_iointr(uint32_t hwpend, struct trap_frame *frame)
{
	struct cpu_info *ci = curcpu();
	int cpuid = cpu_number();
	uint64_t imr, isr, mask;
	int ipl;
	int bit;
	struct intrhand *ih;
	int rc;
	uint64_t sum0 = CIU_IP2_SUM0(cpuid);
	uint64_t en0 = CIU_IP2_EN0(cpuid);

	isr = bus_space_read_8(&obio_tag, obio_h, sum0);
	imr = bus_space_read_8(&obio_tag, obio_h, en0);
	bit = 63;

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	bus_space_write_8(&obio_tag, obio_h, en0, imr & ~isr);

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & obio_imask[cpuid][frame->ipl]) != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process allowed interrupts.
	 */
	if (isr != 0) {
		int lvl, bitno;
		uint64_t tmpisr;

		__asm__ (".set noreorder\n");
		ipl = ci->ci_ipl;
		__asm__ ("sync\n\t.set reorder\n");

		/* Service higher level interrupts first */
		for (lvl = NIPLS - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr & (obio_imask[cpuid][lvl] ^ obio_imask[cpuid][lvl - 1]);
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = (struct intrhand *)obio_intrhand[bitno];
					ih != NULL;
				    ih = ih->ih_next) {
#ifdef MULTIPROCESSOR
					u_int32_t sr;
#endif
					splraise(ih->ih_level);
#ifdef MULTIPROCESSOR
					if (ih->ih_level < IPL_IPI) {
						sr = getsr();
						ENABLEIPI();
						if (ipl < IPL_SCHED)
							__mp_lock(&kernel_lock);
					}
#endif
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						atomic_add_uint64(&ih->ih_count.ec_count, 1);
					}
#ifdef MULTIPROCESSOR
					if (ih->ih_level < IPL_IPI) {
						if (ipl < IPL_SCHED)
							__mp_unlock(&kernel_lock);
						setsr(sr);
					}
#endif
					__asm__ (".set noreorder\n");
					ci->ci_ipl = ipl;
					__asm__ ("sync\n\t.set reorder\n");
				}
				if (rc == 0)
					printf("spurious crime interrupt %d\n", bitno);

				isr ^= mask;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		bus_space_write_8(&obio_tag, obio_h, en0, imr);
	}

	return hwpend;
}

void
obio_setintrmask(int level)
{
	int cpuid = cpu_number();

	bus_space_write_8(&obio_tag, obio_h, CIU_IP2_EN0(cpuid),
		obio_intem[cpuid] & ~obio_imask[cpuid][level]);
}

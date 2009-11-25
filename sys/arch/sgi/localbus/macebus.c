/*	$OpenBSD: macebus.c,v 1.56 2009/11/25 17:39:51 syuu Exp $ */

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
 * This is a combined macebus/crimebus driver. It handles configuration of all
 * devices on the processor bus.
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

#include <sgi/localbus/crimebus.h>
#include <sgi/localbus/macebus.h>
#include <sgi/localbus/macebusvar.h>

int	 macebusmatch(struct device *, void *, void *);
void	 macebusattach(struct device *, struct device *, void *);
int	 macebusprint(void *, const char *);
int	 macebussubmatch(struct device *, void *, void *);

void	 macebus_intr_makemasks(void);
void	 macebus_splx(int);
uint32_t macebus_iointr(uint32_t, struct trap_frame *);
uint32_t macebus_aux(uint32_t, struct trap_frame *);
int	 macebus_iointr_skip(struct intrhand *, uint64_t, uint64_t);
void	 crime_setintrmask(int);

u_int8_t mace_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t mace_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t mace_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t mace_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 mace_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 mace_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 mace_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 mace_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 mace_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 mace_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 mace_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 mace_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 mace_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 mace_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

int	 mace_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 mace_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 mace_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*mace_space_vaddr(bus_space_tag_t, bus_space_handle_t);

bus_addr_t macebus_pa_to_device(paddr_t);
paddr_t	 macebus_device_to_pa(bus_addr_t);

struct cfattach macebus_ca = {
	sizeof(struct device), macebusmatch, macebusattach
};

struct cfdriver macebus_cd = {
	NULL, "macebus", DV_DULL
};

bus_space_t macebus_tag = {
	PHYS_TO_XKPHYS(MACEBUS_BASE, CCA_NC),
	NULL,
	mace_read_1, mace_write_1,
	mace_read_2, mace_write_2,
	mace_read_4, mace_write_4,
	mace_read_8, mace_write_8,
	mace_read_raw_2, mace_write_raw_2,
	mace_read_raw_4, mace_write_raw_4,
	mace_read_raw_8, mace_write_raw_8,
	mace_space_map, mace_space_unmap, mace_space_region,
	mace_space_vaddr
};

bus_space_t crimebus_tag = {
	PHYS_TO_XKPHYS(CRIMEBUS_BASE, CCA_NC),
	NULL,
	mace_read_1, mace_write_1,
	mace_read_2, mace_write_2,
	mace_read_4, mace_write_4,
	mace_read_8, mace_write_8,
	mace_read_raw_2, mace_write_raw_2,
	mace_read_raw_4, mace_write_raw_4,
	mace_read_raw_8, mace_write_raw_8,
	mace_space_map, mace_space_unmap, mace_space_region,
	mace_space_vaddr
};

bus_space_handle_t crime_h;
bus_space_handle_t mace_h;

struct machine_bus_dma_tag mace_bus_dma_tag = {
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
	macebus_pa_to_device,
	macebus_device_to_pa,
	CRIME_MEMORY_MASK
};

/*
 * CRIME/MACE interrupt handling declarations: 32 CRIME sources, 32 MACE
 * sources (multiplexed by CRIME); 1 level.
 * We define another level for periodic tasks as well.
 */

struct crime_intrhand {
	struct	intrhand	ih;
	uint32_t		mace_irqmask;
};
struct crime_intrhand *crime_intrhand[CRIME_NINTS];

#define	INTPRI_MACEIO	(INTPRI_CLOCK + 1)
#define	INTPRI_MACEAUX	(INTPRI_MACEIO + 1)

uint64_t crime_intem, mace_intem;
uint64_t crime_imask[NIPLS];

/*
 * List of macebus child devices.
 */

#define	MACEBUSDEV(name, addr, i, i2) \
	{ name, &macebus_tag, &macebus_tag, &mace_bus_dma_tag, addr, i, i2 }
struct macebus_attach_args macebus_children[] = {
	MACEBUSDEV("com", MACE_ISA_SER1_OFFS, 4, MACE_ISA_INT_SERIAL_1),
	MACEBUSDEV("com", MACE_ISA_SER2_OFFS, 4, MACE_ISA_INT_SERIAL_2),
	MACEBUSDEV("dsrtc", MACE_ISA_RTC_OFFS, -1, 0),
#if 0
	MACEBUSDEV("lpt", MACE_ISA_EPP_OFFS, 4, MACE_ISA_INT_PARALLEL),
#endif
	MACEBUSDEV("macepcibr", MACE_PCI_OFFS, 7, 0),
	MACEBUSDEV("mavb", MACE_IO_AUDIO_OFFS, 6, MACE_ISA_INT_AUDIO),
	MACEBUSDEV("mec", MACE_ETHERNET_OFFS, 3, 0),
	MACEBUSDEV("mkbc", MACE_IO_KBC_OFFS, 5,
	    MACE_ISA_INT_KBD | MACE_ISA_INT_KBD_POLL |
	    MACE_ISA_INT_MOUSE | MACE_ISA_INT_MOUSE_POLL),
	MACEBUSDEV("power", 0, 5, MACE_ISA_INT_RTC)
};
#undef	MACEBUSDEV

/*
 * Match bus only to targets which have this bus.
 */
int
macebusmatch(struct device *parent, void *match, void *aux)
{
	if (sys_config.system_type == SGI_O2)
		return (1);
	return (0);
}

int
macebusprint(void *aux, const char *macebus)
{
	struct macebus_attach_args *maa = aux;

	if (macebus != NULL)
		printf("%s at %s", maa->maa_name, macebus);

	if (maa->maa_baseaddr != 0)
		printf(" base 0x%08x", maa->maa_baseaddr);
	if (maa->maa_intr >= 0)
		printf(" irq %d", maa->maa_intr);

	return (UNCONF);
}

int
macebussubmatch(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct macebus_attach_args *maa = args;

	if (strcmp(cf->cf_driver->cd_name, maa->maa_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)maa->maa_baseaddr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, maa);
}

void
macebusattach(struct device *parent, struct device *self, void *aux)
{
	u_int32_t creg;
	uint i;

	/*
	 * Map and setup CRIME control registers.
	 */
	if (bus_space_map(&crimebus_tag, 0x00000000, 0x400, 0, &crime_h)) {
		printf(": can't map CRIME control registers\n");
		return;
	}

	creg = bus_space_read_8(&crimebus_tag, crime_h, CRIME_REVISION);
	printf(": crime rev %d.%d\n", (creg & 0xf0) >> 4, creg & 0xf);

	bus_space_write_8(&crimebus_tag, crime_h, CRIME_CPU_ERROR_STAT, 0);
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_MEM_ERROR_STAT, 0);

	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_MASK, 0);
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_SOFT, 0);
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_HARD, 0);
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_STAT, 0);

	/*
	 * Map and setup MACE ISA control registers.
	 */
	if (bus_space_map(&macebus_tag, MACE_ISA_OFFS, 0x400, 0, &mace_h)) {
		printf("%s: can't map MACE control registers\n",
		    self->dv_xname);
		return;
	}

	bus_space_write_8(&macebus_tag, mace_h, MACE_ISA_INT_MASK, 0);
	bus_space_write_8(&macebus_tag, mace_h, MACE_ISA_INT_STAT, 0);

	/*
	 * On O2 systems all interrupts are handled by the macebus interrupt
	 * handler. Register all except clock.
	 */
	set_intr(INTPRI_MACEIO, CR_INT_0, macebus_iointr);
	register_splx_handler(macebus_splx);

	/* Set up a handler called when clock interrupts go off. */
	set_intr(INTPRI_MACEAUX, CR_INT_5, macebus_aux);

	/*
	 * Attach subdevices.
	 */
	for (i = 0; i < nitems(macebus_children); i++)
		config_found_sm(self, macebus_children + i,
		    macebusprint, macebussubmatch);
}

/*
 * Bus access primitives. These are really ugly...
 */

u_int8_t
mace_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int8_t *)(h + (o << 8) + 7);
}

u_int16_t
mace_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

u_int32_t
mace_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int32_t *)(h + o);
}

u_int64_t
mace_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile u_int64_t *)(h + o);
}

void
mace_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	*(volatile u_int8_t *)(h + (o << 8) + 7) = v;
}

void
mace_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int16_t v)
{
	panic(__func__);
}

void
mace_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int32_t v)
{
	*(volatile u_int32_t *)(h + o) = v;
}

void
mace_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int64_t v)
{
	*(volatile u_int64_t *)(h + o) = v;
}

void
mace_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
mace_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
mace_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile u_int32_t *addr = (volatile u_int32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(u_int32_t *)buf = *addr;
		buf += 4;
	}
}

void
mace_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile u_int32_t *addr = (volatile u_int32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(u_int32_t *)buf;
		buf += 4;
	}
}

void
mace_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	volatile u_int64_t *addr = (volatile u_int64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(u_int64_t *)buf = *addr;
		buf += 8;
	}
}

void
mace_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	volatile u_int64_t *addr = (volatile u_int64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = *(u_int64_t *)buf;
		buf += 8;
	}
}

int
mace_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	if (ISSET(flags, BUS_SPACE_MAP_CACHEABLE))
		offs +=
		    PHYS_TO_XKPHYS(0, CCA_CACHED) - PHYS_TO_XKPHYS(0, CCA_NC);
	*bshp = t->bus_base + offs;
	return 0;
}

void
mace_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
mace_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void *
mace_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

/*
 * Macebus bus_dma helpers.
 * Mace accesses memory contiguously at 0x40000000 onwards.
 */

bus_addr_t
macebus_pa_to_device(paddr_t pa)
{
	return (pa | CRIME_MEMORY_OFFSET);
}

paddr_t
macebus_device_to_pa(bus_addr_t addr)
{
	paddr_t pa = (paddr_t)addr & CRIME_MEMORY_MASK;

	if (pa >= 256 * 1024 * 1024)
		pa |= CRIME_MEMORY_OFFSET;

	return (pa);
}

/*
 * Macebus interrupt handler driver.
 */

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
macebus_intr_establish(int irq, uint32_t mace_irqmask, int type, int level,
    int (*ih_fun)(void *), void *ih_arg, const char *ih_what)
{
	struct crime_intrhand **p, *q, *ih;
	int s;

#ifdef DIAGNOSTIC
	if (irq >= CRIME_NINTS || irq < 0)
		panic("intr_establish: illegal irq %d", irq);
#endif

	ih = malloc(sizeof *ih, M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return NULL;

	ih->ih.ih_next = NULL;
	ih->ih.ih_fun = ih_fun;
	ih->ih.ih_arg = ih_arg;
	ih->ih.ih_level = level;
	ih->ih.ih_irq = irq;
	ih->mace_irqmask = mace_irqmask;
	evcount_attach(&ih->ih.ih_count, ih_what, (void *)&ih->ih.ih_irq,
	    &evcount_intr);

	s = splhigh();

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &crime_intrhand[irq]; (q = *p) != NULL;
	    p = (struct crime_intrhand **)&q->ih.ih_next)
		;
	*p = ih;

	crime_intem |= 1UL << irq;
	macebus_intr_makemasks();

	/* enable further MACE sources if necessary */
	if (mace_irqmask != 0) {
		mace_intem |= mace_irqmask;
		bus_space_write_8(&macebus_tag, mace_h, MACE_ISA_INT_MASK,
		    mace_intem);
	}

	splx(s);	/* causes hw mask update */

	return (ih);
}

void
macebus_intr_disestablish(void *ih)
{
	/* XXX */
	panic("%s not implemented", __func__);
}

void
macebus_splx(int newipl)
{
	struct cpu_info *ci = curcpu();

	/* Update masks to new ipl. Order highly important! */
	__asm__ (".set noreorder\n");
	ci->ci_ipl = newipl;
	__asm__ ("sync\n\t.set reorder\n");
	crime_setintrmask(newipl);
	/* If we still have softints pending trigger processing. */
	if (ci->ci_softpending != 0 && newipl < IPL_SOFTINT)
		setsoftintr0();
}

/*
 * Crime interrupt handler.
 */

#define	INTR_FUNCTIONNAME	macebus_iointr
#define	MASK_FUNCTIONNAME	macebus_intr_makemasks

#define	INTR_LOCAL_DECLS \
	uint64_t mace_isr, mace_imr;
#define	MASK_LOCAL_DECLS
#define	INTR_GETMASKS \
do { \
	isr = bus_space_read_8(&crimebus_tag, crime_h, CRIME_INT_STAT); \
	imr = bus_space_read_8(&crimebus_tag, crime_h, CRIME_INT_MASK); \
	if (((CRIME_INT_SUPER_IO | CRIME_INT_SUB_MISC | CRIME_INT_SUB_AUDIO) & \
	    isr & imr) != 0) { \
		mace_isr = bus_space_read_8(&macebus_tag, mace_h, \
		    MACE_ISA_INT_STAT); \
		mace_imr = bus_space_read_8(&macebus_tag, mace_h, \
		    MACE_ISA_INT_MASK); \
	} else \
		mace_isr = mace_imr = 0; \
	bit = 63; \
} while (0)
#define	INTR_MASKPENDING \
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_MASK, imr & ~isr)
#define	INTR_IMASK(ipl)		crime_imask[ipl]
#define	INTR_HANDLER(bit)	(struct intrhand *)crime_intrhand[bit]
#define	INTR_SPURIOUS(bit) \
do { \
	if (((CRIME_INT_SUPER_IO | CRIME_INT_SUB_MISC | CRIME_INT_SUB_AUDIO) & \
	    (1 << (bit))) != 0) { \
		if ((mace_isr & mace_imr) != 0) \
			printf("spurious crime interrupt %d" \
			    " mace isr %p imr %p\n", bit, mace_isr, mace_imr); \
	} else \
		printf("spurious crime interrupt %d\n", bit); \
} while (0)
#define	INTR_MASKRESTORE \
	bus_space_write_8(&crimebus_tag, crime_h, CRIME_INT_MASK, imr)
#define	INTR_MASKSIZE		CRIME_NINTS

#define	INTR_HANDLER_SKIP(ih) \
	macebus_iointr_skip((void *)ih, mace_isr, mace_imr)

#include <sgi/sgi/intr_template.c>

int
macebus_iointr_skip(struct intrhand *ih, uint64_t mace_isr, uint64_t mace_imr)
{
	struct crime_intrhand *mih = (struct crime_intrhand *)ih;

	/* do not skip pure CRIME interrupts */
	if (mih->mace_irqmask == 0)
		return 0;

	/*
	 * Several CRIME interrupts (such as superio and miscellaneous) are
	 * shared by multiple devices, so narrow the selection with the
	 * MACE interrupt status.
	 */

	if ((mace_isr & mace_imr & mih->mace_irqmask) != 0)
		return 0;

	return 1;
}

/*
 * Macebus auxilary functions run each clock interrupt.
 */
uint32_t
macebus_aux(uint32_t hwpend, struct trap_frame *cf)
{
	u_int64_t mask;

	mask = bus_space_read_8(&macebus_tag, mace_h, MACE_ISA_MISC_REG);
	mask |= MACE_ISA_MISC_RLED_OFF | MACE_ISA_MISC_GLED_OFF;

	/* GREEN - Idle */
	/* AMBER - System mode */
	/* RED   - User mode */
	if (cf->sr & SR_KSU_USER) {
		mask &= ~MACE_ISA_MISC_RLED_OFF;
	} else if (curproc == NULL ||
	    curproc == curcpu()->ci_schedstate.spc_idleproc) {
		mask &= ~MACE_ISA_MISC_GLED_OFF;
	} else {
		mask &= ~(MACE_ISA_MISC_RLED_OFF | MACE_ISA_MISC_GLED_OFF);
	}
	bus_space_write_8(&macebus_tag, mace_h, MACE_ISA_MISC_REG, mask);

	return 0;	/* Real clock int handler will claim the interrupt. */
}

void
crime_setintrmask(int level)
{
	*(volatile uint64_t *)(PHYS_TO_XKPHYS(CRIMEBUS_BASE, CCA_NC) +
	    CRIME_INT_MASK) = crime_intem & ~crime_imask[level];
}

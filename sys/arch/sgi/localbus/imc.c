/*	$OpenBSD: imc.c,v 1.19 2014/12/10 12:27:56 mikeb Exp $	*/
/*	$NetBSD: imc.c,v 1.32 2011/07/01 18:53:46 dyoung Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
/*
 * Copyright (c) 2001 Rafal K. Boni
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Indigo/Indigo2/Indy on-board Memory Controller support code.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>

#include <sgi/sgi/ip22.h>
#include <sgi/localbus/imcreg.h>
#include <sgi/localbus/imcvar.h>
#include <sgi/localbus/intreg.h>

#include <sgi/hpc/hpcreg.h>
#include <sgi/gio/gioreg.h>
#include <sgi/gio/giovar.h>

#include "eisa.h"

#if NEISA > 0
#include <dev/eisa/eisavar.h>
#endif

int	imc_match(struct device *, void *, void *);
void	imc_attach(struct device *, struct device *, void *);
int	imc_activate(struct device *, int);
int	imc_print(void *, const char *);

const struct cfattach imc_ca = {
	sizeof(struct device), imc_match, imc_attach, NULL, imc_activate
};

struct cfdriver imc_cd = {
	NULL, "imc", DV_DULL
};

uint32_t imc_bus_error(uint32_t, struct trap_frame *);
int	 imc_watchdog_cb(void *, int);

void	 imc_space_barrier(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, int);

/* can't be static for gio_cnattach() */
bus_space_t imcbus_tag = {
	PHYS_TO_XKPHYS(0, CCA_NC),
	NULL,
	imc_read_1, imc_write_1,
	imc_read_2, imc_write_2,
	imc_read_4, imc_write_4,
	imc_read_8, imc_write_8,
	imc_read_raw_2, imc_write_raw_2,
	imc_read_raw_4, imc_write_raw_4,
	imc_read_raw_8, imc_write_raw_8,
	imc_space_map, imc_space_unmap, imc_space_region,
	imc_space_vaddr, imc_space_barrier
};

#if NEISA > 0
void	 imc_eisa_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_eisa_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	 imc_eisa_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_eisa_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	 imc_eisa_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_eisa_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
int	 imc_eisa_io_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	 imc_eisa_io_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
int	 imc_eisa_mem_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	 imc_eisa_mem_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

static bus_space_t imcbus_eisa_io_tag = {
	PHYS_TO_XKPHYS(EISA_IO_BASE, CCA_NC),
	NULL,
	imc_read_1, imc_write_1,
	imc_read_2, imc_write_2,
	imc_read_4, imc_write_4,
	imc_read_8, imc_write_8,
	imc_eisa_read_raw_2, imc_eisa_write_raw_2,
	imc_eisa_read_raw_4, imc_eisa_write_raw_4,
	imc_eisa_read_raw_8, imc_eisa_write_raw_8,
	imc_eisa_io_map, imc_space_unmap, imc_eisa_io_region,
	imc_space_vaddr, imc_space_barrier
};
static bus_space_t imcbus_eisa_mem_tag = {
	PHYS_TO_XKPHYS(0, CCA_NC),
	NULL,
	imc_read_1, imc_write_1,
	imc_read_2, imc_write_2,
	imc_read_4, imc_write_4,
	imc_read_8, imc_write_8,
	imc_read_raw_2, imc_write_raw_2,
	imc_read_raw_4, imc_write_raw_4,
	imc_read_raw_8, imc_write_raw_8,
	imc_eisa_mem_map, imc_space_unmap, imc_eisa_mem_region,
	imc_space_vaddr, imc_space_barrier
};
#endif

bus_addr_t imc_pa_to_device(paddr_t);
paddr_t	 imc_device_to_pa(bus_addr_t);

/* can't be static for gio_cnattach() */
struct machine_bus_dma_tag imc_bus_dma_tag = {
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
	imc_pa_to_device,
	imc_device_to_pa,
	0
};

/*
 * Bus access primitives.
 */

uint8_t
imc_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint8_t *)(h + o);
}

uint16_t
imc_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint16_t *)(h + o);
}

uint32_t
imc_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint32_t *)(h + o);
}

uint64_t
imc_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint64_t *)(h + o);
}

void
imc_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint8_t v)
{
	*(volatile uint8_t *)(h + o) = v;
}

void
imc_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{
	*(volatile uint16_t *)(h + o) = v;
}

void
imc_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint32_t v)
{
	*(volatile uint32_t *)(h + o) = v;
}

void
imc_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, uint64_t v)
{
	*(volatile uint64_t *)(h + o) = v;
}

void
imc_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = *addr;
		buf += 2;
	}
}

void
imc_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*addr = *(uint16_t *)buf;
		buf += 2;
	}
}

void
imc_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = *addr;
		buf += 4;
	}
}

void
imc_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = *(uint32_t *)buf;
		buf += 4;
	}
}

void
imc_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = *addr;
		buf += 8;
	}
}

void
imc_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = *(uint64_t *)buf;
		buf += 8;
	}
}

int
imc_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	*bshp = t->bus_base + offs;
	return 0;
}

void
imc_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
imc_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return 0;
}

void *
imc_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

void
imc_space_barrier(bus_space_tag_t t, bus_space_handle_t h, bus_size_t offs,
    bus_size_t len, int flags)
{
	mips_sync();
}

#if NEISA > 0
void
imc_eisa_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = swap16(*addr);
		buf += 2;
	}
}

void
imc_eisa_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)(h + o);
	len >>= 1;
	while (len-- != 0) {
		*addr = swap16(*(uint16_t *)buf);
		buf += 2;
	}
}

void
imc_eisa_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = swap32(*addr);
		buf += 4;
	}
}

void
imc_eisa_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = swap32(*(uint32_t *)buf);
		buf += 4;
	}
}

void
imc_eisa_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = swap64(*addr);
		buf += 8;
	}
}

void
imc_eisa_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = swap64(*(uint64_t *)buf);
		buf += 8;
	}
}

int
imc_eisa_io_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	if (offs + size > EISA_IO_END - EISA_IO_BASE)
		return EINVAL;

	*bshp = t->bus_base + offs;
	return 0;
}

int
imc_eisa_io_region(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	if ((bsh - t->bus_base) + offset + size > EISA_IO_END - EISA_IO_BASE)
		return EINVAL;

	*nbshp = bsh + offset;
	return 0;
}

int
imc_eisa_mem_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	if ((offs >= EISA_MEM0_BASE && offs + size <= EISA_MEM0_END) ||
	    (offs >= EISA_MEM1_BASE && offs + size <= EISA_MEM1_END)) {
		*bshp = t->bus_base + offs;
		return 0;
	}

	return EINVAL;
}

int
imc_eisa_mem_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	bus_addr_t orig = bsh - t->bus_base;

	if ((orig >= EISA_MEM0_BASE && orig + offset + size <= EISA_MEM0_END) ||
	    (orig >= EISA_MEM1_BASE && orig + offset + size <= EISA_MEM1_END)) {
		*nbshp = t->bus_base + offset;
		return 0;
	}

	return EINVAL;
}
#endif

bus_addr_t
imc_pa_to_device(paddr_t pa)
{
	return (bus_addr_t)pa;
}

paddr_t
imc_device_to_pa(bus_addr_t addr)
{
	return (paddr_t)addr;
}

/*
 * For some reason, reading the arbitration register sometimes returns
 * wrong values, at least on IP20 (where the usual value is 0x400, but
 * nonsense values such as 0x34f have been witnessed).
 * Because of this, we'll treat the register as write-only, once we have
 * been able to read a supposedly safe value.
 * This variable contains the last known value written to this register.
 */
uint32_t imc_arb_value;

/*
 * Autoconf glue.
 */

int
imc_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = (void *)aux;

	switch (sys_config.system_type) {
	case SGI_IP20:
	case SGI_IP22:
	case SGI_IP26:
	case SGI_IP28:
		break;
	default:
		return 0;
	}

	return strcmp(maa->maa_name, imc_cd.cd_name) == 0;
}

void
imc_attach(struct device *parent, struct device *self, void *aux)
{
	struct imc_attach_args iaa;
#if NEISA > 0
	struct eisabus_attach_args eba;
#endif
	uint32_t reg, lastreg;
	uint32_t id, rev;
	int have_eisa;

	id = imc_read(IMC_SYSID);
	rev = id & IMC_SYSID_REVMASK;

	/* EISA exists on Indigo2 only */
	if (sys_config.system_type != SGI_IP20 &&
	    sys_config.system_subtype == IP22_INDIGO2)
		have_eisa = (id & IMC_SYSID_HAVEISA) != 0;
	else
		have_eisa = 0;

	printf(": revision %d\n", rev);

	/* Clear CPU/GIO error status registers to clear any leftover bits. */
	imc_bus_reset();

	/* Disable watchdog if leftover from previous reboot */
	imc_watchdog_cb(self, 0);

	/* Hook the bus error handler into the ISR */
	set_intr(INTPRI_BUSERR, CR_INT_4, imc_bus_error);

	/*
	 * Enable parity reporting on GIO/main memory transactions, except
	 * on systems with the ECC memory controller, where enabling parity
	 * interferes with regular operation and causes sticky false errors.
	 *
	 * Disable parity checking on CPU bus transactions (as turning
	 * it on seems to cause spurious bus errors), but enable parity
	 * checking on CPU reads from main memory (note that this bit
	 * has the opposite sense... Turning it on turns the checks off!).
	 *
	 * Finally, turn on interrupt writes to the CPU from the MC.
	 */
	reg = imc_read(IMC_CPUCTRL0);
	if (ip22_ecc)
		reg &= ~(IMC_CPUCTRL0_GPR | IMC_CPUCTRL0_MPR);
	else
		reg |= IMC_CPUCTRL0_GPR | IMC_CPUCTRL0_MPR;
	reg &= ~IMC_CPUCTRL0_NCHKMEMPAR;
	reg |= IMC_CPUCTRL0_INTENA;
	imc_write(IMC_CPUCTRL0, reg);

	/* Setup the MC write buffer depth */
	/*
	 * XXX This hardcoded value is not documented anywhere, and can be
	 * XXX traced back to DaveM's internship at SGI in 1996, so it can
	 * XXX be considered correct at least for IP24 (and, to a lesser
	 * XXX extent, IP22). IP20 and IP28 systems seem to run happy with
	 * XXX this value as well.
	 */
	reg = imc_read(IMC_CPUCTRL1);
	reg = (reg & ~IMC_CPUCTRL1_MCHWMSK) | 13;

	/*
	 * Force endianness on the onboard HPC and both slots.
	 * This should be safe for Fullhouse, but leave it conditional
	 * for now.
	 */
	switch (sys_config.system_type) {
	case SGI_IP22:
		if (sys_config.system_subtype == IP22_INDIGO2)
			break;
		/* FALLTHROUGH */
	case SGI_IP20:
		reg |= IMC_CPUCTRL1_HPCFX;
		reg |= IMC_CPUCTRL1_EXP0FX;
		reg |= IMC_CPUCTRL1_EXP1FX;
		reg &= ~IMC_CPUCTRL1_HPCLITTLE;
		reg &= ~IMC_CPUCTRL1_EXP0LITTLE;
		reg &= ~IMC_CPUCTRL1_EXP1LITTLE;
		break;
	}
	imc_write(IMC_CPUCTRL1, reg);

	/*
	 * Try and read the GIO64 arbitrator configuration register value.
	 * See comments above the declaration of imc_arb_value for why we
	 * are doing this.
	 */
	reg = 0; lastreg = ~reg;
	while (reg != lastreg || (reg & ~0xffff) != 0) {
		lastreg = reg;
		reg = imc_read(IMC_GIO64ARB);
		/* read another harmless register */
		(void)imc_read(IMC_CPUCTRL0);
	}

	/*
	 * Set GIO64 arbitrator configuration register:
	 *
	 * Preserve PROM-set graphics-related bits, as they seem to depend
	 * on the graphics variant present and I'm not sure how to figure
	 * that out or 100% sure what the correct settings are for each.
	 */
	reg &= (IMC_GIO64ARB_GRX64 | IMC_GIO64ARB_GRXRT | IMC_GIO64ARB_GRXMST);

	/*
	 * Rest of settings are machine/board dependent
	 */
	switch (sys_config.system_type) {
	case SGI_IP20:
		reg |= IMC_GIO64ARB_ONEGIO;
		reg |= IMC_GIO64ARB_EXP0RT | IMC_GIO64ARB_EXP1RT;
		reg |= IMC_GIO64ARB_EXP0MST | IMC_GIO64ARB_EXP1MST;
		reg &= ~(IMC_GIO64ARB_HPC64 |
		     IMC_GIO64ARB_HPCEXP64 | IMC_GIO64ARB_EISA64 |
		     IMC_GIO64ARB_EXP064 | IMC_GIO64ARB_EXP164 |
		     IMC_GIO64ARB_EXP0PIPE | IMC_GIO64ARB_EXP1PIPE);
		break;
	default:
		/*
		 * GIO64 invariant for all IP22 platforms: one GIO bus,
		 * HPC1 @ 64
		 */
		reg |= IMC_GIO64ARB_ONEGIO | IMC_GIO64ARB_HPC64;

		switch (sys_config.system_subtype) {
		default:
		case IP22_INDY:
		case IP22_CHALLS:
			/* XXX is MST mutually exclusive? */
			reg |= IMC_GIO64ARB_EXP0RT | IMC_GIO64ARB_EXP1RT;
			reg |= IMC_GIO64ARB_EXP0MST | IMC_GIO64ARB_EXP1MST;

			/* EISA (VINO, really) can bus-master, is 64-bit */
			reg |= IMC_GIO64ARB_EISAMST | IMC_GIO64ARB_EISA64;
			break;

		case IP22_INDIGO2:
			/*
			 * All Fullhouse boards have a 64-bit HPC2 and pipelined
			 * EXP0 slot.
			 */
			reg |= IMC_GIO64ARB_HPCEXP64 | IMC_GIO64ARB_EXP0PIPE;

			/*
			 * The EISA bus is the real thing, and is a 32-bit bus.
			 */
			reg &= ~IMC_GIO64ARB_EISA64;

			if (rev < 2) {
				/* EXP0 realtime, EXP1 can master */
				reg |= IMC_GIO64ARB_EXP0RT |
				    IMC_GIO64ARB_EXP1MST;
			} else {
				/* EXP1 pipelined as well, EISA masters */
				reg |= IMC_GIO64ARB_EXP1PIPE |
				    IMC_GIO64ARB_EISAMST;
			}
			break;
		}
	}

	imc_write(IMC_GIO64ARB, reg);
	imc_arb_value = reg;

	memset(&iaa, 0, sizeof(iaa));
	iaa.iaa_name = "gio";
	iaa.iaa_st = &imcbus_tag;
	iaa.iaa_dmat = &imc_bus_dma_tag;
	config_found(self, &iaa, imc_print);

#if NEISA > 0
	if (have_eisa) {
		memset(&eba, 0, sizeof(eba));
		eba.eba_busname = "eisa";
		eba.eba_iot = &imcbus_eisa_io_tag;
		eba.eba_memt = &imcbus_eisa_mem_tag;
		eba.eba_dmat = &imc_bus_dma_tag;
		eba.eba_ec = NULL;
		config_found(self, &eba, imc_print);
	}
#endif

#ifndef SMALL_KERNEL
	/* Register watchdog */
	wdog_register(imc_watchdog_cb, self);
#endif
}

int
imc_activate(struct device *self, int act)
{
	int rv = 0;

	switch (act) {
	case DVACT_POWERDOWN:
#ifndef SMALL_KERNEL
		wdog_shutdown(self);
#endif
		rv = config_activate_children(self, act);
		break;
	}

	return (rv);
}

int
imc_print(void *aux, const char *name)
{
	struct imc_attach_args *iaa = aux;

	if (name != NULL)
		printf("%s at %s", iaa->iaa_name, name);

	return UNCONF;
}

void
imc_bus_reset()
{
	imc_write(IMC_CPU_ERRSTAT, 0);
	imc_write(IMC_GIO_ERRSTAT, 0);
}

uint32_t
imc_bus_error(uint32_t hwpend, struct trap_frame *tf)
{
	uint32_t cpustat, giostat;
	paddr_t cpuaddr, gioaddr;
	int cpuquiet = 0, gioquiet = 0;

	cpustat = imc_read(IMC_CPU_ERRSTAT);
	cpuaddr = imc_read(IMC_CPU_ERRADDR);
	giostat = imc_read(IMC_GIO_ERRSTAT);
	gioaddr = imc_read(IMC_GIO_ERRADDR);

	switch (sys_config.system_type) {
	case SGI_IP28:
		/*
		 * R10000 speculative execution may attempt to access
		 * non-existing memory when in the kernel. We do not
		 * want to flood the console about those.
		 */
		if (cpustat & IMC_CPU_ERRSTAT_ADDR) {
			if (IS_XKPHYS((vaddr_t)tf->pc))
				cpuquiet = 1;
		}
		if (giostat != 0) {
			/*
			 * Ignore speculative writes to interrupt controller
			 * registers.
			 */
			if ((giostat & IMC_ECC_ERRSTAT_FUW) &&
			    (gioaddr & ~0x3f) == INT2_IP22)
				gioquiet = 1;
			/* XXX is it wise to hide these? */
			if ((giostat & IMC_GIO_ERRSTAT_TMO) &&
			    !IS_GIO_ADDRESS(gioaddr))
				gioquiet = 1;
		}
		break;
	}

	if (cpustat != 0 && cpuquiet == 0) {
		vaddr_t pc = tf->pc;
		uint32_t insn = 0xffffffff;

		if (tf->pc < 0)
			guarded_read_4(pc, &insn);
		else
			copyin((void *)pc, &insn, sizeof insn);

		printf("bus error: cpu_stat %08x addr %08lx pc %p insn %08x\n",
		    cpustat, cpuaddr, (void *)pc, insn);
	}
	if (giostat != 0 && gioquiet == 0) {
		printf("bus error: gio_stat %08x addr %08lx\n",
		    giostat, gioaddr);
	}

	if (cpustat != 0)
		imc_write(IMC_CPU_ERRSTAT, 0);
	if (giostat != 0)
		imc_write(IMC_GIO_ERRSTAT, 0);

	return hwpend;
}

int
imc_watchdog_cb(void *v, int period)
{
	uint32_t reg;

	if (period == 0) {
		/* reset... */
		imc_write(IMC_WDOG, 0);
		/* ...and disable */
		reg = imc_read(IMC_CPUCTRL0);
		reg &= ~(IMC_CPUCTRL0_WDOG);
		imc_write(IMC_CPUCTRL0, reg);

		return 0;
	} else {
		/* enable... */
		reg = imc_read(IMC_CPUCTRL0);
		reg |= IMC_CPUCTRL0_WDOG;
		imc_write(IMC_CPUCTRL0, reg);
		/* ...and reset */
		imc_write(IMC_WDOG, 0);

		/*
		 * The watchdog period is not controllable; it will fire
		 * when the 20 bit counter, running on a 64 usec clock,
		 * overflows.
		 */
		return (64 << 20) / 1000000;
	}
}

/* intended to be called from gio/gio.c only */
int
imc_gio64_arb_config(int slot, uint32_t flags)
{
	uint32_t reg;

	if (sys_config.system_type == SGI_IP20 ||
	    sys_config.system_subtype != IP22_INDIGO2) {
		/* GIO_SLOT_GFX is only usable on Fullhouse */
		if (slot == GIO_SLOT_GFX)
			return EINVAL;
	} else {
		/* GIO_SLOT_EXP1 is unusable on Fullhouse */
		if (slot == GIO_SLOT_EXP1)
			return EINVAL;
	}

	/* GIO_SLOT_GFX is always pipelined */
	if (slot == GIO_SLOT_GFX && (flags & GIO_ARB_NOPIPE))
		return EINVAL;

	/* IP20 does not support pipelining (XXX what about Indy?) */
	if (((flags & GIO_ARB_PIPE) || (flags & GIO_ARB_NOPIPE)) &&
	    sys_config.system_type == SGI_IP20)
		return EINVAL;

	reg = imc_arb_value;

	if (flags & GIO_ARB_RT) {
		if (slot == GIO_SLOT_EXP0)
			reg |= IMC_GIO64ARB_EXP0RT;
		else if (slot == GIO_SLOT_EXP1)
			reg |= IMC_GIO64ARB_EXP1RT;
		else if (slot == GIO_SLOT_GFX)
			reg |= IMC_GIO64ARB_GRXRT;
	}

	if (flags & GIO_ARB_MST) {
		if (slot == GIO_SLOT_EXP0)
			reg |= IMC_GIO64ARB_EXP0MST;
		else if (slot == GIO_SLOT_EXP1)
			reg |= IMC_GIO64ARB_EXP1MST;
		else if (slot == GIO_SLOT_GFX)
			reg |= IMC_GIO64ARB_GRXMST;
	}

	if (flags & GIO_ARB_PIPE) {
		if (slot == GIO_SLOT_EXP0)
			reg |= IMC_GIO64ARB_EXP0PIPE;
		else if (slot == GIO_SLOT_EXP1)
			reg |= IMC_GIO64ARB_EXP1PIPE;
	}

	if (flags & GIO_ARB_LB) {
		if (slot == GIO_SLOT_EXP0)
			reg &= ~IMC_GIO64ARB_EXP0RT;
		else if (slot == GIO_SLOT_EXP1)
			reg &= ~IMC_GIO64ARB_EXP1RT;
		else if (slot == GIO_SLOT_GFX)
			reg &= ~IMC_GIO64ARB_GRXRT;
	}

	if (flags & GIO_ARB_SLV) {
		if (slot == GIO_SLOT_EXP0)
			reg &= ~IMC_GIO64ARB_EXP0MST;
		else if (slot == GIO_SLOT_EXP1)
			reg &= ~IMC_GIO64ARB_EXP1MST;
		else if (slot == GIO_SLOT_GFX)
			reg &= ~IMC_GIO64ARB_GRXMST;
	}

	if (flags & GIO_ARB_NOPIPE) {
		if (slot == GIO_SLOT_EXP0)
			reg &= ~IMC_GIO64ARB_EXP0PIPE;
		else if (slot == GIO_SLOT_EXP1)
			reg &= ~IMC_GIO64ARB_EXP1PIPE;
	}

	if (flags & GIO_ARB_32BIT) {
		if (slot == GIO_SLOT_EXP0)
			reg &= ~IMC_GIO64ARB_EXP064;
		else if (slot == GIO_SLOT_EXP1)
			reg &= ~IMC_GIO64ARB_EXP164;
	}

	if (flags & GIO_ARB_64BIT) {
		if (slot == GIO_SLOT_EXP0)
			reg |= IMC_GIO64ARB_EXP064;
		else if (slot == GIO_SLOT_EXP1)
			reg |= IMC_GIO64ARB_EXP164;
	}

	if (flags & GIO_ARB_HPC2_32BIT)
		reg &= ~IMC_GIO64ARB_HPCEXP64;

	if (flags & GIO_ARB_HPC2_64BIT)
		reg |= IMC_GIO64ARB_HPCEXP64;

	imc_write(IMC_GIO64ARB, reg);
	imc_arb_value = reg;

	return 0;
}

/*
 * According to chapter 19 of the "IRIX Device Driver Programmer's Guide",
 * some GIO devices, which do not drive all data lines, may cause false
 * memory read parity errors on the SysAD bus. The workaround is to disable
 * parity checking.
 */
void
imc_disable_sysad_parity(void)
{
	uint32_t reg;

	if (ip22_ecc)
		return;

	reg = imc_read(IMC_CPUCTRL0);
	reg |= IMC_CPUCTRL0_NCHKMEMPAR;
	imc_write(IMC_CPUCTRL0, reg);
}

void
imc_enable_sysad_parity(void)
{
	uint32_t reg;

	if (ip22_ecc)
		return;

	reg = imc_read(IMC_CPUCTRL0);
	reg &= ~IMC_CPUCTRL0_NCHKMEMPAR;
	imc_write(IMC_CPUCTRL0, reg);
}

#if 0
int
imc_is_sysad_parity_enabled(void)
{
	uint32_t reg;

	if (ip22_ecc)
		return 0;

	reg = imc_read(IMC_CPUCTRL0);
	return ~reg & IMC_CPUCTRL0_NCHKMEMPAR;
}
#endif

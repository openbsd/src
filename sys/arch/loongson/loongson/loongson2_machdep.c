/*	$OpenBSD: loongson2_machdep.c,v 1.2 2010/01/22 21:45:24 miod Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/loongson2.h>
#include <machine/memconf.h>
#include <machine/pmon.h>

extern struct phys_mem_desc mem_layout[MAXMEMSEGS];

boolean_t is_memory_range(paddr_t, psize_t, psize_t);
void	loongson2e_setup(u_long, u_long);
void	loongson2f_setup(u_long, u_long);

/* CPU view of PCI resources */
paddr_t	loongson_pci_base = 0;
/* PCI view of CPU memory */
paddr_t loongson_dma_base = 0;

/*
 * Setup memory mappings for Loongson 2E processors.
 */

/*
 * Canonical crossbow assignments on Loongson 2F based designs.
 * Might need to move to a per-design header file in the future.
 */

#define	MASTER_CPU	0
#define	MASTER_PCI	1

#define	WINDOW_CPU_DDR	2
#define	WINDOW_CPU_PCI	3

#define	WINDOW_PCI_DDR	0

#define	DDR_PHYSICAL_BASE	0x0000000000000000UL	/* memory starts at 0 */
#define	DDR_PHYSICAL_SIZE	0x0000000080000000UL	/* up to 2GB */
#define	DDR_WINDOW_BASE		0x0000000080000000UL	/* mapped at 2GB */

#define	PCI_RESOURCE_BASE	0x0000000000000000UL
#define	PCI_RESOURCE_SIZE	0x0000000080000000UL
#define	PCI_WINDOW_BASE		0x0000000100000000UL

#define	PCI_DDR_BASE		0x0000000080000000UL	/* PCI->DDR at 2GB */
#define	PCI_DDR_SIZE		DDR_PHYSICAL_SIZE
#define	PCI_DDR_WINDOW_BASE	DDR_PHYSICAL_BASE

void
loongson2e_setup(u_long memlo, u_long memhi)
{
	memlo = atop(memlo << 20);
	memhi = atop(memhi << 20);
	physmem = memlo + memhi;

	/*
	 * Only register the first 256MB of memory.
	 * This will be hopefully be revisited once we get our hands
	 * on Loongson 2E-based hardware...
	 */

	mem_layout[0].mem_first_page = 1; /* do NOT stomp on exception area */
	mem_layout[0].mem_last_page = memlo;

	loongson_dma_base = PCI_DDR_BASE;
}

/*
 * Setup memory mappings for Loongson 2F processors.
 */

void
loongson2f_setup(u_long memlo, u_long memhi)
{
	volatile uint64_t *awrreg;

	/*
	 * Because we'll only set up a 2GB window for the PCI bus to
	 * access local memory, we'll limit ourselves to 2GB of usable
	 * memory as well.
	 *
	 * Note that this is a bad justification for this; it should be
	 * possible to setup a 1GB PCI space / 3GB memory access window,
	 * and use bounce buffers if physmem > 3GB; but at the moment
	 * there is no need to solve this problem until Loongson 2F-based
	 * hardware with more than 2GB of memory is commonly encountered.
	 */

	physmem = memlo + memhi;	/* in MB so far */
	if (physmem > 2048) {
		pmon_printf("WARNING! %d MB of memory will not be used",
		    physmem - 2048);
		memhi = 2048 - 256;
	}

	memlo = atop(memlo << 20);
	memhi = atop(memhi << 20);
	physmem = atop(physmem << 20);

	/*
	 * PMON configures the system with only the low 256MB of memory
	 * accessible.
	 *
	 * We need to reprogram the address windows in order to be able to
	 * access the whole memory, both by the local processor and by the
	 * PCI bus.
	 *
	 * To make our life easier, we'll setup the memory as a contiguous
	 * range starting at 2GB, and take into account the fact that the
	 * first 256MB are also aliased at address zero (which is where the
	 * kernel is loaded, really).
	 */

	/* do NOT stomp on exception area */
	mem_layout[0].mem_first_page = atop(DDR_WINDOW_BASE) + 1;
	mem_layout[0].mem_last_page = atop(DDR_WINDOW_BASE) + memlo + memhi;

	/*
	 * Allow access to memory beyond 256MB, by programming the
	 * Loongson 2F address window registers.
	 */

	/*
	 * Make window #2 span the whole memory at 2GB onwards.
	 * XXX Note that this assumes total memory size is
	 * XXX a power of two.  This is always true on the Lemote
	 * XXX Yeelong, might not be on other products.
	 */
	awrreg = (volatile uint64_t *)PHYS_TO_XKPHYS(
	    LOONGSON_AWR_BASE(MASTER_CPU, WINDOW_CPU_DDR), CCA_NC);
	*awrreg = DDR_WINDOW_BASE;
	(void)*awrreg;

	awrreg = (volatile uint64_t *)PHYS_TO_XKPHYS(
	    LOONGSON_AWR_SIZE(MASTER_CPU, WINDOW_CPU_DDR), CCA_NC);
	*awrreg = 0xffffffffffffffffUL << (ffs(DDR_PHYSICAL_SIZE) - 1);
	(void)*awrreg;

	awrreg = (volatile uint64_t *)PHYS_TO_XKPHYS(
	    LOONGSON_AWR_MMAP(MASTER_CPU, WINDOW_CPU_DDR), CCA_NC);
	*awrreg = DDR_PHYSICAL_BASE | MASTER_CPU;
	(void)*awrreg;
}

boolean_t
is_memory_range(paddr_t pa, psize_t len, psize_t limit)
{
	struct phys_mem_desc *seg;
	uint64_t fp, lp;
	int i;

	fp = atop(pa);
	lp = atop(round_page(pa + len));

	if (limit != 0 && lp > atop(limit))
		return FALSE;

	/*
	 * Allow access to the low 256MB aliased region on 2F systems.
	 */
	if (/* curcpu()->ci_hw.type == MIPS_LOONGSON2 && */
	    (curcpu()->ci_hw.c0prid & 0xff) == 0x2f - 0x2c) {
		if (pa < 0x10000000) {
			fp += atop(DDR_WINDOW_BASE);
			lp += atop(DDR_WINDOW_BASE);
		}
	}

	for (i = 0, seg = mem_layout; i < MAXMEMSEGS; i++, seg++)
		if (fp >= seg->mem_first_page && lp <= seg->mem_last_page)
			return TRUE;

	return FALSE;
}

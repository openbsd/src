/*	$OpenBSD: mainbus.c,v 1.26 2011/04/07 15:30:15 miod Exp $ */
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 2004, Miodrag Vallat.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>

#ifdef M88100
#include <machine/m8820x.h>
#endif
#ifdef MVME187
#include <machine/mvme187.h>
#endif
#ifdef MVME188
#include <machine/mvme188.h>
#endif
#ifdef MVME197
#include <machine/mvme197.h>
#endif

void	mainbus_attach(struct device *, struct device *, void *);
int 	mainbus_match(struct device *, void *, void *);
int	mainbus_print(void *, const char *);
int	mainbus_scan(struct device *, void *, void *);

/*
 * bus_space routines for 1:1 obio mappings
 */

int	mainbus_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	mainbus_unmap(bus_space_handle_t, bus_size_t);
int	mainbus_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void	*mainbus_vaddr(bus_space_handle_t);

const struct mvme88k_bus_space_tag mainbus_bustag = {
	mainbus_map,
	mainbus_unmap,
	mainbus_subregion,
	mainbus_vaddr
};

bus_addr_t	 bs_obio_start;
bus_addr_t	 bs_obio_end;
struct extent	*bs_extent;

/*
 * Obio (internal IO) space is mapped 1:1 (see pmap_bootstrap() for details).
 *
 * However, sram attaches as a child of mainbus, but does not reside in
 * internal IO space. As a result, we have to allow both 1:1 and regular
 * translations, depending upon the address to map.
 */

int
mainbus_map(bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	vaddr_t map;

	map = mapiodev((paddr_t)addr, size);
	if (map == 0)
		return ENOMEM;

	*ret = (bus_space_handle_t)map;
	return 0;
}

void
mainbus_unmap(bus_space_handle_t handle, bus_size_t size)
{
	/* XXX what to do for non-obio mappings? */
}

int
mainbus_subregion(bus_space_handle_t handle, bus_addr_t offset,
    bus_size_t size, bus_space_handle_t *ret)
{
	*ret = handle + offset;
	return (0);
}

void *
mainbus_vaddr(bus_space_handle_t handle)
{
	return (void *)handle;
}

/*
 * Map a range [pa, pa+size) in the given map to a kernel address
 * in iomap space.
 *
 * Note: To be flexible, I did not put a restriction on the alignment
 * of pa. However, it is advisable to have pa page aligned since otherwise,
 * we might have several mappings for a given chunk of the IO page.
 */
vaddr_t
mapiodev(paddr_t addr, int _size)
{
	vaddr_t	va, iova, off;
	paddr_t pa, epa;
	psize_t size;
	int s, error;

	/* sanity checks */
	if (_size <= 0)
		return 0;
	size = (psize_t)_size;
	epa = addr + size;
	if (epa < addr && epa != 0)
		return 0;

	/* check for 1:1 mapping */
	if (addr >= bs_obio_start) {
		if (bs_obio_end == 0 || epa <= bs_obio_end)
			return ((vaddr_t)addr);
		else if (addr <= bs_obio_end)
			/* across obio and non-obio, not supported */
			return 0;
	}

	pa = trunc_page(addr);
	off = addr & PGOFSET;
	size = round_page(off + size);

	s = splhigh();
	error = extent_alloc_region(bs_extent, atop(pa), atop(size),
	    EX_MALLOCOK | (cold ? 0 : EX_WAITSPACE));
	splx(s);

	if (error != 0)
		return 0;

	va = uvm_km_valloc(kernel_map, size);
	if (va == 0) {
		extent_free(bs_extent, atop(pa), atop(size),
		    EX_MALLOCOK | (cold ? 0 : EX_WAITSPACE));
		return 0;
	}

	iova = va + off;
	while (size != 0) {
		pmap_enter(pmap_kernel(), va, pa, UVM_PROT_RW,
		    UVM_PROT_RW | PMAP_WIRED);
		size -= PAGE_SIZE;
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	return (iova);
}

/*
 * Free up the mapping in iomap.
 */
void
unmapiodev(vaddr_t va, int _size)
{
	vaddr_t eva, kva, off;
	vsize_t size;
	paddr_t pa;
	int s, error;

	/* sanity checks */
	if (_size <= 0)
		return;
	size = (vsize_t)_size;
	eva = va + size;
	if (eva < va && eva != 0)
		return;

	/* check for 1:1 mapping */
	if (va >= bs_obio_start) {
		if (bs_obio_end == 0 || eva <= bs_obio_end)
			return;
		else if (va <= bs_obio_end)
			/* across obio and non-obio, not supported */
			return;
	}

	off = va & PGOFSET;
	kva = trunc_page(va);
	size = round_page(off + size);

	if (pmap_extract(pmap_kernel(), kva, &pa) == FALSE)
		panic("unmapiodev(%p,%p)", kva, size);

	pmap_remove(pmap_kernel(), kva, kva + size);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, kva, size);

	s = splhigh();
	error = extent_free(bs_extent, atop(pa), atop(size),
	    EX_MALLOCOK | (cold ? 0 : EX_WAITSPACE));
#ifdef DIAGNOSTIC
	if (error != 0)
		printf("unmapiodev(%p pa %p, %p): extent_free failed\n",
		    kva, pa, size);
#endif
	splx(s);
}

/*
 * Configuration glue
 */

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbus_match(struct device *parent, void *cf, void *args)
{
	return (1);
}

int
mainbus_print(void *args, const char *bus)
{
	struct confargs *ca = args;

	if (ca->ca_paddr != -1)
		printf(" addr 0x%x", ca->ca_paddr);
	return (UNCONF);
}

int
mainbus_scan(struct device *parent, void *child, void *args)
{
	struct cfdata *cf = child;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = &mainbus_bustag;
	oca.ca_dmat = 0;	/* XXX no need for a meaningful value yet */
	oca.ca_bustype = BUS_MAIN;
	oca.ca_paddr = cf->cf_loc[0];
	oca.ca_ipl = -1;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, mainbus_print);
	return (1);
}

void
mainbus_attach(struct device *parent, struct device *self, void *args)
{
	extern void cpu_hatch_secondary_processors(void *);
	extern char cpu_model[];

	printf(": %s\n", cpu_model);

	/*
	 * Display cpu/mmu details for the main processor.
	 */
	cpu_configuration_print(1);

	/*
	 * Initialize an extent to keep track of I/O mappings.
	 */
#ifdef M88100
	if (CPU_IS88100) {
		bs_obio_start = BATC8_VA;	/* hardwired BATC */
		bs_obio_end = 0;
	}
#endif
#ifdef MVME197
	if (CPU_IS88110) {
		bs_obio_start = OBIO197_START;
		bs_obio_end = OBIO197_START + OBIO197_SIZE;
	}
#endif
	bs_extent = extent_create("bus_space", atop(physmem),
	    1 + atop(0U - PAGE_SIZE), M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (bs_extent == NULL)
		panic("unable to allocate bus_space extent");

#ifdef MULTIPROCESSOR
	/*
	 * Spin up the other processors, but do not give them work to
	 * do yet.
	 * On MVME188 boards, the system hangs if secondary processors
	 * try to issue BUG calls (i.e. when printing their information
	 * on console), so we postpone this to the end of autoconf.
	 */
	if (brdtyp != BRD_188)
		cpu_hatch_secondary_processors(NULL);
#endif

	(void)config_search(mainbus_scan, self, args);
}

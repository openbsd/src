/*	$OpenBSD: mainbus.c,v 1.17 2005/11/25 22:14:31 miod Exp $ */
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

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>

#ifdef MVME188
#include <machine/mvme188.h>
#endif
#if defined(MVME187) || defined(MVME197)
#include <machine/mvme1x7.h>
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

extern struct extent *iomap_extent;
extern struct vm_map *iomap_map;

/*
 * Obio (internal IO) space is mapped 1:1 (see pmap_bootstrap() for details).
 *
 * However, sram attaches as a child of mainbus, but does not reside in
 * internal IO space. As a result, we have to allow both 1:1 and iomap
 * translations, depending upon the address to map.
 */

int
mainbus_map(bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	vaddr_t map;
	static bus_addr_t threshold = 0;

	if (threshold == 0) {
		switch (brdtyp) {
#ifdef MVME188
		case BRD_188:
			threshold = MVME188_UTILITY;
			break;
#endif
#ifdef MVME187
		case BRD_187:
		case BRD_8120:
#endif
#ifdef MVME197
		case BRD_197:
#endif
			threshold = OBIO_START;
			break;
		}
	}

	if (addr >= threshold)
		map = (vaddr_t)addr;
	else {
		map = mapiodev((paddr_t)addr, size);
	}

	if (map == NULL)
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
 * Map a range [pa, pa+size] in the given map to a kernel address
 * in iomap space.
 *
 * Note: To be flexible, I did not put a restriction on the alignment
 * of pa. However, it is advisable to have pa page aligned since otherwise,
 * we might have several mappings for a given chunk of the IO page.
 */
vaddr_t
mapiodev(pa, size)
	paddr_t pa;
	int size;
{
	vaddr_t	iova, tva, off;
	paddr_t ppa;
	int s, error;

	if (size <= 0)
		return NULL;

	ppa = trunc_page(pa);
	off = pa & PGOFSET;
	size = round_page(off + size);

	s = splhigh();
	error = extent_alloc(iomap_extent, size, PAGE_SIZE, 0, EX_NOBOUNDARY,
	    EX_WAITSPACE, &iova);
	splx(s);

	if (error != 0)
		return NULL;

	tva = iova;
	while (size != 0) {
		pmap_enter(vm_map_pmap(iomap_map), tva, ppa,
			   VM_PROT_WRITE | VM_PROT_READ,
			   VM_PROT_WRITE | VM_PROT_READ | PMAP_WIRED);
		size -= PAGE_SIZE;
		tva += PAGE_SIZE;
		ppa += PAGE_SIZE;
	}
	pmap_update(vm_map_pmap(iomap_map));

	return (iova + off);
}

/*
 * Free up the mapping in iomap.
 */
void
unmapiodev(va, size)
	vaddr_t va;
	int size;
{
	vaddr_t kva, off;
	int s, error;

	off = va & PGOFSET;
	kva = trunc_page(va);
	size = round_page(off + size);

	pmap_remove(vm_map_pmap(iomap_map), kva, kva + size);
	pmap_update(vm_map_pmap(iomap_map));

	s = splhigh();
	error = extent_free(iomap_extent, kva, size, EX_NOWAIT);
#ifdef DIAGNOSTIC
	if (error != 0)
		printf("unmapiodev: extent_free failed\n");
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
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

int
mainbus_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_paddr != -1)
		printf(" addr 0x%x", ca->ca_paddr);
	return (UNCONF);
}

int
mainbus_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
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
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	extern char cpu_model[];

	printf(": %s\n", cpu_model);

	/*
	 * Display cpu/mmu details. Only for the master CPU so far.
	 */
	cpu_configuration_print(1);

	/* XXX
	 * should have a please-attach-first list for mainbus,
	 * to ensure that the pcc/vme2/mcc chips are attached
	 * first.
	 */

	(void)config_search(mainbus_scan, self, args);
}

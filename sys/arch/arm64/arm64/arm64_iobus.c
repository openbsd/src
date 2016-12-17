/* $OpenBSD: arm64_iobus.c,v 1.1 2016/12/17 23:38:33 patrick Exp $ */
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
 * This is a iobus driver.
 * It handles configuration of all devices on the processor bus except UART.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <uvm/uvm_extern.h>
#include <machine/pmap.h>

#if 0
u_int8_t iobus_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t iobus_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t iobus_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t iobus_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 iobus_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 iobus_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 iobus_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 iobus_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 iobus_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 iobus_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 iobus_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 iobus_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

struct cfattach iobus_ca = {
	sizeof(struct device), iobusmatch, iobusattach
};

struct cfdriver iobus_cd = {
	NULL, "iobus", DV_DULL
};
#endif

int iobus_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp);
void iobus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t size);

bus_space_t arm64_bs_tag = {
	.bus_base = 0ULL, // XXX
	.bus_private = NULL,
	._space_read_1 =	generic_space_read_1,
	._space_write_1 =	generic_space_write_1,
	._space_read_2 =	generic_space_read_2,
	._space_write_2 =	generic_space_write_2,
	._space_read_4 =	generic_space_read_4,
	._space_write_4 =	generic_space_write_4,
	._space_read_8 =	generic_space_read_8,
	._space_write_8 =	generic_space_write_8,
	._space_read_raw_2 =	generic_space_read_raw_2,
	._space_write_raw_2 =	generic_space_write_raw_2,
	._space_read_raw_4 =	generic_space_read_raw_4,
	._space_write_raw_4 =	generic_space_write_raw_4,
	._space_read_raw_8 =	generic_space_read_raw_8,
	._space_write_raw_8 =	generic_space_write_raw_8,
	._space_map =		iobus_space_map,
	._space_unmap =		iobus_space_unmap,
	._space_subregion =	generic_space_region,
	._space_vaddr =		generic_space_vaddr
};

bus_space_handle_t iobus_h;

#if 0
struct machine_bus_dma_tag iobus_bus_dma_tag = {
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
	iobus_pa_to_device,
	iobus_device_to_pa,
	0
};
#endif

int
iobus_space_map(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	u_long startpa, endpa, pa;
	vaddr_t va;
	int cache = flags & BUS_SPACE_MAP_CACHEABLE ?
	    PMAP_CACHE_WB : PMAP_CACHE_CI;

	startpa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	va = (vaddr_t)km_alloc(endpa - startpa, &kv_any, &kp_none, &kd_nowait);
	if (! va)
		return(ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter_cache(va, pa, PROT_READ | PROT_WRITE, cache);
	}
	pmap_update(pmap_kernel());

	return(0);
}

void
iobus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t	va, endva;

	va = trunc_page((vaddr_t)bsh);
	endva = round_page((vaddr_t)bsh + size);

	pmap_kremove(va, endva - va);
	pmap_update(pmap_kernel());
	km_free((void *)va, endva - va, &kv_any, &kp_none);
}

bus_space_t arm64_a4x_bs_tag = {
	.bus_base = 0ULL, // XXX
	.bus_private = NULL,
	._space_read_1 =	a4x_space_read_1,
	._space_write_1 =	a4x_space_write_1,
	._space_read_2 =	a4x_space_read_2,
	._space_write_2 =	a4x_space_write_2,
	._space_read_4 =	a4x_space_read_4,
	._space_write_4 =	a4x_space_write_4,
	._space_read_8 =	a4x_space_read_8,
	._space_write_8 =	a4x_space_write_8,
	._space_read_raw_2 =	a4x_space_read_raw_2,
	._space_write_raw_2 =	a4x_space_write_raw_2,
	._space_read_raw_4 =	a4x_space_read_raw_4,
	._space_write_raw_4 =	a4x_space_write_raw_4,
	._space_read_raw_8 =	a4x_space_read_raw_8,
	._space_write_raw_8 =	a4x_space_write_raw_8,
	._space_map =		iobus_space_map,
	._space_unmap =		iobus_space_unmap,
	._space_subregion =	generic_space_region,
	._space_vaddr =		generic_space_vaddr
};


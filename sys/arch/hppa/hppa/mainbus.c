/*	$OpenBSD: mainbus.c,v 1.34 2002/10/06 22:06:15 art Exp $	*/

/*
 * Copyright (c) 1998-2001 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#undef BTLBDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/extent.h>
#include <sys/mbuf.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

struct mainbus_softc {
	struct  device sc_dv;

	hppa_hpa_t sc_hpa;
};

int	mbmatch(struct device *, void *, void *);
void	mbattach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

/* from machdep.c */
extern struct extent *hppa_ex;
extern struct pdc_btlb pdc_btlb;

int
mbus_add_mapping(bus_addr_t bpa, bus_size_t size, int cachable,
    bus_space_handle_t *bshp)
{
	static u_int32_t bmm[0x4000/32];
	register u_int64_t spa, epa;
	int bank, off, flex = HPPA_FLEX(bpa);

#ifdef BTLBDEBUG
	printf("bus_mem_add_mapping(%x,%x,%scachable,%p)\n",
	    bpa, size, cachable? "" : "non", bshp);
#endif

	if ((bank = vm_physseg_find(atop(bpa), &off)) >= 0)
		panic("mbus_add_mapping: mapping real memory @0x%x", bpa);

	/*
	 * determine if we are mapping IO space, or beyond the physmem
	 * region. use block mapping then
	 *
	 * we map the whole bus module (there are 1024 of those max)
	 * so, check here if it's mapped already, map if needed.
	 * all mappings are equal mappings.
	 */
#ifdef DEBUG
	if (cachable) {
		printf("WARNING: mapping I/O space cachable\n");
		cachable = 0;
	}
#endif

	/* need a new mapping */
	if (!(bmm[flex / 32] & (1 << (flex % 32)))) {
		spa = bpa & HPPA_FLEX_MASK;
		epa = ((u_long)((u_int64_t)bpa + size +
			~HPPA_FLEX_MASK - 1) & HPPA_FLEX_MASK) - 1;
#ifdef BTLBDEBUG
		printf("bus_mem_add_mapping: adding flex=%x "
			"%qx-%qx, ", flex, spa, epa);
#endif
		while (spa < epa) {
			vsize_t len = epa - spa;
			u_int64_t pa;
			if (len > pdc_btlb.max_size << PGSHIFT)
				len = pdc_btlb.max_size << PGSHIFT;
			if (btlb_insert(HPPA_SID_KERNEL, spa, spa, &len,
			    pmap_sid2pid(HPPA_SID_KERNEL) |
			    pmap_prot(pmap_kernel(), UVM_PROT_RW)) >= 0) {
				pa = spa + len - 1;
#ifdef BTLBDEBUG
				printf("--- %x/%x, %qx, %qx-%qx",
				    flex, HPPA_FLEX(pa), pa, spa, epa);
#endif
				/* do the mask */
				for (; flex <= HPPA_FLEX(pa); flex++) {
#ifdef BTLBDEBUG
					printf("mask %x ", flex);
#endif
					bmm[flex / 32] |= (1 << (flex % 32));
				}
				spa = pa;
			} else {
				spa = max(spa, hppa_trunc_page(bpa));
				epa = min(epa, hppa_round_page(bpa));

				if (epa - 1 > ~0U)
					epa = (u_int64_t)~0U + 1;

				for (; spa < epa; spa += PAGE_SIZE)
					pmap_kenter_pa(spa, spa, UVM_PROT_RW);

			}
#ifdef BTLBDEBUG
			printf("\n");
#endif
		}
	}
#ifdef BTLBDEBUG
	else {
		printf("+++ already b-mapped flex=%x, mask=%x",
		    flex, bmm[flex / 8]);
	}
#endif

	*bshp = bpa;
	return (0);
}

int
mbus_map(void *v, bus_addr_t bpa, bus_size_t size,
    int cachable, bus_space_handle_t *bshp)
{
	register int error;

	if ((error = extent_alloc_region(hppa_ex, bpa, size, EX_NOWAIT)))
		return (error);

	if ((error = mbus_add_mapping(bpa, size, cachable, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT)) {
			printf("bus_space_map: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf("bus_space_map: can't free region\n");
		}
	}

	return error;
}

void
mbus_unmap(void *v, bus_space_handle_t bsh, bus_size_t size)
{
	u_long sva, eva;

	sva = hppa_trunc_page(bsh);
	eva = hppa_round_page(bsh + size);

#ifdef DIAGNOSTIC
	if (eva <= sva)
		panic("bus_space_unmap: overflow");
#endif

	if (pmap_extract(pmap_kernel(), bsh, NULL))
		pmap_kremove(sva, eva - sva);
	else
		;	/* XXX assuming equ b-mapping been done */

	if (extent_free(hppa_ex, bsh, size, EX_NOWAIT)) {
		printf("bus_space_unmap: ps 0x%lx, size 0x%lx\n",
		    bsh, size);
		printf("bus_space_unmap: can't free region\n");
	}
}

int
mbus_alloc(void *v, bus_addr_t rstart, bus_addr_t rend, bus_size_t size,
	 bus_size_t align, bus_size_t boundary, int cachable,
	 bus_addr_t *addrp, bus_space_handle_t *bshp)
{
	u_long bpa;
	int error;

	if (rstart < hppa_ex->ex_start || rend > hppa_ex->ex_end)
		panic("bus_space_alloc: bad region start/end");

	if ((error = extent_alloc_subregion(hppa_ex, rstart, rend, size,
	    align, 0, boundary, EX_NOWAIT, &bpa)))
		return (error);

	if ((error = mbus_add_mapping(bpa, size, cachable, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT)) {
			printf("bus_space_alloc: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf("bus_space_alloc: can't free region\n");
		}
	}

	*addrp = bpa;
	return (error);
}

void
mbus_free(void *v, bus_space_handle_t h, bus_size_t size)
{
	/* bus_space_unmap() does all that we need to do. */
	mbus_unmap(v, h, size);
}

int
mbus_subregion(void *v, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void
mbus_barrier(void *v, bus_space_handle_t h, bus_size_t o, bus_size_t l, int op)
{
	sync_caches();
}

u_int8_t
mbus_r1(void *v, bus_space_handle_t h, bus_size_t o)
{
	return *((volatile u_int8_t *)(h + o));
}

u_int16_t
mbus_r2(void *v, bus_space_handle_t h, bus_size_t o)
{
	return *((volatile u_int16_t *)(h + o));
}

u_int32_t
mbus_r4(void *v, bus_space_handle_t h, bus_size_t o)
{
	return *((volatile u_int32_t *)(h + o));
}

u_int64_t
mbus_r8(void *v, bus_space_handle_t h, bus_size_t o)
{
	return *((volatile u_int64_t *)(h + o));
}

void
mbus_w1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv)
{
	*((volatile u_int8_t *)(h + o)) = vv;
}

void
mbus_w2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv)
{
	*((volatile u_int16_t *)(h + o)) = vv;
}

void
mbus_w4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv)
{
	*((volatile u_int32_t *)(h + o)) = vv;
}

void
mbus_w8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv)
{
	*((volatile u_int64_t *)(h + o)) = vv;
}


void
mbus_rm_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *(volatile u_int8_t *)h;
}

void
mbus_rm_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *(volatile u_int16_t *)h;
}

void
mbus_rm_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *(volatile u_int32_t *)h;
}

void
mbus_rm_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *(volatile u_int64_t *)h;
}

void
mbus_wm_1(void *v, bus_space_handle_t h, bus_size_t o, const u_int8_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int8_t *)h = *(a++);
}

void
mbus_wm_2(void *v, bus_space_handle_t h, bus_size_t o, const u_int16_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int16_t *)h = *(a++);
}

void
mbus_wm_4(void *v, bus_space_handle_t h, bus_size_t o, const u_int32_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int32_t *)h = *(a++);
}

void
mbus_wm_8(void *v, bus_space_handle_t h, bus_size_t o, const u_int64_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int64_t *)h = *(a++);
}

void
mbus_sm_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int8_t *)h = vv;
}

void
mbus_sm_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int16_t *)h = vv;
}

void
mbus_sm_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int32_t *)h = vv;
}

void
mbus_sm_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*(volatile u_int64_t *)h = vv;
}

void mbus_rrm_2(void *v, bus_space_handle_t h,
		     bus_size_t o, u_int16_t*a, bus_size_t c);
void mbus_rrm_4(void *v, bus_space_handle_t h,
		     bus_size_t o, u_int32_t*a, bus_size_t c);
void mbus_rrm_8(void *v, bus_space_handle_t h,
		     bus_size_t o, u_int64_t*a, bus_size_t c);

void mbus_wrm_2(void *v, bus_space_handle_t h,
		     bus_size_t o, const u_int16_t *a, bus_size_t c);
void mbus_wrm_4(void *v, bus_space_handle_t h,
		     bus_size_t o, const u_int32_t *a, bus_size_t c);
void mbus_wrm_8(void *v, bus_space_handle_t h,
		     bus_size_t o, const u_int64_t *a, bus_size_t c);

void
mbus_rr_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int8_t *)h)++;
}

void
mbus_rr_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int16_t *)h)++;
}

void
mbus_rr_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int32_t *)h)++;
}

void
mbus_rr_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*(a++) = *((volatile u_int64_t *)h)++;
}

void
mbus_wr_1(void *v, bus_space_handle_t h, bus_size_t o, const u_int8_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int8_t *)h)++ = *(a++);
}

void
mbus_wr_2(void *v, bus_space_handle_t h, bus_size_t o, const u_int16_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int16_t *)h)++ = *(a++);
}

void
mbus_wr_4(void *v, bus_space_handle_t h, bus_size_t o, const u_int32_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int32_t *)h)++ = *(a++);
}

void
mbus_wr_8(void *v, bus_space_handle_t h, bus_size_t o, const u_int64_t *a, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int64_t *)h)++ = *(a++);
}

void mbus_rrr_2(void *v, bus_space_handle_t h,
		   bus_size_t o, u_int16_t *a, bus_size_t c);
void mbus_rrr_4(void *v, bus_space_handle_t h,
		   bus_size_t o, u_int32_t *a, bus_size_t c);
void mbus_rrr_8(void *v, bus_space_handle_t h,
		   bus_size_t o, u_int64_t *a, bus_size_t c);

void mbus_wrr_2(void *v, bus_space_handle_t h,
		   bus_size_t o, const u_int16_t *a, bus_size_t c);
void mbus_wrr_4(void *v, bus_space_handle_t h,
		   bus_size_t o, const u_int32_t *a, bus_size_t c);
void mbus_wrr_8(void *v, bus_space_handle_t h,
		   bus_size_t o, const u_int64_t *a, bus_size_t c);

void
mbus_sr_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int8_t *)h)++ = vv;
}

void
mbus_sr_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int16_t *)h)++ = vv;
}

void
mbus_sr_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int32_t *)h)++ = vv;
}

void
mbus_sr_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv, bus_size_t c)
{
	h += o;
	while (c--)
		*((volatile u_int64_t *)h)++ = vv;
}

void
mbus_cp_1(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	h1 += o1;
	h2 += o2;
	while (c--)
		*((volatile u_int8_t *)h1)++ =
			*((volatile u_int8_t *)h2)++;
}

void
mbus_cp_2(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	h1 += o1;
	h2 += o2;
	while (c--)
		*((volatile u_int16_t *)h1)++ =
			*((volatile u_int16_t *)h2)++;
}

void
mbus_cp_4(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	h1 += o1;
	h2 += o2;
	while (c--)
		*((volatile u_int32_t *)h1)++ =
			*((volatile u_int32_t *)h2)++;
}

void
mbus_cp_8(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	h1 += o1;
	h2 += o2;
	while (c--)
		*((volatile u_int64_t *)h1)++ =
			*((volatile u_int64_t *)h2)++;
}


/* ugly typecast macro */
#define	crr(n)	((void (*)(void *, bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t))(n))
#define	cwr(n)	((void (*)(void *, bus_space_handle_t, bus_size_t, const u_int8_t *, bus_size_t))(n))

const struct hppa_bus_space_tag hppa_bustag = {
	NULL,

	mbus_map, mbus_unmap, mbus_subregion, mbus_alloc, mbus_free,
	mbus_barrier,
	mbus_r1,    mbus_r2,   mbus_r4,   mbus_r8,
	mbus_w1,    mbus_w2,   mbus_w4,   mbus_w8,
	mbus_rm_1,  mbus_rm_2, mbus_rm_4, mbus_rm_8,
	mbus_wm_1,  mbus_wm_2, mbus_wm_4, mbus_wm_8,
	mbus_sm_1,  mbus_sm_2, mbus_sm_4, mbus_sm_8,
	/* *_raw_* are the same as non-raw for native busses */
	            crr(mbus_rm_1), crr(mbus_rm_1), crr(mbus_rm_1),
	            cwr(mbus_wm_1), cwr(mbus_wm_1), cwr(mbus_wm_1),
	mbus_rr_1,  mbus_rr_2, mbus_rr_4, mbus_rr_8,
	mbus_wr_1,  mbus_wr_2, mbus_wr_4, mbus_wr_8,
	/* *_raw_* are the same as non-raw for native busses */
	            crr(mbus_rr_1), crr(mbus_rr_1), crr(mbus_rr_1),
	            cwr(mbus_wr_1), cwr(mbus_wr_1), cwr(mbus_wr_1),
	mbus_sr_1,  mbus_sr_2, mbus_sr_4, mbus_sr_8,
	mbus_cp_1,  mbus_cp_2, mbus_cp_4, mbus_cp_8
};

int
mbus_dmamap_create(void *v, bus_size_t size, int nsegments,
		   bus_size_t maxsegsz, bus_size_t boundary, int flags,
		   bus_dmamap_t *dmamp)
{
	register struct hppa_bus_dmamap *map;
	register size_t mapsize;

	mapsize = sizeof(struct hppa_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	MALLOC(map, struct hppa_bus_dmamap *, mapsize, M_DEVBUF,
		(flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	if (!map)
		return (ENOMEM);

	bzero(map, mapsize);
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

void
mbus_dmamap_unload(void *v, bus_dmamap_t map)
{
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

void
mbus_dmamap_destroy(void *v, bus_dmamap_t map)
{
	if (map->dm_mapsize != 0)
		mbus_dmamap_unload(v, map);

	free(map, M_DEVBUF);
}

int
mbus_dmamap_load(void *v, bus_dmamap_t map, void *addr, bus_size_t size,
		 struct proc *p, int flags)
{
	paddr_t pa, pa_next;
	bus_size_t mapsize;
	bus_size_t off, pagesz;
	int seg;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
	map->_dm_va = (vaddr_t)addr;

	/* Load the memory. */
	pa_next = 0;
	seg = -1;
	mapsize = size;
	off = (bus_size_t)addr & (PAGE_SIZE - 1);
	addr = (void *) ((caddr_t)addr - off);
	for(; size > 0; ) {

		pmap_extract(pmap_kernel(), (vaddr_t)addr, &pa);
		if (pa != pa_next) {
			if (++seg >= map->_dm_segcnt)
				panic("mbus_dmamap_load: nsegs botch");
			map->dm_segs[seg].ds_addr = pa + off;
			map->dm_segs[seg].ds_len = 0;
		}
		pa_next = pa + PAGE_SIZE;
		pagesz = PAGE_SIZE - off;
		if (size < pagesz)
			pagesz = size;
		map->dm_segs[seg].ds_len += pagesz;
		size -= pagesz;
		addr = (caddr_t)addr + off + pagesz;
		off = 0;
	}

	/* Make the map truly valid. */
	map->dm_nsegs = seg + 1;
	map->dm_mapsize = mapsize;

	return (0);
}

int
mbus_dmamap_load_mbuf(void *v, bus_dmamap_t map, struct mbuf *m, int flags)
{
	panic("_dmamap_load_mbuf: not implemented");
}

int
mbus_dmamap_load_uio(void *v, bus_dmamap_t map, struct uio *uio, int flags)
{
	panic("_dmamap_load_uio: not implemented");
}

int
mbus_dmamap_load_raw(void *v, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	panic("_dmamap_load_raw: not implemented");
}

void
mbus_dmamap_sync(void *v, bus_dmamap_t map, bus_addr_t offset, bus_size_t len,
    int ops)
{

	if (ops & (BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE))
		__asm __volatile ("syncdma");

	if (ops & BUS_DMASYNC_PREREAD)
		pdcache(HPPA_SID_KERNEL, map->_dm_va + offset, len);
	else if (ops & BUS_DMASYNC_PREWRITE)
		fdcache(HPPA_SID_KERNEL, map->_dm_va + offset, len);
}

int
mbus_dmamem_alloc(void *v, bus_size_t size, bus_size_t alignment,
		  bus_size_t boundary, bus_dma_segment_t *segs, int nsegs,
		  int *rsegs, int flags)
{
	struct pglist pglist;
	struct vm_page *pg;
	vaddr_t va;

	size = round_page(size);

	if (uvm_pglistalloc(size, VM_MIN_KERNEL_ADDRESS, VM_MAX_KERNEL_ADDRESS,
	    alignment, 0, &pglist, 1, FALSE))
		return ENOMEM;

	if (uvm_map(kernel_map, &va, size, NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW, UVM_INH_NONE,
	      UVM_ADV_RANDOM, 0))) {
		uvm_pglistfree(&pglist);
		return ENOMEM;
	}

	segs[0].ds_addr = va;
	segs[0].ds_len = size;
	*rsegs = 1;

	TAILQ_FOREACH(pg, &pglist, pageq) {

		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), UVM_PROT_RW);
		/* XXX for now */
		pmap_changebit(pg, PTE_PROT(TLB_UNCACHABLE), 0);
		va += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
mbus_dmamem_free(void *v, bus_dma_segment_t *segs, int nsegs)
{
	uvm_km_free(kernel_map, segs[0].ds_addr, segs[0].ds_len);
}

int
mbus_dmamem_map(void *v, bus_dma_segment_t *segs, int nsegs, size_t size,
		caddr_t *kvap, int flags)
{
	*kvap = (caddr_t)segs[0].ds_addr;
	return 0;
}

void
mbus_dmamem_unmap(void *v, caddr_t kva, size_t size)
{
}

paddr_t
mbus_dmamem_mmap(void *v, bus_dma_segment_t *segs, int nsegs, off_t off,
		 int prot, int flags)
{
	panic("_dmamem_mmap: not implemented");
}

const struct hppa_bus_dma_tag hppa_dmatag = {
	NULL,
	mbus_dmamap_create, mbus_dmamap_destroy,
	mbus_dmamap_load, mbus_dmamap_load_mbuf,
	mbus_dmamap_load_uio, mbus_dmamap_load_raw,
	mbus_dmamap_unload, mbus_dmamap_sync,

	mbus_dmamem_alloc, mbus_dmamem_free, mbus_dmamem_map,
	mbus_dmamem_unmap, mbus_dmamem_mmap
};

int
mbmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;

	/* there will be only one */
	if (cf->cf_unit)
		return 0;

	return 1;
}

void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct pdc_hpa pdc_hpa PDC_ALIGNMENT;
	struct confargs nca;
	bus_space_handle_t ioh;

	/* fetch the "default" cpu hpa */
	if (pdc_call((iodcio_t)pdc, 0, PDC_HPA, PDC_HPA_DFLT, &pdc_hpa) < 0)
		panic("mbattach: PDC_HPA failed");

	if (bus_space_map(&hppa_bustag, pdc_hpa.hpa, IOMOD_HPASIZE, 0, &ioh))
		panic("mbattach: cannot map mainbus IO space");

	/*
	 * Local-Broadcast the HPA to all modules on the bus
	 */
	((struct iomod *)(pdc_hpa.hpa & HPPA_FLEX_MASK))[FPA_IOMOD].io_flex =
		(void *)((pdc_hpa.hpa & HPPA_FLEX_MASK) | DMA_ENABLE);

	sc->sc_hpa = pdc_hpa.hpa;
	printf(" [flex %x]\n", pdc_hpa.hpa & HPPA_FLEX_MASK);

	/* PDC first */
	bzero (&nca, sizeof(nca));
	nca.ca_name = "pdc";
	nca.ca_hpa = 0;
	nca.ca_hpamask = 0;
	nca.ca_iot = &hppa_bustag;
	nca.ca_dmatag = &hppa_dmatag;
	config_found(self, &nca, mbprint);

	bzero (&nca, sizeof(nca));
	nca.ca_name = "mainbus";
	nca.ca_hpa = 0;
	nca.ca_irq = -1;
	nca.ca_hpamask = HPPA_IOSPACE;
	nca.ca_iot = &hppa_bustag;
	nca.ca_dmatag = &hppa_dmatag;
	pdc_scanbus(self, &nca, -1, MAXMODBUS);
}

/*
 * retrive CPU #N HPA value
 */
hppa_hpa_t
cpu_gethpa(n)
	int n;
{
	register struct mainbus_softc *sc;

	sc = mainbus_cd.cd_devs[0];

	return sc->sc_hpa;
}

int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct confargs *ca = aux;

	if (pnp)
		printf("\"%s\" at %s (type %x, sv %x)", ca->ca_name, pnp,
		    ca->ca_type.iodc_type, ca->ca_type.iodc_sv_model);
	if (ca->ca_hpa) {
		if (~ca->ca_hpamask)
			printf(" offset %x", ca->ca_hpa & ~ca->ca_hpamask);
		if (!pnp && ca->ca_irq >= 0)
			printf(" irq %d", ca->ca_irq);
	}

	return (UNCONF);
}

int
mbsubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct cfdata *cf = match;
	register struct confargs *ca = aux;
	register int ret;

	if (autoconf_verbose)
		printf(">> hpa %x off %x cf_off %x\n",
		    ca->ca_hpa, ca->ca_hpa & ~ca->ca_hpamask, cf->hppacf_off);

	if (ca->ca_hpa && ~ca->ca_hpamask && cf->hppacf_off != -1 &&
	    ((ca->ca_hpa & ~ca->ca_hpamask) != cf->hppacf_off))
		return (0);

	if ((ret = (*cf->cf_attach->ca_match)(parent, match, aux)))
		ca->ca_irq = cf->hppacf_irq;

	return ret;
}


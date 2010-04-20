/*	$OpenBSD: sg_dma.c,v 1.6 2010/04/20 22:05:41 tedu Exp $	*/
/*
 * Copyright (c) 2009 Owain G. Ainsworth <oga@openbsd.org>
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
 * Copyright (c) 2003 Henric Jungheim
 * Copyright (c) 2001, 2002 Eduardo Horvath
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Support for scatter/gather style dma through agp or an iommu.
 */
#include <sys/param.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#ifndef MAX_DMA_SEGS
#define MAX_DMA_SEGS	20
#endif

int		sg_dmamap_load_seg(bus_dma_tag_t, struct sg_cookie *,
		    bus_dmamap_t, bus_dma_segment_t *, int, int, bus_size_t,
		    bus_size_t);
struct sg_page_map *sg_iomap_create(int);
int		sg_dmamap_append_range(bus_dma_tag_t, bus_dmamap_t, paddr_t,
		    bus_size_t, int, bus_size_t);
int		sg_iomap_insert_page(struct sg_page_map *, paddr_t);
bus_addr_t	sg_iomap_translate(struct sg_page_map *, paddr_t);
void		sg_iomap_load_map(struct sg_cookie *, struct sg_page_map *,
		    bus_addr_t, int);
void		sg_iomap_unload_map(struct sg_cookie *, struct sg_page_map *);
void		sg_iomap_destroy(struct sg_page_map *);
void		sg_iomap_clear_pages(struct sg_page_map *);

struct sg_cookie *
sg_dmatag_init(char *name, void *hdl, bus_addr_t start, bus_size_t size,
    void bind(void *, bus_addr_t, paddr_t, int),
    void unbind(void *, bus_addr_t), void flush_tlb(void *))
{
	struct sg_cookie	*cookie;

	cookie = malloc(sizeof(*cookie), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (cookie == NULL)
		return (NULL);

	cookie->sg_ex = extent_create(name, start, start + size - 1,
	    M_DEVBUF, NULL, NULL, EX_NOWAIT | EX_NOCOALESCE);
	if (cookie->sg_ex == NULL) {
		free(cookie, M_DEVBUF);
		return (NULL);
	}

	cookie->sg_hdl = hdl;
	mtx_init(&cookie->sg_mtx, IPL_HIGH);
	cookie->bind_page = bind;
	cookie->unbind_page = unbind;
	cookie->flush_tlb = flush_tlb;

	return (cookie);
}

void
sg_dmatag_destroy(struct sg_cookie *cookie)
{
	extent_destroy(cookie->sg_ex);
	free(cookie, M_DEVBUF);
}

int
sg_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamap)
{
	struct sg_page_map	*spm;
	bus_dmamap_t		 map;
	int			 ret;

	if ((ret = _bus_dmamap_create(t, size, nsegments, maxsegsz, boundary,
	    flags, &map)) != 0)
		return (ret);

	if ((spm = sg_iomap_create(atop(round_page(size)))) == NULL) {
		_bus_dmamap_destroy(t, map);
		return (ENOMEM);
	}

	map->_dm_cookie = spm;
	*dmamap = map;

	return (0);
}

void
sg_dmamap_set_alignment(bus_dma_tag_t tag, bus_dmamap_t dmam,
    u_long alignment)
{
	if (alignment < PAGE_SIZE)
		return;

	dmam->dm_segs[0]._ds_align = alignment;
}

void
sg_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	/*
	 * The specification (man page) requires a loaded
	 * map to be unloaded before it is destroyed.
	 */
	if (map->dm_nsegs)
		bus_dmamap_unload(t, map);

        if (map->_dm_cookie)
                sg_iomap_destroy(map->_dm_cookie);
	map->_dm_cookie = NULL;
	_bus_dmamap_destroy(t, map);
}

/*
 * Load a contiguous kva buffer into a dmamap.  The physical pages are
 * not assumed to be contiguous.  Two passes are made through the buffer
 * and both call pmap_extract() for the same va->pa translations.  It
 * is possible to run out of pa->dvma mappings; the code should be smart
 * enough to resize the iomap (when the "flags" permit allocation).  It
 * is trivial to compute the number of entries required (round the length
 * up to the page size and then divide by the page size)...
 */
int
sg_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	int err = 0;
	bus_size_t sgsize;
	u_long dvmaddr, sgstart, sgend;
	bus_size_t align, boundary;
	struct sg_cookie *is = t->_cookie;
	struct sg_page_map *spm = map->_dm_cookie;
	pmap_t pmap;

	if (map->dm_nsegs) {
		/*
		 * Is it still in use? _bus_dmamap_load should have taken care
		 * of this.
		 */
#ifdef DIAGNOSTIC
		panic("sg_dmamap_load: map still in use");
#endif
		bus_dmamap_unload(t, map);
	}

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen < 1 || buflen > map->_dm_size)
		return (EINVAL);

	/*
	 * A boundary presented to bus_dmamem_alloc() takes precedence
	 * over boundary in the map.
	 */
	if ((boundary = (map->dm_segs[0]._ds_boundary)) == 0)
		boundary = map->_dm_boundary;
	align = MAX(map->dm_segs[0]._ds_align, PAGE_SIZE);

	pmap = p ? p->p_vmspace->vm_map.pmap : pmap_kernel();

	/* Count up the total number of pages we need */
	sg_iomap_clear_pages(spm);
	{ /* Scope */
		bus_addr_t a, aend;
		bus_addr_t addr = (bus_addr_t)buf;
		int seg_len = buflen;

		aend = round_page(addr + seg_len);
		for (a = trunc_page(addr); a < aend; a += PAGE_SIZE) {
			paddr_t pa;

			if (pmap_extract(pmap, a, &pa) == FALSE) {
				printf("iomap pmap error addr 0x%llx\n", a);
				sg_iomap_clear_pages(spm);
				return (EFBIG);
			}

			err = sg_iomap_insert_page(spm, pa);
			if (err) {
				printf("iomap insert error: %d for "
				    "va 0x%llx pa 0x%lx "
				    "(buf %p len %lld/%llx)\n",
				    err, a, pa, buf, buflen, buflen);
				sg_iomap_clear_pages(spm);
				return (EFBIG);
			}
		}
	}
	sgsize = spm->spm_pagecnt * PAGE_SIZE;

	mtx_enter(&is->sg_mtx);
	if (flags & BUS_DMA_24BIT) {
		sgstart = MAX(is->sg_ex->ex_start, 0xff000000);
		sgend = MIN(is->sg_ex->ex_end, 0xffffffff);
	} else {
		sgstart = is->sg_ex->ex_start;
		sgend = is->sg_ex->ex_end;
	}

	/* 
	 * If our segment size is larger than the boundary we need to 
	 * split the transfer up into little pieces ourselves.
	 */
	err = extent_alloc_subregion(is->sg_ex, sgstart, sgend,
	    sgsize, align, 0, (sgsize > boundary) ? 0 : boundary, 
	    EX_NOWAIT | EX_BOUNDZERO, (u_long *)&dvmaddr);
	mtx_leave(&is->sg_mtx);
	if (err != 0) {
		sg_iomap_clear_pages(spm);
		return (err);
	}

	/* Set the active DVMA map */
	spm->spm_start = dvmaddr;
	spm->spm_size = sgsize;

	map->dm_mapsize = buflen;

	sg_iomap_load_map(is, spm, dvmaddr, flags);

	{ /* Scope */
		bus_addr_t a, aend;
		bus_addr_t addr = (bus_addr_t)buf;
		int seg_len = buflen;

		aend = round_page(addr + seg_len);
		for (a = trunc_page(addr); a < aend; a += PAGE_SIZE) {
			bus_addr_t pgstart;
			bus_addr_t pgend;
			paddr_t pa;
			int pglen;

			/* Yuck... Redoing the same pmap_extract... */
			if (pmap_extract(pmap, a, &pa) == FALSE) {
				printf("iomap pmap error addr 0x%llx\n", a);
				err = EFBIG;
				break;
			}

			pgstart = pa | (MAX(a, addr) & PAGE_MASK);
			pgend = pa | (MIN(a + PAGE_SIZE - 1,
			    addr + seg_len - 1) & PAGE_MASK);
			pglen = pgend - pgstart + 1;

			if (pglen < 1)
				continue;

			err = sg_dmamap_append_range(t, map, pgstart,
			    pglen, flags, boundary);
			if (err == EFBIG)
				break;
			else if (err) {
				printf("iomap load seg page: %d for "
				    "va 0x%llx pa %lx (%llx - %llx) "
				    "for %d/0x%x\n",
				    err, a, pa, pgstart, pgend, pglen, pglen);
				break;
			}
		}
	}
	if (err) {
		sg_dmamap_unload(t, map);
	} else {
		spm->spm_origbuf = buf;
		spm->spm_buftype = BUS_BUFTYPE_LINEAR;
		spm->spm_proc = p;
	}

	return (err);
}

/*
 * Load an mbuf into our map. we convert it to some bus_dma_segment_ts then
 * pass it to load_raw.
 */
int
sg_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *mb,
    int flags)
{
	/*
	 * This code is adapted from sparc64, for very fragmented data
	 * we may need to adapt the algorithm
	 */
	bus_dma_segment_t	 segs[MAX_DMA_SEGS];
	struct sg_page_map	*spm = map->_dm_cookie;
	size_t			 len;
	int			 i, err;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	if (mb->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	i = 0;
	len = 0;
	while (mb) {
		vaddr_t	vaddr = mtod(mb, vaddr_t);
		long	buflen = (long)mb->m_len;

		len += buflen;
		while (buflen > 0 && i < MAX_DMA_SEGS) {
			paddr_t	pa;
			long incr;

			incr = min(buflen, NBPG);

			if (pmap_extract(pmap_kernel(), vaddr, &pa) == FALSE)
				return EINVAL;

			buflen -= incr;
			vaddr += incr;

			if (i > 0 && pa == (segs[i - 1].ds_addr +
			    segs[i - 1].ds_len) && ((segs[i - 1].ds_len + incr)
			    < map->_dm_maxsegsz)) {
				/* contigious, great! */
				segs[i - 1].ds_len += incr;
				continue;
			}
			segs[i].ds_addr = pa;
			segs[i].ds_len = incr;
			segs[i]._ds_boundary = 0;
			segs[i]._ds_align = 0;
			i++;
		}
		mb = mb->m_next;
		if (mb && i >= MAX_DMA_SEGS) {
			/* our map, it is too big! */
			return (EFBIG);
		}
	}

	err = sg_dmamap_load_raw(t, map, segs, i, (bus_size_t)len, flags);

	if (err == 0) {
		spm->spm_origbuf = mb;
		spm->spm_buftype = BUS_BUFTYPE_MBUF;
	}
	return (err);
}

/*
 * Load a uio into the map. Turn it into segments and call load_raw()
 */
int
sg_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio,
    int flags)
{
	/*
	 * loading uios is kinda broken since we can't lock the pages.
	 * and unlock them at unload. Perhaps page loaning is the answer.
	 * 'till then we only accept kernel data
	 */
	bus_dma_segment_t	 segs[MAX_DMA_SEGS];
	struct sg_page_map	*spm = map->_dm_cookie;
	size_t			 len;
	int			 i, j, err;

	/*
	 * Make sure that on errror we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	if (uio->uio_resid > map->_dm_size)
		return (EINVAL);

	if (uio->uio_segflg != UIO_SYSSPACE)
		return (EOPNOTSUPP);

	i = j = 0;
	len = 0;
	while (j < uio->uio_iovcnt) {
		vaddr_t	vaddr = (vaddr_t)uio->uio_iov[j].iov_base;
		long	buflen = (long)uio->uio_iov[j].iov_len;

		len += buflen;
		while (buflen > 0 && i < MAX_DMA_SEGS) {
			paddr_t pa;
			long incr;

			incr = min(buflen, NBPG);
			(void)pmap_extract(pmap_kernel(), vaddr, &pa);
			buflen -= incr;
			vaddr += incr;

			if (i > 0 && pa == (segs[i - 1].ds_addr +
			    segs[i -1].ds_len) && ((segs[i - 1].ds_len + incr)
			    < map->_dm_maxsegsz)) {
				/* contigious, yay! */
				segs[i - 1].ds_len += incr;
				continue;
			}
			segs[i].ds_addr = pa;
			segs[i].ds_len = incr;
			segs[i]._ds_boundary = 0;
			segs[i]._ds_align = 0;
			i++;
		}
		j++;
		if ((uio->uio_iovcnt - j) && i >= MAX_DMA_SEGS) {
			/* Our map, is it too big! */
			return (EFBIG);
		}

	}

	err = sg_dmamap_load_raw(t, map, segs, i, (bus_size_t)len, flags);

	if (err == 0) {
		spm->spm_origbuf = uio;
		spm->spm_buftype = BUS_BUFTYPE_UIO;
	}
	return (err);
}

/*
 * Load a dvmamap from an array of segs.  It calls sg_dmamap_append_range()
 * or for part of the 2nd pass through the mapping.
 */
int
sg_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	int i;
	int left;
	int err = 0;
	bus_size_t sgsize;
	bus_size_t boundary, align;
	u_long dvmaddr, sgstart, sgend;
	struct sg_cookie *is = t->_cookie;
	struct sg_page_map *spm = map->_dm_cookie;

	if (map->dm_nsegs) {
		/* Already in use?? */
#ifdef DIAGNOSTIC
		panic("sg_dmamap_load_raw: map still in use");
#endif
		bus_dmamap_unload(t, map);
	}

	/*
	 * A boundary presented to bus_dmamem_alloc() takes precedence
	 * over boundary in the map.
	 */
	if ((boundary = segs[0]._ds_boundary) == 0)
		boundary = map->_dm_boundary;

	align = MAX(MAX(segs[0]._ds_align, map->dm_segs[0]._ds_align),
	    PAGE_SIZE);

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	sg_iomap_clear_pages(spm);
	/* Count up the total number of pages we need */
	for (i = 0, left = size; left > 0 && i < nsegs; i++) {
		bus_addr_t a, aend;
		bus_size_t len = segs[i].ds_len;
		bus_addr_t addr = segs[i].ds_addr;
		int seg_len = MIN(left, len);

		if (len < 1)
			continue;

		aend = round_page(addr + seg_len);
		for (a = trunc_page(addr); a < aend; a += PAGE_SIZE) {

			err = sg_iomap_insert_page(spm, a);
			if (err) {
				printf("iomap insert error: %d for "
				    "pa 0x%llx\n", err, a);
				sg_iomap_clear_pages(spm);
				return (EFBIG);
			}
		}

		left -= seg_len;
	}
	sgsize = spm->spm_pagecnt * PAGE_SIZE;

	mtx_enter(&is->sg_mtx);
	if (flags & BUS_DMA_24BIT) {
		sgstart = MAX(is->sg_ex->ex_start, 0xff000000);
		sgend = MIN(is->sg_ex->ex_end, 0xffffffff);
	} else {
		sgstart = is->sg_ex->ex_start;
		sgend = is->sg_ex->ex_end;
	}

	/* 
	 * If our segment size is larger than the boundary we need to 
	 * split the transfer up into little pieces ourselves.
	 */
	err = extent_alloc_subregion(is->sg_ex, sgstart, sgend,
	    sgsize, align, 0, (sgsize > boundary) ? 0 : boundary, 
	    EX_NOWAIT | EX_BOUNDZERO, (u_long *)&dvmaddr);
	mtx_leave(&is->sg_mtx);

	if (err != 0) {
		sg_iomap_clear_pages(spm);
		return (err);
	}

	/* Set the active DVMA map */
	spm->spm_start = dvmaddr;
	spm->spm_size = sgsize;

	map->dm_mapsize = size;

	sg_iomap_load_map(is, spm, dvmaddr, flags);

	err = sg_dmamap_load_seg(t, is, map, segs, nsegs, flags,
	    size, boundary);

	if (err) {
		sg_dmamap_unload(t, map);
	} else {
		/* This will be overwritten if mbuf or uio called us */
		spm->spm_origbuf = segs;
		spm->spm_buftype = BUS_BUFTYPE_RAW;
	}

	return (err);
}

/*
 * Insert a range of addresses into a loaded map respecting the specified
 * boundary and alignment restrictions.  The range is specified by its 
 * physical address and length.  The range cannot cross a page boundary.
 * This code (along with most of the rest of the function in this file)
 * assumes that the IOMMU page size is equal to PAGE_SIZE.
 */
int
sg_dmamap_append_range(bus_dma_tag_t t, bus_dmamap_t map, paddr_t pa,
    bus_size_t length, int flags, bus_size_t boundary)
{
	struct sg_page_map *spm = map->_dm_cookie;
	bus_addr_t sgstart, sgend, bd_mask;
	bus_dma_segment_t *seg = NULL;
	int i = map->dm_nsegs;

	sgstart = sg_iomap_translate(spm, pa);
	sgend = sgstart + length - 1;

#ifdef DIAGNOSTIC
	if (sgstart == NULL || sgstart > sgend) {
		printf("append range invalid mapping for %lx "
		    "(0x%llx - 0x%llx)\n", pa, sgstart, sgend);
		map->dm_nsegs = 0;
		return (EINVAL);
	}
#endif

#ifdef DEBUG
	if (trunc_page(sgstart) != trunc_page(sgend)) {
		printf("append range crossing page boundary! "
		    "pa %lx length %lld/0x%llx sgstart %llx sgend %llx\n",
		    pa, length, length, sgstart, sgend);
	}
#endif

	/*
	 * We will attempt to merge this range with the previous entry
	 * (if there is one).
	 */
	if (i > 0) {
		seg = &map->dm_segs[i - 1];
		if (sgstart == seg->ds_addr + seg->ds_len) {
			length += seg->ds_len;
			sgstart = seg->ds_addr;
			sgend = sgstart + length - 1;
		} else
			seg = NULL;
	}

	if (seg == NULL) {
		seg = &map->dm_segs[i];
		if (++i > map->_dm_segcnt) {
			map->dm_nsegs = 0;
			return (EFBIG);
		}
	}

	/*
	 * At this point, "i" is the index of the *next* bus_dma_segment_t
	 * (the segment count, aka map->dm_nsegs) and "seg" points to the
	 * *current* entry.  "length", "sgstart", and "sgend" reflect what
	 * we intend to put in "*seg".  No assumptions should be made about
	 * the contents of "*seg".  Only "boundary" issue can change this
	 * and "boundary" is often zero, so explicitly test for that case
	 * (the test is strictly an optimization).
	 */ 
	if (boundary != 0) {
		bd_mask = ~(boundary - 1);

		while ((sgstart & bd_mask) != (sgend & bd_mask)) {
			/*
			 * We are crossing a boundary so fill in the current
			 * segment with as much as possible, then grab a new
			 * one.
			 */

			seg->ds_addr = sgstart;
			seg->ds_len = boundary - (sgstart & bd_mask);

			sgstart += seg->ds_len; /* sgend stays the same */
			length -= seg->ds_len;

			seg = &map->dm_segs[i];
			if (++i > map->_dm_segcnt) {
				map->dm_nsegs = 0;
				return (EFBIG);
			}
		}
	}

	seg->ds_addr = sgstart;
	seg->ds_len = length;
	map->dm_nsegs = i;

	return (0);
}

/*
 * Populate the iomap from a bus_dma_segment_t array.  See note for
 * sg_dmamap_load() regarding page entry exhaustion of the iomap.
 * This is less of a problem for load_seg, as the number of pages
 * is usually similar to the number of segments (nsegs).
 */
int
sg_dmamap_load_seg(bus_dma_tag_t t, struct sg_cookie *is,
    bus_dmamap_t map, bus_dma_segment_t *segs, int nsegs, int flags,
    bus_size_t size, bus_size_t boundary)
{
	int i;
	int left;
	int seg;

	/*
	 * Keep in mind that each segment could span
	 * multiple pages and that these are not always
	 * adjacent. The code is no longer adding dvma
	 * aliases to the IOMMU.  The STC will not cross
	 * page boundaries anyway and a IOMMU table walk
	 * vs. what may be a streamed PCI DMA to a ring
	 * descriptor is probably a wash.  It eases TLB
	 * pressure and in the worst possible case, it is
	 * only as bad a non-IOMMUed architecture.  More
	 * importantly, the code is not quite as hairy.
	 * (It's bad enough as it is.)
	 */
	left = size;
	seg = 0;
	for (i = 0; left > 0 && i < nsegs; i++) {
		bus_addr_t a, aend;
		bus_size_t len = segs[i].ds_len;
		bus_addr_t addr = segs[i].ds_addr;
		int seg_len = MIN(left, len);

		if (len < 1)
			continue;

		aend = round_page(addr + seg_len);
		for (a = trunc_page(addr); a < aend; a += PAGE_SIZE) {
			bus_addr_t pgstart;
			bus_addr_t pgend;
			int pglen;
			int err;

			pgstart = MAX(a, addr);
			pgend = MIN(a + PAGE_SIZE - 1, addr + seg_len - 1);
			pglen = pgend - pgstart + 1;
			
			if (pglen < 1)
				continue;

			err = sg_dmamap_append_range(t, map, pgstart,
			    pglen, flags, boundary);
			if (err == EFBIG)
				return (err);
			if (err) {
				printf("iomap load seg page: %d for "
				    "pa 0x%llx (%llx - %llx for %d/%x\n",
				    err, a, pgstart, pgend, pglen, pglen);
				return (err);
			}

		}

		left -= seg_len;
	}
	return (0);
}

/*
 * Unload a dvmamap.
 */
void
sg_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct sg_cookie	*is = t->_cookie;
	struct sg_page_map	*spm = map->_dm_cookie;
	bus_addr_t		 dvmaddr = spm->spm_start;
	bus_size_t		 sgsize = spm->spm_size;
	int			 error;

	/* Remove the IOMMU entries */
	sg_iomap_unload_map(is, spm);

	/* Clear the iomap */
	sg_iomap_clear_pages(spm);

	mtx_enter(&is->sg_mtx);
	error = extent_free(is->sg_ex, dvmaddr, 
		sgsize, EX_NOWAIT);
	spm->spm_start = 0;
	spm->spm_size = 0;
	mtx_leave(&is->sg_mtx);
	if (error != 0)
		printf("warning: %qd of DVMA space lost\n", sgsize);

	spm->spm_buftype = BUS_BUFTYPE_INVALID;
	spm->spm_origbuf = NULL;
	spm->spm_proc = NULL;
	_bus_dmamap_unload(t, map);
}

/*
 * Alloc dma safe memory, telling the backend that we're scatter gather
 * to ease pressure on the vm.
 *
 * This assumes that we can map all physical memory. 
 */
int
sg_dmamem_alloc(bus_dma_tag_t t, bus_size_t size,
    bus_size_t alignment, bus_size_t boundary, bus_dma_segment_t *segs,
    int nsegs, int *rsegs, int flags)
{
	return (_bus_dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags | BUS_DMA_SG, 0, -1));
}

/*
 * Create a new iomap.
 */
struct sg_page_map *
sg_iomap_create(int n)
{
	struct sg_page_map	*spm;

	/* Safety for heavily fragmented data, such as mbufs */
	n += 4;
	if (n < 16)
		n = 16;

	spm = malloc(sizeof(*spm) + (n - 1) * sizeof(spm->spm_map[0]),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (spm == NULL)
		return (NULL);

	/* Initialize the map. */
	spm->spm_maxpage = n;
	SPLAY_INIT(&spm->spm_tree);

	return (spm);
}

/*
 * Destroy an iomap.
 */
void
sg_iomap_destroy(struct sg_page_map *spm)
{
#ifdef DIAGNOSTIC
	if (spm->spm_pagecnt > 0)
		printf("sg_iomap_destroy: %d page entries in use\n",
		    spm->spm_pagecnt);
#endif

	free(spm, M_DEVBUF);
}

/*
 * Utility function used by splay tree to order page entries by pa.
 */
static inline int
iomap_compare(struct sg_page_entry *a, struct sg_page_entry *b)
{
	return ((a->spe_pa > b->spe_pa) ? 1 :
		(a->spe_pa < b->spe_pa) ? -1 : 0);
}

SPLAY_PROTOTYPE(sg_page_tree, sg_page_entry, spe_node, iomap_compare);

SPLAY_GENERATE(sg_page_tree, sg_page_entry, spe_node, iomap_compare);

/*
 * Insert a pa entry in the iomap.
 */
int
sg_iomap_insert_page(struct sg_page_map *spm, paddr_t pa)
{
	struct sg_page_entry *e;

	if (spm->spm_pagecnt >= spm->spm_maxpage) {
		struct sg_page_entry spe;

		spe.spe_pa = pa;
		if (SPLAY_FIND(sg_page_tree, &spm->spm_tree, &spe))
			return (0);

		return (ENOMEM);
	}

	e = &spm->spm_map[spm->spm_pagecnt];

	e->spe_pa = pa;
	e->spe_va = NULL;

	e = SPLAY_INSERT(sg_page_tree, &spm->spm_tree, e);

	/* Duplicates are okay, but only count them once. */
	if (e)
		return (0);

	++spm->spm_pagecnt;

	return (0);
}

/*
 * Locate the iomap by filling in the pa->va mapping and inserting it
 * into the IOMMU tables.
 */
void
sg_iomap_load_map(struct sg_cookie *sc, struct sg_page_map *spm,
    bus_addr_t vmaddr, int flags)
{
	struct sg_page_entry	*e;
	int			 i;

	for (i = 0, e = spm->spm_map; i < spm->spm_pagecnt; ++i, ++e) {
		e->spe_va = vmaddr;
		sc->bind_page(sc->sg_hdl, e->spe_va, e->spe_pa, flags);
		vmaddr += PAGE_SIZE;
	}
	sc->flush_tlb(sc->sg_hdl);
}

/*
 * Remove the iomap from the IOMMU.
 */
void
sg_iomap_unload_map(struct sg_cookie *sc, struct sg_page_map *spm)
{
	struct sg_page_entry	*e;
	int			 i;

	for (i = 0, e = spm->spm_map; i < spm->spm_pagecnt; ++i, ++e)
		sc->unbind_page(sc->sg_hdl, e->spe_va);
	sc->flush_tlb(sc->sg_hdl);

}

/*
 * Translate a physical address (pa) into a DVMA address.
 */
bus_addr_t
sg_iomap_translate(struct sg_page_map *spm, paddr_t pa)
{
	struct sg_page_entry	*e, pe;
	paddr_t			 offset = pa & PAGE_MASK;

	pe.spe_pa = trunc_page(pa);

	e = SPLAY_FIND(sg_page_tree, &spm->spm_tree, &pe);

	if (e == NULL)
		return (NULL);

	return (e->spe_va | offset);
}

/*
 * Clear the iomap table and tree.
 */
void
sg_iomap_clear_pages(struct sg_page_map *spm)
{
	spm->spm_pagecnt = 0;
	SPLAY_INIT(&spm->spm_tree);
}

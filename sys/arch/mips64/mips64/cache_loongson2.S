/*	$OpenBSD: cache_loongson2.S,v 1.3 2009/12/25 21:02:15 miod Exp $	*/

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
/*
 * Copyright (c) 1998-2004 Opsycon AB (www.opsycon.se)
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
 * Processors supported:
 * Loongson 2E/2F (code could be modified to work on 2C by not hardcoding
 * the number of ways).
 */

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>

#include "assym.h"

	.set	mips3

#define	LOAD_XKPHYS(reg, cca) \
	li	reg, cca | 0x10; \
	dsll	reg, reg, 59

/* L1 cache operations */
#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexLoadTag_D		0x05
#define	IndexStoreTag_D		0x09
#define	HitInvalidate_D		0x11
#define	HitWBInvalidate_D	0x15
#define	IndexLoadData_D		0x19
#define	IndexStoreData_D	0x1d

/* L2 cache operations */
#define	IndexWBInvalidate_S	0x03
#define	IndexLoadTag_S		0x07
#define	IndexStoreTag_S		0x0b
#define	HitInvalidate_S		0x13
#define	HitWBInvalidate_S	0x17
#define	IndexLoadData_S		0x1b
#define	IndexStoreData_S	0x1f

/*
 *  Define cache type definition bits. NOTE! the 3 lsb may NOT change!
 */
#define	CTYPE_DIR		0x0001	/* Cache is direct mapped */
#define	CTYPE_2WAY		0x0002	/* Cache is TWO way */
#define	CTYPE_4WAY		0x0004	/* Cache is FOUR way */
#define	CTYPE_WAYMASK		0x0007

#define	CTYPE_HAS_IL2		0x0100	/* Internal L2 Cache present */
#define	CTYPE_HAS_XL2		0x0200	/* External L2 Cache present */
#define	CTYPE_HAS_XL3		0x0400	/* External L3 Cache present */

	.set	noreorder		# Noreorder is default style!

/*----------------------------------------------------------------------------
 *
 * Loongson2_ConfigCache --
 *
 *	Setup various cache-dependent variables:
 *	The size of the data cache is stored into CpuPrimaryDataCacheSize.
 *	The size of instruction cache is stored into CpuPrimaryInstCacheSize.
 *	Alignment mask for cache aliasing test is stored in CpuCacheAliasMask.
 *	CpuSecondaryCacheSize is set to the size of the secondary cache.
 *	CpuTertiaryCacheSize is set to the size of the tertiary cache.
 *	CpuNWayCache is set to 0 for direct mapped caches, 2 for two way
 *	caches and 4 for four way caches. This primarily indicates the
 *	primary cache associativity.
 *
 * Allocation:
 *	ta0, ta1 ta2 used to hold I and D set size and Alias mask.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_ConfigCache, 0)
	mfc0	v0, COP_0_CONFIG		# Get configuration register

	srl	t1, v0, 9			# Get I cache size.
	and	t1, 7
	li	t2, 4096
	sllv	ta0, t2, t1			# ta0 = Initial I set size.

	and	t2, v0, 0x20
	srl	t2, t2, 1			# Get I cache line size.
	addu	t2, t2, 16
	sw	t2, CpuPrimaryInstCacheLSize

	srl	t1, v0, 6			# Get D cache size.
	and	t1, 7
	li	t2, 4096			# Fixed page size.
	sllv	ta1, t2, t1

	and	t2, v0, 0x10
	addu	t2, t2, 16			# Get D cache line size.
	sw	t2, CpuPrimaryDataCacheLSize

	li	ta3, 0				# Tertiary size 0.

	li	ta2, 512 * 1024			# fixed L2 size...
	li	t2, (CTYPE_4WAY|CTYPE_HAS_IL2)	# caches are 4-way, internal L2

/*
 * Get here with t2 = Cache type, ta0 = L1 I size, ta1 = L1 D size.
 * ta2 = secondary size, ta3 = tertiary size.
 */
ConfResult:
	sw	v0, CpuConfigRegister
	mfc0	t3, COP_0_STATUS_REG
	sw	t2, CpuCacheType		# Save cache attributes
	sw	t3, CpuStatusRegister
	and	t2, CTYPE_WAYMASK		# isolate number of sets.
	sw	t2, CpuNWayCache
	srl	t2, 1				# get div shift for set size.

	sw	ta2, CpuSecondaryCacheSize
	sw	ta3, CpuTertiaryCacheSize

	addu	t1, ta0, -1			# Use icache for alias mask
	srl	t1, t2				# Some cpus have different
	and	t1, ~(PAGE_SIZE - 1)		# i and d cache sizes...
	sw	t1, CpuCacheAliasMask

	sw	ta0, CpuPrimaryInstCacheSize	# store cache size.
	sw	ta1, CpuPrimaryDataCacheSize	# store cache size.

	/*
	 * Cache way number encoding is done in the lowest bits, and
	 * these variables are not used. We make them nonzero so
	 * that `mi' code can divide by them if necessary.
	 */
	li	ta1, 1
	sw	ta1, CpuPrimaryInstSetSize
	sw	ta1, CpuPrimaryDataSetSize

	j	ra
	nop
END(Loongson2_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_SyncCache --
 *
 *	Sync ALL caches.
 *	No need to look at number of sets since we are cleaning out
 *	the entire cache and thus will address all sets anyway.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_SyncCache, 0)
	sync

	lw	t1, CpuPrimaryInstCacheSize
	srl	t1, t1, 2			# / 4ways
	lw	t2, CpuPrimaryDataCacheSize
	srl	t2, t2, 2			# / 4ways

	/* L1 I$ */

	LOAD_XKPHYS(t0, CCA_CACHED)
	PTR_ADDU t1, t0, t1			# Compute end address
	PTR_SUBU t1, 32
1:
	cache	IndexInvalidate_I, 0(t0)
	bne	t0, t1, 1b
	PTR_ADDU t0, 32

	/* L1 D$ */

	LOAD_XKPHYS(t0, CCA_CACHED)
	PTR_ADDU t1, t0, t2			# End address
	PTR_SUBU t1, 32
1:
	cache	IndexWBInvalidate_D, 0(t0)
	cache	IndexWBInvalidate_D, 1(t0)
	cache	IndexWBInvalidate_D, 2(t0)
	cache	IndexWBInvalidate_D, 3(t0)
	bne	t0, t1, 1b
	PTR_ADDU t0, 32

	/* L2 */

	LOAD_XKPHYS(t0, CCA_CACHED)
	lw	t2, CpuSecondaryCacheSize
	srl	t2, 2				# because cache is 4 way
	PTR_ADDU t1, t0, t2
	PTR_SUBU t1, 32
1:
	cache	IndexWBInvalidate_S, 0(t0)
	cache	IndexWBInvalidate_S, 1(t0)
	cache	IndexWBInvalidate_S, 2(t0)
	cache	IndexWBInvalidate_S, 3(t0)
	bne	t0, t1, 1b
	PTR_ADDU t0, 32

	j	ra
	nop
END(Loongson2_SyncCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_InvalidateICache --
 *
 *	void Loongson2_SyncICache(vaddr_t va, size_t len)
 *
 *	Invalidate the L1 instruction cache for at least range
 *	of va to va + len - 1.
 *	The address is reduced to a XKPHYS index to avoid TLB faults.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_InvalidateICache, 0)
	andi	a0, ((1 << 14) - 1)		# only keep index bits
	PTR_ADDU a1, 31				# Round up size
	LOAD_XKPHYS(a2, CCA_CACHED)
	PTR_ADDU a1, a0				# Add extra from address
	dsrl	a0, a0, 5
	dsll	a0, a0, 5			# align address
	PTR_SUBU a1, a1, a0
	PTR_ADDU a0, a2				# a0 now new XKPHYS address
	dsrl	a1, a1, 5			# Number of unrolled loops
1:
	PTR_ADDU a1, -1
	cache	IndexInvalidate_I, 0(a0)
	bne	a1, zero, 1b
	PTR_ADDU a0, 32

	j	ra
	nop
END(Loongson2_InvalidateICache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_SyncDCachePage --
 *
 *	void Loongson2_SyncDCachePage(paddr_t pa)
 *
 *	Sync the L1 and L2 data cache page for address pa.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_SyncDCachePage, 0)
	sync

	LOAD_XKPHYS(a1, CCA_CACHED)
	or	a0, a1				# a0 now new L1 address
	dsrl	a0, a0, PAGE_SHIFT
	dsll	a0, a0, PAGE_SHIFT		# page align pa
	move	a1, a0				# save for L2

	/* L1 */
	PTR_ADDU a2, a0, PAGE_SIZE-32
1:
	cache	HitWBInvalidate_D, 0(a0)
	bne	a2, a0, 1b
	PTR_ADDU a0, 32

	/* L2 */
	PTR_ADDU a2, a1, PAGE_SIZE-32
2:
	cache	HitWBInvalidate_S, 0(a1)
	bne	a2, a1, 2b
	PTR_ADDU a1, 32

	j	ra
	nop
END(Loongson2_SyncDCachePage)

/*----------------------------------------------------------------------------
 *
 * Loongson2_HitSyncDCache --
 *
 *	void Loongson2_HitSyncDCache(paddr_t pa, size_t len)
 *
 *	Sync L1 and L2 data caches for range of pa to pa + len - 1.
 *	Since L2 is writeback, we need to operate on L1 first, to make sure
 *	L1 is clean. The usual mips strategy of doing L2 first, and then
 *	the L1 orphans, will not work as the orphans would only be pushed
 *	to L2, and not to physical memory.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitSyncDCache, 0)
	sync

	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, 31				# Round up
	PTR_ADDU a1, a1, a0			# Add extra from address
	dsrl	a0, a0, 5
	dsll	a0, a0, 5			# align to cacheline boundary
	PTR_SUBU a1, a1, a0
	dsrl	a1, a1, 5			# Compute number of cache lines
	LOAD_XKPHYS(a2, CCA_CACHED)
	or	a0, a2				# build suitable va

	move	a3, a1				# save for L2
	move	a2, a0

	/* L1 */
1:
	PTR_ADDU a1, -1
	cache	HitWBInvalidate_D, 0(a0)
	bne	a1, zero, 1b
	PTR_ADDU a0, 32

	/* L2 */
2:
	PTR_ADDU a3, -1
	cache	HitWBInvalidate_S, 0(a2)
	bne	a3, zero, 2b
	PTR_ADDU a2, 32

3:
	j	ra
	nop
END(Loongson2_HitSyncDCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_HitInvalidateDCache --
 *
 *	void Loongson2_HitInvalidateDCache(paddr_t pa, size_t len)
 *
 *	Invalidate L1 and L2 data caches for range of pa to pa + len - 1.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitInvalidateDCache, 0)
	sync

	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, 31				# Round up
	PTR_ADDU a1, a1, a0			# Add extra from address
	dsrl	a0, a0, 5
	dsll	a0, a0, 5			# align to cacheline boundary
	PTR_SUBU a1, a1, a0
	dsrl	a1, a1, 5			# Compute number of cache lines
	LOAD_XKPHYS(a2, CCA_CACHED)
	or	a0, a2				# build suitable va

	move	a3, a1				# save for L2
	move	a2, a0

	/* L1 */
1:
	PTR_ADDU a1, -1
	cache	HitInvalidate_D, 0(a0)
	bne	a1, zero, 1b
	PTR_ADDU a0, 32

	/* L2 */
2:
	PTR_ADDU a3, -1
	cache	HitInvalidate_S, 0(a2)
	bne	a3, zero, 2b
	PTR_ADDU a2, 32

3:
	j	ra
	nop
END(Loongson2_HitInvalidateDCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_IOSyncDCache --
 *
 *	void Loongson2_IOSyncDCache(paddr_t pa, size_t len, int how)
 *
 *	Invalidate or flush L1 and L2 data caches for range of pa to
 *	pa + len - 1.
 *
 *	If how == 0 (invalidate):
 *		L1 and L2 caches are invalidated or flushed if the area
 *		does not match the alignment requirements.
 *	If how == 1 (writeback):
 *		L1 and L2 are written back.
 *	If how == 2 (writeback and invalidate):
 *		L1 and L2 are written back to memory and invalidated (flushed).
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(Loongson2_IOSyncDCache, FRAMESZ(CF_SZ+REGSZ), ra)
	sync

	PTR_SUBU sp, FRAMESZ(CF_SZ+REGSZ)
	PTR_S	ra, CF_RA_OFFS+REGSZ(sp)
	beqz	a2, SyncInv			# Sync PREREAD
	nop

SyncWBInv:
	jal	Loongson2_HitSyncDCache
	nop
	b	SyncDone
	PTR_L	ra, CF_RA_OFFS+REGSZ(sp)

SyncInv:
	or	t0, a0, a1			# check if invalidate possible
	and	t0, t0, 31			# both address and size must
	bnez	t0, SyncWBInv			# be aligned to the cache size
	nop

	jal	Loongson2_HitInvalidateDCache
	nop
	PTR_L	ra, CF_RA_OFFS+REGSZ(sp)

SyncDone:
	j	ra
	PTR_ADDU sp, FRAMESZ(CF_SZ+REGSZ)
END(Loongson2_IOSyncDCache)

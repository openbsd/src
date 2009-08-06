/*	$OpenBSD: cache_loongson2.S,v 1.1 2009/08/06 21:11:39 miod Exp $	*/

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
 *  Processors supported:
 *  Loongson 2E/2F
 */

#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>

#include "assym.h"

	.set	mips3

#define	LOAD_XKPHYS(reg, cca) \
	li	reg, cca | 0x10; \
	dsll	reg, reg, 59

/*
 *  Skip the .h file. Noone else need to know!
 */

#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexWBInvalidate_S	0x03

#define	IndexLoadTag_I		0x04
#define	IndexLoadTag_D		0x05
#define	IndexLoadTag_S		0x07

#define	IndexStoreTag_I		0x08
#define	IndexStoreTag_D		0x09
#define	IndexStoreTag_S		0x0b

#define	HitInvalidate_D		0x11
#define	HitInvalidate_S		0x13

#define	Fill_I			0x14
#define	HitWBInvalidate_D	0x15
#define	HitWBInvalidate_S	0x17

#define	HitWB_I			0x18
#define	HitWB_D			0x19
#define	HitWB_S			0x1b

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
 *	Size and configure the caches.
 *	NOTE: should only be called from mips_init().
 *
 * Side effects:
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
	.set	noreorder
	LA	v0, 1f
	LA	v1, KSEG1_BASE
	or	v0, v1
	jr	v0				# Switch to uncached.
	nop
1:
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

	LOAD_XKPHYS(t0, CCA_CACHED)
	li	ta2, 128 * 1024			# fixed L2 size...
	PTR_ADDU t1, t0, ta2
	sll	ta2, 2				# ...is 512KB
1:
	cache	IndexWBInvalidate_S, 0(t0)
	cache	IndexWBInvalidate_S, 1(t0)
	cache	IndexWBInvalidate_S, 2(t0)
	cache	IndexWBInvalidate_S, 3(t0)
	PTR_ADDU t0, t0, 32
	bne	t0, t1, 1b
	nop

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
	and	t1, ~(NBPG - 1)			# i and d cache sizes...
	sw	t1, CpuCacheAliasMask

	sw	ta0, CpuPrimaryInstCacheSize	# store cache size.
	sw	ta1, CpuPrimaryDataCacheSize	# store cache size.

	/* Loongson 2E/2F: cache way information is in the lowest bits */
	sw	zero, CpuPrimaryInstSetSize
	li	ta1, 1
	sw	ta1, CpuPrimaryDataSetSize

	and	v0, ~7
	or	v0, CCA_CACHED			# set cachable writeback kseg0
	mtc0	v0, COP_0_CONFIG		# establish any new config
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
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of ALL caches are Invalidated or Synched.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_SyncCache, 0)
	.set	noreorder
	lw	t1, CpuPrimaryInstCacheSize
	lw	t2, CpuPrimaryDataCacheSize

/*
 * Sync the instruction cache.
 */
	LOAD_XKPHYS(t0, CCA_CACHED)
	PTR_ADDU t1, t0, t1			# Compute end address
	PTR_SUBU t1, 128

1:
	cache	IndexInvalidate_I, 0(t0)
	cache	IndexInvalidate_I, 32(t0)
	cache	IndexInvalidate_I, 64(t0)
	cache	IndexInvalidate_I, 96(t0)

	bne	t0, t1, 1b
	PTR_ADDU t0, t0, 128

/*
 * Sync the data cache. Do L1 first. Indexed only operate on
 * the selected cache and differs from Hit in that sense.
 */

	LOAD_XKPHYS(t0, CCA_CACHED)
	PTR_ADDU t1, t0, t2			# End address
	PTR_SUBU t1, t1, 128
1:
	cache	IndexWBInvalidate_D, 0(t0)
	cache	IndexWBInvalidate_D, 32(t0)
	cache	IndexWBInvalidate_D, 64(t0)
	cache	IndexWBInvalidate_D, 96(t0)

	bne	t0, t1, 1b
	PTR_ADDU t0, t0, 128

/* Do on chip L2 */
	LOAD_XKPHYS(t3, CCA_CACHED)
	lw	ta0, CpuSecondaryCacheSize
	sll	ta0, 2				# because cache is 4 way
10:
	cache	IndexWBInvalidate_S, 0(t3)
	cache	IndexWBInvalidate_S, 1(t3)
	cache	IndexWBInvalidate_S, 2(t3)
	cache	IndexWBInvalidate_S, 3(t3)
	PTR_SUBU ta0, 32			# Fixed cache line size.
	bgtz	ta0, 10b
	PTR_ADDU t3, 32

	j	ra
	nop
END(Loongson2_SyncCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_InvalidateICache --
 *
 *	void Loongson2_SyncICache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Invalidate the L1 instruction cache for at least range
 *	of addr to addr + len - 1.
 *	The address is reduced to a XKPHYS index to avoid TLB faults.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the L1 Instruction cache is flushed.
 *	Must not touch v0.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_InvalidateICache, 0)
	/* Loongson2: I$ index ops affect all ways */
	and	a0, 0x00ffffff			# Reduce addr to cache index
	LOAD_XKPHYS(a2, CCA_CACHED)
	PTR_ADDU a1, 31				# Round up size
	PTR_ADDU a1, a0				# Add extra from address
	and	a0, -32				# Align start address
	PTR_SUBU a1, a1, a0
	PTR_ADDU a0, a2				# a0 now new XKPHYS address
	srl	a1, a1, 5			# Number of unrolled loops
1:
	PTR_ADDU a1, -1
	cache	IndexInvalidate_I, 0(a0)
	bne	a1, zero, 1b
	PTR_ADDU a0, 32

	j	ra
	move	v0, zero
END(Loongson2_InvalidateICache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_SyncDCachePage --
 *
 *	void Loongson2_SyncDCachePage(addr)
 *		vaddr_t addr;
 *
 *	Sync the L1 data cache page for address addr.
 *	The address is reduced to a XKPHYS index to avoid TLB faults.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_SyncDCachePage, 0)
	LOAD_XKPHYS(a2, CCA_CACHED)
	dsll	a0, 34
	dsrl	a0, 34
	PTR_ADDU a0, a2				# a0 now new XKPHYS address
	and	a0, ~PAGE_MASK			# Page align start address
	PTR_ADDU a1, a0, PAGE_SIZE-128

1:
	cache	IndexWBInvalidate_D, 0(a0)	# do set A
	cache	IndexWBInvalidate_D, 32(a0)
	cache	IndexWBInvalidate_D, 64(a0)
	cache	IndexWBInvalidate_D, 96(a0)

	cache	IndexWBInvalidate_D, 1(a0)	# do set B
	cache	IndexWBInvalidate_D, 33(a0)
	cache	IndexWBInvalidate_D, 65(a0)
	cache	IndexWBInvalidate_D, 97(a0)

	cache	IndexWBInvalidate_D, 2(a0)	# do set C
	cache	IndexWBInvalidate_D, 34(a0)
	cache	IndexWBInvalidate_D, 66(a0)
	cache	IndexWBInvalidate_D, 98(a0)

	cache	IndexWBInvalidate_D, 3(a0)	# do set D
	cache	IndexWBInvalidate_D, 35(a0)
	cache	IndexWBInvalidate_D, 67(a0)
	cache	IndexWBInvalidate_D, 99(a0)

	bne	a1, a0, 1b
	PTR_ADDU a0, 128

	j	ra
	nop
END(Loongson2_SyncDCachePage)

/*----------------------------------------------------------------------------
 *
 * Loongson2_HitSyncDCache --
 *
 *	void Loongson2_HitSyncDCache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Sync data cache for range of addr to addr + len - 1.
 *	The address can be any valid virtual address as long
 *	as no TLB invalid traps occur. Only lines with matching
 *	addr are flushed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the L1 cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 * IMPORTANT NOTE:
 *	Since orphaned L1 cache entries will not be synched it is
 *	mandatory to pass over the L1 cache once after the L2 is done.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitSyncDCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, 31				# Round up
	PTR_ADDU a1, a1, a0			# Add extra from address
	and	a0, a0, -32			# align address
	PTR_SUBU a1, a1, a0
	srl	a1, a1, 5			# Compute number of cache lines

1:
	PTR_ADDU a1, -1
	cache	HitWBInvalidate_D, 0(a0)
	bne	a1, zero, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Loongson2_HitSyncDCache)


/*----------------------------------------------------------------------------
 *
 * Loongson2_HitSyncSCache --
 *
 *	void Loongson2_HitSyncSCache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Sync secondary cache for range of addr to addr + len - 1.
 *	The address can be any valid virtual address as long
 *	as no TLB invalid traps occur. Only lines with matching
 *	addr are flushed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the L2 cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 * IMPORTANT NOTE:
 *	Since orphaned L1 cache entries will not be synched it is
 *	mandatory to pass over the L1 cache once after the L2 is done.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitSyncSCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, a1, a0			# Add in extra from align
	and	a0, a0, -32			# Align address
	PTR_SUBU a1, a1, a0
1:
	PTR_ADDU a1, -32

	cache	HitWBInvalidate_S, 0(a0)
	cache	HitWBInvalidate_D, 0(a0)	# Kill any orphans...

	bgtz	a1, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Loongson2_HitSyncSCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_HitInvalidateDCache --
 *
 *	void Loongson2_HitInvalidateDCache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Invalidate data cache for range of addr to addr + len - 1.
 *	The address can be any valid address as long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses or mapped addresses)
 *	Only lines with matching addresses are invalidated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The L1 cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitInvalidateDCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, a1, a0			# Add in extra from align
	and	a0, a0, -32			# Align address
	PTR_SUBU a1, a1, a0

1:
	PTR_ADDU a1, -32

	cache	HitInvalidate_D, 0(a0)

	bgtz	a1, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Loongson2_HitInvalidateDCache)


/*----------------------------------------------------------------------------
 *
 * Loongson2_HitInvalidateSCache --
 *
 *	void Loongson2_HitInvalidateSCache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Invalidate secondary cache for range of addr to addr + len - 1.
 *	The address can be any valid address as long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses or mapped addresses)
 *	Only lines with matching addresses are invalidated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The L2 cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Loongson2_HitInvalidateSCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, a1, a0			# Add in extra from align
	and	a0, a0, -32			# Align address
	PTR_SUBU a1, a1, a0
1:
	PTR_ADDU a1, -32

	cache	HitInvalidate_S, 0(a0)
	cache	HitInvalidate_D, 0(a0)		# Orphans in L1

	bgtz	a1, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Loongson2_HitInvalidateSCache)

/*----------------------------------------------------------------------------
 *
 * Loongson2_IOSyncDCache --
 *
 *	void Loongson2_IOSyncDCache(addr, len, rw)
 *		vaddr_t addr;
 *		int len, rw;
 *
 *	Invalidate or flush data cache for range of addr to addr + len - 1.
 *	The address can be any valid address as long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses or mapped addresses)
 *
 *	In case of the existence of an external cache we invalidate pages
 *	which are in the given range ONLY if transfer direction is READ.
 *	The assumption here is a 'write through' external cache which is
 *	true for all now supported processors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If rw == 0 (read), L1 and on-chip L2 caches are invalidated or
 *		flushed if the area does not match the alignment
 *		requirements.
 *	If rw == 1 (write) or rw == 2 (write-read), L1 and on-chip L2 caches
 *		are written back to memory and invalidated.
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(Loongson2_IOSyncDCache, FRAMESZ(CF_SZ+2*REGSZ), ra)

	PTR_SUBU sp, FRAMESZ(CF_SZ+2*REGSZ)
	PTR_S	ra, CF_RA_OFFS+2*REGSZ(sp)
	REG_S	a0, CF_ARGSZ(sp)		# save args
	beqz	a2, SyncRD			# Sync PREREAD
	REG_S	a1, CF_ARGSZ+REGSZ(sp)

/*
 *  Sync for unaligned read or write-read.
 */
SyncRDWB:
	jal	Loongson2_HitSyncSCache		# Do internal L2 cache
	nop					# L1 done in parallel
	b	SyncDone
	PTR_L	ra, CF_RA_OFFS+2*REGSZ(sp)

SyncRD:
	and	t0, a0, 31			# check if invalidate possible
	bnez	t0, SyncRDWB			# both address and size must
	and	t0, a1, 31			# be aligned at the cache size
	bnez	t0, SyncRDWB
	nop

/*
 *  Sync for aligned read, no writeback required.
 */
	jal	Loongson2_HitInvalidateSCache	# Internal L2 cache
	nop					# L1 done in parallel

	PTR_L	ra, CF_RA_OFFS+2*REGSZ(sp)

SyncDone:
	j	ra
	PTR_ADDU sp, FRAMESZ(CF_SZ+2*REGSZ)
END(Loongson2_IOSyncDCache)

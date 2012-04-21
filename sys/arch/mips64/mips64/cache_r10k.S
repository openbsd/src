/*	$OpenBSD: cache_r10k.S,v 1.16 2012/04/21 12:20:30 miod Exp $ */

/*
 * Copyright (c) 2004 Opsycon AB (www.opsycon.se)
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
 * R10000, R12000, R14000 and R16000.
 *
 * The following assumptions are made:
 * - L1 I$ is 2 way, 64 bytes/line
 * - L1 D$ is WB, 2 way, 32 bytes/line
 * - L2 is WT, 2 way
 */

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>

#include "assym.h"

	.set	mips3

/*
 *  Skip the .h file. Noone else need to know!
 */

#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexFlashInvalidate_T	0x02
#define	IndexWBInvalidate_S	0x03

#define	IndexLoadTag_I		0x04
#define	IndexLoadTag_D		0x05
#define	IndexLoadTag_S		0x07

#define	IndexStoreTag_I		0x08
#define	IndexStoreTag_D		0x09
#define	IndexStoreTag_S		0x0b

#define	CreateDirtyExclusive	0x09

#define	HitInvalidate_I		0x10
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

#define	CTYPE_HAS_XL2		0x0200	/* External L2 Cache present */

	.set	noreorder		# Noreorder is default style!

/*----------------------------------------------------------------------------
 *
 * Mips10k_ConfigCache(struct cpu_info *ci)
 *
 *	Size and configure the caches.
 *	NOTE: should only be called from mips_init().
 *
 * Side effects:
 *	The size of the data cache is stored into ci_l1datacachesize.
 *	The size of instruction cache is stored into ci_l1instcachesize.
 *	Alignment mask for cache aliasing test is stored in cache_valias_mask.
 *	ci_l2size is set to the size of the secondary cache.
 *	ci_l3size is set to the size of the tertiary cache.
 *	ci_cacheways is set to 0 for direct mapped caches, 2 for two way
 *	caches and 4 for four way caches. This primarily indicates the
 *	primary cache associativity.
 *
 * Allocation:
 *	ta0, ta1 ta2 used to hold I and D set size and Alias mask.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_ConfigCache, 0)
	.set	noreorder
	mfc0	v0, COP_0_CONFIG		# Get configuration register

	srl	t1, v0, 29			# Get I cache size.
	and	t1, 7
	li	t2, 4096
	sllv	ta0, t2, t1			# ta0 = Initial I set size.

	li	t2, 64
	sw	t2, CI_L1INSTCACHELINE(a0)

	srl	t1, v0, 26			# Get D cache size.
	and	t1, 7
	li	t2, 4096			# Fixed page size.
	sllv	ta1, t2, t1

	li	t2, 32				# Get D cache line size.
	sw	t2, CI_L1DATACACHELINE(a0)

	li	t2, CTYPE_2WAY			# Assume two way cache
	li	ta3, 0				# Tertiary size 0.

	or	t2, CTYPE_HAS_XL2		# External L2 present.
	srl	t1, v0, 16			# Get L2 cache size.
	and	t1, 7
	li	ta2, 512*1024			# 512k per 'click'.
	sll	ta2, t1

/*
 * Get here with t2 = Cache type, ta0 = L1 I size, ta1 = L1 D size.
 * ta2 = secondary size, ta3 = tertiary size.
 */
ConfResult:
	sw	t2, CI_CACHECONFIGURATION(a0)	# Save cache attributes
	and	t2, CTYPE_WAYMASK		# isolate number of sets.
	sw	t2, CI_CACHEWAYS(a0)
	srl	t2, 1				# get div shift for set size.

	sw	ta2, CI_L2SIZE(a0)
	sw	ta3, CI_L3SIZE(a0)

	addu	t1, ta0, -1			# Use icache for alias mask
	srl	t1, t2
	and	t1, ~(PAGE_SIZE - 1)
	beqz	t1, 1f
	nop
	or	t1, (PAGE_SIZE - 1)
1:
#ifdef MULTIPROCESSOR
	PTR_L	ta2, cache_valias_mask
	or	t1, ta2				# Pick largest mask
#endif
	PTR_S	t1, cache_valias_mask
	PTR_S	t1, pmap_prefer_mask

	sw	ta0, CI_L1INSTCACHESIZE(a0)	# store cache size.
	srl	ta0, t2				# calculate set size.
	sw	ta0, CI_L1INSTCACHESET(a0)

	sw	ta1, CI_L1DATACACHESIZE(a0)	# store cache size.
	srl	ta1, t2				# calculate set size.
	sw	ta1, CI_L1DATACACHESET(a0)

	j	ra
	nop
END(Mips10k_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_SyncCache(struct cpu_info *ci)
 *
 *	Sync ALL caches.
 *	No need to look at number of sets since we are cleaning out
 *	the entire cache and thus will address all sets anyway.
 *
 * Side effects:
 *	The contents of ALL caches are Invalidated or Synched.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_SyncCache, 0)
	.set	noreorder
	lw	t1, CI_L1INSTCACHESET(a0)
	lw	t2, CI_L1DATACACHESET(a0)

/*
 * Sync the instruction cache.
 */

	LOAD_XKPHYS(t0, CCA_CACHED)
	PTR_ADDU t1, t0, t1			# Compute end address
	PTR_SUBU t1, 128

1:
	cache	IndexInvalidate_I, 0(t0)
	cache	IndexInvalidate_I, 64(t0)

	cache	IndexInvalidate_I, 1(t0)
	cache	IndexInvalidate_I, 65(t0)

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

	cache	IndexWBInvalidate_D, 1(t0)
	cache	IndexWBInvalidate_D, 33(t0)
	cache	IndexWBInvalidate_D, 65(t0)
	cache	IndexWBInvalidate_D, 97(t0)

	bne	t0, t1, 1b
	PTR_ADDU t0, t0, 128

/* Do L2 */
	LOAD_XKPHYS(t3, CCA_CACHED)
	lw	ta0, CI_L2SIZE(a0)
	PTR_SRL	ta0, ta0, 1			# proper set size
10:
	cache	IndexWBInvalidate_S, 0(t3)
	cache	IndexWBInvalidate_S, 1(t3)
	PTR_SUBU ta0, 64			# Fixed L2 cache line size.
	bgtz	ta0, 10b
	PTR_ADDU t3, 64

	j	ra
	nop
END(Mips10k_SyncCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_InvalidateICache(struct cpu_info *ci, vaddr_t va, size_t len)
 *
 *	Invalidate the L1 instruction cache for at least range of va to
 *	va + len - 1.
 *
 * Side effects:
 *	The contents of the L1 Instruction cache are invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_InvalidateICache, 0)
	LOAD_XKPHYS(a3, CCA_CACHED)
	and	a1, 0x00ffffff			# Reduce addr to cache index
	PTR_ADDU a2, 63				# Round up size
	PTR_ADDU a2, a1				# Add extra from address
	and	a1, -64				# Align start address
	PTR_SUBU a2, a2, a1
	PTR_ADDU a1, a3				# a1 now new XKPHYS address
	srl	a2, a2, 6			# Number of unrolled loops
1:
	addu	a2, -1

	cache	IndexInvalidate_I, 0(a1)	# do set A
	cache	IndexInvalidate_I, 1(a1)	# do set B

	bne	a2, zero, 1b
	PTR_ADDU a1, 64

	j	ra
	nop
END(Mips10k_InvalidateICache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
 *
 *	Sync the L1 data cache page for address va.
 *	The physical address is not used.
 *
 * Side effects:
 *	The contents of the cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_SyncDCachePage, 0)
	LOAD_XKPHYS(a3, CCA_CACHED)
	dsll	a1, (64 - 57)
	dsrl	a1, (64 - 57)
	PTR_ADDU a1, a3				# a1 now new XKPHYS address
	and	a1, ~PAGE_MASK			# Page align start address
	PTR_ADDU a2, a1, PAGE_SIZE-128

1:
	cache	IndexWBInvalidate_D, 0(a1)	# do set A
	cache	IndexWBInvalidate_D, 32(a1)
	cache	IndexWBInvalidate_D, 64(a1)
	cache	IndexWBInvalidate_D, 96(a1)

	cache	IndexWBInvalidate_D, 1(a1)	# do set B
	cache	IndexWBInvalidate_D, 33(a1)
	cache	IndexWBInvalidate_D, 65(a1)
	cache	IndexWBInvalidate_D, 97(a1)

	bne	a2, a1, 1b
	PTR_ADDU a1, 128

	j	ra
	nop
END(Mips10k_SyncDCachePage)

/*----------------------------------------------------------------------------
 *
 * Mips10k_HitSyncDCache(struct cpu_info *ci, vaddr_t va, size_t len)
 *
 *	Sync data cache for range of va to va + len - 1.
 *	Only lines with matching address are flushed.
 *
 * Side effects:
 *	The contents of the L1 cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_HitSyncDCache, 0)
	beq	a2, zero, 3f			# size is zero!
	PTR_ADDU a2, 31				# Round up
	PTR_ADDU a2, a2, a1			# Add extra from address
	and	a1, a1, -32			# align address
	PTR_SUBU a2, a2, a1
	srl	a2, a2, 5			# Compute number of cache lines

1:
	PTR_ADDU a2, -1
	cache	HitWBInvalidate_D, 0(a1)
	bne	a2, zero, 1b
	PTR_ADDU a1, 32

3:
	j	ra
	nop
END(Mips10k_HitSyncDCache)

/*----------------------------------------------------------------------------
 *
 * _mips10k_HitSyncSCache(struct cpu_info *ci, vaddr_t va, size_t len)
 *
 *	Sync secondary cache for range of va to va + len - 1.
 *	Only lines with matching address are flushed.
 *
 * Side effects:
 *	The contents of the L2 cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
ALEAF(_mips10k_HitSyncSCache)
	beq	a2, zero, 3f			# size is zero!
	PTR_ADDU a2, a2, a1			# Add in extra from align
	and	a1, a1, -32			# Align address
	PTR_SUBU a2, a2, a1
1:
	PTR_ADDU a2, -32

	cache	HitWBInvalidate_S, 0(a1)

	bgtz	a2, 1b
	PTR_ADDU a1, 32

3:
	j	ra
	nop

/*----------------------------------------------------------------------------
 *
 * Mips10k_HitInvalidateDCache(struct cpu_info *ci, vaddr_t va, size_t len)
 *
 *	Invalidate data cache for range of va to va + len - 1.
 *	Only lines with matching addresses are invalidated.
 *
 * Side effects:
 *	The L1 cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_HitInvalidateDCache, 0)
	beq	a2, zero, 3f			# size is zero!
	PTR_ADDU a2, a2, a1			# Add in extra from align
	and	a1, a1, -32			# Align address
	PTR_SUBU a2, a2, a1

1:
	PTR_ADDU a2, -32

	cache	HitInvalidate_D, 0(a1)

	bgtz	a2, 1b
	PTR_ADDU a1, 32

3:
	j	ra
	nop
END(Mips10k_HitInvalidateDCache)

/*----------------------------------------------------------------------------
 *
 * _mips10k_HitInvalidateSCache(struct cpu_info *ci, vaddr_t va, size_t len)
 *
 *	Invalidate secondary cache for range of va to va + len - 1.
 *	Only lines with matching addresses are invalidated.
 *
 * Side effects:
 *	The L2 cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
ALEAF(_mips10k_HitInvalidateSCache)
	beq	a2, zero, 3f			# size is zero!
	PTR_ADDU a2, a2, a1			# Add in extra from align
	and	a1, a1, -32			# Align address
	PTR_SUBU a2, a2, a1
1:
	PTR_ADDU a2, -32

	cache	HitInvalidate_S, 0(a1)

	bgtz	a2, 1b
	PTR_ADDU a1, 32

3:
	j	ra
	nop

/*----------------------------------------------------------------------------
 *
 * Mips10k_IOSyncDCache(struct cpu_info *ci, vaddr_t va, size_t len, int how)
 *
 *	Invalidate or flush data cache for range of va to va + len - 1.
 *
 * Side effects:
 *	If how == 0 (read), L1 and L2 caches are invalidated or flushed if
 *		the area does not match the alignment requirements. 
 *	If how == 1 (write), L1 and L2 caches are written back
 *		to memory and invalidated.
 *	If how == 2 (write-read), L1 and L2 caches are written back
 *		to memory and invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_IOSyncDCache, 0)
	beqz	a3, SyncRD			# Sync PREREAD
	nop
	addiu	a3, -1
	bnez	a3, SyncRDWB			# Sync PREWRITE+PREREAD
	nop

SyncWR:
	j	_mips10k_HitSyncSCache		# Do L2 cache
	nop					# L1 done in parallel

SyncRD:
	and	t0, a1, 63			# check if invalidate possible
	bnez	t0, SyncRDWB			# both address and size must
	and	t0, a2, 63			# be aligned at the cache size
	bnez	t0, SyncRDWB
	nop

	/*
	 *  Sync for aligned read, no writeback required.
	 */
	j	_mips10k_HitInvalidateSCache	# L2 cache
	nop					# L1 done in parallel

SyncRDWB:
	/*
	 *  Sync for unaligned read or write-read.
	 */
	j	_mips10k_HitSyncSCache		# L2 cache
	nop					# L1 done in parallel
END(Mips10k_IOSyncDCache)

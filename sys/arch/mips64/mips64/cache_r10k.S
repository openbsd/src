/*	$OpenBSD: cache_r10k.S,v 1.1 2004/09/20 10:28:36 pefo Exp $ */

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
 *  Processors supported:
 *  R10000
 *  R12000
 *  R14000	(needs to be tested)
 */

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/pte.h>

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

#define	CTYPE_HAS_IL2		0x0100	/* Internal L2 Cache present */
#define	CTYPE_HAS_XL2		0x0200	/* External L2 Cache present */
#define	CTYPE_HAS_XL3		0x0400	/* External L3 Cache present */

	.set	noreorder		# Noreorder is default style!

/*----------------------------------------------------------------------------
 *
 * Mips10k_ConfigCache --
 *
 *	Size and configure the caches.
 *	NOTE: should only be called from mips_init().
 *
 * Results:
 *	Returns the value of the cpu configuration register.
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
LEAF(Mips10k_ConfigCache, 0)
	.set	noreorder
	mfc0	v0, COP_0_CONFIG		# Get configuration register

	srl	t1, v0, 29			# Get I cache size.
	and	t1, 7
	li	t2, 4096
	sllv	ta0, t2, t1			# ta0 = Initial I set size.

	li	t2, 64
	sw	t2, CpuPrimaryInstCacheLSize

	srl	t1, v0, 26			# Get D cache size.
	and	t1, 7
	li	t2, 4096			# Fixed page size.
	sllv	ta1, t2, t1

	li	t2, 32				# Get D cache line size.
	sw	t2, CpuPrimaryDataCacheLSize

	li	t2, CTYPE_2WAY			# Assume two way cache
	li	ta2, 0				# Secondary size 0.
	li	ta3, 0				# Tertiary size 0.

	or	t2, CTYPE_HAS_XL2		# External L2 present.
	srl	t1, v0, 16			# Get I cache size.
	and	t1, 7
	li	ta2, 512*1024			# 512k per 'click'.
	sll	ta2, t1

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
	srl	t1, t2
	and	t1, ~(NBPG - 1)
	sw	t1, CpuCacheAliasMask

	sw	ta0, CpuPrimaryInstCacheSize	# store cache size.
	srl	ta0, t2				# calculate set size.
	sw	ta0, CpuPrimaryInstSetSize

	sw	ta1, CpuPrimaryDataCacheSize	# store cache size.
	srl	ta1, t2				# calculate set size.
	sw	ta1, CpuPrimaryDataSetSize

#if 0
	and	v0, 0xfffffff8
	or	v0, 0x00000003			# set cachable writeback kseg0
	mtc0	v0, COP_0_CONFIG		# establish any new config
#endif
	j	ra
	nop
END(Mips10k_ConfigCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_SyncCache --
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
LEAF(Mips10k_SyncCache, 0)
	.set	noreorder
	lw	t1, CpuPrimaryInstSetSize
	lw	t2, CpuPrimaryDataSetSize

/*
 * Sync the instruction cache.
 */

	LA	t0, KSEG0_BASE
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

	LA	t0, KSEG0_BASE
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
	LA	t3, KSEG0_BASE
	lw	ta0, CpuSecondaryCacheSize	# XXX Need set size here.
10:
	cache	IndexWBInvalidate_S, 0(t3)
	cache	IndexWBInvalidate_S, 1(t3)
	PTR_SUBU ta0, 32			# Fixed cache line size.
	bgtz	ta0, 10b
	PTR_ADDU t3, 32

	j	ra
	nop
END(Mips10k_SyncCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_InvalidateICachePage --
 *
 *	void Mips10k_InvalidateICachePage(addr)
 *		vaddr_t addr;
 *
 *	Invalidate the L1 instruction cache page given by addr.
 *
 * Results:
 *	Void.
 *
 * Side effects:
 *	The contents of the L1 Instruction cache is flushed.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_InvalidateICachePage, 0)
	lw	v0, CpuNWayCache		# Cache properties
	and	a0, ~PAGE_MASK			# Page align start address
	PTR_ADDU a1, a0, PAGE_SIZE-128		# End address.
	addiu	v0, -2				# <0 1way, 0 = two, >0 four
1:
	cache	HitInvalidate_I, 0(a0)
	cache	HitInvalidate_I, 64(a0)

	bne	a0, a1, 1b
	PTR_ADDU a0, 128

	j	ra
	move	v0, zero
END(Mips10k_InvalidateICachePage)

/*----------------------------------------------------------------------------
 *
 * Mips10k_InvalidateICache --
 *
 *	void Mips10k_SyncICache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Invalidate the L1 instruction cache for at least range
 *	of addr to addr + len - 1.
 *	The address is reduced to a KSEG0 index to avoid TLB faults.
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
LEAF(Mips10k_InvalidateICache, 0)
	and	a0, 0x00ffffff			# Reduce addr to cache index
	PTR_ADDU a1, 63				# Round up size
	PTR_ADDU a1, a0				# Add extra from address
	and	a0, -64				# Align start address
	PTR_SUBU a1, a1, a0
	PTR_ADDU a0, KSEG0_BASE			# a0 now new KSEG0 address
	srl	a1, a1, 6			# Number of unrolled loops
1:
	addu	a1, -1

	cache	IndexInvalidate_I, 0(a0)	# do set A
	cache	IndexInvalidate_I, 1(a0)	# do set B

	bne	a1, zero, 1b
	PTR_ADDU a0, 64

	j	ra
	move	v0, zero
END(Mips10k_InvalidateICache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_SyncDCachePage --
 *
 *	void Mips10k_SyncDCachePage(addr)
 *		vaddr_t addr;
 *
 *	Sync the L1 data cache page for address addr.
 *	The address is reduced to a KSEG0 index to avoid TLB faults.
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
LEAF(Mips10k_SyncDCachePage, 0)
	dsll	a0, 34
	dsrl	a0, 34
	PTR_ADDU a0, KSEG0_BASE			# a0 now new KSEG0 address
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

	bne	a1, a0, 1b
	PTR_ADDU a0, 128

	j	ra
	nop
END(Mips10k_SyncDCachePage)

/*----------------------------------------------------------------------------
 *
 * Mips10k_HitSyncDCache --
 *
 *	void Mips10k_HitSyncDCache(addr, len)
 *		vaddr_t addr, len;
 *
 *	Sync data cache for range of addr to addr + len - 1.
 *	The address can be any valid virtual address as long
 *	as no TLB invalid traps occur. Only lines with matching
 *	addr are flushed.
 *
 *	Note: Use the CpuNWayCache flag to select 16 or 32 byte linesize.
 *	      All Nway cpu's now available have a fixed 32byte linesize.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the L1 cache is written back to primary memory.
 *	The cache line is invalidated.
 *
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_HitSyncDCache, 0)
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
END(Mips10k_HitSyncDCache)


/*----------------------------------------------------------------------------
 *
 * Mips10k_HitSyncSCache --
 *
 *	void Mips10k_HitSyncSCache(addr, len)
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
 *----------------------------------------------------------------------------
 */
LEAF(Mips10k_HitSyncSCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, a1, a0			# Add in extra from align
	and	a0, a0, -32			# Align address
	PTR_SUBU a1, a1, a0
1:
	PTR_ADDU a1, -32

	cache	HitWBInvalidate_S, 0(a0)

	bgtz	a1, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Mips10k_HitSyncSCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_HitInvalidateDCache --
 *
 *	void Mips10k_HitInvalidateDCache(addr, len)
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
LEAF(Mips10k_HitInvalidateDCache, 0)
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
END(Mips10k_HitInvalidateDCache)


/*----------------------------------------------------------------------------
 *
 * Mips10k_HitInvalidateSCache --
 *
 *	void Mips10k_HitInvalidateSCache(addr, len)
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
LEAF(Mips10k_HitInvalidateSCache, 0)
	beq	a1, zero, 3f			# size is zero!
	PTR_ADDU a1, a1, a0			# Add in extra from align
	and	a0, a0, -32			# Align address
	PTR_SUBU a1, a1, a0
1:
	PTR_ADDU a1, -32

	cache	HitInvalidate_S, 0(a0)

	bgtz	a1, 1b
	PTR_ADDU a0, 32

3:
	j	ra
	nop
END(Mips10k_HitInvalidateSCache)

/*----------------------------------------------------------------------------
 *
 * Mips10k_IOSyncDCache --
 *
 *	void Mips10k_IOSyncDCache(addr, len, rw)
 *		vaddr_t addr;
 *		int  len, rw;
 *
 *	Invalidate or flush data cache for range of addr to addr + len - 1.
 *	The address can be any valid address as long as no TLB misses occur.
 *	(Be sure to use cached K0SEG kernel addresses or mapped addresses)
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If rw == 0 (read), L1 and L2 caches are invalidated or
 *		flushed if the area does not match the alignment
 *		requirements. 
 *	If rw == 1 (write), L1 and L2 caches are written back
 *		to memory and invalidated.
 *	If rw == 2 (write-read), L1 and L2 caches are written back
 *		to memory and invalidated.
 *
 *----------------------------------------------------------------------------
 */
NON_LEAF(Mips10k_IOSyncDCache, FRAMESZ(CF_SZ+2*REGSZ), ra)

	PTR_SUBU sp, FRAMESZ(CF_SZ+2*REGSZ)
	PTR_S	ra, CF_RA_OFFS+2*REGSZ(sp)
	REG_S	a0, CF_ARGSZ(sp)		# save args
	beqz	a2, SyncRD			# Sync PREREAD
	REG_S	a1, CF_ARGSZ+REGSZ(sp)
	addiu	a2, 1
	bnez	a2, SyncRDWB			# Sync PREWRITE+PREREAD
	nop

SyncWR:
	jal	Mips10k_HitSyncSCache		# Do L2 cache
	nop					# L1 done in parallel
	b	SyncDone
	PTR_L	ra, CF_RA_OFFS+2*REGSZ(sp)

SyncRD:
	and	t0, a0, 63			# check if invalidate possible
	bnez	t0, SyncRDWB			# both address and size must
	and	t0, a1, 63			# be aligned at the cache size
	bnez	t0, SyncRDWB
	nop

/*
 *  Sync for aligned read, no writeback requierd.
 */
	jal	Mips10k_HitInvalidateSCache	# L2 cache
	nop					# L1 done in parallel

	b	SyncDone
	PTR_L	ra, CF_RA_OFFS+2*REGSZ(sp)

/*
 *  Sync for unaligned read or write-read.
 */
SyncRDWB:
	jal	Mips10k_HitSyncSCache		# L2 cache
	nop					# L1 done in parallel

	PTR_L	ra, CF_RA_OFFS+2*REGSZ(sp)

SyncDone:
	j	ra
	PTR_ADDU sp, FRAMESZ(CF_SZ+2*REGSZ)
END(Mips10k_IOSyncDCache)

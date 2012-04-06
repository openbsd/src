/*	$OpenBSD: cache.h,v 1.2 2012/04/06 20:11:18 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#ifndef	_MIPS64_CACHE_H_
#define	_MIPS64_CACHE_H_

/*
 * Declare canonical cache functions for a given processor.
 *
 * Note that the uint64_t arguments are addresses, which can be either
 * virtual or physical addresses, depending upon the particular processor
 * model.  The high-level functions, invoked from pmap, pass both virtual
 * and physical addresses to the Mips_* cache macros declared in
 * <machine/cpu.h>.  It is the responsibility of a given port, when
 * implementing these macros, to pass either the virtual or the physical
 * address to the final cache routines.
 *
 * Note that there are no ports where the supported processors use a mix
 * of virtual and physical addresses.
 */

#define CACHE_PROTOS(chip) \
/* Figure out cache configuration */ \
void	chip##_ConfigCache(struct cpu_info *); \
/* Writeback and invalidate all caches */ \
void  	chip##_SyncCache(struct cpu_info *); \
/* Invalidate all I$ for the given range */ \
void	chip##_InvalidateICache(struct cpu_info *, uint64_t, size_t); \
/* Writeback all D$ for the given page */ \
void	chip##_SyncDCachePage(struct cpu_info *, uint64_t); \
/* Writeback all D$ for the given range */ \
void	chip##_HitSyncDCache(struct cpu_info *, uint64_t, size_t); \
/* Invalidate all D$ for the given range */ \
void	chip##_HitInvalidateDCache(struct cpu_info *, uint64_t, size_t); \
/* Enforce coherency of the given range */ \
void	chip##_IOSyncDCache(struct cpu_info *, uint64_t, size_t, int);

/*
 * Cavium Octeon.
 * ICache routines take virtual addresses.
 * DCache routines take physical addresses.
 */
CACHE_PROTOS(Octeon);

/*
 * STC Loongson 2e and 2f.
 * ICache routines take virtual addresses.
 * DCache routines take physical addresses.
 */
CACHE_PROTOS(Loongson2);
 
/*
 * MIPS R4000 and R4400.
 * ICache routines take virtual addresses.
 * DCache routines take virtual addresses.
 */
CACHE_PROTOS(Mips4k);

/*
 * IDT/QED/PMC-Sierra R5000, RM52xx, RM7xxx, RM9xxx
 * ICache routines take virtual addresses.
 * DCache routines take virtual addresses.
 */
CACHE_PROTOS(Mips5k);

/*
 * MIPS/NEC R10000/R120000/R140000/R16000
 * ICache routines take virtual addresses.
 * DCache routines take virtual addresses.
 */
CACHE_PROTOS(Mips10k);

/*
 * Values used by the IOSyncDCache routine [which acts as the backend of
 * bus_dmamap_sync()].
 */
#define	CACHE_SYNC_R	0	/* WB invalidate, WT invalidate */
#define	CACHE_SYNC_W	1	/* WB writeback + invalidate, WT unaffected */
#define	CACHE_SYNC_X	2	/* WB writeback + invalidate, WT invalidate */

extern vaddr_t cache_valias_mask;

#endif	/* _MIPS64_CACHE_H_ */

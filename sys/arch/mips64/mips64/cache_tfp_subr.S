/*	$OpenBSD: cache_tfp_subr.S,v 1.3 2015/09/23 19:04:31 miod Exp $	*/

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

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>

#include "assym.h"

	.set	mips4
	.set	noreorder

/*
 * The R8000 processor does not have documented cache maintainance
 * instructions. While this is not really a problem for D$, which is
 * write-through and physically tagged, this is a problem for I$,
 * which is virtually tagged.
 *
 * Since there is no way to invalidate I$, the best we can do is
 * override existing cache lines to evict them from the cache.
 *
 * The following routine will attempt to do so. It spans the whole I$
 * cache size, and may be invoked from any offset multiple of 32 bytes
 * (in case a smaller portion of the cache needs to be invalidated).
 *
 * void tfp_inval_icache(register_t size)
 */
	.align	14
LEAF(tfp_inval_icache, 0)						/* { */

#define	ICACHE_CHUNK_INVALIDATE \
	subu	a0, 32; \
	NOP; \
	NOP; \
	NOP; \
	blez	a0, 9f; \
	 NOP; \
	NOP; \
	NOP

#define	ICACHE_256B_INVALIDATE \
	ICACHE_CHUNK_INVALIDATE; ICACHE_CHUNK_INVALIDATE; \
	ICACHE_CHUNK_INVALIDATE; ICACHE_CHUNK_INVALIDATE; \
	ICACHE_CHUNK_INVALIDATE; ICACHE_CHUNK_INVALIDATE; \
	ICACHE_CHUNK_INVALIDATE; ICACHE_CHUNK_INVALIDATE;

#define	ICACHE_2KB_INVALIDATE \
	ICACHE_256B_INVALIDATE; ICACHE_256B_INVALIDATE; \
	ICACHE_256B_INVALIDATE; ICACHE_256B_INVALIDATE; \
	ICACHE_256B_INVALIDATE; ICACHE_256B_INVALIDATE; \
	ICACHE_256B_INVALIDATE; ICACHE_256B_INVALIDATE

#define	ICACHE_16KB_INVALIDATE \
	ICACHE_2KB_INVALIDATE; ICACHE_2KB_INVALIDATE; \
	ICACHE_2KB_INVALIDATE; ICACHE_2KB_INVALIDATE; \
	ICACHE_2KB_INVALIDATE; ICACHE_2KB_INVALIDATE; \
	ICACHE_2KB_INVALIDATE; ICACHE_2KB_INVALIDATE
	
	ICACHE_16KB_INVALIDATE

	b	tfp_inval_icache	/* wrap around */
	 NOP
9:
	j	ra
	 NOP
END(tfp_inval_icache)							/* } */

/*
 * Clear the D$ cache tag for the given address.
 * Interrupts must be disabled (to be free to alter the Vaddr register).
 */

#define	DCTW	.align 4; .word 0x4300000a; SSNOP; SSNOP; SSNOP

LEAF(tfp_dctw_zero, 0)							/* { */
	DMTC0	a0, COP_0_VADDR
	MTC0_HAZARD
	DMTC0	zero, COP_0_DCACHE
	MTC0_HAZARD
	DCTW
	j	ra
	 NOP
END(tfp_dctw_zero)							/* } */

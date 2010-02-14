/*	$OpenBSD: pmon32.S,v 1.3 2010/02/14 22:39:33 miod Exp $	*/

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
 * Wrapper routines to invoke PMON2000 functions from 64-bit code.
 *
 * Since the PMON code is compiled as 32 bit code, the upper part of the
 * registers may be destroyed.
 *
 * As a result, we need to explicitely save all caller-saved registers,
 * prior to invoking any PMON function.
 *
 * Moreover, O32 calling convention only allows up to four arguments to
 * be passed through registers.  It's up to the caller to never invoke
 * pmon_printf() with more than four arguments; other functions are not
 * affected.
 *
 * 64-bit (off_t) return values are preserved (by merging v1 and v0) on a
 * case-by-case basis.
 */

#include <machine/param.h>
#include <machine/asm.h>

#ifndef _STANDALONE
#include "assym.h"
#endif

	.set	mips3

	.data
	.globl	pmon_callvec
pmon_callvec:
	.word	0

	.text

/*
 * Note that we need to provide a 32-bit CF_SZ untouched area above sp, or
 * we'll get our stack corrupted upon return.
 */

#define	PMON_WRAP(name) \
	NNON_LEAF(name, FRAMESZ(CF_SZ + 9 * REGSZ),  ra); \
	PTR_SUBU sp, sp, FRAMESZ(CF_SZ + 9 * REGSZ); \
	REG_S	ra, CF_RA_OFFS(sp); \
	.mask	0xc0ff0000, (CF_RA_OFFS - FRAMESZ(CF_SZ + 9 * REGSZ)); \
	REG_S	s0, (0 * REGSZ + CF_SZ)(sp); \
	REG_S	s1, (1 * REGSZ + CF_SZ)(sp); \
	REG_S	s2, (2 * REGSZ + CF_SZ)(sp); \
	REG_S	s3, (3 * REGSZ + CF_SZ)(sp); \
	REG_S	s4, (4 * REGSZ + CF_SZ)(sp); \
	REG_S	s5, (5 * REGSZ + CF_SZ)(sp); \
	REG_S	s6, (6 * REGSZ + CF_SZ)(sp); \
	REG_S	s7, (7 * REGSZ + CF_SZ)(sp); \
	REG_S	s8, (8 * REGSZ + CF_SZ)(sp)

#define	PMON_END(name) \
	REG_L	s8, (8 * REGSZ + CF_SZ)(sp); \
	REG_L	s7, (7 * REGSZ + CF_SZ)(sp); \
	REG_L	s6, (6 * REGSZ + CF_SZ)(sp); \
	REG_L	s5, (5 * REGSZ + CF_SZ)(sp); \
	REG_L	s4, (4 * REGSZ + CF_SZ)(sp); \
	REG_L	s3, (3 * REGSZ + CF_SZ)(sp); \
	REG_L	s2, (2 * REGSZ + CF_SZ)(sp); \
	REG_L	s1, (1 * REGSZ + CF_SZ)(sp); \
	REG_L	s0, (0 * REGSZ + CF_SZ)(sp); \
	REG_L	ra, CF_RA_OFFS(sp); \
	PTR_ADDU sp, sp, FRAMESZ(CF_SZ + 9 * REGSZ); \
	jr	ra; \
	nop; \
	END(name)

PMON_WRAP(pmon_printf)
	lw	t0, pmon_callvec
	lw	t0, 5 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_printf)

PMON_WRAP(pmon_gets)
	lw	t0, pmon_callvec
	lw	t0, 7 * 4 (t0)
	jalr	t0
	nop
	/* make sure the return value is sign extended */
	dsll32	v0, 0
	dsra32	v0, 0
PMON_END(pmon_gets)

#ifdef _STANDALONE

PMON_WRAP(pmon_cacheflush)
	lw	t0, pmon_callvec
	lw	t0, 6 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_cacheflush)

PMON_WRAP(pmon_open)
	lw	t0, pmon_callvec
	lw	t0, 0 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_open)

PMON_WRAP(pmon_close)
	lw	t0, pmon_callvec
	lw	t0, 1 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_close)

PMON_WRAP(pmon_read)
	lw	t0, pmon_callvec
	lw	t0, 2 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_read)

PMON_WRAP(pmon_lseek)
	lw	t0, pmon_callvec
	lw	t0, 4 * 4 (t0)
#if 0
	/*
	 * Setup arguments the way 32-bit PMON expects them.
	 * 64-bit mode: a0 = fd, a1 = off, a2 = whence
	 * 32-bit mode: a0 = fd, a2:a3 = off, 16(sp) = whence
	 */
	sw	a2, 16(sp)
#ifdef __MIPSEB__
	dsll32	a3, a1, 0
	dsrl32	a2, a1, 0	/* a2 = hi */
	dsrl32	a3, a3, 0	/* a3 = low */
#else
	dsll32	a2, a1, 0
	dsrl32	a3, a1, 0	/* a3 = hi */
	dsrl32	a2, a2, 0	/* a2 = low */
#endif
#endif
	jalr	t0
	nop
#if 0
	/* rebuild the 64 bit return value */
	dsll32	v0, 0
	dsll32	v1, 0
#ifdef __MIPSEB__
	/* v1 = lo << 32, v0 = hi << 32 */
	dsrl32	v1, 0
#else
	/* v1 = hi << 32, v0 = lo << 32 */
	dsrl32	v0, 0
#endif
	or	v0, v1, v0
#endif
PMON_END(pmon_lseek)

#endif

/*
 * The following routines are untested.
 */

#if 0

PMON_WRAP(pmon_write)
	lw	t0, pmon_callvec
	lw	t0, 3 * 4 (t0)
	jalr	t0
	nop
PMON_END(pmon_write)

#endif

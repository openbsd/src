/*	$OpenBSD: pmon32.S,v 1.4 2010/02/18 18:53:33 miod Exp $	*/

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
 * PMON is compiled as 64 bit code, using the gcc o64 ABI (similar to the o32
 * ABI, but using 64 bit registers).
 *
 * As a result, only up to four arguments to functions will be passed through
 * registers.  It's up to the caller to never invoke pmon_printf() with more
 * than four arguments; other functions are not affected.
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
 * Note that we need to provide a CF_SZ untouched area above sp, or we'll risk
 * our stack being corrupted upon return.
 */
#define	PMON_WRAP(name, index) \
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
	REG_S	s8, (8 * REGSZ + CF_SZ)(sp); \
	lw	t0, pmon_callvec; \
	lw	t0, (index) * 4 (t0); \
	jalr	t0; \
	nop; \
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

PMON_WRAP(pmon_printf, 5)
PMON_WRAP(pmon_gets, 7)
#ifdef _STANDALONE
PMON_WRAP(pmon_open, 0)
PMON_WRAP(pmon_close, 1)
PMON_WRAP(pmon_read, 2)
PMON_WRAP(pmon_lseek, 4)
PMON_WRAP(pmon_cacheflush, 6)
#endif
#if 0	/* unused */
PMON_WRAP(pmon_write, 3)
#endif

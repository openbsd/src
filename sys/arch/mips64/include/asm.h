/*	$OpenBSD: asm.h,v 1.3 2004/08/10 21:10:56 pefo Exp $ */

/*
 * Copyright (c) 2001-2002 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
#ifndef _MIPS64_ASM_H
#define _MIPS64_ASM_H

#include <machine/regdef.h>

#define	_MIPS_ISA_MIPS1	1	/* R2000/R3000 */
#define	_MIPS_ISA_MIPS2	2	/* R4000/R6000 */
#define	_MIPS_ISA_MIPS3	3	/* R4000 */
#define	_MIPS_ISA_MIPS4	4	/* TFP (R1x000) */

#if !defined(ABICALLS) && !defined(_NO_ABICALLS)
#define	ABICALLS	.abicalls
#endif

#if defined(ABICALLS) && !defined(_KERNEL)
	ABICALLS
#endif

#define _C_LABEL(x) x		/* XXX Obsolete but keep for a while */

#if !defined(__MIPSEL__) && !defined(__MIPSEB__)
#error "__MIPSEL__ or __MIPSEB__ must be defined"
#endif
/*
 * Define how to access unaligned data word
 */
#if defined(__MIPSEL__)
#define LWLO    lwl
#define LWHI    lwr
#define	SWLO	swl
#define	SWHI	swr
#define LDLO    ldl
#define LDHI    ldr
#define	SDLO	sdl
#define	SDHI	sdr
#endif
#if defined(__MIPSEB__)
#define LWLO    lwr
#define LWHI    lwl
#define	SWLO	swr
#define	SWHI	swl
#define LDLO    ldr
#define LDHI    ldl
#define	SDLO	sdr
#define	SDHI	sdl
#endif

/*
 *  Define programming environment for ABI.
 */
#if defined(ABICALLS) && !defined(_KERNEL) && !defined(_STANDALONE)

#ifndef _MIPS_SIM
#define _MIPS_SIM 1
#define _ABIO32	1
#endif
#ifndef _MIPS_ISA
#define _MIPS_ISA 2
#define _MIPS_ISA_MIPS2 2
#endif

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABI32)
#define NARGSAVE	4

#define	SETUP_GP		\
	.set	noreorder;	\
	.cpload	t9;		\
	.set	reorder;

#define	SAVE_GP(x)		\
	.cprestore x

#endif

#if (_MIPS_SIM == _ABI64) || (_MIPS_SIM == _ABIN32)
#define NARGSAVE	0

#define	SETUP_GP
#define	SAVE_GP(x)
#endif

#else /* defined(ABICALLS) && !defined(_KERNEL) */

#define	NARGSAVE	4
#define	SETUP_GP
#define	SAVE_GP(x)

#endif

#define	ALIGNSZ		16	/* Stack layout alignment */

#define	FRAMESZ(sz)	(((sz) + (ALIGNSZ-1)) & ~(ALIGNSZ-1))

/*
 *  Basic register operations based on selected ISA
 */
#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2)
#define REGSZ		4	/* 32 bit mode register size */
#define LOGREGSZ	2	/* log rsize */
#define	REG_S	sw
#define	REG_L	lw
#define	CF_SZ		24	/* Call frame size */
#define	CF_ARGSZ	16	/* Call frame arg size */
#define	CF_RA_OFFS	20	/* Call ra save offset */
#define	_MIPS_SZPTR	32
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4)
#define REGSZ		8	/* 64 bit mode register size */
#define LOGREGSZ	3	/* log rsize */
#define	REG_S	sd
#define	REG_L	ld
#define	CF_SZ		48	/* Call frame size (multiple of ALIGNSZ) */
#define	CF_ARGSZ	32	/* Call frame arg size */
#define	CF_RA_OFFS	40	/* Call ra save offset */
#define	_MIPS_SZPTR	64
#endif

#if (_MIPS_SZPTR == 32)
#define	PTR_L		lw
#define	PTR_S		sw
#define	PTR_SUB		sub
#define	PTR_ADD		add
#define	PTR_SUBU	subu
#define	PTR_ADDU	addu
#define LI		li
#define	LA		la
#define	PTR_SLL		sll
#define	PTR_SRL		srl
#define	PTR_VAL		.word
#endif

#if (_MIPS_SZPTR == 64)
#define	PTR_L		ld
#define	PTR_S		sd
#define	PTR_ADD		dadd
#define	PTR_SUB		dsub
#define	PTR_SUBU	dsubu
#define	PTR_ADDU	daddu
#define LI		dli
#define LA		dla
#define	PTR_SLL		dsll
#define	PTR_SRL		dsrl
#define	PTR_VAL		.dword
#endif

/*
 * Define -pg profile entry code.
 */
#if defined(XGPROF) || defined(XPROF)
#define	MCOUNT			\
	subu	sp, sp, 32;	\
	SAVE_GP(16);		\
	sw	ra, 28(sp);	\
	sw	gp, 24(sp);	\
	.set	noat;		\
	.set	noreorder;	\
	move	AT, ra;		\
	jal	_mcount;	\
	subu	sp, sp, 8;	\
	lw	ra, 28(sp);	\
	addu	sp, sp, 32;	\
	.set reorder;		\
	.set	at;
#else
#define	MCOUNT
#endif

/*
 * LEAF(x)
 *
 *	Declare a leaf routine.
 */
#define LEAF(x)			\
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, 0, ra;	\
	SETUP_GP		\
	MCOUNT

#define	ALEAF(x)		\
	.globl	x;		\
x:

/*
 * NLEAF(x)
 *
 *	Declare a non-profiled leaf routine.
 */
#define NLEAF(x)		\
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, 0, ra;	\
	SETUP_GP

/*
 * NON_LEAF(x)
 *
 *	Declare a non-leaf routine (a routine that makes other C calls).
 */
#define NON_LEAF(x, fsize, retpc) \
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, retpc; \
	SETUP_GP		\
	MCOUNT

/*
 * NNON_LEAF(x)
 *
 *	Declare a non-profiled non-leaf routine
 *	(a routine that makes other C calls).
 */
#define NNON_LEAF(x, fsize, retpc) \
	.align	3;		\
	.globl x;		\
	.ent x, 0;		\
x: ;				\
	.frame sp, fsize, retpc	\
	SETUP_GP

/*
 * END(x)
 *
 *	Mark end of a procedure.
 */
#define END(x) \
	.end x

/*
 * Macros to panic and printf from assembly language.
 */
#define PANIC(msg) \
	la	a0, 9f; \
	jal	panic;	\
	nop	;	\
	MSG(msg)

#define	PRINTF(msg) \
	la	a0, 9f; \
	jal	printf; \
	nop	;	\
	MSG(msg)

#define	MSG(msg) \
	.rdata; \
9:	.asciiz	msg; \
	.text

#define ASMSTR(str) \
	.asciiz str; \
	.align	3

#endif /* !_MIPS_ASM_H */

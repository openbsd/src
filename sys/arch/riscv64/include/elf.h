/*	$OpenBSD: elf.h,v 1.4 2026/05/02 14:09:17 jsing Exp $	*/

/*-
 * Copyright (c) 1996-1997 John D. Polstra.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_MACHINE_ELF_H_
#define _MACHINE_ELF_H_

/*
 * ELF definitions for the RISC-V architecture.
 */

#ifdef _KERNEL
# define __HAVE_CPU_HWCAP
# define __HAVE_CPU_HWCAP2
extern unsigned long hwcap;
extern unsigned long hwcap2;
#endif /* _KERNEL */

/* Flags passed in AT_HWCAP */
#define HWCAP_ISA_BIT(c)	(1 << ((c) - 'a'))
#define HWCAP_ISA_I		HWCAP_ISA_BIT('i')
#define HWCAP_ISA_M		HWCAP_ISA_BIT('m')
#define HWCAP_ISA_A		HWCAP_ISA_BIT('a')
#define HWCAP_ISA_F		HWCAP_ISA_BIT('f')
#define HWCAP_ISA_D		HWCAP_ISA_BIT('d')
#define HWCAP_ISA_G		\
    (HWCAP_ISA_I | HWCAP_ISA_M | HWCAP_ISA_A | HWCAP_ISA_F | HWCAP_ISA_D)
#define HWCAP_ISA_B		HWCAP_ISA_BIT('b')
#define HWCAP_ISA_C		HWCAP_ISA_BIT('c')
#define HWCAP_ISA_H		HWCAP_ISA_BIT('h')
#define HWCAP_ISA_V		HWCAP_ISA_BIT('v')

#ifdef _KERNEL
/* Kernel Used Extensions */
#define HWCAP_ISA_K_OFFSET	32
#define HWCAP_ISA_K_BIT(n)	(1UL << (HWCAP_ISA_K_OFFSET + (n)))
#define HWCAP_ISA_K_MASK	(0xffffUL << HWCAP_ISA_K_OFFSET)
#define HWCAP_ISA_ZICBOM	HWCAP_ISA_K_BIT(0)	/* Cache Block Management */
#define HWCAP_ISA_ZICBOP	HWCAP_ISA_K_BIT(1)	/* Cache Block Prefetch */
#define HWCAP_ISA_ZICBOZ	HWCAP_ISA_K_BIT(2)	/* Cache Block Zero */

/* Supervisor Extensions */
#define HWCAP_ISA_S_OFFSET	48
#define HWCAP_ISA_S_BIT(n)	(1UL << (HWCAP_ISA_S_OFFSET + (n)))
#define HWCAP_ISA_S_MASK	(0xffffUL << HWCAP_ISA_S_OFFSET)
#define HWCAP_ISA_SVNAPOT	HWCAP_ISA_S_BIT(0)	/* NAPOT Translation */
#define HWCAP_ISA_SVPBMT	HWCAP_ISA_S_BIT(1)	/* Page-Based Memory Types */
#define HWCAP_ISA_SVINVAL	HWCAP_ISA_S_BIT(2)	/* Cache Invalidation */
#define HWCAP_ISA_SSTC		HWCAP_ISA_S_BIT(3)	/* Timer Interrupts */
#define HWCAP_ISA_SCOFPMF	HWCAP_ISA_S_BIT(4)	/* Count Overflow */
#define HWCAP_ISA_SSNPM		HWCAP_ISA_S_BIT(5)	/* Pointer Masking */
#endif /* _KERNEL */

/* Flags passed in AT_HWCAP2 */
#define HWCAP2_ISA_ZBA		(1UL << 0)	/* Address Generation */
#define HWCAP2_ISA_ZBB		(1UL << 1)	/* Basic Bit Manipulation */
#define HWCAP2_ISA_ZBC		(1UL << 2)	/* Carry-Less Multiplication */
#define HWCAP2_ISA_ZBS		(1UL << 3)	/* Single-Bit */
#define HWCAP2_ISA_ZFH		(1UL << 4)	/* Scalar Half Precision FP */
#define HWCAP2_ISA_ZKT		(1UL << 5)	/* Data-Independent Execution */
#define HWCAP2_ISA_ZVBB		(1UL << 6)	/* Vector Basic Bit Manipulation */
#define HWCAP2_ISA_ZVBC		(1UL << 7)	/* Vector Carry-less Multiplication */
#define HWCAP2_ISA_ZVFH		(1UL << 8)	/* Vector Half Precision FP */
#define HWCAP2_ISA_ZVKG		(1UL << 9)	/* Vector GCM/GMAC */
#define HWCAP2_ISA_ZVKNED	(1UL << 10)	/* Vector AES Block Cipher */
#define HWCAP2_ISA_ZVKNHA	(1UL << 11)	/* Vector SHA-256 */
#define HWCAP2_ISA_ZVKNHB	(1UL << 12)	/* Vector SHA-512 */
#define HWCAP2_ISA_ZVKSED	(1UL << 13)	/* Vector ShangMi SM4 */
#define HWCAP2_ISA_ZVKSH	(1UL << 14)	/* Vector ShangMi SM3 */
#define HWCAP2_ISA_ZVKT		(1UL << 15)	/* Vector Data-Independent Execution */

#endif /* !_MACHINE_ELF_H_ */

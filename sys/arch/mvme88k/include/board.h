/*	$OpenBSD: board.h,v 1.11 2001/08/26 14:31:07 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */
#ifndef __MACHINE_BOARD_H__
#define __MACHINE_BOARD_H__
/*
 *      VME187 CPU board constants - derived from Luna88k
 */

/*
 * Something to put append a 'U' to a long constant if it's C so that
 * it'll be unsigned in both ANSI and traditional.
 */
#if defined(_LOCORE)
#define U(num)	num
#elif defined(__STDC__)
#define U(num)	num ## U
#else
#define U(num)	num/**/U
#endif
#define UDEFINED

#define MAX_CPUS	4	/* no. of CPUs */
#define MAX_CMMUS	8	/* 2 CMMUs per CPU - 1 data and 1 code */

#define SYSV_BASE	U(0x00000000)	/* system virtual base */

#define MAXU_ADDR	U(0x40000000)	/* size of user virtual space */
#define MAXPHYSMEM	U(0x10000000)	/* max physical memory */

#define VMEA16		U(0xFFFF0000)	/* VMEbus A16 */
#define VMEA16_SIZE	U(0x0000EFFF)	/* VMEbus A16 size */
#define VMEA32D16    	U(0xFF000000)	/* VMEbus A32/D16 */
#define VMEA32D16_SIZE	U(0x007FFFFF)	/* VMEbus A32/D16 size */


/* These need to be here because of the way m18x_cmmu.c 
   handles the CMMU's. */
#define CMMU_SIZE	0x1000

#ifndef CMMU_DEFS
#define CMMU_DEFS
#define SBC_CMMU_I	U(0xFFF77000) 	/* Single Board Computer code CMMU */
#define SBC_CMMU_D	U(0xFFF7F000) 	/* Single Board Computer data CMMU */

#define VME_CMMU_I0	U(0xFFF7E000) 	/* MVME188 code CMMU 0 */
#define VME_CMMU_I1	U(0xFFF7D000) 	/* MVME188 code CMMU 1 */
#define VME_CMMU_I2	U(0xFFF7B000) 	/* MVME188 code CMMU 2 */
#define VME_CMMU_I3	U(0xFFF77000) 	/* MVME188 code CMMU 3 */
#define VME_CMMU_D0	U(0xFFF6F000) 	/* MVME188 data CMMU 0 */
#define VME_CMMU_D1	U(0xFFF5F000) 	/* MVME188 data CMMU 1 */
#define VME_CMMU_D2	U(0xFFF3F000) 	/* MVME188 data CMMU 2 */
#define VME_CMMU_D3	U(0xFFF7F000) 	/* MVME188 data CMMU 3 */
#endif /* CMMU_DEFS */

/* These are the hardware exceptions. */
#define INT_BIT		0x1		/* interrupt exception		*/
#define IACC_BIT	0x2		/* instruction access exception	*/
#define DACC_BIT	0x4		/* data access exception	*/
#define MACC_BIT	0x8		/* misaligned access exception	*/
#define UOPC_BIT	0x10		/* unimplemented opcode exception*/
#define PRIV_BIT	0x20		/* priviledge violation exception*/
#define BND_BIT		0x40		/* bounds check violation	*/
#define IDE_BIT		0x80		/* illegal integer divide	*/
#define IOV_BIT		0x100		/* integer overflow exception	*/
#define ERR_BIT		0x200		/* error exception		*/
#define FPUP_BIT	0x400		/* FPU precise exception	*/
#define FPUI_BIT	0x800		/* FPU imprecise exception	*/

#if defined(MVME187) || defined(MVME197)
#include <machine/mvme1x7.h>
#endif

#ifdef MVME188
#include <machine/mvme188.h>
#endif

#endif /* __MACHINE_BOARD_H__ */



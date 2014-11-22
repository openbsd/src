/*	$OpenBSD: memeccreg.h,v 1.1 2014/11/22 22:48:38 miod Exp $	*/
/*	$NetBSD: memeccreg.h,v 1.2 2008/04/28 20:23:36 martin Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ECC memory control.
 */

/* Register offsets */
#define ECC_EN_REG	0
#define ECC_FSR_REG	8
#define ECC_AFR0_REG	16
#define ECC_AFR1_REG	20
#define ECC_DIAG_REG	24

/* ECC Memory Enable register */
#define ECC_EN_EE	0x00000001	/* Enable ECC checking */
#define ECC_EN_EI	0x00000002	/* Interrupt on correctable error */
#define ECC_EN_VER	0x0f000000	/* Version */
#define ECC_EN_IMPL	0xf0000000	/* Implementation Id */

/* ECC Memory Fault Status register */
#define ECC_FSR_CE	0x00000001	/* Correctable error */
#define ECC_FSR_TO	0x00000004	/* Timeout on write */
#define ECC_FSR_UE	0x00000008	/* Uncorrectable error */
#define ECC_FSR_DW	0x000000f0	/* Index of double word in block */
#define ECC_FSR_SYND	0x0000ff00	/* Syndrome for correctable error */
#define ECC_FSR_ME	0x00010000	/* Multiple errors */

/*
 * ECC Memory Fault Address registers
 * There are two of these. The first has bits 32-35 of the faulting
 * physical address and assorted MBus bits. The second has bits
 * 0-31 of the faulting physical address.
 */
#define ECC_AFR_PAH	0x0000000f	/* PA[31-35] */
#define ECC_AFR_TYPE	0x000000f0	/* Transaction type */
#define ECC_AFR_SIZE	0x00000700	/* Transaction size */
#define ECC_AFR_C	0x00000800	/* Mapped cacheable */
#define ECC_AFR_LOCK	0x00001000	/* Error occurred in atomic cycle */
#define ECC_AFR_MBL	0x00002000	/* Boot mode */
#define ECC_AFR_VA	0x003fc000	/* VA[12-19] (superset bits) */
#define ECC_AFR_S	0x08000000	/* Access was in supervisor mode */
#define ECC_AFR_MID	0xf0000000	/* Module code */

/* ECC Diagnostic register */
#define ECC_DR_CBX	0x00000001
#define ECC_DR_CB0	0x00000002
#define ECC_DR_CB1	0x00000004
#define ECC_DR_CB2	0x00000008
#define ECC_DR_CB4	0x00000010
#define ECC_DR_CB8	0x00000020
#define ECC_DR_CB16	0x00000040
#define ECC_DR_CB32	0x00000080
#define ECC_DR_DMODE	0x00000c00

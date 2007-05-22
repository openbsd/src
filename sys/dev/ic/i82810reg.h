/*	$OpenBSD: i82810reg.h,v 1.4 2007/05/22 04:14:03 jsg Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Intel i82810/810E memory and graphics controller */

/* Host-Hub Interface Bridge/DRAM Controller Device Registers (Device 0) */
#define	I82810_SMRAM		0x70
#define	I82810_SMRAM_GMS_DIS	0x00
#define	I82810_SMRAM_GMS_RSRVD	0x40
#define	I82810_SMRAM_GMS_512	0x80
#define	I82810_SMRAM_GMS_1024	0xc0
#define	I82810_SMRAM_USMM_DIS	0x00
#define	I82810_SMRAM_USMM_TDHE	0x10
#define	I82810_SMRAM_USMM_T5HE	0x20
#define	I82810_SMRAM_USMM_T1HE	0x30
#define	I82810_SMRAM_LSMM_DIS	0x00
#define	I82810_SMRAM_LSMM_GSM	0x04
#define	I82810_SMRAM_LSMM_CRSH	0x08
#define	I82810_SMRAM_D_LCK	0x02
#define	I82810_SMRAM_E_SMERR	0x01
#define	I82810_MISCC		0x72
#define	I82810_MISCC_GDCWS	0x0001
#define	I82810_MISCC_P_LCK	0x0008
#define	I82810_MISCC_WPTHC_NO	0x0000
#define	I82810_MISCC_WPTHC_625	0x0010
#define	I82810_MISCC_WPTHC_500	0x0020
#define	I82810_MISCC_WPTHC_375	0x0030
#define	I82810_MISCC_RPTHC_NO	0x0000
#define	I82810_MISCC_RPTHC_625	0x0040
#define	I82810_MISCC_RPTHC_500	0x0080
#define	I82810_MISCC_RPTHC_375	0x00c0

/* Graphics Device Registers (Device 1) */
#define	I82810_GMADR		0x10
#define	I82810_MMADR		0x14

#define	I82810_DRT		0x3000
#define	I82810_DRT_DP		0x01
#define	I82810_DRAMCL		0x3001
#define	I82810_DRAMCL_RPT	0x01
#define	I82810_DRAMCL_RT	0x02
#define	I82810_DRAMCL_CL	0x04
#define	I82810_DRAMCL_RCO	0x08
#define	I82810_DRAMCL_PMC	0x10
#define	I82810_DRAMCH		0x3002
#define	I82810_DRAMCH_SMS	0x07
#define	I82810_DRAMCH_DRR	0x18
#define	I82810_GTT		0x10000

/*
 * Intel i82820 memory and graphics controller
 */

/* Host-Hub Interface Bridge/DRAM Controller Device Registers (Device 0) */
#define	I82820_SMRAM		0x9c
#define	I82820_SMRAM_SHIFT	8
#define	I82820_SMRAM_G_SMRAME	(1 << 3)
#define	I82820_SMRAM_D_LCK	(1 << 4)
#define	I82820_SMRAM_D_CLS	(1 << 5)
#define	I82820_SMRAM_D_OPEN	(1 << 6)

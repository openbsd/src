/*	$NetBSD: pciide_pdc202xx_reg.h,v 1.1 1999/08/29 17:20:10 bouyer Exp $ */

/*
 * Copyright (c) 1999 Manuel Bouyer.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Registers definitions for PROMISE PDC20246 PCI IDE controller.
 * Unfortunably the HW docs available don't provide much informations
 * Most of the values set in registers comes from the FreeBSD and linux
 * drivers, and from experiments with the BIOS of a Promise Ultra/33 board.
 */

/* controller initial state */
#define PDC2xx_STATE		0x50
#define PDC2xx_STATE_SHIPID		0x8000
#define PDC2xx_STATE_IOCHRDY		0x0400
#define PDC2xx_STATE_LBA(channel)	(0x0100 << (channel))
#define PDC2xx_STATE_NATIVE		0x0080
#define PDC2xx_STATE_ISAIRQ		0x0008
#define PDC2xx_STATE_EN(channel)	(0x0002 << (channel))
#define PDC2xx_STATE_IDERAID		0x0001

/* per-drive timings */
#define PDC2xx_TIM(channel, drive) (0x60 + 4 * (drive) + 8 * (channel))
#define PDC2xx_TIM_SET_PA(r, x)	(((r) & 0xfffffff0) | ((x) & 0xf))
#define PDC2xx_TIM_SET_PB(r, x)	(((r) & 0xffffe0ff) | (((x) & 0x1f) << 8))
#define PDC2xx_TIM_SET_MB(r, x)	(((r) & 0xffff1fff) | (((x) & 0x7) << 13))
#define PDC2xx_TIM_SET_MC(r, x)	(((r) & 0xfff0ffff) | (((x) & 0xf) << 16))
#define PDC2xx_TIM_PRE		0x00000010
#define PDC2xx_TIM_IORDY	0x00000020
#define PDC2xx_TIM_ERRDY	0x00000040
#define PDC2xx_TIM_SYNC		0x00000080
#define PDC2xx_TIM_DMAW		0x00100000
#define PDC2xx_TIM_DMAR		0x00200000
#define PDC2xx_TIM_IORDYp	0x00400000
#define PDC2xx_TIM_DMARQp	0x00800000

/* The following are extentions of the DMA registers */

/* primary mode (1 byte) */
#define PDC2xx_PM	0x1a
/* secondary mode (1 byte) */
#define PDC2xx_SM	0x1b
/* System control register (4 bytes) */
#define PDC2xx_SCR	0x1c
#define PDC2xx_SCR_SET_GEN(r,x) (((r) & 0xfffffff0) | ((x) & 0xf))
#define PDC2xx_SCR_EMPTY(channel) (0x00000100 << (4 * channel))
#define PDC2xx_SCR_FULL(channel) (0x00000200 << (4 * channel))
#define PDC2xx_SCR_INT(channel) (0x00000400 << (4 * channel))
#define PDC2xx_SCR_ERR(channel) (0x00000800 << (4 * channel))
#define PDC2xx_SCR_SET_I2C(r,x) (((r) & 0xfff0ffff) | (((x) & 0xf) << 16))
#define PDC2xx_SCR_SET_POLL(r,x) (((r) & 0xff0fffff) | (((x) & 0xf) << 20))
#define PDC2xx_SCR_DMA		0x01000000
#define PDC2xx_SCR_IORDY	0x02000000
#define PDC2xx_SCR_G2FD		0x04000000		
#define PDC2xx_SCR_FLOAT	0x08000000
#define PDC2xx_SCR_RSET		0x10000000
#define PDC2xx_SCR_TST		0x20000000

/*
 * The timings provided here results from things gathered from the FreeBSD
 * driver and experimentations with the BIOS of a promise board.
 * Unfortunably I didn't have enouth HW to test all the modes.
 * They may be suboptimal.
 */

static int8_t pdc2xx_pa[] = {0x4, 0x4, 0x4, 0x7, 0x3};
static int8_t pdc2xx_pb[] = {0x13, 0x13, 0x13, 0xf, 0x7};
static int8_t pdc2xx_dma_mb[] = {0x7, 0x3, 0x3};
static int8_t pdc2xx_dma_mc[] = {0xf, 0x4, 0x3};
static int8_t pdc2xx_udma_mb[] = {0x3, 0x2, 0x1, 0x2, 0x1};
static int8_t pdc2xx_udma_mc[] = {0x1, 0x1, 0x1, 0x1, 0x1};

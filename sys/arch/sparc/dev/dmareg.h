/*	$OpenBSD: dmareg.h,v 1.7 2005/02/27 22:01:04 miod Exp $	*/
/*	$NetBSD: dmareg.h,v 1.10 1996/11/28 09:37:34 pk Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <dev/ic/lsi64854reg.h>

struct dma_regs {
	volatile u_long		csr;		/* DMA CSR */
	volatile caddr_t	addr;
	volatile u_long		bcnt;		/* DMA COUNT (in u_longs) */
	volatile u_long		test;		/* DMA TEST (in u_longs) */
#define en_testcsr	addr			/* enet registers overlap */
#define en_cachev	bcnt
#define en_bar		test
};

/*
 * PROM-reported DMA burst sizes for the SBus
 */
#define SBUS_BURST_1	0x1
#define SBUS_BURST_2	0x2
#define SBUS_BURST_4	0x4
#define SBUS_BURST_8	0x8
#define SBUS_BURST_16	0x10
#define SBUS_BURST_32	0x20
#define SBUS_BURST_64	0x40

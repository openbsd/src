/*	$OpenBSD: qecreg.h,v 1.4 2003/06/02 15:54:22 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* QEC registers */
struct qecregs {
	volatile u_int32_t ctrl;		/* control */
	volatile u_int32_t stat;		/* status */
	volatile u_int32_t psize;		/* packet size */
	volatile u_int32_t msize;		/* local-mem size (64K) */
	volatile u_int32_t rsize;		/* receive partition size */
	volatile u_int32_t tsize;		/* transmit partition size */
};

/* qecregs.ctrl: control. */
#define QEC_CTRL_MODEMASK	0xf0000000	/* QEC mode: qe or be */
#define QEC_CTRL_MMODE		0x40000000		/* MACE qec mode */
#define QEC_CTRL_BMODE		0x10000000		/* BE qec mode */
#define QEC_CTRL_EPAR		0x00000020	/* enable parity */
#define QEC_CTRL_ACNTRL		0x00000018	/* sbus arbitration control */
#define QEC_CTRL_B64		0x00000004	/* 64 byte dvma bursts */
#define QEC_CTRL_B32		0x00000002	/* 32 byte dvma bursts */
#define QEC_CTRL_B16		0x00000000	/* 16 byte dvma bursts */
#define QEC_CTRL_RESET		0x00000001	/* reset the qec */

/* qecregs.stat: status. */
#define QEC_STAT_TX		0x00000008	/* bigmac transmit irq */
#define QEC_STAT_RX		0x00000004	/* bigmac receive irq */
#define QEC_STAT_BM		0x00000002	/* bigmac qec irq */
#define QEC_STAT_ER		0x00000001	/* bigmac error irq */

/* qecregs.stat: packet size. */
#define QEC_PSIZE_2048		0x00		/* 2k packet size */
#define QEC_PSIZE_4096		0x01		/* 4k packet size */
#define QEC_PSIZE_6144		0x10		/* 6k packet size */
#define QEC_PSIZE_8192		0x11		/* 8k packet size */

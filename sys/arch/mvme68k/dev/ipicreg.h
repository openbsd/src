/*	$OpenBSD: ipicreg.h,v 1.7 2005/11/24 22:43:16 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct ipicreg {
	volatile u_char		ipic_chipid;
#define IPIC_CHIPID	0x23
	volatile u_char		ipic_chiprev;
	volatile u_char		x0[2];
	volatile u_short	ipic_base[4];		/* [slot] */
	volatile u_char		ipic_size[4];		/* [slot] */
	volatile u_char		ipic_irq[4][2];		/* [slot][irq#] */
#define IPIC_IRQ_PLTY		0x80	/* 1 = rising edge activated */
#define IPIC_IRQ_EL		0x40	/* 1 = edge, 0 = level sensitive */
#define IPIC_IRQ_INT		0x20	/* interrupt is active */
#define IPIC_IRQ_IEN		0x10	/* enable interrupt */
#define IPIC_IRQ_ICLR		0x08	/* clear interrupt */
#define IPIC_IRQ_IPLMASK	0x07
	volatile u_char		ipic_ctl[4];		/* [slot] */
#define IPIC_CTL_ERR		0x80	/* error from IP module */
#define IPIC_CTL_RTMASK		0x30	/* recovery time, measured in ms */
#define IPIC_CTL_RT0MS		0x00
#define IPIC_CTL_RT2MS		0x10
#define IPIC_CTL_RT4MS		0x20
#define IPIC_CTL_RT8MS		0x30
#define IPIC_CTL_WIDTHMASK	0x0c	/* bus width */
#define IPIC_CTL_WIDTH32	0x00
#define IPIC_CTL_WIDTH8		0x04
#define IPIC_CTL_WIDTH16	0x08
#define IPIC_CTL_MEN		0x01	/* ??? */
	volatile u_char		x1[3];
	volatile u_char		ipic_reset;
#define IPIC_RESET		0x01	/* bit clears automatically after 1ms */
};

/*
 * the following macros convert the size in bytes to/from the
 * ipic_Xsize reg values. you should ensure your size in bytes
 * is a legal value, which are 16K, or all the powers of 2 from
 * 64K to 8M.
 */
#define IPIC_SIZE_BTOR(x) \
	(((x) == 16*1024) ? (0xff) : (((x) / 64*1024) - 1))
#define IPIC_SIZE_RTOB(x) \
	(((x) == 0xff) ? (16*1024) : (((x) + 1) * 64*1024))

struct ipid {
	volatile u_char		ipid_I;		/* must be 'I' */
	volatile u_char		:8;
	volatile u_char		ipid_P;		/* must be 'P' */
	volatile u_char		:8;
	volatile u_char		ipid_A;		/* must be 'A' */
	volatile u_char		:8;
	volatile u_char		ipid_C;		/* must be 'C' */
	volatile u_char		:8;
	volatile u_char		ipid_manu;
	volatile u_char		:8;
	volatile u_char		ipid_prod;
	volatile u_char		:8;
	volatile u_char		ipid_rev;
	volatile u_char		:8;
	volatile u_char		ipid_zero;
	volatile u_char		:8;
	volatile u_char		ipid_didl;
	volatile u_char		:8;
	volatile u_char		ipid_didh;
	volatile u_char		:8;
	volatile u_char		ipid_pspecn;	/* # shorts in pack-spec id */
	volatile u_char		:8;
	volatile u_char		ipid_crc;
	volatile u_char		:8;
	volatile u_char		ipic_pspecbase;	/* start of pack-spec id */
};

#define IPIC_IPSPACE		0xfff58000
#define IPIC_IP_MODSIZE		0x00000100
#define IPIC_IP_IDOFFSET	0x80
#define IPIC_IP_REGOFFSET	0x00

struct ipicsoftc {
	struct device	sc_dev;
	struct ipicreg	*sc_ipic;

	vaddr_t		sc_ipspace;
	int		sc_nip;
};

int ipicintr_establish(int, struct intrhand *, const char *);

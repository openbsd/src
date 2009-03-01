/*	$OpenBSD: ofobioreg.h,v 1.1 2009/03/01 22:08:13 miod Exp $	*/

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
 * MVME141 On-board resources (One-Four-One OBIO)
 */

/*
 * Register layout.
 */
struct ofobioreg {
	volatile uint8_t	reg00;	/* 0x32: cpu speed in MHz? */
	volatile uint8_t	reg01;	/* 0x18 */
	volatile uint8_t	reg02;	/* 0xfc */
	volatile uint8_t	reg03;	/* 0xfc */
	volatile uint8_t	reg04;	/* 0xa8 */
	volatile uint8_t	reg05;	/* 0x40 */
	volatile uint8_t	reg06;	/* 0x82 */
	volatile uint8_t	reg07;	/* 0x82 */
	volatile uint8_t	csr_a;
	volatile uint8_t	csr_b;
	volatile uint8_t	reg0a;	/* copy of csr_a */
	volatile uint8_t	reg0b;	/* copy of csr_b */
	volatile uint8_t	csr_c;
	volatile uint8_t	csr_d;
	volatile uint8_t	reg0e;
	volatile uint8_t	reg0f;
	volatile uint8_t	reg10;
	volatile uint8_t	reg11;	/* 0x45 */
	volatile uint8_t	reg12;
	volatile uint8_t	reg13;
	volatile uint8_t	reg14;
	volatile uint8_t	reg15;
	volatile uint8_t	reg16;
	volatile uint8_t	reg17;
	volatile uint8_t	reg18;
	volatile uint8_t	reg19;
	volatile uint8_t	reg1a;
	volatile uint8_t	reg1b;
	volatile uint8_t	reg1c;
	volatile uint8_t	reg1d;
	volatile uint8_t	reg1e;
	volatile uint8_t	reg1f;
};

/* CSR A */
#define	OFO_CSRA_CACHE_MONITOR	0xe0	/* cache monitor control mask */
#define	OFO_CSRA_CACHE_CLEAR	0x10	/* clear board cache */
#define	OFO_CSRA_CACHE_WDIS	0x08	/* cache write disable */
#define	OFO_CSRA_CACHE_RDIS	0x04	/* cache read disable */

/* CSR B */
#define	OFO_CSRB_GLOBAL_INTDIS	0x80	/* global interrupt disable */
#define	OFO_CSRB_VSB_INTDIS	0x40	/* VSB interrupt disable */
#define	OFO_CSRB_TIMER_INTDIS	0x20	/* OP3 timer interrupt disable */
#define	OFO_CSRB_DISABLE_A24	0x08	/* disable A24 mode */

/* CSR C */
#define	OFO_CSRC_TIMER_ACK	0x80	/* OP3 timer acknowledge */

/* CSR D */
#define	OFO_CSRD_ABORT		0x40	/* abort switch pressed */
#define	OFO_CSRD_ABORT_LATCH	0x04	/* abort switch condition, latched */

extern struct ofobioreg *sys_ofobio;

#define	OFOBIO_CSR_ADDR	0xfff60000

#define	OFOBIOVEC_CLOCK	0x1e
#define	OFOBIOVEC_ABORT	0x42
#define	OFOBIOVEC_DART	0x44

void	ofobio_clocksetup(void);

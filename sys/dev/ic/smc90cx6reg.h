/*	$NetBSD: smc90cx6reg.h,v 1.4 1995/06/07 00:16:59 cgd Exp $ */

/*
 * Copyright (c) 1994, 1995 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
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
 */

/*
 * The A2060/A560 card use the SMC COM90C26 Arcnet chipset.
 * First or last 16k segment, resp., write a fifo which drives the reset line.
 * 2nd 16k segment contains the registers.
 * 3rd 16k segment contains the buffer RAM.
 * All are only accessible at even addresses.
 */

/* CBM Arcnet board */
#define MANUFACTURER_1 514
#define PRODUCT_1        9

/* Ameristar board */
#define MANUFACTURER_2 1053
#define PRODUCT_2         9

struct a2060 {
	volatile u_int8_t kick1;
	u_int8_t	  pad1[16383];
	volatile u_int8_t status;		/* also intmask */
	u_int8_t	  pad2;
	volatile u_int8_t command;
	u_int8_t	  pad3[16381];
	volatile u_int8_t buffers[4096];	/* even bytes only */
	u_int8_t	  pad4[12228];
	volatile u_int8_t kick2;
	u_int8_t	  pad5[16383];
};

#define checkbyte	buffers[0]
#define dipswitches	buffers[2]

/* calculate address for board b, buffer no n and offset o */
#define BUFPTR(b,n,o) (&(b)->buffers[(n)*512+(o)*2])

#define ARC_TXDIS	0x01
#define ARC_RXDIS	0x02
#define ARC_TX(x)	(0x03 | ((x)<<3))
#define ARC_RX(x)	(0x04 | ((x)<<3))
#define ARC_RXBC(x)	(0x84 | ((x)<<3))

#define ARC_CONF(x)  	(0x05 | (x))
#define CLR_POR		0x08
#define CLR_RECONFIG	0x10

#define ARC_CLR(x)	(0x06 | (x))
#define CONF_LONG	0x08
#define CONF_SHORT	0x00

/* 
 * These are not in the COM90C65 docs. Derived from the arcnet.asm
 * packet driver by Philippe Prindeville and Russel Nelson. 
 */

#define ARC_LDTST(x)	(0x07 | (x))
#define TEST_ON		0x08
#define TEST_OFF	0x00

#define ARC_TA		1	/* int mask also */
#define ARC_TMA		2	
#define ARC_RECON	4	/* int mask also */
#define ARC_TEST	8	/* not in the COM90C65 docs (see above) */
#define ARC_POR		0x10	/* non maskable interrupt */
#define ARC_ET1		0x20	/* timeout value bits, normally 1 */
#define ARC_ET2		0x40	/* timeout value bits, normally 1 */
#define ARC_RI		0x80	/* int mask also */

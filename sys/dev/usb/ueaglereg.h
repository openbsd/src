/*	$OpenBSD: ueaglereg.h,v 1.3 2007/06/09 11:06:53 mbalmer Exp $	*/

/*-
 * Copyright (c) 2003-2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

/* OPTN: default values from analog devices */
#ifndef UEAGLE_OPTN0
#define UEAGLE_OPTN0	0x80020066
#endif
#ifndef UEAGLE_OPTN2
#define UEAGLE_OPTN2	0x23700000
#endif
#ifndef UEAGLE_OPTN7
#define UEAGLE_OPTN7	0x02cd8044
#endif

#define UEAGLE_CONFIG_NO	1

#define UEAGLE_INTR_IFACE_NO	0
#define	UEAGLE_US_IFACE_NO	1
#define UEAGLE_DS_IFACE_NO	2

#define UEAGLE_ESISTR	4

#define UEAGLE_TX_PIPE		0x02
#define UEAGLE_IDMA_PIPE	0x04
#define UEAGLE_INTR_PIPE	0x84
#define UEAGLE_RX_PIPE		0x88

#define UEAGLE_REQUEST		0

#define UEAGLE_SETBLOCK		0x0001
#define UEAGLE_SETMODE		0x0003
#define UEAGLE_SET2183DATA	0x0004

#define UEAGLE_LOOPBACKOFF	0x0002
#define UEAGLE_LOOPBACKON	0x0003
#define UEAGLE_BOOTIDMA		0x0006
#define UEAGLE_STARTRESET	0x0007
#define UEAGLE_ENDRESET		0x0008
#define UEAGLE_SWAPMAILBOX	0x7fcd
#define UEAGLE_MPTXSTART	0x7fce
#define UEAGLE_MPTXMAILBOX	0x7fd6
#define UEAGLE_MPRXMAILBOX	0x7fdf

/* block within a firmware page */
struct ueagle_block_info {
	uWord	wHdr;
#define UEAGLE_BLOCK_INFO_HDR	0xabcd

	uWord	wAddress;
	uWord	wSize;
	uWord	wOvlOffset;
	uWord	wOvl;	/* overlay */
	uWord	wLast;
} __packed;

/* CMV (Configuration and Management Variable) */
struct ueagle_cmv {
	uWord	wPreamble;
#define UEAGLE_CMV_PREAMBLE	0x535c

	uByte	bDst;
#define UEAGLE_HOST	0x01
#define UEAGLE_MODEM	0x10

	uByte	bFunction;
#define UEAGLE_CR		0x10
#define UEAGLE_CW		0x11		
#define UEAGLE_CR_ACK		0x12
#define UEAGLE_CW_ACK		0x13
#define UEAGLE_MODEMREADY	0x71

	uWord	wIndex;
	uDWord	dwSymbolicAddress;
#define UEAGLE_MAKESA(a, b, c, d) ((c) << 24 | (d) << 16 | (a) << 8 | (b))
#define UEAGLE_CMV_CNTL	UEAGLE_MAKESA('C', 'N', 'T', 'L')
#define UEAGLE_CMV_DIAG	UEAGLE_MAKESA('D', 'I', 'A', 'G')
#define UEAGLE_CMV_INFO	UEAGLE_MAKESA('I', 'N', 'F', 'O')
#define UEAGLE_CMV_OPTN	UEAGLE_MAKESA('O', 'P', 'T', 'N')
#define UEAGLE_CMV_RATE	UEAGLE_MAKESA('R', 'A', 'T', 'E')
#define UEAGLE_CMV_STAT	UEAGLE_MAKESA('S', 'T', 'A', 'T')

	uWord	wOffsetAddress;
	uDWord	dwData;
#define UGETDATA(w)	((w)[2] | (w)[3] << 8 | (w)[0] << 16 | (w)[1] << 24)
#define USETDATA(w, v)							\
	((w)[2] = (uint8_t)(v),						\
	 (w)[3] = (uint8_t)((v) >> 8),					\
	 (w)[0] = (uint8_t)((v) >> 16),					\
	 (w)[1] = (uint8_t)((v) >> 24))
} __packed;

struct ueagle_swap {
	uByte	bPageNo;
	uByte	bOvl;	/* overlay */
} __packed;

struct ueagle_intr {
	uByte  	bType;
	uByte	bNotification;
	uWord	wValue;
	uWord	wIndex;
	uWord	wLength;
	uWord	wInterrupt;
#define UEAGLE_INTR_SWAP	1
#define UEAGLE_INTR_CMV		2
} __packed;

#define UEAGLE_INTR_MAXSIZE	28

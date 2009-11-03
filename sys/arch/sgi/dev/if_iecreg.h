/*	$OpenBSD: if_iecreg.h,v 1.3 2009/11/03 21:41:42 miod Exp $	*/

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
 * Ethernet controller part of the IOC3 ASIC.
 */

/*
 * Receive buffer descriptor.
 */

#define	IEC_NRXDESC_MAX		512

#define IEC_RXDESCSIZE		(4096)
#define IEC_RXD_BUFOFFSET	(64 + 2)	/* to align Ethernet header */
#define IEC_RXD_NRXPAD		(IEC_RXD_BUFOFFSET - 2 * sizeof(uint32_t))
#define IEC_RXD_BUFSIZE		(IEC_RXDESCSIZE - IEC_RXD_BUFOFFSET)

/*
 * IEC_RXDESCSIZE is the smallest multiple of 128 bytes (hardware requirement)
 * able to store ETHER_MAX_DIX_LEN bytes and the rxdesc administrative data.
 *
 * IEC_RXD_BUFOFFSET is choosen to use a different cache line on the CPU.
 * A value of 128 (IOC3 cache line) would be even better, but would not fit
 * in the MCR register.
 */

struct	iec_rxdesc {
	volatile uint32_t	rxd_stat;
#define	IEC_RXSTAT_VALID		0x80000000	/* descriptor valid */
#define	IEC_RXSTAT_LEN_MASK		0x07ff0000
#define	IEC_RXSTAT_LEN_SHIFT		16
#define	IEC_RXSTAT_CHECKSUM_MASK	0x0000ffff
	uint32_t		rxd_err;
#define	IEC_RXERR_CRC			0x00000001	/* CRC error */
#define	IEC_RXERR_FRAME			0x00000002	/* Framing erorr */
#define	IEC_RXERR_CODE			0x00000004	/* Code violation */
#define	IEC_RXERR_INVPREAMB		0x00000008	/* Invalid preamble */
#define	IEC_RXERR_MULTICAST		0x04000000	/* Multicast packet */
#define	IEC_RXERR_BROADCAST		0x08000000	/* Broadcast packet */
#define	IEC_RXERR_LONGEVENT		0x10000000	/* Long packet */
#define	IEC_RXERR_BADPACKET		0x20000000	/* Bad packet */
#define	IEC_RXERR_GOODPACKET		0x40000000
#define	IEC_RXERR_CARRIER		0x80000000	/* Carrier event */
	uint8_t			rxd_pad[IEC_RXD_NRXPAD];
	uint8_t			rxd_buf[IEC_RXD_BUFSIZE];
};

/*
 * Transmit buffer descriptor.
 */

#define	IEC_NTXDESC_MAX		128

#define IEC_TXDESCSIZE		128
#define IEC_NTXPTR		2
#define IEC_TXD_BUFOFFSET	\
	(2 * sizeof(uint32_t) + IEC_NTXPTR * sizeof(uint64_t))
#define IEC_TXD_BUFSIZE		(IEC_TXDESCSIZE - IEC_TXD_BUFOFFSET)

struct	iec_txdesc {
	uint32_t	txd_cmd;
#define	IEC_TXCMD_DATALEN		0x000007ff
#define	IEC_TXCMD_TXINT			0x00001000	/* interrupt after TX */
#define	IEC_TXCMD_BUF_V			0x00010000	/* txd_buf valid */
#define	IEC_TXCMD_PTR0_V		0x00020000	/* tx_ptr[0] valid */
#define	IEC_TXCMD_PTR1_V		0x00040000	/* tx_ptr[1] valid */
#define	IEC_TXCMD_HWCHECKSUM		0x00080000	/* perform hw cksum */
#define	IEC_TXCMD_CHECKSUM_POS_MASK	0x07f00000	/* hw cksum byte pos */
#define	IEC_TXCMD_CHECKSUM_POS_SHIFT	20
	uint32_t	txd_len;
#define	IECTX_BUF0_LEN_MASK		0x0000007f
#define	IECTX_BUF0_LEN_SHIFT		0
#define	IECTX_BUF1_LEN_MASK		0x0007ff00
#define	IECTX_BUF1_LEN_SHIFT		8
#define	IECTX_BUF2_LEN_MASK		0x7ff00000
#define	IECTX_BUF2_LEN_SHIFT		20
	uint64_t	txd_ptr[IEC_NTXPTR];
	uint8_t		txd_buf[IEC_TXD_BUFSIZE];
};

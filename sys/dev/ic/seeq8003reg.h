/*	$OpenBSD: seeq8003reg.h,v 1.1 2012/03/28 20:44:23 miod Exp $	*/
/*	$NetBSD: seeq8003reg.h,v 1.3 2001/06/07 05:19:26 thorpej Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Register definitions for the Seeq 8003 and 80C03 ethernet controllers
 *
 * Based on documentation available at
 * http://www.lsilogic.com/techlib/techdocs/networking/eol/80c03.pdf .
 */

#define	SEEQ_ADDR0	0		/* Station Address Byte 0 */
#define	SEEQ_ADDR1	1		/* Station Address Byte 1 */
#define	SEEQ_ADDR2	2		/* Station Address Byte 2 */
#define	SEEQ_ADDR3	3		/* Station Address Byte 3 */
#define	SEEQ_ADDR4	4		/* Station Address Byte 4 */
#define	SEEQ_ADDR5	5		/* Station Address Byte 5 */

#define SEEQ_TXCOLLS0	0		/* Transmit Collision Counter LSB */
#define SEEQ_TXCOLLS1	1		/* Transmit Collision Counter MSB */
#define SEEQ_ALLCOLL0	2		/* Total Collision Counter LSB */
#define SEEQ_ALLCOLL1	3		/* Total Collision Counter MSB */

#define SEEQ_TEST	4		/* "For Test Only" - Do Not Use */

#define SEEQ_SQE	5		/* SQE / No Carrier */
#define SQE_FLAG	0x01		/* SQE Flag */
#define SQE_NOCARR	0x02		/* No Carrier Flag */

#define SEEQ_RXCMD	6		/* Rx Command */
#define RXCMD_IE_OFLOW	0x01		/* Interrupt on Overflow Error */
#define RXCMD_IE_CRC	0x02		/* Interrupt on CRC Error */
#define RXCMD_IE_DRIB	0x04		/* Interrupt on Dribble Error */
#define RXCMD_IE_SHORT	0x08		/* Interrupt on Short Frame */
#define RXCMD_IE_END	0x10		/* Interrupt on End of Frame */
#define RXCMD_IE_GOOD	0x20		/* Interrupt on Good Frame */
#define RXCMD_REC_MASK	0xc0		/* Receiver Match Mode Mask */
#define RXCMD_REC_NONE	0x00		/* Receiver Disabled */
#define RXCMD_REC_ALL	0x40		/* Receive All Frames */
#define RXCMD_REC_BROAD	0x80		/* Receive Station/Broadcast Frames */
#define RXCMD_REC_MULTI	0xc0		/* Station/Broadcast/Multicast */

#define SEEQ_RXSTAT	6		/* Rx Status */
#define RXSTAT_OFLOW	0x01		/* Frame Overflow Error */
#define RXSTAT_CRC	0x02		/* Frame CRC Error */
#define RXSTAT_DRIB	0x04		/* Frame Dribble Error */
#define RXSTAT_SHORT	0x08		/* Received Short Frame */
#define RXSTAT_END	0x10		/* Received End of Frame */
#define RXSTAT_GOOD	0x20		/* Received Good Frame */
#define RXSTAT_OLDNEW	0x80		/* Old/New Status */

#define SEEQ_TXCMD	7		/* Tx Command */
#define TXCMD_IE_UFLOW	0x01		/* Interrupt on Transmit Underflow */
#define TXCMD_IE_COLL	0x02		/* Interrupt on Transmit Collision */
#define TXCMD_IE_16COLL	0x04		/* Interrupt on 16 Collisions */
#define TXCMD_IE_GOOD	0x08		/* Interrupt on Transmit Succes */
#define TXCMD_ENABLE_C	0xf0		/* (80C03) Enable 80C03 Mode */
#define TXCMD_BANK_MASK	0x60		/* (80C03) Register Bank Mask */
#define TXCMD_BANK0	0x00		/* (80C03) Register Bank 0 (8003) */
#define TXCMD_BANK1	0x20		/* (80C03) Register Bank 1 (Writes) */
#define TXCMD_BANK2	0x40		/* (80C03) Register Bank 2 (Writes) */

#define SEEQ_TXSTAT	7		/* Tx Status */
#define TXSTAT_UFLOW	0x01		/* Transmit Underflow */
#define TXSTAT_COLL	0x02		/* Transmit Collision */
#define TXSTAT_16COLL	0x04		/* 16 Collisions */
#define TXSTAT_GOOD	0x08		/* Transmit Success */
#define TXSTAT_OLDNEW	0x80		/* Old/New Status */

/*
 * 80C03 Mode Register Bank 1
 */

#define SEEQ_MC_HASH0	0		/* Multicast Filter Byte 0 (LSB) */
#define SEEQ_MC_HASH1	1		/* Multicast Filter Byte 1 */
#define SEEQ_MC_HASH2	2		/* Multicast Filter Byte 2 */
#define SEEQ_MC_HASH3	3		/* Multicast Filter Byte 3 */
#define SEEQ_MC_HASH4	4		/* Multicast Filter Byte 4 */
#define SEEQ_MC_HASH5	5		/* Multicast Filter Byte 5 */

/*
 * 80C03 Mode Register Bank 2
 */

#define SEEQ_MC_HASH6	0		/* Multicast Filter Byte 6 */
#define SEEQ_MC_HASH7	1		/* Multicast Filter Byte 7 (MSB) */

#define SEEQ_RESERVED0	2		/* Reserved (Set to All Zeroes) */

#define SEEQ_TXCTRL	3		/* Tx Control */
#define TXCTRL_TXCOLL	0x01		/* Clear/Enable Tx Collision Counter */
#define TXCTRL_COLL	0x02		/* Clear/Enable Collision Counter */
#define TXCTRL_SQE	0x04		/* Clear/Enable SQE Flag */
#define	TXCTRL_HASH	0x08		/* Enable Multicast Hash Filter */
#define TXCTRL_SHORT	0x10		/* Receive Short (<13 Bytes) Frames */
#define TXCTRL_NOCARR	0x20		/* Clear/Enable No Carrier Flag */

#define SEEQ_CFG	4		/* Transmit/Receive Configuration */
#define CFG_RX_GRPADDR	0x01		/* Ignore Last 4 Bits of Address */
#define CFG_TX_AUTOPAD	0x02		/* Automatically Pad to 60 Bytes */
#define CFG_TX_NOPRE	0x04		/* Do Not Add Preamble Pattern */
#define CFG_RX_NOOWN	0x08		/* Do Not Receive Own Packets */
#define CFG_TX_NOCRC	0x10		/* No Not Append CRC */
#define CFG_TX_DUPLEX	0x20		/* AutoDUPLEX - Ignore Carrier */
#define CFG_RX_CRCFIFO	0x40		/* Write CRC to FIFO */
#define CFG_RX_FASTDISC	0x80		/* Fast Receive Discard Mode */

#define SEEQ_RESERVED1	5		/* Reserved */
#define SEEQ_RESERVED2	6		/* Reserved */
#define SEEQ_RESERVED3	7		/* Reserved */

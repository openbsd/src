/* $NetBSD: if_ebreg.h,v 1.2 1996/03/18 21:23:12 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * if_ebreg.h
 *
 * EtherB device driver
 *
 * Created      : 08/07/95
 */

/*
 * SEEQ 80C04 Register Definitions
 */

#define EB_8004_BASE	0x800

/*
 * SEEQ 80C04 registers
 */

#define EB_8004_COMMAND		0x000
#define EB_8004_STATUS		0x000
#define EB_8004_CONFIG1		0x040
#define EB_8004_CONFIG2		0x080
#define EB_8004_RX_END		0x0c0
#define EB_8004_BUFWIN		0x100
#define EB_8004_RX_PTR		0x140
#define EB_8004_TX_PTR		0x180
#define EB_8004_DMA_ADDR	0x1c0

/*  */

#define EB_CMD_TEST_INTEN	(1 << 0)
#define EB_CMD_RX_INTEN		(1 << 1)
#define EB_CMD_TX_INTEN		(1 << 2)
#define EB_CMD_TEST_INTACK	(1 << 4)
#define EB_CMD_RX_INTACK	(1 << 5)
#define EB_CMD_TX_INTACK	(1 << 6)
#define EB_CMD_BW_INTACK	(1 << 7)
#define EB_CMD_TEST_INT1	(1 << 8)
#define EB_CMD_RX_ON		(1 << 9)
#define EB_CMD_TX_ON		(1 << 10)
#define EB_CMD_TEST_INT2	(1 << 11)
#define EB_CMD_RX_OFF		(1 << 12)
#define EB_CMD_TX_OFF		(1 << 13)
#define EB_CMD_FIFO_READ	(1 << 14)
#define EB_CMD_FIFO_WRITE	(1 << 15)

#define EB_STATUS_TEST_INT	(1 << 4)
#define EB_STATUS_RX_INT	(1 << 5)
#define EB_STATUS_TX_INT	(1 << 6)
#define EB_STATUS_RX_ON		(1 << 9)
#define EB_STATUS_TX_ON		(1 << 10)
#define EB_STATUS_TX_NOFAIL	(1 << 12)
#define EB_STATUS_FIFO_FULL	(1 << 13)
#define EB_STATUS_FIFO_EMPTY	(1 << 14)
#define EB_STATUS_FIFO_DIR	(1 << 15)
#define EB_STATUS_FIFO_READ	(1 << 15)

#define EB_CFG1_SPECIFIC	((0 << 15) | (0 << 14))
#define EB_CFG1_BROADCAST	((0 << 15) | (1 << 14))
#define EB_CFG1_MULTICAST	((1 << 15) | (0 << 14))
#define EB_CFG1_PROMISCUOUS	((1 << 15) | (1 << 14))

#define EB_CFG2_BYTESWAP	(1 << 0)
#define EB_CFG2_REA_AUTOUPDATE	(1 << 1)
#define EB_CFG2_RX_TX_DISABLE	(1 << 2)
#define EB_CFG2_CRC_ERR_ENABLE	(1 << 3)
#define EB_CFG2_DRIB_ERR_ENABLE (1 << 4)
#define EB_CFG2_PASS_LONGSHORT	(1 << 5)
#define EB_CFG2_PREAM_SELECT	(1 << 7)
#define EB_CFG2_RX_CRC		(1 << 9)
#define EB_CFG2_NO_TX_CRC	(1 << 10)
#define EB_CFG2_LOOPBACK	(1 << 11)
#define EB_CFG2_OUTPUT		(1 << 12)
#define EB_CFG2_RESET		(1 << 15)

#define EB_CFG3_SLEEP		(1 << 3)

#define EB_BUFCODE_STATION_ADDR	0x00
#define EB_BUFCODE_ADDRESS_PROM 0x06
#define EB_BUFCODE_TX_EAP	0x07
#define EB_BUFCODE_LOCAL_MEM	0x08
#define EB_BUFCODE_TX_COLLS	0x0b
#define EB_BUFCODE_CONFIG3	0x0c
#define EB_BUFCODE_PRODUCTID	0x0d
#define EB_BUFCODE_TESTENABLE   0x0e
#define EB_BUFCODE_MULTICAST	0x0f

#define EB_PKTHDR_TX		(1 << 7)
#define EB_PKTHDR_RX		(0 << 7)
#define EB_PKTHDR_CHAIN_CONT	(1 << 6)
#define EB_PKTHDR_DATA_FOLLOWS	(1 << 5)

#define EB_PKTHDR_DONE		(1 << 7)

#define EB_TXHDR_BABBLE		(1 << 0)
#define EB_TXHDR_COLLISION	(1 << 1)
#define EB_TXHDR_COLLISION16	(1 << 2)
#define EB_TXHDR_COLLISIONMASK	(0x78)
#define EB_TXHDR_ERROR_MASK	(0x07)

#define EB_TXHDR_BABBLE_INT	(1 << 0)
#define EB_TXHDR_COLLISION_INT	(1 << 1)
#define EB_TXHDR_COLLISION16_INT	(1 << 2)
#define EB_TXHDR_XMIT_SUCCESS_INT	(1 << 3)
#define EB_TXHDR_SQET_TEST_INT	(1 << 3)

#define EB_RXHDR_OVERSIZE	(1 << 0)
#define EB_RXHDR_CRC_ERROR	(1 << 1)
#define EB_RXHDR_DRIBBLE_ERROR	(1 << 2)
#define EB_RXHDR_SHORT_FRAME	(1 << 3)

#define EB_BUFFER_SIZE		0x10000
#define EB_TX_BUFFER_SIZE	0x4000
#define EB_RX_BUFFER_SIZE	0xC000

/* Packet buffer size */

#define	EB_BUFSIZ	2048

/* End of if_ebreg.h */

/*	$NetBSD: if_qnreg.h,v 1.1.2.1 1995/11/10 16:39:14 chopps Exp $	*/

/*
 * Copyright (c) 1995 Mika Kortelainen
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
 *      This product includes software developed by  Mika Kortelainen
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
 *
 * Thanks for Aspecs Oy (Finland) for the data book for the NIC used
 * in this card and also many thanks for the Resource Management Force
 * (QuickNet card manufacturer) and especially Daniel Koch for providing
 * me with the necessary 'inside' information to write the driver.
 *
 */

/*
 * The QuickNet card uses the Fujitsu's MB86950B NIC (Network Interface
 * Controller) chip, located at card base address + 0xff00. NIC registers
 * are accessible only at even byte addresses, so the register offsets must
 * be multiplied by two. Actually, these registers are read/written as words.
 *
 * As the card doesn't use DMA, data is input/output at FIFO register
 * (base address + 0xff20).  The card has 64K buffer memory and is pretty
 * fast despite the lack of DMA.
 *
 * The FIFO register MUST be accessed as a word (16 bits).
 *
 */

#define QUICKNET_NIC_BASE	0xff00


#define NIC_DLCR0		( 0    ) /* Transmit status */
#define NIC_DLCR1		( 1 * 2) /* Transmit masks  */
#define NIC_DLCR2		( 2 * 2) /* Receive status  */
#define NIC_DLCR3		( 3 * 2) /* Receive masks   */
#define NIC_DLCR4		( 4 * 2) /* Transmit mode   */
#define NIC_DLCR5		( 5 * 2) /* Receive mode    */
#define NIC_DLCR6		( 6 * 2) /* Software reset  */
#define NIC_DLCR7		( 7 * 2) /* TDR (LSB)       */
#define NIC_DLCR8		( 8 * 2) /* Node ID0        */
#define NIC_DLCR9		( 9 * 2) /* Node ID1        */
#define NIC_DLCR10		(10 * 2) /* Node ID2        */
#define NIC_DLCR11		(11 * 2) /* Node ID3        */
#define NIC_DLCR12		(12 * 2) /* Node ID4        */
#define NIC_DLCR13		(13 * 2) /* Node ID5        */
#define NIC_DLCR15		(15 * 2) /* TDR (MSB)       */
#define NIC_BMPR0		(16 * 2) /* Buffer memory port (FIFO) */
#define NIC_BMPR2		(18 * 2) /* Packet length   */
#define NIC_BMPR4		(20 * 2) /* DMA enable      */

#define QNET_MAGIC		0x30     /* GAL magic */


/* DLCR0 - Transmit Status */
#define BUS_WRITE_ERROR		0x0101 /* Bus write error       */
#define T_SIXTEEN_COL		0x0202 /* 16 collision          */
#define T_COL			0x0404 /* Collision             */
#define T_UNDERFLOW		0x0808 /* Underflow             */
#define T_TMT_OK		0x8080 /* Transmit okay         */
#define CLEAR_T_ERR		0x0f0f /* Clear transmit errors */

/* DLCR1 - Transmit Interrupt Masks */
#define INT_SIXTEEN_COL		0x0202 /* 16 Collision          */
#define INT_TMT_OK		0x8080 /* Transmit okay         */
#define CLEAR_T_MASK		0x0000 /* Clear transmit interrupt masks */

/* DLCR2 - Receive Status */
#define R_BUS_RD_ERR		0x4040 /* Bus read error        */
#define R_PKT_RDY		0x8080 /* Packet ready          */
#define CLEAR_R_ERR		0xcfcf /* Clear receive errors  */

/* DLCR3 - Receive Interrupt Masks */
#define R_INT_OVR_FLO		0x0101 /* Receive buf overflow  */
#define R_INT_CRC_ERR		0x0202 /* CRC error             */
#define R_INT_ALG_ERR		0x0404 /* Alignment error       */
#define R_INT_SRT_PKT		0x0808 /* Short packet          */
#define R_INT_PKT_RDY		0x8080 /* Packet ready          */
#define CLEAR_R_MASK		0x0000 /* Clear receive intr masks */

/* DLCR4 - Transmit Mode */
#define NO_LOOPBACK		0x0202 /* Loopback control      */

/* DLCR5 - Receive Mode */
/* Normal mode: accept physical address, multicast group addresses
 * which match the 1st three bytes and broadcast address.
 */
#define NORMAL_MODE		0x0101
#define PROMISCUOUS_MODE	0x0303 /* Accept all packets    */
#define RM_BUF_EMP		0x4040 /* Buffer empty          */

/* DLCR6 - Enable Data Link Controller */
#define DISABLE_DLC		0x8080 /* Disable data link controller */
#define ENABLE_DLC		0x0000 /* Enable data link controller  */

/* DLCR8:DLCR13 - Node ID Registers */
#define QNET_HARDWARE_ADDRESS	NIC_DLCR8

/* BMPR3:BMPR2 - Packet Length Registers (Write-only) */
#define TRANSMIT_START		0x0080

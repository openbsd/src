/*	$OpenBSD: i82595reg.h,v 1.3 2003/10/21 18:58:49 jmc Exp $	*/
/*	$NetBSD: i82595reg.h,v 1.1 1996/05/06 21:36:51 is Exp $	*/

/*
 * Copyright (c) 1996, Ignatios Souvatzis.
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
 *	This product includes software developed by Ignatios Souvatzis
 *	for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Intel 82595 Ethernet chip register, bit, and structure definitions.
 *
 * Written by is with reference to Intel's i82595FX data sheet, with some 
 * clarification coming from looking at the Clarkson Packet Driver code for this
 * chip written by Russ Nelson and others;
 *
 * and
 *
 * configuration EEPROM layout. Written with reference to Intels
 * public "LAN595 Hardware and Software Specifications" document.
 */

/* registers */

/* bank0 */

#define COMMAND_REG 0	/* available in any bank */

#define		MC_SETUP_CMD	0x03
#define		XMT_CMD		0x04
#define		TDR_CMD		0x05
#define		DUMP_CMD	0x06
#define		DIAG_CMD	0x07
#define		RCV_ENABLE_CMD	0x08
#define		RCV_DISABLE_CMD	0x0a
#define		RCV_STOP_CMD	0x0b
#define		RESET_CMD	0x0e
#define		TRISTATE_CMD	0x16
#define		NO_TRISTATE_CMD	0x17
#define		POWER_DOWN_CMD	0x18
#define		SLEEP_MODE_CMD	0x19
#define		NEGOTIATE_CMD	0x1a
#define		RESUME_XMT_CMD	0x1c
#define		SEL_RESET_CMD	0x1e
#define		BANK_SEL(n)	(n<<6)	/* 0, 1, 2 */

#define STATUS_REG 1

#define		RX_STP_INT	0x01
#define		RX_INT		0x02
#define		TX_INT		0x04
#define		EXEC_INT	0x08
#define		EXEC_STATUS	0x30

#define ID_REG 2

#define		ID_REG_MASK	0x2c
#define		ID_REG_SIG	0x24
#define		R_ROBIN_BITS	0xc0
#define		R_ROBIN_SHIFT	6
#define		AUTO_ENABLE	0x10

#define INT_MASK_REG 3

#define		RX_STOP_BIT	0x01
#define		RX_BIT		0x02
#define		TX_BIT		0x04
#define		EXEC_BIT	0x08
#define		ALL_INTS	0x0f

#define RCV_START_LOW 4
#define RCV_START_HIGH 5

#define RCV_STOP_LOW 6
#define RCV_STOP_HIGH 7

#define XMT_ADDR_REG 0x0a
#define HOST_ADDR_REG 0x0c
#define MEM_PORT_REG 0x0e

/* -------------------- bank1 -------------------- */

#define REG1 1

#define		WORD_WIDTH	0x02
#define		INT_ENABLE	0x80

#define INT_NO_REG 2

#define RCV_LOWER_LIMIT_REG 8
#define RCV_UPPER_LIMIT_REG 9

#define XMT_LOWER_LIMIT_REG 10
#define XMT_UPPER_LIMIT_REG 11

/* bank2 */

/* reg1, apparently */

#define		XMT_CHAIN_INT	0x20	/* interrupt at end of xmt chain */
#define		XMT_CHAIN_ERRSTOP 0x40	/* int at end of chain even if err */
#define		RCV_DISCARD_BAD	0x80	/* Throw bad frames away and continue */

#define RECV_MODES_REG 2

#define		PROMISC_MODE	0x01
#define		NO_RX_CRC	0x04
#define		NO_ADD_INS	0x10
#define		MULTI_IA	0x20

#define		MATCH_ID	(NO_ADD_INS | NO_RX_CRC | 0x02)
#define		MATCH_ALL	(NO_ADD_INS | NO_RX_CRC | 0x01)
#define		MATCH_BRDCST	(NO_ADD_INS | NO_RX_CRC)

#define MEDIA_SELECT 3

#define		TPE_BIT		0x04
#define		BNC_BIT		0x20
#define		TEST_MODE_MASK	0x3f

#define I_ADD(n) (n+4)	/* 0..5 -> 4..9 */

#define EEPROM_REG 10

#define		EEDO 8
#define		EEDI 4
#define		EECS 2
#define		EESK 1

/*
 * EEPROM layout. Written with reference to Intels public "LAN595 Hardware and
 * Software Specifications" document.
 */

#define EEPPW0		0
#define		EEPP_BusWidth	0x0004
#define		EEPP_FlashAdrs	0x0038
#define		EEPP_FLASHTRANSFORM {-1, -1, 0xC8000, 0xCC000, 0xD0000, \
					0xD4000, 0xD8000, 0xDC000}
#define		EEPP_AutoIO	0x0040
#define		EEPP_IOMapping	0xfc00

#define EEPPW1		1
#define		EEPP_Int	0x0007
#define		EEPP_INTMAP	{3, 5, 9, 10, 11, -1, -1, -1}
#define		EEPP_RINTMAP	{0xff, 0xff, 0x02, 0x00, 0xff, 0x01, 0xff, \
				 0xff, 0xff, 0x02, 0x03, 0x04 }

#define		EEPP_LinkInteg	0x0008
#define		EEPP_PolarCorr	0x0010
#define		EEPP_AuiTpe	0x0020
#define		EEPP_Jabber	0x0040
#define		EEPP_AutoPort	0x0080
#define		EEPP_SmOut	0x0100
#define		EEPP_BootFls	0x0200
#define		EEPP_DramSize	0x1000
#define		EEPP_AltReady	0x2000

#define EEPPEther2	2
#define EEPPEther1	3
#define EEPPEther0	4

#define EEPPEther2a	0x3c
#define EEPPEther1a	0x3d
#define EEPPEther0a	0x3e

#define EEPPW5		5
#define		EEPP_BncTpe	0x0001
#define		EEPP_RomSlct	0x0006	/* none, NetWare, NDIS, rsrvd. */
#define		EEPP_NumConn	0x0008	/* 0=2, 1=3 */

#define EEPW6		6
#define EEPP_BoardRev	0x00FF

#define EEPP_LENGTH 0x40
#define EEPP_CHKSUM 0xBABA /* Intel claim 0x0, but this seems to be wrong */

#define I595_XMT_HDRLEN	8

#define CMD_MASK	0x001f
#define TX_DONE		0x0080
#define CHAIN		0x8000

#define XMT_STATUS	0x02
#define XMT_CHAIN	0x04
#define XMT_COUNT	0x06

#define I595_RCV_HDRLEN	8

#define RCV_DONE	0x0008
#define RX_OK		0x2000
#define RX_ERR		0x0d81



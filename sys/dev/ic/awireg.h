/* $NetBSD: awireg.h,v 1.2 1999/11/05 05:13:36 sommerfeld Exp $ */
/* $OpenBSD: awireg.h,v 1.1 1999/12/16 02:56:56 deraadt Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The firmware typically loaded onto Am79C930-based 802.11 interfaces
 * uses a 32k or larger shared memory buffer to communicate with the
 * host.
 *
 * Depending on the exact configuration of the device, this buffer may
 * either be mapped into PCMCIA memory space, or accessible a byte at
 * a type through PCMCIA I/O space.
 *
 * This header defines offsets into this shared memory.
 */


/*
 * LAST_TXD block.  5 32-bit words.
 *
 * There are five different output queues; this defines pointers to
 * the last completed descriptor for each one.
 */
#define AWI_LAST_TXD		0x3ec	/* last completed Tx Descr */

#define AWI_LAST_BCAST_TXD	AWI_LAST_TXD+0
#define AWI_LAST_MGT_TXD	AWI_LAST_TXD+4
#define AWI_LAST_DATA_TXD	AWI_LAST_TXD+8
#define AWI_LAST_PS_POLL_TXD	AWI_LAST_TXD+12
#define AWI_LAST_CF_POLL_TXD	AWI_LAST_TXD+16

/*
 * Banner block; null-terminated string.
 *
 * The doc says it contains
 * "PCnetMobile:v2.00 mmddyy APIx.x\0"
 */

#define AWI_BANNER		0x480	/* Version string */
#define AWI_BANNER_LEN		0x20

/*
 * Command block protocol:
 * write command byte to a zero value.
 * write command status to a zero value.
 * write arguments to AWI_COMMAND_PARAMS
 * write command byte to a non-zero value.
 * wait for command status to be non-zero.
 * write command byte to a zero value.
 * write command status to a zero value.
 */

#define AWI_CMD		0x4a0	/* Command opcode byte */

#define AWI_CMD_IDLE		0x0
#define AWI_CMD_NOP		0x1

#define AWI_CMD_SET_MIB	0x2
#define AWI_CMD_GET_MIB	0x9

#define AWI_CA_MIB_TYPE		0x0
#define AWI_CA_MIB_SIZE		0x1
#define AWI_CA_MIB_INDEX		0x2
#define AWI_CA_MIB_DATA		0x4

#define AWI_MIB_LOCAL		0x0
#define AWI_MIB_MAC_ADDR	0x2
#define AWI_MIB_MAC		0x3
#define AWI_MIB_MAC_STAT	0x4
#define AWI_MIB_MAC_MGT	0x5
#define AWI_MIB_DRVR_MAC	0x6
#define AWI_MIB_PHY		0x7

#define AWI_MIB_LAST		AWI_MIB_PHY


#define AWI_CMD_INIT_TX	0x3

#define AWI_CA_TX_LEN			0x14
#define AWI_CA_TX_DATA			0x0
#define AWI_CA_TX_MGT			0x4
#define AWI_CA_TX_BCAST	       	0x8
#define AWI_CA_TX_PS			0xc
#define AWI_CA_TX_CF			0x10

#define AWI_CMD_FLUSH_TX	0x4

#define AWI_CA_FTX_LEN			0x5
#define AWI_CA_FTX_DATA		0x0
#define AWI_CA_FTX_MGT			0x1
#define AWI_CA_FTX_BCAST		0x2
#define AWI_CA_FTX_PS			0x3
#define AWI_CA_FTX_CF			0x4

#define AWI_CMD_INIT_RX	0x5
#define AWI_CA_IRX_LEN			0x8
#define AWI_CA_IRX_DATA_DESC		0x0 /* return */
#define AWI_CA_IRX_PS_DESC		0x4 /* return */

#define AWI_CMD_KILL_RX	0x6

#define AWI_CMD_SLEEP		0x7
#define AWI_CA_SLEEP_LEN		0x8 
#define AWI_CA_WAKEUP			0x0 /* uint64 */

#define AWI_CMD_WAKE		0x8

#define AWI_CMD_SCAN		0xa
#define AWI_CA_SCAN_LEN		0x6
#define AWI_CA_SCAN_DURATION		0x0
#define AWI_CA_SCAN_SET		0x2
#define AWI_CA_SCAN_PATTERN		0x3
#define AWI_CA_SCAN_IDX		0x4
#define AWI_CA_SCAN_SUSP		0x5

#define AWI_CMD_SYNC		0xb
#define AWI_CA_SYNC_LEN		0x14
#define AWI_CA_SYNC_SET		0x0
#define AWI_CA_SYNC_PATTERN		0x1
#define AWI_CA_SYNC_IDX		0x2
#define AWI_CA_SYNC_STARTBSS		0x3
#define AWI_CA_SYNC_DWELL		0x4
#define AWI_CA_SYNC_MBZ		0x6
#define AWI_CA_SYNC_TIMESTAMP		0x8
#define AWI_CA_SYNC_REFTIME		0x10

#define AWI_CMD_RESUME		0xc

#define AWI_CMD_STATUS		0x4a1 	/* Command status */

#define AWI_STAT_IDLE		0x0
#define AWI_STAT_OK		0x1
#define AWI_STAT_BADCMD	0x2
#define AWI_STAT_BADPARM	0x3
#define AWI_STAT_NOTIMP	0x4
#define AWI_STAT_BADRES	0x5
#define AWI_STAT_BADMODE	0x6

#define AWI_ERROR_OFFSET	0x4a2 	/* Offset to erroneous parameter */
#define AWI_CMD_PARAMS		0x4a4 	/* Command parameters */

#define AWI_CSB		0x4f0 	/* Control/Status block */

#define AWI_SELFTEST		0x4f0

#define AWI_SELFTEST_INIT		0x00 /* initial */
#define AWI_SELFTEST_FIRMCKSUM		0x01 /* firmware cksum running */
#define AWI_SELFTEST_HARDWARE		0x02 /* hardware tests running */
#define AWI_SELFTEST_MIB		0x03 /* mib initializing */

#define AWI_SELFTEST_MIB_FAIL		0xfa
#define AWI_SELFTEST_RADIO_FAIL	0xfb
#define AWI_SELFTEST_MAC_FAIL		0xfc
#define AWI_SELFTEST_FLASH_FAIL	0xfd
#define AWI_SELFTEST_RAM_FAIL		0xfe
#define AWI_SELFTEST_PASSED		0xff

#define AWI_STA_STATE		0x4f1

#define AWI_STA_AP			0x20 /* acting as AP */
#define AWI_STA_NOPSP			0x10 /* Power Saving disabled */
#define AWI_STA_DOZE			0x08 /* about to go to sleep */
#define AWI_STA_PSP			0x04 /* enable PSP */
#define AWI_STA_RXEN			0x02 /* enable RX */
#define AWI_STA_TXEN			0x01 /* enable TX */
					      
#define AWI_INTSTAT		0x4f3
#define AWI_INTMASK		0x4f4

/* Bits in AWI_INTSTAT/AWI_INTMASK */

#define AWI_INT_GROGGY			0x80 /* about to wake up */
#define AWI_INT_CFP_ENDING		0x40 /* cont. free period ending */
#define AWI_INT_DTIM			0x20 /* beacon outgoing */
#define AWI_INT_CFP_START		0x10 /* cont. free period starting */
#define AWI_INT_SCAN_CMPLT		0x08 /* scan complete */
#define AWI_INT_TX			0x04 /* tx done */
#define AWI_INT_RX			0x02 /* rx done */
#define AWI_INT_CMD			0x01 /* cmd done */

#define AWI_INT_BITS "\20\1CMD\2RX\3TX\4SCAN\5CFPST\6DTIM\7CFPE\10GROGGY"

/*
 * The following are used to implement a locking protocol between host
 * and MAC to protect the interrupt status and mask fields.
 *
 * driver: read lockout_host byte; if zero, set lockout_mac to non-zero,
 *	then reread lockout_host byte; if still zero, host has lock.
 *	if non-zero, clear lockout_mac, loop.
 */

#define AWI_LOCKOUT_MAC	0x4f5
#define AWI_LOCKOUT_HOST	0x4f6


#define AWI_INTSTAT2		0x4f7
#define AWI_INTMASK2		0x4fd

/* Bits in AWI_INTSTAT2/INTMASK2 */
#define AWI_INT2_RXMGT		0x80 		/* mgt/ps recieved */
#define AWI_INT2_RXDATA	0x40 		/* data received */
#define AWI_INT2_TXMGT		0x10		/* mgt tx done */
#define AWI_INT2_TXCF		0x08f		/* CF tx done */
#define AWI_INT2_TXPS		0x04		/* PS tx done */
#define AWI_INT2_TXBCAST	0x02		/* Broadcast tx done */
#define AWI_INT2_TXDATA	0x01		/* data tx done */

#define AWI_DIS_PWRDN		0x4fc		/* disable powerdown if set */

#define AWI_DRIVERSTATE	0x4fe		/* driver state */

#define AWI_DRV_STATEMASK		0x0f

#define AWI_DRV_RESET			0x0
#define AWI_DRV_INFSY			0x1 /* inf synced */
#define AWI_DRV_ADHSC			0x2 /* adhoc scan */
#define AWI_DRV_ADHSY			0x3 /* adhoc synced */
#define AWI_DRV_INFSC			0x4 /* inf scanning */
#define AWI_DRV_INFAUTH		0x5 /* inf authed */
#define AWI_DRV_INFASSOC		0x6 /* inf associated */
#define AWI_DRV_INFTOSS		0x7 /* inf handoff */
#define AWI_DRV_APNONE			0x8 /* AP activity: no assoc */
#define AWI_DRV_APQUIET		0xc /* AP: >=one assoc, no traffic */
#define AWI_DRV_APLO			0xd /* AP: >=one assoc, light tfc */
#define AWI_DRV_APMED			0xe /* AP: >=one assoc, mod tfc */
#define AWI_DRV_APHIGH			0xf /* AP: >=one assoc, heavy tfc */

#define AWI_DRV_AUTORXLED			0x10
#define AWI_DRV_AUTOTXLED			0x20
#define AWI_DRV_RXLED				0x40
#define AWI_DRV_TXLED				0x80

#define AWI_VBM		0x500	/* Virtual Bit Map */

#define AWI_BUFFERS		0x600	/* Buffers */

/*
 * Receive descriptors; there are a linked list of these chained
 * through the "NEXT" fields, starting from XXX
 */

#define AWI_RXD_SIZE		0x18

#define AWI_RXD_NEXT		0x4
#define AWI_RXD_NEXT_LAST	0x80000000


#define AWI_RXD_HOST_DESC_STATE	0x9

#define AWI_RXD_ST_OWN		0x80 /* host owns this */
#define AWI_RXD_ST_CONSUMED	0x40 /* host is done */
#define AWI_RXD_ST_LF		0x20 /* last frag */
#define AWI_RXD_ST_CRC		0x08 /* CRC error */
#define AWI_RXD_ST_OFLO	0x02 /* possible buffer overrun */
#define AWI_RXD_ST_RXERROR	0x01 /* this frame is borked; discard me */

#define AWI_RXD_ST_BITS	"\20\1ERROR\2OVERRUN\4CRC\6LF\7CONSUMED\10OWN"

#define AWI_RXD_RSSI		0xa /* 1 byte: radio strength indicator */
#define AWI_RXD_INDEX		0xb /* 1 byte: FH hop index or DS channel */
#define AWI_RXD_LOCALTIME	0xc /* 4 bytes: local time of RX */
#define AWI_RXD_START_FRAME	0x10 /* 4 bytes: ptr to first received byte */
#define AWI_RXD_LEN		0x14 /* 2 bytes: rx len in bytes */
#define AWI_RXD_RATE		0x16 /* 1 byte: rx rate in 1e5 bps */

/*
 * Transmit descriptors.
 */

#define AWI_TXD_SIZE		0x18

#define AWI_TXD_START		0x00 /* pointer to start of frame */
#define AWI_TXD_NEXT		0x04 /* pointer to next TXD */
#define AWI_TXD_LENGTH		0x08 /* length of frame */
#define AWI_TXD_STATE		0x0a /* state */

#define AWI_TXD_ST_OWN			0x80 /* MAC owns this  */
#define AWI_TXD_ST_DONE		0x40 /* MAC is done */
#define AWI_TXD_ST_REJ			0x20 /* MAC doesn't like */
#define AWI_TXD_ST_MSDU		0x10 /* MSDU timeout */
#define AWI_TXD_ST_ABRT		0x08 /* TX aborted */
#define AWI_TXD_ST_RETURNED		0x04 /* TX returned */
#define AWI_TXD_ST_RETRY		0x02 /* TX retries exceeded */
#define AWI_TXD_ST_ERROR		0x01 /* TX error */

#define AWI_TXD_RATE		0x0b /* rate */

#define AWI_RATE_1MBIT			10
#define AWI_RATE_2MBIT			20

#define AWI_TXD_NDA		0x0c /* num DIFS attempts */
#define AWI_TXD_NDF		0x0d /* num DIFS failures */
#define AWI_TXD_NSA		0x0e /* num SIFS attempts */
#define AWI_TXD_NSF		0x0f /* num SIFS failures */

#define AWI_TXD_NRA		0x14 /* num RTS attempts */
#define AWI_TXD_NDTA		0x15 /* num data attempts */
#define AWI_TXD_CTL		0x16 /* control */

#define AWI_TXD_CTL_PSN	0x80	/* preserve sequence in MAC frame */
#define AWI_TXD_CTL_BURST	0x02    /* host is doing 802.11 fragmt. */
#define AWI_TXD_CTL_FRAGS	0x01    /* override normal fragmentation */

/*
 * MIB structures.
 */

/*
 * MIB 0: Local MIB
 */

#define AWI_MIB_LOCAL_NOFRAG		0
#define AWI_MIB_LOCAL_NOPLCP		1
#define AWI_MIB_LOCAL_MACPRES		2
#define AWI_MIB_LOCAL_RXMGTQ		3
#define AWI_MIB_LOCAL_NOREASM		4
#define AWI_MIB_LOCAL_NOSTRIPPLCP	5
#define AWI_MIB_LOCAL_NORXERROR	6
#define AWI_MIB_LOCAL_NOPWRSAVE	7

#define AWI_MIB_LOCAL_FILTMULTI	8
#define AWI_MIB_LOCAL_NOSEQCHECK	9
#define AWI_MIB_LOCAL_CFPENDFLUSHCFPQ	10
#define AWI_MIB_LOCAL_INFRA_MODE	11
#define AWI_MIB_LOCAL_PWD_LEVEL	12
#define AWI_MIB_LOCAL_CFPMODE		13

#define AWI_MIB_LOCAL_TXB_OFFSET		14
#define AWI_MIB_LOCAL_TXB_SIZE		18
#define AWI_MIB_LOCAL_RXB_OFFSET		22
#define AWI_MIB_LOCAL_RXB_SIZE		26

#define AWI_MIB_LOCAL_ACTING_AS_AP	30
#define AWI_MIB_LOCAL_FILL_CFP		31
#define AWI_MIB_LOCAL_SIZE		32

/*
 * MAC mib 
 */

#define AWI_MIB_MAC_RTS_THRESH		4	 /* 2 bytes */
#define AWI_MIB_MAC_CW_MAX		6
#define AWI_MIB_MAC_CW_MIN		8
#define AWI_MIB_MAC_PROMISC		10
#define AWI_MIB_MAC_SHORT_RETRY	16
#define AWI_MIB_MAC_LONG_RETRY		17
#define AWI_MIB_MAC_MAX_FRAME		18
#define AWI_MIB_MAC_MAX_FRAG		20
#define AWI_MIB_MAC_PROBE_DELAY	22
#define AWI_MIB_MAC_PROBE_RESP_MIN	24
#define AWI_MIB_MAC_PROBE_RESP_MAX	26
#define AWI_MIB_MAC_MAX_TX_MSDU_LIFE	28
#define AWI_MIB_MAC_MAX_RX_MSDU_LIFE	32
#define AWI_MIB_MAC_STATION_BASE_RATE	36
#define AWI_MIB_MAC_DES_ESSID		38		/* 34 bytes */

/*
 * MGT mib.
 */

#define AWI_MIB_MGT_POWER_MODE		0
#define AWI_MIB_MGT_SCAN_MODE		1
#define AWI_MIB_MGT_SCAN_STATE		2
#define AWI_MIB_MGT_DTIM_PERIOD	3
#define AWI_MIB_MGT_ATIM_WINDOW	4
#define AWI_MIB_MGT_WEPREQ		6
#define AWI_MIB_MGT_BEACON_PD		8
#define AWI_MIB_MGT_PASSIVE_SCAN	10
#define AWI_MIB_MGT_LISTEN_INT		12
#define AWI_MIB_MGT_MEDIUP_OCC		14
#define AWI_MIB_MGT_MAX_MPDU_TIME	16
#define AWI_MIB_MGT_CFP_MAX_DUR	18
#define AWI_MIB_MGT_CFP_RATE		20
#define AWI_MIB_MGT_NO_DTMS		21
#define AWI_MIB_MGT_STATION_ID		22
#define AWI_MIB_MGT_BSS_ID		24
#define AWI_MIB_MGT_ESS_ID		30 		/* 34 bytes */
#define AWI_MIB_MGT_ESS_SIZE		34


/*
 * MAC address group.
 */

#define AWI_MIB_MAC_ADDR_MINE		0
#define AWI_MIB_MAC_ADDR_MULTI0	6
#define AWI_MIB_MAC_ADDR_MULTI1	12
#define AWI_MIB_MAC_ADDR_MULTI2	18
#define AWI_MIB_MAC_ADDR_MULTI3	24

#define AWI_MIB_MAC_ADDR_TXEN		30

/*
 * 802.11 media layer goo.
 * Should be split out into separate module independant of this driver.
 */

#define IEEEWL_FC		0 		/* frame control */

#define IEEEWL_FC_VERS		0
#define IEEEWL_FC_VERS_MASK	0x03

#define IEEEWL_FC_TYPE_MGT	0
#define IEEEWL_FC_TYPE_CTL	1
#define IEEEWL_FC_TYPE_DATA	2

#define IEEEWL_FC_TYPE_MASK	0x0c
#define IEEEWL_FC_TYPE_SHIFT	2

#define IEEEWL_FC_SUBTYPE_MASK	0xf0
#define IEEEWL_FC_SUBTYPE_SHIFT	4

#define IEEEWL_SUBTYPE_ASSOCREQ		0x00
#define IEEEWL_SUBTYPE_ASSOCRESP	0x01
#define IEEEWL_SUBTYPE_REASSOCREQ	0x02
#define IEEEWL_SUBTYPE_REASSOCRESP	0x03
#define IEEEWL_SUBTYPE_PROBEREQ		0x04
#define IEEEWL_SUBTYPE_PROBERESP	0x05

#define IEEEWL_SUBTYPE_BEACON		0x08
#define IEEEWL_SUBTYPE_DISSOC		0x0a
#define IEEEWL_SUBTYPE_AUTH		0x0b
#define IEEEWL_SUBTYPE_DEAUTH		0x0c

#define IEEEWL_FC2		1		/* second byte of fc */

/*
 * TLV tags for things we care about..
 */
#define IEEEWL_MGT_TLV_SSID		0
#define IEEEWL_MGT_TLV_FHPARMS		2

/*
 * misc frame control bits in second byte of frame control word.
 * there are others, but we don't ever want to set them..
 */

#define IEEEWL_FC2_DSMASK		0x03

#define IEEEWL_FC2_TODS			0x01
#define IEEEWL_FC2_FROMDS		0x02

#define IEEEWL_FH_CHANSET_MIN		1
#define IEEEWL_FH_CHANSET_MAX		3
#define IEEEWL_FH_PATTERN_MIN		0
#define IEEEWL_FH_PATTERN_MAX		77

struct awi_mac_header 
{
	u_int8_t	awi_fc;
	u_int8_t	awi_f2;
	u_int16_t	awi_duration;
	u_int8_t	awi_addr1[6];
	u_int8_t	awi_addr2[6];
	u_int8_t	awi_addr3[6];
	u_int16_t	awi_seqctl;
};

struct awi_llc_header
{
	u_int8_t	awi_llc_goo[8];
};

struct awi_assoc_hdr 
{
	u_int8_t	awi_cap_info[2];
	u_int8_t	awi_li[2];
};

struct awi_auth_hdr 
{
	u_int8_t	awi_algno[2];
	u_int8_t	awi_seqno[2];
	u_int8_t	awi_status[2];		
};

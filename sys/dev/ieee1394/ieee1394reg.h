/*	$OpenBSD: ieee1394reg.h,v 1.4 2003/01/12 12:01:33 tdeval Exp $	*/
/*	$NetBSD: ieee1394reg.h,v 1.12 2002/02/27 05:07:25 jmc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef	_DEV_IEEE1394_IEEE1394REG_H_
#define	_DEV_IEEE1394_IEEE1394REG_H_

#include <dev/std/ieee1212reg.h>

/*
 * Transaction Codes (Table 6-9)
 */
#define	IEEE1394_TCODE_WRITE_REQUEST_QUADLET	0
#define	IEEE1394_TCODE_WRITE_REQUEST_DATABLOCK	1
#define	IEEE1394_TCODE_WRITE_RESPONSE		2
#define	IEEE1394_TCODE_RESERVED_3		3
#define	IEEE1394_TCODE_READ_REQUEST_QUADLET	4
#define	IEEE1394_TCODE_READ_REQUEST_DATABLOCK	5
#define	IEEE1394_TCODE_READ_RESPONSE_QUADLET	6
#define	IEEE1394_TCODE_READ_RESPONSE_DATABLOCK	7
#define	IEEE1394_TCODE_CYCLE_START		8
#define	IEEE1394_TCODE_LOCK_REQUEST		9
#define	IEEE1394_TCODE_ISOCHRONOUS_DATABLOCK	10
#define	IEEE1394_TCODE_LOCK_RESPONSE		11
#define	IEEE1394_TCODE_RESERVED_12		12
#define	IEEE1394_TCODE_RESERVED_13		13
#define	IEEE1394_TCODE_RESERVED_14		14
#define	IEEE1394_TCODE_RESERVED_15		15

/*
 * Extended transaction codes (Table 6-10)
 */
#define	IEEE1394_XTCODE_RESERVED_0		P1212_LOCK_RESERVED_0
#define	IEEE1394_XTCODE_MASK_SWAP		P1212_LOCK_MASK_SWAP
#define	IEEE1394_XTCODE_COMPARE_SWAP		P1212_LOCK_COMPARE_SWAP
#define	IEEE1394_XTCODE_FETCH_ADD		P1212_LOCK_FETCH_ADD
#define	IEEE1394_XTCODE_LITTLE_ADD		P1212_LOCK_LITTLE_ADD
#define	IEEE1394_XTCODE_BOUNDED_ADD		P1212_LOCK_BOUNDED_ADD
#define	IEEE1394_XTCODE_WRAP_ADD		P1212_LOCK_WRAP_ADD
#define	IEEE1394_XTCODE_VENDOR_DEPENDENT	P1212_LOCK_VENDOR_DEPENDENT
/*
 * 0x0008 .. 0xFFFF are reserved.
 */

/*
 * Response codes (Table 6-11)
 */
#define	IEEE1394_RCODE_COMPLETE			0
#define	IEEE1394_RCODE_RESERVED_1		1
#define	IEEE1394_RCODE_RESERVED_2		2
#define	IEEE1394_RCODE_RESERVED_3		3
#define	IEEE1394_RCODE_CONFLICT_ERROR		4
#define	IEEE1394_RCODE_DATA_ERROR		5
#define	IEEE1394_RCODE_TYPE_ERROR		6
#define	IEEE1394_RCODE_ADDRESS_ERROR		7
#define	IEEE1394_RCODE_RESERVED_8		8
#define	IEEE1394_RCODE_RESERVED_9		9
#define	IEEE1394_RCODE_RESERVED_10		10
#define	IEEE1394_RCODE_RESERVED_11		11
#define	IEEE1394_RCODE_RESERVED_12		12
#define	IEEE1394_RCODE_RESERVED_13		13
#define	IEEE1394_RCODE_RESERVED_14		14
#define	IEEE1394_RCODE_RESERVED_15		15

#define	IEEE1394_TAG_UNFORMATTED		0
#define	IEEE1394_TAG_RESERVED_1			1
#define	IEEE1394_TAG_RESERVED_2			2
#define	IEEE1394_TAG_RESERVED_3			3

#define	IEEE1394_ACK_RESERVED_0			0
#define	IEEE1394_ACK_COMPLETE			1
#define	IEEE1394_ACK_PENDING			2
#define	IEEE1394_ACK_RESERVED_3			3
#define	IEEE1394_ACK_BUSY_X			4
#define	IEEE1394_ACK_BUSY_A			5
#define	IEEE1394_ACK_BUSY_B			6
#define	IEEE1394_ACK_RESERVED_7			7
#define	IEEE1394_ACK_RESERVED_8			8
#define	IEEE1394_ACK_RESERVED_9			9
#define	IEEE1394_ACK_RESERVED_10		10
#define	IEEE1394_ACK_RESERVED_11		11
#define	IEEE1394_ACK_RESERVED_12		12
#define	IEEE1394_ACK_DATA_ERROR			13
#define	IEEE1394_ACK_TYPE_ERROR			14
#define	IEEE1394_ACK_RESERVED_15		15

/*
 * PHY packet types.
 */
#define	IEEE1394_PHY_TYPE_MASK			0xC0000000
#define	IEEE1394_PHY_TYPE_BITPOS		30
#define	IEEE1394_PHY_SELF_ID			0x80000000
#define	IEEE1394_PHY_LINK_ON			0x40000000
#define	IEEE1394_PHY_CONFIG			0x00000000
#define	IEEE1394_PHY_ID_MASK			0x3F000000
#define	IEEE1394_PHY_ID_BITPOS			24

/*
 * Link-On PHY Packet Fields.
 */
/* There is no other field than the PHY_ID. */

/*
 * Configuration PHY Packet Fields.
 */
#define	IEEE1394_CONFIG_FORCE_ROOT		0x00800000
#define	IEEE1394_CONFIG_SET_GAPCNT		0x00400000
#define	IEEE1394_CONFIG_GAPCNT_MASK		0x003F0000
#define	IEEE1394_CONFIG_GAPCNT_BITPOS		16

/*
 * Self-ID PHY Packet Fields.
 */
#define	IEEE1394_SELFID_EXTENDED		0x00800000
#define	IEEE1394_SELFID_LINK_ACTIVE		0x00400000
#define	IEEE1394_SELFID_CONTENDER		0x00000800
#define	IEEE1394_SELFID_INITIATED_RESET		0x00000002
#define	IEEE1394_SELFID_MORE_PACKETS		0x00000001
#define	IEEE1394_SELFID_GAPCNT_MASK		0x003F0000
#define	IEEE1394_SELFID_GAPCNT_BITPOS		16
#define	IEEE1394_SELFID_SPEED_MASK		0x0000C000
#define	IEEE1394_SELFID_SPEED_BITPOS		14
#define	IEEE1394_SELFID_DELAY_MASK		0x00003000
#define	IEEE1394_SELFID_DELAY_BITPOS		12
#define	IEEE1394_SELFID_POWER_MASK		0x00000700
#define	IEEE1394_SELFID_POWER_BITPOS		8
#define	IEEE1394_SELFID_EXT_SEQ_MASK		0x00700000
#define	IEEE1394_SELFID_EXT_SEQ_BITPOS		20

/*
 * Node's port status.
 */
#define	IEEE1394_SELFID_PORT_STATUS(packet,port)			\
	(((packet) >> (16 - 2 * (port))) & 0x3)
#define	IEEE1394_PORT_NOT_PRESENT		0
#define	IEEE1394_PORT_NOT_CONNECTED		1
#define	IEEE1394_PORT_CONNECT_PARENT		2
#define	IEEE1394_PORT_CONNECT_CHILD		3

/*
 * Defined IEEE 1394 power classes.
 */
#define	IEEE1394_POW_NONE			0	/* No power feature. */
#define	IEEE1394_POW_SELF_15W			1	/* Provides 15W. */
#define	IEEE1394_POW_SELF_30W			2	/* Provides 30W. */
#define	IEEE1394_POW_SELF_45W			3	/* Provides 45W. */
#define	IEEE1394_POW_USES_1W			4	/* Uses up to 1W. */
#define	IEEE1394_POW_USES_2W			5	/* Uses up to 2W. */
#define	IEEE1394_POW_USES_5W			6	/* Uses up to 5W. */
#define	IEEE1394_POW_USES_9W			7	/* Uses up to 9W. */

#define	IEEE1394_POW_STRINGS	"None", "15W_Src", "30W_Src",	\
				"45W_Src", "1W_Sink", "2W_Sink",	\
				"5W_Sink", "9W_Sink"

/*
 * Defined IEEE 1394 speeds.
 */
#define	IEEE1394_SPD_S100			0	/* 1394-1995 */
#define	IEEE1394_SPD_S200			1	/* 1394-1995 */
#define	IEEE1394_SPD_S400			2	/* 1394-1995 */
#define	IEEE1394_SPD_S800			3	/* 1394b */
#define	IEEE1394_SPD_S1600			4	/* 1394b */
#define	IEEE1394_SPD_S3200			5	/* 1394b */
#define	IEEE1394_SPD_MAX			6

#define	IEEE1394_SPD_STRINGS	"100Mb/s", "200Mb/s", "400Mb/s", "800Mb/s", \
				"1.6Gb/s", "3.2Gb/s"

#if 0
typedef struct ieee1394_async_nodata {
	u_int32_t	an_header_crc;
} ieee1394_async_nodata __attribute((__packed__));
#endif

#define	IEEE1394_BCAST_PHY_ID			0x3f
#define	IEEE1394_ISOCH_MASK			0x3f

/*
 * Signature
 */
#define	IEEE1394_SIGNATURE			0x31333934

/*
 * Tag value
 */
#define	IEEE1394_TAG_GASP			0x3

/*
 * Control and Status Registers (IEEE1212 & IEEE1394)
 */
#define	CSR_BASE_HI				0x0000ffff
#define	CSR_BASE_LO				0xf0000000
#define	CSR_BASE				0x0000fffff0000000UL

#define	CSR_STATE_CLEAR				0x0000
#define	CSR_STATE_SET				0x0004
#define	CSR_NODE_IDS				0x0008
#define	CSR_RESET_START				0x000c
#define	CSR_INDIRECT_ADDRESS			0x0010
#define	CSR_INDIRECT_DATA			0x0014
#define	CSR_SPLIT_TIMEOUT_HI			0x0018
#define	CSR_SPLIT_TIMEOUT_LO			0x001c
#define	CSR_ARGUMENT_HI				0x0020
#define	CSR_ARGUMENT_LO				0x0024
#define	CSR_TEST_START				0x0028
#define	CSR_TEST_STATUS				0x002c
#define	CSR_INTERRUPT_TARGET			0x0050
#define	CSR_INTERRUPT_MASK			0x0054
#define	CSR_CLOCK_VALUE				0x0058
#define	CSR_CLOCK_PERIOD			0x005c
#define	CSR_CLOCK_STROBE_ARRIVED		0x0060
#define	CSR_CLOCK_INFO				0x0064
#define	CSR_MESSAGE_REQUEST			0x0080
#define	CSR_MESSAGE_RESPONSE			0x00c0

#define	CSR_SB_CYCLE_TIME			0x0200
#define	CSR_SB_BUS_TIME				0x0204
#define	CSR_SB_POWER_FAIL_IMMINENT		0x0208
#define	CSR_SB_POWER_SOURCE			0x020c
#define	CSR_SB_BUSY_TIMEOUT			0x0210
#define	CSR_SB_PRIORITY_BUDGET_HI		0x0214
#define	CSR_SB_PRIORITY_BUDGET_LO		0x0218
#define	CSR_SB_BUS_MANAGER_ID			0x021c
#define	CSR_SB_BANDWIDTH_AVAILABLE		0x0220
#define	CSR_SB_CHANNEL_AVAILABLE_HI		0x0224
#define	CSR_SB_CHANNEL_AVAILABLE_LO		0x0228
#define	CSR_SB_MAINT_CONTROL			0x022c
#define	CSR_SB_MAINT_UTILITY			0x0230
#define	CSR_SB_BROADCAST_CHANNEL		0x0234

#define	CSR_CONFIG_ROM				0x0400

#define	CSR_SB_OUTPUT_MASTER_PLUG		0x0900
#define	CSR_SB_OUTPUT_PLUG			0x0904
#define	CSR_SB_INPUT_MASTER_PLUG		0x0980
#define	CSR_SB_INPUT_PLUG			0x0984
#define	CSR_SB_FCP_COMMAND_FRAME		0x0b00
#define	CSR_SB_FCP_RESPONSE_FRAME		0x0d00
#define	CSR_SB_TOPOLOGY_MAP			0x1000
#define	CSR_SB_END				0x1400

#define	IEEE1394_MAX_REC(i)			(0x1 << ((i) + 1))
#define	IEEE1394_MAX_ASYNC(i)			(0x200 << (i))
#define	IEEE1394_BUSINFO_LEN			3

#define	IEEE1394_GET_MAX_REC(i)			(((i) & 0x0000f000) >> 12)
#define	IEEE1394_GET_LINK_SPD(i)		((i) & 0x00000007)

/*
 * XXX. Should be at if_fw level but needed here for constructing the config
 * rom. An interface for if_fw to send up a config rom should be done (probably
 * in the p1212 routines.
 */

#define	FW_FIFO_HI				0x2000
#define	FW_FIFO_LO				0x00000000

#endif	/* _DEV_IEEE1394_IEEE1394REG_H_ */

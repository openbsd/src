/*	$OpenBSD: ar5212var.h,v 1.1 2005/02/19 16:58:00 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
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
 * Specific definitions for the Atheros AR5001 Wireless LAN chipset
 * (AR5212/AR5311).
 */

#ifndef _AR5K_AR5212_VAR_H
#define _AR5K_AR5212_VAR_H

#include <dev/ic/ar5xxx.h>

/*
 * Define a "magic" code for the AR5212 (the HAL layer wants it)
 */

#define AR5K_AR5212_MAGIC		0x0000145b /* 5212 */
#define AR5K_AR5212_TX_NUM_QUEUES	10

#if BYTE_ORDER == BIG_ENDIAN
#define AR5K_AR5212_INIT_CFG	(					\
	AR5K_AR5212_CFG_SWTD | AR5K_AR5212_CFG_SWTB |			\
	AR5K_AR5212_CFG_SWRD | AR5K_AR5212_CFG_SWRB |			\
	AR5K_AR5212_CFG_SWRG						\
)
#else
#define AR5K_AR5212_INIT_CFG	0x00000000
#endif

/*
 * Internal RX/TX descriptor structures
 * (rX: reserved fields possibily used by future versions of the ar5k chipset)
 */

struct ar5k_ar5212_rx_desc {
	/*
	 * rx_control_0
	 */
	u_int32_t	reserved:32;

	/*
	 * rx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	reserved_12:1;
	u_int32_t	inter_req:1;
	u_int32_t	reserved_14_31:18;
} __packed;

struct ar5k_ar5212_rx_status {
	/*
	 * rx_status_0
	 */
	u_int32_t	data_len:12;
	u_int32_t	more:1;
	u_int32_t	decomp_crc_error:1;
	u_int32_t	reserved_14:1;
	u_int32_t	receive_rate:5;
	u_int32_t	receive_sig_strength:8;
	u_int32_t	receive_antenna:4;

	/*
	 * rx_status_1
	 */
	u_int32_t	done:1;
	u_int32_t	frame_receive_ok:1;
	u_int32_t	crc_error:1;
	u_int32_t	decrypt_crc_error:1;
	u_int32_t	phy_error:1;
	u_int32_t	mic_error:1;
	u_int32_t	reserved_6_7:2;
	u_int32_t	key_index_valid:1;
	u_int32_t	key_index:7;
	u_int32_t	receive_timestamp:15;
	u_int32_t	key_cache_miss:1;
} __packed;

struct ar5k_ar5212_rx_error {
	/*
	 * rx_error_0
	 */
	u_int32_t	reserved:32;

	/*
	 * rx_error_1
	 */
	u_int32_t	reserved_1_7:8;
	u_int32_t	phy_error_code:8;
	u_int32_t	reserved_16_31:16;
} __packed;

#define AR5K_AR5212_DESC_RX_PHY_ERROR_NONE		0x00
#define AR5K_AR5212_DESC_RX_PHY_ERROR_TIMING		0x20
#define AR5K_AR5212_DESC_RX_PHY_ERROR_PARITY		0x40
#define AR5K_AR5212_DESC_RX_PHY_ERROR_RATE		0x60
#define AR5K_AR5212_DESC_RX_PHY_ERROR_LENGTH		0x80
#define AR5K_AR5212_DESC_RX_PHY_ERROR_64QAM		0xa0
#define AR5K_AR5212_DESC_RX_PHY_ERROR_SERVICE		0xc0
#define AR5K_AR5212_DESC_RX_PHY_ERROR_TRANSMITOVR	0xe0

struct ar5k_ar5212_tx_desc {
	/*
	 * tx_control_0
	 */
	u_int32_t	frame_len:12;
	u_int32_t	reserved_12_15:4;
	u_int32_t	xmit_power:6;
	u_int32_t	rts_cts_enable:1;
	u_int32_t	veol:1;
	u_int32_t	clear_dest_mask:1;
	u_int32_t	ant_mode_xmit:4;
	u_int32_t	inter_req:1;
	u_int32_t	encrypt_key_valid:1;
	u_int32_t	cts_enable:1;

	/*
	 * tx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	more:1;
	u_int32_t	encrypt_key_index:7;
	u_int32_t	frame_type:4;
	u_int32_t	no_ack:1;
	u_int32_t	comp_proc:2;
	u_int32_t	comp_iv_len:2;
	u_int32_t	comp_icv_len:2;
	u_int32_t	reserved_31:1;

	/*
	 * tx_control_2
	 */
	u_int32_t	rts_duration:15;
	u_int32_t	duration_update_enable:1;
	u_int32_t	xmit_tries0:4;
	u_int32_t	xmit_tries1:4;
	u_int32_t	xmit_tries2:4;
	u_int32_t	xmit_tries3:4;

	/*
	 * tx_control_3
	 */
	u_int32_t	xmit_rate0:5;
	u_int32_t	xmit_rate1:5;
	u_int32_t	xmit_rate2:5;
	u_int32_t	xmit_rate3:5;
	u_int32_t	rts_cts_rate:5;
	u_int32_t	reserved_25_31:7;
} __packed;

#define AR5K_AR5212_DESC_TX_XMIT_RATE_6		0xb
#define AR5K_AR5212_DESC_TX_XMIT_RATE_9		0xf
#define AR5K_AR5212_DESC_TX_XMIT_RATE_12	0xa
#define AR5K_AR5212_DESC_TX_XMIT_RATE_18	0xe
#define AR5K_AR5212_DESC_TX_XMIT_RATE_24	0x9
#define AR5K_AR5212_DESC_TX_XMIT_RATE_36	0xd
#define AR5K_AR5212_DESC_TX_XMIT_RATE_48	0x8
#define AR5K_AR5212_DESC_TX_XMIT_RATE_54	0xc

#define AR5K_AR5212_DESC_TX_FRAME_TYPE_NORMAL	0x00
#define AR5K_AR5212_DESC_TX_FRAME_TYPE_ATIM	0x04
#define AR5K_AR5212_DESC_TX_FRAME_TYPE_PSPOLL	0x08
#define AR5K_AR5212_DESC_TX_FRAME_TYPE_NO_DELAY 0x0c
#define AR5K_AR5212_DESC_TX_FRAME_TYPE_PIFS	0x10

struct ar5k_ar5212_tx_status {
	/*
	 * tx_status_0
	 */
	u_int32_t	frame_xmit_ok:1;
	u_int32_t	excessive_retries:1;
	u_int32_t	fifo_underrun:1;
	u_int32_t	filtered:1;
	u_int32_t	rts_fail_count:4;
	u_int32_t	data_fail_count:4;
	u_int32_t	virt_coll_count:4;
	u_int32_t	send_timestamp:16;

	/*
	 * tx_status_1
	 */
	u_int32_t	done:1;
	u_int32_t	seq_num:12;
	u_int32_t	ack_sig_strength:8;
	u_int32_t	final_ts_index:2;
	u_int32_t	comp_success:1;
	u_int32_t	xmit_antenna:1;
	u_int32_t	reserved_25_31:7;
} __packed;

/*
 * Public function prototypes
 */
extern ar5k_attach_t ar5k_ar5212_attach;

/*
 * Initial register values which have to be loaded into the
 * card at boot time and after each reset.
 */

struct ar5k_ar5212_ini {
	u_int8_t	ini_flags;
	u_int16_t	ini_register;
	u_int32_t	ini_value;

#define AR5K_INI_FLAG_511X	0x00
#define	AR5K_INI_FLAG_5111	0x01
#define AR5K_INI_FLAG_5112	0x02
#define AR5K_INI_FLAG_BOTH	(AR5K_INI_FLAG_5111 | AR5K_INI_FLAG_5112)
};

#define AR5K_AR5212_INI {						\
	{ AR5K_INI_FLAG_BOTH, 0x000c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0034, 0x00000005 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0040, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0044, 0x00000008 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0048, 0x00000008 },			\
	{ AR5K_INI_FLAG_BOTH, 0x004c, 0x00000010 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0050, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0054, 0x0000001f },			\
	{ AR5K_INI_FLAG_BOTH, 0x0800, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0804, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0808, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x080c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0810, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0814, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0818, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x081c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0820, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x0824, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1230, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1270, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1038, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1078, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x10b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x10f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1138, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1178, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x11b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x11f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1238, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1278, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x12b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x12f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1338, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1378, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x13b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x13f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1438, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1478, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x14b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x14f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1538, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1578, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x15b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x15f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1638, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1678, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x16b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x16f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1738, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x1778, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x17b8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x17f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x103c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x107c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x10bc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x10fc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x113c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x117c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x11bc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x11fc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x123c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x127c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x12bc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x12fc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x133c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x137c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x13bc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x13fc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x143c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x147c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8004, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8008, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x800c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8018, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8020, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8024, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8028, 0x00000030 },			\
	{ AR5K_INI_FLAG_BOTH, 0x802c, 0x0007ffff },			\
	{ AR5K_INI_FLAG_BOTH, 0x8030, 0x01ffffff },			\
	{ AR5K_INI_FLAG_BOTH, 0x8034, 0x00000031 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8038, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x803c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8048, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8054, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8058, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x805c, 0xffffc7ff },			\
	{ AR5K_INI_FLAG_BOTH, 0x8080, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8084, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8088, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x808c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8090, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8094, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8098, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80c0, 0x2a82301a },			\
	{ AR5K_INI_FLAG_BOTH, 0x80c4, 0x05dc01e0 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80c8, 0x1f402710 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80cc, 0x01f40000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80d0, 0x00001e1c },			\
	{ AR5K_INI_FLAG_BOTH, 0x80d4, 0x0002aaaa },			\
	{ AR5K_INI_FLAG_BOTH, 0x80d8, 0x02005555 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80dc, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80e0, 0xffffffff },			\
	{ AR5K_INI_FLAG_BOTH, 0x80e4, 0x0000ffff },			\
	{ AR5K_INI_FLAG_BOTH, 0x80e8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80ec, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80f0, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80f4, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80f8, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x80fc, 0x00000088 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8700, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8704, 0x0000008c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8708, 0x000000e4 },			\
	{ AR5K_INI_FLAG_BOTH, 0x870c, 0x000002d5 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8710, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8714, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8718, 0x000000a0 },			\
	{ AR5K_INI_FLAG_BOTH, 0x871c, 0x000001c9 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8720, 0x0000002c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8724, 0x0000002c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8728, 0x00000030 },			\
	{ AR5K_INI_FLAG_BOTH, 0x872c, 0x0000003c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8730, 0x0000002c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8734, 0x0000002c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8738, 0x00000030 },			\
	{ AR5K_INI_FLAG_BOTH, 0x873c, 0x0000003c },			\
	{ AR5K_INI_FLAG_BOTH, 0x8740, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8744, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8748, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x874c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8750, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8754, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8758, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x875c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8760, 0x000000d5 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8764, 0x000000df },			\
	{ AR5K_INI_FLAG_BOTH, 0x8768, 0x00000102 },			\
	{ AR5K_INI_FLAG_BOTH, 0x876c, 0x0000013a },			\
	{ AR5K_INI_FLAG_BOTH, 0x8770, 0x00000075 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8774, 0x0000007f },			\
	{ AR5K_INI_FLAG_BOTH, 0x8778, 0x000000a2 },			\
	{ AR5K_INI_FLAG_BOTH, 0x877c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8100, 0x00010002 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8104, 0x00000001 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8108, 0x000000c0 },			\
	{ AR5K_INI_FLAG_BOTH, 0x810c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8110, 0x00000168 },			\
	{ AR5K_INI_FLAG_BOTH, 0x8114, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87c0, 0x03020100 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87c4, 0x07060504 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87c8, 0x0b0a0908 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87cc, 0x0f0e0d0c },			\
	{ AR5K_INI_FLAG_BOTH, 0x87d0, 0x13121110 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87d4, 0x17161514 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87d8, 0x1b1a1918 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87dc, 0x1f1e1d1c },			\
	{ AR5K_INI_FLAG_BOTH, 0x87e0, 0x03020100 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87e4, 0x07060504 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87e8, 0x0b0a0908 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87ec, 0x0f0e0d0c },			\
	{ AR5K_INI_FLAG_BOTH, 0x87f0, 0x13121110 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87f4, 0x17161514 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87f8, 0x1b1a1918 },			\
	{ AR5K_INI_FLAG_BOTH, 0x87fc, 0x1f1e1d1c },			\
	/* PHY registers */						\
	{ AR5K_INI_FLAG_BOTH, 0x9808, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x980c, 0xad848e19 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9810, 0x7d28e000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9814, 0x9c0a9f6b },			\
	{ AR5K_INI_FLAG_BOTH, 0x981c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x982c, 0x00022ffe },			\
	{ AR5K_INI_FLAG_BOTH, 0x983c, 0x00020100 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9840, 0x206a017a },			\
	{ AR5K_INI_FLAG_BOTH, 0x984c, 0x1284613c },			\
	{ AR5K_INI_FLAG_BOTH, 0x9854, 0x00000859 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9900, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9904, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9908, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x990c, 0x00800000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9910, 0x00000001 },			\
	{ AR5K_INI_FLAG_BOTH, 0x991c, 0x00000c80 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9920, 0x05100000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9928, 0x00000001 },			\
	{ AR5K_INI_FLAG_BOTH, 0x992c, 0x00000004 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9934, 0x1e1f2022 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9938, 0x0a0b0c0d },			\
	{ AR5K_INI_FLAG_BOTH, 0x993c, 0x0000003f },			\
	{ AR5K_INI_FLAG_BOTH, 0x9940, 0x00000004 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9948, 0x9280b212 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9954, 0x5d50e188 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9958, 0x000000ff },			\
	{ AR5K_INI_FLAG_BOTH, 0x995c, 0x004b6a8e },			\
	{ AR5K_INI_FLAG_BOTH, 0x9968, 0x000003ce },			\
	{ AR5K_INI_FLAG_BOTH, 0x9970, 0x192fb515 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9974, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9978, 0x00000001 },			\
	{ AR5K_INI_FLAG_BOTH, 0x997c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa184, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa188, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa18c, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa190, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa194, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa198, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa19c, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1a0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1a4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1a8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1ac, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1b0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1b4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1b8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1bc, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1c0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1c4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1c8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1cc, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1d0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1d4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1d8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1dc, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1e0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1e4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1e8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1ec, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1f0, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1f4, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1f8, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa1fc, 0x10ff10ff },			\
	{ AR5K_INI_FLAG_BOTH, 0xa210, 0x00806333 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa214, 0x00106c10 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa218, 0x009c4060 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa21c, 0x1883800a },			\
	{ AR5K_INI_FLAG_BOTH, 0xa220, 0x018830c6 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa224, 0x00000400 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa228, 0x000001b5 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa22c, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa234, 0x20202020 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa238, 0x20202020 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa23c, 0x13c889af },			\
	{ AR5K_INI_FLAG_BOTH, 0xa240, 0x38490a20 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa244, 0x00007bb6 },			\
	{ AR5K_INI_FLAG_BOTH, 0xa248, 0x0fff3ffc },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b00, 0x00000000 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b28, 0x0000000c },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b38, 0x00000012 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b64, 0x00000021 },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b8c, 0x0000002d },			\
	{ AR5K_INI_FLAG_BOTH, 0x9b9c, 0x00000033 },			\
	/* AR5111 specific */						\
	{ AR5K_INI_FLAG_5111, 0x9930, 0x00004883 },			\
	{ AR5K_INI_FLAG_5111, 0xa204, 0x00000000 },			\
	{ AR5K_INI_FLAG_5111, 0xa208, 0xd03e6788 },			\
	{ AR5K_INI_FLAG_5111, 0x9b04, 0x00000020 },			\
	{ AR5K_INI_FLAG_5111, 0x9b08, 0x00000010 },			\
	{ AR5K_INI_FLAG_5111, 0x9b0c, 0x00000030 },			\
	{ AR5K_INI_FLAG_5111, 0x9b10, 0x00000008 },			\
	{ AR5K_INI_FLAG_5111, 0x9b14, 0x00000028 },			\
	{ AR5K_INI_FLAG_5111, 0x9b18, 0x00000004 },			\
	{ AR5K_INI_FLAG_5111, 0x9b1c, 0x00000024 },			\
	{ AR5K_INI_FLAG_5111, 0x9b20, 0x00000014 },			\
	{ AR5K_INI_FLAG_5111, 0x9b24, 0x00000034 },			\
	{ AR5K_INI_FLAG_5111, 0x9b2c, 0x0000002c },			\
	{ AR5K_INI_FLAG_5111, 0x9b30, 0x00000002 },			\
	{ AR5K_INI_FLAG_5111, 0x9b34, 0x00000022 },			\
	{ AR5K_INI_FLAG_5111, 0x9b3c, 0x00000032 },			\
	{ AR5K_INI_FLAG_5111, 0x9b40, 0x0000000a },			\
	{ AR5K_INI_FLAG_5111, 0x9b44, 0x0000002a },			\
	{ AR5K_INI_FLAG_5111, 0x9b48, 0x00000006 },			\
	{ AR5K_INI_FLAG_5111, 0x9b4c, 0x00000026 },			\
	{ AR5K_INI_FLAG_5111, 0x9b50, 0x00000016 },			\
	{ AR5K_INI_FLAG_5111, 0x9b54, 0x00000036 },			\
	{ AR5K_INI_FLAG_5111, 0x9b58, 0x0000000e },			\
	{ AR5K_INI_FLAG_5111, 0x9b5c, 0x0000002e },			\
	{ AR5K_INI_FLAG_5111, 0x9b60, 0x00000001 },			\
	{ AR5K_INI_FLAG_5111, 0x9b68, 0x00000011 },			\
	{ AR5K_INI_FLAG_5111, 0x9b6c, 0x00000031 },			\
	{ AR5K_INI_FLAG_5111, 0x9b70, 0x00000009 },			\
	{ AR5K_INI_FLAG_5111, 0x9b74, 0x00000029 },			\
	{ AR5K_INI_FLAG_5111, 0x9b78, 0x00000005 },			\
	{ AR5K_INI_FLAG_5111, 0x9b7c, 0x00000025 },			\
	{ AR5K_INI_FLAG_5111, 0x9b80, 0x00000015 },			\
	{ AR5K_INI_FLAG_5111, 0x9b84, 0x00000035 },			\
	{ AR5K_INI_FLAG_5111, 0x9b88, 0x0000000d },			\
	{ AR5K_INI_FLAG_5111, 0x9b90, 0x00000003 },			\
	{ AR5K_INI_FLAG_5111, 0x9b94, 0x00000023 },			\
	{ AR5K_INI_FLAG_5111, 0x9b98, 0x00000013 },			\
	{ AR5K_INI_FLAG_5111, 0x9ba0, 0x0000000b },			\
	{ AR5K_INI_FLAG_5111, 0x9ba4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9ba8, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bac, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bb0, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bb4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bb8, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bbc, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bc0, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bc4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bc8, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bcc, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bd0, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bd4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bd8, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bdc, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9be0, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9be4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9be8, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bec, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bf0, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bf4, 0x0000002b },			\
	{ AR5K_INI_FLAG_5111, 0x9bf8, 0x00000002 },			\
	{ AR5K_INI_FLAG_5111, 0x9bfc, 0x00000016 },			\
	/* AR5112 specific */						\
	{ AR5K_INI_FLAG_5112, 0x9930, 0x00004882 },			\
	{ AR5K_INI_FLAG_5112, 0x9b04, 0x00000001 },			\
	{ AR5K_INI_FLAG_5112, 0x9b08, 0x00000002 },			\
	{ AR5K_INI_FLAG_5112, 0x9b0c, 0x00000003 },			\
	{ AR5K_INI_FLAG_5112, 0x9b10, 0x00000004 },			\
	{ AR5K_INI_FLAG_5112, 0x9b14, 0x00000005 },			\
	{ AR5K_INI_FLAG_5112, 0x9b18, 0x00000008 },			\
	{ AR5K_INI_FLAG_5112, 0x9b1c, 0x00000009 },			\
	{ AR5K_INI_FLAG_5112, 0x9b20, 0x0000000a },			\
	{ AR5K_INI_FLAG_5112, 0x9b24, 0x0000000b },			\
	{ AR5K_INI_FLAG_5112, 0x9b2c, 0x0000000d },			\
	{ AR5K_INI_FLAG_5112, 0x9b30, 0x00000010 },			\
	{ AR5K_INI_FLAG_5112, 0x9b34, 0x00000011 },			\
	{ AR5K_INI_FLAG_5112, 0x9b3c, 0x00000013 },			\
	{ AR5K_INI_FLAG_5112, 0x9b40, 0x00000014 },			\
	{ AR5K_INI_FLAG_5112, 0x9b44, 0x00000015 },			\
	{ AR5K_INI_FLAG_5112, 0x9b48, 0x00000018 },			\
	{ AR5K_INI_FLAG_5112, 0x9b4c, 0x00000019 },			\
	{ AR5K_INI_FLAG_5112, 0x9b50, 0x0000001a },			\
	{ AR5K_INI_FLAG_5112, 0x9b54, 0x0000001b },			\
	{ AR5K_INI_FLAG_5112, 0x9b58, 0x0000001c },			\
	{ AR5K_INI_FLAG_5112, 0x9b5c, 0x0000001d },			\
	{ AR5K_INI_FLAG_5112, 0x9b60, 0x00000020 },			\
	{ AR5K_INI_FLAG_5112, 0x9b68, 0x00000022 },			\
	{ AR5K_INI_FLAG_5112, 0x9b6c, 0x00000023 },			\
	{ AR5K_INI_FLAG_5112, 0x9b70, 0x00000024 },			\
	{ AR5K_INI_FLAG_5112, 0x9b74, 0x00000025 },			\
	{ AR5K_INI_FLAG_5112, 0x9b78, 0x00000028 },			\
	{ AR5K_INI_FLAG_5112, 0x9b7c, 0x00000029 },			\
	{ AR5K_INI_FLAG_5112, 0x9b80, 0x0000002a },			\
	{ AR5K_INI_FLAG_5112, 0x9b84, 0x0000002b },			\
	{ AR5K_INI_FLAG_5112, 0x9b88, 0x0000002c },			\
	{ AR5K_INI_FLAG_5112, 0x9b90, 0x00000030 },			\
	{ AR5K_INI_FLAG_5112, 0x9b94, 0x00000031 },			\
	{ AR5K_INI_FLAG_5112, 0x9b98, 0x00000032 },			\
	{ AR5K_INI_FLAG_5112, 0x9ba0, 0x00000034 },			\
	{ AR5K_INI_FLAG_5112, 0x9ba4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9ba8, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bac, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bb0, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bb4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bb8, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bbc, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bc0, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bc4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bc8, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bcc, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bd0, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bd4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bd8, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bdc, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9be0, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9be4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9be8, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bec, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bf0, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bf4, 0x00000035 },			\
	{ AR5K_INI_FLAG_5112, 0x9bf8, 0x00000010 },			\
	{ AR5K_INI_FLAG_5112, 0x9bfc, 0x0000001a },			\
}

struct ar5k_ar5212_ini_mode {
	u_int16_t	mode_register;
	u_int8_t	mode_flags;
	u_int32_t	mode_value[2][5];

#define AR5K_INI_PHY_5111	0
#define AR5K_INI_PHY_5112	1
#define AR5K_INI_PHY_511X	1
};

#define AR5K_AR5212_INI_MODE {							\
	{ 0x0030, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00008015, 0x00008015, 0x00008015, 0x00008015, 0x00008015 }	\
	} },									\
	{ 0x1040, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1044, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1048, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x104c, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1050, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1054, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1058, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x105c, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1060, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1064, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x002ffc0f, 0x002ffc0f, 0x002ffc1f, 0x002ffc0f, 0x002ffc0f }	\
	} },									\
	{ 0x1030, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000230, 0x000001e0, 0x000000b0, 0x00000160, 0x000001e0 }	\
	} },									\
	{ 0x1070, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000168, 0x000001e0, 0x000001b8, 0x0000018c, 0x000001e0 }	\
	} },									\
	{ 0x10b0, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000e60, 0x00001180, 0x00001f1c, 0x00003e38, 0x00001180 }	\
	} },									\
	{ 0x10f0, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x0000a0e0, 0x00014068, 0x00005880, 0x0000b0e0, 0x00014068 }	\
	} },									\
	{ 0x8014, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x03e803e8, 0x06e006e0, 0x04200420, 0x08400840, 0x06e006e0 }	\
	} },									\
	{ 0x9804, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000000, 0x00000003, 0x00000000, 0x00000000, 0x00000003 }	\
	} },									\
	{ 0x9820, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x02020200, 0x02020200, 0x02010200, 0x02020200, 0x02020200 }	\
	} },									\
	{ 0x9834, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000e0e, 0x00000e0e, 0x00000e0e, 0x00000e0e, 0x00000e0e }	\
	} },									\
	{ 0x9838, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000007, 0x00000007, 0x0000000b, 0x0000000b, 0x0000000b }	\
	} },									\
	{ 0x9844, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x1372161c, 0x13721c25, 0x13721728, 0x137216a2, 0x13721c25 }	\
	} },									\
	{ 0x9850, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x0de8b4e0, 0x0de8b4e0, 0x0de8b4e0, 0x0de8b4e0, 0x0de8b4e0 }	\
	} },									\
	{ 0x9860, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00009d10, 0x00009d10, 0x00009d18, 0x00009d18, 0x00009d10 }	\
	} },									\
	{ 0x9864, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x0001ce00, 0x0001ce00, 0x0001ce00, 0x0001ce00, 0x0001ce00 }	\
	} },									\
	{ 0x9868, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x409a4190, 0x409a4190, 0x409a4190, 0x409a4190, 0x409a4190 }	\
	} },									\
	{ 0x9918, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x000001b8, 0x000001b8, 0x00000084, 0x00000108, 0x000001b8 }	\
	} },									\
	{ 0x9924, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x10058a05, 0x10058a05, 0x10058a05, 0x10058a05, 0x10058a05 }	\
	} },									\
	{ 0xa180, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x10ff14ff, 0x10ff14ff, 0x10ff10ff, 0x10ff19ff, 0x10ff19ff }	\
	} },									\
	{ 0xa230, AR5K_INI_FLAG_511X, {						\
		{ 0, },								\
		{ 0x00000000, 0x00000000, 0x00000000, 0x00000108, 0x00000000 }	\
	} },									\
	{ 0x801c, AR5K_INI_FLAG_BOTH, {						\
		{ 0x128d8fa7, 0x09880fcf, 0x04e00f95, 0x128d8fab, 0x09880fcf },	\
		{ 0x128d93a7, 0x098813cf, 0x04e01395, 0x128d93ab, 0x098813cf }	\
	} },									\
	{ 0x9824, AR5K_INI_FLAG_BOTH, {						\
		{ 0x00000e0e, 0x00000e0e, 0x00000707, 0x00000e0e, 0x00000e0e },	\
		{ 0x00000e0e, 0x00000e0e, 0x00000e0e, 0x00000e0e, 0x00000e0e }	\
	} },									\
	{ 0x9828, AR5K_INI_FLAG_BOTH, {						\
		{ 0x0a020001, 0x0a020001, 0x05010100, 0x0a020001, 0x0a020001 },	\
		{ 0x0a020001, 0x0a020001, 0x05020100, 0x0a020001, 0x0a020001 }	\
	} },									\
	{ 0x9848, AR5K_INI_FLAG_BOTH, {						\
		{ 0x0018da5a, 0x0018da5a, 0x0018ca69, 0x0018ca69, 0x0018ca69 },	\
		{ 0x0018da6d, 0x0018da6d, 0x0018ca75, 0x0018ca75, 0x0018ca75 }	\
	} },									\
	{ 0x9858, AR5K_INI_FLAG_BOTH, {						\
		{ 0x7e800d2e, 0x7e800d2e, 0x7ee84d2e, 0x7ee84d2e, 0x7e800d2e },	\
		{ 0x7e800d2e, 0x7e800d2e, 0x7ee80d2e, 0x7ee80d2e, 0x7e800d2e }	\
	} },									\
	{ 0x985c, AR5K_INI_FLAG_BOTH, {						\
		{ 0x3137665e, 0x3137665e, 0x3137665e, 0x3137665e, 0x3137615e },	\
		{ 0x3137665e, 0x3137665e, 0x3137665e, 0x3137665e, 0x3137665e }	\
	} },									\
	{ 0x986c, AR5K_INI_FLAG_BOTH, {						\
		{ 0x050cb081, 0x050cb081, 0x050cb081, 0x050cb080, 0x050cb080 },	\
		{ 0x050cb081, 0x050cb081, 0x050cb081, 0x050cb081, 0x050cb081 }	\
	} },									\
	{ 0x9914, AR5K_INI_FLAG_BOTH, {						\
		{ 0x00002710, 0x00002710, 0x0000157c, 0x00002af8, 0x00002710 },	\
		{ 0x000007d0, 0x000007d0, 0x0000044c, 0x00000898, 0x000007d0 }	\
	} },									\
	{ 0x9944, AR5K_INI_FLAG_BOTH, {						\
		{ 0xf7b81020, 0xf7b81020, 0xf7b80d20, 0xf7b81020, 0xf7b81020 },	\
		{ 0xf7b81020, 0xf7b81020, 0xf7b80d10, 0xf7b81010, 0xf7b81010 }	\
	} },									\
	{ 0xa204, AR5K_INI_FLAG_5112, {						\
		{ 0, },								\
		{ 0x00000000, 0x00000000, 0x00000004, 0x00000004, 0x00000004 }	\
	} },									\
	{ 0xa208, AR5K_INI_FLAG_5112, {						\
		{ 0, },								\
		{ 0xd6be6788, 0xd6be6788, 0xd03e6788, 0xd03e6788, 0xd03e6788 }	\
	} },									\
	{ 0xa20c, AR5K_INI_FLAG_BOTH, {						\
		{ 0x642c416a, 0x642c416a, 0x6440416a, 0x6440416a, 0x6440416a },	\
		{ 0x642c0140, 0x642c0140, 0x6442c160, 0x6442c160, 0x6442c160 }	\
	} },									\
}

struct ar5k_ar5212_ini_rfgain {
	u_int16_t	rfg_register;
	u_int32_t	rfg_value[2][2];

#define AR5K_INI_RFGAIN_5GHZ	0
#define AR5K_INI_RFGAIN_2GHZ	1
};

#define AR5K_AR5212_INI_RFGAIN	{						\
	{ 0x9a00, {								\
		{ 0x000001a9, 0x00000000 }, { 0x00000007, 0x00000007 } } },	\
	{ 0x9a04, {								\
		{ 0x000001e9, 0x00000040 }, { 0x00000047, 0x00000047 } } },	\
	{ 0x9a08, {								\
		{ 0x00000029, 0x00000080 }, { 0x00000087, 0x00000087 } } },	\
	{ 0x9a0c, {								\
		{ 0x00000069, 0x00000150 }, { 0x000001a0, 0x000001a0 } } },	\
	{ 0x9a10, {								\
		{ 0x00000199, 0x00000190 }, { 0x000001e0, 0x000001e0 } } },	\
	{ 0x9a14, {								\
		{ 0x000001d9, 0x000001d0 }, { 0x00000020, 0x00000020 } } },	\
	{ 0x9a18, {								\
		{ 0x00000019, 0x00000010 }, { 0x00000060, 0x00000060 } } },	\
	{ 0x9a1c, {								\
		{ 0x00000059, 0x00000044 }, { 0x000001a1, 0x000001a1 } } },	\
	{ 0x9a20, {								\
		{ 0x00000099, 0x00000084 }, { 0x000001e1, 0x000001e1 } } },	\
	{ 0x9a24, {								\
		{ 0x000001a5, 0x00000148 }, { 0x00000021, 0x00000021 } } },	\
	{ 0x9a28, {								\
		{ 0x000001e5, 0x00000188 }, { 0x00000061, 0x00000061 } } },	\
	{ 0x9a2c, {								\
		{ 0x00000025, 0x000001c8 }, { 0x00000162, 0x00000162 } } },	\
	{ 0x9a30, {								\
		{ 0x000001c8, 0x00000014 }, { 0x000001a2, 0x000001a2 } } },	\
	{ 0x9a34, {								\
		{ 0x00000008, 0x00000042 }, { 0x000001e2, 0x000001e2 } } },	\
	{ 0x9a38, {								\
		{ 0x00000048, 0x00000082 }, { 0x00000022, 0x00000022 } } },	\
	{ 0x9a3c, {								\
		{ 0x00000088, 0x00000178 }, { 0x00000062, 0x00000062 } } },	\
	{ 0x9a40, {								\
		{ 0x00000198, 0x000001b8 }, { 0x00000163, 0x00000163 } } },	\
	{ 0x9a44, {								\
		{ 0x000001d8, 0x000001f8 }, { 0x000001a3, 0x000001a3 } } },	\
	{ 0x9a48, {								\
		{ 0x00000018, 0x00000012 }, { 0x000001e3, 0x000001e3 } } },	\
	{ 0x9a4c, {								\
		{ 0x00000058, 0x00000052 }, { 0x00000023, 0x00000023 } } },	\
	{ 0x9a50, {								\
		{ 0x00000098, 0x00000092 }, { 0x00000063, 0x00000063 } } },	\
	{ 0x9a54, {								\
		{ 0x000001a4, 0x0000017c }, { 0x00000184, 0x00000184 } } },	\
	{ 0x9a58, {								\
		{ 0x000001e4, 0x000001bc }, { 0x000001c4, 0x000001c4 } } },	\
	{ 0x9a5c, {								\
		{ 0x00000024, 0x000001fc }, { 0x00000004, 0x00000004 } } },	\
	{ 0x9a60, {								\
		{ 0x00000064, 0x0000000a }, { 0x000001ea, 0x0000000b } } },	\
	{ 0x9a64, {								\
		{ 0x000000a4, 0x0000004a }, { 0x0000002a, 0x0000004b } } },	\
	{ 0x9a68, {								\
		{ 0x000000e4, 0x0000008a }, { 0x0000006a, 0x0000008b } } },	\
	{ 0x9a6c, {								\
		{ 0x0000010a, 0x0000015a }, { 0x000000aa, 0x000001ac } } },	\
	{ 0x9a70, {								\
		{ 0x0000014a, 0x0000019a }, { 0x000001ab, 0x000001ec } } },	\
	{ 0x9a74, {								\
		{ 0x0000018a, 0x000001da }, { 0x000001eb, 0x0000002c } } },	\
	{ 0x9a78, {								\
		{ 0x000001ca, 0x0000000e }, { 0x0000002b, 0x00000012 } } },	\
	{ 0x9a7c, {								\
		{ 0x0000000a, 0x0000004e }, { 0x0000006b, 0x00000052 } } },	\
	{ 0x9a80, {								\
		{ 0x0000004a, 0x0000008e }, { 0x000000ab, 0x00000092 } } },	\
	{ 0x9a84, {								\
		{ 0x0000008a, 0x0000015e }, { 0x000001ac, 0x00000193 } } },	\
	{ 0x9a88, {								\
		{ 0x000001ba, 0x0000019e }, { 0x000001ec, 0x000001d3 } } },	\
	{ 0x9a8c, {								\
		{ 0x000001fa, 0x000001de }, { 0x0000002c, 0x00000013 } } },	\
	{ 0x9a90, {								\
		{ 0x0000003a, 0x00000009 }, { 0x0000003a, 0x00000053 } } },	\
	{ 0x9a94, {								\
		{ 0x0000007a, 0x00000049 }, { 0x0000007a, 0x00000093 } } },	\
	{ 0x9a98, {								\
		{ 0x00000186, 0x00000089 }, { 0x000000ba, 0x00000194 } } },	\
	{ 0x9a9c, {								\
		{ 0x000001c6, 0x00000179 }, { 0x000001bb, 0x000001d4 } } },	\
	{ 0x9aa0, {								\
		{ 0x00000006, 0x000001b9 }, { 0x000001fb, 0x00000014 } } },	\
	{ 0x9aa4, {								\
		{ 0x00000046, 0x000001f9 }, { 0x0000003b, 0x0000003a } } },	\
	{ 0x9aa8, {								\
		{ 0x00000086, 0x00000039 }, { 0x0000007b, 0x0000007a } } },	\
	{ 0x9aac, {								\
		{ 0x000000c6, 0x00000079 }, { 0x000000bb, 0x000000ba } } },	\
	{ 0x9ab0, {								\
		{ 0x000000c6, 0x000000b9 }, { 0x000001bc, 0x000001bb } } },	\
	{ 0x9ab4, {								\
		{ 0x000000c6, 0x000001bd }, { 0x000001fc, 0x000001fb } } },	\
	{ 0x9ab8, {								\
		{ 0x000000c6, 0x000001fd }, { 0x0000003c, 0x0000003b } } },	\
	{ 0x9abc, {								\
		{ 0x000000c6, 0x0000003d }, { 0x0000007c, 0x0000007b } } },	\
	{ 0x9ac0, {								\
		{ 0x000000c6, 0x0000007d }, { 0x000000bc, 0x000000bb } } },	\
	{ 0x9ac4, {								\
		{ 0x000000c6, 0x000000bd }, { 0x000000fc, 0x000001bc } } },	\
	{ 0x9ac8, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000001fc } } },	\
	{ 0x9acc, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x0000003c } } },	\
	{ 0x9ad0, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x0000007c } } },	\
	{ 0x9ad4, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000bc } } },	\
	{ 0x9ad8, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9adc, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9ae0, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9ae4, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9ae8, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9aec, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9af0, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9af4, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9af8, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
	{ 0x9afc, {								\
		{ 0x000000c6, 0x000000fd }, { 0x000000fc, 0x000000fc } } },	\
}

#endif /* _AR5K_AR5212_VAR_H */

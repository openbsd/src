/*	$OpenBSD: ar5210var.h,v 1.1 2004/11/02 03:01:16 reyk Exp $	*/

/*
 * Copyright (c) 2004 Reyk Floeter <reyk@vantronix.net>. 
 *
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY
 * SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Specific definitions for the Atheros AR5000 Wireless LAN chipset
 * (AR5210 + AR5110).
 */

#ifndef _AR5K_AR5210_VAR_H
#define _AR5K_AR5210_VAR_H

#include <dev/ic/ar5xxx.h>

/*
 * Define a "magic" code for the AR5210 (the HAL layer wants it)
 */

#define AR5K_AR5210_MAGIC		0x0000145a /* 5210 */
#define AR5K_AR5210_TX_NUM_QUEUES	2

#if BYTE_ORDER == BIG_ENDIAN
#define AR5K_AR5210_INIT_CFG 	(					\
	AR5K_AR5210_CFG_SWTD | AR5K_AR5210_CFG_SWTB | 			\
        AR5K_AR5210_CFG_SWRD | AR5K_AR5210_CFG_SWRB | 			\
        AR5K_AR5210_CFG_SWRG						\
)
#else
#define AR5K_AR5210_INIT_CFG	0x00000000
#endif


/*
 * Internal RX/TX descriptor structures
 * (rX: reserved fields possibily used by future versions of the ar5k chipset)
 */

struct ar5k_ar5210_rx_desc {
	/*
	 * First word
	 */
	u_int32_t	r1;

	/*
	 * Second word
	 */
	u_int32_t	buf_len:12;
	u_int32_t	r2:1;
	u_int32_t	inter_req:1;
	u_int32_t	r3:18;
} __attribute__ ((__packed__));

struct ar5k_ar5210_rx_status {
	/* 
	 * First word 
	 */
	u_int32_t	data_len:12;
	u_int32_t	more:1;
	u_int32_t	r1:1;
	u_int32_t	receive_antenna:1;
	u_int32_t	receive_rate:4;
	u_int32_t	receive_sig_strength:8;
	u_int32_t	r2:5;

	/*
	 * Second word
	 */
	u_int32_t	done:1;
	u_int32_t 	frame_receive_ok:1;
	u_int32_t 	crc_error:1;
	u_int32_t 	fifo_overrun:1;
	u_int32_t 	decrypt_crc_error:1;
	u_int32_t 	phy_error:3;
	u_int32_t 	key_index_valid:1;
	u_int32_t 	key_index:6;
	u_int32_t 	receive_timestamp:13;
	u_int32_t 	key_cache_miss:1;
	u_int32_t 	r3:3;
} __attribute__ ((__packed__));

#define AR5K_AR5210_DESC_RX_PHY_ERROR_NONE		0x00
#define AR5K_AR5210_DESC_RX_PHY_ERROR_TIMING		0x20
#define AR5K_AR5210_DESC_RX_PHY_ERROR_PARITY 		0x40
#define AR5K_AR5210_DESC_RX_PHY_ERROR_RATE 		0x60
#define AR5K_AR5210_DESC_RX_PHY_ERROR_LENGTH 		0x80
#define AR5K_AR5210_DESC_RX_PHY_ERROR_64QAM 		0xa0
#define AR5K_AR5210_DESC_RX_PHY_ERROR_SERVICE 		0xc0
#define AR5K_AR5210_DESC_RX_PHY_ERROR_TRANSMITOVR 	0xe0

struct ar5k_ar5210_tx_desc {
	/*
	 * First word
	 */
	u_int32_t 	frame_len:12;
	u_int32_t 	header_len:6;
	u_int32_t 	xmit_rate:4;
	u_int32_t 	rts_cts_enable:1;
	u_int32_t 	long_packet:1;
	u_int32_t 	clear_dest_mask:1;
	u_int32_t 	ant_mode_xmit:1;
	u_int32_t 	frame_type:3;
	u_int32_t 	inter_req:1;
	u_int32_t 	encrypt_key_valid:1;
	u_int32_t 	r1:1;
	
	/*
	 * Second word
	 */
	u_int32_t 	buf_len:12;
	u_int32_t 	more:1;
	u_int32_t 	encrypt_key_index:6;
	u_int32_t 	rts_duration:13;
} __attribute__ ((__packed__));

#define AR5K_AR5210_DESC_TX_XMIT_RATE_6 	0xb
#define AR5K_AR5210_DESC_TX_XMIT_RATE_9 	0xf
#define AR5K_AR5210_DESC_TX_XMIT_RATE_12	0xa
#define AR5K_AR5210_DESC_TX_XMIT_RATE_18 	0xe
#define AR5K_AR5210_DESC_TX_XMIT_RATE_24 	0x9
#define AR5K_AR5210_DESC_TX_XMIT_RATE_36 	0xd
#define AR5K_AR5210_DESC_TX_XMIT_RATE_48 	0x8
#define AR5K_AR5210_DESC_TX_XMIT_RATE_54 	0xc

#define AR5K_AR5210_DESC_TX_FRAME_TYPE_NORMAL 	0x00
#define AR5K_AR5210_DESC_TX_FRAME_TYPE_ATIM 	0x04
#define AR5K_AR5210_DESC_TX_FRAME_TYPE_PSPOLL 	0x08
#define AR5K_AR5210_DESC_TX_FRAME_TYPE_NO_DELAY 0x0c
#define AR5K_AR5210_DESC_TX_FRAME_TYPE_PIFS 	0x10

struct ar5k_ar5210_tx_status {
	/*
	 * First word
	 */
	u_int32_t 	frame_xmit_ok:1;
	u_int32_t 	excessive_retries:1;
	u_int32_t 	fifo_underrun:1;
	u_int32_t 	filtered:1;
	u_int32_t 	short_retry_count:4;
	u_int32_t 	long_retry_count:4;
	u_int32_t 	r1:4;
	u_int32_t 	send_timestamp:16;

	/*
	 * Second word
	 */
	u_int32_t 	done:1;
	u_int32_t 	seq_num:12;
	u_int32_t 	ack_sig_strength:8;
	u_int32_t 	r2:11;
} __attribute__ ((__packed__));

/*
 * Public function prototypes
 */
extern ar5k_attach_t ar5k_ar5210_attach;

/*
 * Initial mode settings ("Base Mode" or "Turbo Mode")
 */

#define AR5K_AR5210_INI_MODE(_aifs) { 					\
        { AR5K_AR5210_SLOT_TIME, 					\
          AR5K_INIT_SLOT_TIME, 						\
          AR5K_INIT_SLOT_TIME_TURBO }, 					\
        { AR5K_AR5210_SLOT_TIME, 					\
          AR5K_INIT_ACK_CTS_TIMEOUT, 					\
          AR5K_INIT_ACK_CTS_TIMEOUT_TURBO }, 				\
        { AR5K_AR5210_USEC, 						\
          AR5K_INIT_TRANSMIT_LATENCY, 					\
          AR5K_INIT_TRANSMIT_LATENCY_TURBO}, 				\
        { AR5K_AR5210_IFS0, 						\
          ((AR5K_INIT_SIFS + (_aifs) * AR5K_INIT_SLOT_TIME) 		\
              << AR5K_AR5210_IFS0_DIFS_S) | AR5K_INIT_SIFS, 		\
          ((AR5K_INIT_SIFS_TURBO + (_aifs) * AR5K_INIT_SLOT_TIME_TURBO) \
              << AR5K_AR5210_IFS0_DIFS_S) | AR5K_INIT_SIFS_TURBO }, 	\
        { AR5K_AR5210_IFS1, 						\
          AR5K_INIT_PROTO_TIME_CNTRL, 					\
          AR5K_INIT_PROTO_TIME_CNTRL_TURBO }, 				\
        { AR5K_AR5210_PHY(17), 						\
          (AR5K_REG_READ(AR5K_AR5210_PHY(17)) & ~0x7F) | 0x1C, 		\
          (AR5K_REG_READ(AR5K_AR5210_PHY(17)) & ~0x7F) | 0x38 }, 	\
        { AR5K_AR5210_PHY_FC, 						\
                                                                        \
          AR5K_AR5210_PHY_FC_SERVICE_ERR | 				\
          AR5K_AR5210_PHY_FC_TXURN_ERR | 				\
          AR5K_AR5210_PHY_FC_ILLLEN_ERR | 				\
          AR5K_AR5210_PHY_FC_ILLRATE_ERR | 				\
          AR5K_AR5210_PHY_FC_PARITY_ERR | 				\
          AR5K_AR5210_PHY_FC_TIMING_ERR | 0x1020, 			\
	                                                                \
          AR5K_AR5210_PHY_FC_SERVICE_ERR | 				\
          AR5K_AR5210_PHY_FC_TXURN_ERR | 				\
          AR5K_AR5210_PHY_FC_ILLLEN_ERR | 				\
          AR5K_AR5210_PHY_FC_ILLRATE_ERR | 				\
          AR5K_AR5210_PHY_FC_PARITY_ERR | 				\
          AR5K_AR5210_PHY_FC_TURBO_MODE | 				\
          AR5K_AR5210_PHY_FC_TURBO_SHORT |				\
          AR5K_AR5210_PHY_FC_TIMING_ERR | 0x2020 }, 			\
}

/*
 * Initial register values which have to be loaded into the
 * card at boot time and after each reset.
 */

#define AR5K_AR5210_INI { 						\
        /* PCU and MAC registers */					\
	{ AR5K_AR5210_TXDP0, 0 },					\
	{ AR5K_AR5210_TXDP1, 0 },					\
	{ AR5K_AR5210_RXDP, 0 },					\
	{ AR5K_AR5210_CR, 0 },						\
	{ AR5K_AR5210_ISR, 0, INI_READ },				\
	{ AR5K_AR5210_IMR, 0 },						\
	{ AR5K_AR5210_IER, AR5K_AR5210_IER_DISABLE },			\
	{ AR5K_AR5210_BSR, 0, INI_READ },				\
	{ AR5K_AR5210_TXCFG, AR5K_AR5210_DMASIZE_128B },		\
	{ AR5K_AR5210_RXCFG, AR5K_AR5210_DMASIZE_128B },		\
	{ AR5K_AR5210_CFG, AR5K_AR5210_INIT_CFG },			\
	{ AR5K_AR5210_TOPS, AR5K_INIT_TOPS },				\
	{ AR5K_AR5210_RXNOFRM, AR5K_INIT_RXNOFRM },			\
	{ AR5K_AR5210_RPGTO, AR5K_INIT_RPGTO },				\
	{ AR5K_AR5210_TXNOFRM, AR5K_INIT_TXNOFRM },			\
	{ AR5K_AR5210_SFR, 0 },						\
	{ AR5K_AR5210_MIBC, 0 },					\
	{ AR5K_AR5210_MISC, 0 },					\
	{ AR5K_AR5210_RX_FILTER, 0 },					\
	{ AR5K_AR5210_MCAST_FIL0, 0 },					\
	{ AR5K_AR5210_MCAST_FIL1, 0 },					\
	{ AR5K_AR5210_TX_MASK0, 0 },					\
	{ AR5K_AR5210_TX_MASK1, 0 },					\
	{ AR5K_AR5210_CLR_TMASK, 0 },					\
	{ AR5K_AR5210_TRIG_LVL, AR5K_TUNE_MIN_TX_FIFO_THRES },		\
	{ AR5K_AR5210_DIAG_SW, 0 },					\
	{ AR5K_AR5210_RSSI_THR, AR5K_TUNE_RSSI_THRES },			\
	{ AR5K_AR5210_TSF_L32, 0 },					\
	{ AR5K_AR5210_TIMER0, 0 },					\
	{ AR5K_AR5210_TIMER1, 0xffffffff },				\
	{ AR5K_AR5210_TIMER2, 0xffffffff },				\
	{ AR5K_AR5210_TIMER3, 1 },					\
	{ AR5K_AR5210_CFP_DUR, 0 },					\
	{ AR5K_AR5210_CFP_PERIOD, 0 },					\
        /* PHY registers */						\
	{ AR5K_AR5210_PHY(0), 0x00000047 },				\
	{ AR5K_AR5210_PHY_AGC, 0x00000000 },				\
	{ AR5K_AR5210_PHY(3), 0x09848ea6 },				\
	{ AR5K_AR5210_PHY(4), 0x3d32e000 },				\
	{ AR5K_AR5210_PHY(5), 0x0000076b },				\
	{ AR5K_AR5210_PHY_ACTIVE, AR5K_AR5210_PHY_DISABLE },		\
	{ AR5K_AR5210_PHY(8), 0x02020200 },				\
	{ AR5K_AR5210_PHY(9), 0x00000e0e },				\
	{ AR5K_AR5210_PHY(10), 0x0a020201 },				\
	{ AR5K_AR5210_PHY(11), 0x00036ffc },				\
	{ AR5K_AR5210_PHY(12), 0x00000000 },				\
	{ AR5K_AR5210_PHY(13), 0x00000e0e },				\
	{ AR5K_AR5210_PHY(14), 0x00000007 },				\
	{ AR5K_AR5210_PHY(15), 0x00020100 },				\
	{ AR5K_AR5210_PHY(16), 0x89630000 },				\
	{ AR5K_AR5210_PHY(17), 0x1372169c },				\
	{ AR5K_AR5210_PHY(18), 0x0018b633 },				\
	{ AR5K_AR5210_PHY(19), 0x1284613c },				\
	{ AR5K_AR5210_PHY(20), 0x0de8b8e0 },				\
	{ AR5K_AR5210_PHY(21), 0x00074859 },				\
	{ AR5K_AR5210_PHY(22), 0x7e80beba },				\
	{ AR5K_AR5210_PHY(23), 0x313a665e },				\
	{ AR5K_AR5210_PHY_AGCCTL, 0x00001d08 },				\
	{ AR5K_AR5210_PHY(25), 0x0001ce00 },				\
	{ AR5K_AR5210_PHY(26), 0x409a4190 },				\
	{ AR5K_AR5210_PHY(28), 0x0000000f },				\
	{ AR5K_AR5210_PHY(29), 0x00000080 },				\
	{ AR5K_AR5210_PHY(30), 0x00000004 },				\
	{ AR5K_AR5210_PHY(31), 0x00000018 }, /* 0x987c */		\
	{ AR5K_AR5210_PHY(64), 0x00000000 }, /* 0x9900 */		\
	{ AR5K_AR5210_PHY(65), 0x00000000 },				\
	{ AR5K_AR5210_PHY(66), 0x00000000 },				\
	{ AR5K_AR5210_PHY(67), 0x00800000 },				\
	{ AR5K_AR5210_PHY(68), 0x00000003 },				\
        /* BB gain table (64bytes) */					\
	{ AR5K_AR5210_BB_GAIN(0), 0x00000000 },			     	\
	{ AR5K_AR5210_BB_GAIN(0x01), 0x00000020 },			\
	{ AR5K_AR5210_BB_GAIN(0x02), 0x00000010 },			\
	{ AR5K_AR5210_BB_GAIN(0x03), 0x00000030 },			\
	{ AR5K_AR5210_BB_GAIN(0x04), 0x00000008 },			\
	{ AR5K_AR5210_BB_GAIN(0x05), 0x00000028 },			\
	{ AR5K_AR5210_BB_GAIN(0x06), 0x00000028 },			\
	{ AR5K_AR5210_BB_GAIN(0x07), 0x00000004 },			\
	{ AR5K_AR5210_BB_GAIN(0x08), 0x00000024 },			\
	{ AR5K_AR5210_BB_GAIN(0x09), 0x00000014 },			\
	{ AR5K_AR5210_BB_GAIN(0x0a), 0x00000034 },			\
	{ AR5K_AR5210_BB_GAIN(0x0b), 0x0000000c },			\
	{ AR5K_AR5210_BB_GAIN(0x0c), 0x0000002c },			\
	{ AR5K_AR5210_BB_GAIN(0x0d), 0x00000002 },			\
	{ AR5K_AR5210_BB_GAIN(0x0e), 0x00000022 },			\
	{ AR5K_AR5210_BB_GAIN(0x0f), 0x00000012 },			\
	{ AR5K_AR5210_BB_GAIN(0x10), 0x00000032 },			\
	{ AR5K_AR5210_BB_GAIN(0x11), 0x0000000a },			\
	{ AR5K_AR5210_BB_GAIN(0x12), 0x0000002a },			\
	{ AR5K_AR5210_BB_GAIN(0x13), 0x00000001 },			\
	{ AR5K_AR5210_BB_GAIN(0x14), 0x00000021 },			\
	{ AR5K_AR5210_BB_GAIN(0x15), 0x00000011 },			\
	{ AR5K_AR5210_BB_GAIN(0x16), 0x00000031 },			\
	{ AR5K_AR5210_BB_GAIN(0x17), 0x00000009 },			\
	{ AR5K_AR5210_BB_GAIN(0x18), 0x00000029 },			\
	{ AR5K_AR5210_BB_GAIN(0x19), 0x00000005 },			\
	{ AR5K_AR5210_BB_GAIN(0x1a), 0x00000025 },			\
	{ AR5K_AR5210_BB_GAIN(0x1b), 0x00000015 },			\
	{ AR5K_AR5210_BB_GAIN(0x1c), 0x00000035 },			\
	{ AR5K_AR5210_BB_GAIN(0x1d), 0x0000000d },			\
	{ AR5K_AR5210_BB_GAIN(0x1e), 0x0000002d },			\
	{ AR5K_AR5210_BB_GAIN(0x1f), 0x00000003 },			\
	{ AR5K_AR5210_BB_GAIN(0x20), 0x00000023 },			\
	{ AR5K_AR5210_BB_GAIN(0x21), 0x00000013 },			\
	{ AR5K_AR5210_BB_GAIN(0x22), 0x00000033 },			\
	{ AR5K_AR5210_BB_GAIN(0x23), 0x0000000b },			\
	{ AR5K_AR5210_BB_GAIN(0x24), 0x0000002b },			\
	{ AR5K_AR5210_BB_GAIN(0x25), 0x00000007 },			\
	{ AR5K_AR5210_BB_GAIN(0x26), 0x00000027 },			\
	{ AR5K_AR5210_BB_GAIN(0x27), 0x00000017 },			\
	{ AR5K_AR5210_BB_GAIN(0x28), 0x00000037 },			\
	{ AR5K_AR5210_BB_GAIN(0x29), 0x0000000f },			\
	{ AR5K_AR5210_BB_GAIN(0x2a), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x2b), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x2c), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x2d), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x2e), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x2f), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x30), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x31), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x32), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x33), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x34), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x35), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x36), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x37), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x38), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x39), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3a), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3b), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3c), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3d), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3e), 0x0000002f },			\
	{ AR5K_AR5210_BB_GAIN(0x3f), 0x0000002f },			\
        /* RF gain table (64bytes) */					\
	{ AR5K_AR5210_RF_GAIN(0), 0x0000001d },				\
	{ AR5K_AR5210_RF_GAIN(0x01), 0x0000005d },			\
	{ AR5K_AR5210_RF_GAIN(0x02), 0x0000009d },			\
	{ AR5K_AR5210_RF_GAIN(0x03), 0x000000dd },			\
	{ AR5K_AR5210_RF_GAIN(0x04), 0x0000011d },			\
	{ AR5K_AR5210_RF_GAIN(0x05), 0x00000021 },			\
	{ AR5K_AR5210_RF_GAIN(0x06), 0x00000061 },			\
	{ AR5K_AR5210_RF_GAIN(0x07), 0x000000a1 },			\
	{ AR5K_AR5210_RF_GAIN(0x08), 0x000000e1 },			\
	{ AR5K_AR5210_RF_GAIN(0x09), 0x00000031 },			\
	{ AR5K_AR5210_RF_GAIN(0x0a), 0x00000071 },			\
	{ AR5K_AR5210_RF_GAIN(0x0b), 0x000000b1 },			\
	{ AR5K_AR5210_RF_GAIN(0x0c), 0x0000001c },			\
	{ AR5K_AR5210_RF_GAIN(0x0d), 0x0000005c },			\
	{ AR5K_AR5210_RF_GAIN(0x0e), 0x00000029 },			\
	{ AR5K_AR5210_RF_GAIN(0x0f), 0x00000069 },			\
	{ AR5K_AR5210_RF_GAIN(0x10), 0x000000a9 },			\
	{ AR5K_AR5210_RF_GAIN(0x11), 0x00000020 },			\
	{ AR5K_AR5210_RF_GAIN(0x12), 0x00000019 },			\
	{ AR5K_AR5210_RF_GAIN(0x13), 0x00000059 },			\
	{ AR5K_AR5210_RF_GAIN(0x14), 0x00000099 },			\
	{ AR5K_AR5210_RF_GAIN(0x15), 0x00000030 },			\
	{ AR5K_AR5210_RF_GAIN(0x16), 0x00000005 },			\
	{ AR5K_AR5210_RF_GAIN(0x17), 0x00000025 },			\
	{ AR5K_AR5210_RF_GAIN(0x18), 0x00000065 },			\
	{ AR5K_AR5210_RF_GAIN(0x19), 0x000000a5 },			\
	{ AR5K_AR5210_RF_GAIN(0x1a), 0x00000028 },			\
	{ AR5K_AR5210_RF_GAIN(0x1b), 0x00000068 },			\
	{ AR5K_AR5210_RF_GAIN(0x1c), 0x0000001f },			\
	{ AR5K_AR5210_RF_GAIN(0x1d), 0x0000001e },			\
	{ AR5K_AR5210_RF_GAIN(0x1e), 0x00000018 },			\
	{ AR5K_AR5210_RF_GAIN(0x1f), 0x00000058 },			\
	{ AR5K_AR5210_RF_GAIN(0x20), 0x00000098 },			\
	{ AR5K_AR5210_RF_GAIN(0x21), 0x00000003 },			\
	{ AR5K_AR5210_RF_GAIN(0x22), 0x00000004 },			\
	{ AR5K_AR5210_RF_GAIN(0x23), 0x00000044 },			\
	{ AR5K_AR5210_RF_GAIN(0x24), 0x00000084 },			\
	{ AR5K_AR5210_RF_GAIN(0x25), 0x00000013 },			\
	{ AR5K_AR5210_RF_GAIN(0x26), 0x00000012 },			\
	{ AR5K_AR5210_RF_GAIN(0x27), 0x00000052 },			\
	{ AR5K_AR5210_RF_GAIN(0x28), 0x00000092 },			\
	{ AR5K_AR5210_RF_GAIN(0x29), 0x000000d2 },			\
	{ AR5K_AR5210_RF_GAIN(0x2a), 0x0000002b },			\
	{ AR5K_AR5210_RF_GAIN(0x2b), 0x0000002a },			\
	{ AR5K_AR5210_RF_GAIN(0x2c), 0x0000006a },			\
	{ AR5K_AR5210_RF_GAIN(0x2d), 0x000000aa },			\
	{ AR5K_AR5210_RF_GAIN(0x2e), 0x0000001b },			\
	{ AR5K_AR5210_RF_GAIN(0x2f), 0x0000001a },			\
	{ AR5K_AR5210_RF_GAIN(0x30), 0x0000005a },			\
	{ AR5K_AR5210_RF_GAIN(0x31), 0x0000009a },			\
	{ AR5K_AR5210_RF_GAIN(0x32), 0x000000da },			\
	{ AR5K_AR5210_RF_GAIN(0x33), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x34), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x35), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x36), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x37), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x38), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x39), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3a), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3b), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3c), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3d), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3e), 0x00000006 },			\
	{ AR5K_AR5210_RF_GAIN(0x3f), 0x00000006 },			\
        /* PHY activation */						\
	{ AR5K_AR5210_PHY(53), 0x00000020 },				\
	{ AR5K_AR5210_PHY(51), 0x00000004 },				\
	{ AR5K_AR5210_PHY(50), 0x00060106 },				\
	{ AR5K_AR5210_PHY(39), 0x0000006d },				\
	{ AR5K_AR5210_PHY(48), 0x00000000 },				\
	{ AR5K_AR5210_PHY(52), 0x00000014 },				\
	{ AR5K_AR5210_PHY_ACTIVE, AR5K_AR5210_PHY_ENABLE },		\
}

#endif /* _AR5K_AR5210_VAR_H */

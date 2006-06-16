/*	$OpenBSD: if_rumreg.h,v 1.1 2006/06/16 22:30:46 niallo Exp $  */
/*-
 * Copyright (c) 2005, 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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

#define RT2573_CONFIG_NO	1
#define RT2573_IFACE_INDEX	0

#define RT2573_FIRMWARE_RUN	0x01
#define RT2573_WRITE_MAC	0x02
#define RT2573_READ_MAC		0x03
#define RT2573_WRITE_MULTI_MAC	0x06
#define RT2573_READ_MULTI_MAC	0x07
#define RT2573_READ_EEPROM	0x09
#define RT2573_WRITE_LED	0x0a

#define RT2573_PHY_CSR0_RT71	0x3080	/* RF/PS control */
#define RT2573_PHY_CSR1_RT71	0x3084
#define RT2573_PHY_CSR2_RT71	0x3088	/* BBP Pre-Tx control */
#define RT2573_PHY_CSR3_RT71	0x308c	/* BBP access */
#define RT2573_PHY_CSR4_RT71	0x3090	/* RF serial control */
#define RT2573_PHY_CSR5_RT71	0x3094	/* Rx to Tx signal switch timing */
#define RT2573_PHY_CSR6_RT71	0x3098	/* Tx to Rx signal timing */
#define RT2573_PHY_CSR7_RT71	0x309c	/* Tx DAC switching timing */


#define RT2573_HOST_READY	(1 << 2)
#define RT2573_RESET_ASIC	(1 << 0)
#define RT2573_RESET_BBP	(1 << 1)

#define RT2573_ENABLE_TSF			(1 << 0)
#define RT2573_ENABLE_TSF_SYNC(x)		(((x) & 0x3) << 1)
#define RT2573_ENABLE_TBCN			(1 << 3)
#define RT2573_ENABLE_BEACON_GENERATOR	(1 << 4)

#define RT2573_RF_AWAKE	(3 << 7)
#define RT2573_BBP_AWAKE	(3 << 5)

#define RT2573_BBP_WRITE	(1 << 15)

#define RT2573_RF1_AUTOTUNE	0x08000
#define RT2573_RF3_AUTOTUNE	0x00040

#define RT2573_RF_2522	0x00
#define RT2573_RF_2523	0x01
#define RT2573_RF_2524	0x02
#define RT2573_RF_2525	0x03
#define RT2573_RF_2525E	0x04
#define RT2573_RF_2526	0x05
/* dual-band RF */
#define RT2573_RF_5222	0x10
#define RT2573_RF_2528	0x00

#define RT2573_BBP_VERSION	0
#define RT2573_BBP_TX	2
#define RT2573_BBP_RX	14

#define RT2573_BBP_ANTA		0x00
#define RT2573_BBP_DIVERSITY	0x01
#define RT2573_BBP_ANTB		0x02
#define RT2573_BBP_ANTMASK		0x03
#define RT2573_BBP_FLIPIQ		0x04

#define RT2573_JAPAN_FILTER	0x08

struct rum_tx_desc {
	uint32_t	flags;
#define RT2573_TX_RETRY(x)		((x) << 4)
#define RT2573_TX_MORE_FRAG	(1 << 8)
#define RT2573_TX_ACK		(1 << 9)
#define RT2573_TX_TIMESTAMP	(1 << 10)
#define RT2573_TX_OFDM		(1 << 11)
#define RT2573_TX_NEWSEQ		(1 << 12)

#define RT2573_TX_IFS_MASK		0x00006000
#define RT2573_TX_IFS_BACKOFF	(0 << 13)
#define RT2573_TX_IFS_SIFS		(1 << 13)
#define RT2573_TX_IFS_NEWBACKOFF	(2 << 13)
#define RT2573_TX_IFS_NONE		(3 << 13)

	uint16_t	wme;
#define RT2573_QID(v)           (v)
#define RT2573_AIFSN(v)         ((v) << 4)
#define RT2573_LOGCWMAX(x)		(((x) & 0xf) << 12)
#define RT2573_LOGCWMIN(x)		(((x) & 0xf) << 8)
#define RT2573_IVOFFSET(x)		(((x) & 0x3f))

	uint16_t	xflags;
	uint16_t	reserved;
	uint8_t		plcp_signal;
	uint8_t		plcp_service;
#define RT2573_PLCP_LENGEXT	0x80
#define RT2573_TX_HWSEQ         (1 << 12)

	uint8_t		plcp_length_lo;
	uint8_t		plcp_length_hi;
	uint32_t	iv;
	uint32_t	eiv;
	uint8_t		offset;
	uint8_t		qid;
#define RT2573_QID_MGT  13
} __packed;

struct rum_rx_desc {
	uint32_t	flags;
#define RT2573_RX_CRC_ERROR	(1 << 5)
#define RT2573_RX_OFDM		(1 << 6)
#define RT2573_RX_PHY_ERROR	(1 << 7)
#define RT2573_TX_BUSY          (1 << 0)
#define RT2573_TX_VALID         (1 << 1)
#define RT2573_TX_IFS           (1 << 6)
#define RT2573_TX_LONG_RETRY    (1 << 7)
#define RT2573_TX_BURST         (1 << 28)

	uint8_t		rate;
	uint8_t		rssi;
	uint16_t	reserved;

	uint32_t	iv;
	uint32_t	eiv;
} __packed;

#define RT2573_RF_LOBUSY	(1 << 15)
#define RT2573_RF_BUSY	(1 << 31)
#define RT2573_RF_20BIT	(20 << 24)

#define RT2573_RF1	0
#define RT2573_RF2	2
#define RT2573_RF3	1
#define RT2573_RF4	3

#define RT2573_EEPROM_MACBBP	0x0000
#define RT2573_EEPROM_ADDRESS	0x0004
#define RT2573_EEPROM_TXPOWER	0x003c
#define RT2573_EEPROM_CONFIG0	0x0016
#define RT2573_EEPROM_BBP_BASE	0x001c

#define RT2573_EEPROM_VERSION		0x0002
#define RT2573_EEPROM_ADDRESS		0x0004
#define RT2573_EEPROM_TXPOWER_RT70		0x003c
#define RT2573_EEPROM_TXPOWER_RT71		0x0046
#define RT2573_EEPROM_CONFIG0_RT70		0x0016
#define RT2573_EEPROM_CONFIG0_RT71		0x0020
#define RT2573_EEPROM_BBP_BASE_RT70	0x001c
#define RT2573_EEPROM_BBP_BASE_RT71	0x0026

#define RT2573_MCU_CODE_BASE    0x800
#define RT2573_TX_RING_COUNT    32
#define RT2573_MGT_RING_COUNT   32
#define RT2573_RX_RING_COUNT    64

#define RT2573_TX_DESC_SIZE     (sizeof (struct rum_tx_desc))
#define RT2573_TX_DESC_WSIZE    (RT2661_TX_DESC_SIZE / 4)
#define RT2573_RX_DESC_SIZE     (sizeof (struct rum_rx_desc))
#define RT2573_RX_DESC_WSIZE    (RT2661_RX_DESC_SIZE / 4)

#define RT2573_MAX_SCATTER      5


/*
 * Control and status registers.
 */
#define RT2573_HOST_CMD_CSR             0x0008
#define RT2573_MCU_CNTL_CSR             0x000c
#define RT2573_SOFT_RESET_CSR           0x0010
#define RT2573_MCU_INT_SOURCE_CSR       0x0014
#define RT2573_MCU_INT_MASK_CSR         0x0018
#define RT2573_PCI_USEC_CSR             0x001c
#define RT2573_H2M_MAILBOX_CSR          0x2100
#define RT2573_M2H_CMD_DONE_CSR         0x2104
#define RT2573_HW_BEACON_BASE0          0x2400
#define RT2573_MAC_CSR0                 0x3000
#define RT2573_MAC_CSR1                 0x3004
#define RT2573_MAC_CSR2                 0x3008
#define RT2573_MAC_CSR3                 0x300c
#define RT2573_MAC_CSR4                 0x3010
#define RT2573_MAC_CSR5                 0x3014
#define RT2573_MAC_CSR6                 0x3018
#define RT2573_MAC_CSR7                 0x301c
#define RT2573_MAC_CSR8                 0x3020
#define RT2573_MAC_CSR9                 0x3024
#define RT2573_MAC_CSR10                0x3028
#define RT2573_MAC_CSR11                0x302c
#define RT2573_MAC_CSR12                0x3030
#define RT2573_MAC_CSR13                0x3034
#define RT2573_MAC_CSR14                0x3038
#define RT2573_MAC_CSR15                0x303c
#define RT2573_TXRX_CSR0                0x3040
#define RT2573_TXRX_CSR1                0x3044
#define RT2573_TXRX_CSR2                0x3048
#define RT2573_TXRX_CSR3                0x304c
#define RT2573_TXRX_CSR4                0x3050
#define RT2573_TXRX_CSR5                0x3054
#define RT2573_TXRX_CSR6                0x3058
#define RT2573_TXRX_CSR7                0x305c
#define RT2573_TXRX_CSR8                0x3060
#define RT2573_TXRX_CSR9                0x3064
#define RT2573_TXRX_CSR10               0x3068
#define RT2573_TXRX_CSR11               0x306c
#define RT2573_TXRX_CSR12               0x3070
#define RT2573_TXRX_CSR13               0x3074
#define RT2573_TXRX_CSR14               0x3078
#define RT2573_TXRX_CSR15               0x307c
#define RT2573_PHY_CSR0                 0x3080
#define RT2573_PHY_CSR1                 0x3084
#define RT2573_PHY_CSR2                 0x3088
#define RT2573_PHY_CSR3                 0x308c
#define RT2573_PHY_CSR4                 0x3090
#define RT2573_PHY_CSR5                 0x3094
#define RT2573_PHY_CSR6                 0x3098
#define RT2573_PHY_CSR7                 0x309c
#define RT2573_SEC_CSR0                 0x30a0
#define RT2573_SEC_CSR1                 0x30a4
#define RT2573_SEC_CSR2                 0x30a8
#define RT2573_SEC_CSR3                 0x30ac
#define RT2573_SEC_CSR4                 0x30b0

#define RT2573_SEC_CSR5                 0x30b4
#define RT2573_STA_CSR0                 0x30c0
#define RT2573_STA_CSR1                 0x30c4
#define RT2573_STA_CSR2                 0x30c8
#define RT2573_STA_CSR3                 0x30cc
#define RT2573_STA_CSR4                 0x30d0
#define RT2573_AC0_BASE_CSR             0x3400
#define RT2573_AC1_BASE_CSR             0x3404
#define RT2573_AC2_BASE_CSR             0x3408
#define RT2573_AC3_BASE_CSR             0x340c
#define RT2573_MGT_BASE_CSR             0x3410
#define RT2573_TX_RING_CSR0             0x3418
#define RT2573_TX_RING_CSR1             0x341c
#define RT2573_AIFSN_CSR                0x3420
#define RT2573_CWMIN_CSR                0x3424
#define RT2573_CWMAX_CSR                0x3428
#define RT2573_TX_DMA_DST_CSR           0x342c
#define RT2573_TX_CNTL_CSR              0x3430
#define RT2573_LOAD_TX_RING_CSR         0x3434
#define RT2573_RX_BASE_CSR              0x3450
#define RT2573_RX_RING_CSR              0x3454
#define RT2573_RX_CNTL_CSR              0x3458
#define RT2573_PCI_CFG_CSR              0x3460
#define RT2573_INT_SOURCE_CSR           0x3468
#define RT2573_INT_MASK_CSR             0x346c
#define RT2573_E2PROM_CSR               0x3470
#define RT2573_AC_TXOP_CSR0             0x3474
#define RT2573_AC_TXOP_CSR1             0x3478
#define RT2573_TEST_MODE_CSR            0x3484
#define RT2573_IO_CNTL_CSR              0x3498


/* possible flags for register HOST_CMD_CSR */
#define RT2573_KICK_CMD         (1 << 7)
/* Host to MCU (8051) command identifiers */
#define RT2573_MCU_CMD_SLEEP    0x30
#define RT2573_MCU_CMD_WAKEUP   0x31
#define RT2573_MCU_SET_LED      0x50
#define RT2573_MCU_SET_RSSI_LED 0x52

/* possible flags for register MCU_CNTL_CSR */
#define RT2573_MCU_SEL          (1 << 0)
#define RT2573_MCU_RESET        (1 << 1)
#define RT2573_MCU_READY        (1 << 2)
/* possible flags for register MCU_INT_SOURCE_CSR */
#define RT2573_MCU_CMD_DONE             0xff
#define RT2573_MCU_WAKEUP               (1 << 8)
#define RT2573_MCU_BEACON_EXPIRE        (1 << 9)

/* possible flags for register H2M_MAILBOX_CSR */
#define RT2573_H2M_BUSY         (1 << 24)
#define RT2573_TOKEN_NO_INTR    0xff

/* possible flags for register MAC_CSR5 */
#define RT2573_ONE_BSSID        3

/* possible flags for register TXRX_CSR0 */
/* Tx filter flags are in the low 16 bits */
#define RT2573_AUTO_TX_SEQ      (1 << 15)
/* Rx filter flags are in the high 16 bits */
#define RT2573_DISABLE_RX       (1 << 16)
#define RT2573_DROP_CRC_ERROR   (1 << 17)
#define RT2573_DROP_PHY_ERROR   (1 << 18)
#define RT2573_DROP_CTL         (1 << 19)
#define RT2573_DROP_NOT_TO_ME   (1 << 20)
#define RT2573_DROP_TODS        (1 << 21)
#define RT2573_DROP_VER_ERROR   (1 << 22)
#define RT2573_DROP_MULTICAST   (1 << 23)
#define RT2573_DROP_BROADCAST   (1 << 24)
#define RT2573_DROP_ACKCTS      (1 << 25)
#define RT2573_DROP_VERSION_ERROR	(1 << 6)

/* possible flags for register TXRX_CSR4 */
#define RT2573_SHORT_PREAMBLE   (1 << 19)
#define RT2573_MRR_ENABLED      (1 << 20)
#define RT2573_MRR_CCK_FALLBACK (1 << 23)

/* possible flags for register TXRX_CSR9 */
#define RT2573_TSF_TICKING      (1 << 16)
#define RT2573_TSF_MODE(x)      (((x) & 0x3) << 17)
/* TBTT stands for Target Beacon Transmission Time */
#define RT2573_ENABLE_TBTT      (1 << 19)
#define RT2573_GENERATE_BEACON  (1 << 20)

/* possible flags for register PHY_CSR0 */
#define RT2573_PA_PE_2GHZ       (1 << 16)
#define RT2573_PA_PE_5GHZ       (1 << 17)

/* possible flags for register PHY_CSR3 */
#define RT2573_BBP_READ (1 << 15)
#define RT2573_BBP_BUSY (1 << 16)
/* possible flags for register PHY_CSR4 */
#define RT2573_RF_21BIT (21 << 24)
#define RT2573_RF_BUSY  (1 << 31)

/* possible values for register STA_CSR4 */
#define RT2573_TX_STAT_VALID    (1 << 0)
#define RT2573_TX_RESULT(v)     (((v) >> 1) & 0x7)
#define RT2573_TX_RETRYCNT(v)   (((v) >> 4) & 0xf)
#define RT2573_TX_QID(v)        (((v) >> 8) & 0xf)
#define RT2573_TX_SUCCESS       0
#define RT2573_TX_RETRY_FAIL    6

/* possible flags for register TX_CNTL_CSR */
#define RT2573_KICK_MGT (1 << 4)

/* possible flags for register INT_SOURCE_CSR */
#define RT2573_TX_DONE          (1 << 0)
#define RT2573_RX_DONE          (1 << 1)
#define RT2573_TX0_DMA_DONE     (1 << 16)
#define RT2573_TX1_DMA_DONE     (1 << 17)
#define RT2573_TX2_DMA_DONE     (1 << 18)
#define RT2573_TX3_DMA_DONE     (1 << 19)
#define RT2573_MGT_DONE         (1 << 20)
/* possible flags for register E2PROM_CSR */
#define RT2573_C        (1 << 1)
#define RT2573_S        (1 << 2)
#define RT2573_D        (1 << 3)
#define RT2573_Q        (1 << 4)
#define RT2573_93C46    (1 << 5)

/* LED values */
#define RT2573_LED_RADIO	(1 << 8)
#define RT2573_LED_G		(1 << 9)
#define RT2573_LED_A		(1 << 10)
#define RT2573_LED_ON		0x1e1e
#define RT2573_LED_OFF		0x0
#define RT2573_BBPR94_DEFAULT	6

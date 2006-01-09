/*	$OpenBSD: rt2661reg.h,v 1.1 2006/01/09 20:03:34 damien Exp $	*/

/*-
 * Copyright (c) 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
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

#define RT2661_TX_RING_COUNT	32
#define RT2661_MGT_RING_COUNT	32
#define RT2661_RX_RING_COUNT	64

#define RT2661_TX_DESC_SIZE	(sizeof (struct rt2661_tx_desc))
#define RT2661_TX_DESC_WSIZE	(RT2661_TX_DESC_SIZE / 4)
#define RT2661_RX_DESC_SIZE	(sizeof (struct rt2661_rx_desc))
#define RT2661_RX_DESC_WSIZE	(RT2661_RX_DESC_SIZE / 4)

#define RT2661_MAX_SCATTER	5

/*
 * Control and status registers.
 */
#define RT2661_HOST_CMD_CSR		0x0008
#define RT2661_MCU_CNTL_CSR		0x000c
#define RT2661_SOFT_RESET_CSR		0x0010
#define RT2661_MCU_INT_SOURCE_CSR	0x0014
#define RT2661_MCU_INT_MASK_CSR		0x0018
#define RT2661_PCI_USEC_CSR		0x001c
#define RT2661_H2M_MAILBOX_CSR		0x2100
#define RT2661_M2H_CMD_DONE_CSR		0x2104
#define RT2661_HW_BEACON_BASE0		0x2c00
#define RT2661_MAC_CSR0			0x3000
#define RT2661_MAC_CSR1			0x3004
#define RT2661_MAC_CSR2			0x3008
#define RT2661_MAC_CSR3			0x300c
#define RT2661_MAC_CSR4			0x3010
#define RT2661_MAC_CSR5			0x3014
#define RT2661_MAC_CSR6			0x3018
#define RT2661_MAC_CSR7			0x301c
#define RT2661_MAC_CSR8			0x3020
#define RT2661_MAC_CSR9			0x3024
#define RT2661_MAC_CSR10		0x3028
#define RT2661_MAC_CSR11		0x302c
#define RT2661_MAC_CSR12		0x3030
#define RT2661_MAC_CSR13		0x3034
#define RT2661_MAC_CSR14		0x3038
#define RT2661_MAC_CSR15		0x303c
#define RT2661_TXRX_CSR0		0x3040
#define RT2661_TXRX_CSR1		0x3044
#define RT2661_TXRX_CSR2		0x3048
#define RT2661_TXRX_CSR3		0x304c
#define RT2661_TXRX_CSR4		0x3050
#define RT2661_TXRX_CSR5		0x3054
#define RT2661_TXRX_CSR6		0x3058
#define RT2661_TXRX_CSR7		0x305c
#define RT2661_TXRX_CSR8		0x3060
#define RT2661_TXRX_CSR9		0x3064
#define RT2661_TXRX_CSR10		0x3068
#define RT2661_TXRX_CSR11		0x306c
#define RT2661_TXRX_CSR12		0x3070
#define RT2661_TXRX_CSR13		0x3074
#define RT2661_TXRX_CSR14		0x3078
#define RT2661_TXRX_CSR15		0x307c
#define RT2661_PHY_CSR0			0x3080
#define RT2661_PHY_CSR1			0x3084
#define RT2661_PHY_CSR2			0x3088
#define RT2661_PHY_CSR3			0x308c
#define RT2661_PHY_CSR4			0x3090
#define RT2661_PHY_CSR5			0x3094
#define RT2661_PHY_CSR6			0x3098
#define RT2661_PHY_CSR7			0x309c
#define RT2661_SEC_CSR0			0x30a0
#define RT2661_SEC_CSR1			0x30a4
#define RT2661_SEC_CSR2			0x30a8
#define RT2661_SEC_CSR3			0x30ac
#define RT2661_SEC_CSR4			0x30b0
#define RT2661_SEC_CSR5			0x30b4
#define RT2661_STA_CSR0			0x30c0
#define RT2661_STA_CSR1			0x30c4
#define RT2661_STA_CSR2			0x30c8
#define RT2661_STA_CSR3			0x30cc
#define RT2661_STA_CSR4			0x30d0
#define RT2661_AC0_BASE_CSR		0x3400
#define RT2661_AC1_BASE_CSR		0x3404
#define RT2661_AC2_BASE_CSR		0x3408
#define RT2661_AC3_BASE_CSR		0x340c
#define RT2661_MGT_BASE_CSR		0x3410
#define RT2661_TX_RING_CSR0		0x3418
#define RT2661_TX_RING_CSR1		0x341c
#define RT2661_AIFSN_CSR		0x3420
#define RT2661_CWMIN_CSR		0x3424
#define RT2661_CWMAX_CSR		0x3428
#define RT2661_TX_DMA_DST_CSR		0x342c
#define RT2661_TX_CNTL_CSR		0x3430
#define RT2661_LOAD_TX_RING_CSR		0x3434
#define RT2661_RX_BASE_CSR		0x3450
#define RT2661_RX_RING_CSR		0x3454
#define RT2661_RX_CNTL_CSR		0x3458
#define RT2661_PCI_CFG_CSR		0x3460
#define RT2661_INT_SOURCE_CSR		0x3468
#define RT2661_INT_MASK_CSR		0x346c
#define RT2661_E2PROM_CSR		0x3470
#define RT2661_AC_TXOP_CSR0		0x3474
#define RT2661_AC_TXOP_CSR1		0x3478
#define RT2661_TEST_MODE_CSR		0x3484
#define RT2661_IO_CNTL_CSR		0x3498
#define RT2661_MCU_CODE_BASE		0x4000


/* possible flags for register HOST_CMD_CSR */
#define RT2661_KICK_CMD		(1 << 7)
/* Host to MCU (8051) command identifiers */
#define RT2661_MCU_CMD_SLEEP	0x30
#define RT2661_MCU_CMD_WAKEUP	0x31
#define RT2661_MCU_SET_LED	0x50
#define RT2661_MCU_SET_RSSI_LED	0x52

/* possible flags for register MCU_CNTL_CSR */
#define RT2661_MCU_SEL		(1 << 0)
#define RT2661_MCU_RESET	(1 << 1)
#define RT2661_MCU_READY	(1 << 2)

/* possible flags for register MCU_INT_SOURCE_CSR */
#define RT2661_MCU_CMD_DONE		0xff
#define RT2661_MCU_WAKEUP		(1 << 8)
#define RT2661_MCU_BEACON_EXPIRE	(1 << 9)

/* possible flags for register H2M_MAILBOX_CSR */
#define RT2661_H2M_BUSY		(1 << 24)
#define RT2661_TOKEN_NO_INTR	0xff

/* possible flags for register MAC_CSR5 */
#define RT2661_ONE_BSSID	3

/* possible flags for register TXRX_CSR0 */
/* Tx filter flags are in the low 16 bits */
#define RT2661_AUTO_TX_SEQ	(1 << 15)
/* Rx filter flags are in the high 16 bits */
#define RT2661_DISABLE_RX	(1 << 16)
#define RT2661_DROP_CRC_ERROR	(1 << 17)
#define RT2661_DROP_PHY_ERROR	(1 << 18)
#define RT2661_DROP_CTL		(1 << 19)
#define RT2661_DROP_NOT_TO_ME	(1 << 20)
#define RT2661_DROP_TODS	(1 << 21)
#define RT2661_DROP_VER_ERROR	(1 << 22)
#define RT2661_DROP_MULTICAST	(1 << 23)
#define RT2661_DROP_BROADCAST	(1 << 24)
#define RT2661_DROP_ACKCTS	(1 << 25)

/* possible flags for register TXRX_CSR4 */
#define RT2661_SHORT_PREAMBLE	(1 << 19)

/* possible values for register TXRX_CSR9 */
#define RT2661_TSF_TICKING	(1 << 16)
#define RT2661_TSF_MODE(x)	(((x) & 0x3) << 17)
/* TBTT stands for Target Beacon Transmission Time */
#define RT2661_ENABLE_TBTT	(1 << 19)
#define RT2661_GENERATE_BEACON	(1 << 20)

/* possible flags for register PHY_CSR0 */
#define RT2661_PA_PE_2GHZ	(1 << 16)
#define RT2661_PA_PE_5GHZ	(1 << 17)

/* possible values for register STA_CSR4 */
#define RT2661_TX_STAT_VALID	(1 << 0)
#define RT2661_TX_RESULT(v)	(((v) >> 1) & 0x7)
#define RT2661_TX_RETRYCNT(v)	(((v) >> 4) & 0xf)
#define RT2661_TX_QID(v)	(((v) >> 8) & 0xf)
#define RT2661_TX_SUCCESS	0
#define RT2661_TX_RETRY_FAIL	6

/* possible flags for register TX_CNTL_CSR */
#define RT2661_KICK_MGT	(1 << 4)

/* possible flags for register INT_SOURCE_CSR */
#define RT2661_TX_DONE		(1 << 0)
#define RT2661_RX_DONE		(1 << 1)
#define RT2661_TX0_DMA_DONE	(1 << 16)
#define RT2661_TX1_DMA_DONE	(1 << 17)
#define RT2661_TX2_DMA_DONE	(1 << 18)
#define RT2661_TX3_DMA_DONE	(1 << 19)
#define RT2661_MGT_DONE		(1 << 20)

/* possible flags for register E2PROM_CSR */
#define RT2661_C	(1 << 1)
#define RT2661_S	(1 << 2)
#define RT2661_D	(1 << 3)
#define RT2661_Q	(1 << 4)
#define RT2661_93C46	(1 << 5)

/* Tx descriptor */
struct rt2661_tx_desc {
	uint32_t	flags;
#define RT2661_TX_BUSY		(1 << 0)
#define RT2661_TX_VALID		(1 << 1)
#define RT2661_TX_MORE_FRAG	(1 << 2)
#define RT2661_TX_NEED_ACK	(1 << 3)
#define RT2661_TX_TIMESTAMP	(1 << 4)
#define RT2661_TX_OFDM		(1 << 5)
#define RT2661_TX_IFS		(1 << 6)
#define RT2661_TX_LONG_RETRY	(1 << 7)
#define RT2661_TX_BURST		(1 << 28)

	uint16_t	wme;
#define RT2661_AIFSN(v)		((v) << 4)
#define RT2661_LOGCWMIN(v)	((v) << 8)
#define RT2661_LOGCWMAX(v)	((v) << 12)
#define RT2661_QID(v)		(v)

	uint16_t	xflags;
#define RT2661_TX_HWSEQ		(1 << 12)

	uint8_t		plcp_signal;
	uint8_t		plcp_service;
#define RT2661_PLCP_LENGEXT	0x80

	uint8_t		plcp_length_lo;
	uint8_t		plcp_length_hi;

	uint32_t	iv;
	uint32_t	eiv;

	uint8_t		offset;
	uint8_t		id;
	uint8_t		txpower;
#define RT2661_DEFAULT_TXPOWER	0

	uint8_t		reserved1;

	uint32_t	addr[RT2661_MAX_SCATTER];
	uint16_t	len[RT2661_MAX_SCATTER];

	uint16_t	reserved2;
} __packed;

/* Rx descriptor */
struct rt2661_rx_desc {
	uint32_t	flags;
#define RT2661_RX_BUSY		(1 << 0)
#define RT2661_RX_DROP		(1 << 1)
#define RT2661_RX_CRC_ERROR	(1 << 6)
#define RT2661_RX_OFDM		(1 << 7)
#define RT2661_RX_PHY_ERROR	(1 << 8)
#define RT2661_RX_CIPHER_MASK	0x00000600

	uint8_t		signal;
	uint8_t		rssi;
	uint8_t		reserved1;
	uint8_t		offset;
	uint32_t	iv;
	uint32_t	eiv;
	uint32_t	reserved2;
	uint32_t	physaddr;
	uint32_t	reserved3[10];
} __packed;

#define RAL_RF1	0
#define RAL_RF2	2
#define RAL_RF3	1
#define RAL_RF4	3

#define RT2661_BBP_READ	(1 << 15)
#define RT2661_BBP_BUSY	(1 << 16)
#define RT2661_RF_21BIT	(21 << 24)
#define RT2661_RF_BUSY	(1 << 31)

/* dual-band RF */
#define RT2661_RF_5225	1
#define RT2661_RF_5325	2
/* single-band RF */
#define RT2661_RF_2527	3
#define RT2661_RF_2529	4

#define RT2661_RX_DESC_BACK	4

#define RT2661_TXQ_MGT	13

#define RT2661_SMART_MODE	(1 << 0)

#define RT2661_BBPR94_DEFAULT	6

#define RT2661_SHIFT_D	3
#define RT2661_SHIFT_Q	4

#define RT2661_EEPROM_MAC01		0x02
#define RT2661_EEPROM_MAC23		0x03
#define RT2661_EEPROM_MAC45		0x04
#define RT2661_EEPROM_ANTENNA		0x10
#define RT2661_EEPROM_CONFIG2		0x11
#define RT2661_EEPROM_TXPOWER		0x23
#define RT2661_EEPROM_FREQ_OFFSET	0x2f
#define RT2661_EEPROM_RSSI_2GHZ_OFFSET	0x4d
#define RT2661_EEPROM_RSSI_5GHZ_OFFSET	0x4e

#define RT2661_EEPROM_DELAY	1	/* minimum hold time (microsecond) */

/*
 * control and status registers access macros
 */
#define RAL_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define RAL_READ_REGION_4(sc, offset, datap, count)			\
	bus_space_read_region_4((sc)->sc_st, (sc)->sc_sh, (offset),	\
	    (datap), (count))

#define RAL_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define RAL_WRITE_REGION_1(sc, offset, datap, count)			\
	bus_space_write_region_1((sc)->sc_st, (sc)->sc_sh, (offset),	\
	    (datap), (count))

/*
 * EEPROM access macro
 */
#define RT2661_EEPROM_CTL(sc, val) do {					\
	RAL_WRITE((sc), RT2661_E2PROM_CSR, (val));			\
	DELAY(RT2661_EEPROM_DELAY);					\
} while (/* CONSTCOND */0)

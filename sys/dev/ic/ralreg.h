/*	$OpenBSD: ralreg.h,v 1.1 2005/02/15 20:51:21 damien Exp $  */

/*-
 * Copyright (c) 2005
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

#define RAL_TX_RING_COUNT	48
#define RAL_ATIM_RING_COUNT	4
#define RAL_PRIO_RING_COUNT	16
#define RAL_BEACON_RING_COUNT	1
#define RAL_RX_RING_COUNT	32

#define RAL_TX_DESC_SIZE	(sizeof (struct ral_tx_desc))
#define RAL_RX_DESC_SIZE	(sizeof (struct ral_rx_desc))

#define RAL_MAX_SCATTER	1

#define RAL_CSR0	0x0000
#define RAL_CSR1	0x0004
#define RAL_CSR2	0x0008
#define RAL_CSR3	0x000c
#define RAL_CSR4	0x0010
#define RAL_CSR5	0x0014
#define RAL_CSR6	0x0018
#define RAL_CSR7	0x001c
#define RAL_CSR8	0x0020
#define RAL_CSR9	0x0024
#define RAL_CSR11	0x002c
#define RAL_CSR12	0x0030
#define RAL_CSR13	0x0034
#define RAL_CSR14	0x0038
#define RAL_CSR15	0x003c
#define RAL_CSR16	0x0040
#define RAL_CSR17	0x0044
#define RAL_CSR18	0x0048
#define RAL_CSR19	0x004c
#define RAL_CSR20	0x0050
#define RAL_CSR21	0x0054
#define RAL_CSR22	0x0058

#define RAL_SECCSR0	0x0028
#define RAL_SECCSR1	0x0158
#define RAL_SECCSR3	0x00fc

#define RAL_TXCSR0	0x0060
#define RAL_TXCSR1	0x0064
#define RAL_TXCSR2	0x0068
#define RAL_TXCSR3	0x006c
#define RAL_TXCSR4	0x0070
#define RAL_TXCSR5	0x0074
#define RAL_TXCSR6	0x0078
#define RAL_TXCSR7	0x007c
#define RAL_TXCSR8	0x0098
#define RAL_TXCSR9	0x0094

#define RAL_RXCSR0	0x0080
#define RAL_RXCSR1	0x0084
#define RAL_RXCSR2	0x0088
#define RAL_RXCSR3	0x0090
#define RAL_ARCSR1	0x009c

#define RAL_PCICSR	0x008c

#define RAL_CNT0	0x00a0
#define RAL_CNT1	0x00ac
#define RAL_CNT2	0x00b0
#define RAL_CNT3	0x00b8
#define RAL_CNT4	0x00bc
#define RAL_CNT5	0x00c0

#define RAL_PWRCSR0	0x00c4
#define RAL_PSCSR0	0x00c8
#define RAL_PSCSR1	0x00cc
#define RAL_PSCSR2	0x00d0
#define RAL_PSCSR3	0x00d4
#define RAL_PWRCSR1	0x00d8
#define RAL_TIMECSR	0x00dc
#define RAL_MACCSR0	0x00e0
#define RAL_MACCSR1	0x00e4
#define RAL_RALINKCSR	0x00e8
#define RAL_BCNCSR	0x00ec

#define RAL_BBPCSR	0x00f0
#define RAL_RFCSR	0x00f4
#define RAL_LEDCSR	0x00f8

#define RAL_TXACKCSR0	0x0110
#define RAL_ACKCNT0	0x0114
#define RAL_ACKCNT1	0x0118

#define RAL_GPIOCSR	0x0120
#define RAL_FIFOCSR0	0x0128
#define RAL_FIFOCSR1	0x012C
#define RAL_BCNCSR1	0x0130
#define RAL_MACCSR2	0x0134
#define RAL_TESTCSR	0x0138

#define RAL_ARCSR2	0x013c
#define RAL_ARCSR3	0x0140
#define RAL_ARCSR4	0x0144
#define RAL_ARCSR5	0x0148

#define RAL_ARTCSR0	0x014c
#define RAL_ARTCSR1	0x0150
#define RAL_ARTCSR2	0x0154
#define RAL_SECCSR1	0x0158
#define RAL_BBPCSR1	0x015c


#define RAL_CSR1_SOFT_RESET	(1 << 0)
#define RAL_CSR1_HOST_READY	(1 << 2)

#define RAL_CSR21_93C46		(1 << 5)

#define RAL_TXCSR0_KICK_TX	(1 << 0)
#define RAL_TXCSR0_KICK_ATIM	(1 << 1)
#define RAL_TXCSR0_KICK_PRIO	(1 << 2)
#define RAL_TXCSR0_ABORT	(1 << 3)

#define RAL_RXCSR0_DISABLE		(1 << 0)
#define RAL_RXCSR0_DROP_CRC		(1 << 1)
#define RAL_RXCSR0_DROP_PHY		(1 << 2)
#define RAL_RXCSR0_DROP_CTL		(1 << 3)
#define RAL_RXCSR0_DROP_NOT_TO_ME	(1 << 4)
#define RAL_RXCSR0_DROP_TODS		(1 << 5)
#define RAL_RXCSR0_DROP_BAD_VERSION	(1 << 6)

#define RAL_SECCSR0_KICK	(1 << 0)

#define RAL_SECCSR1_KICK	(1 << 0)

/* possible flags for register CSR7 */
#define RAL_CSR7_BEACON_EXPIRE		0x00000001
#define RAL_CSR7_WAKEUP_EXPIRE		0x00000002
#define RAL_CSR7_ATIM_EXPIRE		0x00000004
#define RAL_CSR7_TX_DONE		0x00000008
#define RAL_CSR7_ATIM_DONE		0x00000010
#define RAL_CSR7_PRIO_DONE		0x00000020
#define RAL_CSR7_RX_DONE		0x00000040
#define RAL_CSR7_DECRYPTION_DONE	0x00000080
#define RAL_CSR7_ENCRYPTION_DONE	0x00000100

#define RAL_CSR8_MASK							\
	(~(RAL_CSR7_BEACON_EXPIRE | RAL_CSR7_WAKEUP_EXPIRE |		\
	   RAL_CSR7_TX_DONE | RAL_CSR7_PRIO_DONE | RAL_CSR7_RX_DONE |	\
	   RAL_CSR7_DECRYPTION_DONE | RAL_CSR7_ENCRYPTION_DONE))

#define RAL_CSR14_TSF_AUTOCOUNT		(1 << 0)
#define RAL_CSR14_TSF_SYNC_BSS		(1 << 1)
#define RAL_CSR14_TSF_SYNC_IBSS		(2 << 1)
#define RAL_CSR14_BCN_RELOAD		(1 << 3)
#define RAL_CSR14_GENERATE_BEACON	(1 << 6)
#define RAL_CSR14_PRELOAD_SHIFT		16

/* Tx descriptor */
struct ral_tx_desc {
	uint32_t	flags;
#define RAL_TX_BUSY		(1 << 0)
#define RAL_TX_VALID		(1 << 1)

#define RAL_TX_RESULT_MASK	0x0000001c
#define RAL_TX_SUCCESS		(0 << 2)
#define RAL_TX_SUCCESS_RETRY	(1 << 2)
#define RAL_TX_FAIL_RETRY	(2 << 2)
#define RAL_TX_FAIL_INVALID	(3 << 2)
#define RAL_TX_FAIL_OTHER	(4 << 2)

#define RAL_TX_NOT_LAST		(1 << 8)
#define RAL_TX_NEED_ACK		(1 << 9)
#define RAL_TX_INSERT_TIMESTAMP	(1 << 10)
#define RAL_TX_OFDM		(1 << 11)
#define RAL_TX_CIPHER_BUSY	(1 << 12)

#define RAL_TX_IFS_MASK		0x00006000
#define RAL_TX_IFS_BACKOFF	(0 << 13)
#define RAL_TX_IFS_SIFS		(1 << 13)
#define RAL_TX_IFS_NEW_BACKOFF	(2 << 13)
#define RAL_TX_IFS_NONE		(3 << 13)

#define RAL_TX_CIPHER_MASK	0xe0000000
#define RAL_TX_CIPHER_NONE	(0 << 29)
#define RAL_TX_CIPHER_WEP40	(1 << 29)
#define RAL_TX_CIPHER_WEP104	(2 << 29)
#define RAL_TX_CIPHER_TKIP	(3 << 29)
#define RAL_TX_CIPHER_AES	(4 << 29)

	uint32_t	physaddr;
	uint32_t	wme;
#define RAL_WME_CWMAX_BITS_SHIFT	12
#define RAL_WME_CWMIN_BITS_SHIFT	8
#define RAL_WME_AIFSN_BITS_SHIFT	6

	uint8_t		plcp_signal;
	uint8_t		plcp_service;
#define RAL_PLCP_LENGEXT	0x80

	uint16_t	plcp_length;
	uint32_t	iv;
	uint32_t	eiv;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	reserved[2];
} __packed;

/* Rx descriptor */
struct ral_rx_desc {
	uint32_t	flags;
#define RAL_RX_BUSY		(1 << 0)
#define RAL_RX_CRC_ERROR	(1 << 5)
#define RAL_RX_PHY_ERROR	(1 << 7)
#define RAL_RX_CIPHER_BUSY	(1 << 8)
#define RAL_RX_ICV_ERROR	(1 << 9)

#define RAL_RX_CIPHER_MASK	0xe0000000
#define RAL_RX_CIPHER_NONE	(0 << 29)
#define RAL_RX_CIPHER_WEP40	(1 << 29)
#define RAL_RX_CIPHER_WEP104	(2 << 29)
#define RAL_RX_CIPHER_TKIP	(3 << 29)
#define RAL_RX_CIPHER_AES	(4 << 29)

	uint32_t	physaddr;
	uint8_t		rate;
	uint8_t		rssi;
	uint8_t		ta[IEEE80211_ADDR_LEN];
	uint32_t	iv;
	uint32_t	eiv;
	uint8_t		key[IEEE80211_KEYBUF_SIZE];
	uint32_t	reserved[2];
} __packed;


#define RAL_RF1	0
#define RAL_RF2	2
#define RAL_RF3	1
#define RAL_RF4	3

#define RAL_RF1_AUTOTUNE	0x08000
#define RAL_RF3_AUTOTUNE	0x00040

#define RAL_BBP_BUSY	(1 << 15)
#define RAL_BBP_WRITE	(1 << 16)
#define RAL_RF_BUSY	(1 << 31)
#define RAL_RF_20BIT	(20 << 24)

#define RAL_RF_2522	0x00
#define RAL_RF_2523	0x01
#define RAL_RF_2524	0x02
#define RAL_RF_2525	0x03
#define RAL_RF_2525E	0x04
/* dual-band RF */
#define RAL_RF_5222	0x10

#define RAL_BBP_VERSION	0

#define RAL_LED_MODE_DEFAULT		0
#define RAL_LED_MODE_TXRX_ACTIVITY	1
#define RAL_LED_MODE_SINGLE		2
#define RAL_LED_MODE_ASUS		3

#define RAL_JAPAN_FILTER	0x8

#define RAL_EEPROM_ANTENNA	16
#define RAL_EEPROM_CONFIG	17
#define RAL_EEPROM_COUNTRY	18
#define RAL_EEPROM_BBP_BASE	19
#define RAL_EEPROM_TXPOWER_BASE	35
#define RAL_EEPROM_TSSI_BASE	42
#define RAL_EEPROM_RSSITODBM	62
#define RAL_EEPROM_VERSION	63

#define RAL_EEPROM_DELAY	1	/* minimum hold time (microsecond) */

#define RAL_EEPROM_C	(1 << 1)	/* Serial Clock */
#define RAL_EEPROM_S	(1 << 2)	/* Chip Select */
#define RAL_EEPROM_D	(1 << 3)	/* Serial data input */
#define RAL_EEPROM_Q	(1 << 4)	/* Serial data output */

#define RAL_EEPROM_SHIFT_D	3
#define RAL_EEPROM_SHIFT_Q	4

/*
 * control and status registers access macros
 */
#define RAL_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define RAL_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

/*
 * EEPROM access macro
 */
#define RAL_EEPROM_CTL(sc, val) do {					\
	RAL_WRITE((sc), RAL_CSR21, (val));				\
	DELAY(RAL_EEPROM_DELAY);					\
} while (/* CONSTCOND */0)

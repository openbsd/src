/*	$Id: if_iwireg.h,v 1.1 2004/10/20 12:50:48 deraadt Exp $ */

/*-
 * Copyright (c) 2004
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define IWI_TX_SLOT_SIZE	128
#define IWI_RX_NSLOTS		32

#define IWI_CSR_INTR		0x0008
#define IWI_CSR_INTR_MASK	0x000c
#define IWI_CSR_INDIRECT_ADDR	0x0010
#define IWI_CSR_INDIRECT_DATA	0x0014
#define IWI_CSR_AUTOINC_ADDR	0x0018
#define IWI_CSR_AUTOINC_DATA	0x001c
#define IWI_CSR_RST		0x0020
#define IWI_CSR_CTL		0x0024
#define IWI_CSR_IO		0x0030
#define IWI_CSR_TX_BASE		0x0200
#define IWI_CSR_TX_SIZE		0x0204
#define IWI_CSR_TX_READ_INDEX	0x0280
#define IWI_CSR_RX_READ_INDEX	0x02a0
#define IWI_CSR_RX_SLOT_BASE	0x0500
#define IWI_CSR_TABLE0_SIZE	0x0700
#define IWI_CSR_TABLE0_BASE	0x0704
#define IWI_CSR_CURRENT_TX_RATE	IWI_CSR_TABLE0_BASE
#define IWI_CSR_TX_WRITE_INDEX	0x0f80
#define IWI_CSR_RX_WRITE_INDEX	0x0fa0
#define IWI_CSR_READ_INT	0x0ff4

/* possible flags for IWI_CSR_INTR */
#define IWI_INTR_RX_TRANSFER	0x00000002
#define IWI_INTR_TX0_TRANSFER	0x00000800
#define IWI_INTR_TX1_TRANSFER	0x00001000
#define IWI_INTR_TX2_TRANSFER	0x00002000
#define IWI_INTR_TX3_TRANSFER	0x00004000
#define IWI_INTR_TX4_TRANSFER	0x00008000
#define IWI_INTR_FW_INITED	0x01000000
#define IWI_INTR_FATAL_ERROR	0x40000000
#define IWI_INTR_PARITY_ERROR	0x80000000

#define IWI_INTR_MASK							\
	(IWI_INTR_RX_TRANSFER |	IWI_INTR_TX0_TRANSFER |			\
	 IWI_INTR_TX1_TRANSFER | IWI_INTR_TX2_TRANSFER |		\
	 IWI_INTR_TX3_TRANSFER | IWI_INTR_TX4_TRANSFER |		\
	 IWI_INTR_FW_INITED | IWI_INTR_FATAL_ERROR |			\
	 IWI_INTR_PARITY_ERROR)

/* possible flags for register IWI_CSR_RST */
#define IWI_RST_PRINCETON_RESET	0x00000001
#define IWI_RST_SW_RESET	0x00000080
#define IWI_RST_MASTER_DISABLED	0x00000100
#define IWI_RST_STOP_MASTER	0x00000200

/* possible flags for register IWI_CSR_CTL */
#define IWI_CTL_CLOCK_READY	0x00000001
#define IWI_CTL_ALLOW_STANDBY	0x00000002
#define IWI_CTL_INIT		0x00000004

/* possible flags for register IWI_CSR_IO */
#define IWI_IO_RADIO_ENABLED	0x00010000

/* possible flags for IWI_CSR_READ_INT */
#define IWI_READ_INT_INIT_HOST	0x20000000

/* table2 offsets */
#define IWI_INFO_ADAPTER_MAC	40

/* constants for command blocks */
#define IWI_CB_DEFAULT_CTL	0x8cea0000
#define IWI_CB_MAXDATALEN	8191

/* supported rates */
#define IWI_RATE_DS1	10
#define IWI_RATE_DS2	20
#define IWI_RATE_DS5	55
#define IWI_RATE_DS11	110
#define IWI_RATE_OFDM6	13
#define IWI_RATE_OFDM9	15
#define IWI_RATE_OFDM12	5
#define IWI_RATE_OFDM18	7
#define IWI_RATE_OFDM24	9
#define IWI_RATE_OFDM36	11
#define IWI_RATE_OFDM48	1
#define IWI_RATE_OFDM54	3

struct iwi_hdr {
	u_int8_t	type;
#define IWI_HDR_TYPE_DATA	0
#define IWI_HDR_TYPE_COMMAND	1
#define IWI_HDR_TYPE_NOTIF	3
#define IWI_HDR_TYPE_FRAME	9
	u_int8_t	seq;
	u_int8_t	flags;
#define IWI_HDR_FLAG_IRQ	0x04
	u_int8_t	reserved;
} __packet;

struct iwi_notif {
	u_int32_t	reserved[2];
	u_int8_t	type;
#define IWI_NOTIF_TYPE_ASSOCIATION	10
#define IWI_NOTIF_TYPE_AUTHENTICATION	11
#define IWI_NOTIF_TYPE_SCAN_CHANNEL	12
#define IWI_NOTIF_TYPE_SCAN_COMPLETE	13
#define IWI_NOTIF_TYPE_BEACON		17
#define IWI_NOTIF_TYPE_CALIBRATION	20
	u_int8_t	flags;
	u_int16_t	len;
} __attribute__((__packed__));

/* structure for notification IWI_NOTIF_TYPE_AUTHENTICATION */
struct iwi_notif_authentication {
	u_int8_t	state;
#define IWI_DEAUTHENTICATED	0
#define IWI_AUTHENTICATED	9
} __attribute__((__packed__));

/* structure for notification IWI_NOTIF_TYPE_ASSOCIATION */
struct iwi_notif_association {
	u_int8_t		state;
#define IWI_DEASSOCIATED	0
#define IWI_ASSOCIATED		12
	struct ieee80211_frame	frame;
	u_int16_t		capinfo;
	u_int16_t		status;
	u_int16_t		associd;
} __attribute__((__packed__));

/* structure for notification IWI_NOTIF_TYPE_SCAN_COMPLETE */
struct iwi_notif_scan_complete {
	u_int8_t	type;
	u_int8_t	nchan;
	u_int8_t	status;
	u_int8_t	reserved;
} __attribute__((__packed__));

/* received frame header */
struct iwi_frame {
	u_int32_t	reserved1[2];
	u_int8_t	chan;
	u_int8_t	status;
	u_int8_t	rate;
	u_int8_t	rssi;	/* receiver signal strength indicator */
	u_int8_t	agc;	/* automatic gain control */
#define IWI_RSSI2DBM(rssi, agc)						\
	((u_int8_t)((rssi) -						\
	 ((((agc) & 0x2e) >> 1) + ((((agc) | 0x8c) & 0xcc) >> 2) +	\
	 ((agc) & 0x0f))))
	u_int8_t	reserved2;
	u_int16_t	signal;
	u_int16_t	noise;
	u_int8_t	antenna;
	u_int8_t	control;
	u_int8_t	reserved3[2];
	u_int16_t	len;
} __attribute__((__packed__));

/* header for transmission */
struct iwi_data {
	u_int32_t	reserved1[2];
	u_int8_t	cmd;
#define IWI_DATA_CMD_TX	0x0b
	u_int8_t	seq;
	u_int16_t	len;
	u_int8_t	priority;
	u_int8_t	flags;
#define IWI_DATA_FLAG_SHPREAMBLE	0x04
#define IWI_DATA_FLAG_NO_WEP		0x20
#define IWI_DATA_FLAG_NEED_ACK		0x80
	u_int8_t	xflags;
	u_int8_t	wep_txkey;
	u_int8_t	wepkey[IEEE80211_KEYBUF_SIZE];
	u_int8_t	rate;
	u_int8_t	antenna;
	u_int8_t	reserved2[10];

	struct ieee80211_qosframe_addr4	wh;
	u_int32_t	iv[2];

	u_int32_t	nseg;
#define IWI_MAX_NSEG	6
	u_int32_t	seg_addr[IWI_MAX_NSEG];
	u_int16_t	seg_len[IWI_MAX_NSEG];
} __attribute__((__packed__));

/* command */
struct iwi_cmd {
	u_int8_t	type;
#define IWI_CMD_ENABLE				2
#define IWI_CMD_SET_CONFIGURATION		6
#define IWI_CMD_SET_ESSID			8
#define IWI_CMD_SET_MAC_ADDRESS			11
#define IWI_CMD_SET_RTS_THRESHOLD		15
#define IWI_CMD_SET_POWER_MODE			17
#define IWI_CMD_SET_WEP_KEY			18
#define IWI_CMD_SCAN				20
#define IWI_CMD_ASSOCIATE			21
#define IWI_CMD_SET_RATES			22
#define IWI_CMD_DISABLE				33
#define IWI_CMD_SET_IV				34
#define IWI_CMD_SET_TX_POWER			35
#define IWI_CMD_SET_SENSITIVITY			42
	u_int8_t	len;
	u_int16_t	reserved;
	u_int8_t	data[120];
} __attribute__((__packed__));

/* constants for 'mode' fields */
#define IWI_MODE_11B	1
#define IWI_MODE_11G	2

/* macro for command IWI_CMD_SET_SENSITIVITY */
#define IWI_RSSI2SENS(rssi)	((rssi) + 112)

/* possible values for command IWI_CMD_SET_POWER_MODE */
#define IWI_POWER_MODE_CAM	0

/* structure for command IWI_CMD_SET_RATES */
struct iwi_rateset {
	u_int8_t	mode;
	u_int8_t	nrates;
	u_int8_t	type;
#define IWI_RATESET_TYPE_NEGOCIATED	0
#define IWI_RATESET_TYPE_SUPPORTED	1
	u_int8_t	reserved;
	u_int8_t	rates[12];
} __attribute__((__packed__));

/* structure for command IWI_CMD_SET_TX_POWER */
struct iwi_txpower {
	u_int8_t	nchan;
	u_int8_t	mode;
	struct {
		u_int8_t	chan;
		u_int8_t	power;
#define IWI_TXPOWER_MAX		20
#define IWI_TXPOWER_RATIO	(IEEE80211_TXPOWER_MAX / IWI_TXPOWER_MAX)
	} __attribute__((__packed__)) chan[37];
} __attribute__((__packed__));

/* structure for command IWI_CMD_ASSOCIATE */
struct iwi_associate {
	u_int8_t	chan;
	u_int8_t	auth;
#define IWI_AUTH_OPEN	(0 << 4)
#define IWI_AUTH_SHARED	(1 << 4)
#define IWI_AUTH_NONE	(3 << 4)
	u_int8_t	type;
	u_int8_t	reserved1;
	u_int16_t	reserved2;
	u_int8_t	plen;
	u_int8_t	mode;
	u_int8_t	bssid[IEEE80211_ADDR_LEN];
	u_int8_t	tstamp[8];
	u_int16_t	capinfo;
	u_int16_t	lintval;
	u_int16_t	intval;
	u_int8_t	dst[IEEE80211_ADDR_LEN];
	u_int32_t	reserved3;
} __attribute__((__packed__));

/* structure for command IWI_CMD_SCAN */
struct iwi_scan {
	u_int8_t	type;
#define IWI_SCAN_TYPE_BROADCAST	3
	u_int16_t	intval;
	u_int8_t	channels[54];
#define IWI_CHAN_2GHZ	(1 << 6)
	u_int8_t	reserved[3];
} __attribute__((__packed__));

/* structure for command IWI_CMD_SET_CONFIGURATION */
struct iwi_configuration {
	u_int8_t	bluetooth_coexistence;
	u_int8_t	reserved1;
	u_int8_t	answer_broadcast_probe_req;
	u_int8_t	allow_invalid_frames;
	u_int8_t	enable_multicast;
	u_int8_t	exclude_unicast_unencrypted;
	u_int8_t	disable_unicast_decryption;
	u_int8_t	exclude_multicast_unencrypted;
	u_int8_t	disable_multicast_decryption;
	u_int8_t	antenna;
	u_int8_t	reserved2;
	u_int8_t	bg_autodetect;
	u_int8_t	reserved3;
	u_int8_t	enable_multicast_filtering;
	u_int8_t	bluetooth_threshold;
	u_int8_t	reserved4;
	u_int8_t	allow_beacon_and_probe_resp;
	u_int8_t	allow_mgt;
	u_int8_t	pass_noise;
	u_int8_t	reserved5;
} __attribute__((__packed__));

/* structure for command IWI_CMD_SET_WEP_KEY */
struct iwi_wep_key {
	u_int8_t	cmd;
#define IWI_WEP_KEY_CMD_SETKEY	0x08
	u_int8_t	seq;
	u_int8_t	idx;
	u_int8_t	len;
	u_int8_t	key[IEEE80211_KEYBUF_SIZE];
} __attribute__((__packed__));

/* EEPROM = Electrically Erasable Programmable Read-Only Memory */

#define IWI_MEM_EEPROM_CTL	0x00300040

#define IWI_EEPROM_MAC	0x21

#define IWI_EEPROM_DELAY	1	/* minimum hold time (microsecond) */

#define IWI_EEPROM_C       (1 << 0)        /* Serial Clock */
#define IWI_EEPROM_S       (1 << 1)        /* Chip Select */
#define IWI_EEPROM_D       (1 << 2)        /* Serial data input */
#define IWI_EEPROM_Q       (1 << 4)        /* Serial data output */

#define IWI_EEPROM_SHIFT_D    2
#define IWI_EEPROM_SHIFT_Q    4

/*
 * control and status registers access macros
 */
#define CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_READ_REGION_4(sc, offset, datap, count)			\
	bus_space_read_region_4((sc)->sc_st, (sc)->sc_sh, (offset),	\
	    (datap), (count))

#define CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

/*
 * indirect memory space access macros
 */
#define MEM_WRITE_1(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IWI_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_1((sc), IWI_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_2(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IWI_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_2((sc), IWI_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_4(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IWI_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_4((sc), IWI_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_MULTI_1(sc, addr, buf, len) do {			\
	CSR_WRITE_4((sc), IWI_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_MULTI_1((sc), IWI_CSR_INDIRECT_DATA, (buf), (len));	\
} while (/* CONSTCOND */0)

/*
 * EEPROM access macro
 */
#define IWI_EEPROM_CTL(sc, val) do {					\
	MEM_WRITE_4((sc), IWI_MEM_EEPROM_CTL, (val));			\
	DELAY(IWI_EEPROM_DELAY);					\
} while (0)

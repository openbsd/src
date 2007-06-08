/*	$OpenBSD: if_malovar.h,v 1.9 2007/06/08 22:08:21 mglocker Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
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

/* simplify bus space access */
#define MALO_READ_1(sc, reg) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define MALO_READ_2(sc, reg) \
	bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define	MALO_READ_MULTI_2(sc, reg, off, size) \
	bus_space_read_multi_2((sc)->sc_iot, (sc)->sc_ioh, (reg), (off), \
	(size))
#define MALO_WRITE_1(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define MALO_WRITE_2(sc, reg, val) \
	bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define MALO_WRITE_MULTI_2(sc, reg, off, size) \
	bus_space_write_multi_2((sc)->sc_iot, (sc)->sc_ioh, (reg), (off), \
	(size))

/* miscellaneous */
#define MALO_FW_HELPER_BSIZE	256	/* helper FW block size */
#define MALO_FW_HELPER_LOADED	0x10	/* helper FW loaded */
#define MALO_FW_MAIN_MAXRETRY	20	/* main FW block resend max retry */
#define MALO_CMD_BUFFER_SIZE	256	/* cmd buffer */

/* device flags */
#define MALO_DEVICE_ATTACHED	(1 << 0)
#define MALO_FW_LOADED		(1 << 1)

/* FW command header */
struct malo_cmd_header {
	uint16_t	cmd;
	uint16_t	size;
	uint16_t	seqnum;
	uint16_t	result;
	/* malo_cmd_body */
};

/* FW command bodies */
struct malo_cmd_body_spec {
	uint16_t	hw_if_version;
	uint16_t	hw_version;
	uint16_t	num_of_wcb;
	uint16_t	num_of_mcast;
	uint8_t		macaddr[ETHER_ADDR_LEN];
	uint16_t	regioncode;
	uint16_t	num_of_antenna;
	uint32_t	fw_version;
	uint32_t	wcbbase;
	uint32_t	rxpdrdptr;
	uint32_t	rxpdwrptr;
	uint32_t	fw_capinfo;
} __packed;

struct malo_cmd_body_radio {
	uint16_t	action;
	uint16_t	control;
} __packed;

struct malo_cmd_body_channel {
	uint16_t	action;
	uint16_t	channel;
	uint16_t	rftype;
	uint16_t	reserved;
	uint8_t		channel_list[32];
} __packed;

struct malo_cmd_body_txpower {
	uint16_t	action;
	int16_t		txpower;	
} __packed;

struct malo_cmd_body_antenna {
	uint16_t	action;
	uint16_t	antenna_mode;
} __packed;

struct malo_cmd_body_macctrl {
	uint16_t	action;
	uint16_t	reserved;
} __packed;

struct malo_cmd_body_assoc {
	uint8_t		peermac[ETHER_ADDR_LEN];
	uint16_t	capinfo;
	uint16_t	listenintrv;
	uint16_t	bcnperiod;
	uint8_t		dtimperiod;
	/* malo_cmd_body_assoc_ssid */
	/* malo_cmd_body_assoc_phy */
	/* malo_cmd_body_assoc_cf */
	/* malo_cmd_body_assoc_rate */
} __packed;
#define MALO_TLV_TYPE_SSID	0x0000
#define MALO_TLV_TYPE_PHY	0x0003
#define MALO_TLV_TYPE_CF	0x0004
#define MALO_TLV_TYPE_RATES	0x0001
struct malo_cmd_body_assoc_ssid {
	uint16_t	type;
	uint16_t	size;
	uint8_t		data[1];
} __packed;
struct malo_cmd_body_assoc_phy {
	uint16_t	type;
	uint16_t	size;
	uint8_t		data[1];
} __packed;
struct malo_cmd_body_assoc_cf {
	uint16_t	type;
	uint16_t	size;
	uint8_t		data[1];
} __packed;
struct malo_cmd_body_assoc_rate {
	uint16_t	type;
	uint16_t	size;
	uint8_t		data[1];
} __packed;

/* RX descriptor */
#define MALO_RX_STATUS_OK	0x0001
struct malo_rx_desc {
	uint16_t	status;
	uint8_t		snr;
	uint8_t		control;
	uint16_t	pkglen;
	uint8_t		nf;
	uint8_t		rate;
	uint32_t	pkgoffset;
	uint32_t	reserved1;
	uint8_t		priority;
	uint8_t		reserved2[3];
} __packed;

/* TX descriptor */
struct malo_tx_desc {
	uint32_t	status;
	uint32_t	control;
	uint32_t	pkgoffset;
	uint16_t	pkglen;
	uint8_t		dstaddrhigh[2];
	uint8_t		dstaddrlow[4];
	uint8_t		priority;
	uint8_t		flags;
	uint8_t		reserved[2];
} __packed;

struct malo_softc {
	struct device		 sc_dev;
	struct ieee80211com	 sc_ic;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	int			 (*sc_newstate)
				 (struct ieee80211com *, enum ieee80211_state,
				     int);

	int			 sc_flags;
	void			*sc_cmd;
	uint8_t			 sc_cmd_running;
	void			*sc_data;
	uint8_t			 sc_curchan;
};

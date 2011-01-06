/*	$OpenBSD: if_athn_usb.h,v 1.1 2011/01/06 07:27:15 damien Exp $	*/

/*-
 * Copyright (c) 2011 Damien Bergamini <damien.bergamini@free.fr>
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

/* Maximum number of STAs firmware can handle. */
#define AR_USB_MAX_STA	8

#define AR_USB_DEFAULT_NF	(-95)

/* USB requests. */
#define AR_FW_DOWNLOAD		0x30
#define AR_FW_DOWNLOAD_COMP	0x31

/* USB endpoints addresses. */
#define AR_PIPE_TX_DATA	(UE_DIR_OUT | 1)
#define AR_PIPE_RX_DATA	(UE_DIR_IN  | 2)
#define AR_PIPE_RX_INTR	(UE_DIR_IN  | 3)
#define AR_PIPE_TX_INTR	(UE_DIR_OUT | 4)

/* Wireless module interface commands. */
#define AR_WMI_CMD_ECHO			0x001
#define AR_WMI_CMD_ACCESS_MEMORY	0x002
#define AR_WMI_CMD_DISABLE_INTR		0x003
#define AR_WMI_CMD_ENABLE_INTR		0x004
#define AR_WMI_CMD_RX_LINK		0x005
#define AR_WMI_CMD_ATH_INIT		0x006
#define AR_WMI_CMD_ABORT_TXQ		0x007
#define AR_WMI_CMD_STOP_TX_DMA		0x008
#define AR_WMI_CMD_STOP_DMA_RECV	0x009
#define AR_WMI_CMD_ABORT_TX_DMA		0x00a
#define AR_WMI_CMD_DRAIN_TXQ		0x00b
#define AR_WMI_CMD_DRAIN_TXQ_ALL	0x00c
#define AR_WMI_CMD_START_RECV		0x00d
#define AR_WMI_CMD_STOP_RECV		0x00e
#define AR_WMI_CMD_FLUSH_RECV		0x00f
#define AR_WMI_CMD_SET_MODE		0x010
#define AR_WMI_CMD_RESET		0x011
#define AR_WMI_CMD_NODE_CREATE		0x012
#define AR_WMI_CMD_NODE_REMOVE		0x013
#define AR_WMI_CMD_VAP_REMOVE		0x014
#define AR_WMI_CMD_VAP_CREATE		0x015
#define AR_WMI_CMD_BEACON_UPDATE	0x016
#define AR_WMI_CMD_REG_READ		0x017
#define AR_WMI_CMD_REG_WRITE		0x018
#define AR_WMI_CMD_RC_STATE_CHANGE	0x019
#define AR_WMI_CMD_RC_RATE_UPDATE	0x01a
#define AR_WMI_CMD_DEBUG_INFO		0x01b
#define AR_WMI_CMD_HOST_ATTACH		0x01c
#define AR_WMI_CMD_TARGET_IC_UPDATE	0x01d
#define AR_WMI_CMD_TGT_STATS		0x01e
#define AR_WMI_CMD_TX_AGGR_ENABLE	0x01f
#define AR_WMI_CMD_TGT_DETACH		0x020
#define AR_WMI_CMD_TGT_TXQ_ENABLE	0x021
#define AR_WMI_CMD_AGGR_LIMIT		0x026
/* Wireless module interface events. */
#define AR_WMI_EVT_TGT_RDY		0x001
#define AR_WMI_EVT_SWBA			0x002
#define AR_WMI_EVT_FATAL		0x003
#define AR_WMI_EVT_TXTO			0x004
#define AR_WMI_EVT_BMISS		0x005
#define AR_WMI_EVT_WLAN_TXCOMP		0x006
#define AR_WMI_EVT_DELBA		0x007
#define AR_WMI_EVT_TXRATE		0x008

/* Structure for service AR_SVC_WMI_CONTROL. */
struct ar_wmi_cmd_hdr {
	uint16_t	cmd_id;
#define AR_WMI_EVT_FLAG	0x1000

	uint16_t	seq_no;
} __packed;

/* Values for AR_WMI_CMD_SET_MODE. */
#define AR_HTC_MODE_AUTO	0
#define AR_HTC_MODE_11A		1
#define AR_HTC_MODE_11B		2
#define AR_HTC_MODE_11G		3
#define AR_HTC_MODE_FH		4
#define AR_HTC_MODE_TURBO_A	5
#define AR_HTC_MODE_TURBO_G	6
#define AR_HTC_MODE_11NA	7
#define AR_HTC_MODE_11NG	8

#define AR_MAX_WRITE_COUNT	32
/* Structure for command AR_WMI_CMD_REG_WRITE. */
struct ar_wmi_cmd_reg_write {
	uint32_t	addr;
	uint32_t	val;
} __packed;

/* Structure for command AR_WMI_CMD_NODE_{CREATE,REMOVE}. */
struct ar_htc_target_sta {
	uint16_t	associd;
	uint16_t	txpower;
	uint32_t	pariwisekey;
	uint8_t		macaddr[IEEE80211_ADDR_LEN];
	uint8_t		bssid[IEEE80211_ADDR_LEN];
	uint8_t		sta_index;
	uint8_t		vif_index;
	uint8_t		vif_sta;
	uint16_t	flags;
#define AR_HTC_STA_AUTH	0x0001
#define AR_HTC_STA_QOS	0x0002
#define AR_HTC_STA_ERP	0x0004
#define AR_HTC_STA_HT	0x0008

	uint16_t	htcap;
	uint8_t		valid;
	uint16_t	capinfo;
	uint32_t	reserved[2];
	uint16_t	txseqmgmt;
	uint8_t		is_vif_sta;
	uint16_t	maxampdu;
	uint16_t	iv16;
	uint32_t	iv32;
} __packed;

/* Structures for command AR_WMI_CMD_RC_RATE_UPDATE. */
#define AR_HTC_RATE_MAX	30
struct ar_htc_rateset {
	uint8_t		rs_nrates;
	uint8_t		rs_rates[AR_HTC_RATE_MAX];
} __packed;

struct ar_htc_target_rate {
	uint8_t			sta_index;
	uint8_t			isnew;
	uint32_t		capflags;
#define AR_RC_DS_FLAG	0x00000001
#define AR_RC_TS_FLAG	0x00000002
#define AR_RC_40_FLAG	0x00000004
#define AR_RC_SGI_FLAG	0x00000008
#define AR_RC_HT_FLAG	0x00000010

	struct ar_htc_rateset	lg_rates;
	struct ar_htc_rateset	ht_rates;
} __packed;

/* Structure for command AR_WMI_CMD_TX_AGGR_ENABLE. */
struct ar_htc_target_aggr {
	uint8_t		sta_index;
	uint8_t		tidno;
	uint8_t		aggr_enable;
	uint8_t		padding;
} __packed;

/* Structure for command AR_WMI_CMD_VAP_CREATE. */
struct ar_htc_target_vif {
	uint8_t		index;
	uint8_t		des_bssid[IEEE80211_ADDR_LEN];
	uint32_t	opmode;
#define AR_HTC_M_IBSS		0
#define AR_HTC_M_STA		1
#define AR_HTC_M_WDS		2
#define AR_HTC_M_AHDEMO		3
#define AR_HTC_M_HOSTAP		6
#define AR_HTC_M_MONITOR	8

	uint8_t		myaddr[IEEE80211_ADDR_LEN];
	uint8_t		bssid[IEEE80211_ADDR_LEN];
	uint32_t	flags;
	uint32_t	flags_ext;
	uint16_t	ps_sta;
	uint16_t	rtsthreshold;
	uint8_t		ath_cap;
	int8_t		mcast_rate;
} __packed;

/* Structure for command AM_WMI_CMD_TARGET_IC_UPDATE. */
struct ar_htc_cap_target {
	uint32_t	flags;
	uint32_t	flags_ext;
	uint32_t	ampdu_limit;
	uint8_t		ampdu_subframes;
	uint8_t		ht_txchainmask;
	uint8_t		lg_txchainmask;
	uint8_t		rtscts_ratecode;
	uint8_t		protmode;
} __packed;

/* Structure for event AR_WMI_EVT_TXRATE. */
struct ar_wmi_evt_txrate {
	uint32_t	txrate;
	uint8_t		rssi_thresh;
	uint8_t		per;
} __packed;

/* HTC header. */
struct ar_htc_frame_hdr {
	uint8_t		endpoint_id;
	uint8_t		flags;
#define AR_HTC_FLAG_TRAILER	0x02

	uint16_t	payload_len;
	uint8_t		control[4];
} __packed;

/* Structure for HTC enpoint id 0. */
struct ar_htc_msg_hdr {
	uint16_t	msg_id;
#define AR_HTC_MSG_READY		0x0001
#define AR_HTC_MSG_CONN_SVC		0x0002
#define AR_HTC_MSG_CONN_SVC_RSP		0x0003
#define AR_HTC_MSG_SETUP_COMPLETE	0x0004
#define AR_HTC_MSG_CONF_PIPE		0x0005
#define AR_HTC_MSG_CONF_PIPE_RSP	0x0006
} __packed;

/* Structure for services AR_SVC_WMI_DATA_{VO,VI,BE,BK}. */
struct ar_tx_frame {
	uint8_t		data_type;
#define AR_HTC_AMPDU	1
#define AR_HTC_NORMAL	2

	uint8_t		node_idx;
	uint8_t		vif_idx;
	uint8_t		tid;
	uint32_t	flags;
#define AR_HTC_TX_CTSONLY	0x00000001
#define AR_HTC_TX_RTSCTS	0x00000002
#define AR_HTC_TX_USE_MIN_RATE	0x00000100

	uint8_t		key_type;
	uint8_t		key_idx;
	uint8_t		reserved[26];
} __packed;

/* Structure for service AR_SVC_WMI_MGMT. */
struct ar_tx_mgmt {
	uint8_t		node_idx;
	uint8_t		vif_idx;
	uint8_t		tid;
	uint8_t		flags;
	uint8_t		key_type;
	uint8_t		key_idx;
	uint16_t	reserved;
} __packed;

/* Structure for service AR_SVC_WMI_BEACON. */
struct ar_tx_bcn {
	uint8_t		len_changed;
	uint8_t		vif_idx;
	uint16_t	rev;
} __packed;

/* Structure for message AR_HTC_MSG_READY. */
struct ar_htc_msg_ready {
	uint16_t	credits;
	uint16_t	credits_size;
	uint8_t		max_endpoints;
	uint8_t		reserved;
} __packed;

/* Structure for message AR_HTC_MSG_CONF_PIPE. */
struct ar_htc_msg_config_pipe {
	uint8_t		pipe_id;
	uint8_t		credits;
} __packed;

/* Structure for message AR_HTC_MSG_CONN_SVC. */
struct ar_htc_msg_conn_svc {
	uint16_t	svc_id;
	uint16_t	conn_flags;
	uint8_t		dl_pipeid;
	uint8_t		ul_pipeid;
	uint8_t		svc_meta_len;
	uint8_t		reserved;
} __packed;

/* Structure for message AR_HTC_MSG_CONN_SVC_RSP. */
struct ar_htc_msg_conn_svc_rsp {
	uint16_t	svc_id;
	uint8_t		status;
#define AR_HTC_SVC_SUCCESS	0
#define AR_HTC_SVC_NOT_FOUND	1
#define AR_HTC_SVC_FAILED	2
#define AR_HTC_SVC_NO_RESOURCES	3
#define AR_HTC_SVC_NO_MORE_EP	4

	uint8_t		endpoint_id;
	uint16_t	max_msg_len;
	uint8_t		svc_meta_len;
	uint8_t		reserved;
} __packed;

#define AR_SVC(grp, idx)	((grp) << 8 | (idx))
#define AR_SVC_IDX(svc)		((svc) & 0xff)
/* Service groups. */
#define AR_SVC_GRP_RSVD		0
#define AR_SVC_GRP_WMI		1
/* Service identifiers for WMI group. */
#define AR_SVC_WMI_CONTROL	AR_SVC(AR_SVC_GRP_WMI, 0)
#define AR_SVC_WMI_BEACON	AR_SVC(AR_SVC_GRP_WMI, 1)
#define AR_SVC_WMI_CAB		AR_SVC(AR_SVC_GRP_WMI, 2)
#define AR_SVC_WMI_UAPSD	AR_SVC(AR_SVC_GRP_WMI, 3)
#define AR_SVC_WMI_MGMT		AR_SVC(AR_SVC_GRP_WMI, 4)
#define AR_SVC_WMI_DATA_VO	AR_SVC(AR_SVC_GRP_WMI, 5)
#define AR_SVC_WMI_DATA_VI	AR_SVC(AR_SVC_GRP_WMI, 6)
#define AR_SVC_WMI_DATA_BE	AR_SVC(AR_SVC_GRP_WMI, 7)
#define AR_SVC_WMI_DATA_BK	AR_SVC(AR_SVC_GRP_WMI, 8)

struct ar_stream_hdr {
	uint16_t	len;
	uint16_t	tag;
#define AR_USB_RX_STREAM_TAG	0x4e00
#define AR_USB_TX_STREAM_TAG	0x697e
} __packed __attribute__((aligned(4)));

#define AR_MAX_CHAINS	3

/* Rx descriptor. */
struct ar_rx_status {
	uint64_t	rs_tstamp;
	uint16_t	rs_datalen;
	uint8_t		rs_status;
	uint8_t		rs_phyerr;
	int8_t		rs_rssi;
	int8_t		rs_rssi_ctl[AR_MAX_CHAINS];
	int8_t		rs_rssi_ext[AR_MAX_CHAINS];
	uint8_t		rs_keyix;
	uint8_t		rs_rate;
	uint8_t		rs_antenna;
	uint8_t		rs_more;
	uint8_t		rs_isaggr;
	uint8_t		rs_moreaggr;
	uint8_t		rs_num_delims;
	uint8_t		rs_flags;
#define AR_RXS_FLAG_GI		0x04
#define AR_RXS_FLAG_2040	0x08

	uint8_t		rs_dummy;
	uint32_t	rs_evm[AR_MAX_CHAINS];
} __packed __attribute__((aligned(4)));


/*
 * Driver definitions.
 */
#define ATHN_USB_RX_LIST_COUNT	1
#define ATHN_USB_TX_LIST_COUNT	8

#define ATHN_USB_HOST_CMD_RING_COUNT	32

#define ATHN_USB_RXBUFSZ	(8 * 1024)	/* XXX Linux 16K */
#define ATHN_USB_TXBUFSZ			\
	((sizeof(struct ar_stream_hdr) +	\
	  sizeof(struct ar_htc_frame_hdr) +	\
	  sizeof(struct ar_tx_frame) +		\
	  IEEE80211_MAX_LEN + 3) & ~3)
#define ATHN_USB_TXCMDSZ	512

#define ATHN_USB_TX_TIMEOUT	5000	/* ms */
#define ATHN_USB_CMD_TIMEOUT	1000	/* ms */

struct athn_usb_softc;

struct athn_usb_rx_stream {
	struct mbuf	*m;
	int		moff;
	int		left;
};

struct athn_usb_rx_data {
	struct athn_usb_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
};

struct athn_usb_tx_data {
	struct athn_usb_softc		*sc;
	usbd_xfer_handle		xfer;
	uint8_t				*buf;
	TAILQ_ENTRY(athn_usb_tx_data)	next;
};

struct athn_usb_host_cmd {
	void	(*cb)(struct athn_usb_softc *, void *);
	uint8_t	data[256];
};

struct athn_usb_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct athn_usb_cmd_key {
	struct ieee80211_node	*ni;
	struct ieee80211_key	*key;
};

struct athn_usb_aggr_cmd {
	uint8_t	sta_index;
	uint8_t	tid;
};

struct athn_usb_host_cmd_ring {
	struct athn_usb_host_cmd	cmd[ATHN_USB_HOST_CMD_RING_COUNT];
	int				cur;
	int				next;
	int				queued;
};

struct athn_usb_softc {
	struct athn_softc		sc_sc;
#define usb_dev	sc_sc.sc_dev

	/* USB specific goo. */
	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;
	struct usb_task			sc_task;

	u_int				flags;
#define ATHN_USB_FLAG_AR7010	0x01

	struct athn_usb_rx_stream	rx_stream;

	usbd_pipe_handle		tx_data_pipe;
	usbd_pipe_handle		rx_data_pipe;
	usbd_pipe_handle		rx_intr_pipe;
	usbd_pipe_handle		tx_intr_pipe;
	uint8_t 			*ibuf;

	struct ar_wmi_cmd_reg_write	wbuf[AR_MAX_WRITE_COUNT];
	int				wcount;

	int				wmi_done;
	uint16_t			wmi_seq_no;
	uint16_t			wait_cmd_id;
	uint16_t			wait_msg_id;
	void				*obuf;
	struct ar_htc_msg_conn_svc_rsp	*msg_conn_svc_rsp;

	struct athn_usb_host_cmd_ring	cmdq;
	struct athn_usb_rx_data		rx_data[ATHN_USB_RX_LIST_COUNT];
	struct athn_usb_tx_data		tx_data[ATHN_USB_TX_LIST_COUNT];
	TAILQ_HEAD(, athn_usb_tx_data)	tx_free_list;
	struct athn_usb_tx_data		tx_cmd;
	struct athn_usb_tx_data		*bcndata;

	uint8_t				ep_ctrl;
	uint8_t				ep_bcn;
	uint8_t				ep_cab;
	uint8_t				ep_uapsd;
	uint8_t				ep_mgmt;
	uint8_t				ep_data[EDCA_NUM_AC];
};

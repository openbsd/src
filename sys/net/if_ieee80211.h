/*	$OpenBSD: if_ieee80211.h,v 1.10 2004/01/07 00:13:22 fgsch Exp $	*/
/*	$NetBSD: if_ieee80211.h,v 1.36 2003/07/06 20:54:24 dyoung Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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

#ifndef _NET_IF_IEEE80211_H_
#define _NET_IF_IEEE80211_H_

#define	IEEE80211_ADDR_LEN			ETHER_ADDR_LEN

/* IEEE 802.11 PLCP header */
struct ieee80211_plcp_hdr {
	u_int16_t	i_sfd;		/* IEEE80211_PLCP_SFD */
	u_int8_t	i_signal;	/* MPDU data rate in 100Kb/s */
	u_int8_t	i_service;	/* IEEE80211_PLCP_SERVICE */
	u_int16_t	i_length;	/* MPDU duration in microseconds */
	u_int16_t	i_crc;		/* CRC16 of i_signal, i_service,
					 * i_length
					 */
} __packed;

#define IEEE80211_PLCP_SFD	0xF3A0
#define IEEE80211_PLCP_SERVICE	0x00

/*
 * generic definitions for IEEE 802.11 frames
 */
struct ieee80211_frame {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	/* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
	/* see below */
};

struct ieee80211_frame_addr4 {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	u_int8_t	i_addr4[IEEE80211_ADDR_LEN];
};

#define	IEEE80211_FC0_VERSION_MASK		0x03
#define	IEEE80211_FC0_VERSION_SHIFT		0
#define	IEEE80211_FC0_VERSION_0			0x00
#define	IEEE80211_FC0_TYPE_MASK			0x0c
#define	IEEE80211_FC0_TYPE_SHIFT		2
#define	IEEE80211_FC0_TYPE_MGT			0x00
#define	IEEE80211_FC0_TYPE_CTL			0x04
#define	IEEE80211_FC0_TYPE_DATA			0x08

#define	IEEE80211_FC0_SUBTYPE_MASK		0xf0
#define	IEEE80211_FC0_SUBTYPE_SHIFT		4
/* for TYPE_MGT */
#define	IEEE80211_FC0_SUBTYPE_ASSOC_REQ		0x00
#define	IEEE80211_FC0_SUBTYPE_ASSOC_RESP	0x10
#define	IEEE80211_FC0_SUBTYPE_REASSOC_REQ	0x20
#define	IEEE80211_FC0_SUBTYPE_REASSOC_RESP	0x30
#define	IEEE80211_FC0_SUBTYPE_PROBE_REQ		0x40
#define	IEEE80211_FC0_SUBTYPE_PROBE_RESP	0x50
#define	IEEE80211_FC0_SUBTYPE_BEACON		0x80
#define	IEEE80211_FC0_SUBTYPE_ATIM		0x90
#define	IEEE80211_FC0_SUBTYPE_DISASSOC		0xa0
#define	IEEE80211_FC0_SUBTYPE_AUTH		0xb0
#define	IEEE80211_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define	IEEE80211_FC0_SUBTYPE_PS_POLL		0xa0
#define	IEEE80211_FC0_SUBTYPE_RTS		0xb0
#define	IEEE80211_FC0_SUBTYPE_CTS		0xc0
#define	IEEE80211_FC0_SUBTYPE_ACK		0xd0
#define	IEEE80211_FC0_SUBTYPE_CF_END		0xe0
#define	IEEE80211_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define	IEEE80211_FC0_SUBTYPE_DATA		0x00
#define	IEEE80211_FC0_SUBTYPE_CF_ACK		0x10
#define	IEEE80211_FC0_SUBTYPE_CF_POLL		0x20
#define	IEEE80211_FC0_SUBTYPE_CF_ACPL		0x30
#define	IEEE80211_FC0_SUBTYPE_NODATA		0x40
#define	IEEE80211_FC0_SUBTYPE_CFACK		0x50
#define	IEEE80211_FC0_SUBTYPE_CFPOLL		0x60
#define	IEEE80211_FC0_SUBTYPE_CF_ACK_CF_POLL	0x70

#define	IEEE80211_FC1_DIR_MASK			0x03
#define	IEEE80211_FC1_DIR_NODS			0x00	/* STA->STA */
#define	IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP  */
#define	IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA */
#define	IEEE80211_FC1_DIR_DSTODS		0x03	/* AP ->AP  */

#define	IEEE80211_FC1_MORE_FRAG			0x04
#define	IEEE80211_FC1_RETRY			0x08
#define	IEEE80211_FC1_PWR_MGT			0x10
#define	IEEE80211_FC1_MORE_DATA			0x20
#define	IEEE80211_FC1_WEP			0x40
#define	IEEE80211_FC1_ORDER			0x80

#define	IEEE80211_SEQ_FRAG_MASK			0x000f
#define	IEEE80211_SEQ_FRAG_SHIFT		0
#define	IEEE80211_SEQ_SEQ_MASK			0xfff0
#define	IEEE80211_SEQ_SEQ_SHIFT			4

#define	IEEE80211_NWID_LEN			32

/*
 * BEACON management packets
 *
 *	octet timestamp[8]
 *	octet beacon interval[2]
 *	octet capability information[2]
 *	information element
 *		octet elemid
 *		octet length
 *		octet information[length]
 */

typedef uint8_t *ieee80211_mgt_beacon_t;

#define	IEEE80211_BEACON_INTERVAL(beacon) \
	((beacon)[8] | ((beacon)[9] << 8))
#define	IEEE80211_BEACON_CAPABILITY(beacon) \
	((beacon)[10] | ((beacon)[11] << 8))

#define	IEEE80211_CAPINFO_ESS			0x0001
#define	IEEE80211_CAPINFO_IBSS			0x0002
#define	IEEE80211_CAPINFO_CF_POLLABLE		0x0004
#define	IEEE80211_CAPINFO_CF_POLLREQ		0x0008
#define	IEEE80211_CAPINFO_PRIVACY		0x0010
#define	IEEE80211_CAPINFO_SHORT_PREAMBLE	0x0020
#define	IEEE80211_CAPINFO_PBCC			0x0040
#define	IEEE80211_CAPINFO_CHNL_AGILITY		0x0080
#define	IEEE80211_CAPINFO_BITS			"\20\01ESS\02IBSS\03POLLABLE\04POLLREQ\05PRIVACY\06SHORT_PREAMBLE\07PBCC\08CHNL_AGILITY"

#define	IEEE80211_RATE_BASIC			0x80
#define	IEEE80211_RATE_VAL			0x7f

/* One Time Unit (TU) is 1Kus = 1024 microseconds. */
#define IEEE80211_DUR_TU		1024

/* IEEE 802.11b durations for DSSS PHY in microseconds */
#define IEEE80211_DUR_DS_LONG_PREAMBLE	144
#define IEEE80211_DUR_DS_SHORT_PREAMBLE	72
#define IEEE80211_DUR_DS_FAST_PLCPHDR	24
#define IEEE80211_DUR_DS_SLOW_PLCPHDR	48
#define IEEE80211_DUR_DS_SLOW_ACK	112
#define IEEE80211_DUR_DS_FAST_ACK	56
#define IEEE80211_DUR_DS_SLOW_CTS	112
#define IEEE80211_DUR_DS_FAST_CTS	56
#define IEEE80211_DUR_DS_SLOT		20
#define IEEE80211_DUR_DS_SIFS		10
#define IEEE80211_DUR_DS_PIFS	(IEEE80211_DUR_DS_SIFS + IEEE80211_DUR_DS_SLOT)
#define IEEE80211_DUR_DS_DIFS	(IEEE80211_DUR_DS_SIFS + \
				 2 * IEEE80211_DUR_DS_SLOT)
#define IEEE80211_DUR_DS_EIFS	(IEEE80211_DUR_DS_SIFS + \
				 IEEE80211_DUR_DS_SLOW_ACK + \
				 IEEE80211_DUR_DS_LONG_PREAMBLE + \
				 IEEE80211_DUR_DS_SLOW_PLCPHDR + \
				 IEEE80211_DUR_DIFS)

/*
 * Management information elements
 */

struct ieee80211_information {
	char	ssid[IEEE80211_NWID_LEN+1];
	struct rates {
		u_int8_t	*p;
	} rates;
	struct fh {
		u_int16_t	dwell;
		u_int8_t	set;
		u_int8_t	pattern;
		u_int8_t	index;
	} fh;
	struct ds {
		u_int8_t	channel;
	} ds;
	struct cf {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	maxdur[2];
		u_int8_t	dur[2];
	} cf;
	struct tim {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	bitctl;
		/* u_int8_t	pvt[251]; The driver needs to use this. */
	} tim;
	struct ibss {
		u_int16_t	atim;
	} ibss;
	struct challenge {
		u_int8_t	*p;
		u_int8_t	len;
	} challenge;
};

#define	IEEE80211_ELEMID_SSID			0
#define	IEEE80211_ELEMID_RATES			1
#define	IEEE80211_ELEMID_FHPARMS		2
#define	IEEE80211_ELEMID_DSPARMS		3
#define	IEEE80211_ELEMID_CFPARMS		4
#define	IEEE80211_ELEMID_TIM			5
#define	IEEE80211_ELEMID_IBSSPARMS		6
#define	IEEE80211_ELEMID_CHALLENGE		16

/*
 * AUTH management packets
 *
 *	octet algo[2]
 *	octet seq[2]
 *	octet status[2]
 *	octet chal.id
 *	octet chal.length
 *	octet chal.text[253]
 */

typedef u_int8_t *ieee80211_mgt_auth_t;

#define	IEEE80211_AUTH_ALGORITHM(auth) \
	((auth)[0] | ((auth)[1] << 8))
#define	IEEE80211_AUTH_TRANSACTION(auth) \
	((auth)[2] | ((auth)[3] << 8))
#define	IEEE80211_AUTH_STATUS(auth) \
	((auth)[4] | ((auth)[5] << 8))

#define	IEEE80211_AUTH_ALG_OPEN			0x0000
#define	IEEE80211_AUTH_ALG_SHARED		0x0001

#define	IEEE80211_AUTH_OPEN_REQUEST		1
#define	IEEE80211_AUTH_OPEN_RESPONSE		2

#define	IEEE80211_AUTH_SHARED_REQUEST		1
#define	IEEE80211_AUTH_SHARED_CHALLENGE		2
#define	IEEE80211_AUTH_SHARED_RESPONSE		3
#define	IEEE80211_AUTH_SHARED_PASS		4

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */

#define	IEEE80211_REASON_UNSPECIFIED		1
#define	IEEE80211_REASON_AUTH_EXPIRE		2
#define	IEEE80211_REASON_AUTH_LEAVE		3
#define	IEEE80211_REASON_ASSOC_EXPIRE		4
#define	IEEE80211_REASON_ASSOC_TOOMANY		5
#define	IEEE80211_REASON_NOT_AUTHED		6  
#define	IEEE80211_REASON_NOT_ASSOCED		7
#define	IEEE80211_REASON_ASSOC_LEAVE		8
#define	IEEE80211_REASON_ASSOC_NOT_AUTHED	9

#define	IEEE80211_STATUS_SUCCESS		0
#define	IEEE80211_STATUS_UNSPECIFIED		1
#define	IEEE80211_STATUS_CAPINFO		10
#define	IEEE80211_STATUS_NOT_ASSOCED		11
#define	IEEE80211_STATUS_OTHER			12
#define	IEEE80211_STATUS_ALG			13
#define	IEEE80211_STATUS_SEQUENCE		14
#define	IEEE80211_STATUS_CHALLENGE		15
#define	IEEE80211_STATUS_TIMEOUT		16
#define	IEEE80211_STATUS_TOOMANY		17
#define	IEEE80211_STATUS_BASIC_RATE		18
#define	IEEE80211_STATUS_SP_REQUIRED		19
#define	IEEE80211_STATUS_PBCC_REQUIRED		20
#define	IEEE80211_STATUS_CA_REQUIRED		21
#define	IEEE80211_STATUS_TOO_MANY_STATIONS	22
#define	IEEE80211_STATUS_RATES			23

#define	IEEE80211_WEP_KEYLEN			5	/* 40bit */
#define	IEEE80211_WEP_IVLEN			3	/* 24bit */
#define	IEEE80211_WEP_KIDLEN			1	/* 1 octet */
#define	IEEE80211_WEP_CRCLEN			4	/* CRC-32 */
#define	IEEE80211_WEP_NKID			4	/* number of key ids */

#define	IEEE80211_CRC_LEN			4

#define	IEEE80211_MTU				1500
#define	IEEE80211_MAX_LEN			(2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

#define IEEE80211_MAX_AID			2007

#define IEEE80211_AID_SET(b, w) \
	((w)[((b) & ~0xc000) / 32] |= (1 << (((b) & ~0xc000) % 32)))
#define IEEE80211_AID_CLR(b, w) \
	((w)[((b) & ~0xc000) / 32] &= ~(1 << (((b) & ~0xc000) % 32)))
#define IEEE80211_AID_ISSET(b, w) \
	((w)[((b) & ~0xc000) / 32] & (1 << (((b) & ~0xc000) % 32)))


/*
 * ioctls
 */

/* nwid is pointed at by ifr.ifr_data */
struct ieee80211_nwid {
	u_int8_t	i_len;
	u_int8_t	i_nwid[IEEE80211_NWID_LEN];
};

#define	SIOCS80211NWID		_IOWR('i', 230, struct ifreq)
#define	SIOCG80211NWID		_IOWR('i', 231, struct ifreq)

/* the first member must be matched with struct ifreq */
struct ieee80211_nwkey {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_wepon;		/* wep enabled flag */
	int		i_defkid;		/* default encrypt key id */
	struct {
		int		i_keylen;
		u_int8_t	*i_keydat;
	}		i_key[IEEE80211_WEP_NKID];
};
#define	SIOCS80211NWKEY		 _IOW('i', 232, struct ieee80211_nwkey)
#define	SIOCG80211NWKEY		_IOWR('i', 233, struct ieee80211_nwkey)
/* i_wepon */
#define	IEEE80211_NWKEY_OPEN	0		/* No privacy */
#define	IEEE80211_NWKEY_WEP	1		/* WEP enabled */
#define	IEEE80211_NWKEY_EAP	2		/* EAP enabled */
#define	IEEE80211_NWKEY_PERSIST	0x100		/* designate persist keyset */

/* power management parameters */
struct ieee80211_power {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_enabled;		/* 1 == on, 0 == off */
	int		i_maxsleep;		/* max sleep in ms */
};
#define	SIOCS80211POWER		 _IOW('i', 234, struct ieee80211_power)
#define	SIOCG80211POWER		_IOWR('i', 235, struct ieee80211_power)

struct ieee80211_auth {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_authtype;
};

#define	IEEE80211_AUTH_NONE	0
#define	IEEE80211_AUTH_OPEN	1
#define	IEEE80211_AUTH_SHARED	2

#define	SIOCS80211AUTH		 _IOW('i', 236, struct ieee80211_auth)
#define	SIOCG80211AUTH		_IOWR('i', 237, struct ieee80211_auth)

struct ieee80211_channel {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	u_int16_t	i_channel;
};

#define	IEEE80211_CHAN_ANY	0xffff

#define	SIOCS80211CHANNEL	 _IOW('i', 238, struct ieee80211_channel)
#define	SIOCG80211CHANNEL	_IOWR('i', 239, struct ieee80211_channel)

struct ieee80211_bssid {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	u_int8_t	i_bssid[IEEE80211_ADDR_LEN];
};

#define	SIOCS80211BSSID		 _IOW('i', 240, struct ieee80211_bssid)
#define	SIOCG80211BSSID		_IOWR('i', 241, struct ieee80211_bssid)

#ifdef _KERNEL

#define	IEEE80211_ASCAN_WAIT	2		/* active scan wait */
#define	IEEE80211_PSCAN_WAIT 	5		/* passive scan wait */
#define	IEEE80211_TRANS_WAIT 	5		/* transition wait */
#define	IEEE80211_INACT_WAIT	5		/* inactivity timer interval */
#define	IEEE80211_INACT_MAX	(300/IEEE80211_INACT_WAIT)

/*
 * Structure for IEEE 802.11 drivers.
 */

#define	IEEE80211_CHAN_MAX	255
#define	IEEE80211_RATE_SIZE	12
#define	IEEE80211_KEYBUF_SIZE	16
#define	IEEE80211_NODE_HASHSIZE	32
/* simple hash is enough for variation of macaddr */
#define	IEEE80211_NODE_HASH(addr)	\
	(((u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % IEEE80211_NODE_HASHSIZE)

enum ieee80211_phytype {
	IEEE80211_T_DS,
	IEEE80211_T_FH,
	IEEE80211_T_OFDM
};

enum ieee80211_opmode {
	IEEE80211_M_STA = 1,		/* infrastructure station */
	IEEE80211_M_IBSS = 0,		/* IBSS (adhoc) station */
	IEEE80211_M_AHDEMO = 3,		/* Old lucent compatible adhoc demo */
	IEEE80211_M_HOSTAP = 6,		/* Software Access Point */
	IEEE80211_M_MONITOR = 8		/* Monitor mode (doesn't correspond
					   to a PRISM port like the others) */
};

enum ieee80211_state {
	IEEE80211_S_INIT,		/* default state */
	IEEE80211_S_SCAN,		/* scanning */
	IEEE80211_S_AUTH,		/* try to authenticate */
	IEEE80211_S_ASSOC,		/* try to assoc */
	IEEE80211_S_RUN			/* associated */
};

/*
 * Node specific information.
 */
struct ieee80211_node {
	TAILQ_ENTRY(ieee80211_node)	ni_list;
	LIST_ENTRY(ieee80211_node)	ni_hash;

	/* hardware */
	u_int8_t		ni_rssi;
	u_int32_t		ni_rstamp;

	/* header */
	u_int8_t		ni_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t		ni_bssid[IEEE80211_ADDR_LEN];

	/* beacon, probe response */
	u_int8_t		ni_tstamp[8];
	u_int16_t		ni_intval;
	u_int16_t		ni_capinfo;
	u_int8_t		ni_esslen;
	u_int8_t		ni_essid[IEEE80211_NWID_LEN];
	int			ni_nrate;
	u_int8_t		ni_rates[IEEE80211_RATE_SIZE];
	u_int8_t		ni_chan;
	u_int16_t		ni_fhdwell;	/* FH only */
	u_int8_t		ni_fhindex;	/* FH only */

	/* power saving mode */

	u_int8_t		ni_pwrsave;
	struct ifqueue		ni_savedq;	/* packets queued for pspoll */

	/* others */
	u_int16_t		ni_associd;	/* assoc response */
	u_int16_t		ni_txseq;	/* seq to be transmitted */
	u_int16_t		ni_rxseq;	/* seq previous received */
	int			ni_fails;	/* failure count to associate */
	int			ni_inact;	/* inactivity mark count */
	int			ni_txrate;	/* index to ni_rates[] */
	void			*ni_private;	/* driver private */
};

/* ni_chan encoding for FH phy */
#define	IEEE80211_FH_CHANMOD	80
#define	IEEE80211_FH_CHAN(set,pat)	(((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define	IEEE80211_FH_CHANSET(chan)	((chan)/IEEE80211_FH_CHANMOD+1)
#define	IEEE80211_FH_CHANPAT(chan)	((chan)%IEEE80211_FH_CHANMOD)

#define IEEE80211_PS_SLEEP	0x1	/* STA is in power saving mode */

#define IEEE80211_PS_MAX_QUEUE	50	/* maximum saved packets */

struct ieee80211_wepkey {
	int			wk_len;
	u_int8_t		wk_key[IEEE80211_KEYBUF_SIZE];
};

struct ieee80211com {
#ifdef __NetBSD__
	struct ethercom		ic_ec;
#endif
#ifdef __FreeBSD__
	struct arpcom		ic_ac;
	struct mtx		ic_mtx;
#endif
#ifdef __OpenBSD__
	struct arpcom		ic_ac;
#endif
	void			(*ic_recv_mgmt[16])(struct ieee80211com *,
				    struct mbuf *, int, u_int32_t);
	int			(*ic_send_mgmt[16])(struct ieee80211com *,
				    struct ieee80211_node *, int, int);
	int			(*ic_newstate)(void *, enum ieee80211_state);
	int			(*ic_chancheck)(void *, u_char *);
	int			(*ic_set_tim)(struct ieee80211com *, int, int);
	u_int8_t		ic_myaddr[IEEE80211_ADDR_LEN];
	u_int8_t		ic_sup_rates[IEEE80211_RATE_SIZE];
	u_char			ic_chan_avail[roundup(IEEE80211_CHAN_MAX,NBBY)];
	u_char			ic_chan_active[roundup(IEEE80211_CHAN_MAX, NBBY)];
	u_char			ic_chan_scan[roundup(IEEE80211_CHAN_MAX, NBBY)];
	struct ifqueue		ic_mgtq;
	struct ifqueue		ic_pwrsaveq;
	int			ic_flags;
	int                     ic_hdrlen;	/* either 0 or extended header
						 * length, e.g. for addr4
						 */

	enum ieee80211_phytype	ic_phytype;
	enum ieee80211_opmode	ic_opmode;
	enum ieee80211_state	ic_state;
	caddr_t			ic_rawbpf;	/* packet filter structure */
	struct ieee80211_node	ic_bss;		/* information for this node */
	int			ic_node_privlen;/* size for ni_private */
	void			(*ic_node_free)(struct ieee80211com *,
				    struct ieee80211_node *);	/* callback */
	u_int8_t		ic_ibss_chan;
	int			ic_fixed_rate;	/* index to ic_sup_rates[] */
	TAILQ_HEAD(, ieee80211_node) ic_node;	/* information of all nodes */
	LIST_HEAD(, ieee80211_node) ic_hash[IEEE80211_NODE_HASHSIZE];
	u_int16_t		ic_lintval;	/* listen interval */
	int			ic_mgt_timer;	/* mgmt timeout */
	int			ic_scan_timer;	/* scant wait */
	int			ic_inact_timer;	/* inactivity timer wait */
	int			ic_des_esslen;
	u_int8_t		ic_des_essid[IEEE80211_NWID_LEN];
	int			ic_des_chan;	/* desired channel */
	u_int8_t		ic_des_bssid[IEEE80211_ADDR_LEN];
	struct ieee80211_wepkey	ic_nw_keys[IEEE80211_WEP_NKID];
	int			ic_wep_txkey;	/* default tx key index */
	void			*ic_wep_ctx;	/* wep crypt context */
	int			ic_iv_flag;
	u_int32_t		ic_iv;		/* initial vector for wep */
	u_int32_t		ic_aid_bitmap[IEEE80211_MAX_AID / 32 + 1];
	u_int16_t		ic_max_aid;
	struct ifmedia		ic_media;
};
#ifdef __NetBSD__
#define	ic_if		ic_ec.ec_if
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#define	ic_if		ic_ac.ac_if
#endif
#define	ic_softc	ic_if.if_softc

#define IEEE80211_HEADER_LEN(ic) (((ic)->ic_hdrlen > 0) \
	? (ic)->ic_hdrlen : sizeof(struct ieee80211_frame))

#define	IEEE80211_SEND_MGMT(ic,ni,type,arg)	do {			      \
	if ((ic)->ic_send_mgmt[(type)>>IEEE80211_FC0_SUBTYPE_SHIFT] != NULL)  \
		(*(ic)->ic_send_mgmt[(type)>>IEEE80211_FC0_SUBTYPE_SHIFT])    \
		    (ic,ni,type,arg);					      \
} while (/*CONSTCOND*/ 0)

#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#define	IEEE80211_ADDR_COPY(dst,src)	memcpy(dst,src,IEEE80211_ADDR_LEN)

#define	IEEE80211_IS_MULTICAST(a)	ETHER_IS_MULTICAST(a)

/* ic_flags */
#define	IEEE80211_F_ASCAN	0x00000001	/* STATUS: active scan */
#define	IEEE80211_F_SIBSS	0x00000002	/* STATUS: start IBSS */
#define	IEEE80211_F_WEPON	0x00000100	/* CONF: WEP enabled */
#define	IEEE80211_F_IBSSON	0x00000200	/* CONF: IBSS creation enable */
#define	IEEE80211_F_PMGTON	0x00000400	/* CONF: Power mgmt enable */
#define	IEEE80211_F_DESBSSID	0x00000800	/* CONF: des_bssid is set */
#define	IEEE80211_F_SCANAP	0x00001000	/* CONF: Scanning AP */
#define	IEEE80211_F_HASWEP	0x00010000	/* CAPABILITY: WEP available */
#define	IEEE80211_F_HASIBSS	0x00020000	/* CAPABILITY: IBSS available */
#define	IEEE80211_F_HASPMGT	0x00040000	/* CAPABILITY: Power mgmt */
#define	IEEE80211_F_HASHOSTAP	0x00080000	/* CAPABILITY: HOSTAP avail */
#define	IEEE80211_F_HASAHDEMO	0x00100000	/* CAPABILITY: Old Adhoc Demo */
#define	IEEE80211_F_HASMONITOR	0x00200000	/* CAPABILITY: Monitor mode */

/* flags for ieee80211_fix_rate() */
#define	IEEE80211_F_DOSORT	0x00000001	/* sort rate list */
#define	IEEE80211_F_DOFRATE	0x00000002	/* use fixed rate */
#define	IEEE80211_F_DONEGO	0x00000004	/* calc negotiated rate */
#define	IEEE80211_F_DODEL	0x00000008	/* delete ignore rate */

void	ieee80211_ifattach(struct ifnet *);
void	ieee80211_ifdetach(struct ifnet *);
void	ieee80211_input(struct ifnet *, struct mbuf *, int, u_int32_t);
int	ieee80211_mgmt_output(struct ifnet *, struct ieee80211_node *,
    struct mbuf *, int);
struct mbuf *ieee80211_encap(struct ifnet *, struct mbuf *);
struct mbuf *ieee80211_decap(struct ifnet *, struct mbuf *);
int	ieee80211_ioctl(struct ifnet *, u_long, caddr_t);
void	ieee80211_print_essid(u_int8_t *, int);
void	ieee80211_dump_pkt(u_int8_t *, int, int, int);
void	ieee80211_watchdog(struct ifnet *);
void	ieee80211_next_scan(struct ifnet *);
void	ieee80211_end_scan(struct ifnet *);
void	ieee80211_create_ibss(struct ieee80211com *);
int	ieee80211_match_bss(struct ieee80211com *, struct ieee80211_node *);
int	ieee80211_get_rate(struct ieee80211com *);
int	ieee80211_get_channel(struct ieee80211com *);
struct ieee80211_node *ieee80211_alloc_node(struct ieee80211com *, u_int8_t *,
    int);
struct ieee80211_node *ieee80211_find_node(struct ieee80211com *, u_int8_t *);
void	ieee80211_free_node(struct ieee80211com *, struct ieee80211_node *);
void	ieee80211_free_allnodes(struct ieee80211com *);
int	ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_node *, int);
int	ieee80211_new_state(struct ifnet *, enum ieee80211_state, int);
struct mbuf *ieee80211_wep_crypt(struct ifnet *, struct mbuf *, int);
int	ieee80211_rate2media(int, enum ieee80211_phytype);
int	ieee80211_media2rate(int, enum ieee80211_phytype);

int	ieee80211_cfgget(struct ifnet *, u_long, caddr_t);
int	ieee80211_cfgset(struct ifnet *, u_long, caddr_t);

void	ieee80211_pwrsave(struct ieee80211com *, struct ieee80211_node *, 
			  struct mbuf *);

int	ieee80211_media_change(struct ifnet *);
void	ieee80211_media_status(struct ifnet *, struct ifmediareq *);

#endif /* _KERNEL */

#endif /* _NET_IF_IEEE80211_H_ */

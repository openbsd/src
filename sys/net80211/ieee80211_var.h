/*	$OpenBSD: ieee80211_var.h,v 1.32 2007/07/28 11:13:41 damien Exp $	*/
/*	$NetBSD: ieee80211_var.h,v 1.7 2004/05/06 03:07:10 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net80211/ieee80211_var.h,v 1.15 2004/04/05 22:10:26 sam Exp $
 */
#ifndef _NET80211_IEEE80211_VAR_H_
#define _NET80211_IEEE80211_VAR_H_

/*
 * Definitions for IEEE 802.11 drivers.
 */

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>		/* for ieee80211_stats */
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_proto.h>

#define	IEEE80211_CHAN_MAX	255
#define	IEEE80211_CHAN_ANY	0xffff		/* token for ``any channel'' */
#define	IEEE80211_CHAN_ANYC \
	((struct ieee80211_channel *) IEEE80211_CHAN_ANY)

#define	IEEE80211_TXPOWER_MAX	100	/* max power */
#define	IEEE80211_TXPOWER_MIN	-50	/* kill radio (if possible) */

enum ieee80211_phytype {
	IEEE80211_T_DS,			/* direct sequence spread spectrum */
	IEEE80211_T_FH,			/* frequency hopping */
	IEEE80211_T_OFDM,		/* frequency division multiplexing */
	IEEE80211_T_TURBO,		/* high rate OFDM, aka turbo mode */
	IEEE80211_T_XR		        /* extended range mode */
};
#define	IEEE80211_T_CCK	IEEE80211_T_DS	/* more common nomenclature */

/* XXX not really a mode; there are really multiple PHY's */
enum ieee80211_phymode {
	IEEE80211_MODE_AUTO	= 0,	/* autoselect */
	IEEE80211_MODE_11A	= 1,	/* 5GHz, OFDM */
	IEEE80211_MODE_11B	= 2,	/* 2GHz, CCK */
	IEEE80211_MODE_11G	= 3,	/* 2GHz, OFDM */
	IEEE80211_MODE_FH	= 4,	/* 2GHz, GFSK */
	IEEE80211_MODE_TURBO	= 5	/* 5GHz, OFDM, 2x clock */
};
#define	IEEE80211_MODE_MAX	(IEEE80211_MODE_TURBO+1)

enum ieee80211_opmode {
	IEEE80211_M_STA		= 1,	/* infrastructure station */
	IEEE80211_M_IBSS	= 0,	/* IBSS (adhoc) station */
	IEEE80211_M_AHDEMO	= 3,	/* Old lucent compatible adhoc demo */
	IEEE80211_M_HOSTAP	= 6,	/* Software Access Point */
	IEEE80211_M_MONITOR	= 8	/* Monitor mode */
};

/*
 * 802.11g protection mode.
 */
enum ieee80211_protmode {
	IEEE80211_PROT_NONE	= 0,	/* no protection */
	IEEE80211_PROT_CTSONLY	= 1,	/* CTS to self */
	IEEE80211_PROT_RTSCTS	= 2	/* RTS-CTS */
};

/*
 * Channels are specified by frequency and attributes.
 */
struct ieee80211_channel {
	u_int16_t	ic_freq;	/* setting in MHz */
	u_int16_t	ic_flags;	/* see below */
};

#define	IEEE80211_IS_CHAN_FHSS(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define	IEEE80211_IS_CHAN_A(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define	IEEE80211_IS_CHAN_B(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define	IEEE80211_IS_CHAN_PUREG(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define	IEEE80211_IS_CHAN_G(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define	IEEE80211_IS_CHAN_T(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_T) == IEEE80211_CHAN_T)
#define	IEEE80211_IS_CHAN_TG(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_TG) == IEEE80211_CHAN_TG)

#define	IEEE80211_IS_CHAN_2GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_2GHZ) != 0)
#define	IEEE80211_IS_CHAN_5GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_5GHZ) != 0)
#define	IEEE80211_IS_CHAN_OFDM(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_OFDM) != 0)
#define	IEEE80211_IS_CHAN_CCK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_CCK) != 0)
#define	IEEE80211_IS_CHAN_GFSK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_GFSK) != 0)
#define	IEEE80211_IS_CHAN_XR(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_XR) != 0)

/* ni_chan encoding for FH phy */
#define	IEEE80211_FH_CHANMOD	80
#define	IEEE80211_FH_CHAN(set,pat)	(((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define	IEEE80211_FH_CHANSET(chan)	((chan)/IEEE80211_FH_CHANMOD+1)
#define	IEEE80211_FH_CHANPAT(chan)	((chan)%IEEE80211_FH_CHANMOD)

/*
 * 802.11e EDCA AC parameters.
 */
struct ieee80211_edca_ac_params {
	u_int8_t	ac_ecwmin;	/* CWmin = 2^ECWmin - 1 */
	u_int8_t	ac_ecwmax;	/* CWmax = 2^ECWmax - 1 */
	u_int8_t	ac_aifsn;
	u_int16_t	ac_txoplimit;	/* 32TU */
#define IEEE80211_TXOP_TO_US(txop)	((txop) * 32)

	u_int8_t	ac_acm;
};

#define	IEEE80211_PS_SLEEP	0x1	/* STA is in power saving mode */

#define	IEEE80211_PS_MAX_QUEUE	50	/* maximum saved packets */

#define	IEEE80211_SCAN_UNLOCKED	0x0
#define	IEEE80211_SCAN_LOCKED	0x1
#define	IEEE80211_SCAN_REQUEST	0x2
#define	IEEE80211_SCAN_RESUME	0x4

struct ieee80211com {
	struct arpcom		ic_ac;
	LIST_ENTRY(ieee80211com) ic_list;	/* chain of all ieee80211com */
	void			(*ic_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, u_int32_t);
	int			(*ic_send_mgmt)(struct ieee80211com *,
				    struct ieee80211_node *, int, int);
	void			(*ic_recv_eapol)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *);
	int			(*ic_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*ic_newassoc)(struct ieee80211com *,
				    struct ieee80211_node *, int);
	void			(*ic_updateslot)(struct ieee80211com *);
	void			(*ic_updateedca)(struct ieee80211com *);
	void			(*ic_set_tim)(struct ieee80211com *, int, int);
	int			(*ic_set_key)(struct ieee80211com *,
				    struct ieee80211_node *,
				    const struct ieee80211_key *);
	void			(*ic_delete_key)(struct ieee80211com *,
				    struct ieee80211_node *, int);
	u_int8_t		ic_myaddr[IEEE80211_ADDR_LEN];
	struct ieee80211_rateset ic_sup_rates[IEEE80211_MODE_MAX];
	struct ieee80211_channel ic_channels[IEEE80211_CHAN_MAX+1];
	u_char			ic_chan_avail[howmany(IEEE80211_CHAN_MAX,NBBY)];
	u_char			ic_chan_active[howmany(IEEE80211_CHAN_MAX, NBBY)];
	u_char			ic_chan_scan[howmany(IEEE80211_CHAN_MAX,NBBY)];
	struct ifqueue		ic_mgtq;
	struct ifqueue		ic_pwrsaveq;
	u_int			ic_scan_lock;	/* user-initiated scan */
	u_int8_t		ic_scan_count;	/* count scans */
	u_int32_t		ic_flags;	/* state flags */
	u_int32_t		ic_caps;	/* capabilities */
	u_int16_t		ic_modecaps;	/* set of mode capabilities */
	u_int16_t		ic_curmode;	/* current mode */
	enum ieee80211_phytype	ic_phytype;	/* XXX wrong for multi-mode */
	enum ieee80211_opmode	ic_opmode;	/* operation mode */
	enum ieee80211_state	ic_state;	/* 802.11 state */
	u_int32_t		*ic_aid_bitmap;
	u_int16_t		ic_max_aid;
	enum ieee80211_protmode	ic_protmode;	/* 802.11g protection mode */
	struct ifmedia		ic_media;	/* interface media config */
	caddr_t			ic_rawbpf;	/* packet filter structure */
	struct ieee80211_node	*ic_bss;	/* information for this node */
	struct ieee80211_channel *ic_ibss_chan;
	int			ic_fixed_rate;	/* index to ic_sup_rates[] */
	u_int16_t		ic_rtsthreshold;
	u_int16_t		ic_fragthreshold;
	u_int			ic_scangen;	/* gen# for timeout scan */
	struct ieee80211_node	*(*ic_node_alloc)(struct ieee80211com *);
	void			(*ic_node_free)(struct ieee80211com *,
					struct ieee80211_node *);
	void			(*ic_node_copy)(struct ieee80211com *,
					struct ieee80211_node *,
					const struct ieee80211_node *);
	u_int8_t		(*ic_node_getrssi)(struct ieee80211com *,
					const struct ieee80211_node *);
	u_int8_t		ic_max_rssi;
	struct ieee80211_tree	ic_tree;
	int			ic_nnodes;	/* length of ic_nnodes */
	int			ic_max_nnodes;	/* max length of ic_nnodes */
	u_int16_t		ic_lintval;	/* listen interval */
	u_int16_t		ic_holdover;	/* PM hold over duration */
	u_int16_t		ic_txmin;	/* min tx retry count */
	u_int16_t		ic_txmax;	/* max tx retry count */
	u_int16_t		ic_txlifetime;	/* tx lifetime */
	int16_t			ic_txpower;	/* tx power setting (dBm) */
	u_int16_t		ic_bmisstimeout;/* beacon miss threshold (ms) */
	u_int16_t		ic_nonerpsta;	/* # non-ERP stations */
	u_int16_t		ic_longslotsta;	/* # long slot time stations */
	int			ic_mgt_timer;	/* mgmt timeout */
	int			ic_inact_timer;	/* inactivity timer wait */
	int			ic_des_esslen;
	u_int8_t		ic_des_essid[IEEE80211_NWID_LEN];
	struct ieee80211_channel *ic_des_chan;	/* desired channel */
	u_int8_t		ic_des_bssid[IEEE80211_ADDR_LEN];
	struct ieee80211_key	ic_nw_keys[IEEE80211_WEP_NKID];
	int			ic_wep_txkey;	/* default tx key index */
	void			*ic_wep_ctx;	/* wep crypt context */
	u_int32_t		ic_iv;		/* initial vector for wep */
	struct ieee80211_stats	ic_stats;	/* statistics */
	struct timeval		ic_last_merge_print;	/* for rate-limiting
							 * IBSS merge print-outs
							 */
	struct ieee80211_edca_ac_params ic_edca_ac[EDCA_NUM_AC];
	u_int			ic_edca_updtcount;
	u_int8_t		ic_globalcnt[EAPOL_KEY_NONCE_LEN];
	u_int64_t		ic_keyreplaycnt;

	u_int8_t		*ic_tim_bitmap;
	u_int			ic_tim_len;
	u_int			ic_tim_mcast;
	u_int			ic_dtim_period;
	u_int			ic_dtim_count;
};
#define	ic_if		ic_ac.ac_if
#define	ic_softc	ic_if.if_softc

LIST_HEAD(ieee80211com_head, ieee80211com);
extern struct ieee80211com_head ieee80211com_head;

#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#define	IEEE80211_ADDR_COPY(dst,src)	memcpy(dst,src,IEEE80211_ADDR_LEN)

/* ic_flags */
#define	IEEE80211_F_ASCAN	0x00000001	/* STATUS: active scan */
#define	IEEE80211_F_SIBSS	0x00000002	/* STATUS: start IBSS */
#define	IEEE80211_F_WEPON	0x00000100	/* CONF: WEP enabled */
#define	IEEE80211_F_IBSSON	0x00000200	/* CONF: IBSS creation enable */
#define	IEEE80211_F_PMGTON	0x00000400	/* CONF: Power mgmt enable */
#define	IEEE80211_F_DESBSSID	0x00000800	/* CONF: des_bssid is set */
#define	IEEE80211_F_SCANAP	0x00001000	/* CONF: Scanning AP */
#define	IEEE80211_F_ROAMING	0x00002000	/* CONF: roaming enabled */
#define	IEEE80211_F_SWRETRY	0x00004000	/* CONF: sw tx retry enabled */
#define	IEEE80211_F_TXPMGT	0x00018000	/* STATUS: tx power */
#define IEEE80211_F_TXPOW_OFF	0x00000000	/* TX Power: radio disabled */
#define IEEE80211_F_TXPOW_FIXED	0x00008000	/* TX Power: fixed rate */
#define IEEE80211_F_TXPOW_AUTO	0x00010000	/* TX Power: undefined */
#define	IEEE80211_F_SHSLOT	0x00020000	/* STATUS: short slot time */
#define	IEEE80211_F_SHPREAMBLE	0x00040000	/* STATUS: short preamble */
#define IEEE80211_F_QOS		0x00080000	/* CONF: QoS enabled */
#define	IEEE80211_F_USEPROT	0x00100000	/* STATUS: protection enabled */
#define	IEEE80211_F_RSN		0x00200000	/* CONF: RSN enabled */
#define	IEEE80211_F_WPA1	0x00400000	/* CONF: WPA1 enabled */
#define IEEE80211_F_USERMASK	0xf0000000	/* CONF: ioctl flag mask */

/* ic_caps */
#define	IEEE80211_C_WEP		0x00000001	/* CAPABILITY: WEP available */
#define	IEEE80211_C_IBSS	0x00000002	/* CAPABILITY: IBSS available */
#define	IEEE80211_C_PMGT	0x00000004	/* CAPABILITY: Power mgmt */
#define	IEEE80211_C_HOSTAP	0x00000008	/* CAPABILITY: HOSTAP avail */
#define	IEEE80211_C_AHDEMO	0x00000010	/* CAPABILITY: Old Adhoc Demo */
#define	IEEE80211_C_SWRETRY	0x00000020	/* CAPABILITY: sw tx retry */
#define	IEEE80211_C_TXPMGT	0x00000040	/* CAPABILITY: tx power mgmt */
#define	IEEE80211_C_SHSLOT	0x00000080	/* CAPABILITY: short slottime */
#define	IEEE80211_C_SHPREAMBLE	0x00000100	/* CAPABILITY: short preamble */
#define	IEEE80211_C_MONITOR	0x00000200	/* CAPABILITY: monitor mode */
#define IEEE80211_C_SCANALL	0x00000400	/* CAPABILITY: scan all chan */
#define IEEE80211_C_QOS		0x00000800	/* CAPABILITY: QoS avail */
#define IEEE80211_C_RSN		0x00001000	/* CAPABILITY: RSN avail */

/* flags for ieee80211_fix_rate() */
#define	IEEE80211_F_DOSORT	0x00000001	/* sort rate list */
#define	IEEE80211_F_DOFRATE	0x00000002	/* use fixed rate */
#define	IEEE80211_F_DONEGO	0x00000004	/* calc negotiated rate */
#define	IEEE80211_F_DODEL	0x00000008	/* delete ignore rate */

void	ieee80211_ifattach(struct ifnet *);
void	ieee80211_ifdetach(struct ifnet *);
void	ieee80211_media_init(struct ifnet *, ifm_change_cb_t, ifm_stat_cb_t);
int	ieee80211_media_change(struct ifnet *);
void	ieee80211_media_status(struct ifnet *, struct ifmediareq *);
int	ieee80211_ioctl(struct ifnet *, u_long, caddr_t);
int	ieee80211_get_rate(struct ieee80211com *);
void	ieee80211_watchdog(struct ifnet *);
int	ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_node *, int);
int	ieee80211_rate2media(struct ieee80211com *, int,
		enum ieee80211_phymode);
int	ieee80211_media2rate(int);
u_int	ieee80211_mhz2ieee(u_int, u_int);
u_int	ieee80211_chan2ieee(struct ieee80211com *,
		const struct ieee80211_channel *);
u_int	ieee80211_ieee2mhz(u_int, u_int);
int	ieee80211_setmode(struct ieee80211com *, enum ieee80211_phymode);
enum ieee80211_phymode ieee80211_next_mode(struct ifnet *);
enum ieee80211_phymode ieee80211_chan2mode(struct ieee80211com *,
		const struct ieee80211_channel *);

#ifdef IEEE80211_DEBUG
extern	int ieee80211_debug;
#define	IEEE80211_DPRINTF(X)	do { if (ieee80211_debug) printf X; } while(0)
#define	IEEE80211_DPRINTF2(X)	do { if (ieee80211_debug>1) printf X; } while(0)
#else
#define	IEEE80211_DPRINTF(X)
#define	IEEE80211_DPRINTF2(X)
#endif

extern	int ieee80211_cache_size;

#endif /* _NET80211_IEEE80211_VAR_H_ */

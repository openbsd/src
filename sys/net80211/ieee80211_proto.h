/*	$OpenBSD: ieee80211_proto.h,v 1.12 2007/06/06 19:31:07 damien Exp $	*/
/*	$NetBSD: ieee80211_proto.h,v 1.3 2003/10/13 04:23:56 dyoung Exp $	*/

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
 * $FreeBSD: src/sys/net80211/ieee80211_proto.h,v 1.4 2003/08/19 22:17:03 sam Exp $
 */
#ifndef _NET80211_IEEE80211_PROTO_H_
#define _NET80211_IEEE80211_PROTO_H_

/*
 * 802.11 protocol implementation definitions.
 */

enum ieee80211_state {
	IEEE80211_S_INIT	= 0,	/* default state */
	IEEE80211_S_SCAN	= 1,	/* scanning */
	IEEE80211_S_AUTH	= 2,	/* try to authenticate */
	IEEE80211_S_ASSOC	= 3,	/* try to assoc */
	IEEE80211_S_RUN		= 4	/* associated */
};
#define	IEEE80211_S_MAX		(IEEE80211_S_RUN+1)

#define	IEEE80211_SEND_MGMT(_ic,_ni,_type,_arg) \
	((*(_ic)->ic_send_mgmt)(_ic, _ni, _type, _arg))

extern	const char *ieee80211_mgt_subtype_name[];
extern	const char *ieee80211_phymode_name[];

extern	void ieee80211_proto_attach(struct ifnet *);
extern	void ieee80211_proto_detach(struct ifnet *);

struct ieee80211_node;
extern	void ieee80211_input(struct ifnet *, struct mbuf *,
		struct ieee80211_node *, int, u_int32_t);
extern	int ieee80211_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		struct rtentry *);
extern	void ieee80211_recv_mgmt(struct ieee80211com *, struct mbuf *,
		struct ieee80211_node *, int, int, u_int32_t);
extern	int ieee80211_send_mgmt(struct ieee80211com *, struct ieee80211_node *,
		int, int);
extern	struct mbuf *ieee80211_encap(struct ifnet *, struct mbuf *,
		struct ieee80211_node **);
extern	struct mbuf *ieee80211_get_rts(struct ieee80211com *,
		const struct ieee80211_frame *, u_int16_t);
extern	struct mbuf *ieee80211_get_cts_to_self(struct ieee80211com *,
		u_int16_t);
extern	struct mbuf *ieee80211_beacon_alloc(struct ieee80211com *,
		struct ieee80211_node *);
extern	void ieee80211_pwrsave(struct ieee80211com *, struct ieee80211_node *,
		struct mbuf *);
extern	struct mbuf *ieee80211_decap(struct ifnet *, struct mbuf *);
extern	u_int8_t *ieee80211_add_rates(u_int8_t *frm,
		const struct ieee80211_rateset *);
#define	ieee80211_new_state(_ic, _nstate, _arg) \
	(((_ic)->ic_newstate)((_ic), (_nstate), (_arg)))
extern	u_int8_t *ieee80211_add_xrates(u_int8_t *frm,
		const struct ieee80211_rateset *);
extern	u_int8_t *ieee80211_add_ssid(u_int8_t *, const u_int8_t *, u_int);
extern	u_int8_t *ieee80211_add_erp(u_int8_t *, struct ieee80211com *);
extern	void ieee80211_print_essid(u_int8_t *, int);
extern	void ieee80211_dump_pkt(u_int8_t *, int, int, int);
extern	int ieee80211_ibss_merge(struct ieee80211com *,
		struct ieee80211_node *, u_int64_t);
extern	int ieee80211_compute_duration(struct ieee80211_frame *, int,
		uint32_t, int, int, struct ieee80211_duration *,
		struct ieee80211_duration *, int *, int);
extern	void ieee80211_reset_erp(struct ieee80211com *);
extern	void ieee80211_set_shortslottime(struct ieee80211com *, int);

extern	const char *ieee80211_state_name[IEEE80211_S_MAX];
#endif /* _NET80211_IEEE80211_PROTO_H_ */

/*-
 * Copyright (c) 2007,2008 Damien Bergamini <damien.bergamini@free.fr>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>

#include <dev/rndvar.h>

void	ieee80211_recv_4way_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg2(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *,
	    const u_int8_t *);
void	ieee80211_recv_4way_msg3(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg4(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_4way_msg2or4(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_rsn_group_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_wpa_group_msg1(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_group_msg2(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);
void	ieee80211_recv_eapol_key_req(struct ieee80211com *,
	    struct ieee80211_eapol_key *, struct ieee80211_node *);

/* unaligned big endian access */
#define BE_READ_2(p)						\
	((u_int16_t)(p)[0] << 8 | (u_int16_t)(p)[1])

#define BE_READ_8(p)						\
	((u_int64_t)(p)[0] << 56 | (u_int64_t)(p)[1] << 48 |	\
	 (u_int64_t)(p)[2] << 40 | (u_int64_t)(p)[3] << 32 |	\
	 (u_int64_t)(p)[4] << 24 | (u_int64_t)(p)[5] << 16 |	\
	 (u_int64_t)(p)[6] <<  8 | (u_int64_t)(p)[7])

/* unaligned little endian access */
#define LE_READ_6(p)						\
	((u_int64_t)(p)[5] << 40 | (u_int64_t)(p)[4] << 32 |	\
	 (u_int64_t)(p)[3] << 24 | (u_int64_t)(p)[2] << 16 |	\
	 (u_int64_t)(p)[1] <<  8 | (u_int64_t)(p)[0])

/*
 * Process an incoming EAPOL frame.  Notice that we are only interested in
 * EAPOL-Key frames with an IEEE 802.11 or WPA descriptor type.
 */
void
ieee80211_recv_eapol(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ether_header *eh;
	struct ieee80211_eapol_key *key;
	u_int16_t info, desc;

	ifp->if_ibytes += m0->m_pkthdr.len;

	if (m0->m_len < sizeof(*eh) + sizeof(*key))
		return;
	eh = mtod(m0, struct ether_header *);
	if (IEEE80211_IS_MULTICAST(eh->ether_dhost)) {
		ifp->if_imcasts++;
		return;
	}
	m_adj(m0, sizeof(*eh));
	key = mtod(m0, struct ieee80211_eapol_key *);

	if (key->type != EAPOL_KEY)
		return;
	ic->ic_stats.is_rx_eapol_key++;

	if ((ni->ni_rsnprotos == IEEE80211_PROTO_RSN &&
	     key->desc != EAPOL_KEY_DESC_IEEE80211) ||
	    (ni->ni_rsnprotos == IEEE80211_PROTO_WPA &&
	     key->desc != EAPOL_KEY_DESC_WPA))
		return;

	/* check packet body length */
	if (m0->m_len < 4 + BE_READ_2(key->len))
		return;

	/* check key data length */
	if (m0->m_len < sizeof(*key) + BE_READ_2(key->paylen))
		return;

	info = BE_READ_2(key->info);

	/* discard EAPOL-Key frames with an unknown descriptor version */
	desc = info & EAPOL_KEY_VERSION_MASK;
	if (desc != EAPOL_KEY_DESC_V1 && desc != EAPOL_KEY_DESC_V2)
		return;

	if ((ni->ni_rsncipher == IEEE80211_CIPHER_CCMP ||
	     ni->ni_rsngroupcipher == IEEE80211_CIPHER_CCMP) &&
	    desc != EAPOL_KEY_DESC_V2)
		return;

	/* determine message type (see 8.5.3.7) */
	if (info & EAPOL_KEY_REQUEST) {
		/* EAPOL-Key Request frame */
		ieee80211_recv_eapol_key_req(ic, key, ni);

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* 4-Way Handshake */
		if (info & EAPOL_KEY_KEYMIC) {
			if (info & EAPOL_KEY_KEYACK)
				ieee80211_recv_4way_msg3(ic, key, ni);
			else
				ieee80211_recv_4way_msg2or4(ic, key, ni);
		} else if (info & EAPOL_KEY_KEYACK)
			ieee80211_recv_4way_msg1(ic, key, ni);
	} else {
		/* Group Key Handshake */
		if (!(info & EAPOL_KEY_KEYMIC))
			return;
		if (info & EAPOL_KEY_KEYACK) {
			if (key->desc == EAPOL_KEY_DESC_WPA)
				ieee80211_recv_wpa_group_msg1(ic, key, ni);
			else
				ieee80211_recv_rsn_group_msg1(ic, key, ni);
		} else
			ieee80211_recv_group_msg2(ic, key, ni);
	}
}

/*
 * 4-Way Handshake Message 1 is sent by the authenticator to the supplicant
 * (see 8.5.3.1).
 */
void
ieee80211_recv_4way_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_ptk tptk;
	const u_int8_t *frm, *efrm;
	const u_int8_t *pmkid;
	const u_int8_t *pmk;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (ni->ni_replaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* save authenticator's nonce (ANonce) */
	memcpy(ni->ni_nonce, key->nonce, EAPOL_KEY_NONCE_LEN);

	/* parse key data field (may contain an encapsulated PMKID) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	pmkid = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_PMKID:
					pmkid = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* check that the PMKID KDE is valid (if present) */
	if (pmkid != NULL && pmkid[1] < 4 + 16)
		return;

	/* generate a new supplicant's nonce (SNonce) */
	arc4random_buf(ic->ic_nonce, EAPOL_KEY_NONCE_LEN);

	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, pmkid)) == NULL) {
		/* no PMK configured for this STA/PMKID */
		return;
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ni->ni_macaddr,
	    ic->ic_myaddr, key->nonce, ic->ic_nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator using TPTK */
	(void)ieee80211_send_4way_msg2(ic, ni, key->replaycnt, &tptk);
}

/*
 * 4-Way Handshake Message 2 is sent by the supplicant to the authenticator
 * (see 8.5.3.2).
 */
void
ieee80211_recv_4way_msg2(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni,
    const u_int8_t *rsnie)
{
	struct ieee80211_ptk tptk;
	const u_int8_t *pmk;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_state != RSNA_PTKSTART &&
	    ni->ni_rsn_state != RSNA_PTKCALCNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}
	ni->ni_rsn_state = RSNA_PTKCALCNEGOTIATING;

	/* replay counter has already been verified by caller */

	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, NULL)) == NULL) {
		/* no PMK configured for this STA */
		return;	/* will timeout.. */
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ic->ic_myaddr,
	    ni->ni_macaddr, ni->ni_nonce, key->nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, tptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;	/* will timeout.. */
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_state = RSNA_PTKCALCNEGOTIATING_2;
	ni->ni_rsn_retries = 0;

	/* install TPTK as PTK now that MIC is verified */
	memcpy(&ni->ni_ptk, &tptk, sizeof(tptk));

	/*
	 * The RSN IE must match bit-wise with what the STA included in its
	 * (Re)Association Request.
	 */
	if (ni->ni_rsnie == NULL || rsnie[1] != ni->ni_rsnie[1] ||
	    memcmp(rsnie, ni->ni_rsnie, 2 + rsnie[1]) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_RSN_DIFFERENT_IE);
		ieee80211_node_leave(ic, ni);
		return;
	}

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 3 to supplicant */
	(void)ieee80211_send_4way_msg3(ic, ni);
}

/*
 * 4-Way Handshake Message 3 is sent by the authenticator to the supplicant
 * (see 8.5.3.3).
 */
void
ieee80211_recv_4way_msg3(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_ptk tptk;
	struct ieee80211_key *k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsnie1, *rsnie2, *gtk;
	const u_int8_t *pmk;
	u_int16_t info, reason = 0;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (ni->ni_replaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}

	/* check that ANonce matches that of message 1 */
	if (memcmp(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN) != 0) {
		IEEE80211_DPRINTF(("%s: ANonce does not match msg 1/4\n",
		    __func__));
		return;
	}
	/* retrieve PMK and derive TPTK */
	if ((pmk = ieee80211_get_pmk(ic, ni, NULL)) == NULL) {
		/* no PMK configured for this STA */
		return;
	}
	ieee80211_derive_ptk(pmk, IEEE80211_PMK_LEN, ni->ni_macaddr,
	    ic->ic_myaddr, key->nonce, ic->ic_nonce, (u_int8_t *)&tptk,
	    sizeof(tptk));

	info = BE_READ_2(key->info);

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, tptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/* install TPTK as PTK now that MIC is verified */
	memcpy(&ni->ni_ptk, &tptk, sizeof(tptk));

	/* if encrypted, decrypt Key Data field using KEK */
	if ((info & EAPOL_KEY_ENCRYPTED) &&
	    ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}

	/* parse key data field */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	/*
	 * Some WPA1+WPA2 APs (like hostapd) appear to include both WPA and
	 * RSN IEs in message 3/4.  We only take into account the IE of the
	 * version of the protocol we negotiated at association time.
	 */
	rsnie1 = rsnie2 = gtk = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			if (ni->ni_rsnprotos != IEEE80211_PROTO_RSN)
				break;
			if (rsnie1 == NULL)
				rsnie1 = frm;
			else if (rsnie2 == NULL)
				rsnie2 = frm;
			/* ignore others if more than two RSN IEs */
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_GTK:
					gtk = frm;
					break;
				}
			} else if (memcmp(&frm[2], MICROSOFT_OUI, 3) == 0) {
				switch (frm[5]) {
				case 1:	/* WPA */
					if (ni->ni_rsnprotos !=
					    IEEE80211_PROTO_WPA)
						break;
					rsnie1 = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* first WPA/RSN IE is mandatory */
	if (rsnie1 == NULL) {
		IEEE80211_DPRINTF(("%s: missing RSN IE\n", __func__));
		return;
	}
	/* key data must be encrypted if GTK is included */
	if (gtk != NULL && !(info & EAPOL_KEY_ENCRYPTED)) {
		IEEE80211_DPRINTF(("%s: GTK not encrypted\n", __func__));
		return;
	}
	/*
	 * Check that first WPA/RSN IE is identical to the one received in
	 * the beacon or probe response frame.
	 */
	if (ni->ni_rsnie == NULL || rsnie1[1] != ni->ni_rsnie[1] ||
	    memcmp(rsnie1, ni->ni_rsnie, 2 + rsnie1[1]) != 0) {
		reason = IEEE80211_REASON_RSN_DIFFERENT_IE;
		goto deauth;
	}

	/*
	 * If a second RSN information element is present, use its pairwise
	 * cipher suite or deauthenticate.
	 */
	if (rsnie2 != NULL) {
		struct ieee80211_rsnparams rsn;

		if (ieee80211_parse_rsn(ic, rsnie2, &rsn) == 0) {
			if (rsn.rsn_akms != ni->ni_rsnakms ||
			    rsn.rsn_groupcipher != ni->ni_rsngroupcipher ||
			    rsn.rsn_nciphers != 1 ||
			    !(rsn.rsn_ciphers & ic->ic_rsnciphers)) {
				reason = IEEE80211_REASON_BAD_PAIRWISE_CIPHER;
				goto deauth;
			}
			/* use pairwise cipher suite of second RSN IE */
			ni->ni_rsnciphers = rsn.rsn_ciphers;
			ni->ni_rsncipher = ni->ni_rsnciphers;
		}
	}

	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);
	ni->ni_replaycnt_ok = 1;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 3, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 4 to authenticator */
	if (ieee80211_send_4way_msg4(ic, ni) != 0)
		return;	/* ..authenticator will retry */

	if (info & EAPOL_KEY_INSTALL) {
		u_int64_t prsc;

		/* check that key length matches that of pairwise cipher */
		if (BE_READ_2(key->keylen) !=
		    ieee80211_cipher_keylen(ni->ni_rsncipher)) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* install the PTK */
		prsc = (gtk == NULL) ? LE_READ_6(key->rsc) : 0;
		k = &ni->ni_pairwise_key;
		ieee80211_map_ptk(&ni->ni_ptk, ni->ni_rsncipher, prsc, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
	}
	if (gtk != NULL) {
		u_int64_t rsc;
		u_int8_t kid;

		/* check that the GTK KDE is valid */
		if (gtk[1] < 4 + 2) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* check that key length matches that of group cipher */
		if (gtk[1] - 6 !=
		    ieee80211_cipher_keylen(ni->ni_rsngroupcipher)) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
		/* install the GTK */
		kid = gtk[6] & 3;
		rsc = LE_READ_6(key->rsc);
		k = &ic->ic_nw_keys[kid];
		ieee80211_map_gtk(&gtk[8], ni->ni_rsngroupcipher, kid,
		    gtk[6] & (1 << 2), rsc, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			reason = IEEE80211_REASON_AUTH_LEAVE;
			goto deauth;
		}
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
 deauth:
	if (reason != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    reason);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
}

/*
 * 4-Way Handshake Message 4 is sent by the supplicant to the authenticator
 * (see 8.5.3.4).
 */
void
ieee80211_recv_4way_msg4(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_state != RSNA_PTKINITNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}

	/* replay counter has already been verified by caller */

	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;	/* will timeout.. */
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_state = RSNA_PTKINITDONE;
	ni->ni_rsn_retries = 0;

	if (ni->ni_rsncipher != IEEE80211_CIPHER_USEGROUP) {
		/* install the PTK */
		struct ieee80211_key *k = &ni->ni_pairwise_key;
		ieee80211_map_ptk(&ni->ni_ptk, ni->ni_rsncipher, 0, k);
		if ((*ic->ic_set_key)(ic, ni, k) != 0) {
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ic, ni);
			return;
		}
	}
	if (ic->ic_opmode != IEEE80211_M_IBSS || ++ni->ni_key_count == 2) {
		IEEE80211_DPRINTF(("%s: marking port %s valid\n", __func__,
		    ether_sprintf(ni->ni_macaddr)));
		ni->ni_port_valid = 1;
	}

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 4, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	/* initiate a group key handshake for WPA */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA)
		(void)ieee80211_send_group_msg1(ic, ni);
	else
		ni->ni_rsn_gstate = RSNA_IDLE;
}

/*
 * Differentiate Message 2 from Message 4 of the 4-Way Handshake based on
 * the presence of an RSN or WPA Information Element.
 */
void
ieee80211_recv_4way_msg2or4(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	const u_int8_t *frm, *efrm;
	const u_int8_t *rsnie;

	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}

	/* parse key data field (check if an RSN IE is present) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	rsnie = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_RSN:
			rsnie = frm;
			break;
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], MICROSOFT_OUI, 3) == 0) {
				switch (frm[5]) {
				case 1:	/* WPA */
					rsnie = frm;
					break;
				}
			}
		}
		frm += 2 + frm[1];
	}
	if (rsnie != NULL)
		ieee80211_recv_4way_msg2(ic, key, ni, rsnie);
	else
		ieee80211_recv_4way_msg4(ic, key, ni);
}

/*
 * Group Key Handshake Message 1 is sent by the authenticator to the
 * supplicant (see 8.5.4.1).
 */
void
ieee80211_recv_rsn_group_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key *k;
	const u_int8_t *frm, *efrm;
	const u_int8_t *gtk;
	u_int64_t rsc;
	u_int16_t info;
	u_int8_t kid;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	info = BE_READ_2(key->info);

	/* check that encrypted and decrypt Key Data field using KEK */
	if (!(info & EAPOL_KEY_ENCRYPTED) ||
	    ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}

	/* parse key data field (shall contain a GTK KDE) */
	frm = (const u_int8_t *)&key[1];
	efrm = frm + BE_READ_2(key->paylen);

	gtk = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm)
			break;
		switch (frm[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (frm[1] < 4)
				break;
			if (memcmp(&frm[2], IEEE80211_OUI, 3) == 0) {
				switch (frm[5]) {
				case IEEE80211_KDE_GTK:
					gtk = frm;
					break;
				}
			}
			break;
		}
		frm += 2 + frm[1];
	}
	/* check that the GTK KDE is present and valid */
	if (gtk == NULL || gtk[1] < 4 + 2) {
		IEEE80211_DPRINTF(("%s: missing or invalid GTK KDE\n",
		    __func__));
		return;
	}

	/* check that key length matches that of group cipher */
	if (gtk[1] - 6 != ieee80211_cipher_keylen(ni->ni_rsngroupcipher))
		return;

	/* install the GTK */
	kid = gtk[6] & 3;
	rsc = LE_READ_6(key->rsc);
	k = &ic->ic_nw_keys[kid];
	ieee80211_map_gtk(&gtk[8], ni->ni_rsngroupcipher, kid,
	    gtk[6] & (1 << 2), rsc, k);
	if ((*ic->ic_set_key)(ic, ni, k) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_AUTH_LEAVE);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return;
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	(void)ieee80211_send_group_msg2(ic, ni, k);
}

void
ieee80211_recv_wpa_group_msg1(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	struct ieee80211_key *k;
	const u_int8_t *frm;
	u_int64_t rsc;
	u_int16_t info;
	u_int8_t kid;
	int keylen;

	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	if (BE_READ_8(key->replaycnt) <= ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/*
	 * EAPOL-Key data field is encrypted even though WPA doesn't set
	 * the ENCRYPTED bit in the info field.
	 */
	if (ieee80211_eapol_key_decrypt(key, ni->ni_ptk.kek) != 0) {
		IEEE80211_DPRINTF(("%s: decryption failed\n", __func__));
		return;
	}
	info = BE_READ_2(key->info);
	keylen = ieee80211_cipher_keylen(ni->ni_rsngroupcipher);

	/* check that key length matches that of group cipher */
	if (BE_READ_2(key->keylen) != keylen)
		return;

	/* check that the data length is large enough to hold the key */
	if (BE_READ_2(key->paylen) < keylen)
		return;

	/* key data field contains the GTK */
	frm = (const u_int8_t *)&key[1];

	/* install the GTK */
	kid = (info >> EAPOL_KEY_WPA_KID_SHIFT) & 3;
	rsc = LE_READ_6(key->rsc);
	k = &ic->ic_nw_keys[kid];
	ieee80211_map_gtk(frm, ni->ni_rsngroupcipher, kid,
	    info & EAPOL_KEY_WPA_TX, rsc, k);
	if ((*ic->ic_set_key)(ic, ni, k) != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_AUTH_LEAVE);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return;
	}
	if (info & EAPOL_KEY_SECURE) {
		if (ic->ic_opmode != IEEE80211_M_IBSS ||
		    ++ni->ni_key_count == 2) {
			IEEE80211_DPRINTF(("%s: marking port %s valid\n",
			    __func__, ether_sprintf(ni->ni_macaddr)));
			ni->ni_port_valid = 1;
		}
	}
	/* update the last seen value of the key replay counter field */
	ni->ni_replaycnt = BE_READ_8(key->replaycnt);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	/* send message 2 to authenticator */
	(void)ieee80211_send_group_msg2(ic, ni, k);
}

/*
 * Group Key Handshake Message 2 is sent by the supplicant to the
 * authenticator (see 8.5.4.2).
 */
void
ieee80211_recv_group_msg2(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* discard if we're not expecting this message */
	if (ni->ni_rsn_gstate != RSNA_REKEYNEGOTIATING) {
		IEEE80211_DPRINTF(("%s: unexpected in state: %d\n",
		    __func__, ni->ni_rsn_state));
		return;
	}
	if (BE_READ_8(key->replaycnt) != ni->ni_replaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	/* check Key MIC field using KCK */
	if (ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}

	timeout_del(&ni->ni_rsn_timeout);
	ni->ni_rsn_gstate = RSNA_REKEYESTABLISHED;

	if ((ni->ni_flags & IEEE80211_NODE_REKEY) &&
	    --ic->ic_rsn_keydonesta == 0)
		ieee80211_setkeysdone(ic);
	ni->ni_flags &= ~IEEE80211_NODE_REKEY;

	ni->ni_rsn_gstate = RSNA_IDLE;
	ni->ni_rsn_retries = 0;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: received msg %d/%d of the %s handshake from %s\n",
		    ic->ic_if.if_xname, 2, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiates either a 4-Way Handshake or Group Key Handshake,
 * or to report a MIC failure in a TKIP MSDU.
 */
void
ieee80211_recv_eapol_key_req(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, struct ieee80211_node *ni)
{
	u_int16_t info;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	info = BE_READ_2(key->info);

	/* enforce monotonicity of key request replay counter */
	if (ni->ni_reqreplaycnt_ok &&
	    BE_READ_8(key->replaycnt) <= ni->ni_reqreplaycnt) {
		ic->ic_stats.is_rx_eapol_replay++;
		return;
	}
	if (!(info & EAPOL_KEY_KEYMIC) ||
	    ieee80211_eapol_key_check_mic(key, ni->ni_ptk.kck) != 0) {
		IEEE80211_DPRINTF(("%s: key MIC failed\n", __func__));
		ic->ic_stats.is_rx_eapol_badmic++;
		return;
	}
	/* update key request replay counter now that MIC is verified */
	ni->ni_reqreplaycnt = BE_READ_8(key->replaycnt);
	ni->ni_reqreplaycnt_ok = 1;

	if (info & EAPOL_KEY_ERROR) {	/* TKIP MIC failure */
		/* ignore reports from STAs not using TKIP */
		if (ic->ic_bss->ni_rsngroupcipher != IEEE80211_CIPHER_TKIP &&
		    ni->ni_rsncipher != IEEE80211_CIPHER_TKIP) {
			IEEE80211_DPRINTF(("%s: MIC failure report from "
			    "STA not using TKIP: %s\n", __func__,
			    ether_sprintf(ni->ni_macaddr)));
			return;
		}
		ic->ic_stats.is_rx_remmicfail++;
		ieee80211_michael_mic_failure(ic, LE_READ_6(key->rsc));

	} else if (info & EAPOL_KEY_PAIRWISE) {
		/* initiate a 4-Way Handshake */

	} else {
		/*
		 * Should change the GTK, initiate the 4-Way Handshake and
		 * then execute a Group Key Handshake with all supplicants.
		 */
	}
}

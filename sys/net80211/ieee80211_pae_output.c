/*	$OpenBSD: ieee80211_pae_output.c,v 1.1 2008/07/21 19:05:21 damien Exp $	*/

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
#include <net/if_llc.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>

int		ieee80211_send_eapol_key(struct ieee80211com *, struct mbuf *,
		    struct ieee80211_node *, const struct ieee80211_ptk *);
u_int8_t 	*ieee80211_add_gtk_kde(u_int8_t *, struct ieee80211_node *,
		    const struct ieee80211_key *);
u_int8_t	*ieee80211_add_pmkid_kde(u_int8_t *, const u_int8_t *);
struct mbuf 	*ieee80211_get_eapol_key(int, int, u_int);

/* unaligned big endian access */
#define BE_READ_2(p)				\
	((u_int16_t)(p)[0] << 8 | (u_int16_t)(p)[1])

#define BE_WRITE_2(p, v) do {			\
	(p)[0] = (v) >>  8; (p)[1] = (v);	\
} while (0)

#define BE_WRITE_8(p, v) do {			\
	(p)[0] = (v) >> 56; (p)[1] = (v) >> 48;	\
	(p)[2] = (v) >> 40; (p)[3] = (v) >> 32;	\
	(p)[4] = (v) >> 24; (p)[5] = (v) >> 16;	\
	(p)[6] = (v) >>  8; (p)[7] = (v);	\
} while (0)

/* unaligned little endian access */
#define LE_WRITE_6(p, v) do {			\
	(p)[5] = (v) >> 40; (p)[4] = (v) >> 32;	\
	(p)[3] = (v) >> 24; (p)[2] = (v) >> 16;	\
	(p)[1] = (v) >>  8; (p)[0] = (v);	\
} while (0)

/*
 * Send an EAPOL-Key frame to node `ni'.  If MIC or encryption is required,
 * the PTK must be passed (otherwise it can be set to NULL.)
 */
int
ieee80211_send_eapol_key(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, const struct ieee80211_ptk *ptk)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ether_header *eh;
	struct ieee80211_eapol_key *key;
	u_int16_t len, info;
	int s, error;

	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	/* no need to m_pullup here (ok by construction) */
	eh = mtod(m, struct ether_header *);
	eh->ether_type = htons(ETHERTYPE_PAE);
	IEEE80211_ADDR_COPY(eh->ether_shost, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(eh->ether_dhost, ni->ni_macaddr);

	key = (struct ieee80211_eapol_key *)&eh[1];
	key->version = EAPOL_VERSION;
	key->type = EAPOL_KEY;
	key->desc = (ni->ni_rsnprotos == IEEE80211_PROTO_RSN) ?
	    EAPOL_KEY_DESC_IEEE80211 : EAPOL_KEY_DESC_WPA;

	info = BE_READ_2(key->info);
	/* use V2 descriptor if pairwise or group cipher is CCMP */
	if (ni->ni_rsncipher == IEEE80211_CIPHER_CCMP ||
	    ni->ni_rsngroupcipher == IEEE80211_CIPHER_CCMP)
		info |= EAPOL_KEY_DESC_V2;
	else
		info |= EAPOL_KEY_DESC_V1;
	BE_WRITE_2(key->info, info);

	len = m->m_len - sizeof(struct ether_header);
	BE_WRITE_2(key->paylen, len - sizeof(*key));
	BE_WRITE_2(key->len, len - 4);

	if (info & EAPOL_KEY_ENCRYPTED) {
		if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
			/* clear "Encrypted" bit for WPA */
			info &= ~EAPOL_KEY_ENCRYPTED;
			BE_WRITE_2(key->info, info);
		}
		ieee80211_eapol_key_encrypt(ic, key, ptk->kek);

		if ((info & EAPOL_KEY_VERSION_MASK) == EAPOL_KEY_DESC_V2) {
			/* AES Key Wrap adds 8 bytes + padding */
			m->m_pkthdr.len = m->m_len =
			    sizeof(*eh) + 4 + BE_READ_2(key->len);
		}
	}
	if (info & EAPOL_KEY_KEYMIC)
		ieee80211_eapol_key_mic(key, ptk->kck);

	s = splnet();
	/* start a 100ms timeout if an answer is expected from supplicant */
	if (info & EAPOL_KEY_KEYACK)
		timeout_add(&ni->ni_rsn_timeout, hz / 10);
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	if (error == 0) {
		ifp->if_obytes += m->m_pkthdr.len;
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			(*ifp->if_start)(ifp);
	}
	splx(s);

	return error;
}

/*
 * Handle EAPOL-Key timeouts (no answer from supplicant).
 */
void
ieee80211_eapol_timeout(void *arg)
{
	struct ieee80211_node *ni = arg;
	struct ieee80211com *ic = ni->ni_ic;
	int s;

	IEEE80211_DPRINTF(("%s: no answer from station %s in state %d\n",
	    __func__, ether_sprintf(ni->ni_macaddr), ni->ni_rsn_state));

	s = splnet();

	switch (ni->ni_rsn_state) {
	case RSNA_PTKSTART:
	case RSNA_PTKCALCNEGOTIATING:
		(void)ieee80211_send_4way_msg1(ic, ni);
		break;
	case RSNA_PTKINITNEGOTIATING:
		(void)ieee80211_send_4way_msg3(ic, ni);
		break;
	}

	switch (ni->ni_rsn_gstate) {
	case RSNA_REKEYNEGOTIATING:
		(void)ieee80211_send_group_msg1(ic, ni);
		break;
	}

	splx(s);
}

/*
 * Add a GTK KDE to an EAPOL-Key frame (see Figure 144).
 */
u_int8_t *
ieee80211_add_gtk_kde(u_int8_t *frm, struct ieee80211_node *ni,
    const struct ieee80211_key *k)
{
	KASSERT(k->k_flags & IEEE80211_KEY_GROUP);

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 6 + k->k_len;
	memcpy(frm, IEEE80211_OUI, 3); frm += 3;
	*frm++ = IEEE80211_KDE_GTK;
	*frm = k->k_id & 3;
	/*
	 * The TxRx flag for sending a GTK is always the opposite of whether
	 * the pairwise key is used for data encryption/integrity or not.
	 */
	if (ni->ni_rsncipher == IEEE80211_CIPHER_USEGROUP)
		*frm |= 1 << 2;	/* set the Tx bit */
	frm++;
	*frm++ = 0;	/* reserved */
	memcpy(frm, k->k_key, k->k_len);
	return frm + k->k_len;
}

/*
 * Add a PMKID KDE to an EAPOL-Key frame (see Figure 146).
 */
u_int8_t *
ieee80211_add_pmkid_kde(u_int8_t *frm, const u_int8_t *pmkid)
{
	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 20;
	memcpy(frm, IEEE80211_OUI, 3); frm += 3;
	*frm++ = IEEE80211_KDE_PMKID;
	memcpy(frm, pmkid, IEEE80211_PMKID_LEN);
	return frm + IEEE80211_PMKID_LEN;
}

struct mbuf *
ieee80211_get_eapol_key(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	/* reserve space for 802.11 encapsulation and EAPOL-Key header */
	pktlen += sizeof(struct ieee80211_frame) + sizeof(struct llc) +
	    sizeof(struct ieee80211_eapol_key);

	if (pktlen > MCLBYTES)
		panic("EAPOL-Key frame too large: %u", pktlen);
	MGETHDR(m, flags, type);
	if (m == NULL)
		return NULL;
	if (pktlen >= MINCLSIZE) {
		MCLGET(m, flags);
		if (!(m->m_flags & M_EXT))
			return m_free(m);
	}
	m->m_data += sizeof(struct ieee80211_frame) + sizeof(struct llc);
	return m;
}

/*
 * 4-Way Handshake Message 1 is sent by the authenticator to the supplicant
 * (see 8.5.3.1).
 */
int
ieee80211_send_4way_msg1(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info, keylen;
	u_int8_t *frm;

	ni->ni_rsn_state = RSNA_PTKSTART;
	if (++ni->ni_rsn_retries > 3) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_4WAY_TIMEOUT);
		ieee80211_node_leave(ic, ni);
		return 0;
	}
	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    (ni->ni_rsnprotos == IEEE80211_PROTO_RSN) ? 2 + 20 : 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYACK;
	BE_WRITE_2(key->info, info);

	/* copy the authenticator's nonce (ANonce) */
	memcpy(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN);

	keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
	BE_WRITE_2(key->keylen, keylen);

	frm = (u_int8_t *)&key[1];
	/* WPA does not have PMKID KDE */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_RSN &&
	    ni->ni_rsnakms == IEEE80211_AKM_IEEE8021X) {
		/* XXX retrieve PMKID from the PMKSA cache */
		/* frm = ieee80211_add_pmkid_kde(frm, pmkid); */
	}

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 1, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	ni->ni_replaycnt++;
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	return ieee80211_send_eapol_key(ic, m, ni, NULL);
}

/*
 * 4-Way Handshake Message 2 is sent by the supplicant to the authenticator
 * (see 8.5.3.2).
 */
int
ieee80211_send_4way_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *replaycnt, const struct ieee80211_ptk *tptk)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info;
	u_int8_t *frm;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    2 + 48);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;
	BE_WRITE_2(key->info, info);

	/* copy key replay counter from Message 1/4 */
	memcpy(key->replaycnt, replaycnt, 8);

	/* copy the supplicant's nonce (SNonce) */
	memcpy(key->nonce, ic->ic_nonce, EAPOL_KEY_NONCE_LEN);

	frm = (u_int8_t *)&key[1];
	/* add the WPA/RSN IE used in the (Re)Association Request */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		u_int16_t keylen;
		frm = ieee80211_add_wpa(frm, ic, ni);
		/* WPA sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
		BE_WRITE_2(key->keylen, keylen);
	} else	/* RSN */
		frm = ieee80211_add_rsn(frm, ic, ni);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 2, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, tptk);
}

/*
 * 4-Way Handshake Message 3 is sent by the authenticator to the supplicant
 * (see 8.5.3.3).
 */
int
ieee80211_send_4way_msg3(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct ieee80211_key *k;
	struct mbuf *m;
	u_int16_t info, keylen;
	u_int8_t *frm;

	ni->ni_rsn_state = RSNA_PTKINITNEGOTIATING;
	if (++ni->ni_rsn_retries > 3) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_4WAY_TIMEOUT);
		ieee80211_node_leave(ic, ni);
		return 0;
	}
	if (ni->ni_rsnprotos == IEEE80211_PROTO_RSN)
		k = &ic->ic_nw_keys[ic->ic_def_txkey];

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    2 + 48 +
	    ((ni->ni_rsnprotos == IEEE80211_PROTO_RSN) ?
		2 + 6 + k->k_len : 0) +
	    8);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYACK | EAPOL_KEY_KEYMIC;
	if (ni->ni_rsncipher != IEEE80211_CIPHER_USEGROUP)
		info |= EAPOL_KEY_INSTALL;

	/* use same nonce as in Message 1 */
	memcpy(key->nonce, ni->ni_nonce, EAPOL_KEY_NONCE_LEN);

	ni->ni_replaycnt++;
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
	BE_WRITE_2(key->keylen, keylen);

	frm = (u_int8_t *)&key[1];
	/* add the WPA/RSN IE included in Beacon/Probe Response */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_RSN) {
		frm = ieee80211_add_rsn(frm, ic, ic->ic_bss);
		/* encapsulate the GTK and ask for encryption */
		frm = ieee80211_add_gtk_kde(frm, ni, k);
		LE_WRITE_6(key->rsc, k->k_tsc);
		info |= EAPOL_KEY_ENCRYPTED | EAPOL_KEY_SECURE;
	} else	/* WPA */
		frm = ieee80211_add_wpa(frm, ic, ic->ic_bss);

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 3, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * 4-Way Handshake Message 4 is sent by the supplicant to the authenticator
 * (see 8.5.3.4).
 */
int
ieee80211_send_4way_msg4(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;
	u_int16_t info;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;

	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		u_int16_t keylen;
		/* WPA sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
		BE_WRITE_2(key->keylen, keylen);
	} else
		info |= EAPOL_KEY_SECURE;

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	/* empty key data field */
	m->m_pkthdr.len = m->m_len = sizeof(*key);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 4, 4, "4-way",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * Group Key Handshake Message 1 is sent by the authenticator to the
 * supplicant (see 8.5.4.1).
 */
int
ieee80211_send_group_msg1(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	const struct ieee80211_key *k;
	struct mbuf *m;
	u_int16_t info;
	u_int8_t *frm;

	ni->ni_rsn_gstate = RSNA_REKEYNEGOTIATING;
	if (++ni->ni_rsn_retries > 3) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_GROUP_TIMEOUT);
		ieee80211_node_leave(ic, ni);
		return 0;
	}
	k = &ic->ic_nw_keys[ic->ic_def_txkey];

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA,
	    ((ni->ni_rsnprotos == IEEE80211_PROTO_WPA) ?
		k->k_len : 2 + 6 + k->k_len) +
	    8);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_KEYACK | EAPOL_KEY_KEYMIC | EAPOL_KEY_SECURE |
	    EAPOL_KEY_ENCRYPTED;

	ni->ni_replaycnt++;
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	frm = (u_int8_t *)&key[1];
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		/* WPA does not have GTK KDE */
		BE_WRITE_2(key->keylen, k->k_len);
		memcpy(frm, k->k_key, k->k_len);
		frm += k->k_len;
		info |= (k->k_id & 0x3) << EAPOL_KEY_WPA_KID_SHIFT;
		if (ni->ni_rsncipher == IEEE80211_CIPHER_USEGROUP)
			info |= EAPOL_KEY_WPA_TX;
	} else	/* RSN */
		frm = ieee80211_add_gtk_kde(frm, ni, k);

	/* RSC = last transmit sequence number for the GTK */
	LE_WRITE_6(key->rsc, k->k_tsc);

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	m->m_pkthdr.len = m->m_len = frm - (u_int8_t *)key;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 1, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * Group Key Handshake Message 2 is sent by the supplicant to the
 * authenticator (see 8.5.4.2).
 */
int
ieee80211_send_group_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
    const struct ieee80211_key *k)
{
	struct ieee80211_eapol_key *key;
	u_int16_t info;
	struct mbuf *m;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info = EAPOL_KEY_KEYMIC | EAPOL_KEY_SECURE;

	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);

	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		/* WPA sets the key length and key id fields here */
		BE_WRITE_2(key->keylen, k->k_len);
		info |= (k->k_id & 3) << EAPOL_KEY_WPA_KID_SHIFT;
	}

	/* write the key info field */
	BE_WRITE_2(key->info, info);

	/* empty key data field */
	m->m_pkthdr.len = m->m_len = sizeof(*key);

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		    ic->ic_if.if_xname, 2, 2, "group key",
		    ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiates either a 4-Way Handshake or Group Key Handshake,
 * or to report a MIC failure in a TKIP MSDU.
 */
int
ieee80211_send_eapol_key_req(struct ieee80211com *ic,
    struct ieee80211_node *ni, u_int16_t info, u_int64_t tsc)
{
	struct ieee80211_eapol_key *key;
	struct mbuf *m;

	m = ieee80211_get_eapol_key(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));

	info |= EAPOL_KEY_REQUEST;
	BE_WRITE_2(key->info, info);

	/* in case of TKIP MIC failure, fill the RSC field */
	if (info & EAPOL_KEY_ERROR)
		LE_WRITE_6(key->rsc, tsc);

	/* use our separate key replay counter for key requests */
	BE_WRITE_8(key->replaycnt, ni->ni_reqreplaycnt);
	ni->ni_reqreplaycnt++;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending EAPOL-Key request to %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr));

	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*	$OpenBSD: ieee80211_crypto.h,v 1.19 2008/08/12 19:29:07 damien Exp $	*/

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

#ifndef _NET80211_IEEE80211_CRYPTO_H_
#define _NET80211_IEEE80211_CRYPTO_H_

/*
 * 802.11 protocol crypto-related definitions.
 */

/*
 * 802.11 ciphers.
 */
enum ieee80211_cipher {
	IEEE80211_CIPHER_NONE		= 0x00000000,
	IEEE80211_CIPHER_USEGROUP	= 0x00000001,
	IEEE80211_CIPHER_WEP40		= 0x00000002,
	IEEE80211_CIPHER_TKIP		= 0x00000004,
	IEEE80211_CIPHER_CCMP		= 0x00000008,
	IEEE80211_CIPHER_WEP104		= 0x00000010,
	IEEE80211_CIPHER_AES128_CMAC	= 0x00000020	/* 11w */
};

/*
 * 802.11 Authentication and Key Management Protocols.
 */
enum ieee80211_akm {
	IEEE80211_AKM_NONE		= 0x00000000,
	IEEE80211_AKM_8021X		= 0x00000001,
	IEEE80211_AKM_PSK		= 0x00000002,
	IEEE80211_AKM_FBT_8021X		= 0x00000004,	/* 11r */
	IEEE80211_AKM_FBT_PSK		= 0x00000008,	/* 11r */
	IEEE80211_AKM_SHA256_8021X	= 0x00000010,	/* 11w */
	IEEE80211_AKM_SHA256_PSK	= 0x00000020	/* 11w */
};

#define	IEEE80211_KEYBUF_SIZE	16

#define IEEE80211_TKIP_HDRLEN	8
#define IEEE80211_TKIP_MICLEN	8
#define IEEE80211_TKIP_ICVLEN	4
#define IEEE80211_CCMP_HDRLEN	8
#define IEEE80211_CCMP_MICLEN	8

#define IEEE80211_PMK_LEN	32

struct ieee80211_key {
	u_int8_t		k_id;		/* identifier (0-5) */
	enum ieee80211_cipher	k_cipher;
	u_int			k_flags;
#define IEEE80211_KEY_GROUP	0x00000001	/* group data key */
#define IEEE80211_KEY_TX	0x00000002	/* Tx+Rx */
#define IEEE80211_KEY_IGTK	0x00000004	/* integrity group key */

	u_int			k_len;
	u_int64_t		k_rsc[IEEE80211_NUM_TID];
	u_int64_t		k_mgmt_rsc;
	u_int64_t		k_tsc;
	u_int8_t		k_key[32];
	void			*k_priv;
};

/* forward references */
struct	ieee80211com;
struct	ieee80211_node;

void	ieee80211_crypto_attach(struct ifnet *);
void	ieee80211_crypto_detach(struct ifnet *);

struct	ieee80211_key *ieee80211_get_txkey(struct ieee80211com *,
	    const struct ieee80211_frame *, struct ieee80211_node *);
struct	ieee80211_key *ieee80211_get_rxkey(struct ieee80211com *,
	    struct mbuf *, struct ieee80211_node *);
struct	mbuf *ieee80211_encrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_decrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);

int	ieee80211_set_key(struct ieee80211com *, struct ieee80211_node *,
	    struct ieee80211_key *);
void	ieee80211_delete_key(struct ieee80211com *, struct ieee80211_node *,
	    struct ieee80211_key *);

void	ieee80211_eapol_key_mic(struct ieee80211_eapol_key *,
	    const u_int8_t *);
int	ieee80211_eapol_key_check_mic(struct ieee80211_eapol_key *,
	    const u_int8_t *);
void	ieee80211_eapol_key_encrypt(struct ieee80211com *,
	    struct ieee80211_eapol_key *, const u_int8_t *);
int	ieee80211_eapol_key_decrypt(struct ieee80211_eapol_key *,
	    const u_int8_t *);

const	u_int8_t *ieee80211_get_pmk(struct ieee80211com *,
	    struct ieee80211_node *, const u_int8_t *);
void	ieee80211_derive_ptk(enum ieee80211_akm, const u_int8_t *,
	    const u_int8_t *, const u_int8_t *, const u_int8_t *,
	    const u_int8_t *, struct ieee80211_ptk *);
int	ieee80211_cipher_keylen(enum ieee80211_cipher);

int	ieee80211_wep_set_key(struct ieee80211com *, struct ieee80211_key *);
void	ieee80211_wep_delete_key(struct ieee80211com *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_wep_encrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_wep_decrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);

int	ieee80211_tkip_set_key(struct ieee80211com *, struct ieee80211_key *);
void	ieee80211_tkip_delete_key(struct ieee80211com *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_tkip_encrypt(struct ieee80211com *,
	    struct mbuf *, struct ieee80211_key *);
struct	mbuf *ieee80211_tkip_decrypt(struct ieee80211com *,
	    struct mbuf *, struct ieee80211_key *);
void	ieee80211_tkip_mic(struct mbuf *, int, const u_int8_t *,
	    u_int8_t[IEEE80211_TKIP_MICLEN]);
void	ieee80211_michael_mic_failure(struct ieee80211com *, u_int64_t);

int	ieee80211_ccmp_set_key(struct ieee80211com *, struct ieee80211_key *);
void	ieee80211_ccmp_delete_key(struct ieee80211com *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_ccmp_encrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_ccmp_decrypt(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);

int	ieee80211_bip_set_key(struct ieee80211com *, struct ieee80211_key *);
void	ieee80211_bip_delete_key(struct ieee80211com *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_bip_encap(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);
struct	mbuf *ieee80211_bip_decap(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_key *);

#endif /* _NET80211_IEEE80211_CRYPTO_H_ */

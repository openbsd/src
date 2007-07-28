/*	$OpenBSD: ieee80211_crypto.h,v 1.4 2007/07/28 11:01:19 damien Exp $	*/
/*	$NetBSD: ieee80211_crypto.h,v 1.2 2003/09/14 01:14:55 dyoung Exp $	*/

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
 * $FreeBSD: src/sys/net80211/ieee80211_crypto.h,v 1.2 2003/06/27 05:13:52 sam Exp $
 */
#ifndef _NET80211_IEEE80211_CRYPTO_H_
#define _NET80211_IEEE80211_CRYPTO_H_

/*
 * 802.11 protocol crypto-related definitions.
 */

/*
 * 802.11i ciphers.
 */
enum ieee80211_cipher {
	IEEE80211_CIPHER_NONE     = 0x00000000,
	IEEE80211_CIPHER_USEGROUP = 0x00000001,
	IEEE80211_CIPHER_WEP40    = 0x00000002,
	IEEE80211_CIPHER_TKIP     = 0x00000004,
	IEEE80211_CIPHER_CCMP     = 0x00000008,
	IEEE80211_CIPHER_WEP104   = 0x00000010
};

/*
 * 802.11i Authentication and Key Management.
 */
enum ieee80211_akm {
	IEEE80211_AKM_NONE	= 0x00000000,
	IEEE80211_AKM_IEEE8021X	= 0x00000001,
	IEEE80211_AKM_PSK	= 0x00000002
};

#define	IEEE80211_KEYBUF_SIZE	16

struct ieee80211_key {
	u_int8_t		k_id;		/* identifier (0-3) */
	enum ieee80211_cipher	k_cipher;
	u_int			k_flags;
#define IEEE80211_KEY_GROUP	0x00000001	/* group key */
#define IEEE80211_KEY_TX	0x00000002	/* Tx+Rx */

	u_int64_t		k_rsc;		/* receive sequence counter */
	int			k_len;
	u_int8_t		k_key[IEEE80211_KEYBUF_SIZE];
};

extern	void ieee80211_crypto_attach(struct ifnet *);
extern	void ieee80211_crypto_detach(struct ifnet *);
extern	struct mbuf *ieee80211_wep_crypt(struct ifnet *, struct mbuf *, int);
extern	void ieee80211_derive_ptk(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, const u_int8_t *, const u_int8_t *, u_int8_t *,
	    size_t);
extern	int ieee80211_cipher_keylen(enum ieee80211_cipher);

#endif /* _NET80211_IEEE80211_CRYPTO_H_ */

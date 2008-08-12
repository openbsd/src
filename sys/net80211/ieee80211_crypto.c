/*	$OpenBSD: ieee80211_crypto.c,v 1.46 2008/08/12 16:14:05 damien Exp $	*/

/*-
 * Copyright (c) 2008 Damien Bergamini <damien.bergamini@free.fr>
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
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_priv.h>

#include <dev/rndvar.h>
#include <crypto/arc4.h>
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/hmac.h>
#include <crypto/rijndael.h>
#include <crypto/key_wrap.h>

void	ieee80211_prf(const u_int8_t *, size_t, const u_int8_t *, size_t,
	    const u_int8_t *, size_t, u_int8_t *, size_t);
void	ieee80211_derive_pmkid(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, u_int8_t *);

void
ieee80211_crypto_attach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_caps & IEEE80211_C_RSN) {
		ic->ic_rsnprotos = IEEE80211_PROTO_WPA | IEEE80211_PROTO_RSN;
		ic->ic_rsnakms = IEEE80211_AKM_PSK | IEEE80211_AKM_IEEE8021X;
		ic->ic_rsnciphers = IEEE80211_CIPHER_TKIP |
		    IEEE80211_CIPHER_CCMP;
		ic->ic_rsngroupcipher = IEEE80211_CIPHER_TKIP;
	}
	ic->ic_set_key = ieee80211_set_key;
	ic->ic_delete_key = ieee80211_delete_key;
}

void
ieee80211_crypto_detach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int i;

	/* clear all keys from memory */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (ic->ic_nw_keys[i].k_cipher != IEEE80211_CIPHER_NONE)
			(*ic->ic_delete_key)(ic, NULL, &ic->ic_nw_keys[i]);
		memset(&ic->ic_nw_keys[i], 0, sizeof(struct ieee80211_key));
	}
	memset(ic->ic_psk, 0, IEEE80211_PMK_LEN);
}

int
ieee80211_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	int error;

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
		error = ieee80211_wep_set_key(ic, k);
		break;
	case IEEE80211_CIPHER_TKIP:
		error = ieee80211_tkip_set_key(ic, k);
		break;
	case IEEE80211_CIPHER_CCMP:
		error = ieee80211_ccmp_set_key(ic, k);
		break;
	default:
		/* should not get there */
		error = EINVAL;
	}
	return error;
}

void
ieee80211_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
		ieee80211_wep_delete_key(ic, k);
		break;
	case IEEE80211_CIPHER_TKIP:
		ieee80211_tkip_delete_key(ic, k);
		break;
	case IEEE80211_CIPHER_CCMP:
		ieee80211_ccmp_delete_key(ic, k);
		break;
	default:
		/* should not get there */
		break;
	}
	memset(k, 0, sizeof(*k));	/* XXX */
}

/*
 * Retrieve the pairwise master key configured for a given node.
 * When PSK AKMP is in use, the pairwise master key is the pre-shared key
 * and the node is not used.
 */
const u_int8_t *
ieee80211_get_pmk(struct ieee80211com *ic, struct ieee80211_node *ni,
    const u_int8_t *pmkid)
{
	if (ni->ni_rsnakms == IEEE80211_AKM_PSK)
		return ic->ic_psk;	/* the PMK is the PSK */

	/* XXX find the PMK in the PMKSA cache using the PMKID */

	return NULL;	/* not yet supported */
}

struct ieee80211_key *
ieee80211_get_txkey(struct ieee80211com *ic, const struct ieee80211_frame *wh,
    struct ieee80211_node *ni)
{
	if (!(ic->ic_flags & IEEE80211_F_RSNON) ||
	    IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    ni->ni_rsncipher == IEEE80211_CIPHER_USEGROUP)
		return &ic->ic_nw_keys[ic->ic_wep_txkey];
	return &ni->ni_pairwise_key;
}

struct mbuf *
ieee80211_encrypt(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_key *k)
{
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
		m0 = ieee80211_wep_encrypt(ic, m0, k);
		break;
	case IEEE80211_CIPHER_TKIP:
		m0 = ieee80211_tkip_encrypt(ic, m0, k);
		break;
	case IEEE80211_CIPHER_CCMP:
		m0 = ieee80211_ccmp_encrypt(ic, m0, k);
		break;
	default:
		/* should not get there */
		m_freem(m0);
		m0 = NULL;
	}
	return m0;
}

struct mbuf *
ieee80211_decrypt(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni)
{
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;

	/* select the key for decryption */
	wh = mtod(m0, struct ieee80211_frame *);
	if (!(ic->ic_flags & IEEE80211_F_RSNON) ||
	    IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    ni->ni_rsncipher == IEEE80211_CIPHER_USEGROUP) {
		/* XXX check length! */
		int hdrlen = ieee80211_get_hdrlen(wh);
		const u_int8_t *ivp = (u_int8_t *)wh + hdrlen;
		/* key identifier is always located at the same index */
		int kid = ivp[IEEE80211_WEP_IVLEN] >> 6;
		k = &ic->ic_nw_keys[kid];
	} else
		k = &ni->ni_pairwise_key;

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
		m0 = ieee80211_wep_decrypt(ic, m0, k);
		break;
	case IEEE80211_CIPHER_TKIP:
		m0 = ieee80211_tkip_decrypt(ic, m0, k);
		break;
	case IEEE80211_CIPHER_CCMP:
		m0 = ieee80211_ccmp_decrypt(ic, m0, k);
		break;
	default:
		/* key not defined */
		m_freem(m0);
		m0 = NULL;
	}
	return m0;
}

/*
 * SHA1-based Pseudo-Random Function (see 8.5.1.1).
 */
void
ieee80211_prf(const u_int8_t *key, size_t key_len, const u_int8_t *label,
    size_t label_len, const u_int8_t *context, size_t context_len,
    u_int8_t *output, size_t len)
{
	HMAC_SHA1_CTX ctx;
	u_int8_t digest[SHA1_DIGEST_LENGTH];
	u_int8_t count;

	for (count = 0; len != 0; count++) {
		HMAC_SHA1_Init(&ctx, key, key_len);
		HMAC_SHA1_Update(&ctx, label, label_len);
		HMAC_SHA1_Update(&ctx, context, context_len);
		HMAC_SHA1_Update(&ctx, &count, 1);
		if (len < SHA1_DIGEST_LENGTH) {
			HMAC_SHA1_Final(digest, &ctx);
			/* truncate HMAC-SHA1 to len bytes */
			memcpy(output, digest, len);
			break;
		}
		HMAC_SHA1_Final(output, &ctx);
		output += SHA1_DIGEST_LENGTH;
		len -= SHA1_DIGEST_LENGTH;
	}
}

/*
 * Derive Pairwise Transient Key (PTK) (see 8.5.1.2).
 */
void
ieee80211_derive_ptk(enum ieee80211_akm akm, const u_int8_t *pmk,
    const u_int8_t *aa, const u_int8_t *spa, const u_int8_t *anonce,
    const u_int8_t *snonce, struct ieee80211_ptk *ptk)
{
	u_int8_t buf[2 * IEEE80211_ADDR_LEN + 2 * EAPOL_KEY_NONCE_LEN];
	int ret;

	/* Min(AA,SPA) || Max(AA,SPA) */
	ret = memcmp(aa, spa, IEEE80211_ADDR_LEN) < 0;
	memcpy(&buf[ 0], ret ? aa : spa, IEEE80211_ADDR_LEN);
	memcpy(&buf[ 6], ret ? spa : aa, IEEE80211_ADDR_LEN);

	/* Min(ANonce,SNonce) || Max(ANonce,SNonce) */
	ret = memcmp(anonce, snonce, EAPOL_KEY_NONCE_LEN) < 0;
	memcpy(&buf[12], ret ? anonce : snonce, EAPOL_KEY_NONCE_LEN);
	memcpy(&buf[44], ret ? snonce : anonce, EAPOL_KEY_NONCE_LEN);

	ieee80211_prf(pmk, IEEE80211_PMK_LEN, "Pairwise key expansion", 23,
	    buf, sizeof buf, (u_int8_t *)ptk, sizeof(*ptk));
}

/*
 * Derive Pairwise Master Key Identifier (PMKID) (see 8.5.1.2).
 */
void
ieee80211_derive_pmkid(const u_int8_t *pmk, size_t pmk_len, const u_int8_t *aa,
    const u_int8_t *spa, u_int8_t *pmkid)
{
	HMAC_SHA1_CTX ctx;
	u_int8_t digest[SHA1_DIGEST_LENGTH];

	HMAC_SHA1_Init(&ctx, pmk, pmk_len);
	HMAC_SHA1_Update(&ctx, "PMK Name", 8);
	HMAC_SHA1_Update(&ctx, aa, IEEE80211_ADDR_LEN);
	HMAC_SHA1_Update(&ctx, spa, IEEE80211_ADDR_LEN);
	HMAC_SHA1_Final(digest, &ctx);
	/* use the first 128 bits of the HMAC-SHA1 */
	memcpy(pmkid, digest, IEEE80211_PMKID_LEN);
}

typedef union _ANY_CTX {
	HMAC_MD5_CTX	md5;
	HMAC_SHA1_CTX	sha1;
} ANY_CTX;

/*
 * Compute the Key MIC field of an EAPOL-Key frame using the specified Key
 * Confirmation Key (KCK).  The hash function can be either HMAC-MD5 or
 * HMAC-SHA1 depending on the EAPOL-Key Key Descriptor Version.
 */
void
ieee80211_eapol_key_mic(struct ieee80211_eapol_key *key, const u_int8_t *kck)
{
	u_int8_t digest[SHA1_DIGEST_LENGTH];
	ANY_CTX ctx;	/* XXX off stack? */
	u_int len;

	len = BE_READ_2(key->len) + 4;

	switch (BE_READ_2(key->info) & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		HMAC_MD5_Init(&ctx.md5, kck, 16);
		HMAC_MD5_Update(&ctx.md5, (u_int8_t *)key, len);
		HMAC_MD5_Final(key->mic, &ctx.md5);
		break;
	case EAPOL_KEY_DESC_V2:
		HMAC_SHA1_Init(&ctx.sha1, kck, 16);
		HMAC_SHA1_Update(&ctx.sha1, (u_int8_t *)key, len);
		HMAC_SHA1_Final(digest, &ctx.sha1);
		/* truncate HMAC-SHA1 to its 128 MSBs */
		memcpy(key->mic, digest, EAPOL_KEY_MIC_LEN);
		break;
	}
}

/*
 * Check the MIC of a received EAPOL-Key frame using the specified Key
 * Confirmation Key (KCK).
 */
int
ieee80211_eapol_key_check_mic(struct ieee80211_eapol_key *key,
    const u_int8_t *kck)
{
	u_int8_t mic[EAPOL_KEY_MIC_LEN];

	memcpy(mic, key->mic, EAPOL_KEY_MIC_LEN);
	memset(key->mic, 0, EAPOL_KEY_MIC_LEN);
	ieee80211_eapol_key_mic(key, kck);

	return memcmp(key->mic, mic, EAPOL_KEY_MIC_LEN) != 0;
}

/*
 * Encrypt the Key Data field of an EAPOL-Key frame using the specified Key
 * Encryption Key (KEK).  The encryption algorithm can be either ARC4 or
 * AES Key Wrap depending on the EAPOL-Key Key Descriptor Version.
 */
void
ieee80211_eapol_key_encrypt(struct ieee80211com *ic,
    struct ieee80211_eapol_key *key, const u_int8_t *kek)
{
	union {
		struct rc4_ctx rc4;
		aes_key_wrap_ctx aes;
	} ctx;	/* XXX off stack? */
	u_int8_t keybuf[EAPOL_KEY_IV_LEN + 16];
	u_int16_t len, info;
	u_int8_t *data;
	int n;

	len  = BE_READ_2(key->paylen);
	info = BE_READ_2(key->info);
	data = (u_int8_t *)(key + 1);

	switch (info & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		/* set IV to the lower 16 octets of our global key counter */
		memcpy(key->iv, ic->ic_globalcnt + 16, 16);
		/* increment our global key counter (256-bit, big-endian) */
		for (n = 31; n >= 0 && ++ic->ic_globalcnt[n] == 0; n--);

		/* concatenate the EAPOL-Key IV field and the KEK */
		memcpy(keybuf, key->iv, EAPOL_KEY_IV_LEN);
		memcpy(keybuf + EAPOL_KEY_IV_LEN, kek, 16);

		rc4_keysetup(&ctx.rc4, keybuf, sizeof keybuf);
		/* discard the first 256 octets of the ARC4 key stream */
		rc4_skip(&ctx.rc4, RC4STATE);
		rc4_crypt(&ctx.rc4, data, data, len);
		break;
	case EAPOL_KEY_DESC_V2:
		if (len < 16 || (len & 7) != 0) {
			/* insert padding */
			n = (len < 16) ? 16 - len : 8 - (len & 7);
			data[len++] = IEEE80211_ELEMID_VENDOR;
			memset(&data[len], 0, n - 1);
			len += n - 1;
		}
		aes_key_wrap_set_key_wrap_only(&ctx.aes, kek, 16);
		aes_key_wrap(&ctx.aes, data, len / 8, data);
		len += 8;	/* AES Key Wrap adds 8 bytes */
		/* update key data length */
		BE_WRITE_2(key->paylen, len);
		/* update packet body length */
		BE_WRITE_2(key->len, sizeof(*key) + len - 4);
		break;
	}
}

/*
 * Decrypt the Key Data field of an EAPOL-Key frame using the specified Key
 * Encryption Key (KEK).  The encryption algorithm can be either ARC4 or
 * AES Key Wrap depending on the EAPOL-Key Key Descriptor Version.
 */
int
ieee80211_eapol_key_decrypt(struct ieee80211_eapol_key *key,
    const u_int8_t *kek)
{
	union {
		struct rc4_ctx rc4;
		aes_key_wrap_ctx aes;
	} ctx;	/* XXX off stack? */
	u_int8_t keybuf[EAPOL_KEY_IV_LEN + 16];
	u_int16_t len, info;
	u_int8_t *data;

	len  = BE_READ_2(key->paylen);
	info = BE_READ_2(key->info);
	data = (u_int8_t *)(key + 1);

	switch (info & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		/* concatenate the EAPOL-Key IV field and the KEK */
		memcpy(keybuf, key->iv, EAPOL_KEY_IV_LEN);
		memcpy(keybuf + EAPOL_KEY_IV_LEN, kek, 16);

		rc4_keysetup(&ctx.rc4, keybuf, sizeof keybuf);
		/* discard the first 256 octets of the ARC4 key stream */
		rc4_skip(&ctx.rc4, RC4STATE);
		rc4_crypt(&ctx.rc4, data, data, len);
		return 0;
	case EAPOL_KEY_DESC_V2:
		/* Key Data Length must be a multiple of 8 */
		if (len < 16 + 8 || (len & 7) != 0)
			return 1;
		len -= 8;	/* AES Key Wrap adds 8 bytes */
		aes_key_wrap_set_key(&ctx.aes, kek, 16);
		return aes_key_unwrap(&ctx.aes, data, data, len / 8);
	}

	return 1;	/* unknown Key Descriptor Version */
}

/*
 * Return the length in bytes of a cipher suite key (see Table 60).
 */
int
ieee80211_cipher_keylen(enum ieee80211_cipher cipher)
{
	switch (cipher) {
	case IEEE80211_CIPHER_WEP40:
		return 5;
	case IEEE80211_CIPHER_TKIP:
		return 32;
	case IEEE80211_CIPHER_CCMP:
		return 16;
	case IEEE80211_CIPHER_WEP104:
		return 13;
	default:	/* unknown cipher */
		return 0;
	}
}

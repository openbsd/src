/*	$OpenBSD: ieee80211_crypto.c,v 1.39 2008/04/18 09:16:14 djm Exp $	*/

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

#include <dev/rndvar.h>
#include <crypto/arc4.h>
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/rijndael.h>

/* similar to iovec except that it accepts const pointers */
struct vector {
	const void	*base;
	size_t		len;
};

void	ieee80211_prf(const u_int8_t *, size_t, struct vector *, int,
	    u_int8_t *, size_t);
void	ieee80211_derive_pmkid(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, u_int8_t *);
void	ieee80211_derive_gtk(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, u_int8_t *, size_t);

void
ieee80211_crypto_attach(struct ifnet *ifp)
{
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
 * AES Key Wrap (see RFC 3394).
 */
static const u_int8_t aes_key_wrap_iv[8] =
	{ 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };

static void
aes_key_wrap(const u_int8_t *kek, size_t kek_len, const u_int8_t *pt,
    size_t len, u_int8_t *ct)
{
	rijndael_ctx ctx;
	u_int8_t *a, *r, ar[16];
	u_int64_t t, b[2];
	size_t i;
	int j;

	/* allow ciphertext and plaintext to overlap (ct == pt) */
	ovbcopy(pt, ct + 8, len * 8);
	a = ct;
	memcpy(a, aes_key_wrap_iv, 8);	/* default IV */

	rijndael_set_key_enc_only(&ctx, (u_int8_t *)kek, kek_len * 8);

	for (j = 0, t = 1; j < 6; j++) {
		r = ct + 8;
		for (i = 0; i < len; i++, t++) {
			memcpy(ar, a, 8);
			memcpy(ar + 8, r, 8);
			rijndael_encrypt(&ctx, ar, (u_int8_t *)b);
			b[0] ^= htobe64(t);
			memcpy(a, &b[0], 8);
			memcpy(r, &b[1], 8);

			r += 8;
		}
	}
}

static int
aes_key_unwrap(const u_int8_t *kek, size_t kek_len, const u_int8_t *ct,
    u_int8_t *pt, size_t len)
{
	rijndael_ctx ctx;
	u_int8_t a[8], *r, b[16];
	u_int64_t t, ar[2];
	size_t i;
	int j;

	memcpy(a, ct, 8);
	/* allow ciphertext and plaintext to overlap (ct == pt) */
	ovbcopy(ct + 8, pt, len * 8);

	rijndael_set_key(&ctx, (u_int8_t *)kek, kek_len * 8);

	for (j = 0, t = 6 * len; j < 6; j++) {
		r = pt + (len - 1) * 8;
		for (i = 0; i < len; i++, t--) {
			memcpy(&ar[0], a, 8);
			ar[0] ^= htobe64(t);
			memcpy(&ar[1], r, 8);
			rijndael_decrypt(&ctx, (u_int8_t *)ar, b);
			memcpy(a, b, 8);
			memcpy(r, b + 8, 8);

			r -= 8;
		}
	}
	return memcmp(a, aes_key_wrap_iv, 8) != 0;
}

/*
 * HMAC-MD5 (see RFC 2104).
 */
static void
hmac_md5(const struct vector *vec, int vcnt, const u_int8_t *key,
    size_t key_len, u_int8_t digest[MD5_DIGEST_LENGTH])
{
	MD5_CTX ctx;
	u_int8_t k_pad[MD5_BLOCK_LENGTH];
	u_int8_t tk[MD5_DIGEST_LENGTH];
	int i;

	if (key_len > MD5_BLOCK_LENGTH) {
		MD5Init(&ctx);
		MD5Update(&ctx, key, key_len);
		MD5Final(tk, &ctx);

		key = tk;
		key_len = MD5_DIGEST_LENGTH;
	}

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < MD5_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x36;

	MD5Init(&ctx);
	MD5Update(&ctx, k_pad, MD5_BLOCK_LENGTH);
	for (i = 0; i < vcnt; i++)
		MD5Update(&ctx, vec[i].base, vec[i].len);
	MD5Final(digest, &ctx);

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < MD5_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x5c;

	MD5Init(&ctx);
	MD5Update(&ctx, k_pad, MD5_BLOCK_LENGTH);
	MD5Update(&ctx, digest, MD5_DIGEST_LENGTH);
	MD5Final(digest, &ctx);
}

/*
 * HMAC-SHA1 (see RFC 2104).
 */
static void
hmac_sha1(const struct vector *vec, int vcnt, const u_int8_t *key,
    size_t key_len, u_int8_t digest[SHA1_DIGEST_LENGTH])
{
	SHA1_CTX ctx;
	u_int8_t k_pad[SHA1_BLOCK_LENGTH];
	u_int8_t tk[SHA1_DIGEST_LENGTH];
	int i;

	if (key_len > SHA1_BLOCK_LENGTH) {
		SHA1Init(&ctx);
		SHA1Update(&ctx, key, key_len);
		SHA1Final(tk, &ctx);

		key = tk;
		key_len = SHA1_DIGEST_LENGTH;
	}

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < SHA1_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x36;

	SHA1Init(&ctx);
	SHA1Update(&ctx, k_pad, SHA1_BLOCK_LENGTH);
	for (i = 0; i < vcnt; i++)
		SHA1Update(&ctx, vec[i].base, vec[i].len);
	SHA1Final(digest, &ctx);

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < SHA1_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x5c;

	SHA1Init(&ctx);
	SHA1Update(&ctx, k_pad, SHA1_BLOCK_LENGTH);
	SHA1Update(&ctx, digest, SHA1_DIGEST_LENGTH);
	SHA1Final(digest, &ctx);
}

/*
 * SHA1-based Pseudo-Random Function (see 8.5.1.1).
 */
void
ieee80211_prf(const u_int8_t *key, size_t key_len, struct vector *vec,
    int vcnt, u_int8_t *output, size_t len)
{
	u_int8_t hash[SHA1_DIGEST_LENGTH];
	u_int8_t count = 0;

	/* single octet count, starts at 0 */
	vec[vcnt].base = &count;
	vec[vcnt].len  = 1;
	vcnt++;

	while (len >= SHA1_DIGEST_LENGTH) {
		hmac_sha1(vec, vcnt, key, key_len, output);
		count++;

		output += SHA1_DIGEST_LENGTH;
		len -= SHA1_DIGEST_LENGTH;
	}
	if (len > 0) {
		hmac_sha1(vec, vcnt, key, key_len, hash);
		/* truncate HMAC-SHA1 to len bytes */
		memcpy(output, hash, len);
	}
}

/*
 * Derive Pairwise Transient Key (PTK) (see 8.5.1.2).
 */
void
ieee80211_derive_ptk(const u_int8_t *pmk, size_t pmk_len, const u_int8_t *aa,
    const u_int8_t *spa, const u_int8_t *anonce, const u_int8_t *snonce,
    u_int8_t *ptk, size_t ptk_len)
{
	struct vector vec[6];	/* +1 for PRF */
	int ret;

	vec[0].base = "Pairwise key expansion";
	vec[0].len  = 23;	/* include trailing '\0' */

	ret = memcmp(aa, spa, IEEE80211_ADDR_LEN) < 0;
	/* Min(AA,SPA) */
	vec[1].base = ret ? aa : spa;
	vec[1].len  = IEEE80211_ADDR_LEN;
	/* Max(AA,SPA) */
	vec[2].base = ret ? spa : aa;
	vec[2].len  = IEEE80211_ADDR_LEN;

	ret = memcmp(anonce, snonce, EAPOL_KEY_NONCE_LEN) < 0;
	/* Min(ANonce,SNonce) */
	vec[3].base = ret ? anonce : snonce;
	vec[3].len  = EAPOL_KEY_NONCE_LEN;
	/* Max(ANonce,SNonce) */
	vec[4].base = ret ? snonce : anonce;
	vec[4].len  = EAPOL_KEY_NONCE_LEN;

	ieee80211_prf(pmk, pmk_len, vec, 5, ptk, ptk_len);
}

/*
 * Derive Pairwise Master Key Identifier (PMKID) (see 8.5.1.2).
 */
void
ieee80211_derive_pmkid(const u_int8_t *pmk, size_t pmk_len, const u_int8_t *aa,
    const u_int8_t *spa, u_int8_t *pmkid)
{
	struct vector vec[3];
	u_int8_t hash[SHA1_DIGEST_LENGTH];

	vec[0].base = "PMK Name";
	vec[0].len  = 8;	/* does *not* include trailing '\0' */
	vec[1].base = aa;
	vec[1].len  = IEEE80211_ADDR_LEN;
	vec[2].base = spa;
	vec[2].len  = IEEE80211_ADDR_LEN;

	hmac_sha1(vec, 3, pmk, pmk_len, hash);
	/* use the first 128 bits of the HMAC-SHA1 */
	memcpy(pmkid, hash, IEEE80211_PMKID_LEN);
}

/* unaligned big endian access */
#define BE_READ_2(p)				\
	((u_int16_t)				\
         ((((const u_int8_t *)(p))[0] << 8) |	\
          (((const u_int8_t *)(p))[1])))

#define BE_WRITE_2(p, v) do {			\
	((u_int8_t *)(p))[0] = (v) >> 8;	\
	((u_int8_t *)(p))[1] = (v) & 0xff;	\
} while (0)

/*
 * Compute the Key MIC field of an EAPOL-Key frame using the specified Key
 * Confirmation Key (KCK).  The hash function can be either HMAC-MD5 or
 * HMAC-SHA1 depending on the EAPOL-Key Key Descriptor Version.
 */
void
ieee80211_eapol_key_mic(struct ieee80211_eapol_key *key, const u_int8_t *kck)
{
	u_int8_t hash[SHA1_DIGEST_LENGTH];
	struct vector vec;

	vec.base = key;
	vec.len  = BE_READ_2(key->len) + 4;

	switch (BE_READ_2(key->info) & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		hmac_md5(&vec, 1, kck, 16, key->mic);
		break;
	case EAPOL_KEY_DESC_V2:
		hmac_sha1(&vec, 1, kck, 16, hash);
		/* truncate HMAC-SHA1 to its 128 MSBs */
		memcpy(key->mic, hash, EAPOL_KEY_MIC_LEN);
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
	struct rc4_ctx ctx;
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

		rc4_keysetup(&ctx, keybuf, sizeof keybuf);
		/* discard the first 256 octets of the ARC4 key stream */
		rc4_skip(&ctx, RC4STATE);
		rc4_crypt(&ctx, data, data, len);
		break;
	case EAPOL_KEY_DESC_V2:
		if (len < 16 || (len & 7) != 0) {
			/* insert padding */
			n = (len < 16) ? 16 - len : 8 - (len & 7);
			data[len++] = IEEE80211_ELEMID_VENDOR;
			memset(&data[len], 0, n - 1);
			len += n - 1;
		}
		aes_key_wrap(kek, 16, data, len / 8, data);
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
	struct rc4_ctx ctx;
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

		rc4_keysetup(&ctx, keybuf, sizeof keybuf);
		/* discard the first 256 octets of the ARC4 key stream */
		rc4_skip(&ctx, RC4STATE);
		rc4_crypt(&ctx, data, data, len);
		return 0;
	case EAPOL_KEY_DESC_V2:
		/* Key Data Length must be a multiple of 8 */
		if (len < 16 + 8 || (len & 7) != 0)
			return 1;
		len -= 8;	/* AES Key Wrap adds 8 bytes */
		return aes_key_unwrap(kek, 16, data, data, len / 8);
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

/*
 * Map PTK to IEEE 802.11 key (see 8.6).
 */
void
ieee80211_map_ptk(const struct ieee80211_ptk *ptk,
    enum ieee80211_cipher cipher, u_int64_t rsc, struct ieee80211_key *k)
{
	memset(k, 0, sizeof(*k));
	k->k_cipher = cipher;
	k->k_flags = IEEE80211_KEY_TX;
	k->k_len = ieee80211_cipher_keylen(cipher);
	k->k_rsc[0] = rsc;
	memcpy(k->k_key, ptk->tk, k->k_len);
}

/*
 * Map GTK to IEEE 802.11 key (see 8.6).
 */
void
ieee80211_map_gtk(const u_int8_t *gtk, enum ieee80211_cipher cipher, int kid,
    int txflag, u_int64_t rsc, struct ieee80211_key *k)
{
	memset(k, 0, sizeof(*k));
	k->k_id = kid;
	k->k_cipher = cipher;
	k->k_flags = IEEE80211_KEY_GROUP;
	if (txflag)
		k->k_flags |= IEEE80211_KEY_TX;
	k->k_len = ieee80211_cipher_keylen(cipher);
	k->k_rsc[0] = rsc;
	memcpy(k->k_key, gtk, k->k_len);
}

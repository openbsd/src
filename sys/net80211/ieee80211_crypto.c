/*	$OpenBSD: ieee80211_crypto.c,v 1.24 2007/07/24 19:39:22 damien Exp $	*/
/*	$NetBSD: ieee80211_crypto.c,v 1.5 2003/12/14 09:56:53 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007 Damien Bergamini
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
 */

#include "bpfilter.h"

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

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

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

struct vector {
	const void	*base;
	size_t		len;
};

void	ieee80211_crc_init(void);
u_int32_t ieee80211_crc_update(u_int32_t, const u_int8_t *, int);
void	ieee80211_aes_key_wrap(const u_int8_t *, size_t, const u_int8_t *,
	    size_t, u_int8_t *);
int	ieee80211_aes_key_unwrap(const u_int8_t *, size_t, const u_int8_t *,
	    u_int8_t *, size_t);
void	ieee80211_hmac_md5_v(const struct vector *, int, const u_int8_t *,
	    size_t, u_int8_t digest[]);
void	ieee80211_hmac_md5(const u_int8_t *, size_t, const u_int8_t *, size_t,
	    u_int8_t digest[]);
void	ieee80211_hmac_sha1_v(const struct vector *, int, const u_int8_t *,
	    size_t, u_int8_t digest[]);
void	ieee80211_hmac_sha1(const u_int8_t *, size_t, const u_int8_t *, size_t,
	    u_int8_t digest[]);
void	ieee80211_prf(const u_int8_t *, size_t, struct vector *, int,
	    u_int8_t *, size_t);
void	ieee80211_derive_ptk(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, const u_int8_t *, const u_int8_t *, u_int8_t *,
	    size_t);
void	ieee80211_derive_pmkid(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, u_int8_t *);
void	ieee80211_derive_gtk(const u_int8_t *, size_t, const u_int8_t *,
	    const u_int8_t *, u_int8_t *, size_t);

void
ieee80211_crypto_attach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	ieee80211_crc_init();

	/* initialize 256-bit global key counter to a random value */
	get_random_bytes(ic->ic_globalcnt, EAPOL_KEY_NONCE_LEN);
}

void
ieee80211_crypto_detach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_wep_ctx != NULL) {
		free(ic->ic_wep_ctx, M_DEVBUF);
		ic->ic_wep_ctx = NULL;
	}
}

/* Round up to a multiple of IEEE80211_WEP_KEYLEN + IEEE80211_WEP_IVLEN */
#define klen_round(x)							\
	(((x) + (IEEE80211_WEP_KEYLEN + IEEE80211_WEP_IVLEN - 1)) &	\
	~(IEEE80211_WEP_KEYLEN + IEEE80211_WEP_IVLEN - 1))

struct mbuf *
ieee80211_wep_crypt(struct ifnet *ifp, struct mbuf *m0, int txflag)
{
	struct ieee80211com *ic = (void *)ifp;
	struct mbuf *m, *n, *n0;
	struct ieee80211_frame *wh;
	int i, left, len, moff, noff, kid;
	u_int32_t iv, crc;
	u_int8_t *ivp;
	void *ctx;
	u_int8_t keybuf[klen_round(IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE)];
	u_int8_t crcbuf[IEEE80211_WEP_CRCLEN];

	n0 = NULL;
	if ((ctx = ic->ic_wep_ctx) == NULL) {
		ctx = malloc(sizeof(struct rc4_ctx), M_DEVBUF, M_NOWAIT);
		if (ctx == NULL) {
			ic->ic_stats.is_crypto_nomem++;
			goto fail;
		}
		ic->ic_wep_ctx = ctx;
	}
	m = m0;
	left = m->m_pkthdr.len;
	MGET(n, M_DONTWAIT, m->m_type);
	n0 = n;
	if (n == NULL) {
		if (txflag)
			ic->ic_stats.is_tx_nombuf++;
		else
			ic->ic_stats.is_rx_nombuf++;
		goto fail;
	}
	M_DUP_PKTHDR(n, m);
	len = IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
	if (txflag) {
		n->m_pkthdr.len += len;
	} else {
		n->m_pkthdr.len -= len;
		left -= len;
	}
	n->m_len = MHLEN;
	if (n->m_pkthdr.len >= MINCLSIZE) {
		MCLGET(n, M_DONTWAIT);
		if (n->m_flags & M_EXT)
			n->m_len = n->m_ext.ext_size;
	}
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] &
	     (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_QOS)) ==
	    (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS))
		len = sizeof(struct ieee80211_qosframe);
	else
		len = sizeof(struct ieee80211_frame);
	memcpy(mtod(n, caddr_t), wh, len);
	wh = mtod(n, struct ieee80211_frame *);
	left -= len;
	moff = len;
	noff = len;
	if (txflag) {
		kid = ic->ic_wep_txkey;
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
		iv = ic->ic_iv ? ic->ic_iv : arc4random();
		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 8
		 */
		if (iv >= 0x03ff00 &&
		    (iv & 0xf8ff00) == 0x00ff00)
			iv += 0x000100;
		ic->ic_iv = iv + 1;
		/* put iv in little endian to prepare 802.11i */
		ivp = mtod(n, u_int8_t *) + noff;
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++) {
			ivp[i] = iv & 0xff;
			iv >>= 8;
		}
		ivp[IEEE80211_WEP_IVLEN] = kid << 6;	/* pad and keyid */
		noff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	} else {
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		ivp = mtod(m, u_int8_t *) + moff;
		kid = ivp[IEEE80211_WEP_IVLEN] >> 6;
		moff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	}

	/*
	 * Copy the IV and the key material.  The input key has been padded
	 * with zeros by the ioctl.  The output key buffer length is rounded
	 * to a multiple of 64bit to allow variable length keys padded by
	 * zeros.
	 */
	bzero(&keybuf, sizeof(keybuf));
	memcpy(keybuf, ivp, IEEE80211_WEP_IVLEN);
	memcpy(keybuf + IEEE80211_WEP_IVLEN, ic->ic_nw_keys[kid].k_key,
	    ic->ic_nw_keys[kid].k_len);
	len = klen_round(IEEE80211_WEP_IVLEN + ic->ic_nw_keys[kid].k_len);
	rc4_keysetup(ctx, keybuf, len);

	/* encrypt with calculating CRC */
	crc = ~0;
	while (left > 0) {
		len = m->m_len - moff;
		if (len == 0) {
			m = m->m_next;
			moff = 0;
			continue;
		}
		if (len > n->m_len - noff) {
			len = n->m_len - noff;
			if (len == 0) {
				MGET(n->m_next, M_DONTWAIT, n->m_type);
				if (n->m_next == NULL) {
					if (txflag)
						ic->ic_stats.is_tx_nombuf++;
					else
						ic->ic_stats.is_rx_nombuf++;
					goto fail;
				}
				n = n->m_next;
				n->m_len = MLEN;
				if (left >= MINCLSIZE) {
					MCLGET(n, M_DONTWAIT);
					if (n->m_flags & M_EXT)
						n->m_len = n->m_ext.ext_size;
				}
				noff = 0;
				continue;
			}
		}
		if (len > left)
			len = left;
		rc4_crypt(ctx, mtod(m, caddr_t) + moff,
		    mtod(n, caddr_t) + noff, len);
		if (txflag)
			crc = ieee80211_crc_update(crc,
			    mtod(m, u_int8_t *) + moff, len);
		else
			crc = ieee80211_crc_update(crc,
			    mtod(n, u_int8_t *) + noff, len);
		left -= len;
		moff += len;
		noff += len;
	}
	crc = ~crc;
	if (txflag) {
		*(u_int32_t *)crcbuf = htole32(crc);
		if (n->m_len >= noff + sizeof(crcbuf))
			n->m_len = noff + sizeof(crcbuf);
		else {
			n->m_len = noff;
			MGET(n->m_next, M_DONTWAIT, n->m_type);
			if (n->m_next == NULL) {
				ic->ic_stats.is_tx_nombuf++;
				goto fail;
			}
			n = n->m_next;
			n->m_len = sizeof(crcbuf);
			noff = 0;
		}
		rc4_crypt(ctx, crcbuf, mtod(n, caddr_t) + noff,
		    sizeof(crcbuf));
	} else {
		n->m_len = noff;
		for (noff = 0; noff < sizeof(crcbuf); noff += len) {
			len = sizeof(crcbuf) - noff;
			if (len > m->m_len - moff)
				len = m->m_len - moff;
			if (len > 0)
				rc4_crypt(ctx, mtod(m, caddr_t) + moff,
				    crcbuf + noff, len);
			m = m->m_next;
			moff = 0;
		}
		if (crc != letoh32(*(u_int32_t *)crcbuf)) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				printf("%s: decrypt CRC error\n",
				    ifp->if_xname);
				if (ieee80211_debug > 1)
					ieee80211_dump_pkt(n0->m_data,
					    n0->m_len, -1, -1);
			}
#endif
			ic->ic_stats.is_rx_decryptcrc++;
			goto fail;
		}
	}
	m_freem(m0);
	return n0;

 fail:
	m_freem(m0);
	m_freem(n0);
	return NULL;
}

/*
 * CRC 32 -- routine from RFC 2083
 */

/* Table of CRCs of all 8-bit messages */
static u_int32_t ieee80211_crc_table[256];

/* Make the table for a fast CRC. */
void
ieee80211_crc_init(void)
{
	u_int32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (u_int32_t)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320UL ^ (c >> 1);
			else
				c = c >> 1;
		}
		ieee80211_crc_table[n] = c;
	}
}

/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC
 */
u_int32_t
ieee80211_crc_update(u_int32_t crc, const u_int8_t *buf, int len)
{
	const u_int8_t *endbuf;

	for (endbuf = buf + len; buf < endbuf; buf++)
		crc = ieee80211_crc_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
	return crc;
}

/*
 * AES Key Wrap Algorithm (see RFC 3394).
 */
static const u_int8_t aes_key_wrap_iv[8] =
	{ 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };

void
ieee80211_aes_key_wrap(const u_int8_t *kek, size_t kek_len, const u_int8_t *pt,
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

int
ieee80211_aes_key_unwrap(const u_int8_t *kek, size_t kek_len,
    const u_int8_t *ct, u_int8_t *pt, size_t len)
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

void
ieee80211_hmac_md5_v(const struct vector *vec, int vcnt, const u_int8_t *key,
    size_t key_len, u_int8_t digest[MD5_DIGEST_LENGTH])
{
	MD5_CTX ctx;
	u_int8_t k_pad[MD5_BLOCK_LENGTH];
	u_int8_t tk[MD5_DIGEST_LENGTH];
	int i;

	if (key_len > MD5_BLOCK_LENGTH) {
		MD5Init(&ctx);
		MD5Update(&ctx, (u_int8_t *)key, key_len);
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
		MD5Update(&ctx, (u_int8_t *)vec[i].base, vec[i].len);
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

void
ieee80211_hmac_md5(const u_int8_t *text, size_t text_len, const u_int8_t *key,
    size_t key_len, u_int8_t digest[MD5_DIGEST_LENGTH])
{
	struct vector vec;
	vec.base = text;
	vec.len  = text_len;
	ieee80211_hmac_md5_v(&vec, 1, key, key_len, digest);
}

void
ieee80211_hmac_sha1_v(const struct vector *vec, int vcnt, const u_int8_t *key,
    size_t key_len, u_int8_t digest[SHA1_DIGEST_LENGTH])
{
	SHA1_CTX ctx;
	u_int8_t k_pad[SHA1_BLOCK_LENGTH];
	u_int8_t tk[SHA1_DIGEST_LENGTH];
	int i;

	if (key_len > SHA1_BLOCK_LENGTH) {
		SHA1Init(&ctx);
		SHA1Update(&ctx, (u_int8_t *)key, key_len);
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
		SHA1Update(&ctx, (u_int8_t *)vec[i].base, vec[i].len);
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

void
ieee80211_hmac_sha1(const u_int8_t *text, size_t text_len, const u_int8_t *key,
    size_t key_len, u_int8_t digest[SHA1_DIGEST_LENGTH])
{
	struct vector vec;
	vec.base = text;
	vec.len  = text_len;
	ieee80211_hmac_sha1_v(&vec, 1, key, key_len, digest);
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

	while (len > SHA1_DIGEST_LENGTH) {
		ieee80211_hmac_sha1_v(vec, vcnt, key, key_len, output);
		count++;

		output += SHA1_DIGEST_LENGTH;
		len -= SHA1_DIGEST_LENGTH;
	}
	if (len > 0) {
		ieee80211_hmac_sha1_v(vec, vcnt, key, key_len, hash);
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

	ieee80211_hmac_sha1_v(vec, 3, pmk, pmk_len, hash);
	/* use the first 128 bits of the HMAC-SHA1 */
	memcpy(pmkid, hash, IEEE80211_PMKID_LEN);
}

/*
 * Derive Group Temporal Key (GTK) (see 8.5.1.3).
 */
void
ieee80211_derive_gtk(const u_int8_t *gmk, size_t gmk_len, const u_int8_t *aa,
    const u_int8_t *gnonce, u_int8_t *gtk, size_t gtk_len)
{
	struct vector vec[4];	/* +1 for PRF */

	vec[0].base = "Group key expansion";
	vec[0].len  = 20;	/* include trailing '\0' */
	vec[1].base = aa;
	vec[1].len  = IEEE80211_ADDR_LEN;
	vec[2].base = gnonce;
	vec[2].len  = EAPOL_KEY_NONCE_LEN;

	ieee80211_prf(gmk, gmk_len, vec, 3, gtk, gtk_len);
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
	u_int16_t len, info;

	len  = BE_READ_2(key->len);
	info = BE_READ_2(key->info);
	KASSERT(!(info & EAPOL_KEY_KEYMIC));

	switch (info & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		ieee80211_hmac_md5(kck, 16, (u_int8_t *)key, len, key->mic);
		break;
	case EAPOL_KEY_DESC_V2:
		ieee80211_hmac_sha1(kck, 16, (u_int8_t *)key, len, hash);
		/* truncate HMAC-SHA1 to its 128 MSBs */
		memcpy(key->mic, hash, EAPOL_KEY_MIC_LEN);
		break;
	}

	/* set the Key MIC field */
	info |= EAPOL_KEY_KEYMIC;
	BE_WRITE_2(key->info, info);
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
	u_int8_t buf[EAPOL_KEY_IV_LEN + 16];
	u_int16_t len, info;
	u_int8_t *data;
	int n;

	len  = BE_READ_2(key->paylen);
	info = BE_READ_2(key->info);
	/* should not come here if key data is already encrypted */
	KASSERT(!(info & EAPOL_KEY_ENCRYPTED));
	data = (u_int8_t *)(key + 1);

	switch (info & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		/* set IV to the lower 16 octets of our global key counter */
		memcpy(key->iv, ic->ic_globalcnt + 16, 16);
		/* increment our global key counter (256-bit, big-endian) */
		for (n = 31; n >= 0 && ++ic->ic_globalcnt[n] == 0; n--);

		/* concatenate the EAPOL-Key IV field and the KEK */
		memcpy(buf, key->iv, EAPOL_KEY_IV_LEN);
		memcpy(buf + EAPOL_KEY_IV_LEN, kek, 16);

		rc4_keysetup(&ctx, buf, sizeof buf);
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
		ieee80211_aes_key_wrap(kek, 16, data, len / 8, data);
		len += 8;	/* AES Key Wrap adds 8 bytes */
		/* update key data length */
		BE_WRITE_2(key->paylen, len);
		break;
	}

	/* set the Encrypted Key Data field */
	info |= EAPOL_KEY_ENCRYPTED;
	BE_WRITE_2(key->info, info);
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
	u_int8_t buf[EAPOL_KEY_IV_LEN + 16];
	u_int16_t len, info;
	u_int8_t *data;

	len  = BE_READ_2(key->paylen);
	info = BE_READ_2(key->info);
	/* should not come here if key data is not encrypted */
	KASSERT(info & EAPOL_KEY_ENCRYPTED);
	data = (u_int8_t *)(key + 1);

	switch (info & EAPOL_KEY_VERSION_MASK) {
	case EAPOL_KEY_DESC_V1:
		/* concatenate the EAPOL-Key IV field and the KEK */
		memcpy(buf, key->iv, EAPOL_KEY_IV_LEN);
		memcpy(buf + EAPOL_KEY_IV_LEN, kek, 16);

		rc4_keysetup(&ctx, buf, sizeof buf);
		/* discard the first 256 octets of the ARC4 key stream */
		rc4_skip(&ctx, RC4STATE);
		rc4_crypt(&ctx, data, data, len);
		return 0;
	case EAPOL_KEY_DESC_V2:
		/* Key Data Length must be a multiple of 8 */
		if (len < 16 + 8 || (len & 7) != 0)
			return 1;
		len = (len / 8) - 1;
		return ieee80211_aes_key_unwrap(kek, 16, data, data, len);
	}

	return 1;	/* unknown Key Descriptor Version */
}

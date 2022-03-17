/* $OpenBSD: ssl_transcript.c,v 1.7 2022/03/17 17:22:16 jsing Exp $ */
/*
 * Copyright (c) 2017 Joel Sing <jsing@openbsd.org>
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

#include <openssl/ssl.h>

#include "ssl_locl.h"

int
tls1_transcript_hash_init(SSL *s)
{
	const unsigned char *data;
	const EVP_MD *md;
	size_t len;

	tls1_transcript_hash_free(s);

	if (!ssl_get_handshake_evp_md(s, &md)) {
		SSLerrorx(ERR_R_INTERNAL_ERROR);
		goto err;
	}

	if ((s->s3->handshake_hash = EVP_MD_CTX_new()) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!EVP_DigestInit_ex(s->s3->handshake_hash, md, NULL)) {
		SSLerror(s, ERR_R_EVP_LIB);
		goto err;
	}

	if (!tls1_transcript_data(s, &data, &len)) {
		SSLerror(s, SSL_R_BAD_HANDSHAKE_LENGTH);
		goto err;
	}
	if (!tls1_transcript_hash_update(s, data, len)) {
		SSLerror(s, ERR_R_EVP_LIB);
		goto err;
	}

	return 1;

 err:
	tls1_transcript_hash_free(s);

	return 0;
}

int
tls1_transcript_hash_update(SSL *s, const unsigned char *buf, size_t len)
{
	if (s->s3->handshake_hash == NULL)
		return 1;

	return EVP_DigestUpdate(s->s3->handshake_hash, buf, len);
}

int
tls1_transcript_hash_value(SSL *s, unsigned char *out, size_t len,
    size_t *outlen)
{
	EVP_MD_CTX *mdctx = NULL;
	unsigned int mdlen;
	int ret = 0;

	if (s->s3->handshake_hash == NULL)
		goto err;

	if (EVP_MD_CTX_size(s->s3->handshake_hash) > len)
		goto err;

	if ((mdctx = EVP_MD_CTX_new()) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!EVP_MD_CTX_copy_ex(mdctx, s->s3->handshake_hash)) {
		SSLerror(s, ERR_R_EVP_LIB);
		goto err;
	}
	if (!EVP_DigestFinal_ex(mdctx, out, &mdlen)) {
		SSLerror(s, ERR_R_EVP_LIB);
		goto err;
	}
	if (outlen != NULL)
		*outlen = mdlen;

	ret = 1;

 err:
	EVP_MD_CTX_free(mdctx);

	return (ret);
}

void
tls1_transcript_hash_free(SSL *s)
{
	EVP_MD_CTX_free(s->s3->handshake_hash);
	s->s3->handshake_hash = NULL;
}

int
tls1_transcript_init(SSL *s)
{
	if (s->s3->handshake_transcript != NULL)
		return 0;

	if ((s->s3->handshake_transcript = BUF_MEM_new()) == NULL)
		return 0;

	tls1_transcript_reset(s);

	return 1;
}

void
tls1_transcript_free(SSL *s)
{
	BUF_MEM_free(s->s3->handshake_transcript);
	s->s3->handshake_transcript = NULL;
}

void
tls1_transcript_reset(SSL *s)
{
	/*
	 * We should check the return value of BUF_MEM_grow_clean(), however
	 * due to yet another bad API design, when called with a length of zero
	 * it is impossible to tell if it succeeded (returning a length of zero)
	 * or if it failed (and returned zero)... our implementation never
	 * fails with a length of zero, so we trust all is okay...
	 */
	(void)BUF_MEM_grow_clean(s->s3->handshake_transcript, 0);

	tls1_transcript_unfreeze(s);
}

int
tls1_transcript_append(SSL *s, const unsigned char *buf, size_t len)
{
	size_t olen, nlen;

	if (s->s3->handshake_transcript == NULL)
		return 1;

	if (s->s3->flags & TLS1_FLAGS_FREEZE_TRANSCRIPT)
		return 1;

	olen = s->s3->handshake_transcript->length;
	nlen = olen + len;

	if (nlen < olen)
		return 0;

	if (BUF_MEM_grow(s->s3->handshake_transcript, nlen) == 0)
		return 0;

	memcpy(s->s3->handshake_transcript->data + olen, buf, len);

	return 1;
}

int
tls1_transcript_data(SSL *s, const unsigned char **data, size_t *len)
{
	if (s->s3->handshake_transcript == NULL)
		return 0;

	*data = s->s3->handshake_transcript->data;
	*len = s->s3->handshake_transcript->length;

	return 1;
}

void
tls1_transcript_freeze(SSL *s)
{
	s->s3->flags |= TLS1_FLAGS_FREEZE_TRANSCRIPT;
}

void
tls1_transcript_unfreeze(SSL *s)
{
	s->s3->flags &= ~TLS1_FLAGS_FREEZE_TRANSCRIPT;
}

int
tls1_transcript_record(SSL *s, const unsigned char *buf, size_t len)
{
	if (!tls1_transcript_hash_update(s, buf, len))
		return 0;

	if (!tls1_transcript_append(s, buf, len))
		return 0;

	return 1;
}

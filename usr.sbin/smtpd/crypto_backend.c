/*	$OpenBSD: crypto_backend.c,v 1.4 2012/08/31 22:40:56 fgsch Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "smtpd.h"
#include "log.h"


#define	CRYPTO_BUFFER_SIZE	16384


static struct crypto_params {
	const EVP_CIPHER	*cipher;
	const EVP_MD		*digest;
	uint8_t			 key[EVP_MAX_MD_SIZE];
} cp;


int
crypto_setup(const char *cipher, const char *digest, const char *key)
{
	EVP_MD_CTX	*mdctx;
	int		 mdlen;

	log_info("crypto: setting up crypto parameters");
	bzero(&cp, sizeof cp);

	cp.cipher = EVP_get_cipherbyname(cipher);
	if (cp.cipher == NULL) {
		log_warnx("crypto: unknown cipher: %s", cipher);
		return 0;
	}
	log_info("crypto: cipher: %s", cipher);

	cp.digest = EVP_get_digestbyname(digest);
	if (cp.digest == NULL) {
		log_warnx("crypto: unknown digest: %s", digest);
		return 0;
	}
	log_info("crypto: digest: %s", digest);

	mdctx = EVP_MD_CTX_create();
	if (mdctx == NULL) {
		log_warnx("crypto: unable to create digest context");
		return 0;
	}

	EVP_DigestInit_ex(mdctx, cp.digest, NULL);
	EVP_DigestUpdate(mdctx, key, strlen(key));
	EVP_DigestFinal_ex(mdctx, cp.key, &mdlen);

	EVP_MD_CTX_destroy(mdctx);

	log_info("crypto: crypto parameters set");
	return 1;
}

void
crypto_clear(void)
{
	/* to be called in EVERY process *but* queue before event dispatch */
	bzero(&cp, sizeof cp);
}

int
crypto_encrypt_file(FILE *in, FILE *out)
{
	EVP_CIPHER_CTX	ctx;
	uint8_t		ibuf[CRYPTO_BUFFER_SIZE];
	uint8_t		obuf[CRYPTO_BUFFER_SIZE];
	size_t		r,w;
	size_t		bs = EVP_CIPHER_block_size(cp.cipher);
	int		olen;
	int		ret = 0;

	log_debug("crypto_encrypt_file");

	/* generate IV and encrypt it */
	for (r = 0; r < (size_t)EVP_CIPHER_iv_length(cp.cipher); ++r)
		ibuf[r] = arc4random_uniform(0xff+1);
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit(&ctx, cp.cipher, cp.key, NULL);
	if (! EVP_EncryptUpdate(&ctx, obuf, &olen, ibuf, r))
		goto end;
	if (olen && (w = fwrite(obuf, 1, olen, out)) != (size_t)olen)
		goto end;
	if (! EVP_EncryptFinal(&ctx, obuf, &olen))
		goto end;
	if (olen && (w = fwrite(obuf, 1, olen, out)) != (size_t)olen)
		goto end;

	/* encrypt real content */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit(&ctx, cp.cipher, cp.key, ibuf);
	while ((r = fread(ibuf, 1, CRYPTO_BUFFER_SIZE-bs, in)) != 0) {
		if (! EVP_EncryptUpdate(&ctx, obuf, &olen, ibuf, r))
			goto end;
		if (olen && (w = fwrite(obuf, olen, 1, out)) != 1)
			goto end;
	}
	if (! feof(in))
		goto end;

	if (! EVP_EncryptFinal(&ctx, obuf, &olen))
		goto end;
	if (olen && (w = fwrite(obuf, olen, 1, out)) != 1)
		goto end;
	fflush(out);

	ret = 1;

end:
	EVP_CIPHER_CTX_cleanup(&ctx);
	log_debug("crypto_encrypt_file: ret=%d", ret);
	return ret;
}

int
crypto_decrypt_file(FILE *in, FILE *out)
{
	EVP_CIPHER_CTX	ctx;
	uint8_t		ibuf[CRYPTO_BUFFER_SIZE];
	uint8_t		obuf[CRYPTO_BUFFER_SIZE];
	size_t		r,w;
	size_t		bs = EVP_CIPHER_block_size(cp.cipher);
	int		olen;
	int		ret = 0;

	log_debug("crypto_decrypt_file");

	/* extract and decrypt IV */
	r = fread(ibuf, 1, EVP_CIPHER_block_size(cp.cipher)*2, in);
	if (r != (size_t)EVP_CIPHER_block_size(cp.cipher)*2)
		goto end;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit(&ctx, cp.cipher, cp.key, NULL);
	if (! EVP_DecryptUpdate(&ctx, obuf, &olen, ibuf, r))
		goto end;
	if (! EVP_DecryptFinal(&ctx, obuf+olen, &olen))
		goto end;

	/* decrypt real content */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit(&ctx, cp.cipher, cp.key, obuf);
	while ((r = fread(ibuf, 1, CRYPTO_BUFFER_SIZE-bs, in)) != 0) {
		if (! EVP_DecryptUpdate(&ctx, obuf, &olen, ibuf, r))
			goto end;
		if (olen && (w = fwrite(obuf, olen, 1, out)) != 1)
			goto end;
	}
	if (! feof(in))
		goto end;

	if (! EVP_DecryptFinal(&ctx, obuf, &olen))
		goto end;
	if (olen && (w = fwrite(obuf, olen, 1, out)) != 1)
		goto end;
	fflush(out);
	ret = 1;

end:
	log_debug("crypto_decrypt_file: ret=%d", ret);
	EVP_CIPHER_CTX_cleanup(&ctx);
	return ret;
}

size_t
crypto_encrypt_buffer(const char *in, size_t inlen, char *out, size_t outlen)
{
	EVP_CIPHER_CTX	ctx;
	uint8_t		ibuf[CRYPTO_BUFFER_SIZE];
	uint8_t		obuf[CRYPTO_BUFFER_SIZE];
	int		olen;
	ssize_t		r;
	int		len = 0;
	int		ret = 0;

	log_debug("crypto_encrypt_buffer");

	/* out does not have enough room */
	if (outlen < inlen + (EVP_CIPHER_block_size(cp.cipher) * 4))
		goto end;

	bzero(ibuf, sizeof ibuf);
	bzero(obuf, sizeof obuf);

	/* generate IV and encrypt it */
	for (r = 0; r < EVP_CIPHER_iv_length(cp.cipher); ++r)
		ibuf[r] = arc4random_uniform(0xff+1);
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit(&ctx, cp.cipher, cp.key, NULL);
	if (! EVP_EncryptUpdate(&ctx, out, &olen, ibuf, r))
		goto end;
	if (! EVP_EncryptFinal(&ctx, out+olen, &olen))
		goto end;
	len += EVP_CIPHER_block_size(cp.cipher)*2;

	/* encrypt real content */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit(&ctx, cp.cipher, cp.key, ibuf);
	if (! EVP_EncryptUpdate(&ctx, out+len, &olen, in, inlen))
		goto end;
	len += olen;
	if (! EVP_EncryptFinal(&ctx, out+len, &olen))
		goto end;
	ret = len + olen;

end:
	log_debug("crypto_encrypt_buffer: ret=%d", ret);
	EVP_CIPHER_CTX_cleanup(&ctx);
	return ret;
}

size_t
crypto_decrypt_buffer(const char *in, size_t inlen, char *out, size_t outlen)
{
	EVP_CIPHER_CTX	ctx;
	uint8_t		obuf[CRYPTO_BUFFER_SIZE];
	int		olen;
	int		len = 0;
	int		ret = 0;

	log_debug("crypto_decrypt_buffer");

	/* out does not have enough room */
	if (outlen < inlen + (EVP_CIPHER_block_size(cp.cipher) * 4))
		goto end;

	bzero(obuf, sizeof obuf);

	/* extract and decrypt IV */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit(&ctx, cp.cipher, cp.key, NULL);
	if (! EVP_DecryptUpdate(&ctx, obuf, &olen, in,
		EVP_CIPHER_block_size(cp.cipher)*2))
		goto end;
	if (! EVP_DecryptFinal(&ctx, obuf+olen, &olen))
		goto end;
	inlen -= EVP_CIPHER_block_size(cp.cipher)*2;
	in    += EVP_CIPHER_block_size(cp.cipher)*2;

	/* decrypt real content */
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit(&ctx, cp.cipher, cp.key, obuf);
	if (! EVP_DecryptUpdate(&ctx, out, &olen, in, inlen))
		goto end;
	len += olen;
	if (! EVP_DecryptFinal(&ctx, out+len, &olen))
		goto end;
	len += olen;

	ret = len;

end:
	log_debug("crypto_decrypt_buffer: ret=%d", ret);
	EVP_CIPHER_CTX_cleanup(&ctx);
	return ret;
}

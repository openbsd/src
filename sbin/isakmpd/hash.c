/* $OpenBSD: hash.c,v 1.22 2008/09/06 12:01:34 djm Exp $	 */
/* $EOM: hash.c,v 1.10 1999/04/17 23:20:34 niklas Exp $	 */

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <string.h>
#include <md5.h>
#include <sha1.h>
#include <sha2.h>

#include "hash.h"
#include "log.h"

void	hmac_init(struct hash *, unsigned char *, unsigned int);
void	hmac_final(unsigned char *, struct hash *);

/* Temporary hash contexts.  */
static union {
	MD5_CTX		md5ctx;
	SHA1_CTX        sha1ctx;
	SHA2_CTX	sha2ctx;
} Ctx, Ctx2;

/* Temporary hash digest.  */
static unsigned char digest[HASH_MAX];

/* Encapsulation of hash functions.  */

static struct hash hashes[] = {
    {
	HASH_MD5, 5, MD5_SIZE, (void *)&Ctx.md5ctx, digest,
	sizeof(MD5_CTX), (void *)&Ctx2.md5ctx,
	(void (*)(void *))MD5Init,
	(void (*)(void *, unsigned char *, unsigned int))MD5Update,
	(void (*)(unsigned char *, void *))MD5Final,
	hmac_init,
	hmac_final
    }, {
	HASH_SHA1, 6, SHA1_SIZE, (void *)&Ctx.sha1ctx, digest,
	sizeof(SHA1_CTX), (void *)&Ctx2.sha1ctx,
	(void (*)(void *))SHA1Init,
	(void (*)(void *, unsigned char *, unsigned int))SHA1Update,
	(void (*)(unsigned char *, void *))SHA1Final,
	hmac_init,
	hmac_final
    }, {
	HASH_SHA2_256, 7, SHA2_256_SIZE, (void *)&Ctx.sha2ctx, digest,
	sizeof(SHA2_CTX), (void *)&Ctx2.sha2ctx,
	(void (*)(void *))SHA256Init,
	(void (*)(void *, unsigned char *, unsigned int))SHA256Update,
	(void (*)(u_int8_t *, void *))SHA256Final,
	hmac_init,
	hmac_final
    }, {
	HASH_SHA2_384, 8, SHA2_384_SIZE, (void *)&Ctx.sha2ctx, digest,
	sizeof(SHA2_CTX), (void *)&Ctx2.sha2ctx,
	(void (*)(void *))SHA384Init,
	(void (*)(void *, unsigned char *, unsigned int))SHA384Update,
	(void (*)(u_int8_t *, void *))SHA384Final,
	hmac_init,
	hmac_final
    }, {
	HASH_SHA2_512, 9, SHA2_512_SIZE, (void *)&Ctx.sha2ctx, digest,
	sizeof(SHA2_CTX), (void *)&Ctx2.sha2ctx,
	(void (*)(void *))SHA512Init,
	(void (*)(void *, unsigned char *, unsigned int))SHA512Update,
	(void (*)(u_int8_t *, void *))SHA512Final,
	hmac_init,
	hmac_final
    }
};

struct hash *
hash_get(enum hashes hashtype)
{
	size_t	i;

	LOG_DBG((LOG_CRYPTO, 60, "hash_get: requested algorithm %d",
	    hashtype));

	for (i = 0; i < sizeof hashes / sizeof hashes[0]; i++)
		if (hashtype == hashes[i].type)
			return &hashes[i];

	return 0;
}

/*
 * Initial a hash for HMAC usage this requires a special init function.
 * ctx, ctx2 hold the contexts, if you want to use the hash object for
 * something else in the meantime, be sure to store the contexts somewhere.
 */

void
hmac_init(struct hash *hash, unsigned char *okey, unsigned int len)
{
	unsigned int    i, blocklen = HMAC_BLOCKLEN;
	unsigned char   key[HMAC_BLOCKLEN];

	bzero(key, blocklen);
	if (len > blocklen) {
		/* Truncate key down to blocklen */
		hash->Init(hash->ctx);
		hash->Update(hash->ctx, okey, len);
		hash->Final(key, hash->ctx);
	} else {
		memcpy(key, okey, len);
	}

	/* HMAC I and O pad computation */
	for (i = 0; i < blocklen; i++)
		key[i] ^= HMAC_IPAD_VAL;

	hash->Init(hash->ctx);
	hash->Update(hash->ctx, key, blocklen);

	for (i = 0; i < blocklen; i++)
		key[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

	hash->Init(hash->ctx2);
	hash->Update(hash->ctx2, key, blocklen);

	bzero(key, blocklen);
}

/*
 * HMAC Final function
 */

void
hmac_final(unsigned char *dgst, struct hash *hash)
{
	hash->Final(dgst, hash->ctx);
	hash->Update(hash->ctx2, dgst, hash->hashsize);
	hash->Final(dgst, hash->ctx2);
}

/*	$OpenBSD: xform.c,v 1.28 2004/12/20 20:31:18 hshoexer Exp $	*/
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/rmd160.h>
#include <crypto/blf.h>
#include <crypto/cast.h>
#include <crypto/skipjack.h>
#include <crypto/rijndael.h>
#include <crypto/cryptodev.h>
#include <crypto/xform.h>
#include <crypto/deflate.h>

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);

int  des_set_key(caddr_t, caddr_t);
int  des1_setkey(u_int8_t **, u_int8_t *, int);
int  des3_setkey(u_int8_t **, u_int8_t *, int);
int  blf_setkey(u_int8_t **, u_int8_t *, int);
int  cast5_setkey(u_int8_t **, u_int8_t *, int);
int  skipjack_setkey(u_int8_t **, u_int8_t *, int);
int  rijndael128_setkey(u_int8_t **, u_int8_t *, int);
void des1_encrypt(caddr_t, u_int8_t *);
void des3_encrypt(caddr_t, u_int8_t *);
void blf_encrypt(caddr_t, u_int8_t *);
void cast5_encrypt(caddr_t, u_int8_t *);
void skipjack_encrypt(caddr_t, u_int8_t *);
void rijndael128_encrypt(caddr_t, u_int8_t *);
void des1_decrypt(caddr_t, u_int8_t *);
void des3_decrypt(caddr_t, u_int8_t *);
void blf_decrypt(caddr_t, u_int8_t *);
void cast5_decrypt(caddr_t, u_int8_t *);
void skipjack_decrypt(caddr_t, u_int8_t *);
void rijndael128_decrypt(caddr_t, u_int8_t *);
void des1_zerokey(u_int8_t **);
void des3_zerokey(u_int8_t **);
void blf_zerokey(u_int8_t **);
void cast5_zerokey(u_int8_t **);
void skipjack_zerokey(u_int8_t **);
void rijndael128_zerokey(u_int8_t **);
void null_encrypt(caddr_t, u_int8_t *);
void null_zerokey(u_int8_t **);
int  null_setkey(u_int8_t **, u_int8_t *, int);
void null_decrypt(caddr_t, u_int8_t *);

int MD5Update_int(void *, u_int8_t *, u_int16_t);
int SHA1Update_int(void *, u_int8_t *, u_int16_t);
int RMD160Update_int(void *, u_int8_t *, u_int16_t);
int SHA256_Update_int(void *, u_int8_t *, u_int16_t);
int SHA384_Update_int(void *, u_int8_t *, u_int16_t);
int SHA512_Update_int(void *, u_int8_t *, u_int16_t);

u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t lzs_dummy(u_int8_t *, u_int32_t, u_int8_t **);

/* Encryption instances */
struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	8, 8, 8,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	des1_zerokey,
};

struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	8, 24, 24,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	des3_zerokey
};

struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	8, 5, 56 /* 448 bits, max key */,
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	blf_zerokey
};

struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	8, 5, 16,
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	cast5_zerokey
};

struct enc_xform enc_xform_skipjack = {
	CRYPTO_SKIPJACK_CBC, "Skipjack",
	8, 10, 10,
	skipjack_encrypt,
	skipjack_decrypt,
	skipjack_setkey,
	skipjack_zerokey
};

struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	16, 16, 32,
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	rijndael128_zerokey,
};

struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 1, 32,
	NULL,
	NULL,
	NULL,
	NULL,
};

struct enc_xform enc_xform_null = {
	CRYPTO_NULL, "NULL",
	8, 24, 24,
	null_encrypt,
	null_decrypt,
	null_setkey,
	null_zerokey,
};

/* Authentication instances */
struct auth_hash auth_hash_hmac_md5_96 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, 16, 12, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1_96 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, 20, 12, sizeof(SHA1_CTX),
	(void (*) (void *)) SHA1Init, SHA1Update_int,
	(void (*) (u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_hmac_ripemd_160_96 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, 20, 12, sizeof(RMD160_CTX),
	(void (*)(void *)) RMD160Init, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_hmac_sha2_256_96 = {
	CRYPTO_SHA2_256_HMAC, "HMAC-SHA2-256",
	32, 32, 12, sizeof(SHA256_CTX),
	(void (*)(void *)) SHA256_Init, SHA256_Update_int,
	(void (*)(u_int8_t *, void *)) SHA256_Final
};

struct auth_hash auth_hash_hmac_sha2_384_96 = {
	CRYPTO_SHA2_384_HMAC, "HMAC-SHA2-384",
	48, 48, 12, sizeof(SHA384_CTX),
	(void (*)(void *)) SHA384_Init, SHA384_Update_int,
	(void (*)(u_int8_t *, void *)) SHA384_Final
};

struct auth_hash auth_hash_hmac_sha2_512_96 = {
	CRYPTO_SHA2_512_HMAC, "HMAC-SHA2-512",
	64, 64, 12, sizeof(SHA512_CTX),
	(void (*)(void *)) SHA512_Init, SHA512_Update_int,
	(void (*)(u_int8_t *, void *)) SHA512_Final
};

struct auth_hash auth_hash_key_md5 = {
	CRYPTO_MD5_KPDK, "Keyed MD5",
	0, 16, 16, sizeof(MD5_CTX),
	(void (*)(void *)) MD5Init, MD5Update_int,
	(void (*)(u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_key_sha1 = {
	CRYPTO_SHA1_KPDK, "Keyed SHA1",
	0, 20, 20, sizeof(SHA1_CTX),
	(void (*)(void *)) SHA1Init, SHA1Update_int,
	(void (*)(u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_md5 = {
	CRYPTO_MD5, "MD5",
	0, 16, 16, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_sha1 = {
	CRYPTO_SHA1, "SHA1",
	0, 20, 20, sizeof(SHA1_CTX),
	(void (*)(void *)) SHA1Init, SHA1Update_int,
	(void (*)(u_int8_t *, void *)) SHA1Final
};

/* Compression instance */
struct comp_algo comp_algo_deflate = {
	CRYPTO_DEFLATE_COMP, "Deflate",
	90, deflate_compress,
	deflate_decompress
};

struct comp_algo comp_algo_lzs = {
	CRYPTO_LZS_COMP, "LZS",
	90, lzs_dummy,
	lzs_dummy
};

/*
 * Encryption wrapper routines.
 */
void
des1_encrypt(caddr_t key, u_int8_t *blk)
{
	des_ecb_encrypt(blk, blk, key, 1);
}

void
des1_decrypt(caddr_t key, u_int8_t *blk)
{
	des_ecb_encrypt(blk, blk, key, 0);
}

int
des1_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, 128, M_CRYPTO_DATA, M_WAITOK);
	bzero(*sched, 128);

	if (des_set_key(key, *sched) < 0) {
		des1_zerokey(sched);
		return -1;
	}

	return 0;
}

void
des1_zerokey(u_int8_t **sched)
{
	bzero(*sched, 128);
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
des3_encrypt(caddr_t key, u_int8_t *blk)
{
	des_ecb3_encrypt(blk, blk, key, key + 128, key + 256, 1);
}

void
des3_decrypt(caddr_t key, u_int8_t *blk)
{
	des_ecb3_encrypt(blk, blk, key + 256, key + 128, key, 0);
}

int
des3_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, 384, M_CRYPTO_DATA, M_WAITOK);
	bzero(*sched, 384);

	if (des_set_key(key, *sched) < 0 || des_set_key(key + 8, *sched + 128)
	    < 0 || des_set_key(key + 16, *sched + 256) < 0) {
		des3_zerokey(sched);
		return -1;
	}

	return 0;
}

void
des3_zerokey(u_int8_t **sched)
{
	bzero(*sched, 384);
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
blf_encrypt(caddr_t key, u_int8_t *blk)
{
	blf_ecb_encrypt((blf_ctx *) key, blk, 8);
}

void
blf_decrypt(caddr_t key, u_int8_t *blk)
{
	blf_ecb_decrypt((blf_ctx *) key, blk, 8);
}

int
blf_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, sizeof(blf_ctx), M_CRYPTO_DATA, M_WAITOK);
	bzero(*sched, sizeof(blf_ctx));
	blf_key((blf_ctx *)*sched, key, len);

	return 0;
}

void
blf_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(blf_ctx));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

int
null_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	return 0;
}

void
null_zerokey(u_int8_t **sched)
{
}

void
null_encrypt(caddr_t key, u_int8_t *blk)
{
}

void
null_decrypt(caddr_t key, u_int8_t *blk)
{
}

void
cast5_encrypt(caddr_t key, u_int8_t *blk)
{
	cast_encrypt((cast_key *) key, blk, blk);
}

void
cast5_decrypt(caddr_t key, u_int8_t *blk)
{
	cast_decrypt((cast_key *) key, blk, blk);
}

int
cast5_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, sizeof(cast_key), M_CRYPTO_DATA, M_WAITOK);
	bzero(*sched, sizeof(cast_key));
	cast_setkey((cast_key *)*sched, key, len);

	return 0;
}

void
cast5_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(cast_key));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
skipjack_encrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_forwards(blk, blk, (u_int8_t **) key);
}

void
skipjack_decrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_backwards(blk, blk, (u_int8_t **) key);
}

int
skipjack_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, 10 * sizeof(u_int8_t *), M_CRYPTO_DATA,
	    M_WAITOK);
	bzero(*sched, 10 * sizeof(u_int8_t *));
	subkey_table_gen(key, (u_int8_t **) *sched);

	return 0;
}

void
skipjack_zerokey(u_int8_t **sched)
{
	int k;

	for (k = 0; k < 10; k++) {
		if (((u_int8_t **)(*sched))[k]) {
			bzero(((u_int8_t **)(*sched))[k], 0x100);
			FREE(((u_int8_t **)(*sched))[k], M_CRYPTO_DATA);
		}
	}
	bzero(*sched, 10 * sizeof(u_int8_t *));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

void
rijndael128_encrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_encrypt((rijndael_ctx *) key, (u_char *) blk, (u_char *) blk);
}

void
rijndael128_decrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_decrypt((rijndael_ctx *) key, (u_char *) blk, (u_char *) blk);
}

int
rijndael128_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	MALLOC(*sched, u_int8_t *, sizeof(rijndael_ctx), M_CRYPTO_DATA,
	    M_WAITOK);
	bzero(*sched, sizeof(rijndael_ctx));

	if (rijndael_set_key((rijndael_ctx *)*sched, (u_char *)key, len * 8)
	    < 0) {
		rijndael128_zerokey(sched);
		return -1;
	}

	return 0;
}

void
rijndael128_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(rijndael_ctx));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

/*
 * And now for auth.
 */

int
RMD160Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

int
MD5Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	MD5Update(ctx, buf, len);
	return 0;
}

int
SHA1Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA1Update(ctx, buf, len);
	return 0;
}

int
SHA256_Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA256_Update(ctx, buf, len);
	return 0;
}

int
SHA384_Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA384_Update(ctx, buf, len);
	return 0;
}

int
SHA512_Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA512_Update(ctx, buf, len);
	return 0;
}

/*
 * And compression
 */

u_int32_t
deflate_compress(u_int8_t *data, u_int32_t size, u_int8_t **out)
{
	return deflate_global(data, size, 0, out);
}

u_int32_t
deflate_decompress(u_int8_t *data, u_int32_t size, u_int8_t **out)
{
	return deflate_global(data, size, 1, out);
}

u_int32_t
lzs_dummy(u_int8_t *data, u_int32_t size, u_int8_t **out)
{
	*out = NULL;
	return (0);
}

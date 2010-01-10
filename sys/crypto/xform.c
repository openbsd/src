/*	$OpenBSD: xform.c,v 1.37 2010/01/10 12:43:07 markus Exp $	*/
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Damien Miller (djm@mindrot.org).
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
 * AES XTS implementation in 2008 by Damien Miller
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Copyright (C) 2008, Damien Miller
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
int  aes_ctr_setkey(u_int8_t **, u_int8_t *, int);
int  aes_xts_setkey(u_int8_t **, u_int8_t *, int);
int  null_setkey(u_int8_t **, u_int8_t *, int);

void des1_encrypt(caddr_t, u_int8_t *);
void des3_encrypt(caddr_t, u_int8_t *);
void blf_encrypt(caddr_t, u_int8_t *);
void cast5_encrypt(caddr_t, u_int8_t *);
void skipjack_encrypt(caddr_t, u_int8_t *);
void rijndael128_encrypt(caddr_t, u_int8_t *);
void null_encrypt(caddr_t, u_int8_t *);
void aes_xts_encrypt(caddr_t, u_int8_t *);

void des1_decrypt(caddr_t, u_int8_t *);
void des3_decrypt(caddr_t, u_int8_t *);
void blf_decrypt(caddr_t, u_int8_t *);
void cast5_decrypt(caddr_t, u_int8_t *);
void skipjack_decrypt(caddr_t, u_int8_t *);
void rijndael128_decrypt(caddr_t, u_int8_t *);
void null_decrypt(caddr_t, u_int8_t *);
void aes_xts_decrypt(caddr_t, u_int8_t *);

void aes_ctr_crypt(caddr_t, u_int8_t *);

void des1_zerokey(u_int8_t **);
void des3_zerokey(u_int8_t **);
void blf_zerokey(u_int8_t **);
void cast5_zerokey(u_int8_t **);
void skipjack_zerokey(u_int8_t **);
void rijndael128_zerokey(u_int8_t **);
void aes_ctr_zerokey(u_int8_t **);
void aes_xts_zerokey(u_int8_t **);
void null_zerokey(u_int8_t **);

void aes_ctr_reinit(caddr_t, u_int8_t *);
void aes_xts_reinit(caddr_t, u_int8_t *);

int MD5Update_int(void *, const u_int8_t *, u_int16_t);
int SHA1Update_int(void *, const u_int8_t *, u_int16_t);
int RMD160Update_int(void *, const u_int8_t *, u_int16_t);
int SHA256Update_int(void *, const u_int8_t *, u_int16_t);
int SHA384Update_int(void *, const u_int8_t *, u_int16_t);
int SHA512Update_int(void *, const u_int8_t *, u_int16_t);

u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t lzs_dummy(u_int8_t *, u_int32_t, u_int8_t **);

/* Helper */
struct aes_xts_ctx;
void aes_xts_crypt(struct aes_xts_ctx *, u_int8_t *, u_int);

/* Encryption instances */
struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	8, 8, 8, 8,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	des1_zerokey,
	NULL
};

struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	8, 8, 24, 24,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	des3_zerokey,
	NULL
};

struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	8, 8, 5, 56 /* 448 bits, max key */,
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	blf_zerokey,
	NULL
};

struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	8, 8, 5, 16,
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	cast5_zerokey,
	NULL
};

struct enc_xform enc_xform_skipjack = {
	CRYPTO_SKIPJACK_CBC, "Skipjack",
	8, 8, 10, 10,
	skipjack_encrypt,
	skipjack_decrypt,
	skipjack_setkey,
	skipjack_zerokey,
	NULL
};

struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	16, 16, 16, 32,
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	rijndael128_zerokey,
	NULL
};

struct enc_xform enc_xform_aes_ctr = {
	CRYPTO_AES_CTR, "AES-CTR",
	16, 8, 16+4, 32+4,
	aes_ctr_crypt,
	aes_ctr_crypt,
	aes_ctr_setkey,
	aes_ctr_zerokey,
	aes_ctr_reinit
};

struct enc_xform enc_xform_aes_xts = {
	CRYPTO_AES_XTS, "AES-XTS",
	16, 8, 32, 64,
	aes_xts_encrypt,
	aes_xts_decrypt,
	aes_xts_setkey,
	aes_xts_zerokey,
	aes_xts_reinit
};

struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 1, 1, 32,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

struct enc_xform enc_xform_null = {
	CRYPTO_NULL, "NULL",
	4, 0, 0, 256,
	null_encrypt,
	null_decrypt,
	null_setkey,
	null_zerokey,
	NULL
};

/* Authentication instances */
struct auth_hash auth_hash_hmac_md5_96 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, 16, 12, sizeof(MD5_CTX), HMAC_MD5_BLOCK_LEN,
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1_96 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, 20, 12, sizeof(SHA1_CTX), HMAC_SHA1_BLOCK_LEN,
	(void (*) (void *)) SHA1Init, SHA1Update_int,
	(void (*) (u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_hmac_ripemd_160_96 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, 20, 12, sizeof(RMD160_CTX), HMAC_RIPEMD160_BLOCK_LEN,
	(void (*)(void *)) RMD160Init, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_hmac_sha2_256_128 = {
	CRYPTO_SHA2_256_HMAC, "HMAC-SHA2-256",
	32, 32, 16, sizeof(SHA2_CTX), HMAC_SHA2_256_BLOCK_LEN,
	(void (*)(void *)) SHA256Init, SHA256Update_int,
	(void (*)(u_int8_t *, void *)) SHA256Final
};

struct auth_hash auth_hash_hmac_sha2_384_192 = {
	CRYPTO_SHA2_384_HMAC, "HMAC-SHA2-384",
	48, 48, 24, sizeof(SHA2_CTX), HMAC_SHA2_384_BLOCK_LEN,
	(void (*)(void *)) SHA384Init, SHA384Update_int,
	(void (*)(u_int8_t *, void *)) SHA384Final
};

struct auth_hash auth_hash_hmac_sha2_512_256 = {
	CRYPTO_SHA2_512_HMAC, "HMAC-SHA2-512",
	64, 64, 32, sizeof(SHA2_CTX), HMAC_SHA2_512_BLOCK_LEN,
	(void (*)(void *)) SHA512Init, SHA512Update_int,
	(void (*)(u_int8_t *, void *)) SHA512Final
};

struct auth_hash auth_hash_key_md5 = {
	CRYPTO_MD5_KPDK, "Keyed MD5",
	0, 16, 16, sizeof(MD5_CTX), 0,
	(void (*)(void *)) MD5Init, MD5Update_int,
	(void (*)(u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_key_sha1 = {
	CRYPTO_SHA1_KPDK, "Keyed SHA1",
	0, 20, 20, sizeof(SHA1_CTX), 0,
	(void (*)(void *)) SHA1Init, SHA1Update_int,
	(void (*)(u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_md5 = {
	CRYPTO_MD5, "MD5",
	0, 16, 16, sizeof(MD5_CTX), 0,
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_sha1 = {
	CRYPTO_SHA1, "SHA1",
	0, 20, 20, sizeof(SHA1_CTX), 0,
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
	*sched = malloc(128, M_CRYPTO_DATA, M_WAITOK | M_ZERO);

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
	free(*sched, M_CRYPTO_DATA);
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
	*sched = malloc(384, M_CRYPTO_DATA, M_WAITOK | M_ZERO);

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
	free(*sched, M_CRYPTO_DATA);
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
	*sched = malloc(sizeof(blf_ctx), M_CRYPTO_DATA, M_WAITOK | M_ZERO);
	blf_key((blf_ctx *)*sched, key, len);

	return 0;
}

void
blf_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(blf_ctx));
	free(*sched, M_CRYPTO_DATA);
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
	*sched = malloc(sizeof(cast_key), M_CRYPTO_DATA, M_WAITOK | M_ZERO);
	cast_setkey((cast_key *)*sched, key, len);

	return 0;
}

void
cast5_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(cast_key));
	free(*sched, M_CRYPTO_DATA);
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
	*sched = malloc(10 * sizeof(u_int8_t *), M_CRYPTO_DATA, M_WAITOK |
	    M_ZERO);
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
			free(((u_int8_t **)(*sched))[k], M_CRYPTO_DATA);
		}
	}
	bzero(*sched, 10 * sizeof(u_int8_t *));
	free(*sched, M_CRYPTO_DATA);
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
	*sched = malloc(sizeof(rijndael_ctx), M_CRYPTO_DATA, M_WAITOK | M_ZERO);

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
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

#define AESCTR_NONCESIZE	4
#define AESCTR_IVSIZE		8
#define AESCTR_BLOCKSIZE	16

struct aes_ctr_ctx {
	u_int32_t	ac_ek[4*(AES_MAXROUNDS + 1)];
	u_int8_t	ac_block[AESCTR_BLOCKSIZE];
	int		ac_nr;
};

void
aes_ctr_reinit(caddr_t key, u_int8_t *iv)
{
	struct aes_ctr_ctx *ctx;

	ctx = (struct aes_ctr_ctx *)key;
	bcopy(iv, ctx->ac_block + AESCTR_NONCESIZE, AESCTR_IVSIZE);

	/* reset counter */
	bzero(ctx->ac_block + AESCTR_NONCESIZE + AESCTR_IVSIZE, 4);
}

void
aes_ctr_crypt(caddr_t key, u_int8_t *data)
{
	struct aes_ctr_ctx *ctx;
	u_int8_t keystream[AESCTR_BLOCKSIZE];
	int i;

	ctx = (struct aes_ctr_ctx *)key;
	/* increment counter */
	for (i = AESCTR_BLOCKSIZE - 1;
	     i >= AESCTR_NONCESIZE + AESCTR_IVSIZE; i--)
		if (++ctx->ac_block[i])   /* continue on overflow */
			break;
	rijndaelEncrypt(ctx->ac_ek, ctx->ac_nr, ctx->ac_block, keystream);
	for (i = 0; i < AESCTR_BLOCKSIZE; i++)
		data[i] ^= keystream[i];
}

int
aes_ctr_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	struct aes_ctr_ctx *ctx;

	if (len < AESCTR_NONCESIZE)
		return -1;

	*sched = malloc(sizeof(struct aes_ctr_ctx), M_CRYPTO_DATA, M_WAITOK |
	    M_ZERO);
	ctx = (struct aes_ctr_ctx *)*sched;
	ctx->ac_nr = rijndaelKeySetupEnc(ctx->ac_ek, (u_char *)key,
	    (len - AESCTR_NONCESIZE) * 8);
	if (ctx->ac_nr == 0) {
		aes_ctr_zerokey(sched);
		return -1;
	}
	bcopy(key + len - AESCTR_NONCESIZE, ctx->ac_block, AESCTR_NONCESIZE);
	return 0;
}

void
aes_ctr_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(struct aes_ctr_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

#define AES_XTS_BLOCKSIZE	16
#define AES_XTS_IVSIZE		8
#define AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

struct aes_xts_ctx {
	rijndael_ctx key1;
	rijndael_ctx key2;
	u_int8_t tweak[AES_XTS_BLOCKSIZE];
};

void
aes_xts_reinit(caddr_t key, u_int8_t *iv)
{
	struct aes_xts_ctx *ctx = (struct aes_xts_ctx *)key;
	u_int64_t blocknum;
	u_int i;

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
	bcopy(iv, &blocknum, AES_XTS_IVSIZE);
	for (i = 0; i < AES_XTS_IVSIZE; i++) {
		ctx->tweak[i] = blocknum & 0xff;
		blocknum >>= 8;
	}
	/* Last 64 bits of IV are always zero */
	bzero(ctx->tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);

	rijndael_encrypt(&ctx->key2, ctx->tweak, ctx->tweak);
}

void
aes_xts_crypt(struct aes_xts_ctx *ctx, u_int8_t *data, u_int do_encrypt)
{
	u_int8_t block[AES_XTS_BLOCKSIZE];
	u_int i, carry_in, carry_out;

	for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
		block[i] = data[i] ^ ctx->tweak[i];

	if (do_encrypt)
		rijndael_encrypt(&ctx->key1, block, data);
	else
		rijndael_decrypt(&ctx->key1, block, data);

	for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
		data[i] ^= ctx->tweak[i];

	/* Exponentiate tweak */
	carry_in = 0;
	for (i = 0; i < AES_XTS_BLOCKSIZE; i++) {
		carry_out = ctx->tweak[i] & 0x80;
		ctx->tweak[i] = (ctx->tweak[i] << 1) | (carry_in ? 1 : 0);
		carry_in = carry_out;
	}
	if (carry_in)
		ctx->tweak[0] ^= AES_XTS_ALPHA;
	bzero(block, sizeof(block));
}

void
aes_xts_encrypt(caddr_t key, u_int8_t *data)
{
	aes_xts_crypt((struct aes_xts_ctx *)key, data, 1);
}

void
aes_xts_decrypt(caddr_t key, u_int8_t *data)
{
	aes_xts_crypt((struct aes_xts_ctx *)key, data, 0);
}

int
aes_xts_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	struct aes_xts_ctx *ctx;

	if (len != 32 && len != 64)
		return -1;

	*sched = malloc(sizeof(struct aes_xts_ctx), M_CRYPTO_DATA,
	    M_WAITOK | M_ZERO);
	ctx = (struct aes_xts_ctx *)*sched;

	rijndael_set_key(&ctx->key1, key, len * 4);
	rijndael_set_key(&ctx->key2, key + (len / 2), len * 4);

	return 0;
}

void
aes_xts_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(struct aes_xts_ctx));
	free(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}


/*
 * And now for auth.
 */

int
RMD160Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

int
MD5Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	MD5Update(ctx, buf, len);
	return 0;
}

int
SHA1Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA1Update(ctx, buf, len);
	return 0;
}

int
SHA256Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA256Update(ctx, buf, len);
	return 0;
}

int
SHA384Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA384Update(ctx, buf, len);
	return 0;
}

int
SHA512Update_int(void *ctx, const u_int8_t *buf, u_int16_t len)
{
	SHA512Update(ctx, buf, len);
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

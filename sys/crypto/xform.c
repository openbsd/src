/*	$OpenBSD: xform.c,v 1.45 2014/12/28 10:02:37 tedu Exp $	*/
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
#include <crypto/rijndael.h>
#include <crypto/cryptodev.h>
#include <crypto/xform.h>
#include <lib/libz/zlib.h>
#include <crypto/gmac.h>

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);

int  des_set_key(void *, caddr_t);
int  des1_setkey(void *, u_int8_t *, int);
int  des3_setkey(void *, u_int8_t *, int);
int  blf_setkey(void *, u_int8_t *, int);
int  cast5_setkey(void *, u_int8_t *, int);
int  rijndael128_setkey(void *, u_int8_t *, int);
int  aes_ctr_setkey(void *, u_int8_t *, int);
int  aes_xts_setkey(void *, u_int8_t *, int);
int  null_setkey(void *, u_int8_t *, int);

void des1_encrypt(caddr_t, u_int8_t *);
void des3_encrypt(caddr_t, u_int8_t *);
void blf_encrypt(caddr_t, u_int8_t *);
void cast5_encrypt(caddr_t, u_int8_t *);
void rijndael128_encrypt(caddr_t, u_int8_t *);
void null_encrypt(caddr_t, u_int8_t *);
void aes_xts_encrypt(caddr_t, u_int8_t *);

void des1_decrypt(caddr_t, u_int8_t *);
void des3_decrypt(caddr_t, u_int8_t *);
void blf_decrypt(caddr_t, u_int8_t *);
void cast5_decrypt(caddr_t, u_int8_t *);
void rijndael128_decrypt(caddr_t, u_int8_t *);
void null_decrypt(caddr_t, u_int8_t *);
void aes_xts_decrypt(caddr_t, u_int8_t *);

void aes_ctr_crypt(caddr_t, u_int8_t *);

void aes_ctr_reinit(caddr_t, u_int8_t *);
void aes_xts_reinit(caddr_t, u_int8_t *);
void aes_gcm_reinit(caddr_t, u_int8_t *);

int MD5Update_int(void *, const u_int8_t *, u_int16_t);
int SHA1Update_int(void *, const u_int8_t *, u_int16_t);
int RMD160Update_int(void *, const u_int8_t *, u_int16_t);
int SHA256Update_int(void *, const u_int8_t *, u_int16_t);
int SHA384Update_int(void *, const u_int8_t *, u_int16_t);
int SHA512Update_int(void *, const u_int8_t *, u_int16_t);

u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **);
u_int32_t lzs_dummy(u_int8_t *, u_int32_t, u_int8_t **);

#define AESCTR_NONCESIZE	4
#define AESCTR_IVSIZE		8
#define AESCTR_BLOCKSIZE	16

struct aes_ctr_ctx {
	u_int32_t	ac_ek[4*(AES_MAXROUNDS + 1)];
	u_int8_t	ac_block[AESCTR_BLOCKSIZE];
	int		ac_nr;
};

#define AES_XTS_BLOCKSIZE	16
#define AES_XTS_IVSIZE		8
#define AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

struct aes_xts_ctx {
	rijndael_ctx key1;
	rijndael_ctx key2;
	u_int8_t tweak[AES_XTS_BLOCKSIZE];
};

/* Helper */
void aes_xts_crypt(struct aes_xts_ctx *, u_int8_t *, u_int);

/* Encryption instances */
struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	8, 8, 8, 8, 128,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	NULL
};

struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	8, 8, 24, 24, 384,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	NULL
};

struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	8, 8, 5, 56 /* 448 bits, max key */,
	sizeof(blf_ctx),
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	NULL
};

struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	8, 8, 5, 16,
	sizeof(cast_key),
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	NULL
};

struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	16, 16, 16, 32,
	sizeof(rijndael_ctx),
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	NULL
};

struct enc_xform enc_xform_aes_ctr = {
	CRYPTO_AES_CTR, "AES-CTR",
	16, 8, 16+4, 32+4,
	sizeof(struct aes_ctr_ctx),
	aes_ctr_crypt,
	aes_ctr_crypt,
	aes_ctr_setkey,
	aes_ctr_reinit
};

struct enc_xform enc_xform_aes_gcm = {
	CRYPTO_AES_GCM_16, "AES-GCM",
	1, 8, 16+4, 32+4,
	sizeof(struct aes_ctr_ctx),
	aes_ctr_crypt,
	aes_ctr_crypt,
	aes_ctr_setkey,
	aes_gcm_reinit
};

struct enc_xform enc_xform_aes_gmac = {
	CRYPTO_AES_GMAC, "AES-GMAC",
	1, 8, 16+4, 32+4, 0,
	NULL,
	NULL,
	NULL,
	NULL
};

struct enc_xform enc_xform_aes_xts = {
	CRYPTO_AES_XTS, "AES-XTS",
	16, 8, 32, 64,
	sizeof(struct aes_xts_ctx),
	aes_xts_encrypt,
	aes_xts_decrypt,
	aes_xts_setkey,
	aes_xts_reinit
};

struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 1, 1, 32, 0,
	NULL,
	NULL,
	NULL,
	NULL
};

struct enc_xform enc_xform_null = {
	CRYPTO_NULL, "NULL",
	4, 0, 0, 256, 0,
	null_encrypt,
	null_decrypt,
	null_setkey,
	NULL
};

/* Authentication instances */
struct auth_hash auth_hash_hmac_md5_96 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, 16, 12, sizeof(MD5_CTX), HMAC_MD5_BLOCK_LEN,
	(void (*) (void *)) MD5Init, NULL, NULL,
	MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1_96 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, 20, 12, sizeof(SHA1_CTX), HMAC_SHA1_BLOCK_LEN,
	(void (*) (void *)) SHA1Init, NULL, NULL,
	SHA1Update_int,
	(void (*) (u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_hmac_ripemd_160_96 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, 20, 12, sizeof(RMD160_CTX), HMAC_RIPEMD160_BLOCK_LEN,
	(void (*)(void *)) RMD160Init, NULL, NULL,
	RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_hmac_sha2_256_128 = {
	CRYPTO_SHA2_256_HMAC, "HMAC-SHA2-256",
	32, 32, 16, sizeof(SHA2_CTX), HMAC_SHA2_256_BLOCK_LEN,
	(void (*)(void *)) SHA256Init, NULL, NULL,
	SHA256Update_int,
	(void (*)(u_int8_t *, void *)) SHA256Final
};

struct auth_hash auth_hash_hmac_sha2_384_192 = {
	CRYPTO_SHA2_384_HMAC, "HMAC-SHA2-384",
	48, 48, 24, sizeof(SHA2_CTX), HMAC_SHA2_384_BLOCK_LEN,
	(void (*)(void *)) SHA384Init, NULL, NULL,
	SHA384Update_int,
	(void (*)(u_int8_t *, void *)) SHA384Final
};

struct auth_hash auth_hash_hmac_sha2_512_256 = {
	CRYPTO_SHA2_512_HMAC, "HMAC-SHA2-512",
	64, 64, 32, sizeof(SHA2_CTX), HMAC_SHA2_512_BLOCK_LEN,
	(void (*)(void *)) SHA512Init, NULL, NULL,
	SHA512Update_int,
	(void (*)(u_int8_t *, void *)) SHA512Final
};

struct auth_hash auth_hash_gmac_aes_128 = {
	CRYPTO_AES_128_GMAC, "GMAC-AES-128",
	16+4, 16, 16, sizeof(AES_GMAC_CTX), GMAC_BLOCK_LEN,
	(void (*)(void *)) AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Update,
	(void (*)(u_int8_t *, void *)) AES_GMAC_Final
};

struct auth_hash auth_hash_gmac_aes_192 = {
	CRYPTO_AES_192_GMAC, "GMAC-AES-192",
	24+4, 16, 16, sizeof(AES_GMAC_CTX), GMAC_BLOCK_LEN,
	(void (*)(void *)) AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Update,
	(void (*)(u_int8_t *, void *)) AES_GMAC_Final
};

struct auth_hash auth_hash_gmac_aes_256 = {
	CRYPTO_AES_256_GMAC, "GMAC-AES-256",
	32+4, 16, 16, sizeof(AES_GMAC_CTX), GMAC_BLOCK_LEN,
	(void (*)(void *)) AES_GMAC_Init,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Setkey,
	(void (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Reinit,
	(int  (*)(void *, const u_int8_t *, u_int16_t)) AES_GMAC_Update,
	(void (*)(u_int8_t *, void *)) AES_GMAC_Final
};

struct auth_hash auth_hash_md5 = {
	CRYPTO_MD5, "MD5",
	0, 16, 16, sizeof(MD5_CTX), 0,
	(void (*) (void *)) MD5Init, NULL, NULL,
	MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_sha1 = {
	CRYPTO_SHA1, "SHA1",
	0, 20, 20, sizeof(SHA1_CTX), 0,
	(void (*)(void *)) SHA1Init, NULL, NULL,
	SHA1Update_int,
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
des1_setkey(void *sched, u_int8_t *key, int len)
{
	return des_set_key(key, sched);
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
des3_setkey(void *sched, u_int8_t *key, int len)
{
	if (des_set_key(key, sched) < 0 || des_set_key(key + 8, sched + 128)
	    < 0 || des_set_key(key + 16, sched + 256) < 0)
		return -1;

	return 0;
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
blf_setkey(void *sched, u_int8_t *key, int len)
{
	blf_key((blf_ctx *)sched, key, len);

	return 0;
}

int
null_setkey(void *sched, u_int8_t *key, int len)
{
	return 0;
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
cast5_setkey(void *sched, u_int8_t *key, int len)
{
	cast_setkey((cast_key *)sched, key, len);

	return 0;
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
rijndael128_setkey(void *sched, u_int8_t *key, int len)
{
	return rijndael_set_key((rijndael_ctx *)sched, (u_char *)key, len * 8);
}

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
aes_gcm_reinit(caddr_t key, u_int8_t *iv)
{
	struct aes_ctr_ctx *ctx;

	ctx = (struct aes_ctr_ctx *)key;
	bcopy(iv, ctx->ac_block + AESCTR_NONCESIZE, AESCTR_IVSIZE);

	/* reset counter */
	bzero(ctx->ac_block + AESCTR_NONCESIZE + AESCTR_IVSIZE, 4);
	ctx->ac_block[AESCTR_BLOCKSIZE - 1] = 1; /* GCM starts with 1 */
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
	explicit_bzero(keystream, sizeof(keystream));
}

int
aes_ctr_setkey(void *sched, u_int8_t *key, int len)
{
	struct aes_ctr_ctx *ctx;

	if (len < AESCTR_NONCESIZE)
		return -1;

	ctx = (struct aes_ctr_ctx *)sched;
	ctx->ac_nr = rijndaelKeySetupEnc(ctx->ac_ek, (u_char *)key,
	    (len - AESCTR_NONCESIZE) * 8);
	if (ctx->ac_nr == 0)
		return -1;
	bcopy(key + len - AESCTR_NONCESIZE, ctx->ac_block, AESCTR_NONCESIZE);
	return 0;
}

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
	explicit_bzero(block, sizeof(block));
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
aes_xts_setkey(void *sched, u_int8_t *key, int len)
{
	struct aes_xts_ctx *ctx;

	if (len != 32 && len != 64)
		return -1;

	ctx = (struct aes_xts_ctx *)sched;

	rijndael_set_key(&ctx->key1, key, len * 4);
	rijndael_set_key(&ctx->key2, key + (len / 2), len * 4);

	return 0;
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


u_int32_t deflate_global(u_int8_t *, u_int32_t, int, u_int8_t **);

struct deflate_buf {
        u_int8_t *out;
        u_int32_t size;
        int flag;
};

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

/*	$OpenBSD: ip_xform.c,v 1.5 2000/01/27 08:09:12 angelos Exp $	*/

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
 * Permission to use, copy, and modify this software without fee
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

/*
 * Encapsulation Security Payload Processing
 * Per RFC1827 (Atkinson, 1995)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>
#include <net/pfkeyv2.h>
#include <net/if_enc.h>

extern void des_ecb3_encrypt(caddr_t, caddr_t, caddr_t, caddr_t, caddr_t, int);
extern void des_ecb_encrypt(caddr_t, caddr_t, caddr_t, int);
extern void des_set_key(caddr_t, caddr_t);

static void des1_encrypt(struct tdb *, u_int8_t *);
static void des3_encrypt(struct tdb *, u_int8_t *);
static void blf_encrypt(struct tdb *, u_int8_t *);
static void cast5_encrypt(struct tdb *, u_int8_t *);
static void skipjack_encrypt(struct tdb *, u_int8_t *);
static void des1_decrypt(struct tdb *, u_int8_t *);
static void des3_decrypt(struct tdb *, u_int8_t *);
static void blf_decrypt(struct tdb *, u_int8_t *);
static void cast5_decrypt(struct tdb *, u_int8_t *);
static void skipjack_decrypt(struct tdb *, u_int8_t *);

static void
des1_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb_encrypt(blk, blk, tdb->tdb_key, 1);
}

static void
des1_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb_encrypt(blk, blk, tdb->tdb_key, 0);
}

static void
des1_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
    MALLOC(*sched, u_int8_t *, 128, M_XDATA, M_WAITOK);
    bzero(*sched, 128);
    des_set_key(key, *sched);
}

static void
des1_zerokey(u_int8_t **sched)
{
    bzero(*sched, 128);
    FREE(*sched, M_XDATA);
    *sched = NULL;
}

struct enc_xform enc_xform_des = {
    SADB_EALG_DESCBC, "Data Encryption Standard (DES)",
    ESP_DES_BLKS, ESP_DES_IVS,
    8, 8, 8,
    des1_encrypt,
    des1_decrypt,
    des1_setkey,
    des1_zerokey,
};

static void
des3_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb3_encrypt(blk, blk, tdb->tdb_key, tdb->tdb_key + 128,
		     tdb->tdb_key + 256, 1);
}

static void
des3_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    des_ecb3_encrypt(blk, blk, tdb->tdb_key + 256, tdb->tdb_key + 128,
		     tdb->tdb_key, 0);
}

static void
des3_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
    MALLOC(*sched, u_int8_t *, 384, M_XDATA, M_WAITOK);
    bzero(*sched, 384);
    des_set_key(key, *sched);
    des_set_key(key + 8, *sched + 128);
    des_set_key(key + 16, *sched + 256);
}

static void
des3_zerokey(u_int8_t **sched)
{
    bzero(*sched, 384);
    FREE(*sched, M_XDATA);
    *sched = NULL;
}

struct enc_xform enc_xform_3des = {
    SADB_EALG_3DESCBC, "Triple DES (3DES)",
    ESP_3DES_BLKS, ESP_3DES_IVS,
    24, 24, 8,
    des3_encrypt,
    des3_decrypt,
    des3_setkey,
    des3_zerokey
};

static void
blf_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    blf_ecb_encrypt((blf_ctx *) tdb->tdb_key, blk, 8);
}

static void
blf_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    blf_ecb_decrypt((blf_ctx *) tdb->tdb_key, blk, 8);
}

static void
blf_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
    MALLOC(*sched, u_int8_t *, sizeof(blf_ctx), M_XDATA, M_WAITOK);
    bzero(*sched, sizeof(blf_ctx));
    blf_key((blf_ctx *)*sched, key, len);
}

static void
blf_zerokey(u_int8_t **sched)
{
    bzero(*sched, sizeof(blf_ctx));
    FREE(*sched, M_XDATA);
    *sched = NULL;
}

struct enc_xform enc_xform_blf = {
    SADB_X_EALG_BLF, "Blowfish",
    ESP_BLF_BLKS, ESP_BLF_IVS,
    5, BLF_MAXKEYLEN, 8,
    blf_encrypt,
    blf_decrypt,
    blf_setkey,
    blf_zerokey
};

static void
cast5_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    cast_encrypt((cast_key *) tdb->tdb_key, blk, blk);
}

static void
cast5_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    cast_decrypt((cast_key *) tdb->tdb_key, blk, blk);
}

static void
cast5_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
    MALLOC(*sched, u_int8_t *, sizeof(blf_ctx), M_XDATA, M_WAITOK);
    bzero(*sched, sizeof(blf_ctx));
    cast_setkey((cast_key *)*sched, key, len);
}

static void
cast5_zerokey(u_int8_t **sched)
{
    bzero(*sched, sizeof(cast_key));
    FREE(*sched, M_XDATA);
    *sched = NULL;
}

struct enc_xform enc_xform_cast5 = {
    SADB_X_EALG_CAST, "CAST",
    ESP_CAST_BLKS, ESP_CAST_IVS,
    5, 16, 8,
    cast5_encrypt,
    cast5_decrypt,
    cast5_setkey,
    cast5_zerokey
};

static void
skipjack_encrypt(struct tdb *tdb, u_int8_t *blk)
{
    skipjack_forwards(blk, blk, (u_int8_t **) tdb->tdb_key);
}

static void
skipjack_decrypt(struct tdb *tdb, u_int8_t *blk)
{
    skipjack_backwards(blk, blk, (u_int8_t **) tdb->tdb_key);
}

static void
skipjack_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
    MALLOC(*sched, u_int8_t *, 10 * sizeof(u_int8_t *), M_XDATA, M_WAITOK);
    bzero(*sched, 10 * sizeof(u_int8_t *));
    subkey_table_gen(key, (u_int8_t **) *sched);
}

static void
skipjack_zerokey(u_int8_t **sched)
{
    int k;

    for (k = 0; k < 10; k++)
	if (((u_int8_t **)(*sched))[k])
	{
	    bzero(((u_int8_t **)(*sched))[k], 0x100);
	    FREE(((u_int8_t **)(*sched))[k], M_XDATA);
	}
    bzero(*sched, sizeof(cast_key));
    FREE(*sched, M_XDATA);
    *sched = NULL;
}

struct enc_xform enc_xform_skipjack = {
    SADB_X_EALG_SKIPJACK, "Skipjack",
    ESP_SKIPJACK_BLKS, ESP_SKIPJACK_IVS,
    10, 10, 8,
    skipjack_encrypt,
    skipjack_decrypt,
    skipjack_setkey,
    skipjack_zerokey
};

/*
 * And now for auth
 */

struct auth_hash auth_hash_hmac_md5_96 = {
    SADB_AALG_MD5HMAC96, "HMAC-MD5-96",
    MD5HMAC96_KEYSIZE, AH_MD5_ALEN, AH_HMAC_HASHLEN,
    sizeof(MD5_CTX),
    (void (*) (void *)) MD5Init,
    (void (*) (void *, u_int8_t *, u_int16_t)) MD5Update,
    (void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1_96 = {
    SADB_AALG_SHA1HMAC96, "HMAC-SHA1-96",
    SHA1HMAC96_KEYSIZE, AH_SHA1_ALEN, AH_HMAC_HASHLEN,
    sizeof(SHA1_CTX),
    (void (*) (void *)) SHA1Init,
    (void (*) (void *, u_int8_t *, u_int16_t)) SHA1Update,
    (void (*) (u_int8_t *, void *)) SHA1Final
};

struct auth_hash auth_hash_hmac_ripemd_160_96 = {
    SADB_X_AALG_RIPEMD160HMAC96, "HMAC-RIPEMD-160-96",
    RIPEMD160HMAC96_KEYSIZE, AH_RMD160_ALEN, AH_HMAC_HASHLEN,
    sizeof(RMD160_CTX),
    (void (*)(void *)) RMD160Init,
    (void (*)(void *, u_int8_t *, u_int16_t)) RMD160Update,
    (void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_key_md5 = {
    SADB_X_AALG_MD5, "Keyed MD5", 
    0, AH_MD5_ALEN, AH_MD5_ALEN,
    sizeof(MD5_CTX),
    (void (*)(void *))MD5Init, 
    (void (*)(void *, u_int8_t *, u_int16_t))MD5Update, 
    (void (*)(u_int8_t *, void *))MD5Final 
};

struct auth_hash auth_hash_key_sha1 = {
    SADB_X_AALG_SHA1, "Keyed SHA1",
    0, AH_SHA1_ALEN, AH_SHA1_ALEN,
    sizeof(SHA1_CTX),
    (void (*)(void *))SHA1Init, 
    (void (*)(void *, u_int8_t *, u_int16_t))SHA1Update, 
    (void (*)(u_int8_t *, void *))SHA1Final 
};

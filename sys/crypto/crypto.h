/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _CRYPTO_CRYPTO_H_
#define _CRYPTO_CRYPTO_H_

/* Some initial values */
#define CRYPTO_DRIVERS_INITIAL  4
#define CRYPTO_SW_SESSIONS      32

#ifndef CRYPTO_MAX_CACHED
#define CRYPTO_MAX_CACHED	128
#endif

/* HMAC values */
#define HMAC_BLOCK_LEN		64
#define HMAC_IPAD_VAL           0x36
#define HMAC_OPAD_VAL           0x5C

/* Encryption algorithm block sizes */
#define DES_BLOCK_LEN           8
#define DES3_BLOCK_LEN          8
#define BLOWFISH_BLOCK_LEN      8
#define SKIPJACK_BLOCK_LEN      8
#define CAST128_BLOCK_LEN       8
#define EALG_MAX_BLOCK_LEN      8  /* Keep this updated */

/* Maximum hash algorithm result length */
#define AALG_MAX_RESULT_LEN     20 /* Keep this updated */

#define CRYPTO_DES_CBC          1
#define CRYPTO_3DES_CBC         2
#define CRYPTO_BLF_CBC          3
#define CRYPTO_CAST_CBC         4
#define CRYPTO_SKIPJACK_CBC     5
#define CRYPTO_MD5_HMAC96       6
#define CRYPTO_SHA1_HMAC96      7
#define CRYPTO_RIPEMD160_HMAC96 8
#define CRYPTO_MD5_KPDK         9
#define CRYPTO_SHA1_KPDK        10

#define CRYPTO_ALGORITHM_MAX    10 /* Keep this updated */

/* Standard initialization structure beginning */
struct cryptoini
{ 
    int                cri_alg;     /* Algorithm to use */
    int                cri_klen;    /* Key length, in bits */
    int                cri_rnd;     /* Algorithm rounds, where relevant */
    caddr_t            cri_key;     /* key to use */
    struct cryptoini  *cri_next;
};

/* Describe boundaries of a single crypto operation */
struct cryptodesc
{
    int                crd_skip;   /* How many bytes to ignore from start */
    int                crd_len;    /* How many bytes to process */
    int                crd_inject; /* Where to inject results, if applicable */
    int                crd_flags;

#define CRD_F_ENCRYPT             0x1 /* Set when doing encryption */
#define CRD_F_HALFIV              0x2
#define CRD_F_IV_PRESENT          0x4 /* Used/sensible only when encrypting */

    struct cryptoini   CRD_INI;    /* Initialization/context data */
#define crd_key  CRD_INI.cri_key
#define crd_rnd  CRD_INI.cri_rnd
#define crd_alg  CRD_INI.cri_alg
#define crd_klen CRD_INI.cri_klen

    struct cryptodesc *crd_next;
};

/* Structure describing complete operation */
struct cryptop
{
    u_int64_t          crp_sid;   /* Session ID */
    int                crp_ilen;  /* Input data total length */
    int                crp_olen;  /* Result total length (unused for now) */
    int                crp_alloctype; /* Type of buf to allocate if needed */

    int                crp_etype; /* Error type (zero means no error).
				   * All error codes except EAGAIN
				   * indicate possible data corruption (as in,
				   * the data have been touched). On all
				   * errors, the crp_sid may have changed
				   * (reset to a new one), so the caller
				   * should always check and use the new
				   * value on future requests.
				   */
    int                crp_flags;

#define CRYPTO_F_IMBUF 0x0001    /* Input is an mbuf chain, otherwise contig */

    caddr_t            crp_buf;   /* Data to be processed */

    caddr_t            crp_opaque1;/* Opaque pointer, passed along */
    caddr_t            crp_opaque2;/* Opaque pointer, passed along */
    caddr_t            crp_opaque3;/* Opaque pointer, passed along */
    caddr_t            crp_opaque4;/* Opaque pointer, passed along */

    struct cryptodesc *crp_desc;  /* Linked list of processing descriptors */

    int (*crp_callback) (struct cryptop *); /* Callback function */

    struct cryptop    *crp_next;
};

#define CRYPTO_BUF_CONTIG     0x1
#define CRYPTO_BUF_MBUF       0x2

#define CRYPTO_OP_DECRYPT     0x0
#define CRYPTO_OP_ENCRYPT     0x1

/* Crypto capabilities structure */
struct cryptocap
{
    u_int32_t         cc_sessions;

    u_int8_t          cc_alg[CRYPTO_ALGORITHM_MAX + 1]; /* Supported */
    u_int8_t          cc_flags;
#define CRYPTOCAP_F_CLEANUP   0x1

    int             (*cc_newsession) (u_int32_t *, struct cryptoini *);
    int             (*cc_process) (struct cryptop *);
    int             (*cc_freesession) (u_int32_t);
};


#ifdef _KERNEL
extern int crypto_newsession(u_int64_t *, struct cryptoini *);
extern int crypto_freesession(u_int64_t);
extern int crypto_dispatch(struct cryptop *);
extern int crypto_register(u_int32_t, int, void *, void *, void *);
extern int crypto_unregister(u_int32_t, int);
extern int32_t crypto_get_driverid(void);

extern struct cryptop *crypto_getreq(int);
extern void crypto_freereq(struct cryptop *);
#endif /* _KERNEL */
#endif /* _CRYPTO_CRYPTO_H_ */

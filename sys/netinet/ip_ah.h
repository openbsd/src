/*	$OpenBSD: ip_ah.h,v 1.10 1997/11/04 09:10:59 provos Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Additional transforms and features in 1997 by Angelos D. Keromytis and
 * Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * Authentication Header Processing
 * Per RFC1826 (Atkinson, 1995)
 */

#include <sys/md5k.h>
#include <netinet/ip_sha1.h>

struct ah_hash {
    int type;
    char *name;
    u_int16_t hashsize; 
    u_int16_t ctxsize;
    void (*Init)(void *);
    void (*Update)(void *, u_int8_t *, u_int16_t);
    void (*Final)(u_int8_t *, void *);
};

struct ah_old
{
    u_int8_t	ah_nh;			/* Next header (protocol) */
    u_int8_t	ah_hl;			/* AH length, in 32-bit words */
    u_int16_t	ah_rv;			/* reserved, must be 0 */
    u_int32_t	ah_spi;			/* Security Parameters Index */
    u_int8_t	ah_data[1];		/* More, really */
};

#define AH_OLD_FLENGTH		8	/* size of fixed part */

/* Authenticator lengths */
#define AH_MD5_ALEN		16
#define AH_SHA1_ALEN		20

#define AH_ALEN_MAX		AH_SHA1_ALEN 	/* Keep this updated */

struct ahstat
{
    u_int32_t	ahs_hdrops;	/* packet shorter than header shows */
    u_int32_t	ahs_notdb;
    u_int32_t	ahs_badkcr;
    u_int32_t	ahs_badauth;
    u_int32_t	ahs_noxform;
    u_int32_t	ahs_qfull;
    u_int32_t   ahs_wrap;
    u_int32_t   ahs_replay;
    u_int32_t	ahs_badauthl;	/* bad authenticator length */
    u_int32_t	ahs_input;	/* Input AH packets */
    u_int32_t	ahs_output;	/* Output AH packets */
    u_int32_t   ahs_invalid;    /* Trying to use an invalid TDB */
    u_int64_t	ahs_ibytes;	/* input bytes */
    u_int64_t   ahs_obytes;	/* output bytes */
};

#define AH_HMAC_HASHLEN		12	/* 96 bits of authenticator */
#define AH_HMAC_RPLENGTH        4	/* 32 bits of replay counter */
#define AH_HMAC_INITIAL_RPL	1	/* Replay counter initial value */

#define HMAC_IPAD_VAL           0x36
#define HMAC_OPAD_VAL           0x5C
#define HMAC_BLOCK_LEN		64

struct ah_new
{
    u_int8_t        ah_nh;                  /* Next header (protocol) */
    u_int8_t        ah_hl;                  /* AH length, in 32-bit words */
    u_int16_t       ah_rv;                  /* reserved, must be 0 */
    u_int32_t       ah_spi;                 /* Security Parameters Index */
    u_int32_t       ah_rpl;                 /* Replay prevention */
    u_int8_t        ah_data[AH_HMAC_HASHLEN];/* Authenticator */
};

#define AH_NEW_FLENGTH		(sizeof(struct ah_new))

struct ah_new_xencap
{
    u_int32_t       amx_hash_algorithm;
    int32_t         amx_wnd;
    u_int32_t       amx_keylen;
    u_int8_t        amx_key[1];
};

#define AH_NEW_XENCAP_LEN	(3 * sizeof(u_int32_t))

struct ah_new_xdata
{
    u_int32_t       amx_hash_algorithm;
    int32_t         amx_wnd;
    u_int32_t       amx_rpl;                /* Replay counter */
    u_int32_t       amx_bitmap;
    struct ah_hash  *amx_hash;
    union
    {
        MD5_CTX         amx_MD5_ictx;       /* Internal key+padding */
        SHA1_CTX	amx_SHA1_ictx;
    } amx_ictx;
    union 
    {
        MD5_CTX         amx_MD5_octx;       /* External key+padding */
	SHA1_CTX        amx_SHA1_octx;
    } amx_octx;
};

#define amx_md5_ictx	amx_ictx.amx_MD5_ictx
#define amx_md5_octx	amx_octx.amx_MD5_octx
#define amx_sha1_ictx	amx_ictx.amx_SHA1_ictx
#define amx_sha1_octx	amx_octx.amx_SHA1_octx

#define AHMD5_ALEN      16		/* Size of MD5 digest */
#define AHSHA1_ALEN     20		/* Size of SHA-1 digest */

struct ah_old_xdata
{
    u_int32_t       amx_hash_algorithm;
    u_int32_t       amx_keylen;             /* Key material length */
    struct ah_hash  *amx_hash;
    union
    {
	MD5_CTX	    amx_MD5_ctx;
	SHA1_CTX    amx_SHA1_ctx;
    } amx_ctx;
    u_int8_t        amx_key[1];             /* Key material */
};

#define amx_md5_ctx	amx_ctx.amx_MD5_ctx
#define amx_sha1_ctx 	amx_ctx.amx_SHA1_ctx

struct ah_old_xencap
{
    u_int32_t       amx_hash_algorithm;
    u_int32_t       amx_keylen;
    u_int8_t        amx_key[1];
};

#define AH_OLD_XENCAP_LEN	(2 * sizeof(u_int32_t))

#define AH_HMAC_IPAD_VAL	0x36
#define AH_HMAC_OPAD_VAL	0x5C

#ifdef _KERNEL
struct ahstat ahstat;
#endif

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
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

#include <netinet/ip_md5.h>
#include <netinet/ip_sha1.h>

struct ah
{
	u_int8_t	ah_nh;			/* Next header (protocol) */
	u_int8_t	ah_hl;			/* AH length, in 32-bit words */
	u_int16_t	ah_rv;			/* reserved, must be 0 */
	u_int32_t	ah_spi;			/* Security Parameters Index */
	u_int8_t	ah_data[1];		/* More, really*/
};

#define AH_FLENGTH	8		/* size of fixed part */

struct ahstat
{
	u_int32_t	ahs_hdrops;	/* packet shorter than header shows */
	u_int32_t	ahs_notdb;
	u_int32_t	ahs_badkcr;
	u_int32_t	ahs_badauth;
	u_int32_t	ahs_noxform;
	u_int32_t	ahs_qfull;
        u_int32_t       ahs_wrap;
        u_int32_t       ahs_replay;
	u_int32_t	ahs_badauthl;	/* bad authenticator length */
};

#define AHHMACMD5_KMAX  64              /* max 512 bits key */
#define AHHMACMD5_AMAX  64              /* up to 512 bits of authenticator */
#define AHHMACMD5_RPLS  2               /* 64 bits of replay counter */

#define HMACMD5_HASHLEN         16
#define HMACMD5_RPLENGTH        8

#define HMACMD5_IPAD_VAL        0x36
#define HMACMD5_OPAD_VAL        0x5C

#define AHHMACMD5_KMAX  64              /* max 512 bits key */
#define AHHMACMD5_AMAX  64              /* up to 512 bits of authenticator */
#define AHHMACMD5_RPLS  2               /* 64 bits of replay counter */

#define HMACMD5_HASHLEN         16
#define HMACMD5_RPLENGTH        8

#define HMACMD5_IPAD_VAL        0x36
#define HMACMD5_OPAD_VAL        0x5C

struct ahhmacmd5
{
        u_int8_t        ah_nh;                  /* Next header (protocol) */
        u_int8_t        ah_hl;                 /* AH length, in 32-bit words */
        u_int16_t       ah_rv;                  /* reserved, must be 0 */
        u_int32_t       ah_spi;                 /* Security Parameters Index */
        u_int64_t       ah_rpl;                 /* Replay prevention */
        u_int8_t        ah_data[AHHMACMD5_AMAX];/*  Authenticator */
};

struct ahhmacmd5_xencap
{
        u_int16_t       amx_alen;
        u_int16_t       amx_rpl;
        int32_t         amx_wnd;
        u_int8_t        amx_key[AHHMACMD5_KMAX];
};

struct ahhmacmd5_xdata
{
        u_int32_t       amx_alen;               /* authenticator length */
        int32_t         amx_wnd;
        u_int64_t       amx_rpl;                /* Replay counter */
        u_int64_t       amx_bitmap;
        MD5_CTX         amx_ictx;               /* Internal key+padding */
        MD5_CTX         amx_octx;               /* External key+padding */
};

#define AHHMACSHA1_KMAX 64              /* max 512 bits key */
#define AHHMACSHA1_AMAX 64              /* up to 512 bits of authenticator */
#define AHHMACSHA1_RPLS 2               /* 64 bits of replay counter */

#define HMACSHA1_HASHLEN                20
#define HMACSHA1_RPLENGTH       8

#define HMACSHA1_IPAD_VAL       0x36
#define HMACSHA1_OPAD_VAL       0x5C

struct ahhmacsha1
{
        u_int8_t        ah_nh;                  /* Next header (protocol) */
        u_int8_t        ah_hl;                 /* AH length, in 32-bit words */
        u_int16_t       ah_rv;                  /* reserved, must be 0 */
        u_int32_t       ah_spi;                 /* Security Parameters Index */
        u_int64_t       ah_rpl;                 /* Replay prevention */
        u_int8_t        ah_data[AHHMACSHA1_AMAX];/*  Authenticator */
};

struct ahhmacsha1_xencap
{
        u_int32_t       amx_alen;
        int32_t         amx_wnd;
        u_int8_t        amx_key[AHHMACSHA1_KMAX];
};

struct ahhmacsha1_xdata
{
        u_int32_t       amx_alen;               /* authenticator length */
        int32_t         amx_wnd;
        u_int64_t       amx_rpl;                /* Replay counter */
        u_int64_t       amx_bitmap;
        SHA1_CTX        amx_ictx;               /* Internal key+padding */
        SHA1_CTX        amx_octx;               /* External key+padding */
};

#define AHMD5_KMAX      32              /* max 256 bits key */
#define AHMD5_AMAX      64              /* up to 512 bits of authenticator */

struct ahmd5
{
        u_int8_t        ah_nh;                  /* Next header (protocol) */
        u_int8_t        ah_hl;          /* AH length, in 32-bit words */
        u_int16_t       ah_rv;                  /* reserved, must be 0 */
        u_int32_t       ah_spi;                 /* Security Parameters Index */
        u_int8_t        ah_data[AHMD5_AMAX];    /*  */
};

struct ahmd5_xdata
{
        u_int16_t       amx_klen;               /* Key material length */
        u_int16_t       amx_alen;               /* authenticator length */
        u_int8_t        amx_key[AHMD5_KMAX];    /* Key material */
};

#ifdef _KERNEL
struct ahstat ahstat;
#endif

/*	$OpenBSD: ip_esp.h,v 1.9 1997/07/14 08:48:46 provos Exp $	*/

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
 * Encapsulation Security Payload Processing
 * Per RFC1827 (Atkinson, 1995)
 */

#ifndef _MD5_H_
#include <sys/md5k.h>
#endif

#include <netinet/ip_sha1.h>

/* IV lengths */
#define ESP_DES_IVS		8
#define ESP_3DES_IVS		8

#define ESP_MAX_IVS		ESP_3DES_IVS

/* Block sizes -- it is assumed that they're powers of 2 */
#define ESP_DES_BLKS		8
#define ESP_3DES_BLKS		8

/* Various defines for the "new" ESP */
#define ESP_NEW_ALEN		12	/* 96bits authenticator */
#define ESP_NEW_IPAD_VAL	0x36
#define	ESP_NEW_OPAD_VAL	0x5C

struct esp_old
{
    u_int32_t	esp_spi;	/* Security Parameters Index */
    u_int8_t	esp_iv[8];	/* iv[4] may actually be data! */
};

struct espstat
{
    u_int32_t	esps_hdrops;	/* packet shorter than header shows */
    u_int32_t	esps_notdb;
    u_int32_t	esps_badkcr;
    u_int32_t	esps_qfull;
    u_int32_t	esps_noxform;
    u_int32_t	esps_badilen;
    u_int32_t   esps_wrap;	/* Replay counter wrapped around */
    u_int32_t	esps_badauth;	/* Only valid for transforms with auth */
    u_int32_t   esps_replay;	/* Possible packet replay detected */
    u_int32_t	esps_input;	/* Input ESP packets */
    u_int32_t 	esps_output;	/* Output ESP packets */
    u_int32_t	esps_invalid;   /* Trying to use an invalid TDB */
    u_int64_t	esps_ibytes;	/* input bytes */
    u_int64_t   esps_obytes;	/* output bytes */
};

struct esp_old_xdata
{
    u_int32_t   edx_enc_algorithm;
    int32_t     edx_ivlen;      /* 4 or 8 */
    union
    {
	u_int8_t  Iv[ESP_3DES_IVS]; /* that's enough space */
	u_int32_t Ivl;      	/* make sure this is 4 bytes */
	u_int64_t Ivq; 		/* make sure this is 8 bytes! */
    }Iu;
#define edx_iv  Iu.Iv
#define edx_ivl Iu.Ivl
#define edx_ivq Iu.Ivq
    union
    {
	u_int8_t  Rk[3][8];
	u_int32_t Eks[3][16][2];
    }Xu;
#define edx_rk  Xu.Rk
#define edx_eks Xu.Eks
};

struct esp_old_xencap
{
    u_int32_t   edx_enc_algorithm;
    u_int32_t	edx_ivlen;
    u_int32_t	edx_keylen;
    u_int8_t	edx_data[1];	/* IV + key material */
};

#define ESP_OLD_XENCAP_LEN	(3 * sizeof(u_int32_t))

struct esp_new_xencap
{
    u_int32_t   edx_enc_algorithm;
    u_int32_t   edx_hash_algorithm;
    int32_t	edx_ivlen;	/* 0 or 8 */
    u_int32_t	edx_keylen;
    u_int32_t	edx_wnd;
    u_int32_t   edx_flags;
    u_int8_t	edx_data[1];	/* IV + key material */
};

#define ESP_NEW_XENCAP_LEN	(6 * sizeof(u_int32_t))

#define ESP_NEW_FLAG_AUTH	0x00000001	/* Doing authentication too */
struct esp_new_xdata
{
    u_int32_t   edx_enc_algorithm;
    u_int32_t   edx_hash_algorithm;
    int32_t     edx_ivlen;      /* 0 or 8 */
    u_int32_t   edx_rpl;	/* Replay counter */
    u_int32_t   edx_wnd;	/* Replay window */
    u_int32_t   edx_bitmap;
    u_int32_t   edx_flags;
    u_int32_t   edx_initial;	/* initial replay value */
    union
    {
	u_int8_t  Iv[ESP_MAX_IVS]; /* that's enough space */
	u_int32_t Ivl;      	/* make sure this is 4 bytes */
	u_int64_t Ivq; 		/* make sure this is 8 bytes! */
    }Iu;
    union
    {
	u_int8_t  Rk[3][8];
	u_int32_t Eks[3][16][2];
    }Xu;
    union
    {
	struct
	{
    	    MD5_CTX	edx_ictx;
    	    MD5_CTX	edx_octx;
	} MD5stuff;
	struct
	{
	    SHA1_CTX	edx_ictx;
	    SHA1_CTX 	edx_octx;
	} SHA1stuff;   
    } Hashes;
};

#define edx_md5_ictx	Hashes.MD5stuff.edx_ictx
#define edx_md5_octx	Hashes.MD5stuff.edx_octx
#define edx_sha1_ictx	Hashes.SHA1stuff.edx_ictx
#define edx_sha1_octx	Hashes.SHA1stuff.edx_octx

#define ESP_OLD_FLENGTH		12

#ifdef _KERNEL
struct espstat espstat;
#endif

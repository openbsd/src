/*	$OpenBSD: ip_esp.h,v 1.17 1998/05/18 21:10:41 provos Exp $	*/

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
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
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

#ifndef _MD5_H_
#include <sys/md5k.h>
#endif

#include <netinet/ip_sha1.h>
#include <netinet/ip_rmd160.h>
#include <netinet/ip_blf.h>
#include <netinet/ip_cast.h>

/* IV lengths */
#define ESP_DES_IVS		8
#define ESP_3DES_IVS		8
#define ESP_BLF_IVS             8
#define ESP_CAST_IVS            8

#define ESP_MAX_IVS		ESP_3DES_IVS

/* Block sizes -- it is assumed that they're powers of 2 */
#define ESP_DES_BLKS		8
#define ESP_3DES_BLKS		8
#define ESP_BLF_BLKS            8
#define ESP_CAST_BLKS           8

#define ESP_MAX_BLKS            ESP_3DES_BLKS

/* Various defines for the "new" ESP */
#define ESP_NEW_ALEN		12	/* 96bits authenticator */
#define ESP_NEW_IPAD_VAL	0x36
#define	ESP_NEW_OPAD_VAL	0x5C

struct esp_hash {
    int type;
    char *name;
    u_int16_t hashsize; 
    u_int16_t ctxsize;
    void (*Init)(void *);
    void (*Update)(void *, u_int8_t *, u_int16_t);
    void (*Final)(u_int8_t *, void *);
};

struct esp_xform {
    int type;
    char *name;
    u_int16_t blocksize, ivsize;
    u_int16_t minkey, maxkey;
    u_int32_t ivmask;           /* Or all possible modes, zero iv = 1 */ 
    void (*encrypt)(void *, u_int8_t *);
    void (*decrypt)(void *, u_int8_t *);
};

struct esp_old
{
    u_int32_t	esp_spi;	/* Security Parameters Index */
    u_int8_t	esp_iv[8];	/* iv[4] may actually be data! */
};

struct esp_new
{
    u_int32_t   esp_spi;        /* Security Parameter Index */
    u_int32_t   esp_rpl;        /* Sequence Number, Replay Counter */
    u_int8_t    esp_iv[8];      /* Data may start already at iv[0]! */
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
    struct esp_xform *edx_xform;
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
    u_int32_t	edx_ivlen;	/* 0 or 8 */
    u_int16_t	edx_confkeylen;
    u_int16_t	edx_authkeylen;
    int32_t	edx_wnd;
    u_int32_t   edx_flags;
    u_int8_t	edx_data[1];	/* IV + key material */
};

#define ESP_NEW_XENCAP_LEN	(6 * sizeof(u_int32_t))

#define ESP_NEW_FLAG_AUTH	0x00000001	/* Doing authentication too */
#define ESP_NEW_FLAG_NPADDING	0x00000002	/* New style padding */

struct esp_new_xdata
{
    u_int32_t   edx_enc_algorithm;
    u_int32_t   edx_hash_algorithm;
    u_int32_t   edx_ivlen;      /* 0 or 8 */
    u_int32_t   edx_rpl;	/* Replay counter */
    int32_t     edx_wnd;		/* Replay window */
    u_int32_t   edx_bitmap;
    u_int32_t   edx_flags;
    u_int32_t   edx_initial;	/* initial replay value */
    struct esp_hash *edx_hash;
    struct esp_xform *edx_xform;
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
	blf_ctx   Bks;
	cast_key  Cks;
    }Xu;
    union
    {
	MD5_CTX    edx_MD5_ictx;
	SHA1_CTX   edx_SHA1_ictx;
        RMD160_CTX edx_RMD160_ictx;
    } edx_ictx;
    union 
    {
    	MD5_CTX	   edx_MD5_octx;
	SHA1_CTX   edx_SHA1_octx;
        RMD160_CTX edx_RMD160_octx;
    } edx_octx;
};

#define edx_bks         Xu.Bks
#define edx_cks         Xu.Cks
#define edx_md5_ictx	edx_ictx.edx_MD5_ictx
#define edx_md5_octx	edx_octx.edx_MD5_octx
#define edx_sha1_ictx	edx_ictx.edx_SHA1_ictx
#define edx_sha1_octx	edx_octx.edx_SHA1_octx
#define edx_rmd160_ictx	edx_ictx.edx_RMD160_ictx
#define edx_rmd160_octx	edx_octx.edx_RMD160_octx

#define ESP_OLD_FLENGTH		12
#define ESP_NEW_FLENGTH         16

#ifdef _KERNEL
struct espstat espstat;
#endif

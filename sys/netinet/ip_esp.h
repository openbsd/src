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
#include <netinet/ip_md5.h>
#endif

#define ESPDESMD5_KEYSZ		64
#define ESPDESMD5_IVS		8
#define ESPDESMD5_ALEN		16
#define ESPDESMD5_IPAD_VAL	0x36
#define	ESPDESMD5_OPAD_VAL	0x5C
#define ESPDESMD5_DESBLK	8
#define ESPDESMD5_RPLENGTH	4
#define ESPDESMD5_DPADI		0x5C
#define ESPDESMD5_DPADR		0x3A
#define ESPDESMD5_IPADI		0xAC
#define ESPDESMD5_IPADR		0x55
#define ESPDESMD5_HPADI		0x53
#define ESPDESMD5_HPADR		0x3C
#define ESPDESMD5_RPADI		0x35
#define ESPDESMD5_RPADR		0xCC

#define ESP3DESMD5_KEYSZ		64
#define ESP3DESMD5_IVS	        	8
#define ESP3DESMD5_ALEN		        16
#define ESP3DESMD5_IPAD_VAL	        0x36
#define	ESP3DESMD5_OPAD_VAL	        0x5C
#define ESP3DESMD5_DESBLK	        8
#define ESP3DESMD5_RPLENGTH	        4
#define ESP3DESMD5_DPADI		0x5C
#define ESP3DESMD5_DPADR		0x3A
#define ESP3DESMD5_IPADI		0xAC
#define ESP3DESMD5_IPADR		0x55
#define ESP3DESMD5_HPADI		0x53
#define ESP3DESMD5_HPADR		0x3C
#define ESP3DESMD5_RPADI		0x35
#define ESP3DESMD5_RPADR		0xCC

struct esp
{
	u_int32_t	esp_spi;	/* Security Parameters Index */
	u_int8_t	esp_iv[8];	/* iv[4] may actually be data! */
};

struct espstat
{
	u_long	esps_hdrops;	/* packet shorter than header shows */
	u_long	esps_notdb;
	u_long	esps_badkcr;
	u_long	esps_qfull;
	u_long	esps_noxform;
	u_long	esps_badilen;
	u_long  esps_wrap;	/* Replay counter wrapped around */
	u_long	esps_badauth;	/* Only valid for transforms with auth */
	u_long  esps_replay;	/* Possible packet replay detected */
};

struct espdes_xdata
{
        int32_t     edx_ivlen;              /* 4 or 8 */
        union
        {
                u_int8_t  Iv[8];        /* that's enough space */
                u_int32_t Ivl;      	/* make sure this is 4 bytes */
                u_int64_t Ivq; 		/* make sure this is 8 bytes! */
        }Iu;
#define edx_iv  Iu.Iv
#define edx_ivl Iu.Ivl
#define edx_ivq Iu.Ivq
        union
        {
                u_int8_t  Rk[8];
                u_int32_t Eks[16][2];
        }Xu;
#define edx_rk  Xu.Rk
#define edx_eks Xu.Eks
};

struct esp3desmd5_xencap
{
	int8_t		edx_ivlen;		/* 0 or 8 */
	int8_t		edx_initiator;		/* 1 if setting an I key */
	u_int16_t	edx_keylen;
	u_int32_t	edx_wnd;
	u_int8_t	edx_ivv[ESP3DESMD5_IVS];
	u_int8_t	edx_key[ESP3DESMD5_KEYSZ];
};

struct espdesmd5_xencap
{
	int8_t		edx_ivlen;		/* 0 or 8 */
	int8_t		edx_initiator;		/* 1 if setting an I key */
	u_int16_t	edx_keylen;
	u_int32_t	edx_wnd;
	u_int8_t	edx_ivv[ESPDESMD5_IVS];
	u_int8_t	edx_key[ESPDESMD5_KEYSZ];
};

#define ESPDESMD5_ULENGTH 8+ESPDESMD5_IVS+ESPDESMD5_KEYSZ
#define ESP3DESMD5_ULENGTH 8+ESP3DESMD5_IVS+ESP3DESMD5_KEYSZ

struct espdesmd5_xdata
{
        int32_t     edx_ivlen;          /* 0 or 8 */
	u_int32_t   edx_rpl;		/* Replay counter */
	u_int32_t   edx_wnd;		/* Replay window */
	u_int32_t   edx_bitmap;
	u_int32_t   edx_initial;	/* initial replay value */
        union
        {
                u_int8_t  Iv[8];        /* that's enough space */
                u_int32_t Ivl;      	/* make sure this is 4 bytes */
                u_int64_t Ivq; 		/* make sure this is 8 bytes! */
        }Iu;
        union
        {
                u_int8_t  Rk[8];
                u_int32_t Eks[16][2];
        }Xu;
	MD5_CTX	    edx_ictx;
	MD5_CTX	    edx_octx;
};

struct esp3desmd5_xdata
{
        int32_t     edx_ivlen;          /* 0 or 4 or 8 */
	u_int32_t   edx_rpl;		/* Replay counter */
	u_int32_t   edx_wnd;		/* Replay window */
	u_int32_t   edx_bitmap;
	u_int32_t   edx_initial;
        union
        {
                u_int8_t  Iv[8];        /* that's enough space */
                u_int32_t Ivl;      	/* make sure this is 4 bytes */
                u_int64_t Ivq; 		/* make sure this is 8 bytes! */
        }Iu;
        union
        {
                u_int8_t  Rk[3][8];
                u_int32_t Eks[3][16][2];
        }Xu;
	MD5_CTX		edx_ictx;
	MD5_CTX		edx_octx;
};

#define ESP_FLENGTH	12
#define ESP_ULENGTH	20		/* coming from user mode */

#ifdef _KERNEL

struct espstat espstat;

#endif

/*	$OpenBSD: ip_esp.h,v 1.23 1999/04/11 19:41:38 niklas Exp $	*/

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

/* Various defines for the "new" ESP */
#define ESP_NEW_ALEN		12	/* 96bits authenticator */

struct esp_old
{
    u_int32_t	esp_spi;	/* Security Parameters Index */
    u_int8_t	esp_iv[8];	/* iv[4] may actually be data! */
};

#define ESP_OLD_FLENGTH    12
#define ESP_NEW_FLENGTH    16

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
    u_int32_t	esps_toobig;	/* packet got larger than IP_MAXPACKET */
    u_int32_t	esps_pdrops;	/* packet blocked due to policy */
};

/*
 * Names for ESP sysctl objects
 */
#define	ESPCTL_ENABLE	1		/* Enable ESP processing */
#define ESPCTL_MAXID	2

#define ESPCTL_NAMES { \
	{ 0, 0 }, \
	{ "enable", CTLTYPE_INT }, \
}

#ifdef _KERNEL
void	esp_input __P((struct mbuf *, ...));
int	esp_output __P((struct mbuf *, struct sockaddr_encap *,
    struct tdb *, struct mbuf **));
int	esp_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));

extern int esp_enable;
struct espstat espstat;
#endif /* _Kernel */

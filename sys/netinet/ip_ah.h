/*	$OpenBSD: ip_ah.h,v 1.22 2000/01/09 23:42:37 angelos Exp $	*/

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
 * Authentication Header Processing
 * Per RFC1826 (Atkinson, 1995)
 */

struct ah_old
{
    u_int8_t	ah_nh;			/* Next header (protocol) */
    u_int8_t	ah_hl;			/* AH length, in 32-bit words */
    u_int16_t	ah_rv;			/* reserved, must be 0 */
    u_int32_t	ah_spi;			/* Security Parameters Index */
    u_int8_t	ah_data[1];		/* More, really */
};

#define AH_OLD_FLENGTH		8	/* size of fixed part */

struct ahstat
{
    u_int32_t	ahs_hdrops;	/* packet shorter than header shows */
    u_int32_t   ahs_nopf;      /* Protocol family not supported */
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
    u_int32_t	ahs_toobig;	/* packet got larger than IP_MAXPACKET */
    u_int32_t	ahs_pdrops;	/* packet blocked due to policy */
};

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

/* Size of the largest hash function output used in AH-new, in bytes */
#define AH_MAX_HASHLEN		20

/*
 * Names for AH sysctl objects
 */
#define	AHCTL_ENABLE	1		/* Enable AH processing */
#define AHCTL_MAXID	2

#define AHCTL_NAMES { \
	{ 0, 0 }, \
	{ "enable", CTLTYPE_INT }, \
}

#ifdef _KERNEL
void	ah_input __P((struct mbuf *, ...));
int	ah_output __P((struct mbuf *, struct tdb *, struct mbuf **));
int	ah_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));

#ifdef INET6
int	ah6_input __P((struct mbuf **, int *, int));
#endif /* INET6 */

extern int ah_enable;
struct ahstat ahstat;
#endif /* _KERNEL */

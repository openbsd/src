/*	$OpenBSD: ip_ah.h,v 1.25 2000/03/17 10:25:22 angelos Exp $	*/

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

#ifndef _NETINET_AH_H_
#define _NETINET_AH_H_

struct ahstat
{
    u_int32_t	ahs_hdrops;	/* Packet shorter than header shows */
    u_int32_t	ahs_nopf;	/* Protocol family not supported */
    u_int32_t	ahs_notdb;
    u_int32_t	ahs_badkcr;
    u_int32_t	ahs_badauth;
    u_int32_t	ahs_noxform;
    u_int32_t	ahs_qfull;
    u_int32_t	ahs_wrap;
    u_int32_t	ahs_replay;
    u_int32_t	ahs_badauthl;	/* Bad authenticator length */
    u_int32_t	ahs_input;	/* Input AH packets */
    u_int32_t	ahs_output;	/* Output AH packets */
    u_int32_t	ahs_invalid;	/* Trying to use an invalid TDB */
    u_int64_t	ahs_ibytes;	/* Input bytes */
    u_int64_t	ahs_obytes;	/* Output bytes */
    u_int32_t	ahs_toobig;	/* Packet got larger than IP_MAXPACKET */
    u_int32_t	ahs_pdrops;	/* Packet blocked due to policy */
    u_int32_t	ahs_crypto;	/* Crypto processing failure */
};

struct ah
{
    u_int8_t   ah_nh;
    u_int8_t   ah_hl;
    u_int16_t  ah_rv;
    u_int32_t  ah_spi;
    u_int32_t  ah_rpl;  /* We may not use this, if we're using old xforms */
};

/* Length of base AH header */
#define AH_FLENGTH		8

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
extern int ah_enable;
struct ahstat ahstat;
#endif /* _KERNEL */
#endif /* _NETINET_AH_H_ */

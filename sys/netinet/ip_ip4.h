/*	$OpenBSD: ip_ip4.h,v 1.11 1999/02/17 18:10:38 deraadt Exp $	*/

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
 * IP-inside-IP processing.
 * Not quite all the functionality of RFC-1853, but the main idea is there.
 */

struct ip4stat {
	u_int32_t	ip4s_ipackets;	/* total input packets */
	u_int32_t	ip4s_opackets;	/* total output packets */
	u_int32_t	ip4s_hdrops;	/* packet shorter than header shows */
	u_int32_t	ip4s_badlen;
	u_int32_t	ip4s_notip4;
	u_int32_t	ip4s_qfull;
	u_int64_t	ip4s_ibytes;
	u_int64_t	ip4s_obytes;
};

#define IP4_DEFAULT_TTL    0
#define IP4_SAME_TTL	  -1

#ifdef _KERNEL
struct ip4stat ip4stat;
#endif

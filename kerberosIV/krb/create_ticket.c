/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/create_ticket.c,v $
 *
 * $Locker:  $
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

/*
 * Create ticket takes as arguments information that should be in a
 * ticket, and the KTEXT object in which the ticket should be
 * constructed.  It then constructs a ticket and returns, leaving the
 * newly created ticket in tkt.
 * The length of the ticket is a multiple of
 * eight bytes and is in tkt->length.
 *
 * If the ticket is too long, the ticket will contain nulls.
 * The return value of the routine is undefined.
 *
 * The corresponding routine to extract information from a ticket it
 * decomp_ticket.  When changes are made to this routine, the
 * corresponding changes should also be made to that file.
 *
 * The packet is built in the following format:
 * 
 * 			variable
 * type			or constant	   data
 * ----			-----------	   ----
 *
 * tkt->length		length of ticket (multiple of 8 bytes)
 * 
 * tkt->dat:
 * 
 * unsigned char	flags		   namely, HOST_BYTE_ORDER
 * 
 * string		pname		   client's name
 * 
 * string		pinstance	   client's instance
 * 
 * string		prealm		   client's realm
 * 
 * 4 bytes		paddress	   client's address
 * 
 * 8 bytes		session		   session key
 * 
 * 1 byte		life		   ticket lifetime
 * 
 * 4 bytes		time_sec	   KDC timestamp
 * 
 * string		sname		   service's name
 * 
 * string		sinstance	   service's instance
 * 
 * <=7 bytes		null		   null pad to 8 byte multiple
 *
 */

int
krb_create_ticket(tkt, flags, pname, pinstance, prealm, paddress,
		  session, life, time_sec, sname, sinstance, key)
	KTEXT tkt;		/* Gets filled in by the ticket */
	unsigned char flags;	/* Various Kerberos flags */
	char *pname;		/* Principal's name */
	char *pinstance;	/* Principal's instance */
	char *prealm;		/* Principal's authentication domain */
	int32_t paddress;	/* Net address of requesting entity */
	char *session;		/* Session key inserted in ticket */
	int16_t life;		/* Lifetime of the ticket */
	int32_t time_sec;	/* Issue time and date */
	char *sname;		/* Service Name */
	char *sinstance;	/* Instance Name */
	des_cblock *key;	/* Service's secret key */
{
    des_key_schedule key_s;
    register char *data;        /* running index into ticket */

    tkt->length = 0;            /* Clear previous data  */
    flags |= HOST_BYTE_ORDER;   /* ticket byte order   */
    bcopy((char *) &flags,(char *) (tkt->dat),sizeof(flags));
    data = ((char *)tkt->dat) + sizeof(flags);
    (void) strcpy(data, pname);
    data += 1 + strlen(pname);
    (void) strcpy(data, pinstance);
    data += 1 + strlen(pinstance);
    (void) strcpy(data, prealm);
    data += 1 + strlen(prealm);
    bcopy((char *) &paddress, data, 4);
    data += 4;

    bcopy((char *) session, data, 8);
    data += 8;
    *(data++) = (char) life;
    /* issue time */
    bcopy((char *) &time_sec, data, 4);
    data += 4;
    (void) strcpy(data, sname);
    data += 1 + strlen(sname);
    (void) strcpy(data, sinstance);
    data += 1 + strlen(sinstance);

    /* guarantee null padded ticket to multiple of 8 bytes */
    bzero(data, 7);
    tkt->length = ((data - ((char *)tkt->dat) + 7)/8)*8;

    /* Check length of ticket */
    if (tkt->length > (sizeof(KTEXT_ST) - 7)) {
        bzero(tkt->dat, tkt->length);
        tkt->length = 0;
        return KFAILURE /* XXX */;
    }

#ifndef NOENCRYPTION
    des_key_sched(key,key_s);
    des_pcbc_encrypt((des_cblock *)tkt->dat,(des_cblock *)tkt->dat,(long)tkt->length,
	key_s,key, DES_ENCRYPT);
#endif
    return 0;
}

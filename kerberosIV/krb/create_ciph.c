/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/create_ciph.c,v $
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
 * This routine is used by the authentication server to create
 * a packet for its client, containing a ticket for the requested
 * service (given in "tkt"), and some information about the ticket,
 *
 * Returns KSUCCESS no matter what.
 *
 * The length of the cipher is stored in c->length; the format of
 * c->dat is as follows:
 *
 * 			variable
 * type			or constant	   data
 * ----			-----------	   ----
 * 
 * 
 * 8 bytes		session		session key for client, service
 * 
 * string		service		service name
 * 
 * string		instance	service instance
 * 
 * string		realm		KDC realm
 * 
 * unsigned char	life		ticket lifetime
 * 
 * unsigned char	kvno		service key version number
 * 
 * unsigned char	tkt->length	length of following ticket
 * 
 * data			tkt->dat	ticket for service
 * 
 * 4 bytes		kdc_time	KDC's timestamp
 *
 * <=7 bytes		null		   null pad to 8 byte multiple
 *
 */

int
create_ciph(c, session, service, instance, realm,
	    life, kvno, tkt, kdc_time, key)
	KTEXT c;		/* Text block to hold ciphertext */
	unsigned char *session;	/* Session key to send to user */
	char *service;		/* Service name on ticket */
	char *instance;		/* Instance name on ticket */
	char *realm;		/* Realm of this KDC */
	u_int32_t life;		/* Lifetime of the ticket */
	int kvno;		/* Key version number for service */
	KTEXT tkt;		/* The ticket for the service */
	u_int32_t kdc_time;	/* KDC time */
	des_cblock *key;	/* Key to encrypt ciphertext with */
{
    char            *ptr;
    des_key_schedule    key_s;

    ptr = (char *) c->dat;

    bcopy((char *) session, ptr, 8);
    ptr += 8;

    (void) strcpy(ptr,service);
    ptr += strlen(service) + 1;

    (void) strcpy(ptr,instance);
    ptr += strlen(instance) + 1;

    (void) strcpy(ptr,realm);
    ptr += strlen(realm) + 1;

    *(ptr++) = (unsigned char) life;
    *(ptr++) = (unsigned char) kvno;
    *(ptr++) = (unsigned char) tkt->length;

    bcopy((char *)(tkt->dat),ptr,tkt->length);
    ptr += tkt->length;

    bcopy((char *) &kdc_time,ptr,4);
    ptr += 4;

    /* guarantee null padded encrypted data to multiple of 8 bytes */
    bzero(ptr, 7);

    c->length = (((ptr - (char *) c->dat) + 7) / 8) * 8;

#ifndef NOENCRYPTION
    des_key_sched(key,key_s);
    des_pcbc_encrypt((des_cblock *)c->dat,(des_cblock *)c->dat,(long) c->length,key_s,
	key, DES_ENCRYPT);
#endif /* NOENCRYPTION */

    return(KSUCCESS);
}

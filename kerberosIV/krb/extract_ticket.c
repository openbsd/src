/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/extract_ticket.c,v $
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
 * This routine is obsolete.
 *
 * This routine accepts the ciphertext returned by kerberos and
 * extracts the nth ticket.  It also fills in the variables passed as
 * session, liftime and kvno.
 */

void
extract_ticket(cipher, n, session, lifetime, kvno, realm, ticket)
	KTEXT cipher;		/* The ciphertext */
	int n;			/* Which ticket */
	char *session;		/* The session key for this tkt */
	int *lifetime;		/* The life of this ticket */
	int *kvno;		/* The kvno for the service */
	char *realm;		/* Realm in which tkt issued */
	KTEXT ticket;		/* The ticket itself */
{
    char *ptr;
    int i;

    /* Start after the ticket lengths */
    ptr = (char *) cipher->dat;
    ptr = ptr + 1 + (int) *(cipher->dat);

    /* Step through earlier tickets */
    for (i = 1; i < n; i++)
	ptr = ptr + 11 + strlen(ptr+10) + (int) *(cipher->dat+i);
    bcopy(ptr, (char *) session, 8); /* Save the session key */
    ptr += 8;
    *lifetime = (unsigned char) *(ptr++); /* Save the life of the ticket */
    *kvno = *(ptr++);		/* Save the kvno */
    (void) strcpy(realm,ptr);	/* instance */
    ptr += strlen(realm) + 1;

    /* Save the ticket if its length is non zero */
    ticket->length = *(cipher->dat+n);
    if (ticket->length)
	bcopy(ptr, (char *) (ticket->dat), ticket->length);
}

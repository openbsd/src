/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_request.c,v $
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
 * This procedure is obsolete.  It is used in the kerberos_slave
 * code for Version 3 tickets.
 *
 * This procedure sets s_name, and instance to point to
 * the corresponding fields from tne nth request in the packet.
 * it returns the lifetime requested.  Garbage will be returned
 * if there are less than n requests in the packet.
 */

int
get_request(pkt, n, s_name, instance)
	KTEXT pkt;		/* The packet itself */
	int n;			/* Which request do we want */
	char **s_name;		/* Service name to be filled in */
	char **instance;	/* Instance name to be filled in */
{
    /* Go to the beginning of the request list */
    char *ptr = (char *) pkt_a_realm(pkt) + 6 +
	strlen((char *)pkt_a_realm(pkt));

    /* Read requests until we hit the right one */
    while (n-- > 1) {
        ptr++;
        ptr += 1 + strlen(ptr);
        ptr += 1 + strlen(ptr);
    }

    /* Set the arguments to point to the right place */
    *s_name = 1 + ptr;
    *instance = 2 + ptr + strlen(*s_name);

    /* Return the requested lifetime */
    return((int) *ptr);
}

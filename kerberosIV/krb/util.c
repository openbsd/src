/* $KTH: util.c,v 1.6 1996/10/05 00:18:34 joda Exp $ */

/*
  Copyright 1988 by the Massachusetts Institute of Technology.

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

  Miscellaneous debug printing utilities
 */

#include "krb_locl.h"

/*
 * Print some of the contents of the given authenticator structure
 * (AUTH_DAT defined in "krb.h").  Fields printed are:
 *
 * pname, pinst, prealm, netaddr, flags, cksum, timestamp, session
 */

void
ad_print(AUTH_DAT *x)
{
    /*
     * Print the contents of an auth_dat struct.
     */
    struct in_addr address;
    address.s_addr = x->address;
    printf("\n%s %s %s %s flags %u cksum 0x%X\n\ttkt_tm 0x%X sess_key",
           x->pname, x->pinst, x->prealm,
           inet_ntoa(address), x->k_flags,
           x->checksum, x->time_sec);
    printf("[8] =");
#ifdef NOENCRYPTION
    placebo_cblock_print(x->session);
#else
    des_cblock_print_file(&x->session,stdout);
#endif
    /* skip reply for now */
}

/*
 * Print in hex the 8 bytes of the given session key.
 *
 * Printed format is:  " 0x { x, x, x, x, x, x, x, x }"
 */

#ifdef NOENCRYPTION
placebo_cblock_print(x)
    des_cblock x;
{
    unsigned char *y = (unsigned char *) x;
    int i = 0;

    printf(" 0x { ");

    while (i++ <8) {
        printf("%x",*y++);
        if (i<8) printf(", ");
    }
    printf(" }");
}
#endif

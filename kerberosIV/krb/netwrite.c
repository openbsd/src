/*	$OpenBSD: netwrite.c,v 1.5 1998/02/25 15:51:33 art Exp $	*/
/* $KTH: netwrite.c,v 1.8 1997/06/19 23:56:25 assar Exp $ */

/*
 * This source code is no longer held under any constraint of USA
 * `cryptographic laws' since it was exported legally.  The cryptographic
 * functions were removed from the code and a "Bones" distribution was
 * made.  A Commodity Jurisdiction Request #012-94 was filed with the
 * USA State Department, who handed it to the Commerce department.  The
 * code was determined to fall under General License GTDA under ECCN 5D96G,
 * and hence exportable.  The cryptographic interfaces were re-added by Eric
 * Young, and then KTH proceeded to maintain the code in the free world.
 *
 */

/* 
 *  Copyright (C) 1989 by the Massachusetts Institute of Technology
 *
 *  Export of this software from the United States of America is assumed
 *  to require a specific license from the United States Government.
 *  It is the responsibility of any person or organization contemplating
 *  export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#include "krb_locl.h"

/*
 * krb_net_write() writes "len" bytes from "buf" to the file
 * descriptor "fd".  It returns the number of bytes written or
 * a write() error.  (The calling interface is identical to
 * write(2).)
 *
 * XXX must not use non-blocking I/O
 */

int
krb_net_write(int fd, const void *v, size_t len)
{
    int cc;
    int wrlen = len;
    const char *buf = (const char*)v;

    if (buf == NULL)
      return -1;

    do {
	cc = write(fd, buf, wrlen);
	if (cc < 0)
	    return(cc);
	else {
	    buf += cc;
	    wrlen -= cc;
	}
    } while (wrlen > 0);
    return(len);
}

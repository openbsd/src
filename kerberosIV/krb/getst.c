/*	$OpenBSD: getst.c,v 1.5 1998/02/25 15:51:16 art Exp $	*/
/* $KTH: getst.c,v 1.6 1997/03/23 03:53:11 joda Exp $ */

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
 * getst() takes a file descriptor, a string and a count.  It reads
 * from the file until either it has read "count" characters, or until
 * it reads a null byte.  When finished, what has been read exists in
 * the given string "s".  If "count" characters were actually read, the
 * last is changed to a null, so the returned string is always null-
 * terminated.  getst() returns the number of characters read, including
 * the null terminator.
 */

int
getst(int fd, char *s, int n)
{
    int count = n;
    while (read(fd, s, 1) > 0 && --count)
        if (*s++ == '\0')
            return (n - count);
    *s = '\0';
    return (n - count);
}

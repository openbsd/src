/* $KTH: stime.c,v 1.6 1997/05/02 14:29:20 assar Exp $ */

/*
  Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute of Technology.
 
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
 * Given a pointer to a long containing the number of seconds
 * since the beginning of time (midnight 1 Jan 1970 GMT), return
 * a string containing the local time in the form:
 *
 * "25-Jan-1988 10:17:56"
 */

const char *
krb_stime(time_t *t)
{
    static char st[40];
    struct tm *tm;

    tm = localtime(t);
    snprintf(st, sizeof(st),
	     "%2d-%s-%04d %02d:%02d:%02d",tm->tm_mday,
	     month_sname(tm->tm_mon + 1),tm->tm_year + 1900,
	     tm->tm_hour, tm->tm_min, tm->tm_sec);
    return st;
}

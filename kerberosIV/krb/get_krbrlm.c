/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_krbrlm.c,v $
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
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure.
 *
 * NOTE: for archaic & compatibility reasons, this routine will only return
 * valid results when n = 1.
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 */

int
krb_get_lrealm(r, n)
	char *r;
	int n;
{
    FILE *cnffile;

    if (n > 1)
	return(KFAILURE);  /* Temporary restriction */

    if ((cnffile = fopen(KRB_CONF, "r")) == NULL) {
        char tbuf[128];
        char *tdir = (char *) getenv("KRBCONFDIR");
        strncpy(tbuf, tdir ? tdir : "/etc", sizeof(tbuf));
        strncat(tbuf, "/krb.conf", sizeof(tbuf));
        tbuf[sizeof(tbuf)-1] = 0;
        if ((cnffile = fopen(tbuf,"r")) == NULL)
            return(KFAILURE);
    }

    if (fscanf(cnffile,"%s",r) != 1) {
        (void) fclose(cnffile);
        return(KFAILURE);
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}

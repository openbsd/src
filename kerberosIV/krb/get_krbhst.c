/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_krbhst.c,v $
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
 * Given a Kerberos realm, find a host on which the Kerberos authenti-
 * cation server can be found.
 *
 * krb_get_krbhst takes a pointer to be filled in, a pointer to the name
 * of the realm for which a server is desired, and an integer, n, and
 * returns (in h) the nth entry from the configuration file (KRB_CONF,
 * defined in "krb.h") associated with the specified realm.
 *
 * On end-of-file, krb_get_krbhst returns KFAILURE. If all goes well,
 * the routine returns KSUCCESS.
 *
 * The KRB_CONF file contains the name of the local realm in the first
 * line (not used by this routine), followed by lines indicating realm/host
 * entries.  The words "admin server" following the hostname indicate that 
 * the host provides an administrative database server.
 *
 * For example:
 *
 *	ATHENA.MIT.EDU
 *	ATHENA.MIT.EDU kerberos-1.mit.edu admin server
 *	ATHENA.MIT.EDU kerberos-2.mit.edu
 *	LCS.MIT.EDU kerberos.lcs.mit.edu admin server
 *
 * This is a temporary hack to allow us to find the nearest system running
 * kerberos.  In the long run, this functionality will be provided by a
 * nameserver.
 */

int
krb_get_krbhst(h, r, n)
	char *h;
	char *r;
	int n;
{
    FILE *cnffile;
    char tr[REALM_SZ];
    char linebuf[BUFSIZ];
    register int i;

    if ((cnffile = fopen(KRB_CONF,"r")) == NULL) {
        char tbuf[128];
        char *tdir = (char *) getenv("KRBCONFDIR");
        strncpy(tbuf, tdir ? tdir : "/etc", sizeof(tbuf));
        strncat(tbuf, "/krb.conf", sizeof(tbuf));
        tbuf[sizeof(tbuf)-1] = 0;
        if ((cnffile = fopen(tbuf,"r")) == NULL)
            return(KFAILURE);
    }
    if (fscanf(cnffile,"%s",tr) == EOF)
        return(KFAILURE);
    /* run through the file, looking for the nth server for this realm */
    for (i = 1; i <= n;) {
	if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
            (void) fclose(cnffile);
            return(KFAILURE);
        }
	if (sscanf(linebuf, "%s %s", tr, h) != 2)
	    continue;
        if (!strcmp(tr,r))
            i++;
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}

/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_admhst.c,v $
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
 * Given a Kerberos realm, find a host on which the Kerberos database
 * administration server can be found.
 *
 * krb_get_admhst takes a pointer to be filled in, a pointer to the name
 * of the realm for which a server is desired, and an integer n, and
 * returns (in h) the nth administrative host entry from the configuration
 * file (KRB_CONF, defined in "krb.h") associated with the specified realm.
 *
 * On error, get_admhst returns KFAILURE. If all goes well, the routine
 * returns KSUCCESS.
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 *
 * This is a temporary hack to allow us to find the nearest system running
 * a Kerberos admin server.  In the long run, this functionality will be
 * provided by a nameserver.
 */

int
krb_get_admhst(h, r, n)
	char *h;
	char *r;
	int n;
{
    FILE *cnffile;
    char tr[REALM_SZ];
    char linebuf[BUFSIZ];
    char scratch[64];
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
    if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
	/* error reading */
	(void) fclose(cnffile);
	return(KFAILURE);
    }
    if (!strchr(linebuf, '\n')) {
	/* didn't all fit into buffer, punt */
	(void) fclose(cnffile);
	return(KFAILURE);
    }
    for (i = 0; i < n; ) {
	/* run through the file, looking for admin host */
	if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
            (void) fclose(cnffile);
            return(KFAILURE);
        }
	/* need to scan for a token after 'admin' to make sure that
	   admin matched correctly */
	if (sscanf(linebuf, "%s %s admin %s", tr, h, scratch) != 3)
	    continue;
        if (!strcmp(tr,r))
            i++;
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}

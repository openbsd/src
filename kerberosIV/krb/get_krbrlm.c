/*	$OpenBSD: get_krbrlm.c,v 1.15 1998/05/18 00:53:41 art Exp $	*/
/*	$KTH: get_krbrlm.c,v 1.20 1998/03/18 13:46:51 bg Exp $		*/

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
 * krb_get_lrealm takes a pointer to a string, and a number, n.  It fills
 * in the string, r, with the name of the nth realm specified on the
 * first line of the kerberos config file (KRB_CONF, defined in "krb.h").
 * It returns 0 (KSUCCESS) on success, and KFAILURE on failure.  If the
 * config file does not exist, and if n=1, a successful return will occur
 * with r = KRB_REALM (also defined in "krb.h").
 *
 * NOTE: for archaic & compatibility reasons, this routine will only return
 * valid results when n = 1.
 *
 * For the format of the KRB_CONF file, see comments describing the routine
 * krb_get_krbhst().
 */

static int
krb_get_lrealm_f(char *r, int n, const char *fname)
{
    FILE *f;
    int ret = KFAILURE;
    f = fopen(fname, "r");
    if(f){
	char buf[REALM_SZ];
	if(fgets(buf, sizeof(buf), f)){
	    char *p = buf + strspn(buf, " \t");
	    p[strcspn(p, " \t\r\n")] = 0;
	    p[REALM_SZ - 1] = 0;
	    strncpy(r, p, REALM_SZ);
	    r[REALM_SZ-1] = '\0';
	    if (*p != '#')
		ret = KSUCCESS;
	}
	fclose(f);
    }
    return ret;
}

static const char *no_default_realm = "NO.DEFAULT.REALM";

int
krb_get_lrealm(char *r, int n)
{
    int i;
    char file[MAXPATHLEN];
    
    if (n > 1)
	return(KFAILURE);		/* Temporary restriction */

    r[0] = '#';
    
    for (i = 0; krb_get_krbconf(i, file, sizeof(file)) == 0; i++)
	if (krb_get_lrealm_f(r, n, file) == KSUCCESS)
	    return KSUCCESS;
    
    if (r[0] == '#')
	return(KFAILURE);
    
    /* When nothing else works try default realm */
    if (n == 1) {
	char *t = krb_get_default_realm();

	if (strcmp(t, no_default_realm) == 0)
	    return KFAILURE;
	    
	strncpy (r, t, REALM_SZ - 1);
	r[REALM_SZ - 1] = '\0';

	return KSUCCESS;
    }
    else
	return(KFAILURE);
}

/* For SunOS5 compat. */
char *
krb_get_default_realm(void)
{
    static char local_realm[REALM_SZ]; /* Local kerberos realm */

    if (local_realm[0] == 0)
    {
	char *t, hostname[MAXHOSTNAMELEN];
	
	strncpy(local_realm, no_default_realm, sizeof(local_realm) - 1);
	local_realm[sizeof(local_realm) - 1] = '\0';
	
	gethostname(hostname, sizeof(hostname));
	t = krb_realmofhost(hostname);
	if (t && strcmp(t, no_default_realm) != 0) {
	    strncpy(local_realm, t, sizeof(local_realm) - 1);
	    local_realm[sizeof(local_realm) - 1] = '\0';
	}
    }

    return local_realm;
}

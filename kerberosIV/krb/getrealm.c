/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/getrealm.c,v $
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
#include <netdb.h>

#define MATCH_SUBDOMAINS        0

/* for Ultrix and friends ... */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

/*
 * krb_realmofhost.
 * Given a fully-qualified domain-style primary host name,
 * return the name of the Kerberos realm for the host.
 * If the hostname contains no discernable domain, or an error occurs,
 * return the local realm name, as supplied by get_krbrlm().
 * If the hostname contains a domain, but no translation is found,
 * the hostname's domain is converted to upper-case and returned.
 *
 * The format of each line of the translation file is:
 * domain_name kerberos_realm
 * -or-
 * host_name kerberos_realm
 *
 * domain_name should be of the form .XXX.YYY (e.g. .LCS.MIT.EDU)
 * host names should be in the usual form (e.g. FOO.BAR.BAZ)
 */

static char ret_realm[REALM_SZ+1];

char *
krb_realmofhost(host)
	char *host;
{
	char *domain;
	FILE *trans_file;
	char trans_host[MAXHOSTNAMELEN+1];
	char trans_realm[REALM_SZ+1];
	struct hostent *hp;
	int retval;

	if ((hp = gethostbyname(host)) != NULL)
		host = hp->h_name;

	domain = strchr(host, '.');

	/* prepare default */
	if (domain) {
		char *cp;

		strncpy(ret_realm, &domain[1], REALM_SZ);
		ret_realm[REALM_SZ] = '\0';
		/* Upper-case realm */
		for (cp = ret_realm; *cp; cp++)
			if (islower(*cp))
				*cp = toupper(*cp);
	} else {
		krb_get_lrealm(ret_realm, 1);
	}

	if ((trans_file = fopen(KRB_RLM_TRANS, "r")) == (FILE *) 0) {
	        char tbuf[128];
		char *tdir = (char *) getenv("KRBCONFDIR");
		strncpy(tbuf, tdir ? tdir : "/etc", sizeof(tbuf));
		strncat(tbuf, "/krb.realms", sizeof(tbuf));
		tbuf[sizeof(tbuf)-1] = 0;
		if ((trans_file = fopen(tbuf,"r")) == NULL)
                        return(ret_realm); /* krb_errno = KRB_NO_TRANS */
	}
	while (1) {
		if ((retval = fscanf(trans_file, "%s %s",
				     trans_host, trans_realm)) != 2) {
			if (retval == EOF) {
				fclose(trans_file);
				return(ret_realm);
			}
			continue;	/* ignore broken lines */
		}
		trans_host[MAXHOSTNAMELEN] = '\0';
		trans_realm[REALM_SZ] = '\0';
		if (!strcasecmp(trans_host, host)) {
			/* exact match of hostname, so return the realm */
			(void) strcpy(ret_realm, trans_realm);
			fclose(trans_file);
			return(ret_realm);
		}
		if ((trans_host[0] == '.') && domain) { 
#if     MATCH_SUBDOMAINS
                        char *cp;
                        for (cp = domain; cp != NULL; cp = strchr(cp+1, '.')) {
                                /* this is a domain match */
                                if (!strcasecmp(trans_host, cp)) {
                                        /* domain match, save for later */
                                        (void) strcpy(ret_realm, trans_realm);
                                        continue;
                                }
                        }
#else /* MATCH_SUBDOMAINS */
			/* this is a domain match */ 
			if (!strcasecmp(trans_host, domain)) { 
				/* domain match, save for later */ 
				(void) strcpy(ret_realm, trans_realm); 
				continue; 
			} 
#endif /* MATCH_SUBDOMAINS */
		}
	}
}

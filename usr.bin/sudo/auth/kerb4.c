/*
 * Copyright (c) 1999, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <krb.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: kerb4.c,v 1.11 2004/02/13 21:36:47 millert Exp $";
#endif /* lint */

int
kerb4_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static char realm[REALM_SZ];

    /* Don't try to verify root */
    if (pw->pw_uid == 0)
	return(AUTH_FAILURE);

    /* Get the local realm, or retrun failure (no krb.conf) */
    if (krb_get_lrealm(realm, 1) != KSUCCESS)
	return(AUTH_FAILURE);

    /* Stash a pointer to the realm (used in kerb4_verify) */
    auth->data = (VOID *) realm;

    return(AUTH_SUCCESS);
}

int
kerb4_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    char tkfile[sizeof(_PATH_SUDO_TIMEDIR) + 4 + MAX_UID_T_LEN];
    char *realm = (char *) auth->data;
    int error;

    /*
     * Set the ticket file to be in sudo sudo timedir so we don't
     * wipe out other (real) kerberos tickets.
     */
    (void) snprintf(tkfile, sizeof(tkfile), "%s/tkt%lu",
	_PATH_SUDO_TIMEDIR, (unsigned long) pw->pw_uid);
    (void) krb_set_tkt_string(tkfile);

    /* Convert the password to a ticket given. */
    error = krb_get_pw_in_tkt(pw->pw_name, "", realm, "krbtgt", realm,
	DEFAULT_TKT_LIFE, pass);

    switch (error) {
	case INTK_OK:
	    dest_tkt();			/* we are done with the temp ticket */
	    return(AUTH_SUCCESS);
	    break;
	case INTK_BADPW:
	case KDC_PR_UNKNOWN:
	    break;
	default:
	    (void) fprintf(stderr, "Warning: Kerberos error: %s\n",
		krb_err_txt[error]);
    }

    return(AUTH_FAILURE);
}

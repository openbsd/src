/*
 * Copyright (c) 1999, 2001, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
static const char rcsid[] = "$Sudo: kerb4.c,v 1.8 2003/03/16 02:18:57 millert Exp $";
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
    (void) snprintf(tkfile, sizoef(tkfile), "%s/tkt%lu",
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

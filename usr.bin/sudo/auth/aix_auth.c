/*
 * Copyright (c) 1999-2005, 2007-2008 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <config.h>

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

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: aix_auth.c,v 1.25 2008/11/09 14:13:13 millert Exp $";
#endif /* lint */

/*
 * For a description of the AIX authentication API, see
 * http://publib16.boulder.ibm.com/doc_link/en_US/a_doc_lib/libs/basetrf1/authenticate.htm
 */
int
aixauth_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    char *pass;
    char *message = NULL;
    int reenter = 1;
    int rval = AUTH_FAILURE;

    pass = tgetpass(prompt, def_passwd_timeout * 60, tgetpass_flags);
    if (pass) {
	/* XXX - should probably print message on failure. */
	if (authenticate(pw->pw_name, pass, &reenter, &message) == 0)
	    rval = AUTH_SUCCESS;
	free(message);
	zero_bytes(pass, strlen(pass));
    }
    return(rval);
}

int
aixauth_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    /* Unset AUTHSTATE as it may not be correct for the runas user. */
    sudo_unsetenv("AUTHSTATE");

    return(AUTH_SUCCESS);
}

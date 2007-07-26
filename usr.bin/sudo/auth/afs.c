/*
 * Copyright (c) 1999, 2001-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <sys/param.h>
#include <sys/types.h>
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

#undef VOID
#include <afs/stds.h>
#include <afs/kautils.h>

#ifndef lint
__unused static const char rcsid[] = "$Sudo: afs.c,v 1.10.2.2 2007/06/12 01:28:42 millert Exp $";
#endif /* lint */

int
afs_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    struct ktc_encryptionKey afs_key;
    struct ktc_token afs_token;

    /* Try to just check the password */
    ka_StringToKey(pass, NULL, &afs_key);
    if (ka_GetAdminToken(pw->pw_name,		/* name */
			 NULL,			/* instance */
			 NULL,			/* realm */
			 &afs_key,		/* key (contains password) */
			 0,			/* lifetime */
			 &afs_token,		/* token */
			 0) == 0)		/* new */
	return(AUTH_SUCCESS);

    /* Fall back on old method XXX - needed? */
    setpag();
    if (ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION+KA_USERAUTH_DOSETPAG,
				   pw->pw_name,	/* name */
				   NULL,	/* instance */
				   NULL,	/* realm */
				   pass,	/* password */
				   0,		/* lifetime */
				   NULL,	/* expiration ptr (unused) */
				   0,		/* spare */
				   NULL) == 0)	/* reason */
	return(AUTH_SUCCESS);

    return(AUTH_FAILURE);
}

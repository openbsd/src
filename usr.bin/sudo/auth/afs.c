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
static const char rcsid[] = "$Sudo: afs.c,v 1.8 2003/03/15 20:37:44 millert Exp $";
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

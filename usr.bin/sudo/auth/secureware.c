/*
 * Copyright (c) 1998, 1999, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
#ifdef __hpux
#  undef MAXINT
#  include <hpsecurity.h>
#else
#  include <sys/security.h>
#endif /* __hpux */
#include <prot.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: secureware.c,v 1.10 2004/02/13 21:36:47 millert Exp $";
#endif /* lint */

int
secureware_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
#ifdef __alpha
    extern int crypt_type;

    if (crypt_type == INT_MAX)
	return(AUTH_FAILURE);			/* no shadow */
#endif
    return(AUTH_SUCCESS);
}

int
secureware_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
#ifdef __alpha
    extern int crypt_type;

#  ifdef HAVE_DISPCRYPT
    if (strcmp(user_passwd, dispcrypt(pass, user_passwd, crypt_type)) == 0)
	return(AUTH_SUCCESS);
#  else
    if (crypt_type == AUTH_CRYPT_BIGCRYPT) {
	if (strcmp(user_passwd, bigcrypt(pass, user_passwd)) == 0)
	    return(AUTH_SUCCESS);
    } else if (crypt_type == AUTH_CRYPT_CRYPT16) {
	if (strcmp(user_passwd, crypt(pass, user_passwd)) == 0)
	    return(AUTH_SUCCESS);
    }
#  endif /* HAVE_DISPCRYPT */
#elif defined(HAVE_BIGCRYPT)
    if (strcmp(user_passwd, bigcrypt(pass, user_passwd)) == 0)
	return(AUTH_SUCCESS);
#endif /* __alpha */

	return(AUTH_FAILURE);
}

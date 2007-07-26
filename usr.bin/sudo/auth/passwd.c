/*
 * Copyright (c) 1999-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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
__unused static const char rcsid[] = "$Sudo: passwd.c,v 1.14.2.2 2007/06/12 01:28:42 millert Exp $";
#endif /* lint */

#define DESLEN			13
#define HAS_AGEINFO(p, l)	(l == 18 && p[DESLEN] == ',')

int
passwd_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
#ifdef HAVE_SKEYACCESS
    if (skeyaccess(pw, user_tty, NULL, NULL) == 0)
	return(AUTH_FAILURE);
#endif
    return(AUTH_SUCCESS);
}

int
passwd_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    char sav, *epass;
    size_t pw_len;
    int error;

    pw_len = strlen(pw->pw_passwd);

#ifdef HAVE_GETAUTHUID
    /* Ultrix shadow passwords may use crypt16() */
    error = strcmp(pw->pw_passwd, (char *) crypt16(pass, pw->pw_passwd));
    if (!error)
	return(AUTH_SUCCESS);
#endif /* HAVE_GETAUTHUID */

    /*
     * Truncate to 8 chars if standard DES since not all crypt()'s do this.
     * If this turns out not to be safe we will have to use OS #ifdef's (sigh).
     */
    sav = pass[8];
    if (pw_len == DESLEN || HAS_AGEINFO(pw->pw_passwd, pw_len))
	pass[8] = '\0';

    /*
     * Normal UN*X password check.
     * HP-UX may add aging info (separated by a ',') at the end so
     * only compare the first DESLEN characters in that case.
     */
    epass = (char *) crypt(pass, pw->pw_passwd);
    pass[8] = sav;
    if (HAS_AGEINFO(pw->pw_passwd, pw_len) && strlen(epass) == DESLEN)
	error = strncmp(pw->pw_passwd, epass, DESLEN);
    else
	error = strcmp(pw->pw_passwd, epass);

    return(error ? AUTH_FAILURE : AUTH_SUCCESS);
}

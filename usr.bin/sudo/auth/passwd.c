/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <sys/param.h>
#include <sys/types.h>
#include <pwd.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: passwd.c,v 1.7 2000/03/23 00:27:41 millert Exp $";
#endif /* lint */

#define DESLEN			13
#define HAS_AGEINFO(p, l)	(l == 18 && p[DESLEN] == ',')

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

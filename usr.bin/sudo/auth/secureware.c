/*
 * Copyright (c) 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
static const char rcsid[] = "$Sudo: secureware.c,v 1.7 1999/08/22 09:59:28 millert Exp $";
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

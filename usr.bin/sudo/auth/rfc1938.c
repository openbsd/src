/*
 * Copyright (c) 1994-1996, 1998-1999, 2001, 2003
 *	Todd C. Miller <Todd.Miller@courtesan.com>.  All rights reserved.
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

#if defined(HAVE_SKEY)
# include <skey.h>
# define RFC1938				skey
#  ifdef __NetBSD__
#   define rfc1938challenge(a,b,c,d)	skeychallenge((a),(b),(c),(d))
#  else
#   define rfc1938challenge(a,b,c,d)	skeychallenge((a),(b),(c))
#  endif
# define rfc1938verify(a,b)		skeyverify((a),(b))
#elif defined(HAVE_OPIE)
# include <opie.h>
# define RFC1938			opie
# define rfc1938challenge(a,b,c,d)	opiechallenge((a),(b),(c))
# define rfc1938verify(a,b)		opieverify((a),(b))
#endif

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: rfc1938.c,v 1.12 2003/03/16 02:18:34 millert Exp $";
#endif /* lint */

int
rfc1938_setup(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    char challenge[256];
    static char *orig_prompt = NULL, *new_prompt = NULL;
    static int op_len, np_size;
    static struct RFC1938 rfc1938;

    /* Stash a pointer to the rfc1938 struct if we have not initialized */
    if (!auth->data)
	auth->data = &rfc1938;

    /* Save the original prompt */
    if (orig_prompt == NULL) {
	orig_prompt = *promptp;
	op_len = strlen(orig_prompt);

	/* Ignore trailing colon (we will add our own) */
	if (orig_prompt[op_len - 1] == ':')
	    op_len--;
	else if (op_len >= 2 && orig_prompt[op_len - 1] == ' '
	    && orig_prompt[op_len - 2] == ':')
	    op_len -= 2;
    }

#ifdef HAVE_SKEY
    /* Close old stream */
    if (rfc1938.keyfile)
	(void) fclose(rfc1938.keyfile);
#endif

    /*
     * Look up the user and get the rfc1938 challenge.
     * If the user is not in the OTP db, only post a fatal error if
     * we are running alone (since they may just use a normal passwd).
     */
    if (rfc1938challenge(&rfc1938, pw->pw_name, challenge, sizeof(challenge))) {
	if (IS_ONEANDONLY(auth)) {
	    (void) fprintf(stderr,
			   "%s: You do not exist in the %s database.\n",
			   Argv[0], auth->name);
	    return(AUTH_FATAL);
	} else {
	    return(AUTH_FAILURE);
	}
    }

    /* Get space for new prompt with embedded challenge */
    if (np_size < op_len + strlen(challenge) + 7) {
	np_size = op_len + strlen(challenge) + 7;
	new_prompt = (char *) erealloc(new_prompt, np_size);
    }

    if (def_flag(I_LONG_OTP_PROMPT))
	(void) snprintf(new_prompt, np_size, "%s\n%s", challenge, orig_prompt);
    else
	(void) snprintf(new_prompt, np_size, "%.*s [ %s ]:", op_len,
	    orig_prompt, challenge);

    *promptp = new_prompt;
    return(AUTH_SUCCESS);
}

int
rfc1938_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{

    if (rfc1938verify((struct RFC1938 *) auth->data, pass) == 0)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
}

/*
 * Copyright (c) 1994-1996, 1998-1999, 2001, 2003
 *	Todd C. Miller <Todd.Miller@courtesan.com>.
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
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
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
static const char rcsid[] = "$Sudo: rfc1938.c,v 1.16 2004/02/13 21:36:47 millert Exp $";
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
	    warnx("you do not exist in the %s database", auth->name);
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

    if (def_long_otp_prompt)
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

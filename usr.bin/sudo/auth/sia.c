/*
 * Copyright (c) 1999-2001, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Spider Boardman
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
#include <siad.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: sia.c,v 1.12 2003/04/16 00:42:10 millert Exp $";
#endif /* lint */

static int sudo_collect	__P((int, int, uchar_t *, int, prompt_t *));

static char *def_prompt;

/*
 * Collection routine (callback) for limiting the timeouts in SIA
 * prompts and (possibly) setting a custom prompt.
 */
static int
sudo_collect(timeout, rendition, title, nprompts, prompts)
    int timeout;
    int rendition;
    uchar_t *title;
    int nprompts;
    prompt_t *prompts;
{
    switch (rendition) {
	case SIAFORM:
	case SIAONELINER:
	    if (timeout <= 0 || timeout > def_ival(I_PASSWD_TIMEOUT) * 60)
		timeout = def_ival(I_PASSWD_TIMEOUT) * 60;
	    /*
	     * Substitute custom prompt if a) the sudo prompt is not "Password:"
	     * and b) the SIA prompt is "Password:" (so we know it is safe).
	     * This keeps us from overwriting things like S/Key challenges.
	     */
	    if (strcmp((char *)prompts[0].prompt, "Password:") == 0 &&
		strcmp(def_prompt, "Password:") != 0)
		prompts[0].prompt = (unsigned char *)def_prompt;
	    break;
	default:
	    break;
    }

    return sia_collect_trm(timeout, rendition, title, nprompts, prompts);
}

int
sia_setup(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    SIAENTITY *siah = NULL;
    extern int Argc;
    extern char **Argv;

    if (sia_ses_init(&siah, Argc, Argv, NULL, pw->pw_name, ttyname(0), 1, NULL)
	!= SIASUCCESS) {

	log_error(USE_ERRNO|NO_EXIT|NO_MAIL,
	    "unable to initialize SIA session");
	return(AUTH_FATAL);
    }

    auth->data = (VOID *) siah;
    return(AUTH_SUCCESS);
}

int
sia_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    SIAENTITY *siah = (SIAENTITY *) auth->data;

    def_prompt = prompt;		/* for sudo_collect */

    /* XXX - need a way to detect user hitting return or EOF at prompt */
    if (sia_ses_reauthent(sudo_collect, siah) == SIASUCCESS)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
}

int
sia_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    SIAENTITY *siah = (SIAENTITY *) auth->data;

    (void) sia_ses_release(&siah);
    return(AUTH_SUCCESS);
}

/*
 * Copyright (c) 2000-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <ctype.h>
#include <pwd.h>
#include <signal.h>

#include <login_cap.h>
#include <bsd_auth.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: bsdauth.c,v 1.16.2.2 2007/06/12 01:28:42 millert Exp $";
#endif /* lint */

extern char *login_style;		/* from sudo.c */

int
bsdauth_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static auth_session_t *as;
    extern login_cap_t *lc;			/* from sudo.c */

    if ((as = auth_open()) == NULL) {
	log_error(USE_ERRNO|NO_EXIT|NO_MAIL,
	    "unable to begin bsd authentication");
	return(AUTH_FATAL);
    }

    /* XXX - maybe sanity check the auth style earlier? */
    login_style = login_getstyle(lc, login_style, "auth-sudo");
    if (login_style == NULL) {
	log_error(NO_EXIT|NO_MAIL, "invalid authentication type");
	auth_close(as);
	return(AUTH_FATAL);
    }

     if (auth_setitem(as, AUTHV_STYLE, login_style) < 0 ||
	auth_setitem(as, AUTHV_NAME, pw->pw_name) < 0 ||
	auth_setitem(as, AUTHV_CLASS, login_class) < 0) {
	log_error(NO_EXIT|NO_MAIL, "unable to setup authentication");
	auth_close(as);
	return(AUTH_FATAL);
    }

    auth->data = (VOID *) as;
    return(AUTH_SUCCESS);
}

int
bsdauth_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    char *pass;
    char *s;
    size_t len;
    int authok = 0;
    sigaction_t sa, osa;
    auth_session_t *as = (auth_session_t *) auth->data;
    extern int nil_pw;

    /* save old signal handler */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_DFL;
    (void) sigaction(SIGCHLD, &sa, &osa);

    /*
     * If there is a challenge then print that instead of the normal
     * prompt.  If the user just hits return we prompt again with echo
     * turned on, which is useful for challenge/response things like
     * S/Key.
     */
    if ((s = auth_challenge(as)) == NULL) {
	pass = tgetpass(prompt, def_passwd_timeout * 60, tgetpass_flags);
    } else {
	pass = tgetpass(s, def_passwd_timeout * 60, tgetpass_flags);
	if (pass && *pass == '\0') {
	    if ((prompt = strrchr(s, '\n')))
		prompt++;
	    else
		prompt = s;

	    /*
	     * Append '[echo on]' to the last line of the challenge and
	     * reprompt with echo turned on.
	     */
	    len = strlen(prompt) - 1;
	    while (isspace(prompt[len]) || prompt[len] == ':')
		prompt[len--] = '\0';
	    easprintf(&s, "%s [echo on]: ", prompt);
	    pass = tgetpass(s, def_passwd_timeout * 60,
		tgetpass_flags | TGP_ECHO);
	    free(s);
	}
    }

    if (!pass || *pass == '\0')		/* ^C or empty password */
	nil_pw = 1;

    if (pass) {
	authok = auth_userresponse(as, (char *)pass, 1);
	zero_bytes(pass, strlen(pass));
    }

    /* restore old signal handler */
    (void) sigaction(SIGCHLD, &osa, NULL);

    if (authok)
	return(AUTH_SUCCESS);

    if ((s = auth_getvalue(as, "errormsg")) != NULL)
	log_error(NO_EXIT|NO_MAIL, "%s", s);
    return(AUTH_FAILURE);
}

int
bsdauth_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    auth_session_t *as = (auth_session_t *) auth->data;

    auth_close(as);

    return(AUTH_SUCCESS);
}

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

#include <auth.h>
#include <firewall.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: fwtk.c,v 1.10 2000/02/27 03:49:06 millert Exp $";
#endif /* lint */

int
fwtk_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static Cfg *confp;			/* Configuration entry struct */
    char resp[128];			/* Response from the server */

    if ((confp = cfg_read("sudo")) == (Cfg *)-1) {
	(void) fprintf(stderr, "%s: cannot read fwtk config.\n", Argv[0]);
	return(AUTH_FATAL);
    }

    if (auth_open(confp)) {
	(void) fprintf(stderr, "%s: cannot connect to authentication server.\n",
	    Argv[0]);
	return(AUTH_FATAL);
    }

    /* Get welcome message from auth server */
    if (auth_recv(resp, sizeof(resp))) {
	(void) fprintf(stderr,
	    "%s: lost connection to authentication server.\n", Argv[0]);
	return(AUTH_FATAL);
    }
    if (strncmp(resp, "Authsrv ready", 13) != 0) {
	(void) fprintf(stderr,
	    "%s: authentication server error.\n%s\n", Argv[0], resp);
	return(AUTH_FATAL);
    }

    return(AUTH_SUCCESS);
}

int
fwtk_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    char *pass;				/* Password from the user */
    char buf[SUDO_PASS_MAX + 12];	/* General prupose buffer */
    char resp[128];			/* Response from the server */
    extern int nil_pw;

    /* Send username to authentication server. */
    (void) snprintf(buf, sizeof(buf), "authorize %s 'sudo'", pw->pw_name);
    if (auth_send(buf) || auth_recv(resp, sizeof(resp))) {
	(void) fprintf(stderr,
	    "%s: lost connection to authentication server.\n", Argv[0]);
	return(AUTH_FATAL);
    }

    /* Get the password/response from the user. */
    if (strncmp(resp, "challenge ", 10) == 0) {
	(void) snprintf(buf, sizeof(buf), "%s\nResponse: ", &resp[10]);
	pass = tgetpass(buf, def_ival(I_PW_TIMEOUT) * 60,
	    tgetpass_flags | TGP_ECHO);
    } else if (strncmp(resp, "password", 8) == 0) {
	pass = tgetpass(prompt, def_ival(I_PW_TIMEOUT) * 60, tgetpass_flags);
    } else {
	(void) fprintf(stderr, "%s: %s\n", Argv[0], resp);
	return(AUTH_FATAL);
    }
    if (!pass || *pass == '\0')
	nil_pw = 1;			/* empty password */

    /* Send the user's response to the server */
    (void) snprintf(buf, sizeof(buf), "response '%s'", pass);
    if (auth_send(buf) || auth_recv(resp, sizeof(resp))) {
	(void) fprintf(stderr,
	    "%s: lost connection to authentication server.\n", Argv[0]);
	return(AUTH_FATAL);
    }

    if (strncmp(resp, "ok", 2) == 0)
	return(AUTH_SUCCESS);

    /* Main loop prints "Permission Denied" or insult. */
    if (strcmp(resp, "Permission Denied.") != 0)
	fprintf(stderr, "%s: %s\n", Argv[0], resp);
    return(AUTH_FAILURE);
}

int
fwtk_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{

    auth_close();
    return(AUTH_SUCCESS);
}

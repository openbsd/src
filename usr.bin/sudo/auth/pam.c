/*
 * Copyright (c) 1999-2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
# if defined(HAVE_MEMORY_H) && !defined(STDC_HEADERS)
#  include <memory.h>
# endif
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

#include <security/pam_appl.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: pam.c,v 1.23 2001/12/31 17:18:12 millert Exp $";
#endif /* lint */

static int sudo_conv __P((int, PAM_CONST struct pam_message **,
			  struct pam_response **, VOID *));
static char *def_prompt;

#ifndef PAM_DATA_SILENT
#define PAM_DATA_SILENT	0
#endif

int
pam_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static struct pam_conv pam_conv;
    pam_handle_t *pamh;

    /* Initial PAM setup */
    pam_conv.conv = sudo_conv;
    if (pam_start("sudo", pw->pw_name, &pam_conv, &pamh) != PAM_SUCCESS) {
	log_error(USE_ERRNO|NO_EXIT|NO_MAIL, 
	    "unable to initialize PAM");
	return(AUTH_FATAL);
    }
    if (strcmp(user_tty, "unknown"))
	(void) pam_set_item(pamh, PAM_TTY, user_tty);

    auth->data = (VOID *) pamh;
    return(AUTH_SUCCESS);
}

int
pam_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    int error;
    const char *s;
    pam_handle_t *pamh = (pam_handle_t *) auth->data;

    def_prompt = prompt;	/* for sudo_conv */

    /* PAM_SILENT prevents the authentication service from generating output. */
    error = pam_authenticate(pamh, PAM_SILENT);
    switch (error) {
	case PAM_SUCCESS:
	    return(AUTH_SUCCESS);
	case PAM_AUTH_ERR:
	case PAM_MAXTRIES:
	    return(AUTH_FAILURE);
	default:
	    if ((s = pam_strerror(pamh, error)))
		log_error(NO_EXIT|NO_MAIL, "pam_authenticate: %s", s);
	    return(AUTH_FATAL);
    }
}

int
pam_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    pam_handle_t *pamh = (pam_handle_t *) auth->data;
    int status = PAM_DATA_SILENT;

    /* Convert AUTH_FOO -> PAM_FOO as best we can. */
    /* XXX - store real value somewhere in auth->data and use it */
    switch (auth->status) {
	case AUTH_SUCCESS:
	    status |= PAM_SUCCESS;
	    break;
	case AUTH_FAILURE:
	    status |= PAM_AUTH_ERR;
	    break;
	case AUTH_FATAL:
	default:
	    status |= PAM_ABORT;
	    break;
    }

    if (pam_end(pamh, status) == PAM_SUCCESS)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
}

int
pam_prep_user(pw)
    struct passwd *pw;
{
    struct pam_conv pam_conv;
    pam_handle_t *pamh;
    const char *s;
    int error;

    /* We need to setup a new PAM session for the user we are changing *to*. */
    pam_conv.conv = sudo_conv;
    if (pam_start("sudo", pw->pw_name, &pam_conv, &pamh) != PAM_SUCCESS) {
	log_error(USE_ERRNO|NO_EXIT|NO_MAIL, 
	    "unable to initialize PAM");
	return(AUTH_FATAL);
    }
    (void) pam_set_item(pamh, PAM_RUSER, user_name);
    if (strcmp(user_tty, "unknown"))
	(void) pam_set_item(pamh, PAM_TTY, user_tty);

    /* Set credentials (may include resource limits, device ownership, etc). */
    if ((error = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) {
	if ((s = pam_strerror(pamh, error)))
	    log_error(NO_EXIT|NO_MAIL, "pam_setcred: %s", s);
    }

    if (pam_end(pamh, error) != PAM_SUCCESS)
	return(AUTH_FAILURE);

    return(error == PAM_SUCCESS ? AUTH_SUCCESS : AUTH_FAILURE);
}

/*
 * ``Conversation function'' for PAM.
 */
static int
sudo_conv(num_msg, msg, response, appdata_ptr)
    int num_msg;
    PAM_CONST struct pam_message **msg;
    struct pam_response **response;
    VOID *appdata_ptr;
{
    struct pam_response *pr;
    PAM_CONST struct pam_message *pm;
    const char *p = def_prompt;
    extern int nil_pw;

    if ((*response = malloc(num_msg * sizeof(struct pam_response))) == NULL)
	return(PAM_CONV_ERR);
    (void) memset((VOID *)*response, 0, num_msg * sizeof(struct pam_response));

    for (pr = *response, pm = *msg; num_msg--; pr++, pm++) {
	switch (pm->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		tgetpass_flags |= TGP_ECHO;
	    case PAM_PROMPT_ECHO_OFF:
		/* Only override PAM prompt if it matches /^Password: ?/ */
		if (strncmp(pm->msg, "Password:", 9) || (pm->msg[9] != '\0'
		    && (pm->msg[9] != ' ' || pm->msg[10] != '\0')))
		    p = pm->msg;
		/* Read the password. */
		pr->resp = estrdup((char *) tgetpass(p,
		    def_ival(I_PASSWD_TIMEOUT) * 60, tgetpass_flags));
		if (pr->resp == NULL || *pr->resp == '\0')
		    nil_pw = 1;		/* empty password */
		break;
	    case PAM_TEXT_INFO:
		if (pm->msg)
		    (void) puts(pm->msg);
		break;
	    case PAM_ERROR_MSG:
		if (pm->msg) {
		    (void) fputs(pm->msg, stderr);
		    (void) fputc('\n', stderr);
		}
		break;
	    default:
		/* Something odd happened */
		/* XXX - should free non-NULL response members */
		free(*response);
		*response = NULL;
		return(PAM_CONV_ERR);
		break;
	}
    }

    return(PAM_SUCCESS);
}

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

#include <security/pam_appl.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: pam.c,v 1.15 2000/02/27 03:49:06 millert Exp $";
#endif /* lint */

static int sudo_conv __P((int, PAM_CONST struct pam_message **,
			  struct pam_response **, VOID *));
static char *def_prompt;

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

    /* PAM_SILENT prevents error messages from going to syslog(3) */
    if ((error = pam_authenticate(pamh, PAM_SILENT)) == PAM_SUCCESS)
	return(AUTH_SUCCESS);

    /* Any error other than PAM_AUTH_ERR or PAM_MAXTRIES is probably fatal.  */
    if (error != PAM_AUTH_ERR && error != PAM_MAXTRIES) {
	if ((s = pam_strerror(pamh, error)))
	    log_error(NO_EXIT|NO_MAIL, "pam_authenticate: %s\n", s);
	return(AUTH_FATAL);
    }
    return(AUTH_FAILURE);
}

int
pam_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    pam_handle_t *pamh = (pam_handle_t *) auth->data;

    if (pam_end(pamh, (auth->status == AUTH_SUCCESS)) == PAM_SUCCESS)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
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
		    def_ival(I_PW_TIMEOUT) * 60, tgetpass_flags));
		if (*pr->resp == '\0')
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

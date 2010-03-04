/*
 * Copyright (c) 1999-2005, 2007-2009 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <errno.h>

#ifdef HAVE_PAM_PAM_APPL_H
# include <pam/pam_appl.h>
#else
# include <security/pam_appl.h>
#endif

#ifdef HAVE_DGETTEXT
# include <libintl.h>
# if defined(__LINUX_PAM__)
#  define PAM_TEXT_DOMAIN	"Linux-PAM"
# elif defined(__sun__)
#  define PAM_TEXT_DOMAIN	"SUNW_OST_SYSOSPAM"
# endif
#endif

#include "sudo.h"
#include "sudo_auth.h"

/* Only OpenPAM and Linux PAM use const qualifiers. */
#if defined(_OPENPAM) || defined(__LIBPAM_VERSION) || defined(__LINUX_PAM__)
# define PAM_CONST	const
#else
# define PAM_CONST
#endif

static int sudo_conv __P((int, PAM_CONST struct pam_message **,
			  struct pam_response **, void *));
static char *def_prompt = "Password:";
static int gotintr;

#ifndef PAM_DATA_SILENT
#define PAM_DATA_SILENT	0
#endif

static pam_handle_t *pamh;	/* global due to pam_prep_user() */

int
pam_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static struct pam_conv pam_conv;
    static int pam_status;

    /* Initial PAM setup */
    if (auth != NULL)
	auth->data = (void *) &pam_status;
    pam_conv.conv = sudo_conv;
    pam_status = pam_start("sudo", pw->pw_name, &pam_conv, &pamh);
    if (pam_status != PAM_SUCCESS) {
	log_error(USE_ERRNO|NO_EXIT|NO_MAIL, "unable to initialize PAM");
	return(AUTH_FATAL);
    }

    /*
     * Set PAM_RUSER to the invoking user (the "from" user).
     * We set PAM_RHOST to avoid a bug in Solaris 7 and below.
     */
    (void) pam_set_item(pamh, PAM_RUSER, user_name);
    (void) pam_set_item(pamh, PAM_RHOST, user_host);

    /*
     * Some versions of pam_lastlog have a bug that
     * will cause a crash if PAM_TTY is not set so if
     * there is no tty, set PAM_TTY to the empty string.
     */
    if (user_ttypath == NULL)
	(void) pam_set_item(pamh, PAM_TTY, "");
    else
	(void) pam_set_item(pamh, PAM_TTY, user_ttypath);

    return(AUTH_SUCCESS);
}

int
pam_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    const char *s;
    int *pam_status = (int *) auth->data;

    def_prompt = prompt;	/* for sudo_conv */

    /* PAM_SILENT prevents the authentication service from generating output. */
    *pam_status = pam_authenticate(pamh, PAM_SILENT);
    switch (*pam_status) {
	case PAM_SUCCESS:
	    *pam_status = pam_acct_mgmt(pamh, PAM_SILENT);
	    switch (*pam_status) {
		case PAM_SUCCESS:
		    return(AUTH_SUCCESS);
		case PAM_AUTH_ERR:
		    log_error(NO_EXIT|NO_MAIL, "pam_acct_mgmt: %d",
			*pam_status);
		    return(AUTH_FAILURE);
		case PAM_NEW_AUTHTOK_REQD:
		    log_error(NO_EXIT|NO_MAIL, "%s, %s",
			"Account or password is expired",
			"reset your password and try again");
		    *pam_status = pam_chauthtok(pamh,
			PAM_CHANGE_EXPIRED_AUTHTOK);
		    if (*pam_status == PAM_SUCCESS)
			return(AUTH_SUCCESS);
		    if ((s = pam_strerror(pamh, *pam_status)))
			log_error(NO_EXIT|NO_MAIL, "pam_chauthtok: %s", s);
		    return(AUTH_FAILURE);
		case PAM_AUTHTOK_EXPIRED:
		    log_error(NO_EXIT|NO_MAIL,
			"Password expired, contact your system administrator");
		    return(AUTH_FATAL);
		case PAM_ACCT_EXPIRED:
		    log_error(NO_EXIT|NO_MAIL, "%s %s",
			"Account expired or PAM config lacks an \"account\"",
			"section for sudo, contact your system administrator");
		    return(AUTH_FATAL);
	    }
	    /* FALLTHROUGH */
	case PAM_AUTH_ERR:
	    if (gotintr) {
		/* error or ^C from tgetpass() */
		return(AUTH_INTR);
	    }
	case PAM_MAXTRIES:
	case PAM_PERM_DENIED:
	    return(AUTH_FAILURE);
	default:
	    if ((s = pam_strerror(pamh, *pam_status)))
		log_error(NO_EXIT|NO_MAIL, "pam_authenticate: %s", s);
	    return(AUTH_FATAL);
    }
}

int
pam_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    int *pam_status = (int *) auth->data;

    /* If successful, we can't close the session until pam_prep_user() */
    if (auth->status == AUTH_SUCCESS)
	return(AUTH_SUCCESS);

    *pam_status = pam_end(pamh, *pam_status | PAM_DATA_SILENT);
    return(*pam_status == PAM_SUCCESS ? AUTH_SUCCESS : AUTH_FAILURE);
}

int
pam_prep_user(pw)
    struct passwd *pw;
{
    int eval;

    if (pamh == NULL)
	pam_init(pw, NULL, NULL);

    /*
     * Update PAM_USER to reference the user we are running the command
     * as, as opposed to the user we authenticated as.
     */
    (void) pam_set_item(pamh, PAM_USER, pw->pw_name);

    /*
     * Set credentials (may include resource limits, device ownership, etc).
     * We don't check the return value here because in Linux-PAM 0.75
     * it returns the last saved return code, not the return code
     * for the setcred module.  Because we haven't called pam_authenticate(),
     * this is not set and so pam_setcred() returns PAM_PERM_DENIED.
     * We can't call pam_acct_mgmt() with Linux-PAM for a similar reason.
     */
    (void) pam_setcred(pamh, PAM_ESTABLISH_CRED);

#ifndef NO_PAM_SESSION
    /*
     * To fully utilize PAM sessions we would need to keep a
     * sudo process around until the command exits.  However, we
     * can at least cause pam_limits to be run by opening and then
     * immediately closing the session.
     */
    if ((eval = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
	(void) pam_end(pamh, eval | PAM_DATA_SILENT);
	return(AUTH_FAILURE);
    }
    (void) pam_close_session(pamh, 0);
#endif

    if (pam_end(pamh, PAM_SUCCESS | PAM_DATA_SILENT) == PAM_SUCCESS)
	return(AUTH_SUCCESS);
    else
	return(AUTH_FAILURE);
}

/*
 * ``Conversation function'' for PAM.
 * XXX - does not handle PAM_BINARY_PROMPT
 */
static int
sudo_conv(num_msg, msg, response, appdata_ptr)
    int num_msg;
    PAM_CONST struct pam_message **msg;
    struct pam_response **response;
    void *appdata_ptr;
{
    struct pam_response *pr;
    PAM_CONST struct pam_message *pm;
    const char *prompt;
    char *pass;
    int n, flags, std_prompt;

    if ((*response = malloc(num_msg * sizeof(struct pam_response))) == NULL)
	return(PAM_SYSTEM_ERR);
    zero_bytes(*response, num_msg * sizeof(struct pam_response));

    for (pr = *response, pm = *msg, n = num_msg; n--; pr++, pm++) {
	flags = tgetpass_flags;
	switch (pm->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		SET(flags, TGP_ECHO);
	    case PAM_PROMPT_ECHO_OFF:
		prompt = def_prompt;

		/* Is the sudo prompt standard? (If so, we'l just use PAM's) */
		std_prompt =  strncmp(def_prompt, "Password:", 9) == 0 &&
		    (def_prompt[9] == '\0' ||
		    (def_prompt[9] == ' ' && def_prompt[10] == '\0'));

		/* Only override PAM prompt if it matches /^Password: ?/ */
#if defined(PAM_TEXT_DOMAIN) && defined(HAVE_DGETTEXT)
		if (!def_passprompt_override && (std_prompt ||
		    (strcmp(pm->msg, dgettext(PAM_TEXT_DOMAIN, "Password: ")) &&
		    strcmp(pm->msg, dgettext(PAM_TEXT_DOMAIN, "Password:")))))
		    prompt = pm->msg;
#else
		if (!def_passprompt_override && (std_prompt ||
		    strncmp(pm->msg, "Password:", 9) || (pm->msg[9] != '\0'
		    && (pm->msg[9] != ' ' || pm->msg[10] != '\0'))))
		    prompt = pm->msg;
#endif
		/* Read the password unless interrupted. */
		pass = tgetpass(prompt, def_passwd_timeout * 60, flags);
		if (pass == NULL) {
		    /* We got ^C instead of a password; abort quickly. */
		    if (errno == EINTR)
			gotintr = 1;
#if defined(__darwin__) || defined(__APPLE__)
		    pass = "";
#else
		    goto err;
#endif
		}
		pr->resp = estrdup(pass);
		zero_bytes(pass, strlen(pass));
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
		goto err;
	}
    }

    return(PAM_SUCCESS);

err:
    /* Zero and free allocated memory and return an error. */
    for (pr = *response, n = num_msg; n--; pr++) {
	if (pr->resp != NULL) {
	    zero_bytes(pr->resp, strlen(pr->resp));
	    free(pr->resp);
	    pr->resp = NULL;
	}
    }
    zero_bytes(*response, num_msg * sizeof(struct pam_response));
    free(*response);
    *response = NULL;
    return(gotintr ? PAM_AUTH_ERR : PAM_CONV_ERR);
}

/*
 * Copyright (c) 1999, 2001, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Frank Cusack
 * <fcusack@fcusack.com>.
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
#include <krb5.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: kerb5.c,v 1.14 2003/04/02 18:57:34 millert Exp $";
#endif /* lint */

static int verify_krb_v5_tgt __P((krb5_context, krb5_ccache, char *));
static struct _sudo_krb5_data {
    krb5_context	sudo_context;
    krb5_principal	princ;
    krb5_ccache		ccache;
} sudo_krb5_data = { NULL, NULL, NULL };
typedef struct _sudo_krb5_data *sudo_krb5_datap;

extern krb5_cc_ops krb5_mcc_ops;

int
kerb5_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    krb5_context	sudo_context;
    krb5_ccache		ccache;
    krb5_principal	princ;
    krb5_error_code 	error;
    char		cache_name[64];
    char		*pname;

    auth->data = (VOID *) &sudo_krb5_data; /* Stash all our data here */

    if ((error = krb5_init_context(&(sudo_krb5_data.sudo_context))))
	return(AUTH_FAILURE);
    sudo_context = sudo_krb5_data.sudo_context;

    if ((error = krb5_parse_name(sudo_context, pw->pw_name,
	&(sudo_krb5_data.princ)))) {
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to parse '%s': %s", auth->name, pw->pw_name,
		  error_message(error));
	return(AUTH_FAILURE);
    }
    princ = sudo_krb5_data.princ;

    /*
     * Really, we need to tell the caller not to prompt for password.
     * The API does not currently provide this unless the auth is standalone.
     */
#if 1
    if ((error = krb5_unparse_name(sudo_context, princ, &pname))) {
	log_error(NO_EXIT|NO_MAIL,
		  "%s: unable to unparse princ ('%s'): %s", auth->name,
		  pw->pw_name, error_message(error));
	return(AUTH_FAILURE);
    }

    /* Only rewrite prompt if user didn't specify their own. */
    /*if (!strcmp(prompt, PASSPROMPT)) { */
	easprintf(promptp, "Password for %s: ", pname);
    /*}*/
    free(pname);
#endif

    /* For CNS compatibility */
    if ((error = krb5_cc_register(sudo_context, &krb5_mcc_ops, FALSE))) {
	if (error != KRB5_CC_TYPE_EXISTS) {
	    log_error(NO_EXIT|NO_MAIL, 
		      "%s: unable to use Memory ccache: %s", auth->name,
		      error_message(error));
	    return(AUTH_FAILURE);
	}
    }

    (void) snprintf(cache_name, sizeof(cache_name), "MEMORY:sudocc_%ld",
		    (long) getpid());
    if ((error = krb5_cc_resolve(sudo_context, cache_name,
	&(sudo_krb5_data.ccache)))) {
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to resolve ccache: %s", auth->name,
		  error_message(error));
	return(AUTH_FAILURE);
    }
    ccache = sudo_krb5_data.ccache;

    if ((error = krb5_cc_initialize(sudo_context, ccache, princ))) {
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to initialize ccache: %s", auth->name,
		  error_message(error));
	return(AUTH_FAILURE);
    }

    return(AUTH_SUCCESS);
}

int
kerb5_verify(pw, pass, auth)
    struct passwd *pw;
    char *pass;
    sudo_auth *auth;
{
    krb5_context	sudo_context;
    krb5_principal	princ;
    krb5_ccache		ccache;
    krb5_creds		creds;
    krb5_error_code	error;
    krb5_get_init_creds_opt opts;

    sudo_context = ((sudo_krb5_datap) auth->data)->sudo_context;
    princ = ((sudo_krb5_datap) auth->data)->princ;
    ccache = ((sudo_krb5_datap) auth->data)->ccache;

    /* Initialize options to defaults */
    krb5_get_init_creds_opt_init(&opts);

    /* Note that we always obtain a new TGT to verify the user */
    if ((error = krb5_get_init_creds_password(sudo_context, &creds, princ,
					     pass, krb5_prompter_posix,
					     NULL, 0, NULL, &opts))) {
	if (error == KRB5KRB_AP_ERR_BAD_INTEGRITY) /* Bad password */
	    return(AUTH_FAILURE);
	/* Some other error */
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to get credentials: %s", auth->name,
		  error_message(error));
	return(AUTH_FAILURE);
    }

    /* Stash the TGT so we can verify it. */
    if ((error = krb5_cc_store_cred(sudo_context, ccache, &creds))) {
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to store credentials: %s", auth->name,
		  error_message(error));
    } else {
	error = verify_krb_v5_tgt(sudo_context, ccache, auth->name);
    }

    krb5_free_cred_contents(sudo_context, &creds);
    return (error ? AUTH_FAILURE : AUTH_SUCCESS);
}

int
kerb5_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    krb5_context	sudo_context;
    krb5_principal	princ;
    krb5_ccache		ccache;

    sudo_context = ((sudo_krb5_datap) auth->data)->sudo_context;
    princ = ((sudo_krb5_datap) auth->data)->princ;
    ccache = ((sudo_krb5_datap) auth->data)->ccache;

    if (sudo_context) {
	if (ccache)
	    krb5_cc_destroy(sudo_context, ccache);
	if (princ)
	    krb5_free_principal(sudo_context, princ);
	krb5_free_context(sudo_context);
    }

    return(AUTH_SUCCESS);
}

/*
 * This routine with some modification is from the MIT V5B6 appl/bsd/login.c
 *
 * Verify the Kerberos ticket-granting ticket just retrieved for the
 * user.  If the Kerberos server doesn't respond, assume the user is
 * trying to fake us out (since we DID just get a TGT from what is
 * supposedly our KDC). If the host/<host> service is unknown (i.e.,
 * the local keytab doesn't have it), return success but log the error.
 *
 * This needs to run as root (to read the host service ticket).
 *
 * Returns 0 for successful authentication, non-zero for failure.
 */
static int
verify_krb_v5_tgt(sudo_context, ccache, auth_name)
    krb5_context	sudo_context;
    krb5_ccache		ccache;
    char		*auth_name; /* For error reporting */
{
    char		phost[BUFSIZ];
    krb5_error_code	error;
    krb5_principal	princ;
    krb5_data		packet;
    krb5_keyblock	*keyblock = 0;
    krb5_auth_context	auth_context = NULL;

    packet.data = 0;

    /*
     * Get the server principal for the local host.
     * (Use defaults of "host" and canonicalized local name.)
     */
    if ((error = krb5_sname_to_principal(sudo_context, NULL, NULL,
					KRB5_NT_SRV_HST, &princ))) {
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: unable to get host principal: %s", auth_name,
		  error_message(error));
	return(-1);
    }

    /* Extract the name directly. */
    strlcpy(phost, krb5_principal_get_realm(sudo_context, princ),
	    sizeof(phost));

    /*
     * Do we have host/<host> keys?
     * (use default keytab, kvno IGNORE_VNO to get the first match,
     * and enctype is currently ignored anyhow.)
     */
    if ((error = krb5_kt_read_service_key(sudo_context, NULL, princ, 0,
					 ETYPE_DES_CBC_MD5, &keyblock))) {
	/* Keytab or service key does not exist. */
	log_error(NO_EXIT,
		  "%s: host service key not found: %s", auth_name,
		  error_message(error));
	error = 0;
	goto cleanup;
    }
    if (keyblock)
	krb5_free_keyblock(sudo_context, keyblock);

    /* Talk to the kdc and construct the ticket. */
    error = krb5_mk_req(sudo_context, &auth_context, 0, "host", phost,
			NULL, ccache, &packet);
    if (auth_context) {
	krb5_auth_con_free(sudo_context, auth_context);
	auth_context = NULL;	/* setup for rd_req */
    }

    /* Try to use the ticket. */
    if (!error)
	error = krb5_rd_req(sudo_context, &auth_context, &packet, princ,
			    NULL, NULL, NULL);
cleanup:
    if (packet.data)
	krb5_data_free(&packet);
    krb5_free_principal(sudo_context, princ);

    if (error)
	log_error(NO_EXIT|NO_MAIL, 
		  "%s: Cannot verify TGT! Possible attack!: %s", auth_name,
		  error_message(error));
    return(error);
}

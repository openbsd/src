/*
 * Copyright (c) 1999-2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <time.h>
#include <signal.h>

#include "sudo.h"
#include "sudo_auth.h"
#include "insults.h"

#ifndef lint
static const char rcsid[] = "$Sudo: sudo_auth.c,v 1.29 2003/04/16 00:42:10 millert Exp $";
#endif /* lint */

sudo_auth auth_switch[] = {
#ifdef AUTH_STANDALONE
    AUTH_STANDALONE
#else
#  ifndef WITHOUT_PASSWD
    AUTH_ENTRY(0, "passwd", passwd_init, NULL, passwd_verify, NULL)
#  endif
#  if defined(HAVE_GETPRPWNAM) && !defined(WITHOUT_PASSWD)
    AUTH_ENTRY(0, "secureware", secureware_init, NULL, secureware_verify, NULL)
#  endif
#  ifdef HAVE_AFS
    AUTH_ENTRY(0, "afs", NULL, NULL, afs_verify, NULL)
#  endif
#  ifdef HAVE_DCE
    AUTH_ENTRY(0, "dce", NULL, NULL, dce_verify, NULL)
#  endif
#  ifdef HAVE_KERB4
    AUTH_ENTRY(0, "kerb4", kerb4_init, NULL, kerb4_verify, NULL)
#  endif
#  ifdef HAVE_KERB5
    AUTH_ENTRY(0, "kerb5", kerb5_init, NULL, kerb5_verify, kerb5_cleanup)
#  endif
#  ifdef HAVE_SKEY
    AUTH_ENTRY(0, "S/Key", NULL, rfc1938_setup, rfc1938_verify, NULL)
#  endif
#  ifdef HAVE_OPIE
    AUTH_ENTRY(0, "OPIE", NULL, rfc1938_setup, rfc1938_verify, NULL)
#  endif
#endif /* AUTH_STANDALONE */
    AUTH_ENTRY(0, NULL, NULL, NULL, NULL, NULL)
};

int nil_pw;		/* I hate resorting to globals like this... */

void
verify_user(pw, prompt)
    struct passwd *pw;
    char *prompt;
{
    int counter = def_ival(I_PASSWD_TRIES) + 1;
    int success = AUTH_FAILURE;
    int status;
    int flags;
    volatile char *p;
    sudo_auth *auth;
    sigaction_t sa, osa;

    /* Enable suspend during password entry. */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_DFL;
    (void) sigaction(SIGTSTP, &sa, &osa);

    /* Make sure we have at least one auth method. */
    if (auth_switch[0].name == NULL)
    	log_error(0, "%s  %s %s",
	    "There are no authentication methods compiled into sudo!",
	    "If you want to turn off authentication, use the",
	    "--disable-authentication configure option.");

    /* Set FLAG_ONEANDONLY if there is only one auth method. */
    if (auth_switch[1].name == NULL)
	auth_switch[0].flags |= FLAG_ONEANDONLY;

    /* Initialize auth methods and unconfigure the method if necessary. */
    for (auth = auth_switch; auth->name; auth++) {
	if (auth->init && IS_CONFIGURED(auth)) {
	    if (NEEDS_USER(auth))
		set_perms(PERM_USER);

	    status = (auth->init)(pw, &prompt, auth);
	    if (status == AUTH_FAILURE)
		auth->flags &= ~FLAG_CONFIGURED;
	    else if (status == AUTH_FATAL)	/* XXX log */
		exit(1);		/* assume error msg already printed */

	    if (NEEDS_USER(auth))
		set_perms(PERM_ROOT);
	}
    }

    while (--counter) {
	/* Do any per-method setup and unconfigure the method if needed */
	for (auth = auth_switch; auth->name; auth++) {
	    if (auth->setup && IS_CONFIGURED(auth)) {
		if (NEEDS_USER(auth))
		    set_perms(PERM_USER);

		status = (auth->setup)(pw, &prompt, auth);
		if (status == AUTH_FAILURE)
		    auth->flags &= ~FLAG_CONFIGURED;
		else if (status == AUTH_FATAL)	/* XXX log */
		    exit(1);		/* assume error msg already printed */

		if (NEEDS_USER(auth))
		    set_perms(PERM_ROOT);
	    }
	}

	/* Get the password unless the auth function will do it for us */
	nil_pw = 0;
#ifdef AUTH_STANDALONE
	p = prompt;
#else
	p = (char *) tgetpass(prompt, def_ival(I_PASSWD_TIMEOUT) * 60,
	    tgetpass_flags);
	if (!p || *p == '\0')
	    nil_pw = 1;
#endif /* AUTH_STANDALONE */

	/* Call authentication functions. */
	for (auth = auth_switch; p && auth->name; auth++) {
	    if (!IS_CONFIGURED(auth))
		continue;

	    if (NEEDS_USER(auth))
		set_perms(PERM_USER);

	    success = auth->status = (auth->verify)(pw, (char *)p, auth);

	    if (NEEDS_USER(auth))
		set_perms(PERM_ROOT);

	    if (auth->status != AUTH_FAILURE)
		goto cleanup;
	}
#ifndef AUTH_STANDALONE
	if (p)
	    (void) memset(p, 0, strlen(p));
#endif

	/* Exit loop on nil password, but give it a chance to match first. */
	if (nil_pw) {
	    if (counter == def_ival(I_PASSWD_TRIES))
		exit(1);
	    else
		break;
	}

	pass_warn(stderr);
    }

cleanup:
    /* Call cleanup routines. */
    for (auth = auth_switch; auth->name; auth++) {
	if (auth->cleanup && IS_CONFIGURED(auth)) {
	    if (NEEDS_USER(auth))
		set_perms(PERM_USER);

	    status = (auth->cleanup)(pw, auth);
	    if (status == AUTH_FATAL)	/* XXX log */
		exit(1);		/* assume error msg already printed */

	    if (NEEDS_USER(auth))
		set_perms(PERM_ROOT);
	}
    }

    switch (success) {
	case AUTH_SUCCESS:
	    (void) sigaction(SIGTSTP, &osa, NULL);
	    return;
	case AUTH_FAILURE:
	    if (def_flag(I_MAIL_BADPASS) || def_flag(I_MAIL_ALWAYS))
		flags = 0;
	    else
		flags = NO_MAIL;
	    log_error(flags, "%d incorrect password attempt%s",
		def_ival(I_PASSWD_TRIES) - counter,
		(def_ival(I_PASSWD_TRIES) - counter == 1) ? "" : "s");
	case AUTH_FATAL:
	    exit(1);
    }
    /* NOTREACHED */
}

void
pass_warn(fp)
    FILE *fp;
{

#ifdef INSULT
    if (def_flag(I_INSULTS))
	(void) fprintf(fp, "%s\n", INSULT);
    else
#endif
	(void) fprintf(fp, "%s\n", def_str(I_BADPASS_MESSAGE));
}

void
dump_auth_methods()
{
    sudo_auth *auth;

    (void) fputs("Authentication methods:", stdout);
    for (auth = auth_switch; auth->name; auth++)
        (void) printf(" '%s'", auth->name);
    (void) putchar('\n');
}

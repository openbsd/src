/*
 * Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
# include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pwd.h>
#ifdef HAVE_GETSPNAM
# include <shadow.h>
#endif /* HAVE_GETSPNAM */
#ifdef HAVE_GETPRPWNAM
# ifdef __hpux
#  undef MAXINT
#  include <hpsecurity.h>
# else
#  include <sys/security.h>
# endif /* __hpux */
# include <prot.h>
#endif /* HAVE_GETPRPWNAM */
#ifdef HAVE_GETPWANAM
# include <sys/label.h>
# include <sys/audit.h>
# include <pwdadj.h>
#endif /* HAVE_GETPWANAM */
#ifdef HAVE_GETAUTHUID
# include <auth.h>
#endif /* HAVE_GETAUTHUID */

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: getspwuid.c,v 1.56 2000/02/18 17:56:26 millert Exp $";
#endif /* lint */

#ifndef STDC_HEADERS
extern char *getenv     __P((const char *));
#endif /* !STDC_HEADERS */

/*
 * Global variables (yuck)
 */
#if defined(HAVE_GETPRPWNAM) && defined(__alpha)
int crypt_type = INT_MAX;
#endif /* HAVE_GETPRPWNAM && __alpha */


/*
 * Local functions not visible outside getspwuid.c
 */
static char *sudo_getshell	__P((struct passwd *));


/*
 * Return the user's shell based on either the SHELL
 * environment variable or the passwd(5) entry (in that order).
 */
static char *
sudo_getshell(pw)
    struct passwd *pw;
{
    char *pw_shell;

    if ((pw_shell = getenv("SHELL")) == NULL)
	pw_shell = pw->pw_shell;

#ifdef _PATH_BSHELL
    /* empty string "" means bourne shell */
    if (*pw_shell == '\0')
	pw_shell = _PATH_BSHELL;
#endif /* _PATH_BSHELL */

    return(pw_shell);
}

/*
 * Return the encrypted password for the user described by pw.  If shadow
 * passwords are in use, look in the shadow file.
 */
char *
sudo_getepw(pw)
    struct passwd *pw;
{

    /* If there is a function to check for shadow enabled, use it... */
#ifdef HAVE_ISCOMSEC
    if (!iscomsec())
	return(pw->pw_passwd);
#endif /* HAVE_ISCOMSEC */
#ifdef HAVE_ISSECURE
    if (!issecure())
	return(pw->pw_passwd);
#endif /* HAVE_ISSECURE */

#ifdef HAVE_GETPRPWNAM
    {
	struct pr_passwd *spw;

	spw = getprpwnam(pw->pw_name);
	if (spw != NULL && spw->ufld.fd_encrypt != NULL) {
# ifdef __alpha
	    crypt_type = spw->ufld.fd_oldcrypt;
# endif /* __alpha */
	    return(spw->ufld.fd_encrypt);
	}
    }
#endif /* HAVE_GETPRPWNAM */
#ifdef HAVE_GETSPNAM
    {
	struct spwd *spw;

	if ((spw = getspnam(pw->pw_name)) && spw->sp_pwdp)
	    return(spw->sp_pwdp);
    }
#endif /* HAVE_GETSPNAM */
#ifdef HAVE_GETSPWUID
    {
	struct s_passwd *spw;

	if ((spw = getspwuid(pw->pw_uid)) && spw->pw_passwd)
	    return(spw->pw_passwd);
    }
#endif /* HAVE_GETSPWUID */
#ifdef HAVE_GETPWANAM
    {
	struct passwd_adjunct *spw;

	if ((spw = getpwanam(pw->pw_name)) && spw->pwa_passwd)
	    return(spw->pwa_passwd);
    }
#endif /* HAVE_GETPWANAM */
#ifdef HAVE_GETAUTHUID
    {
	AUTHORIZATION *spw;

	if ((spw = getauthuid(pw->pw_uid)) && spw->a_password)
	    return(spw->a_password);
    }
#endif /* HAVE_GETAUTHUID */

    /* Fall back on normal password. */
    return(pw->pw_passwd);
}

/*
 * Dynamically allocate space for a struct password and the constituent parts
 * that we care about.  Fills in pw_passwd from shadow file if necessary.
 */
struct passwd *
sudo_getpwuid(uid)
    uid_t uid;
{
    struct passwd *pw, *local_pw;

    if ((pw = getpwuid(uid)) == NULL)
	return(NULL);

    /* Allocate space for a local copy of pw. */
    local_pw = (struct passwd *) emalloc(sizeof(struct passwd));

    /*
     * Copy the struct passwd and the interesting strings...
     */
    (void) memcpy(local_pw, pw, sizeof(struct passwd));
    local_pw->pw_name = estrdup(pw->pw_name);
    local_pw->pw_dir = estrdup(pw->pw_dir);

    /* pw_shell is a special case since we overide with $SHELL */
    local_pw->pw_shell = estrdup(sudo_getshell(pw));

    /* pw_passwd gets a shadow password if applicable */
    local_pw->pw_passwd = estrdup(sudo_getepw(pw));

    return(local_pw);
}

/*
 * Copyright (c) 1996, 1998-2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <sys/stat.h>
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
static const char rcsid[] = "$Sudo: getspwuid.c,v 1.63 2003/04/16 00:42:10 millert Exp $";
#endif /* lint */

/*
 * Global variables (yuck)
 */
#if defined(HAVE_GETPRPWNAM) && defined(__alpha)
int crypt_type = INT_MAX;
#endif /* HAVE_GETPRPWNAM && __alpha */


/*
 * Local functions not visible outside getspwuid.c
 */
static struct passwd *sudo_pwdup	__P((struct passwd *));


/*
 * Return a copy of the encrypted password for the user described by pw.
 * If shadow passwords are in use, look in the shadow file.
 */
char *
sudo_getepw(pw)
    struct passwd *pw;
{
    char *epw;

    /* If there is a function to check for shadow enabled, use it... */
#ifdef HAVE_ISCOMSEC
    if (!iscomsec())
	return(estrdup(pw->pw_passwd));
#endif /* HAVE_ISCOMSEC */
#ifdef HAVE_ISSECURE
    if (!issecure())
	return(estrdup(pw->pw_passwd));
#endif /* HAVE_ISSECURE */

    epw = NULL;
#ifdef HAVE_GETPRPWNAM
    {
	struct pr_passwd *spw;

	setprpwent();
	if ((spw = getprpwnam(pw->pw_name)) && spw->ufld.fd_encrypt) {
# ifdef __alpha
	    crypt_type = spw->ufld.fd_oldcrypt;
# endif /* __alpha */
	    epw = estrdup(spw->ufld.fd_encrypt);
	}
	endprpwent();
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETPRPWNAM */
#ifdef HAVE_GETSPNAM
    {
	struct spwd *spw;

	setspent();
	if ((spw = getspnam(pw->pw_name)) && spw->sp_pwdp)
	    epw = estrdup(spw->sp_pwdp);
	endspent();
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETSPNAM */
#ifdef HAVE_GETSPWUID
    {
	struct s_passwd *spw;

	setspwent();
	if ((spw = getspwuid(pw->pw_uid)) && spw->pw_passwd)
	    epw = estrdup(spw->pw_passwd);
	endspwent();
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETSPWUID */
#ifdef HAVE_GETPWANAM
    {
	struct passwd_adjunct *spw;

	setpwaent();
	if ((spw = getpwanam(pw->pw_name)) && spw->pwa_passwd)
	    epw = estrdup(spw->pwa_passwd);
	endpwaent();
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETPWANAM */
#ifdef HAVE_GETAUTHUID
    {
	AUTHORIZATION *spw;

	setauthent();
	if ((spw = getauthuid(pw->pw_uid)) && spw->a_password)
	    epw = estrdup(spw->a_password);
	endauthent();
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETAUTHUID */

    /* Fall back on normal password. */
    return(estrdup(pw->pw_passwd));
}

/*
 * Dynamically allocate space for a struct password and the constituent parts
 * that we care about.  Fills in pw_passwd from shadow file if necessary.
 */
static struct passwd *
sudo_pwdup(pw)
    struct passwd *pw;
{
    struct passwd *local_pw;

    /* Allocate space for a local copy of pw. */
    local_pw = (struct passwd *) emalloc(sizeof(struct passwd));

    /*
     * Copy the struct passwd and the interesting strings...
     */
    (void) memcpy(local_pw, pw, sizeof(struct passwd));
    local_pw->pw_name = estrdup(pw->pw_name);
    local_pw->pw_dir = estrdup(pw->pw_dir);
    local_pw->pw_gecos = estrdup(pw->pw_gecos);
#ifdef HAVE_LOGIN_CAP_H
    local_pw->pw_class = estrdup(pw->pw_class);
#endif

    /* If shell field is empty, expand to _PATH_BSHELL. */
    if (local_pw->pw_shell[0] == '\0')
	local_pw->pw_shell = _PATH_BSHELL;
    else
	local_pw->pw_shell = estrdup(pw->pw_shell);

    /* pw_passwd gets a shadow password if applicable */
    local_pw->pw_passwd = sudo_getepw(pw);

    return(local_pw);
}

/*
 * Get a password entry by uid and allocate space for it.
 * Fills in pw_passwd from shadow file if necessary.
 */
struct passwd *
sudo_getpwuid(uid)
    uid_t uid;
{
    struct passwd *pw;

    if ((pw = getpwuid(uid)) == NULL)
	return(NULL);
    else
	return(sudo_pwdup(pw));
}

/*
 * Get a password entry by name and allocate space for it.
 * Fills in pw_passwd from shadow file if necessary.
 */
struct passwd *
sudo_getpwnam(name)
    const char *name;
{
    struct passwd *pw;

    if ((pw = getpwnam(name)) == NULL)
	return(NULL);
    else
	return(sudo_pwdup(pw));
}

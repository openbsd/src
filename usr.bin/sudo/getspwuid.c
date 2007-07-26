/*
 * Copyright (c) 1996, 1998-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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
__unused static const char rcsid[] = "$Sudo: getspwuid.c,v 1.65.2.2 2007/06/12 01:28:41 millert Exp $";
#endif /* lint */

/*
 * Global variables (yuck)
 */
#if defined(HAVE_GETPRPWNAM) && defined(__alpha)
int crypt_type = INT_MAX;
#endif /* HAVE_GETPRPWNAM && __alpha */


/*
 * Return a copy of the encrypted password for the user described by pw.
 * If shadow passwords are in use, look in the shadow file.
 */
char *
sudo_getepw(pw)
    const struct passwd *pw;
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
struct passwd *
sudo_pwdup(pw)
    const struct passwd *pw;
{
    char *cp;
    const char *pw_passwd, *pw_shell;
    size_t nsize, psize, csize, gsize, dsize, ssize, total;
    struct passwd *newpw;

    /* Get shadow password if available. */
    pw_passwd = sudo_getepw(pw);

    /* If shell field is empty, expand to _PATH_BSHELL. */
    pw_shell = (pw->pw_shell == NULL || pw->pw_shell[0] == '\0')
	? _PATH_BSHELL : pw->pw_shell;

    /* Allocate in one big chunk for easy freeing. */
    nsize = psize = csize = gsize = dsize = ssize = 0;
    total = sizeof(struct passwd);
    if (pw->pw_name) {
	    nsize = strlen(pw->pw_name) + 1;
	    total += nsize;
    }
    if (pw_passwd) {
	    psize = strlen(pw_passwd) + 1;
	    total += psize;
    }
#ifdef HAVE_LOGIN_CAP_H
    if (pw->pw_class) {
	    csize = strlen(pw->pw_class) + 1;
	    total += csize;
    }
#endif
    if (pw->pw_gecos) {
	    gsize = strlen(pw->pw_gecos) + 1;
	    total += gsize;
    }
    if (pw->pw_dir) {
	    dsize = strlen(pw->pw_dir) + 1;
	    total += dsize;
    }
    if (pw_shell) {
	    ssize = strlen(pw_shell) + 1;
	    total += ssize;
    }
    if ((cp = malloc(total)) == NULL)
	    return (NULL);
    newpw = (struct passwd *)cp;

    /*
     * Copy in passwd contents and make strings relative to space
     * at the end of the buffer.
     */
    (void)memcpy(newpw, pw, sizeof(struct passwd));
    cp += sizeof(struct passwd);
    if (nsize) {
	    (void)memcpy(cp, pw->pw_name, nsize);
	    newpw->pw_name = cp;
	    cp += nsize;
    }
    if (psize) {
	    (void)memcpy(cp, pw_passwd, psize);
	    newpw->pw_passwd = cp;
	    cp += psize;
    }
#ifdef HAVE_LOGIN_CAP_H
    if (csize) {
	    (void)memcpy(cp, pw->pw_class, csize);
	    newpw->pw_class = cp;
	    cp += csize;
    }
#endif
    if (gsize) {
	    (void)memcpy(cp, pw->pw_gecos, gsize);
	    newpw->pw_gecos = cp;
	    cp += gsize;
    }
    if (dsize) {
	    (void)memcpy(cp, pw->pw_dir, dsize);
	    newpw->pw_dir = cp;
	    cp += dsize;
    }
    if (ssize) {
	    (void)memcpy(cp, pw_shell, ssize);
	    newpw->pw_shell = cp;
	    cp += ssize;
    }

    return (newpw);
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

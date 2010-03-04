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
#include <grp.h>
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

/*
 * Exported for auth/secureware.c
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

	if ((spw = getprpwnam(pw->pw_name)) && spw->ufld.fd_encrypt) {
# ifdef __alpha
	    crypt_type = spw->ufld.fd_oldcrypt;
# endif /* __alpha */
	    epw = estrdup(spw->ufld.fd_encrypt);
	}
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETPRPWNAM */
#ifdef HAVE_GETSPNAM
    {
	struct spwd *spw;

	if ((spw = getspnam(pw->pw_name)) && spw->sp_pwdp)
	    epw = estrdup(spw->sp_pwdp);
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETSPNAM */
#ifdef HAVE_GETSPWUID
    {
	struct s_passwd *spw;

	if ((spw = getspwuid(pw->pw_uid)) && spw->pw_passwd)
	    epw = estrdup(spw->pw_passwd);
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETSPWUID */
#ifdef HAVE_GETPWANAM
    {
	struct passwd_adjunct *spw;

	if ((spw = getpwanam(pw->pw_name)) && spw->pwa_passwd)
	    epw = estrdup(spw->pwa_passwd);
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETPWANAM */
#ifdef HAVE_GETAUTHUID
    {
	AUTHORIZATION *spw;

	if ((spw = getauthuid(pw->pw_uid)) && spw->a_password)
	    epw = estrdup(spw->a_password);
	if (epw)
	    return(epw);
    }
#endif /* HAVE_GETAUTHUID */

    /* Fall back on normal password. */
    return(estrdup(pw->pw_passwd));
}

void
sudo_setspent()
{
#ifdef HAVE_GETPRPWNAM
    setprpwent();
#endif
#ifdef HAVE_GETSPNAM
    setspent();
#endif
#ifdef HAVE_GETSPWUID
    setspwent();
#endif
#ifdef HAVE_GETPWANAM
    setpwaent();
#endif
#ifdef HAVE_GETAUTHUID
    setauthent();
#endif
}

void
sudo_endspent()
{
#ifdef HAVE_GETPRPWNAM
    endprpwent();
#endif
#ifdef HAVE_GETSPNAM
    endspent();
#endif
#ifdef HAVE_GETSPWUID
    endspwent();
#endif
#ifdef HAVE_GETPWANAM
    endpwaent();
#endif
#ifdef HAVE_GETAUTHUID
    endauthent();
#endif
}

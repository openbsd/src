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
 *
 * $Sudo: sudo_auth.h,v 1.19 2001/12/14 19:55:01 millert Exp $
 */

#ifndef SUDO_AUTH_H
#define SUDO_AUTH_H

/* Auth function return values.  */
#define AUTH_SUCCESS	0
#define AUTH_FAILURE	1
#define AUTH_FATAL	2

typedef struct sudo_auth {
    short flags;		/* various flags, see below */
    short status;		/* status from verify routine */
    char *name;			/* name of the method as a string */
    VOID *data;			/* method-specific data pointer */
    int (*init) __P((struct passwd *pw, char **prompt, struct sudo_auth *auth));
    int (*setup) __P((struct passwd *pw, char **prompt, struct sudo_auth *auth));
    int (*verify) __P((struct passwd *pw, char *p, struct sudo_auth *auth));
    int (*cleanup) __P((struct passwd *pw, struct sudo_auth *auth));
} sudo_auth;

/* Values for sudo_auth.flags.  */
/* XXX - these names are too long for my liking */
#define FLAG_USER	0x01	/* functions must run as the user, not root */
#define FLAG_CONFIGURED	0x02	/* method configured ok */
#define FLAG_ONEANDONLY	0x04	/* one and only auth method */

/* Shortcuts for using the flags above. */
#define NEEDS_USER(x)		((x)->flags & FLAG_USER)
#define IS_CONFIGURED(x)	((x)->flags & FLAG_CONFIGURED)
#define IS_ONEANDONLY(x)	((x)->flags & FLAG_ONEANDONLY)

/* Prototypes for standalone methods */
int fwtk_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int fwtk_verify __P((struct passwd *pw, char *prompt, sudo_auth *auth));
int fwtk_cleanup __P((struct passwd *pw, sudo_auth *auth));
int pam_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int pam_verify __P((struct passwd *pw, char *prompt, sudo_auth *auth));
int pam_cleanup __P((struct passwd *pw, sudo_auth *auth));
int sia_setup __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int sia_verify __P((struct passwd *pw, char *prompt, sudo_auth *auth));
int sia_cleanup __P((struct passwd *pw, sudo_auth *auth));
int aixauth_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int bsdauth_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int bsdauth_verify __P((struct passwd *pw, char *prompt, sudo_auth *auth));
int bsdauth_cleanup __P((struct passwd *pw, sudo_auth *auth));

/* Prototypes for normal methods */
int passwd_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int passwd_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int secureware_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int secureware_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int rfc1938_setup __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int rfc1938_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int afs_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int dce_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int kerb4_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int kerb4_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int kerb5_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int kerb5_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));
int kerb5_cleanup __P((struct passwd *pw, sudo_auth *auth));
int securid_init __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int securid_setup __P((struct passwd *pw, char **prompt, sudo_auth *auth));
int securid_verify __P((struct passwd *pw, char *pass, sudo_auth *auth));

/* Fields: need_root, name, init, setup, verify, cleanup */
#define AUTH_ENTRY(r, n, i, s, v, c) \
	{ (r|FLAG_CONFIGURED), AUTH_FAILURE, n, NULL, i, s, v, c },

/* Some methods cannots (or should not) interoperate with any others */
#if defined(HAVE_PAM)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "pam", \
	    pam_init, NULL, pam_verify, pam_cleanup)
#elif defined(HAVE_SECURID)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "SecurId", \
	    securid_init, securid_setup, securid_verify, NULL)
#elif defined(HAVE_SIA)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "sia", \
	    NULL, sia_setup, sia_verify, sia_cleanup)
#elif defined(HAVE_AUTHENTICATE)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "aixauth", \
	    NULL, NULL, aixauth_verify, NULL)
#elif defined(HAVE_FWTK)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "fwtk", \
	    fwtk_init, NULL, fwtk_verify, fwtk_cleanup)
#elif defined(HAVE_BSD_AUTH_H)
#  define AUTH_STANDALONE \
	AUTH_ENTRY(0, "bsdauth", \
	    bsdauth_init, NULL, bsdauth_verify, bsdauth_cleanup)
#endif

#endif /* SUDO_AUTH_H */

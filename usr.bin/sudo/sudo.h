/*
 * Copyright (c) 1994-1996,1998-2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * $Sudo: sudo.h,v 1.168 2000/01/17 23:46:26 millert Exp $
 */

#ifndef _SUDO_SUDO_H
#define _SUDO_SUDO_H

#include <pathnames.h>
#include "compat.h"
#include "defaults.h"
#include "logging.h"

/*
 * Info pertaining to the invoking user.
 */
struct sudo_user {
    struct passwd *pw;
    char *tty;
    char  cwd[MAXPATHLEN];
    char *host;
    char *shost;
    char **runas;
    char *prompt;
    char *cmnd_safe;
    char *cmnd;
    char *cmnd_args;
};

/*
 * Return values for sudoers_lookup(), also used as arguments for log_auth()
 * Note: cannot use '0' as a value here.
 */
/* XXX - VALIDATE_SUCCESS and VALIDATE_FAILURE instead? */
#define VALIDATE_ERROR          0x01
#define VALIDATE_OK		0x02
#define VALIDATE_NOT_OK		0x04
#define FLAG_NOPASS		0x10
#define FLAG_NO_USER		0x20
#define FLAG_NO_HOST		0x40
#define FLAG_NO_CHECK		0x80

/*
 * Boolean values
 */
#undef TRUE
#define TRUE                     1
#undef FALSE
#define FALSE                    0

/*
 * find_path()/load_cmnd() return values
 */
#define FOUND                    1
#define NOT_FOUND                0
#define NOT_FOUND_DOT		-1

/*
 * Various modes sudo can be in (based on arguments) in octal
 */
#define MODE_RUN                 00001
#define MODE_VALIDATE            00002
#define MODE_INVALIDATE          00004
#define MODE_KILL                00010
#define MODE_VERSION             00020
#define MODE_HELP                00040
#define MODE_LIST                00100
#define MODE_LISTDEFS            00200
#define MODE_BACKGROUND          00400
#define MODE_SHELL               01000
#define MODE_IMPLIED_SHELL       02000
#define MODE_RESET_HOME          04000

/*
 * Used with set_perms()
 */
#define PERM_ROOT                0x00
#define PERM_USER                0x01
#define PERM_FULL_USER           0x02
#define PERM_SUDOERS             0x03
#define PERM_RUNAS	         0x04

/*
 * Shortcuts for sudo_user contents.
 */
#define user_name		(sudo_user.pw->pw_name)
#define user_passwd		(sudo_user.pw->pw_passwd)
#define user_uid		(sudo_user.pw->pw_uid)
#define user_gid		(sudo_user.pw->pw_gid)
#define user_shell		(sudo_user.pw->pw_shell)
#define user_dir		(sudo_user.pw->pw_dir)
#define user_tty		(sudo_user.tty)
#define user_cwd		(sudo_user.cwd)
#define user_runas		(sudo_user.runas)
#define user_cmnd		(sudo_user.cmnd)
#define user_args		(sudo_user.cmnd_args)
#define user_prompt		(sudo_user.prompt)
#define user_host		(sudo_user.host)
#define user_shost		(sudo_user.shost)
#define safe_cmnd		(sudo_user.cmnd_safe)

/*
 * We used to use the system definition of PASS_MAX or _PASSWD_LEN,
 * but that caused problems with various alternate authentication
 * methods.  So, we just define our own and assume that it is >= the
 * system max.
 */
#define SUDO_PASS_MAX	256

/*
 * Flags for lock_file()
 */
#define SUDO_LOCK	1		/* lock a file */
#define SUDO_TLOCK	2		/* test & lock a file (non-blocking) */
#define SUDO_UNLOCK	4		/* unlock a file */

/*
 * Flags for sudoers_lookup:
 *  PASSWD_NEVER:  user never has to give a passwd
 *  PASSWD_ALL:    no passwd needed if all entries for host have NOPASSWD flag
 *  PASSWD_ANY:    no passwd needed if any entry for host has a NOPASSWD flag
 *  PASSWD_ALWAYS: passwd always needed
 */
#define PWCHECK_NEVER	0x01
#define PWCHECK_ALL	0x02
#define PWCHECK_ANY	0x04
#define PWCHECK_ALWAYS	0x08

/*
 * Function prototypes
 */
#define YY_DECL int yylex __P((void))

#ifndef HAVE_GETCWD
char *getcwd		__P((char *, size_t size));
#endif
#if !defined(HAVE_PUTENV) && !defined(HAVE_SETENV)
int putenv		__P((const char *));
#endif
#ifndef HAVE_SNPRINTF
int snprintf		__P((char *, size_t, const char *, ...));
#endif
#ifndef HAVE_VSNPRINTF
int vsnprintf		__P((char *, size_t, const char *, va_list));
#endif
#ifndef HAVE_ASPRINTF
int asprintf		__P((char **, const char *, ...));
#endif
#ifndef HAVE_VASPRINTF
int vasprintf		__P((char **, const char *, va_list));
#endif
#ifndef HAVE_STRCASECMP
int strcasecmp		__P((const char *, const char *));
#endif
char *sudo_goodpath	__P((const char *));
int sudo_setenv		__P((char *, char *));
char *tgetpass		__P((const char *, int, int));
int find_path		__P((char *, char **));
void check_user		__P((void));
void verify_user	__P((char *));
int sudoers_lookup	__P((int));
void set_perms		__P((int, int));
void remove_timestamp	__P((int));
int check_secureware	__P((char *));
void sia_attempt_auth	__P((void));
void pam_attempt_auth	__P((void));
int yyparse		__P((void));
void pass_warn		__P((FILE *));
VOID *emalloc		__P((size_t));
VOID *erealloc		__P((VOID *, size_t));
char *estrdup		__P((const char *));
void easprintf		__P((char **, const char *, ...));
void evasprintf		__P((char **, const char *, va_list));
void dump_defaults	__P((void));
void dump_auth_methods	__P((void));
int lock_file		__P((int, int));
int touch		__P((char *, time_t));
void set_fqdn		__P((void));
YY_DECL;

/* Only provide extern declarations outside of sudo.c. */
#ifndef _SUDO_SUDO_C
extern struct sudo_user sudo_user;

extern int Argc;
extern char **Argv;
extern FILE *sudoers_fp;
#endif
extern int errno;

#endif /* _SUDO_SUDO_H */

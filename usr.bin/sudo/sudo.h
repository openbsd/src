/*
 * Copyright (c) 1993-1996,1998-2007 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * $Sudo: sudo.h,v 1.209.2.11 2007/09/13 23:06:51 millert Exp $
 */

#ifndef _SUDO_SUDO_H
#define _SUDO_SUDO_H

#include <pathnames.h>
#include <limits.h>
#include "compat.h"
#include "defaults.h"
#include "logging.h"

/*
 * Info pertaining to the invoking user.
 */
struct sudo_user {
    struct passwd *pw;
    struct passwd *_runas_pw;
    struct stat *cmnd_stat;
    char *path;
    char *shell;
    char *tty;
    char *ttypath;
    char  cwd[PATH_MAX];
    char *host;
    char *shost;
    char **runas;
    char *prompt;
    char *cmnd;
    char *cmnd_args;
    char *cmnd_base;
    char *cmnd_safe;
    char *class_name;
    int ngroups;
    gid_t *groups;
    struct list_member *env_vars;
};

/*
 * Return values for sudoers_lookup(), also used as arguments for log_auth()
 * Note: cannot use '0' as a value here.
 */
/* XXX - VALIDATE_SUCCESS and VALIDATE_FAILURE instead? */
#define VALIDATE_ERROR          0x001
#define VALIDATE_OK		0x002
#define VALIDATE_NOT_OK		0x004
#define FLAG_CHECK_USER		0x010
#define FLAG_NOPASS		0x020
#define FLAG_NO_USER		0x040
#define FLAG_NO_HOST		0x080
#define FLAG_NO_CHECK		0x100
#define FLAG_NOEXEC		0x200
#define FLAG_SETENV		0x400

/*
 * Pseudo-boolean values
 */
#undef TRUE
#define TRUE                     1
#undef FALSE
#define FALSE                    0
#undef NOMATCH
#define NOMATCH                 -1
#undef UNSPEC
#define UNSPEC                  -2

/*
 * find_path()/load_cmnd() return values
 */
#define FOUND                    1
#define NOT_FOUND                0
#define NOT_FOUND_DOT		-1

/*
 * Various modes sudo can be in (based on arguments) in hex
 */
#define MODE_RUN		0x0001
#define MODE_EDIT		0x0002
#define MODE_VALIDATE		0x0004
#define MODE_INVALIDATE		0x0008
#define MODE_KILL		0x0010
#define MODE_VERSION		0x0020
#define MODE_HELP		0x0040
#define MODE_LIST		0x0080
#define MODE_LISTDEFS		0x0100
#define MODE_BACKGROUND		0x0200
#define MODE_SHELL		0x0400
#define MODE_LOGIN_SHELL	0x0800
#define MODE_IMPLIED_SHELL	0x1000
#define MODE_RESET_HOME		0x2000
#define MODE_PRESERVE_GROUPS	0x4000
#define MODE_PRESERVE_ENV	0x8000

/*
 * Used with set_perms()
 */
#define PERM_ROOT                0x00
#define PERM_USER                0x01
#define PERM_FULL_USER           0x02
#define PERM_SUDOERS             0x03
#define PERM_RUNAS               0x04
#define PERM_FULL_RUNAS          0x05
#define PERM_TIMESTAMP           0x06

/*
 * Shortcuts for sudo_user contents.
 */
#define user_name		(sudo_user.pw->pw_name)
#define user_passwd		(sudo_user.pw->pw_passwd)
#define user_uid		(sudo_user.pw->pw_uid)
#define user_gid		(sudo_user.pw->pw_gid)
#define user_dir		(sudo_user.pw->pw_dir)
#define user_shell		(sudo_user.shell)
#define user_ngroups		(sudo_user.ngroups)
#define user_groups		(sudo_user.groups)
#define user_tty		(sudo_user.tty)
#define user_ttypath		(sudo_user.ttypath)
#define user_cwd		(sudo_user.cwd)
#define user_runas		(sudo_user.runas)
#define user_cmnd		(sudo_user.cmnd)
#define user_args		(sudo_user.cmnd_args)
#define user_base		(sudo_user.cmnd_base)
#define user_stat		(sudo_user.cmnd_stat)
#define user_path		(sudo_user.path)
#define user_prompt		(sudo_user.prompt)
#define user_host		(sudo_user.host)
#define user_shost		(sudo_user.shost)
#define safe_cmnd		(sudo_user.cmnd_safe)
#define login_class		(sudo_user.class_name)
#define runas_pw		(sudo_user._runas_pw)

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
 * Flags for tgetpass()
 */
#define TGP_ECHO	0x01		/* leave echo on when reading passwd */
#define TGP_STDIN	0x02		/* read from stdin, not /dev/tty */

struct passwd;
struct timespec;
struct timeval;

/*
 * Function prototypes
 */
#define YY_DECL int yylex __P((void))

#ifndef HAVE_CLOSEFROM
void closefrom		__P((int));
#endif
#ifndef HAVE_GETCWD
char *getcwd		__P((char *, size_t size));
#endif
#ifndef HAVE_UTIMES
int utimes		__P((const char *, const struct timeval *));
#endif
#ifdef HAVE_FUTIME
int futimes		__P((int, const struct timeval *));
#endif
#ifndef HAVE_SNPRINTF
int snprintf		__P((char *, size_t, const char *, ...))
			    __printflike(3, 4);
#endif
#ifndef HAVE_VSNPRINTF
int vsnprintf		__P((char *, size_t, const char *, va_list))
			    __printflike(3, 0);
#endif
#ifndef HAVE_ASPRINTF
int asprintf		__P((char **, const char *, ...))
			    __printflike(2, 3);
#endif
#ifndef HAVE_VASPRINTF
int vasprintf		__P((char **, const char *, va_list))
			    __printflike(2, 0);
#endif
#ifndef HAVE_STRCASECMP
int strcasecmp		__P((const char *, const char *));
#endif
#ifndef HAVE_STRLCAT
size_t strlcat		__P((char *, const char *, size_t));
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy		__P((char *, const char *, size_t));
#endif
#ifndef HAVE_MEMRCHR
VOID *memrchr		__P((const VOID *, int, size_t));
#endif
#ifndef HAVE_MKSTEMP
int mkstemp		__P((char *));
#endif
char *sudo_goodpath	__P((const char *, struct stat *));
char *tgetpass		__P((const char *, int, int));
int find_path		__P((char *, char **, struct stat *, char *));
void check_user		__P((int));
void verify_user	__P((struct passwd *, char *));
int sudoers_lookup	__P((int));
#ifdef HAVE_LDAP
int sudo_ldap_check	__P((int));
void sudo_ldap_list_matches __P((void));
#endif
void set_perms		__P((int));
void remove_timestamp	__P((int));
int check_secureware	__P((char *));
void sia_attempt_auth	__P((void));
void pam_attempt_auth	__P((void));
int yyparse		__P((void));
void pass_warn		__P((FILE *));
VOID *emalloc		__P((size_t));
VOID *emalloc2		__P((size_t, size_t));
VOID *erealloc		__P((VOID *, size_t));
VOID *erealloc3		__P((VOID *, size_t, size_t));
char *estrdup		__P((const char *));
int easprintf		__P((char **, const char *, ...))
			    __printflike(2, 3);
int evasprintf		__P((char **, const char *, va_list))
			    __printflike(2, 0);
void efree		__P((VOID *));
void dump_defaults	__P((void));
void dump_auth_methods	__P((void));
void init_envtables	__P((void));
int lock_file		__P((int, int));
int touch		__P((int, char *, struct timespec *));
int user_is_exempt	__P((void));
void set_fqdn		__P((void));
int set_runaspw		__P((char *));
char *sudo_getepw	__P((const struct passwd *));
int pam_prep_user	__P((struct passwd *));
void zero_bytes		__P((volatile VOID *, size_t));
int gettime		__P((struct timespec *));
YY_DECL;

/* Only provide extern declarations outside of sudo.c. */
#ifndef _SUDO_MAIN
extern struct sudo_user sudo_user;
extern struct passwd *auth_pw;

extern FILE *sudoers_fp;
extern int tgetpass_flags;
extern uid_t timestamp_uid;
#endif
#ifndef errno
extern int errno;
#endif

#endif /* _SUDO_SUDO_H */

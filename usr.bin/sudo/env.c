/*
 * Copyright (c) 2000-2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
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
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <pwd.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: env.c,v 1.42 2004/09/08 15:57:49 millert Exp $";
#endif /* lint */

/*
 * Flags used in rebuild_env()
 */
#undef DID_TERM
#define DID_TERM	0x01
#undef DID_PATH
#define DID_PATH	0x02
#undef DID_HOME
#define DID_HOME	0x04
#undef DID_SHELL
#define DID_SHELL	0x08
#undef DID_LOGNAME
#define DID_LOGNAME	0x10
#undef DID_USER
#define DID_USER    	0x12

#undef VNULL
#define	VNULL	(VOID *)NULL

/*
 * Prototypes
 */
char **rebuild_env		__P((char **, int, int));
char **zero_env			__P((char **));
static void insert_env		__P((char *, int));
static char *format_env		__P((char *, ...));

/*
 * Default table of "bad" variables to remove from the environment.
 * XXX - how to omit TERMCAP if it starts with '/'?
 */
static const char *initial_badenv_table[] = {
    "IFS",
    "CDPATH",
    "LOCALDOMAIN",
    "RES_OPTIONS",
    "HOSTALIASES",
    "NLSPATH",
    "PATH_LOCALE",
    "LD_*",
    "_RLD*",
#ifdef __hpux
    "SHLIB_PATH",
#endif /* __hpux */
#ifdef _AIX
    "LIBPATH",
#endif /* _AIX */
#ifdef __APPLE__
    "DYLD_*",
#endif
#ifdef HAVE_KERB4
    "KRB_CONF*",
    "KRBCONFDIR",
    "KRBTKFILE",
#endif /* HAVE_KERB4 */
#ifdef HAVE_KERB5
    "KRB5_CONFIG*",
#endif /* HAVE_KERB5 */
#ifdef HAVE_SECURID
    "VAR_ACE",
    "USR_ACE",
    "DLC_ACE",
#endif /* HAVE_SECURID */
    "TERMINFO",
    "TERMINFO_DIRS",
    "TERMPATH",
    "TERMCAP",			/* XXX - only if it starts with '/' */
    "ENV",
    "BASH_ENV",
    NULL
};

/*
 * Default table of variables to check for '%' and '/' characters.
 */
static const char *initial_checkenv_table[] = {
    "LC_*",
    "LANG",
    "LANGUAGE",
    NULL
};

static char **new_environ;	/* Modified copy of the environment */
static size_t env_size;		/* size of new_environ in char **'s */
static size_t env_len;		/* number of slots used, not counting NULL */

/*
 * Zero out environment and replace with a minimal set of KRB5CCNAME
 * USER, LOGNAME, HOME, TZ, PATH (XXX - should just set path to default)
 * May set user_path, user_shell, and/or user_prompt as side effects.
 */
char **
zero_env(envp)
    char **envp;
{
    static char *newenv[9];
    char **ep, **nep = newenv;
    char **ne_last = &newenv[(sizeof(newenv) / sizeof(newenv[0])) - 1];
    extern char *prev_user;

    for (ep = envp; *ep; ep++) {
	switch (**ep) {
	    case 'H':
		if (strncmp("HOME=", *ep, 5) == 0)
		    break;
		continue;
	    case 'K':
		if (strncmp("KRB5CCNAME=", *ep, 11) == 0)
		    break;
		continue;
	    case 'L':
		if (strncmp("LOGNAME=", *ep, 8) == 0)
		    break;
		continue;
	    case 'P':
		if (strncmp("PATH=", *ep, 5) == 0) {
		    user_path = *ep + 5;
		    /* XXX - set to sane default instead of user's? */
		    break;
		}
		continue;
	    case 'S':
		if (strncmp("SHELL=", *ep, 6) == 0)
		    user_shell = *ep + 6;
		else if (!user_prompt && strncmp("SUDO_PROMPT=", *ep, 12) == 0)
		    user_prompt = *ep + 12;
		else if (strncmp("SUDO_USER=", *ep, 10) == 0)
		    prev_user = *ep + 10;
		continue;
	    case 'T':
		if (strncmp("TZ=", *ep, 3) == 0)
		    break;
		continue;
	    case 'U':
		if (strncmp("USER=", *ep, 5) == 0)
		    break;
		continue;
	    default:
		continue;
	}

	/* Deal with multiply defined variables (take first instance) */
	for (nep = newenv; *nep; nep++) {
	    if (**nep == **ep)
		break;
	}
	if (*nep == NULL) {
	    if (nep < ne_last)
		*nep++ = *ep;
	    else
		errx(1, "internal error, attempt to write outside newenv");
	}
    }

#ifdef HAVE_LDAP
    /*
     * Prevent OpenLDAP from reading any user dotfiles
     * or files in the current directory.
     *
     */	     
    if (nep < ne_last)
	*nep++ = "LDAPNOINIT=1";
    else
	errx(1, "internal error, attempt to write outside newenv");
#endif

    return(&newenv[0]);
}

/*
 * Given a variable and value, allocate and format an environment string.
 */
static char *
#ifdef __STDC__
format_env(char *var, ...)
#else
format_env(var, va_alist)
    char *var;
    va_dcl
#endif
{
    char *estring;
    char *val;
    size_t esize;
    va_list ap;

#ifdef __STDC__
    va_start(ap, var);
#else
    va_start(ap);
#endif
    esize = strlen(var) + 2;
    while ((val = va_arg(ap, char *)) != NULL)
	esize += strlen(val);
    va_end(ap);
    estring = (char *) emalloc(esize);

    /* Store variable name and the '=' separator.  */
    if (strlcpy(estring, var, esize) >= esize ||
	strlcat(estring, "=", esize) >= esize) {

	errx(1, "internal error, format_env() overflow");
    }

    /* Now store the variable's value (if any) */
#ifdef __STDC__
    va_start(ap, var);
#else
    va_start(ap);
#endif
    while ((val = va_arg(ap, char *)) != NULL) {
	if (strlcat(estring, val, esize) >= esize)
	    errx(1, "internal error, format_env() overflow");
    }
    va_end(ap);

    return(estring);
}

/*
 * Insert str into new_environ, assumes str has an '=' in it.
 * NOTE: no other routines may modify new_environ, env_size, or env_len.
 */
static void
insert_env(str, dupcheck)
    char *str;
    int dupcheck;
{
    char **nep;
    size_t varlen;

    /* Make sure there is room for the new entry plus a NULL. */
    if (env_len + 2 > env_size) {
	env_size += 128;
	new_environ = erealloc3(new_environ, env_size, sizeof(char *));
    }

    if (dupcheck) {
	    varlen = (strchr(str, '=') - str) + 1;

	    for (nep = new_environ; *nep; nep++) {
		if (strncmp(str, *nep, varlen) == 0) {
		    *nep = str;
		    return;
		}
	    }
    } else
	nep = &new_environ[env_len];

    env_len++;
    *nep++ = str;
    *nep = NULL;
}

/*
 * Build a new environment and ether clear potentially dangerous
 * variables from the old one or start with a clean slate.
 * Also adds sudo-specific variables (SUDO_*).
 */
char **
rebuild_env(envp, sudo_mode, noexec)
    char **envp;
    int sudo_mode;
    int noexec;
{
    char **ep, *cp, *ps1;
    int okvar, iswild, didvar;
    size_t len;
    struct list_member *cur;

    /*
     * Either clean out the environment or reset to a safe default.
     */
    ps1 = NULL;
    didvar = 0;
    if (def_env_reset) {
	int keepit;

	/* Pull in vars we want to keep from the old environment. */
	for (ep = envp; *ep; ep++) {
	    keepit = 0;

	    /* Skip variables with values beginning with () (bash functions) */
	    if ((cp = strchr(*ep, '=')) != NULL) {
		if (strncmp(cp, "=() ", 3) == 0)
		    continue;
	    }

	    for (cur = def_env_keep; cur; cur = cur->next) {
		len = strlen(cur->value);
		/* Deal with '*' wildcard */
		if (cur->value[len - 1] == '*') {
		    len--;
		    iswild = 1;
		} else
		    iswild = 0;
		if (strncmp(cur->value, *ep, len) == 0 &&
		    (iswild || (*ep)[len] == '=')) {
		    /* We always preserve TERM, no special treatment needed. */
		    if (strncmp(*ep, "TERM=", 5) != 0)
			keepit = 1;
		    break;
		}
	    }

	    /* For SUDO_PS1 -> PS1 conversion. */
	    if (strncmp(*ep, "SUDO_PS1=", 8) == 0)
		ps1 = *ep + 5;

	    if (keepit) {
		/* Preserve variable. */
		switch (**ep) {
		    case 'H':
			if (strncmp(*ep, "HOME=", 5) == 0)
			    SET(didvar, DID_HOME);
			break;
		    case 'S':
			if (strncmp(*ep, "SHELL=", 6) == 0)
			    SET(didvar, DID_SHELL);
			break;
		    case 'L':
			if (strncmp(*ep, "LOGNAME=", 8) == 0)
			    SET(didvar, DID_LOGNAME);
			break;
		    case 'U':
			if (strncmp(*ep, "USER=", 5) == 0)
			    SET(didvar, DID_USER);
			break;
		}
		insert_env(*ep, 0);
	    } else {
		/* Preserve TERM and PATH, ignore anything else. */
		if (!ISSET(didvar, DID_TERM) && strncmp(*ep, "TERM=", 5) == 0) {
		    insert_env(*ep, 0);
		    SET(didvar, DID_TERM);
		} else if (!ISSET(didvar, DID_PATH) && strncmp(*ep, "PATH=", 5) == 0) {
		    insert_env(*ep, 0);
		    SET(didvar, DID_PATH);
		}
	    }
	}

	/*
	 * Add in defaults.  In -i mode these come from the runas user,
	 * otherwise they may be from the user's environment (depends
	 * on sudoers options).
	 */
	if (ISSET(sudo_mode, MODE_LOGIN_SHELL)) {
	    insert_env(format_env("HOME", runas_pw->pw_dir, VNULL), 0);
	    insert_env(format_env("SHELL", runas_pw->pw_shell, VNULL), 0);
	    insert_env(format_env("LOGNAME", runas_pw->pw_name, VNULL), 0);
	    insert_env(format_env("USER", runas_pw->pw_name, VNULL), 0);
	} else {
	    if (!ISSET(didvar, DID_HOME))
		insert_env(format_env("HOME", user_dir, VNULL), 0);
	    if (!ISSET(didvar, DID_SHELL))
		insert_env(format_env("SHELL", sudo_user.pw->pw_shell, VNULL), 0);
	    if (!ISSET(didvar, DID_LOGNAME))
		insert_env(format_env("LOGNAME", user_name, VNULL), 0);
	    if (!ISSET(didvar, DID_USER))
		insert_env(format_env("USER", user_name, VNULL), 0);
	}
    } else {
	/*
	 * Copy envp entries as long as they don't match env_delete or
	 * env_check.
	 */
	for (ep = envp; *ep; ep++) {
	    okvar = 1;

	    /* Skip variables with values beginning with () (bash functions) */
	    if ((cp = strchr(*ep, '=')) != NULL) {
		if (strncmp(cp, "=() ", 3) == 0)
		    continue;
	    }

	    /* Skip anything listed in env_delete. */
	    for (cur = def_env_delete; cur && okvar; cur = cur->next) {
		len = strlen(cur->value);
		/* Deal with '*' wildcard */
		if (cur->value[len - 1] == '*') {
		    len--;
		    iswild = 1;
		} else
		    iswild = 0;
		if (strncmp(cur->value, *ep, len) == 0 &&
		    (iswild || (*ep)[len] == '=')) {
		    okvar = 0;
		}
	    }

	    /* Check certain variables for '%' and '/' characters. */
	    for (cur = def_env_check; cur && okvar; cur = cur->next) {
		len = strlen(cur->value);
		/* Deal with '*' wildcard */
		if (cur->value[len - 1] == '*') {
		    len--;
		    iswild = 1;
		} else
		    iswild = 0;
		if (strncmp(cur->value, *ep, len) == 0 &&
		    (iswild || (*ep)[len] == '=') &&
		    strpbrk(*ep, "/%")) {
		    okvar = 0;
		}
	    }

	    if (okvar) {
		if (strncmp(*ep, "SUDO_PS1=", 9) == 0)
		    ps1 = *ep + 5;
		else if (strncmp(*ep, "PATH=", 5) == 0)
		    SET(didvar, DID_PATH);
		else if (strncmp(*ep, "TERM=", 5) == 0)
		    SET(didvar, DID_TERM);
		insert_env(*ep, 0);
	    }
	}
    }
    /* Provide default values for $TERM and $PATH if they are not set. */
    if (!ISSET(didvar, DID_TERM))
	insert_env("TERM=unknown", 0);
    if (!ISSET(didvar, DID_PATH))
	insert_env(format_env("PATH", _PATH_DEFPATH, VNULL), 0);

#ifdef SECURE_PATH
    /* Replace the PATH envariable with a secure one. */
    insert_env(format_env("PATH", SECURE_PATH, VNULL), 1);
#endif

    /* Set $USER and $LOGNAME to target if "set_logname" is true. */
    if (def_set_logname && runas_pw->pw_name) {
	insert_env(format_env("LOGNAME", runas_pw->pw_name, VNULL), 1);
	insert_env(format_env("USER", runas_pw->pw_name, VNULL), 1);
    }

    /* Set $HOME for `sudo -H'.  Only valid at PERM_FULL_RUNAS. */
    if (ISSET(sudo_mode, MODE_RESET_HOME) && runas_pw->pw_dir)
	insert_env(format_env("HOME", runas_pw->pw_dir, VNULL), 1);

    /*
     * Preload a noexec file?  For a list of LD_PRELOAD-alikes, see
     * http://www.fortran-2000.com/ArnaudRecipes/sharedlib.html
     * XXX - should prepend to original value, if any
     */
    if (noexec && def_noexec_file != NULL)
#if defined(__darwin__) || defined(__APPLE__)
	insert_env(format_env("DYLD_INSERT_LIBRARIES", def_noexec_file, VNULL), 1);
	insert_env(format_env("DYLD_FORCE_FLAT_NAMESPACE", VNULL), 1);
#else
# if defined(__osf__) || defined(__sgi)
	insert_env(format_env("_RLD_LIST", def_noexec_file, ":DEFAULT", VNULL), 1);
# else
	insert_env(format_env("LD_PRELOAD", def_noexec_file, VNULL), 1);
# endif
#endif

    /* Set PS1 if SUDO_PS1 is set. */
    if (ps1)
	insert_env(ps1, 1);

    /* Add the SUDO_COMMAND envariable (cmnd + args). */
    if (user_args)
	insert_env(format_env("SUDO_COMMAND", user_cmnd, " ", user_args, VNULL), 1);
    else
	insert_env(format_env("SUDO_COMMAND", user_cmnd, VNULL), 1);

    /* Add the SUDO_USER, SUDO_UID, SUDO_GID environment variables. */
    insert_env(format_env("SUDO_USER", user_name, VNULL), 1);
    easprintf(&cp, "SUDO_UID=%lu", (unsigned long) user_uid);
    insert_env(cp, 1);
    easprintf(&cp, "SUDO_GID=%lu", (unsigned long) user_gid);
    insert_env(cp, 1);

    return(new_environ);
}

void
init_envtables()
{
    struct list_member *cur;
    const char **p;

    /* Fill in "env_delete" variable. */
    for (p = initial_badenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_env_delete;
	def_env_delete = cur;
    }

    /* Fill in "env_check" variable. */
    for (p = initial_checkenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_env_check;
	def_env_check = cur;
    }
}

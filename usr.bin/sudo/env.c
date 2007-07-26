/*
 * Copyright (c) 2000-2007 Todd C. Miller <Todd.Miller@courtesan.com>
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
__unused static const char rcsid[] = "$Sudo: env.c,v 1.39.2.15 2007/07/09 19:15:43 millert Exp $";
#endif /* lint */

/*
 * Flags used in rebuild_env()
 */
#undef DID_TERM
#define DID_TERM	0x0001
#undef DID_PATH
#define DID_PATH	0x0002
#undef DID_HOME
#define DID_HOME	0x0004
#undef DID_SHELL
#define DID_SHELL	0x0008
#undef DID_LOGNAME
#define DID_LOGNAME	0x0010
#undef DID_USER
#define DID_USER    	0x0020
#undef DID_USERNAME
#define DID_USERNAME   	0x0040
#undef DID_MAX
#define DID_MAX    	0x00ff

#undef KEPT_TERM
#define KEPT_TERM	0x0100
#undef KEPT_PATH
#define KEPT_PATH	0x0200
#undef KEPT_HOME
#define KEPT_HOME	0x0400
#undef KEPT_SHELL
#define KEPT_SHELL	0x0800
#undef KEPT_LOGNAME
#define KEPT_LOGNAME	0x1000
#undef KEPT_USER
#define KEPT_USER    	0x2000
#undef KEPT_USERNAME
#define KEPT_USERNAME	0x4000
#undef KEPT_MAX
#define KEPT_MAX    	0xff00

#undef VNULL
#define	VNULL	(VOID *)NULL

struct environment {
    char **envp;		/* pointer to the new environment */
    size_t env_size;		/* size of new_environ in char **'s */
    size_t env_len;		/* number of slots used, not counting NULL */
};

/*
 * Prototypes
 */
char **rebuild_env		__P((char **, int, int));
static void insert_env		__P((char *, struct environment *, int));
static char *format_env		__P((char *, ...));

/*
 * Copy of the sudo-managed environment.
 */
static struct environment env;

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
    "LDR_*",
    "LIBPATH",
#endif
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
    "KRB5_KTNAME",
#endif /* HAVE_KERB5 */
#ifdef HAVE_SECURID
    "VAR_ACE",
    "USR_ACE",
    "DLC_ACE",
#endif /* HAVE_SECURID */
    "TERMINFO",			/* terminfo, exclusive path to terminfo files */
    "TERMINFO_DIRS",		/* terminfo, path(s) to terminfo files */
    "TERMPATH",			/* termcap, path(s) to termcap files */
    "TERMCAP",			/* XXX - only if it starts with '/' */
    "ENV",			/* ksh, file to source before script runs */
    "BASH_ENV",			/* bash, file to source before script runs */
    "PS4",			/* bash, prefix for lines in xtrace mode */
    "GLOBIGNORE",		/* bash, globbing patterns to ignore */
    "SHELLOPTS",		/* bash, extra command line options */
    "JAVA_TOOL_OPTIONS",	/* java, extra command line options */
    "PERLIO_DEBUG ",		/* perl, debugging output file */
    "PERLLIB",			/* perl, search path for modules/includes */
    "PERL5LIB",			/* perl 5, search path for modules/includes */
    "PERL5OPT",			/* perl 5, extra command line options */
    "PERL5DB",			/* perl 5, command used to load debugger */
    "FPATH",			/* ksh, search path for functions */
    "NULLCMD",			/* zsh, command for null file redirection */
    "READNULLCMD",		/* zsh, command for null file redirection */
    "ZDOTDIR",			/* zsh, search path for dot files */
    "TMPPREFIX",		/* zsh, prefix for temporary files */
    "PYTHONHOME",		/* python, module search path */
    "PYTHONPATH",		/* python, search path */
    "PYTHONINSPEC",		/* python, allow inspection */
    "RUBYLIB",			/* ruby, library load path */
    "RUBYOPT",			/* ruby, extra command line options */
    NULL
};

/*
 * Default table of variables to check for '%' and '/' characters.
 */
static const char *initial_checkenv_table[] = {
    "COLORTERM",
    "LANG",
    "LANGUAGE",
    "LC_*",
    "LINGUAS",
    "TERM",
    NULL
};

/*
 * Default table of variables to preserve in the environment.
 */
static const char *initial_keepenv_table[] = {
    "COLORS",
    "DISPLAY",
    "HOSTNAME",
    "KRB5CCNAME",
    "LS_COLORS",
    "MAIL",
    "PATH",
    "PS1",
    "PS2",
    "TZ",
    "XAUTHORITY",
    "XAUTHORIZATION",
    NULL
};

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
 * Insert str into e->envp, assumes str has an '=' in it.
 */
static void
insert_env(str, e, dupcheck)
    char *str;
    struct environment *e;
    int dupcheck;
{
    char **nep;
    size_t varlen;

    /* Make sure there is room for the new entry plus a NULL. */
    if (e->env_len + 2 > e->env_size) {
	e->env_size += 128;
	e->envp = erealloc3(e->envp, e->env_size, sizeof(char *));
    }

    if (dupcheck) {
	    varlen = (strchr(str, '=') - str) + 1;

	    for (nep = e->envp; *nep; nep++) {
		if (strncmp(str, *nep, varlen) == 0) {
		    *nep = str;
		    return;
		}
	    }
    } else
	nep = e->envp + e->env_len;

    e->env_len++;
    *nep++ = str;
    *nep = NULL;
}

/*
 * Check the env_delete blacklist.
 * Returns TRUE if the variable was found, else false.
 */
static int
matches_env_delete(var)
    const char *var;
{
    struct list_member *cur;
    size_t len;
    int iswild, match = FALSE;

    /* Skip anything listed in env_delete. */
    for (cur = def_env_delete; cur; cur = cur->next) {
	len = strlen(cur->value);
	/* Deal with '*' wildcard */
	if (cur->value[len - 1] == '*') {
	    len--;
	    iswild = TRUE;
	} else
	    iswild = FALSE;
	if (strncmp(cur->value, var, len) == 0 &&
	    (iswild || var[len] == '=')) {
	    match = TRUE;
	    break;
	}
    }
    return(match);
}

/*
 * Apply the env_check list.
 * Returns TRUE if the variable is allowed, FALSE if denied
 * or -1 if no match.
 */
static int
matches_env_check(var)
    const char *var;
{
    struct list_member *cur;
    size_t len;
    int iswild, keepit = -1;

    for (cur = def_env_check; cur; cur = cur->next) {
	len = strlen(cur->value);
	/* Deal with '*' wildcard */
	if (cur->value[len - 1] == '*') {
	    len--;
	    iswild = TRUE;
	} else
	    iswild = FALSE;
	if (strncmp(cur->value, var, len) == 0 &&
	    (iswild || var[len] == '=')) {
	    keepit = !strpbrk(var, "/%");
	    break;
	}
    }
    return(keepit);
}

/*
 * Check the env_keep list.
 * Returns TRUE if the variable is allowed else FALSE.
 */
static int
matches_env_keep(var)
    const char *var;
{
    struct list_member *cur;
    size_t len;
    int iswild, keepit = FALSE;

    for (cur = def_env_keep; cur; cur = cur->next) {
	len = strlen(cur->value);
	/* Deal with '*' wildcard */
	if (cur->value[len - 1] == '*') {
	    len--;
	    iswild = TRUE;
	} else
	    iswild = FALSE;
	if (strncmp(cur->value, var, len) == 0 &&
	    (iswild || var[len] == '=')) {
	    keepit = TRUE;
	    break;
	}
    }
    return(keepit);
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
    unsigned int didvar;

    /*
     * Either clean out the environment or reset to a safe default.
     */
    ps1 = NULL;
    didvar = 0;
    memset(&env, 0, sizeof(env));
    if (def_env_reset) {
	/* Pull in vars we want to keep from the old environment. */
	for (ep = envp; *ep; ep++) {
	    int keepit;

	    /* Skip variables with values beginning with () (bash functions) */
	    if ((cp = strchr(*ep, '=')) != NULL) {
		if (strncmp(cp, "=() ", 3) == 0)
		    continue;
	    }

	    /*
	     * First check certain variables for '%' and '/' characters.
	     * If no match there, check the keep list.
	     * If nothing matched, we remove it from the environment.
	     */
	    keepit = matches_env_check(*ep);
	    if (keepit == -1)
		keepit = matches_env_keep(*ep);

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
		    case 'L':
			if (strncmp(*ep, "LOGNAME=", 8) == 0)
			    SET(didvar, DID_LOGNAME);
			break;
		    case 'P':
			if (strncmp(*ep, "PATH=", 5) == 0)
			    SET(didvar, DID_PATH);
			break;
		    case 'S':
			if (strncmp(*ep, "SHELL=", 6) == 0)
			    SET(didvar, DID_SHELL);
			break;
		    case 'T':
			if (strncmp(*ep, "TERM=", 5) == 0)
			    SET(didvar, DID_TERM);
			break;
		    case 'U':
			if (strncmp(*ep, "USER=", 5) == 0)
			    SET(didvar, DID_USER);
			if (strncmp(*ep, "USERNAME=", 5) == 0)
			    SET(didvar, DID_USERNAME);
			break;
		}
		insert_env(*ep, &env, 0);
	    }
	}
	didvar |= didvar << 8;		/* convert DID_* to KEPT_* */

	/*
	 * Add in defaults.  In -i mode these come from the runas user,
	 * otherwise they may be from the user's environment (depends
	 * on sudoers options).
	 */
	if (ISSET(sudo_mode, MODE_LOGIN_SHELL)) {
	    insert_env(format_env("HOME", runas_pw->pw_dir, VNULL), &env,
		ISSET(didvar, DID_HOME));
	    insert_env(format_env("SHELL", runas_pw->pw_shell, VNULL), &env,
		ISSET(didvar, DID_SHELL));
	    insert_env(format_env("LOGNAME", runas_pw->pw_name, VNULL), &env,
		ISSET(didvar, DID_LOGNAME));
	    insert_env(format_env("USER", runas_pw->pw_name, VNULL), &env,
		ISSET(didvar, DID_USER));
	    insert_env(format_env("USERNAME", runas_pw->pw_name, VNULL), &env,
		ISSET(didvar, DID_USERNAME));
	} else {
	    if (!ISSET(didvar, DID_HOME))
		insert_env(format_env("HOME", user_dir, VNULL), &env, 0);
	    if (!ISSET(didvar, DID_SHELL))
		insert_env(format_env("SHELL", sudo_user.pw->pw_shell, VNULL),
		    &env, 0);
	    if (!ISSET(didvar, DID_LOGNAME))
		insert_env(format_env("LOGNAME", user_name, VNULL), &env, 0);
	    if (!ISSET(didvar, DID_USER))
		insert_env(format_env("USER", user_name, VNULL), &env, 0);
	    if (!ISSET(didvar, DID_USERNAME))
		insert_env(format_env("USERNAME", user_name, VNULL), &env, 0);
	}
    } else {
	/*
	 * Copy envp entries as long as they don't match env_delete or
	 * env_check.
	 */
	for (ep = envp; *ep; ep++) {
	    int okvar;

	    /* Skip variables with values beginning with () (bash functions) */
	    if ((cp = strchr(*ep, '=')) != NULL) {
		if (strncmp(cp, "=() ", 3) == 0)
		    continue;
	    }

	    /*
	     * First check variables against the blacklist in env_delete.
	     * If no match there check for '%' and '/' characters.
	     */
	    okvar = matches_env_delete(*ep) != TRUE;
	    if (okvar)
		okvar = matches_env_check(*ep) != FALSE;

	    if (okvar) {
		if (strncmp(*ep, "SUDO_PS1=", 9) == 0)
		    ps1 = *ep + 5;
		else if (strncmp(*ep, "PATH=", 5) == 0)
		    SET(didvar, DID_PATH);
		else if (strncmp(*ep, "TERM=", 5) == 0)
		    SET(didvar, DID_TERM);
		insert_env(*ep, &env, 0);
	    }
	}
    }

#ifdef SECURE_PATH
    /* Replace the PATH envariable with a secure one. */
    if (!user_is_exempt()) {
	insert_env(format_env("PATH", SECURE_PATH, VNULL), &env, 1);
	SET(didvar, DID_PATH);
    }
#endif

    /* Set $USER, $LOGNAME and $USERNAME to target if "set_logname" is true. */
    if (def_set_logname && runas_pw->pw_name) {
	if (!ISSET(didvar, KEPT_LOGNAME))
	    insert_env(format_env("LOGNAME", runas_pw->pw_name, VNULL), &env, 1);
	if (!ISSET(didvar, KEPT_USER))
	    insert_env(format_env("USER", runas_pw->pw_name, VNULL), &env, 1);
	if (!ISSET(didvar, KEPT_USERNAME))
	    insert_env(format_env("USERNAME", runas_pw->pw_name, VNULL), &env, 1);
    }

    /* Set $HOME for `sudo -H'.  Only valid at PERM_FULL_RUNAS. */
    if (runas_pw->pw_dir) {
	if (ISSET(sudo_mode, MODE_RESET_HOME) ||
	    (ISSET(sudo_mode, MODE_RUN) && (def_always_set_home ||
	    (ISSET(sudo_mode, MODE_SHELL) && def_set_home))))
	    insert_env(format_env("HOME", runas_pw->pw_dir, VNULL), &env, 1);
    }

    /* Provide default values for $TERM and $PATH if they are not set. */
    if (!ISSET(didvar, DID_TERM))
	insert_env("TERM=unknown", &env, 0);
    if (!ISSET(didvar, DID_PATH))
	insert_env(format_env("PATH", _PATH_DEFPATH, VNULL), &env, 0);

    /*
     * Preload a noexec file?  For a list of LD_PRELOAD-alikes, see
     * http://www.fortran-2000.com/ArnaudRecipes/sharedlib.html
     * XXX - should prepend to original value, if any
     */
    if (noexec && def_noexec_file != NULL) {
#if defined(__darwin__) || defined(__APPLE__)
	insert_env(format_env("DYLD_INSERT_LIBRARIES", def_noexec_file, VNULL),
	    &env, 1);
	insert_env(format_env("DYLD_FORCE_FLAT_NAMESPACE", VNULL), &env, 1);
#else
# if defined(__osf__) || defined(__sgi)
	insert_env(format_env("_RLD_LIST", def_noexec_file, ":DEFAULT", VNULL),
	    &env, 1);
# else
#  ifdef _AIX
	insert_env(format_env("LDR_PRELOAD", def_noexec_file, VNULL), &env, 1);
#  else
	insert_env(format_env("LD_PRELOAD", def_noexec_file, VNULL), &env, 1);
#  endif /* _AIX */
# endif /* __osf__ || __sgi */
#endif /* __darwin__ || __APPLE__ */
    }

    /* Set PS1 if SUDO_PS1 is set. */
    if (ps1)
	insert_env(ps1, &env, 1);

    /* Add the SUDO_COMMAND envariable (cmnd + args). */
    if (user_args)
	insert_env(format_env("SUDO_COMMAND", user_cmnd, " ", user_args, VNULL),
	    &env, 1);
    else
	insert_env(format_env("SUDO_COMMAND", user_cmnd, VNULL), &env, 1);

    /* Add the SUDO_USER, SUDO_UID, SUDO_GID environment variables. */
    insert_env(format_env("SUDO_USER", user_name, VNULL), &env, 1);
    easprintf(&cp, "SUDO_UID=%lu", (unsigned long) user_uid);
    insert_env(cp, &env, 1);
    easprintf(&cp, "SUDO_GID=%lu", (unsigned long) user_gid);
    insert_env(cp, &env, 1);

    return(env.envp);
}

char **
insert_env_vars(envp, env_vars)
    char **envp;
    struct list_member *env_vars;
{
    struct list_member *cur;

    if (env_vars == NULL)
	return (envp);

    /*
     * Make sure we still own the environment and steal it back if not.
     */
    if (env.envp != envp) {
	size_t evlen;
	char **ep;

	for (ep = envp; *ep != NULL; ep++)
	    continue;
	evlen = ep - envp;
	if (evlen + 1 > env.env_size) {
	    efree(env.envp);
	    env.env_size = evlen + 1 + 128;
	    env.envp = emalloc2(env.env_size, sizeof(char *));
	}
	memcpy(env.envp, envp, evlen + 1);
	env.env_len = evlen;
    }

    /* Add user-specified environment variables. */
    for (cur = env_vars; cur != NULL; cur = cur->next)
	insert_env(cur->value, &env, 1);

    return(env.envp);
}

/*
 * Validate the list of environment variables passed in on the command
 * line against env_delete, env_check, and env_keep.
 * Calls log_error() if any specified variables are not allowed.
 */
void
validate_env_vars(env_vars)
    struct list_member *env_vars;
{
    struct list_member *var;
    char *eq, *bad = NULL;
    size_t len, blen = 0, bsize = 0;
    int okvar;

    for (var = env_vars; var != NULL; var = var->next) {
#ifdef SECURE_PATH
	if (!user_is_exempt() && strncmp(var->value, "PATH=", 5) == 0) {
	    okvar = FALSE;
	} else
#endif
	if (def_env_reset) {
	    okvar = matches_env_check(var->value);
	    if (okvar == -1)
		okvar = matches_env_keep(var->value);
	} else {
	    okvar = matches_env_delete(var->value) == FALSE;
	    if (okvar == FALSE)
		okvar = matches_env_check(var->value) != FALSE;
	}
	if (okvar == FALSE) {
	    /* Not allowed, add to error string, allocating as needed. */
	    if ((eq = strchr(var->value, '=')) != NULL)
		*eq = '\0';
	    len = strlen(var->value) + 2;
	    if (blen + len >= bsize) {
		do {
		    bsize += 1024;
		} while (blen + len >= bsize);
		bad = erealloc(bad, bsize);
		bad[blen] = '\0';
	    }
	    strlcat(bad, var->value, bsize);
	    strlcat(bad, ", ", bsize);
	    blen += len;
	    if (eq != NULL)
		*eq = '=';
	}
    }
    if (bad != NULL) {
	bad[blen - 2] = '\0';		/* remove trailing ", " */
	log_error(NO_MAIL,
	    "sorry, you are not allowed to set the following environment variables: %s", bad);
	/* NOTREACHED */
	efree(bad);
    }
}

void
init_envtables()
{
    struct list_member *cur;
    const char **p;

    /* Fill in the "env_delete" list. */
    for (p = initial_badenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_env_delete;
	def_env_delete = cur;
    }

    /* Fill in the "env_check" list. */
    for (p = initial_checkenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_env_check;
	def_env_check = cur;
    }

    /* Fill in the "env_keep" list. */
    for (p = initial_keepenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_env_keep;
	def_env_keep = cur;
    }
}

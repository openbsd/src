/*
 * Copyright (c) 2000-2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <pwd.h>
#include <errno.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: env.c,v 1.25 2003/03/15 20:31:01 millert Exp $";
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

/*
 * Prototypes
 */
char **rebuild_env		__P((int, char **));
char **zero_env			__P((char **));
static void insert_env		__P((char *, int));
static char *format_env		__P((char *, char *));

/*
 * Default table of "bad" variables to remove from the environment.
 * XXX - how to omit TERMCAP if it starts with '/'?
 */
static const char *initial_badenv_table[] = {
    "IFS",
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
    "KRBCONFDIR"
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
 * Zero out environment and replace with a minimal set of
 * USER, LOGNAME, HOME, TZ, PATH (XXX - should just set path to default)
 * May set user_path, user_shell, and/or user_prompt as side effects.
 */
char **
zero_env(envp)
    char **envp;
{
    char **ep, **nep;
    static char *newenv[7];

    for (ep = envp; *ep; ep++) {
	switch (**ep) {
	    case 'H':
		if (strncmp("HOME=", *ep, 5) == 0)
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
		else if (!user_prompt && !strncmp("SUDO_PROMPT=", *ep, 12))
		    user_prompt = *ep + 12;
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
	if (*nep == NULL)
	    *nep++ = *ep;
    }
    return(&newenv[0]);
}

/*
 * Given a variable and value, allocate and format an environment string.
 */
static char *
format_env(var, val)
    char *var;
    char *val;
{
    char *estring;
    size_t esize;

    esize = strlen(var) + 1 + strlen(val) + 1;
    estring = (char *) emalloc(esize);

    /* We pre-allocate enough space, so this should never overflow. */
    if (strlcpy(estring, var, esize) >= esize ||
	strlcat(estring, "=", esize) >= esize ||
	strlcat(estring, val, esize) >= esize) {
	(void) fprintf(stderr, "%s: internal error, format_env() overflow\n",
	    Argv[0]);
	exit(1);
    }

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

    /* Make sure there is room for the new entry. */
    if (env_len + 1 > env_size) {
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
rebuild_env(sudo_mode, envp)
    int sudo_mode;
    char **envp;
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
    if (def_flag(I_ENV_RESET)) {
	int keepit;

	/* Pull in vars we want to keep from the old environment. */
	for (ep = envp; *ep; ep++) {
	    keepit = 0;
	    for (cur = def_list(I_ENV_KEEP); cur; cur = cur->next) {
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
			    didvar |= DID_HOME;
			break;
		    case 'S':
			if (strncmp(*ep, "SHELL=", 6) == 0)
			    didvar |= DID_SHELL;
			break;
		    case 'L':
			if (strncmp(*ep, "LOGNAME=", 8) == 0)
			    didvar |= DID_LOGNAME;
			break;
		    case 'U':
			if (strncmp(*ep, "USER=", 5) == 0)
			    didvar |= DID_USER;
			break;
		}
		insert_env(*ep, 0);
	    } else {
		/* Preserve TERM and PATH, ignore anything else. */
		if (!(didvar & DID_TERM) && !strncmp(*ep, "TERM=", 5)) {
		    insert_env(*ep, 0);
		    didvar |= DID_TERM;
		} else if (!(didvar & DID_PATH) && !strncmp(*ep, "PATH=", 5)) {
		    insert_env(*ep, 0);
		    didvar |= DID_PATH;
		}
	    }
	}

	/*
	 * Add in defaults unless they were preserved from the
	 * user's environment.
	 */
	if (!(didvar & DID_HOME))
	    insert_env(format_env("HOME", user_dir), 0);
	if (!(didvar & DID_SHELL))
	    insert_env(format_env("SHELL", sudo_user.pw->pw_shell), 0);
	if (!(didvar & DID_LOGNAME))
	    insert_env(format_env("LOGNAME", user_name), 0);
	if (!(didvar & DID_USER))
	    insert_env(format_env("USER", user_name), 0);
    } else {
	/*
	 * Copy envp entries as long as they don't match env_delete or
	 * env_check.
	 */
	for (ep = envp; *ep; ep++) {
	    okvar = 1;

	    /* Skip anything listed in env_delete. */
	    for (cur = def_list(I_ENV_DELETE); cur && okvar; cur = cur->next) {
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
	    for (cur = def_list(I_ENV_CHECK); cur && okvar; cur = cur->next) {
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
		    didvar |= DID_PATH;
		else if (strncmp(*ep, "TERM=", 5) == 0)
		    didvar |= DID_TERM;
		insert_env(*ep, 0);
	    }
	}
    }
    /* Provide default values for $TERM and $PATH if they are not set. */
    if (!(didvar & DID_TERM))
	insert_env("TERM=unknown", 0);
    if (!(didvar & DID_PATH))
	insert_env(format_env("PATH", _PATH_DEFPATH), 0);

#ifdef SECURE_PATH
    /* Replace the PATH envariable with a secure one. */
    insert_env(format_env("PATH", SECURE_PATH), 1);
#endif

    /* Set $USER and $LOGNAME to target if "set_logname" is true. */
    if (def_flag(I_SET_LOGNAME) && runas_pw->pw_name) {
	insert_env(format_env("LOGNAME", runas_pw->pw_name), 1);
	insert_env(format_env("USER", runas_pw->pw_name), 1);
    }

    /* Set $HOME for `sudo -H'.  Only valid at PERM_RUNAS. */
    if ((sudo_mode & MODE_RESET_HOME) && runas_pw->pw_dir)
	insert_env(format_env("HOME", runas_pw->pw_dir), 1);

    /* Set PS1 if SUDO_PS1 is set. */
    if (ps1)
	insert_env(ps1, 1);

    /* Add the SUDO_COMMAND envariable (cmnd + args). */
    if (user_args) {
	easprintf(&cp, "SUDO_COMMAND=%s %s", user_cmnd, user_args);
	insert_env(cp, 1);
    } else
	insert_env(format_env("SUDO_COMMAND", user_cmnd), 1);

    /* Add the SUDO_USER, SUDO_UID, SUDO_GID environment variables. */
    insert_env(format_env("SUDO_USER", user_name), 1);
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
	cur->next = def_list(I_ENV_DELETE);
	def_list(I_ENV_DELETE) = cur;
    }

    /* Fill in "env_check" variable. */
    for (p = initial_checkenv_table; *p; p++) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = estrdup(*p);
	cur->next = def_list(I_ENV_CHECK);
	def_list(I_ENV_CHECK) = cur;
    }
}

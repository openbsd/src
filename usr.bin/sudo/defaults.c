/*
 * Copyright (c) 1999-2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <sys/types.h>
#include <sys/param.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: defaults.c,v 1.23 2000/03/22 23:40:09 millert Exp $";
#endif /* lint */

/*
 * For converting between syslog numbers and strings.
 */
struct strmap {
    char *name;
    int num;
};

#ifdef LOG_NFACILITIES
static struct strmap facilities[] = {
#ifdef LOG_AUTHPRIV
	{ "authpriv",	LOG_AUTHPRIV },
#endif
	{ "auth",	LOG_AUTH },
	{ "daemon",	LOG_DAEMON },
	{ "user",	LOG_USER },
	{ "local0",	LOG_LOCAL0 },
	{ "local1",	LOG_LOCAL1 },
	{ "local2",	LOG_LOCAL2 },
	{ "local3",	LOG_LOCAL3 },
	{ "local4",	LOG_LOCAL4 },
	{ "local5",	LOG_LOCAL5 },
	{ "local6",	LOG_LOCAL6 },
	{ "local7",	LOG_LOCAL7 },
	{ NULL,		-1 }
};
#endif /* LOG_NFACILITIES */

static struct strmap priorities[] = {
	{ "alert",	LOG_ALERT },
	{ "crit",	LOG_CRIT },
	{ "debug",	LOG_DEBUG },
	{ "emerg",	LOG_EMERG },
	{ "err",	LOG_ERR },
	{ "info",	LOG_INFO },
	{ "notice",	LOG_NOTICE },
	{ "warning",	LOG_WARNING },
	{ NULL,		-1 }
};

extern int sudolineno;

/*
 * Local prototypes.
 */
static int store_int __P((char *, struct sudo_defs_types *, int));
static int store_str __P((char *, struct sudo_defs_types *, int));
static int store_syslogfac __P((char *, struct sudo_defs_types *, int));
static int store_syslogpri __P((char *, struct sudo_defs_types *, int));
static int store_mode __P((char *, struct sudo_defs_types *, int));
static int store_pwflag __P((char *, struct sudo_defs_types *, int));

/*
 * Table describing compile-time and run-time options.
 */
struct sudo_defs_types sudo_defs_table[] = {
    {
	"syslog_ifac", T_INT, NULL
    }, {
	"syslog_igoodpri", T_INT, NULL
    }, {
	"syslog_ibadpri", T_INT, NULL
    }, {
	"syslog", T_LOGFAC|T_BOOL,
	"Syslog facility if syslog is being used for logging: %s"
    }, {
	"syslog_goodpri", T_LOGPRI,
	"Syslog priority to use when user authenticates successfully: %s"
    }, {
	"syslog_badpri", T_LOGPRI,
	"Syslog priority to use when user authenticates unsuccessfully: %s"
    }, {
	"long_otp_prompt", T_FLAG,
	"Put OTP prompt on its own line"
    }, {
	"ignore_dot", T_FLAG,
	"Ignore '.' in $PATH"
    }, {
	"mail_always", T_FLAG,
	"Always send mail when sudo is run"
    }, {
	"mail_no_user", T_FLAG,
	"Send mail if the user is not in sudoers"
    }, {
	"mail_no_host", T_FLAG,
	"Send mail if the user is not in sudoers for this host"
    }, {
	"mail_no_perms", T_FLAG,
	"Send mail if the user is not allowed to run a command"
    }, {
	"tty_tickets", T_FLAG,
	"Use a separate timestamp for each user/tty combo"
    }, {
	"lecture", T_FLAG,
	"Lecture user the first time they run sudo"
    }, {
	"authenticate", T_FLAG,
	"Require users to authenticate by default"
    }, {
	"root_sudo", T_FLAG,
	"Root may run sudo"
    }, {
	"log_host", T_FLAG,
	"Log the hostname in the (non-syslog) log file"
    }, {
	"log_year", T_FLAG,
	"Log the year in the (non-syslog) log file"
    }, {
	"shell_noargs", T_FLAG,
	"If sudo is invoked with no arguments, start a shell"
    }, {
	"set_home", T_FLAG,
	"Set $HOME to the target user when starting a shell with -s"
    }, {
	"path_info", T_FLAG,
	"Allow some information gathering to give useful error messages"
    }, {
	"fqdn", T_FLAG,
	"Require fully-qualified hsotnames in the sudoers file"
    }, {
	"insults", T_FLAG,
	"Insult the user when they enter an incorrect password"
    }, {
	"requiretty", T_FLAG,
	"Only allow the user to run sudo if they have a tty"
    }, {
	"env_editor", T_FLAG,
	"Visudo will honor the EDITOR environment variable"
    }, {
	"rootpw", T_FLAG,
	"Prompt for root's password, not the users's"
    }, {
	"runaspw", T_FLAG,
	"Prompt for the runas_default user's password, not the users's"
    }, {
	"targetpw", T_FLAG,
	"Prompt for the target user's password, not the users's"
    }, {
	"use_loginclass", T_FLAG,
	"Apply defaults in the target user's login class if there is one"
    }, {
	"set_logname", T_FLAG,
	"Set the LOGNAME and USER environment variables"
    }, {
	"loglinelen", T_INT|T_BOOL,
	"Length at which to wrap log file lines (0 for no wrap): %d"
    }, {
	"timestamp_timeout", T_INT|T_BOOL,
	"Authentication timestamp timeout: %d minutes"
    }, {
	"passwd_timeout", T_INT|T_BOOL,
	"Password prompt timeout: %d minutes"
    }, {
	"passwd_tries", T_INT,
	"Number of tries to enter a password: %d"
    }, {
	"umask", T_MODE|T_BOOL,
	"Umask to use or 0777 to use user's: 0%o"
    }, {
	"logfile", T_STR|T_BOOL|T_PATH,
	"Path to log file: %s"
    }, {
	"mailerpath", T_STR|T_BOOL|T_PATH,
	"Path to mail program: %s"
    }, {
	"mailerflags", T_STR|T_BOOL,
	"Flags for mail program: %s"
    }, {
	"mailto", T_STR|T_BOOL,
	"Address to send mail to: %s"
    }, {
	"mailsub", T_STR,
	"Subject line for mail messages: %s"
    }, {
	"badpass_message", T_STR,
	"Incorrect password message: %s"
    }, {
	"timestampdir", T_STR|T_PATH,
	"Path to authentication timestamp dir: %s"
    }, {
	"exempt_group", T_STR|T_BOOL,
	"Users in this group are exempt from password and PATH requirements: %s"
    }, {
	"passprompt", T_STR,
	"Default password prompt: %s"
    }, {
	"runas_default", T_STR,
	"Default user to run commands as: %s"
    }, {
	"secure_path", T_STR|T_BOOL,
	"Value to override user's $PATH with: %s"
    }, {
	"editor", T_STR|T_PATH,
	"Path to the editor for use by visudo: %s"
    }, {
	"listpw_i", T_INT, NULL
    }, {
	"verifypw_i", T_INT, NULL
    }, {
	"listpw", T_PWFLAG,
	"When to require a password for 'list' pseudocommand: %s"
    }, {
	"verifypw", T_PWFLAG,
	"When to require a password for 'verify' pseudocommand: %s"
    }, {
	NULL, 0, NULL
    }
};

/*
 * Print version and configure info.
 */
void
dump_defaults()
{
    struct sudo_defs_types *cur;

    for (cur = sudo_defs_table; cur->name; cur++) {
	if (cur->desc) {
	    switch (cur->type & T_MASK) {
		case T_FLAG:
		    if (cur->sd_un.flag)
			puts(cur->desc);
		    break;
		case T_STR:
		case T_LOGFAC:
		case T_LOGPRI:
		case T_PWFLAG:
		    if (cur->sd_un.str) {
			(void) printf(cur->desc, cur->sd_un.str);
			putchar('\n');
		    }
		    break;
		case T_INT:
		    (void) printf(cur->desc, cur->sd_un.ival);
		    putchar('\n');
		    break;
		case T_MODE:
		    (void) printf(cur->desc, cur->sd_un.mode);
		    putchar('\n');
		    break;
	    }
	}
    }
}

/*
 * List each option along with its description.
 */
void
list_options()
{
    struct sudo_defs_types *cur;
    char *p;

    (void) puts("Available options in a sudoers ``Defaults'' line:\n");
    for (cur = sudo_defs_table; cur->name; cur++) {
	if (cur->name && cur->desc) {
	    switch (cur->type & T_MASK) {
		case T_FLAG:
		    (void) printf("%s: %s\n", cur->name, cur->desc);
		    break;
		default:
		    p = strrchr(cur->desc, ':');
		    if (p)
			(void) printf("%s: %.*s\n", cur->name, p - cur->desc,
			    cur->desc);
		    else
			(void) printf("%s: %s\n", cur->name, cur->desc);
		    break;
	    }
	}
    }
}

/*
 * Sets/clears an entry in the defaults structure
 * If a variable that takes a value is used in a boolean
 * context with op == 0, disable that variable.
 * Eg. you may want to turn off logging to a file for some hosts.
 * This is only meaningful for variables that are *optional*.
 */
int
set_default(var, val, op)
    char *var;
    char *val;
    int op;     /* TRUE or FALSE */
{
    struct sudo_defs_types *cur;
    int num;

    for (cur = sudo_defs_table, num = 0; cur->name; cur++, num++) {
	if (strcmp(var, cur->name) == 0)
	    break;
    }
    if (!cur->name) {
	(void) fprintf(stderr,
	    "%s: unknown defaults entry `%s' referenced near line %d\n", Argv[0],
	    var, sudolineno);
	return(FALSE);
    }

    switch (cur->type & T_MASK) {
	case T_LOGFAC:
	    if (!store_syslogfac(val, cur, op)) {
		if (val)
		    (void) fprintf(stderr,
			"%s: value '%s' is invalid for option '%s'\n", Argv[0],
			val, var);
		else
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_LOGPRI:
	    if (!store_syslogpri(val, cur, op)) {
		if (val)
		    (void) fprintf(stderr,
			"%s: value '%s' is invalid for option '%s'\n", Argv[0],
			val, var);
		else
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_PWFLAG:
	    if (!store_pwflag(val, cur, op)) {
		if (val)
		    (void) fprintf(stderr,
			"%s: value '%s' is invalid for option '%s'\n", Argv[0],
			val, var);
		else
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_STR:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if ((cur->type & T_PATH) && *val != '/') {
		(void) fprintf(stderr,
		    "%s: values for `%s' must start with a '/'\n", Argv[0],
		    var);
		return(FALSE);
	    }
	    if (!store_str(val, cur, op)) {
		(void) fprintf(stderr,
		    "%s: value '%s' is invalid for option '%s'\n", Argv[0],
		    val, var);
		return(FALSE);
	    }
	    break;
	case T_INT:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_int(val, cur, op)) {
		(void) fprintf(stderr,
		    "%s: value '%s' is invalid for option '%s'\n", Argv[0],
		    val, var);
		return(FALSE);
	    }
	    break;
	case T_MODE:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    (void) fprintf(stderr,
			"%s: no value specified for `%s' on line %d\n", Argv[0],
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_mode(val, cur, op)) {
		(void) fprintf(stderr,
		    "%s: value '%s' is invalid for option '%s'\n", Argv[0],
		    val, var);
		return(FALSE);
	    }
	    break;
	case T_FLAG:
	    if (val) {
		(void) fprintf(stderr,
		    "%s: option `%s' does not take a value on line %d\n",
		    Argv[0], var, sudolineno);
		return(FALSE);
	    }
	    cur->sd_un.flag = op;

	    /* Special action for I_FQDN.  Move to own switch if we get more */
	    if (num == I_FQDN && op)
		set_fqdn();
	    break;
    }

    return(TRUE);
}

/*
 * Set default options to compiled-in values.
 * Any of these may be overridden at runtime by a "Defaults" file.
 */
void
init_defaults()
{
    static int firsttime = 1;
    struct sudo_defs_types *def;

    /* Free any strings that were set. */
    if (!firsttime) {
	for (def = sudo_defs_table; def->name; def++)
	    switch (def->type & T_MASK) {
		case T_STR:
		case T_LOGFAC:
		case T_LOGPRI:
		case T_PWFLAG:
		    if (def->sd_un.str) {
			free(def->sd_un.str);
			def->sd_un.str = NULL;
		    }
		    break;
	    }
    }

    /* First initialize the flags. */
#ifdef LONG_OTP_PROMPT
    def_flag(I_LONG_OTP_PROMPT) = TRUE;
#endif
#ifdef IGNORE_DOT_PATH
    def_flag(I_IGNORE_DOT) = TRUE;
#endif
#ifdef ALWAYS_SEND_MAIL
    def_flag(I_MAIL_ALWAYS) = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NO_USER
    def_flag(I_MAIL_NOUSER) = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NO_HOST
    def_flag(I_MAIL_NOHOST) = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NOT_OK
    def_flag(I_MAIL_NOPERMS) = TRUE;
#endif
#ifdef USE_TTY_TICKETS
    def_flag(I_TTY_TICKETS) = TRUE;
#endif
#ifndef NO_LECTURE
    def_flag(I_LECTURE) = TRUE;
#endif
#ifndef NO_AUTHENTICATION
    def_flag(I_AUTHENTICATE) = TRUE;
#endif
#ifndef NO_ROOT_SUDO
    def_flag(I_ROOT_SUDO) = TRUE;
#endif
#ifdef HOST_IN_LOG
    def_flag(I_LOG_HOST) = TRUE;
#endif
#ifdef SHELL_IF_NO_ARGS
    def_flag(I_SHELL_NOARGS) = TRUE;
#endif
#ifdef SHELL_SETS_HOME
    def_flag(I_SET_HOME) = TRUE;
#endif
#ifndef DONT_LEAK_PATH_INFO
    def_flag(I_PATH_INFO) = TRUE;
#endif
#ifdef FQDN
    def_flag(I_FQDN) = TRUE;
#endif
#ifdef USE_INSULTS
    def_flag(I_INSULTS) = TRUE;
#endif
#ifdef ENV_EDITOR
    def_flag(I_ENV_EDITOR) = TRUE;
#endif
    def_flag(I_LOGNAME) = TRUE;

    /* Syslog options need special care since they both strings and ints */
#if (LOGGING & SLOG_SYSLOG)
    (void) store_syslogfac(LOGFAC, &sudo_defs_table[I_LOGFACSTR], TRUE);
    (void) store_syslogpri(PRI_SUCCESS, &sudo_defs_table[I_GOODPRISTR], TRUE);
    (void) store_syslogpri(PRI_FAILURE, &sudo_defs_table[I_BADPRISTR], TRUE);
#endif

    /* Password flags also have a string and integer component. */
    (void) store_pwflag("any", &sudo_defs_table[I_LISTPWSTR], TRUE);
    (void) store_pwflag("all", &sudo_defs_table[I_VERIFYPWSTR], TRUE);

    /* Then initialize the int-like things. */
#ifdef SUDO_UMASK
    def_mode(I_UMASK) = SUDO_UMASK;
#else
    def_mode(I_UMASK) = 0777;
#endif
    def_ival(I_LOGLEN) = MAXLOGFILELEN;
    def_ival(I_TS_TIMEOUT) = TIMEOUT;
    def_ival(I_PW_TIMEOUT) = PASSWORD_TIMEOUT;
    def_ival(I_PW_TRIES) = TRIES_FOR_PASSWORD;

    /* Finally do the strings */
    def_str(I_MAILTO) = estrdup(MAILTO);
    def_str(I_MAILSUB) = estrdup(MAILSUBJECT);
    def_str(I_BADPASS_MSG) = estrdup(INCORRECT_PASSWORD);
    def_str(I_TIMESTAMPDIR) = estrdup(_PATH_SUDO_TIMEDIR);
    def_str(I_PASSPROMPT) = estrdup(PASSPROMPT);
    def_str(I_RUNAS_DEF) = estrdup(RUNAS_DEFAULT);
#ifdef _PATH_SENDMAIL
    def_str(I_MAILERPATH) = estrdup(_PATH_SENDMAIL);
    def_str(I_MAILERFLAGS) = estrdup("-t");
#endif
#if (LOGGING & SLOG_FILE)
    def_str(I_LOGFILE) = estrdup(_PATH_SUDO_LOGFILE);
#endif
#ifdef EXEMPTGROUP
    def_str(I_EXEMPT_GRP) = estrdup(EXEMPTGROUP);
#endif
#ifdef SECURE_PATH
    def_str(I_SECURE_PATH) = estrdup(SECURE_PATH);
#endif
    def_str(I_EDITOR) = estrdup(EDITOR);

    /*
     * The following depend on the above values.
     * We use a pointer to the string so that if its
     * value changes we get the change.
     */
    if (user_runas == NULL)
	user_runas = &def_str(I_RUNAS_DEF);

    firsttime = 0;
}

static int
store_int(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    char *endp;
    long l;

    if (op == FALSE) {
	def->sd_un.ival = 0;
    } else {
	l = strtol(val, &endp, 10);
	if (*endp != '\0' || l < 0)
	    return(FALSE);
	/* XXX - should check against INT_MAX */
	def->sd_un.ival = (unsigned int)l;
    }
    return(TRUE);
}

static int
store_str(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{

    if (def->sd_un.str)
	free(def->sd_un.str);
    if (op == FALSE)
	def->sd_un.str = NULL;
    else
	def->sd_un.str = estrdup(val);
    return(TRUE);
}

static int
store_syslogfac(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    struct strmap *fac;

    if (op == FALSE) {
	if (def->sd_un.str) {
	    free(def->sd_un.str);
	    def->sd_un.str = NULL;
	}
	return(TRUE);
    }
#ifdef LOG_NFACILITIES
    if (!val)
	return(FALSE);
    for (fac = facilities; fac->name && strcmp(val, fac->name); fac++)
	;
    if (fac->name == NULL)
	return(FALSE);				/* not found */

    /* Store both name and number. */
    if (def->sd_un.str) {
	free(def->sd_un.str);
	closelog();
    }
    openlog(Argv[0], 0, fac->num);
    def->sd_un.str = estrdup(fac->name);
    sudo_defs_table[I_LOGFAC].sd_un.ival = fac->num;
#else
    if (def->sd_un.str) {
	free(def->sd_un.str);
	closelog();
    }
    openlog(Argv[0], 0);
    def->sd_un.str = estrdup("default");
#endif /* LOG_NFACILITIES */
    return(TRUE);
}

static int
store_syslogpri(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    struct strmap *pri;
    struct sudo_defs_types *idef;

    if (op == FALSE || !val)
	return(FALSE);
    if (def == &sudo_defs_table[I_GOODPRISTR])
	idef = &sudo_defs_table[I_GOODPRI];
    else if (def == &sudo_defs_table[I_BADPRISTR])
	idef = &sudo_defs_table[I_BADPRI];
    else
	return(FALSE);

    for (pri = priorities; pri->name && strcmp(val, pri->name); pri++)
	;
    if (pri->name == NULL)
	return(FALSE);				/* not found */

    /* Store both name and number. */
    if (def->sd_un.str)
	free(def->sd_un.str);
    def->sd_un.str = estrdup(pri->name);
    idef->sd_un.ival = pri->num;
    return(TRUE);
}

static int
store_mode(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    char *endp;
    long l;

    if (op == FALSE) {
	def->sd_un.mode = (mode_t)0777;
    } else {
	l = strtol(val, &endp, 8);
	if (*endp != '\0' || l < 0 || l >= 0777)
	    return(FALSE);
	def->sd_un.mode = (mode_t)l;
    }
    return(TRUE);
}

static int
store_pwflag(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    int isub, flags;

    if (strcmp(def->name, "verifypw") == 0)
	isub = I_VERIFYPW;
    else
	isub = I_LISTPW;

    /* Handle !foo. */
    if (op == FALSE) {
	if (def->sd_un.str) {
	    free(def->sd_un.str);
	    def->sd_un.str = NULL;
	}
	def->sd_un.str = estrdup("never");
	sudo_defs_table[isub].sd_un.ival = PWCHECK_NEVER;
	return(TRUE);
    }
    if (!val)
	return(FALSE);

    /* Convert strings to integer values. */
    if (strcmp(val, "all") == 0)
	flags = PWCHECK_ALL;
    else if (strcmp(val, "any") == 0)
	flags = PWCHECK_ANY;
    else if (strcmp(val, "never") == 0)
	flags = PWCHECK_NEVER;
    else if (strcmp(val, "always") == 0)
	flags = PWCHECK_ALWAYS;
    else
	return(FALSE);

    /* Store both name and number. */
    if (def->sd_un.str)
	free(def->sd_un.str);
    def->sd_un.str = estrdup(val);
    sudo_defs_table[isub].sd_un.ival = flags;

    return(TRUE);
}

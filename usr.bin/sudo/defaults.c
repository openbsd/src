/*
 * Copyright (c) 1999-2001, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: defaults.c,v 1.39 2003/04/02 18:25:19 millert Exp $";
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
static int store_uint __P((char *, struct sudo_defs_types *, int));
static int store_str __P((char *, struct sudo_defs_types *, int));
static int store_syslogfac __P((char *, struct sudo_defs_types *, int));
static int store_syslogpri __P((char *, struct sudo_defs_types *, int));
static int store_mode __P((char *, struct sudo_defs_types *, int));
static int store_pwflag __P((char *, struct sudo_defs_types *, int));
static int store_list __P((char *, struct sudo_defs_types *, int));
static void list_op __P((char *, size_t, struct sudo_defs_types *, enum list_ops));

/*
 * Table describing compile-time and run-time options.
 */
#include <def_data.c>

/*
 * Print version and configure info.
 */
void
dump_defaults()
{
    struct sudo_defs_types *cur;
    struct list_member *item;

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
		case T_UINT:
		case T_INT:
		    (void) printf(cur->desc, cur->sd_un.ival);
		    putchar('\n');
		    break;
		case T_MODE:
		    (void) printf(cur->desc, cur->sd_un.mode);
		    putchar('\n');
		    break;
		case T_LIST:
		    if (cur->sd_un.list) {
			puts(cur->desc);
			for (item = cur->sd_un.list; item; item = item->next)
			    printf("\t%s\n", item->value);
		    }
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
			(void) printf("%s: %.*s\n", cur->name,
			    (int) (p - cur->desc), cur->desc);
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
	warnx("unknown defaults entry `%s' referenced near line %d",
	    var, sudolineno);
	return(FALSE);
    }

    switch (cur->type & T_MASK) {
	case T_LOGFAC:
	    if (!store_syslogfac(val, cur, op)) {
		if (val)
		    warnx("value `%s' is invalid for option `%s'", val, var);
		else
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_LOGPRI:
	    if (!store_syslogpri(val, cur, op)) {
		if (val)
		    warnx("value `%s' is invalid for option `%s'", val, var);
		else
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_PWFLAG:
	    if (!store_pwflag(val, cur, op)) {
		if (val)
		    warnx("value `%s' is invalid for option `%s'", val, var);
		else
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_STR:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if ((cur->type & T_PATH) && val && *val != '/') {
		warnx("values for `%s' must start with a '/'", var);
		return(FALSE);
	    }
	    if (!store_str(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_INT:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_int(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_UINT:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_uint(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_MODE:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_mode(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_FLAG:
	    if (val) {
		warnx("option `%s' does not take a value on line %d",
		    var, sudolineno);
		return(FALSE);
	    }
	    cur->sd_un.flag = op;

	    /* Special action for I_FQDN.  Move to own switch if we get more */
	    if (num == I_FQDN && op)
		set_fqdn();
	    break;
	case T_LIST:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!(cur->type & T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_list(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
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
		case T_LIST:
		    list_op(NULL, 0, def, freeall);
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
    def_flag(I_MAIL_NO_USER) = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NO_HOST
    def_flag(I_MAIL_NO_HOST) = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NOT_OK
    def_flag(I_MAIL_NO_PERMS) = TRUE;
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
    def_flag(I_SET_LOGNAME) = TRUE;

    /* Syslog options need special care since they both strings and ints */
#if (LOGGING & SLOG_SYSLOG)
    (void) store_syslogfac(LOGFAC, &sudo_defs_table[I_SYSLOG], TRUE);
    (void) store_syslogpri(PRI_SUCCESS, &sudo_defs_table[I_SYSLOG_GOODPRI],
	TRUE);
    (void) store_syslogpri(PRI_FAILURE, &sudo_defs_table[I_SYSLOG_BADPRI],
	TRUE);
#endif

    /* Password flags also have a string and integer component. */
    (void) store_pwflag("any", &sudo_defs_table[I_LISTPW], TRUE);
    (void) store_pwflag("all", &sudo_defs_table[I_VERIFYPW], TRUE);

    /* Then initialize the int-like things. */
#ifdef SUDO_UMASK
    def_mode(I_UMASK) = SUDO_UMASK;
#else
    def_mode(I_UMASK) = 0777;
#endif
    def_ival(I_LOGLINELEN) = MAXLOGFILELEN;
    def_ival(I_TIMESTAMP_TIMEOUT) = TIMEOUT;
    def_ival(I_PASSWD_TIMEOUT) = PASSWORD_TIMEOUT;
    def_ival(I_PASSWD_TRIES) = TRIES_FOR_PASSWORD;

    /* Now do the strings */
    def_str(I_MAILTO) = estrdup(MAILTO);
    def_str(I_MAILSUB) = estrdup(MAILSUBJECT);
    def_str(I_BADPASS_MESSAGE) = estrdup(INCORRECT_PASSWORD);
    def_str(I_TIMESTAMPDIR) = estrdup(_PATH_SUDO_TIMEDIR);
    def_str(I_PASSPROMPT) = estrdup(PASSPROMPT);
    def_str(I_RUNAS_DEFAULT) = estrdup(RUNAS_DEFAULT);
#ifdef _PATH_SUDO_SENDMAIL
    def_str(I_MAILERPATH) = estrdup(_PATH_SUDO_SENDMAIL);
    def_str(I_MAILERFLAGS) = estrdup("-t");
#endif
#if (LOGGING & SLOG_FILE)
    def_str(I_LOGFILE) = estrdup(_PATH_SUDO_LOGFILE);
#endif
#ifdef EXEMPTGROUP
    def_str(I_EXEMPT_GROUP) = estrdup(EXEMPTGROUP);
#endif
    def_str(I_EDITOR) = estrdup(EDITOR);

    /* Finally do the lists (currently just environment tables). */
    init_envtables();

    /*
     * The following depend on the above values.
     * We use a pointer to the string so that if its
     * value changes we get the change.
     */
    if (user_runas == NULL)
	user_runas = &def_str(I_RUNAS_DEFAULT);

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
	if (*endp != '\0')
	    return(FALSE);
	/* XXX - should check against INT_MAX */
	def->sd_un.ival = (unsigned int)l;
    }
    return(TRUE);
}

static int
store_uint(val, def, op)
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
store_list(str, def, op)
    char *str;
    struct sudo_defs_types *def;
    int op;
{
    char *start, *end;

    /* Remove all old members. */
    if (op == FALSE || op == TRUE)
	list_op(NULL, 0, def, freeall);

    /* Split str into multiple space-separated words and act on each one. */
    if (op != FALSE) {
	end = str;
	do {
	    /* Remove leading blanks, if nothing but blanks we are done. */
	    for (start = end; isblank(*start); start++)
		;
	    if (*start == '\0')
		break;

	    /* Find end position and perform operation. */
	    for (end = start; *end && !isblank(*end); end++) 
		;
	    list_op(start, end - start, def, op == '-' ? delete : add);
	} while (*end++ != '\0');
    }
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
    if (def->sd_un.str)
	free(def->sd_un.str);
    def->sd_un.str = estrdup(fac->name);
    sudo_defs_table[I_LOGFAC].sd_un.ival = fac->num;
#else
    if (def->sd_un.str)
	free(def->sd_un.str);
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
    if (def == &sudo_defs_table[I_SYSLOG_GOODPRI])
	idef = &sudo_defs_table[I_GOODPRI];
    else if (def == &sudo_defs_table[I_SYSLOG_BADPRI])
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
	if (*endp != '\0' || l < 0 || l > 0777)
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
	isub = I_VERIFYPW_I;
    else
	isub = I_LISTPW_I;

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

static void
list_op(val, len, def, op)
    char *val;
    size_t len;
    struct sudo_defs_types *def;
    enum list_ops op;
{
    struct list_member *cur, *prev, *tmp;

    if (op == freeall) {
	for (cur = def->sd_un.list; cur; ) {
	    tmp = cur;
	    cur = tmp->next;
	    free(tmp->value);
	    free(tmp);
	}
	def->sd_un.list = NULL;
	return;
    }

    for (cur = def->sd_un.list, prev = NULL; cur; prev = cur, cur = cur->next) {
	if ((strncmp(cur->value, val, len) == 0 && cur->value[len] == '\0')) {

	    if (op == add)
		return;			/* already exists */

	    /* Delete node */
	    if (prev != NULL)
		prev->next = cur->next;
	    else
		def->sd_un.list = cur->next;
	    free(cur->value);
	    free(cur);
	    break;
	}
    }

    /* Add new node to the head of the list. */
    if (op == add) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = emalloc(len + 1);
	(void) memcpy(cur->value, val, len);
	cur->value[len] = '\0';
	cur->next = def->sd_un.list;
	def->sd_un.list = cur;
    }
}

/*	$OpenBSD: entry.c,v 1.32 2009/10/29 18:56:47 markus Exp $	*/

/*
 * Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* vix 26jan87 [RCS'd; rest of log is in RCS file]
 * vix 01jan87 [added line-level error recovery]
 * vix 31dec86 [added /step to the from-to range, per bob@acornrc]
 * vix 30dec86 [written]
 */

#include "cron.h"

typedef	enum ecode {
	e_none, e_minute, e_hour, e_dom, e_month, e_dow,
	e_cmd, e_timespec, e_username, e_option, e_memory
} ecode_e;

static const char *ecodes[] =
	{
		"no error",
		"bad minute",
		"bad hour",
		"bad day-of-month",
		"bad month",
		"bad day-of-week",
		"bad command",
		"bad time specifier",
		"bad username",
		"bad option",
		"out of memory"
	};

static int	get_list(bitstr_t *, int, int, const char *[], int, FILE *),
		get_range(bitstr_t *, int, int, const char *[], int, FILE *),
		get_number(int *, int, const char *[], int, FILE *, const char *),
		set_element(bitstr_t *, int, int, int);

void
free_entry(entry *e) {
	free(e->cmd);
	free(e->pwd);
	env_free(e->envp);
	free(e);
}

/* return NULL if eof or syntax error occurs;
 * otherwise return a pointer to a new entry.
 */
entry *
load_entry(FILE *file, void (*error_func)(const char *), struct passwd *pw,
    char **envp) {
	/* this function reads one crontab entry -- the next -- from a file.
	 * it skips any leading blank lines, ignores comments, and returns
	 * NULL if for any reason the entry can't be read and parsed.
	 *
	 * the entry is also parsed here.
	 *
	 * syntax:
	 *   user crontab:
	 *	minutes hours doms months dows cmd\n
	 *   system crontab (/etc/crontab):
	 *	minutes hours doms months dows USERNAME cmd\n
	 */

	ecode_e	ecode = e_none;
	entry *e;
	int ch;
	char cmd[MAX_COMMAND];
	char envstr[MAX_ENVSTR];
	char **tenvp;

	Debug(DPARS, ("load_entry()...about to eat comments\n"))

	skip_comments(file);

	ch = get_char(file);
	if (ch == EOF)
		return (NULL);

	/* ch is now the first useful character of a useful line.
	 * it may be an @special or it may be the first character
	 * of a list of minutes.
	 */

	e = (entry *) calloc(sizeof(entry), sizeof(char));

	if (ch == '@') {
		/* all of these should be flagged and load-limited; i.e.,
		 * instead of @hourly meaning "0 * * * *" it should mean
		 * "close to the front of every hour but not 'til the
		 * system load is low".  Problems are: how do you know
		 * what "low" means? (save me from /etc/cron.conf!) and:
		 * how to guarantee low variance (how low is low?), which
		 * means how to we run roughly every hour -- seems like
		 * we need to keep a history or let the first hour set
		 * the schedule, which means we aren't load-limited
		 * anymore.  too much for my overloaded brain. (vix, jan90)
		 * HINT
		 */
		ch = get_string(cmd, MAX_COMMAND, file, " \t\n");
		if (!strcmp("reboot", cmd)) {
			e->flags |= WHEN_REBOOT;
		} else if (!strcmp("yearly", cmd) || !strcmp("annually", cmd)){
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_set(e->dom, 0);
			bit_set(e->month, 0);
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
			e->flags |= DOW_STAR;
		} else if (!strcmp("monthly", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_set(e->dom, 0);
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
			e->flags |= DOW_STAR;
		} else if (!strcmp("weekly", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_set(e->dow, 0);
			e->flags |= DOW_STAR;
		} else if (!strcmp("daily", cmd) || !strcmp("midnight", cmd)) {
			bit_set(e->minute, 0);
			bit_set(e->hour, 0);
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
		} else if (!strcmp("hourly", cmd)) {
			bit_set(e->minute, 0);
			bit_nset(e->hour, 0, (LAST_HOUR-FIRST_HOUR+1));
			bit_nset(e->dom, 0, (LAST_DOM-FIRST_DOM+1));
			bit_nset(e->month, 0, (LAST_MONTH-FIRST_MONTH+1));
			bit_nset(e->dow, 0, (LAST_DOW-FIRST_DOW+1));
			e->flags |= HR_STAR;
		} else {
			ecode = e_timespec;
			goto eof;
		}
		/* Advance past whitespace between shortcut and
		 * username/command.
		 */
		Skip_Blanks(ch, file);
		if (ch == EOF || ch == '\n') {
			ecode = e_cmd;
			goto eof;
		}
	} else {
		Debug(DPARS, ("load_entry()...about to parse numerics\n"))

		if (ch == '*')
			e->flags |= MIN_STAR;
		ch = get_list(e->minute, FIRST_MINUTE, LAST_MINUTE,
			      PPC_NULL, ch, file);
		if (ch == EOF) {
			ecode = e_minute;
			goto eof;
		}

		/* hours
		 */

		if (ch == '*')
			e->flags |= HR_STAR;
		ch = get_list(e->hour, FIRST_HOUR, LAST_HOUR,
			      PPC_NULL, ch, file);
		if (ch == EOF) {
			ecode = e_hour;
			goto eof;
		}

		/* DOM (days of month)
		 */

		if (ch == '*')
			e->flags |= DOM_STAR;
		ch = get_list(e->dom, FIRST_DOM, LAST_DOM,
			      PPC_NULL, ch, file);
		if (ch == EOF) {
			ecode = e_dom;
			goto eof;
		}

		/* month
		 */

		ch = get_list(e->month, FIRST_MONTH, LAST_MONTH,
			      MonthNames, ch, file);
		if (ch == EOF) {
			ecode = e_month;
			goto eof;
		}

		/* DOW (days of week)
		 */

		if (ch == '*')
			e->flags |= DOW_STAR;
		ch = get_list(e->dow, FIRST_DOW, LAST_DOW,
			      DowNames, ch, file);
		if (ch == EOF) {
			ecode = e_dow;
			goto eof;
		}
	}

	/* make sundays equivalent */
	if (bit_test(e->dow, 0) || bit_test(e->dow, 7)) {
		bit_set(e->dow, 0);
		bit_set(e->dow, 7);
	}

	/* check for permature EOL and catch a common typo */
	if (ch == '\n' || ch == '*') {
		ecode = e_cmd;
		goto eof;
	}

	/* ch is the first character of a command, or a username */
	unget_char(ch, file);

	if (!pw) {
		char		*username = cmd;	/* temp buffer */

		Debug(DPARS, ("load_entry()...about to parse username\n"))
		ch = get_string(username, MAX_COMMAND, file, " \t\n");

		Debug(DPARS, ("load_entry()...got %s\n",username))
		if (ch == EOF || ch == '\n' || ch == '*') {
			ecode = e_cmd;
			goto eof;
		}

		pw = getpwnam(username);
		if (pw == NULL) {
			ecode = e_username;
			goto eof;
		}
		Debug(DPARS, ("load_entry()...uid %lu, gid %lu\n",
			      (unsigned long)pw->pw_uid,
			      (unsigned long)pw->pw_gid))
	}

	if ((e->pwd = pw_dup(pw)) == NULL) {
		ecode = e_memory;
		goto eof;
	}
	bzero(e->pwd->pw_passwd, strlen(e->pwd->pw_passwd));

	/* copy and fix up environment.  some variables are just defaults and
	 * others are overrides.
	 */
	if ((e->envp = env_copy(envp)) == NULL) {
		ecode = e_memory;
		goto eof;
	}
	if (!env_get("SHELL", e->envp)) {
		if (snprintf(envstr, sizeof envstr, "SHELL=%s", _PATH_BSHELL) >=
		    sizeof(envstr))
			log_it("CRON", getpid(), "error", "can't set SHELL");
		else {
			if ((tenvp = env_set(e->envp, envstr)) == NULL) {
				ecode = e_memory;
				goto eof;
			}
			e->envp = tenvp;
		}
	}
	if (!env_get("HOME", e->envp)) {
		if (snprintf(envstr, sizeof envstr, "HOME=%s", pw->pw_dir) >=
		    sizeof(envstr))
			log_it("CRON", getpid(), "error", "can't set HOME");
		else {
			if ((tenvp = env_set(e->envp, envstr)) == NULL) {
				ecode = e_memory;
				goto eof;
			}
			e->envp = tenvp;
		}
	}
#ifndef LOGIN_CAP
	/* If login.conf is in use we will get the default PATH later. */
	if (!env_get("PATH", e->envp)) {
		if (snprintf(envstr, sizeof envstr, "PATH=%s", _PATH_DEFPATH) >=
		    sizeof(envstr))
			log_it("CRON", getpid(), "error", "can't set PATH");
		else {
			if ((tenvp = env_set(e->envp, envstr)) == NULL) {
				ecode = e_memory;
				goto eof;
			}
			e->envp = tenvp;
		}
	}
#endif /* LOGIN_CAP */
	if (snprintf(envstr, sizeof envstr, "LOGNAME=%s", pw->pw_name) >=
		sizeof(envstr))
		log_it("CRON", getpid(), "error", "can't set LOGNAME");
	else {
		if ((tenvp = env_set(e->envp, envstr)) == NULL) {
			ecode = e_memory;
			goto eof;
		}
		e->envp = tenvp;
	}
#if defined(BSD) || defined(__linux)
	if (snprintf(envstr, sizeof envstr, "USER=%s", pw->pw_name) >=
		sizeof(envstr))
		log_it("CRON", getpid(), "error", "can't set USER");
	else {
		if ((tenvp = env_set(e->envp, envstr)) == NULL) {
			ecode = e_memory;
			goto eof;
		}
		e->envp = tenvp;
	}
#endif

	Debug(DPARS, ("load_entry()...about to parse command\n"))

	/* If the first character of the command is '-' it is a cron option.
	 */
	ch = get_char(file);
	while (ch == '-') {
		switch (ch = get_char(file)) {
		case 'q':
			e->flags |= DONT_LOG;
			Skip_Nonblanks(ch, file)
			break;
		default:
			ecode = e_option;
			goto eof;
		}
		Skip_Blanks(ch, file)
		if (ch == EOF || ch == '\n') {
			ecode = e_cmd;
			goto eof;
		}
	}
	unget_char(ch, file);

	/* Everything up to the next \n or EOF is part of the command...
	 * too bad we don't know in advance how long it will be, since we
	 * need to malloc a string for it... so, we limit it to MAX_COMMAND.
	 */
	ch = get_string(cmd, MAX_COMMAND, file, "\n");

	/* a file without a \n before the EOF is rude, so we'll complain...
	 */
	if (ch == EOF) {
		ecode = e_cmd;
		goto eof;
	}

	/* got the command in the 'cmd' string; save it in *e.
	 */
	if ((e->cmd = strdup(cmd)) == NULL) {
		ecode = e_memory;
		goto eof;
	}

	Debug(DPARS, ("load_entry()...returning successfully\n"))

	/* success, fini, return pointer to the entry we just created...
	 */
	return (e);

 eof:
	if (e->envp)
		env_free(e->envp);
	if (e->pwd)
		free(e->pwd);
	if (e->cmd)
		free(e->cmd);
	free(e);
	while (ch != '\n' && !feof(file))
		ch = get_char(file);
	if (ecode != e_none && error_func)
		(*error_func)(ecodes[(int)ecode]);
	return (NULL);
}

static int
get_list(bitstr_t *bits, int low, int high, const char *names[],
	 int ch, FILE *file)
{
	int done;

	/* we know that we point to a non-blank character here;
	 * must do a Skip_Blanks before we exit, so that the
	 * next call (or the code that picks up the cmd) can
	 * assume the same thing.
	 */

	Debug(DPARS|DEXT, ("get_list()...entered\n"))

	/* list = range {"," range}
	 */

	/* clear the bit string, since the default is 'off'.
	 */
	bit_nclear(bits, 0, (high-low+1));

	/* process all ranges
	 */
	done = FALSE;
	while (!done) {
		if (EOF == (ch = get_range(bits, low, high, names, ch, file)))
			return (EOF);
		if (ch == ',')
			ch = get_char(file);
		else
			done = TRUE;
	}

	/* exiting.  skip to some blanks, then skip over the blanks.
	 */
	Skip_Nonblanks(ch, file)
	Skip_Blanks(ch, file)

	Debug(DPARS|DEXT, ("get_list()...exiting w/ %02x\n", ch))

	return (ch);
}


static int
get_range(bitstr_t *bits, int low, int high, const char *names[],
	  int ch, FILE *file)
{
	/* range = number | number "-" number [ "/" number ]
	 */

	int i, num1, num2, num3;

	Debug(DPARS|DEXT, ("get_range()...entering, exit won't show\n"))

	if (ch == '*') {
		/* '*' means "first-last" but can still be modified by /step
		 */
		num1 = low;
		num2 = high;
		ch = get_char(file);
		if (ch == EOF)
			return (EOF);
	} else {
		ch = get_number(&num1, low, names, ch, file, ",- \t\n");
		if (ch == EOF)
			return (EOF);

		if (ch != '-') {
			/* not a range, it's a single number.
			 */
			if (EOF == set_element(bits, low, high, num1)) {
				unget_char(ch, file);
				return (EOF);
			}
			return (ch);
		} else {
			/* eat the dash
			 */
			ch = get_char(file);
			if (ch == EOF)
				return (EOF);

			/* get the number following the dash
			 */
			ch = get_number(&num2, low, names, ch, file, "/, \t\n");
			if (ch == EOF || num1 > num2)
				return (EOF);
		}
	}

	/* check for step size
	 */
	if (ch == '/') {
		/* eat the slash
		 */
		ch = get_char(file);
		if (ch == EOF)
			return (EOF);

		/* get the step size -- note: we don't pass the
		 * names here, because the number is not an
		 * element id, it's a step size.  'low' is
		 * sent as a 0 since there is no offset either.
		 */
		ch = get_number(&num3, 0, PPC_NULL, ch, file, ", \t\n");
		if (ch == EOF || num3 == 0)
			return (EOF);
	} else {
		/* no step.  default==1.
		 */
		num3 = 1;
	}

	/* range. set all elements from num1 to num2, stepping
	 * by num3.  (the step is a downward-compatible extension
	 * proposed conceptually by bob@acornrc, syntactically
	 * designed then implemented by paul vixie).
	 */
	for (i = num1;  i <= num2;  i += num3)
		if (EOF == set_element(bits, low, high, i)) {
			unget_char(ch, file);
			return (EOF);
		}

	return (ch);
}

static int
get_number(int *numptr, int low, const char *names[], int ch, FILE *file,
    const char *terms) {
	char temp[MAX_TEMPSTR], *pc;
	int len, i;

	pc = temp;
	len = 0;

	/* first look for a number */
	while (isdigit((unsigned char)ch)) {
		if (++len >= MAX_TEMPSTR)
			goto bad;
		*pc++ = ch;
		ch = get_char(file);
	}
	*pc = '\0';
	if (len != 0) {
		/* got a number, check for valid terminator */
		if (!strchr(terms, ch))
			goto bad;
		*numptr = atoi(temp);
		return (ch);
	}

	/* no numbers, look for a string if we have any */
	if (names) {
		while (isalpha((unsigned char)ch)) {
			if (++len >= MAX_TEMPSTR)
				goto bad;
			*pc++ = ch;
			ch = get_char(file);
		}
		*pc = '\0';
		if (len != 0 && strchr(terms, ch)) {
			for (i = 0;  names[i] != NULL;  i++) {
				Debug(DPARS|DEXT,
					("get_num, compare(%s,%s)\n", names[i],
					temp))
				if (!strcasecmp(names[i], temp)) {
					*numptr = i+low;
					return (ch);
				}
			}
		}
	}

bad:
	unget_char(ch, file);
	return (EOF);
}

static int
set_element(bitstr_t *bits, int low, int high, int number) {
	Debug(DPARS|DEXT, ("set_element(?,%d,%d,%d)\n", low, high, number))

	if (number < low || number > high)
		return (EOF);

	bit_set(bits, (number-low));
	return (OK);
}

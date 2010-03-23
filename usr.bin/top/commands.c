/* $OpenBSD: commands.c,v 1.29 2010/03/23 16:16:09 lum Exp $	 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file contains the routines that implement some of the interactive
 *  mode commands.  Note that some of the commands are implemented in-line
 *  in "main".  This is necessary because they change the global state of
 *  "top" (i.e.:  changing the number of processes to display).
 */

#include <sys/types.h>
#include <stdio.h>
#include <err.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "top.h"

#include "boolean.h"
#include "utils.h"
#include "machine.h"

static char    *next_field(char *);
static int      scan_arg(char *, int *, char *);
static char    *err_string(void);
static size_t   str_adderr(char *, size_t, int);
static size_t   str_addarg(char *, size_t, char *, int);
static int      err_compar(const void *, const void *);

/*
 *  Utility routines that help with some of the commands.
 */
static char *
next_field(char *str)
{
	if ((str = strchr(str, ' ')) == NULL)
		return (NULL);

	*str = '\0';
	while (*++str == ' ')	/* loop */
		;

	/* if there is nothing left of the string, return NULL */
	/* This fix is dedicated to Greg Earle */
	return (*str == '\0' ? NULL : str);
}

/*
 * Scan the renice or kill interactive arguments for data and/or errors.
 */
static int
scan_arg(char *str, int *intp, char *nptr)
{
	int val = 0, bad_flag = 0;
	char ch;

	*nptr = '\0';

	if (*str == '\0')
		return (-1);

	while ((ch = *str++) != '\0') {
		if (isspace(ch))
			break;
		else if (!isdigit(ch))
			bad_flag = 1;
		else
			val = val * 10 + (ch - '0');

		*(nptr++) = ch;
	}
	*nptr = '\0';

	if (bad_flag == 1)
		return(-1);

	*intp = val;
	return (0);
}

/*
 *  Some of the commands make system calls that could generate errors.
 *  These errors are collected up in an array of structures for later
 *  contemplation and display.  Such routines return a string containing an
 *  error message, or NULL if no errors occurred.  The next few routines are
 *  for manipulating and displaying these errors.  We need an upper limit on
 *  the number of errors, so we arbitrarily choose 20.
 */

#define ERRMAX 20

struct errs	errs[ERRMAX];
int		errcnt;
static char    *err_toomany = " too many errors occurred";
static char    *err_listem =
	" Many errors occurred.  Press `e' to display the list of errors.";

/* These macros get used to reset and log the errors */
#define ERR_RESET   errcnt = 0
#define ERROR(p, e) \
	if (errcnt >= ERRMAX) { \
		return(err_toomany); \
	} else { \
		free(errs[errcnt].arg); \
		if ((errs[errcnt].arg = strdup(p)) == NULL) \
			err(1, "strdup"); \
		errs[errcnt++].err = (e); \
	}

#define STRMAX 80

/*
 *  err_string() - return an appropriate error string.  This is what the
 *	command will return for displaying.  If no errors were logged, then
 *	return NULL.  The maximum length of the error string is defined by
 *	"STRMAX".
 */
static char *
err_string(void)
{
	int cnt = 0, first = Yes, currerr = -1;
	static char string[STRMAX];
	struct errs *errp;

	/* if there are no errors, return NULL */
	if (errcnt == 0)
		return (NULL);

	/* sort the errors */
	qsort(errs, errcnt, sizeof(struct errs), err_compar);

	/* need a space at the front of the error string */
	string[0] = ' ';
	string[1] = '\0';

	/* loop thru the sorted list, building an error string */
	while (cnt < errcnt) {
		errp = &(errs[cnt++]);
		if (errp->err != currerr) {
			if (currerr != -1) {
				if (str_adderr(string, sizeof string, currerr) >
				    sizeof string - 2)
					return (err_listem);

				/* we know there's more */
				(void) strlcat(string, "; ", sizeof string);
			}
			currerr = errp->err;
			first = Yes;
		}
		if (str_addarg(string, sizeof string, errp->arg, first) >=
		    sizeof string)
			return (err_listem);

		first = No;
	}

	/* add final message */
	if (str_adderr(string, sizeof string, currerr) >= sizeof string)
		return (err_listem);

	/* return the error string */
	return (string);
}

/*
 *  str_adderr(str, len, err) - add an explanation of error "err" to
 *	the string "str".
 */
static size_t
str_adderr(char *str, size_t len, int err)
{
	size_t msglen;
	char *msg;

	msg = err == 0 ? "Not a number" : strerror(err);

	if ((msglen = strlcat(str, ": ", len)) >= len)
		return (msglen);

	return (strlcat(str, msg, len));
}

/*
 *  str_addarg(str, len, arg, first) - add the string argument "arg" to
 *	the string "str".  This is the first in the group when "first"
 *	is set (indicating that a comma should NOT be added to the front).
 */
static size_t
str_addarg(char *str, size_t len, char *arg, int first)
{
	size_t msglen;

	if (!first) {
		if ((msglen = strlcat(str, ", ", len)) >= len)
			return (msglen);
	}
	return (strlcat(str, arg, len));
}

/*
 *  err_compar(p1, p2) - comparison routine used by "qsort"
 *	for sorting errors.
 */
static int
err_compar(const void *e1, const void *e2)
{
	const struct errs *p1 = (const struct errs *) e1;
	const struct errs *p2 = (const struct errs *) e2;
	int result;

	if ((result = p1->err - p2->err) == 0)
		return (strcmp(p1->arg, p2->arg));
	return (result);
}

/*
 *  error_count() - return the number of errors currently logged.
 */
int
error_count(void)
{
	return (errcnt);
}

/*
 *  kill_procs(str) - send signals to processes, much like the "kill"
 *		command does; invoked in response to 'k'.
 */
char *
kill_procs(char *str)
{
	int signum = SIGTERM, procnum;
	uid_t uid, puid;
	char tempbuf[TEMPBUFSIZE];
	char *nptr;

	/* reset error array */
	ERR_RESET;

	/* remember our uid */
	uid = getuid();

	/* skip over leading white space */
	while (isspace(*str))
		str++;

	if (str[0] == '-') {
		/* explicit signal specified */
		if ((nptr = next_field(str)) == NULL)
			return (" kill: no processes specified");

		if (isdigit(str[1])) {
			(void) scan_arg(str + 1, &signum, nptr);
			if (signum <= 0 || signum >= NSIG)
				return (" invalid signal number");
		} else {
			/* translate the name into a number */
			for (signum = 0; signum < NSIG; signum++) {
				if (strcasecmp(sys_signame[signum], str + 1) == 0)
					break;
			}

			/* was it ever found */
			if (signum == NSIG)
				return (" bad signal name");
		}
		/* put the new pointer in place */
		str = nptr;
	}
	nptr = tempbuf;
	/* loop thru the string, killing processes */
	do {
		if (scan_arg(str, &procnum, nptr) == -1) {
			ERROR(nptr, 0);
		} else {
			/* check process owner if we're not root */
			puid = proc_owner(procnum);
			if (puid == (uid_t)(-1)) {
				ERROR(nptr, ESRCH);
			} else if (uid && (uid != puid)) {
				ERROR(nptr, EACCES);
			} else if (kill(procnum, signum) == -1) {
				ERROR(nptr, errno);
			}
		}
	} while ((str = next_field(str)) != NULL);

	/* return appropriate error string */
	return (err_string());
}

/*
 *  renice_procs(str) - change the "nice" of processes, much like the
 *		"renice" command does; invoked in response to 'r'.
 */
char *
renice_procs(char *str)
{
	uid_t uid;
	char negate;
	int prio, procnum;
	char tempbuf[TEMPBUFSIZE];
	char *nptr;

	ERR_RESET;
	uid = getuid();

	/* skip over leading white space */
	while (isspace(*str))
		str++;

	/* allow for negative priority values */
	if ((negate = (*str == '-')) != 0) {
		/* move past the minus sign */
		str++;
	}

	nptr = tempbuf;
	/* use procnum as a temporary holding place and get the number */
	procnum = scan_arg(str, &prio, nptr);

	/* negate if necessary */
	if (negate)
		prio = -prio;

#if defined(PRIO_MIN) && defined(PRIO_MAX)
	/* check for validity */
	if (procnum == -1 || prio < PRIO_MIN || prio > PRIO_MAX)
		return (" bad priority value");
#endif

	/* move to the first process number */
	if ((str = next_field(str)) == NULL)
		return (" no processes specified");

	/* loop thru the process numbers, renicing each one */
	do {
		if (scan_arg(str, &procnum, nptr) == -1) {
			ERROR(nptr, 0);
		}
		/* check process owner if we're not root */
		else if (uid && (uid != proc_owner(procnum))) {
			ERROR(nptr, EACCES);
		} else if (setpriority(PRIO_PROCESS, procnum, prio) == -1) {
			ERROR(nptr, errno);
		}
	} while ((str = next_field(str)) != NULL);

	/* return appropriate error string */
	return (err_string());
}

/*	$OpenBSD: distopt.c,v 1.11 2009/10/27 23:59:42 deraadt Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "defs.h"

/*
 * Dist Option functions
 */


/*
 * Distfile Option Information
 */
DISTOPTINFO distoptinfo[] = {
	{ DO_CHKNFS,		"chknfs", 	NULL,		0},
	{ DO_CHKREADONLY,	"chkreadonly",	NULL,		0},
	{ DO_CHKSYM,		"chksym",	NULL,		0},
	{ DO_COMPARE,		"compare", 	NULL,		0},
	{ DO_DEFGROUP,		"defgroup",	defgroup,	sizeof(defgroup) },
	{ DO_DEFOWNER,		"defowner",	defowner,	sizeof(defowner) },
	{ DO_FOLLOW,		"follow", 	NULL,		0},
	{ DO_HISTORY,		"history", 	NULL,		0},
	{ DO_IGNLNKS,		"ignlnks",	NULL,		0},
	{ DO_NOCHKGROUP,	"nochkgroup",	NULL,		0},
	{ DO_NOCHKMODE,		"nochkmode",	NULL,		0},
	{ DO_NOCHKOWNER,	"nochkowner",	NULL,		0},
	{ DO_NODESCEND,		"nodescend",	NULL,		0},
	{ DO_NOEXEC,		"noexec",	NULL,		0},
	{ DO_NUMCHKGROUP,	"numchkgroup",	NULL,		0},
	{ DO_NUMCHKOWNER,	"numchkowner",	NULL,		0},
	{ DO_QUIET,		"quiet",	NULL,		0},
	{ DO_REMOVE,		"remove",	NULL,		0},
	{ DO_SAVETARGETS,	"savetargets",	NULL,		0},
	{ DO_SPARSE,		"sparse",	NULL,		0},
	{ DO_UPDATEPERM,	"updateperm",	NULL,		0},
	{ DO_VERIFY,		"verify",	NULL,		0},
	{ DO_WHOLE,		"whole",	NULL,		0},
	{ DO_YOUNGER,		"younger",	NULL,		0},
	{ 0 },
};

/*
 * Get a Distfile Option entry named "name".
 */
DISTOPTINFO *
getdistopt(char *name, int *len)
{
	int i;

	for (i = 0; distoptinfo[i].do_name; ++i)
		if (strncasecmp(name, distoptinfo[i].do_name,
				*len = strlen(distoptinfo[i].do_name)) == 0)
			return(&distoptinfo[i]);

	return(NULL);
}

/*
 * Parse a dist option string.  Set option flags to optptr.
 * If doerrs is true, print out own error message.  Returns
 * 0 on success.
 */
int
parsedistopts(char *str, opt_t *optptr, int doerrs)
{
	char *string, *optstr;
	DISTOPTINFO *distopt;
	int len;

	/* strtok() is destructive */
	string = xstrdup(str);

	for (optstr = strtok(string, ","); optstr;
	     optstr = strtok(NULL, ",")) {
		/* Try Yes */
		if ((distopt = getdistopt(optstr, &len)) != NULL) {
			FLAG_ON(*optptr, distopt->do_value);
			if (distopt->do_arg && optstr[len] == '=')
				(void) strlcpy(distopt->do_arg,
				    &optstr[len + 1], distopt->arg_size);
			continue;
		}

		/* Try No */
		if ((distopt = getdistopt(optstr+2, &len)) != NULL) {
			FLAG_OFF(*optptr, distopt->do_value);
			continue;
		}

		if (doerrs)
			error("Dist option \"%s\" is not valid.", optstr);
	}

	if (string)
		(void) free(string);

	return(nerrs);
}

/*
 * Get a list of the Distfile Option Entries.
 */
char *
getdistoptlist(void)
{
	int i;
	static char buf[1024];

	for (i = 0, buf[0] = CNULL; distoptinfo[i].do_name; ++i) {
		if (buf[0] == CNULL)
			(void) strlcpy(buf, distoptinfo[i].do_name, sizeof buf);
		else {
			(void) strlcat(buf, ",", sizeof buf);
			(void) strlcat(buf, distoptinfo[i].do_name, sizeof buf);
		}
	}

	return(buf);
}

/*
 * Get a list of the Distfile Option Entries for each enabled 
 * value in "opts".
 */
char *
getondistoptlist(opt_t opts)
{
	int i;
	static char buf[1024];

	for (i = 0, buf[0] = CNULL; distoptinfo[i].do_name; ++i) {
		if (!IS_ON(opts, distoptinfo[i].do_value))
			continue;

		if (buf[0] == CNULL)
			(void) strlcpy(buf, distoptinfo[i].do_name, sizeof buf);
		else {
			(void) strlcat(buf, ",", sizeof buf);
			(void) strlcat(buf, distoptinfo[i].do_name, sizeof buf);
		}
		if (distoptinfo[i].do_arg) {
			(void) strlcat(buf, "=", sizeof buf);
			(void) strlcat(buf, distoptinfo[i].do_arg, sizeof buf);
		}
	}

	return(buf);
}


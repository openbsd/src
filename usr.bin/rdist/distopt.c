/*	$OpenBSD: distopt.c,v 1.4 1998/06/26 21:21:07 millert Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char RCSid[] = 
"$From: distopt.c,v 6.10 1996/01/30 01:52:07 mcooper Exp $";
#else
static char RCSid[] = 
"$OpenBSD: distopt.c,v 1.4 1998/06/26 21:21:07 millert Exp $";
#endif

static char sccsid[] = "@(#)distopt.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* !lint */

/*
 * Dist Option functions
 */

#include "defs.h"

/*
 * Distfile Option Information
 */
DISTOPTINFO distoptinfo[] = {
	{ DO_CHKNFS,		"chknfs" },
	{ DO_CHKREADONLY,	"chkreadonly" },
	{ DO_CHKSYM,		"chksym" },
	{ DO_COMPARE,		"compare" },
	{ DO_FOLLOW,		"follow" },
	{ DO_IGNLNKS,		"ignlnks" },
	{ DO_NOCHKGROUP,	"nochkgroup" },
	{ DO_NOCHKMODE,		"nochkmode" },
	{ DO_NOCHKOWNER,	"nochkowner" },
	{ DO_NODESCEND,		"nodescend" },
	{ DO_NOEXEC,		"noexec" },
	{ DO_NUMCHKGROUP,	"numchkgroup" },
	{ DO_NUMCHKOWNER,	"numchkowner" },
	{ DO_QUIET,		"quiet" },
	{ DO_REMOVE,		"remove" },
	{ DO_SAVETARGETS,	"savetargets" },
	{ DO_SPARSE,            "sparse" },
	{ DO_VERIFY,		"verify" },
	{ DO_WHOLE,		"whole" },
	{ DO_YOUNGER,		"younger" },
	{ 0 },
};

/*
 * Get a Distfile Option entry named "name".
 */
extern DISTOPTINFO *getdistopt(name)
	char *name;
{
	register int i;

	for (i = 0; distoptinfo[i].do_name; ++i)
		if (strcasecmp(name, distoptinfo[i].do_name) == 0)
			return(&distoptinfo[i]);

	return(NULL);
}

/*
 * Parse a dist option string.  Set option flags to optptr.
 * If doerrs is true, print out own error message.  Returns
 * 0 on success.
 */
extern int parsedistopts(str, optptr, doerrs)
	char *str;
	opt_t *optptr;
	int doerrs;
{
	register char *string, *optstr;
	DISTOPTINFO *distopt;
	int negate;

	/* strtok() is harmful */
	string = strdup(str);

	for (optstr = strtok(string, ","); optstr;
	     optstr = strtok(NULL, ",")) {
		if (strncasecmp(optstr, "no", 2) == 0)
			negate = TRUE;
		else
			negate = FALSE;

		/*
		 * Try looking up option name.  If that fails
		 * and the option starts with "no", strip "no"
		 * from option and retry lookup.
		 */
		if ((distopt = getdistopt(optstr))) {
			FLAG_ON(*optptr, distopt->do_value);
			continue;
		}
		if (negate && (distopt = getdistopt(optstr+2))) {
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
extern char *getdistoptlist()
{
	register int i;
	static char buf[1024];

	for (i = 0, buf[0] = CNULL; distoptinfo[i].do_name; ++i) {
		if (buf[0] == CNULL)
			(void) strcpy(buf, distoptinfo[i].do_name);
		else {
			(void) strcat(buf, ",");
			(void) strcat(buf, distoptinfo[i].do_name);
		}
	}

	return(buf);
}

/*
 * Get a list of the Distfile Option Entries for each enabled 
 * value in "opts".
 */
extern char *getondistoptlist(opts)
	opt_t opts;
{
	register int i;
	static char buf[1024];

	for (i = 0, buf[0] = CNULL; distoptinfo[i].do_name; ++i) {
		if (!IS_ON(opts, distoptinfo[i].do_value))
			continue;

		if (buf[0] == CNULL)
			(void) strcpy(buf, distoptinfo[i].do_name);
		else {
			(void) strcat(buf, ",");
			(void) strcat(buf, distoptinfo[i].do_name);
		}
	}

	return(buf);
}


/*	$OpenBSD: rdist.c,v 1.29 2015/01/20 09:00:16 guenther Exp $	*/

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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "y.tab.h"


/*
 * Remote distribution program.
 */

int     	maxchildren = MAXCHILDREN;	/* Max no of concurrent PIDs */
int		nflag = 0;			/* Say without doing */
int64_t		min_freespace = 0;		/* Min filesys free space */
int64_t		min_freefiles = 0;		/* Min filesys free # files */
FILE   	       *fin = NULL;			/* Input file pointer */
char		localmsglist[] = "stdout=all:notify=all:syslog=nerror,ferror";
char   	       *remotemsglist = NULL;
char		optchars[] = "A:a:bcd:DFf:hil:L:M:m:NnOo:p:P:qRrst:Vvwxy";
char	       *path_rdistd = _PATH_RDISTD;
char	       *path_remsh = NULL;

static void addhostlist(char *, struct namelist **);
static void usage(void);
int main(int, char **, char **);

/*
 * Add a hostname to the host list
 */
static void
addhostlist(char *name, struct namelist **hostlist)
{
	struct namelist *ptr, *new;

	if (!name || !hostlist)
		return;

	new = xmalloc(sizeof *new);
	new->n_name = xstrdup(name);
	new->n_regex = NULL;
	new->n_next = NULL;

	if (*hostlist) {
		for (ptr = *hostlist; ptr && ptr->n_next; ptr = ptr->n_next)
			;
		ptr->n_next = new;
	} else
		*hostlist = new;
}

int
main(int argc, char **argv, char **envp)
{
	extern char *__progname;
	struct namelist *hostlist = NULL;
	char *distfile = NULL;
	char *cp;
	int cmdargs = 0;
	int c;
	const char *errstr;

	progname = __progname;

	if ((cp = msgparseopts(localmsglist, TRUE)) != NULL) {
		error("Bad builtin log option (%s): %s.", 
		      localmsglist, cp);
		usage();
	}

	if ((cp = getenv("RDIST_OPTIONS")) != NULL)
		if (parsedistopts(cp, &options, TRUE)) {
			error("Bad dist option environment string \"%s\".", 
			      cp);
			exit(1);
		}

	if (init(argc, argv, envp) < 0)
		exit(1);

	/*
	 * Perform check to make sure we are not incorrectly installed
	 * setuid to root or anybody else.
	 */
	if (getuid() != geteuid())
		fatalerr("This version of rdist should not be installed setuid.");

	while ((c = getopt(argc, argv, optchars)) != -1)
		switch (c) {
		case 'l':
			if ((cp = msgparseopts(optarg, TRUE)) != NULL) {
				error("Bad log option \"%s\": %s.", optarg,cp);
				usage();
			}
			break;

		case 'L':
			remotemsglist = xstrdup(optarg);
			break;

		case 'A':
		case 'a':
		case 'M':
		case 't':
			if (!isdigit((unsigned char)*optarg)) {
				error("\"%s\" is not a number.", optarg);
				usage();
			}
			if (c == 'a') {
				min_freespace = (int64_t)strtonum(optarg,
					0, LLONG_MAX, &errstr);
				if (errstr)
					fatalerr("Minimum free space is %s: "
						 "'%s'", errstr, optarg);
			}
			else if (c == 'A') {
				min_freefiles = (int64_t)strtonum(optarg,
					0, LLONG_MAX, &errstr);
				if (errstr)
					fatalerr("Minimum free files is %s: "
						 "'%s'", errstr, optarg);
			}
			else if (c == 'M')
				maxchildren = atoi(optarg);
			else if (c == 't')
				rtimeout = atoi(optarg);
			break;

		case 'F':
			do_fork = FALSE;
			break;

		case 'f':
			distfile = xstrdup(optarg);
			if (distfile[0] == '-' && distfile[1] == CNULL)
				fin = stdin;
			break;

		case 'm':
			addhostlist(optarg, &hostlist);
			break;

		case 'd':
			define(optarg);
			break;

		case 'D':
			debug = DM_ALL;
			if ((cp = msgparseopts("stdout=all,debug",
			    TRUE)) != NULL) {
				error("Enable debug messages failed: %s.", cp);
				usage();
			}
			break;

		case 'c':
			cmdargs++;
			break;

		case 'n':
			nflag++;
			break;

		case 'V':
			printf("%s\n", getversion());
			exit(0);

		case 'o':
			if (parsedistopts(optarg, &options, TRUE)) {
				error("Bad dist option string \"%s\".", 
				      optarg);
				usage();
			}
			break;

		case 'p':
			if (!optarg) {
				error("No path specified to \"-p\".");
				usage();
			}
			path_rdistd = xstrdup(optarg);
			break;

		case 'P':
			if (!optarg) {
				error("No path specified to \"-P\".");
				usage();
			}
			if ((cp = searchpath(optarg)) != NULL)
				path_remsh = xstrdup(cp);
			else {
				error("No component of path \"%s\" exists.",
				      optarg);
				usage();
			}
			break;

			/*
			 * These options are obsoleted by -o.  They are
			 * provided only for backwards compatibility
			 */
		case 'v':	FLAG_ON(options, DO_VERIFY);		break;
		case 'N':	FLAG_ON(options, DO_CHKNFS);		break;
		case 'O':	FLAG_ON(options, DO_CHKREADONLY);	break;
		case 'q':	FLAG_ON(options, DO_QUIET);		break;
		case 'b':	FLAG_ON(options, DO_COMPARE);		break;
		case 'r':	FLAG_ON(options, DO_NODESCEND);		break;
		case 'R':	FLAG_ON(options, DO_REMOVE);		break;
		case 's':	FLAG_ON(options, DO_SAVETARGETS);	break;
		case 'w':	FLAG_ON(options, DO_WHOLE);		break;
		case 'y':	FLAG_ON(options, DO_YOUNGER);		break;
		case 'h':	FLAG_ON(options, DO_FOLLOW);		break;
		case 'i':	FLAG_ON(options, DO_IGNLNKS);		break;
		case 'x':	FLAG_ON(options, DO_NOEXEC);		break;

		case '?':
		default:
			usage();
		}

	if (debug) {
		printf("%s\n", getversion());
		msgprconfig();
	}

	if (nflag && IS_ON(options, DO_VERIFY))
		fatalerr(
		 "The -n flag and \"verify\" mode may not both be used.");

	if (path_remsh == NULL) {
		if ((cp = getenv("RSH")) != NULL && *cp != '\0')
			path_remsh = cp;
		else
			path_remsh = _PATH_RSH;
	}

	/*
	 * Don't fork children for nflag
	 */
	if (nflag)
		do_fork = 0;

	if (cmdargs)
		docmdargs(realargc - optind, &realargv[optind]);
	else {
		if (fin == NULL)
			fin = opendist(distfile);
		(void) yyparse();
		/*
		 * Need to keep stdin open for child processing later
		 */
		if (fin != stdin)
			(void) fclose(fin);
		if (nerrs == 0)
			docmds(hostlist, realargc-optind, &realargv[optind]);
	}

	exit(nerrs != 0);
}

/*
 * Open a distfile
 */
FILE *
opendist(char *distfile)
{
	char *file = NULL;
	FILE *fp;

	if (distfile == NULL) {
		if (access("distfile", R_OK) == 0)
			file = "distfile";
		else if (access("Distfile", R_OK) == 0)
			file = "Distfile";
	} else {
		/*
		 * Try to test to see if file is readable before running m4.
		 */
		if (access(distfile, R_OK) != 0)
			fatalerr("%s: Cannot access file: %s.", 
				 distfile, SYSERR);
		file = distfile;
	}

	if (file == NULL)
		fatalerr("No distfile found.");

	fp = fopen(file, "r");

	if (fp == NULL)
		fatalerr("%s: open failed: %s.", file, SYSERR);

	return(fp);
}

/*
 * Print usage message and exit.
 */
static void
usage(void)
{
	extern char *__progname;

	(void) fprintf(stderr,
		"usage: %s [-DFnV] [-A num] [-a num] "
		"[-c mini_distfile]\n"
		"\t[-d var=value] [-f distfile] [-L remote_logopts] "
		"[-l local_logopts]\n"
		"\t[-M maxproc] [-m host] [-o distopts] [-P rsh-path] "
		"[-p rdistd-path]\n"
		"\t[-t timeout] [name ...]\n", __progname);


	(void) fprintf(stderr, "\nThe values for <distopts> are:\n\t%s\n",
		       getdistoptlist());

	msgprusage();

	exit(1);
}

/*
 * rcp like interface for distributing files.
 */
void
docmdargs(int nargs, char **args)
{
	struct namelist *nl, *prev;
	char *cp;
	struct namelist *files, *hosts;
	struct subcmd *scmds;
	char *dest;
	static struct namelist tnl;
	int i;

	if (nargs < 2)
		usage();

	prev = NULL;
	files = NULL;
	for (i = 0; i < nargs - 1; i++) {
		nl = makenl(args[i]);
		if (prev == NULL)
			files = prev = nl;
		else {
			prev->n_next = nl;
			prev = nl;
		}
	}

	cp = args[i];
	if ((dest = strchr(cp, ':')) != NULL)
		*dest++ = '\0';
	tnl.n_name = cp;
	tnl.n_regex = NULL;
	tnl.n_next = NULL;
	hosts = expand(&tnl, E_ALL);
	if (nerrs)
		exit(1);

	if (dest == NULL || *dest == '\0')
		scmds = NULL;
	else {
		scmds = makesubcmd(INSTALL);
		scmds->sc_options = options;
		scmds->sc_name = dest;
	}

	debugmsg(DM_MISC, "docmdargs()\nfiles = %s", getnlstr(files));
	debugmsg(DM_MISC, "host = %s", getnlstr(hosts));

	insert(NULL, files, hosts, scmds);
	docmds(NULL, 0, NULL);
}

/*
 * Get a list of NAME blocks (mostly for debugging).
 */
char *
getnlstr(struct namelist *nl)
{
	static char buf[16384];
	size_t len = 0;

	(void) snprintf(buf, sizeof(buf), "(");

	while (nl != NULL) {
		if (nl->n_name == NULL)
			continue;
		len += strlen(nl->n_name) + 2;
		if (len >= sizeof(buf)) {
			(void) strlcpy(buf,
				       "getnlstr() Buffer not large enough",
				       sizeof(buf));
			return(buf);
		}
		(void) strlcat(buf, " ", sizeof(buf));
		(void) strlcat(buf, nl->n_name, sizeof(buf));
		nl = nl->n_next;
	}

	(void) strlcat(buf, " )", sizeof(buf));

	return(buf);
}

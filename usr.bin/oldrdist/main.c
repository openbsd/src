/*	$OpenBSD: main.c,v 1.21 2009/10/27 23:59:41 deraadt Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <stdarg.h>
#include <libgen.h>
#include "defs.h"

#define NHOSTS 100

/*
 * Remote distribution program.
 */

char	*distfile = NULL;
#define _RDIST_TMP	"rdistXXXXXXXXXX"
char	tempfile[sizeof _PATH_TMP + sizeof _RDIST_TMP - 1];
char	*tempname;

int	debug;		/* debugging flag */
int	nflag;		/* NOP flag, just print commands without executing */
int	qflag;		/* Quiet. Don't print messages */
int	options;	/* global options */
int	iamremote;	/* act as remote server for transfering files */

FILE	*fin = NULL;	/* input file pointer */
int	rem = -1;	/* file descriptor to remote source/sink process */
char	host[MAXHOSTNAMELEN]; /* host name */
int	nerrs;		/* number of errors while sending/receiving */
char	user[MAXLOGNAME]; /* user's name */
char	homedir[MAXPATHLEN]; /* user's home directory */
uid_t	userid;		/* user's user ID */
gid_t	groupid;	/* user's group ID */

struct	passwd *pw;	/* pointer to static area used by getpwent */
struct	group *gr;	/* pointer to static area used by getgrent */

static void usage(void);
static void docmdargs(int, char *[]);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *arg;
	int cmdargs = 0;
	char *dhosts[NHOSTS], **hp = dhosts;

	pw = getpwuid(userid = getuid());
	if (pw == NULL) {
		fprintf(stderr, "%s: Who are you?\n", argv[0]);
		exit(1);
	}
	strlcpy(user, pw->pw_name, sizeof user);
	strlcpy(homedir, pw->pw_dir, sizeof homedir);
	groupid = pw->pw_gid;
	gethostname(host, sizeof(host));
	strlcpy(tempfile, _PATH_TMP, sizeof tempfile);
	strlcat(tempfile, _RDIST_TMP, sizeof tempfile);
	tempname = basename(tempfile);

	while (--argc > 0) {
		if ((arg = *++argv)[0] != '-')
			break;
		if (!strcmp(arg, "-Server"))
			iamremote++;
		else while (*++arg)
			switch (*arg) {
			case 'f':
				if (--argc <= 0)
					usage();
				distfile = *++argv;
				if (distfile[0] == '-' && distfile[1] == '\0')
					fin = stdin;
				break;

			case 'm':
				if (--argc <= 0)
					usage();
				if (hp >= &dhosts[NHOSTS-2]) {
					fprintf(stderr, "rdist: too many destination hosts\n");
					exit(1);
				}
				*hp++ = *++argv;
				break;

			case 'd':
				if (--argc <= 0)
					usage();
				define(*++argv);
				break;

			case 'D':
				debug++;
				break;

			case 'c':
				cmdargs++;
				break;

			case 'n':
				if (options & VERIFY) {
					printf("rdist: -n overrides -v\n");
					options &= ~VERIFY;
				}
				nflag++;
				break;

			case 'q':
				qflag++;
				break;

			case 'b':
				options |= COMPARE;
				break;

			case 'R':
				options |= REMOVE;
				break;

			case 'v':
				if (nflag) {
					printf("rdist: -n overrides -v\n");
					break;
				}
				options |= VERIFY;
				break;

			case 'w':
				options |= WHOLE;
				break;

			case 'y':
				options |= YOUNGER;
				break;

			case 'h':
				options |= FOLLOW;
				break;

			case 'i':
				options |= IGNLNKS;
				break;

			default:
				usage();
			}
	}
	*hp = NULL;

#if	defined(DIRECT_RCMD)
	seteuid(userid);
#else	/* DIRECT_RCMD */
	if (!iamremote && getuid() != geteuid()) {
		error("This version of rdist should not be installed setuid.\n");
		exit(1);
	}
#endif	/* DIRECT_RCMD */

	if (mktemp(tempfile) == NULL)
		fatal("cannot get temp file\n");

	if (iamremote) {
		server();
		exit(nerrs != 0);
	}

	if (cmdargs)
		docmdargs(argc, argv);
	else {
		if (fin == NULL) {
			if (distfile == NULL) {
				if ((fin = fopen("distfile","r")) == NULL)
					fin = fopen("Distfile", "r");
			} else
				fin = fopen(distfile, "r");
			if (fin == NULL) {
				perror(distfile ? distfile : "distfile");
				exit(1);
			}
		}
		yyparse();
		if (nerrs == 0)
			docmds(dhosts, argc, argv);
	}

	exit(nerrs != 0);
}

static void
usage()
{
	printf(
	    "usage: rdist [-bhinqRvwy] [-d var=value] [-f distfile] [-m host] [name ...]\n"
	    "       rdist [-bhinqRvwy] -c name ...  [login@]host[:dest]\n");
	exit(1);
}

/*
 * rcp like interface for distributing files.
 */
static void
docmdargs(nargs, args)
	int nargs;
	char *args[];
{
	struct namelist *nl, *prev;
	char *cp;
	struct namelist *files, *hosts;
	struct subcmd *cmds;
	char *dest;
	static struct namelist tnl = { NULL, NULL };
	int i;

	if (nargs < 2)
		usage();

	prev = NULL;
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
	hosts = expand(&tnl, E_ALL);
	if (nerrs)
		exit(1);

	if (dest == NULL || *dest == '\0')
		cmds = NULL;
	else {
		cmds = makesubcmd(INSTALL);
		cmds->sc_options = options;
		cmds->sc_name = dest;
	}

	if (debug) {
		printf("docmdargs()\nfiles = ");
		prnames(files);
		printf("hosts = ");
		prnames(hosts);
	}
	insert(NULL, files, hosts, cmds);
	docmds(NULL, 0, NULL);
}

/*
 * Print a list of NAME blocks (mostly for debugging).
 */
void
prnames(nl)
	struct namelist *nl;
{
	printf("( ");
	while (nl != NULL) {
		printf("%s ", nl->n_name);
		nl = nl->n_next;
	}
	printf(")\n");
}

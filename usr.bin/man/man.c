/*	$OpenBSD: man.c,v 1.30 2005/10/31 10:38:04 otto Exp $	*/
/*	$NetBSD: man.c,v 1.7 1995/09/28 06:05:34 tls Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994, 1995\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)man.c	8.17 (Berkeley) 1/31/95";
#else
static char rcsid[] = "$OpenBSD: man.c,v 1.30 2005/10/31 10:38:04 otto Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "pathnames.h"

int f_all, f_where;
static TAG *section;	/* could be passed to cleanup() instead */

extern char *__progname;

static void	 build_page(char *, char **);
static void	 cat(char *);
static char	*check_pager(char *);
static int	 cleanup(void);
static void	 how(char *);
static void	 jump(char **, char *, char *);
static int	 manual(char *, TAG *, glob_t *);
static void	 onsig(int);
static void	 usage(void);

sigset_t	blocksigs;

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	TAG *defp, *defnewp, *sectnewp, *subp;
	ENTRY *e_defp, *e_sectp, *e_subp, *ep;
	glob_t pg;
	size_t len;
	int ch, f_cat, f_how, found;
	char **ap, *cmd, *machine, *p, *p_add, *p_path, *pager, *sflag, *slashp;
	char *conffile, buf[MAXPATHLEN * 2];

	if (argv[1] == NULL && strcmp(basename(__progname), "help") == 0) {
		static char *nargv[3];
		nargv[0] = "man";
		nargv[1] = "help";
		nargv[2] = NULL;
		argv = nargv;
		argc = 2;
	}

	machine = sflag = NULL;
	f_cat = f_how = 0;
	conffile = p_add = p_path = NULL;
	while ((ch = getopt(argc, argv, "aC:cfhkM:m:P:s:S:w-")) != -1)
		switch (ch) {
		case 'a':
			f_all = 1;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'c':
		case '-':		/* Deprecated. */
			f_cat = 1;
			break;
		case 'h':
			f_how = 1;
			break;
		case 'm':
			p_add = optarg;
			break;
		case 'M':
		case 'P':		/* Backward compatibility. */
			p_path = optarg;
			break;
		case 's':		/* SVR4 compatibility. */
			sflag = optarg;
			break;
		case 'S':
			machine = optarg;
			break;
		/*
		 * The -f and -k options are backward compatible
		 * ways of calling whatis(1) and apropos(1).
		 */
		case 'f':
			jump(argv, "-f", "whatis");
			/* NOTREACHED */
		case 'k':
			jump(argv, "-k", "apropos");
			/* NOTREACHED */
		case 'w':
			f_all = f_where = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		usage();

	if (!f_cat && !f_how && !f_where) {
		if (!isatty(1))
			f_cat = 1;
		else if ((pager = getenv("MANPAGER")) != NULL &&
				(*pager != '\0'))
			pager = check_pager(pager);
		else if ((pager = getenv("PAGER")) != NULL && (*pager != '\0'))
			pager = check_pager(pager);
		else
			pager = _PATH_PAGER;
	}

	/* Read the configuration file. */
	config(conffile);

	/* Get the machine type unless specified by -S. */
	if (machine == NULL && (machine = getenv("MACHINE")) == NULL)
		machine = MACHINE;

	/* If there's no _default list, create an empty one. */
	if ((defp = getlist("_default")) == NULL)
		defp = addlist("_default");

	/*
	 * 1: If the user specified a MANPATH variable, or set the -M
	 *    option, we replace the _default list with the user's list,
	 *    appending the entries in the _subdir list and the machine.
	 */
	if (p_path == NULL)
		p_path = getenv("MANPATH");
	if (p_path != NULL) {
		while ((e_defp = TAILQ_FIRST(&defp->list)) != NULL) {
			free(e_defp->s);
			TAILQ_REMOVE(&defp->list, e_defp, q);
		}
		for (p = strtok(p_path, ":");
		    p != NULL; p = strtok(NULL, ":")) {
			slashp = p[strlen(p) - 1] == '/' ? "" : "/";
			e_subp = (subp = getlist("_subdir")) == NULL ?
			    NULL : TAILQ_FIRST(&subp->list);
			for (; e_subp != NULL; e_subp = TAILQ_NEXT(e_subp, q)) {
				(void)snprintf(buf, sizeof(buf), "%s%s%s{/%s,}",
				    p, slashp, e_subp->s, machine);
				if ((ep = malloc(sizeof(ENTRY))) == NULL ||
				    (ep->s = strdup(buf)) == NULL)
					err(1, NULL);
				TAILQ_INSERT_TAIL(&defp->list, ep, q);
			}
		}
	}

	/*
	 * 2: If the user did not specify MANPATH, -M or a section, rewrite
	 *    the _default list to include the _subdir list and the machine.
	 */
	if (sflag == NULL && argv[1] == NULL)
		section = NULL;
	else {
		if (sflag != NULL && (section = getlist(sflag)) == NULL)
			errx(1, "unknown manual section `%s'", sflag);
		else if (sflag == NULL && (section = getlist(*argv)) != NULL)
			++argv;
	}
	if (p_path == NULL && section == NULL) {
		defnewp = addlist("_default_new");
		e_defp = TAILQ_FIRST(&defp->list);
		for (; e_defp != NULL; e_defp = TAILQ_NEXT(e_defp, q)) {
			slashp =
			    e_defp->s[strlen(e_defp->s) - 1] == '/' ? "" : "/";
			e_subp = (subp = getlist("_subdir")) == NULL ?
			    NULL : TAILQ_FIRST(&subp->list);
			for (; e_subp != NULL; e_subp = TAILQ_NEXT(e_subp, q)) {
				(void)snprintf(buf, sizeof(buf), "%s%s%s{/%s,}",
				e_defp->s, slashp, e_subp->s, machine);
				if ((ep = malloc(sizeof(ENTRY))) == NULL ||
				    (ep->s = strdup(buf)) == NULL)
					err(1, NULL);
				TAILQ_INSERT_TAIL(&defnewp->list, ep, q);
			}
		}
		defp = getlist("_default");
		while ((e_defp = TAILQ_FIRST(&defp->list)) != NULL) {
			free(e_defp->s);
			TAILQ_REMOVE(&defp->list, e_defp, q);
		}
		free(defp->s);
		TAILQ_REMOVE(&head, defp, q);
		defnewp = getlist("_default_new");
		free(defnewp->s);
		defnewp->s = "_default";
		defp = defnewp;
	}

	/*
	 * 3: If the user set the -m option, insert the user's list before
	 *    whatever list we have, again appending the _subdir list and
	 *    the machine.
	 */
	if (p_add != NULL) {
		e_sectp = NULL;
		for (p = strtok(p_add, ":"); p != NULL; p = strtok(NULL, ":")) {
			slashp = p[strlen(p) - 1] == '/' ? "" : "/";
			e_subp = (subp = getlist("_subdir")) == NULL ?
			    NULL : TAILQ_FIRST(&subp->list);
			for (; e_subp != NULL; e_subp = TAILQ_NEXT(e_subp, q)) {
				(void)snprintf(buf, sizeof(buf), "%s%s%s{/%s,}",
				    p, slashp, e_subp->s, machine);
				if ((ep = malloc(sizeof(ENTRY))) == NULL ||
				    (ep->s = strdup(buf)) == NULL)
					err(1, NULL);

				if (e_sectp == NULL)
					TAILQ_INSERT_HEAD(&defp->list, ep, q);
				else
					TAILQ_INSERT_AFTER(&defp->list, e_sectp,
					    ep, q);
				e_sectp = ep;
			}
		}
	}
	/*
	 * 4: If no -m was specified, and a section was, rewrite the section's
	 *    paths (if they have a trailing slash) to append the _subdir list
	 *    and the machine.  This then becomes the _default list.
	 */
	if (p_add == NULL && section != NULL) {
		sectnewp = addlist("_section_new");
		TAILQ_FOREACH(e_sectp, &section->list, q) {
			if (e_sectp->s[strlen(e_sectp->s) - 1] != '/') {
				(void)snprintf(buf, sizeof(buf),
				    "%s{/%s,}", e_sectp->s, machine);
				if ((ep = malloc(sizeof(ENTRY))) == NULL ||
				    (ep->s = strdup(buf)) == NULL)
					err(1, NULL);
				TAILQ_INSERT_TAIL(&sectnewp->list, ep, q);
				continue;
			}
			e_subp = (subp = getlist("_subdir")) == NULL ?
			    NULL : TAILQ_FIRST(&subp->list);
			for (; e_subp != NULL; e_subp = TAILQ_NEXT(e_subp, q)) {
				(void)snprintf(buf, sizeof(buf), "%s%s{/%s,}",
				    e_sectp->s, e_subp->s, machine);
				if ((ep = malloc(sizeof(ENTRY))) == NULL ||
				    (ep->s = strdup(buf)) == NULL)
					err(1, NULL);
				TAILQ_INSERT_TAIL(&sectnewp->list, ep, q);
			}
		}
		sectnewp->s = section->s;
		defp = sectnewp;
		TAILQ_REMOVE(&head, section, q);
	}

	/*
	 * 5: Search for the files.  Set up an interrupt handler, so the
	 *    temporary files go away.
	 */
	(void)signal(SIGINT, onsig);
	(void)signal(SIGHUP, onsig);

	sigemptyset(&blocksigs);
	sigaddset(&blocksigs, SIGINT);
	sigaddset(&blocksigs, SIGHUP);

	memset(&pg, 0, sizeof(pg));
	for (found = 0; *argv; ++argv)
		if (manual(*argv, defp, &pg))
			found = 1;

	/* 6: If nothing found, we're done. */
	if (!found) {
		(void)cleanup();
		exit (1);
	}

	/* 7: If it's simple, display it fast. */
	if (f_cat) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			cat(*ap);
		}
		exit (cleanup());
	}
	if (f_how) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			how(*ap);
		}
		exit(cleanup());
	}
	if (f_where) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			(void)puts(*ap);
		}
		exit(cleanup());
	}

	/*
	 * 8: We display things in a single command; build a list of things
	 *    to display.
	 */
	for (ap = pg.gl_pathv, len = strlen(pager) + 1; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len += strlen(*ap) + 1;
	}
	if ((cmd = malloc(len)) == NULL) {
		warn(NULL);
		(void)cleanup();
		exit(1);
	}
	p = cmd;
	len = strlen(pager);
	memcpy(p, pager, len);
	p += len;
	*p++ = ' ';
	for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len = strlen(*ap);
		memcpy(p, *ap, len);
		p += len;
		*p++ = ' ';
	}
	*--p = '\0';

	/* Use system(3) in case someone's pager is "pager arg1 arg2". */
	(void)system(cmd);

	exit(cleanup());
}

/*
 * manual --
 *	Search the manuals for the pages.
 */
static int
manual(char *page, TAG *tag, glob_t *pg)
{
	ENTRY *ep, *e_sufp, *e_tag;
	TAG *missp, *sufp;
	int anyfound, cnt, found;
	char *p, buf[MAXPATHLEN];

	anyfound = 0;
	buf[0] = '*';

	/* For each element in the list... */
	e_tag = tag == NULL ? NULL : TAILQ_FIRST(&tag->list);
	for (; e_tag != NULL; e_tag = TAILQ_NEXT(e_tag, q)) {
		(void)snprintf(buf, sizeof(buf), "%s/%s.*", e_tag->s, page);
		if (glob(buf,
		    GLOB_APPEND | GLOB_BRACE | GLOB_NOSORT | GLOB_QUOTE,
		    NULL, pg)) {
			warn("globbing");
			(void)cleanup();
			exit(1);
		}
		if (pg->gl_matchc == 0)
			continue;

		/* Find out if it's really a man page. */
		for (cnt = pg->gl_pathc - pg->gl_matchc;
		    cnt < pg->gl_pathc; ++cnt) {

			/*
			 * Try the _suffix key words first.
			 *
			 * XXX
			 * Older versions of man.conf didn't have the suffix
			 * key words, it was assumed that everything was a .0.
			 * We just test for .0 first, it's fast and probably
			 * going to hit.
			 */
			(void)snprintf(buf, sizeof(buf), "*/%s.0", page);
			if (!fnmatch(buf, pg->gl_pathv[cnt], 0))
				goto next;

			e_sufp = (sufp = getlist("_suffix")) == NULL ?
			    NULL : TAILQ_FIRST(&sufp->list);
			for (found = 0;
			    e_sufp != NULL; e_sufp = TAILQ_NEXT(e_sufp, q)) {
				(void)snprintf(buf,
				     sizeof(buf), "*/%s%s", page, e_sufp->s);
				if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
					found = 1;
					break;
				}
			}
			if (found)
				goto next;

			/* Try the _build key words next. */
			e_sufp = (sufp = getlist("_build")) == NULL ?
			    NULL : TAILQ_FIRST(&sufp->list);
			for (found = 0;
			    e_sufp != NULL; e_sufp = TAILQ_NEXT(e_sufp, q)) {
				for (p = e_sufp->s;
				    *p != '\0' && !isspace(*p); ++p);
				if (*p == '\0')
					continue;
				*p = '\0';
				(void)snprintf(buf,
				     sizeof(buf), "*/%s%s", page, e_sufp->s);
				if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
					if (!f_where)
						build_page(p + 1,
						    &pg->gl_pathv[cnt]);
					*p = ' ';
					found = 1;
					break;
				}
				*p = ' ';
			}
			if (found) {
next:				anyfound = 1;
				if (!f_all) {
					/* Delete any other matches. */
					while (++cnt< pg->gl_pathc)
						pg->gl_pathv[cnt] = "";
					break;
				}
				continue;
			}

			/* It's not a man page, forget about it. */
			pg->gl_pathv[cnt] = "";
		}

		if (anyfound && !f_all)
			break;
	}

	/* If not found, enter onto the missing list. */
	if (!anyfound) {
		sigset_t osigs;

		sigprocmask(SIG_BLOCK, &blocksigs, &osigs);

		if ((missp = getlist("_missing")) == NULL)
			missp = addlist("_missing");
		if ((ep = malloc(sizeof(ENTRY))) == NULL ||
		    (ep->s = strdup(page)) == NULL) {
			warn(NULL);
			(void)cleanup();
			exit(1);
		}
		TAILQ_INSERT_TAIL(&missp->list, ep, q);
		sigprocmask(SIG_SETMASK, &osigs, NULL);
	}
	return (anyfound);
}

/*
 * build_page --
 *	Build a man page for display.
 */
static void
build_page(char *fmt, char **pathp)
{
	static int warned;
	ENTRY *ep;
	TAG *intmpp;
	int fd, n;
	char *p, *b;
	char buf[MAXPATHLEN], cmd[MAXPATHLEN], tpath[MAXPATHLEN];
	sigset_t osigs;

	/* Let the user know this may take awhile. */
	if (!warned) {
		warned = 1;
		warnx("Formatting manual page...");
	}

       /*
        * Historically man chdir'd to the root of the man tree.
        * This was used in man pages that contained relative ".so"
        * directives (including other man pages for command aliases etc.)
        * It even went one step farther, by examining the first line
        * of the man page and parsing the .so filename so it would
        * make hard(?) links to the cat'ted man pages for space savings.
        * (We don't do that here, but we could).
        */

       /* copy and find the end */
       for (b = buf, p = *pathp; (*b++ = *p++) != '\0';)
               continue;

       /* skip the last two path components, page name and man[n] */
       for (--b, n = 2; b != buf; b--)
               if (*b == '/')
                       if (--n == 0) {
                               *b = '\0';
                               (void) chdir(buf);
                       }


	/* Add a remove-when-done list. */
	sigprocmask(SIG_BLOCK, &blocksigs, &osigs);
	if ((intmpp = getlist("_intmp")) == NULL)
		intmpp = addlist("_intmp");
	sigprocmask(SIG_SETMASK, &osigs, NULL);

	/* Move to the printf(3) format string. */
	for (; *fmt && isspace(*fmt); ++fmt)
		;

	/*
	 * Get a temporary file and build a version of the file
	 * to display.  Replace the old file name with the new one.
	 */
	(void)strlcpy(tpath, _PATH_TMPFILE, sizeof(tpath));
	if ((fd = mkstemp(tpath)) == -1) {
		warn("%s", tpath);
		(void)cleanup();
		exit(1);
	}
	(void)snprintf(buf, sizeof(buf), "%s > %s", fmt, tpath);
	(void)snprintf(cmd, sizeof(cmd), buf, *pathp);
	(void)system(cmd);
	(void)close(fd);
	if ((*pathp = strdup(tpath)) == NULL) {
		warn(NULL);
		(void)cleanup();
		exit(1);
	}

	/* Link the built file into the remove-when-done list. */
	if ((ep = malloc(sizeof(ENTRY))) == NULL) {
		warn(NULL);
		(void)cleanup();
		exit(1);
	}
	ep->s = *pathp;
	TAILQ_INSERT_TAIL(&intmpp->list, ep, q);
}

/*
 * how --
 *	display how information
 */
static void
how(char *fname)
{
	FILE *fp;

	int lcnt, print;
	char *p, buf[256];

	if (!(fp = fopen(fname, "r"))) {
		warn("%s", fname);
		(void)cleanup();
		exit (1);
	}
#define	S1	"SYNOPSIS"
#define	S2	"S\bSY\bYN\bNO\bOP\bPS\bSI\bIS\bS"
#define	D1	"DESCRIPTION"
#define	D2	"D\bDE\bES\bSC\bCR\bRI\bIP\bPT\bTI\bIO\bON\bN"
	for (lcnt = print = 0; fgets(buf, sizeof(buf), fp);) {
		if (!strncmp(buf, S1, sizeof(S1) - 1) ||
		    !strncmp(buf, S2, sizeof(S2) - 1)) {
			print = 1;
			continue;
		} else if (!strncmp(buf, D1, sizeof(D1) - 1) ||
		    !strncmp(buf, D2, sizeof(D2) - 1))
			return;
		if (!print)
			continue;
		if (*buf == '\n')
			++lcnt;
		else {
			while (lcnt) {
				--lcnt;
				(void)putchar('\n');
			}
			for (p = buf; isspace(*p); ++p)
				;
			(void)fputs(p, stdout);
		}
	}
	(void)fclose(fp);
}

/*
 * cat --
 *	cat out the file
 */
static void
cat(char *fname)
{
	int fd, n;
	char buf[2048];

	if ((fd = open(fname, O_RDONLY, 0)) < 0) {
		warn("%s", fname);
		(void)cleanup();
		exit(1);
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		if (write(STDOUT_FILENO, buf, n) != n) {
			warn("write");
			(void)cleanup();
			exit (1);
		}
	if (n == -1) {
		warn("read");
		(void)cleanup();
		exit(1);
	}
	(void)close(fd);
}

/*
 * check_pager --
 *	check the user supplied page information
 */
static char *
check_pager(char *name)
{
	char *p, *save;
	int len;

	/*
	 * if the user uses "more", we make it "more -s"; watch out for
	 * PAGER = "mypager /usr/bin/more"
	 */
	for (p = name; *p && !isspace(*p); ++p)
		;
	for (; p > name && *p != '/'; --p)
		;
	if (p != name)
		++p;

	/* make sure it's "more", not "morex" */
	if (!strncmp(p, "more", 4) && (p[4] == '\0' || isspace(p[4]))){
		save = name;
		/* allocate space to add the "-s" */
		len = strlen(save) + 1 + sizeof("-s");
		if ((name = malloc(len)) == NULL)
			err(1, NULL);
		(void)snprintf(name, len, "%s %s", save, "-s");
	}
	return(name);
}

/*
 * jump --
 *	strip out flag argument and jump
 */
static void
jump(char **argv, char *flag, char *name)
{
	char **arg;

	argv[0] = name;
	for (arg = argv + 1; *arg; ++arg)
		if (!strcmp(*arg, flag))
			break;
	for (; *arg; ++arg)
		arg[0] = arg[1];
	execvp(name, argv);
	(void)fprintf(stderr, "%s: Command not found.\n", name);
	exit(1);
}

/*
 * onsig --
 *	If signaled, delete the temporary files.
 */
static void
onsig(int signo)
{
	(void)cleanup();	/* XXX signal race */

	(void)signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);

	/* NOTREACHED */
	_exit(1);
}

/*
 * cleanup --
 *	Clean up temporary files, show any error messages.
 */
static int
cleanup(void)
{
	TAG *intmpp, *missp;
	ENTRY *ep;
	int rval;

	rval = 0;
	ep = (missp = getlist("_missing")) == NULL ?
	    NULL : TAILQ_FIRST(&missp->list);
	if (ep != NULL)
		for (; ep != NULL; ep = TAILQ_NEXT(ep, q)) {
			if (section)
				warnx("no entry for %s in section %s of the manual.",
					ep->s, section->s);
			else
				warnx("no entry for %s in the manual.", ep->s);
			rval = 1;
		}

	ep = (intmpp = getlist("_intmp")) == NULL ?
	    NULL : TAILQ_FIRST(&intmpp->list);
	for (; ep != NULL; ep = TAILQ_NEXT(ep, q))
		(void)unlink(ep->s);
	return (rval);
}

/*
 * usage --
 *	print usage message and die
 */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-achw] [-C file] [-M path] [-m path] "
	    "[-S subsection] [-s section]\n\t   [section] name [...]\n",
	    __progname);
	(void)fprintf(stderr, "usage: %s -f command\n", __progname);
	(void)fprintf(stderr, "usage: %s -k keyword\n", __progname);
	exit(1);
}

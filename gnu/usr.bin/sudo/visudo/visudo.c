/*	$OpenBSD: visudo.c,v 1.11 1999/03/29 20:29:08 millert Exp $	*/

/*
 *  CU sudo version 1.5.9
 *  Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 *  visudo.c -- locks the sudoers file for safe editing and check
 *  for parse errors.
 *
 *  Todd C. Miller <Todd.Miller@courtesan.com> Sat Mar 25 21:50:36 MST 1995
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
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>

#include "sudo.h"
#include "version.h"

#ifndef STDC_HEADERS
#ifndef __GNUC__	/* gcc has its own malloc */
extern char *malloc	__P((size_t));
#endif /* __GNUC__ */
extern char *getenv	__P((const char *));
extern int stat		__P((const char *, struct stat *));
#endif /* !STDC_HEADERS */

#if defined(POSIX_SIGNALS) && !defined(SA_RESETHAND)
#define SA_RESETHAND    0
#endif /* POSIX_SIGNALS && !SA_RESETHAND */

#ifndef lint
static const char rcsid[] = "$Sudo: visudo.c,v 1.100 1999/03/29 04:05:14 millert Exp $";
#endif /* lint */

/*
 * Function prototypes
 */
static void usage		__P((void));
static char whatnow		__P((void));
static void whatnow_help	__P((void));
static RETSIGTYPE Exit		__P((int));
static void setup_signals	__P((void));
int command_matches		__P((char *, char *, char *, char *));
int addr_matches		__P((char *));
int netgr_matches		__P((char *, char *, char *));
int usergr_matches		__P((char *, char *));
void init_parser		__P((void));


/*
 * External globals
 */
extern FILE *yyin, *yyout;
extern int errorlineno, sudolineno;


/*
 * Globals
 */
char **Argv;
char **NewArgv = NULL;
int NewArgc = 0;
char *sudoers = _PATH_SUDO_SUDOERS;
char *stmp = _PATH_SUDO_STMP;
int parse_error = FALSE;
char *runas_user = RUNAS_DEFAULT;

/*
 * For the parsing routines
 */
char host[] = "";
char *shost = "";
char *cmnd = "";
char *cmnd_args = NULL;
struct passwd *user_pw_ent;


/********************************************************************
 *
 *  main()
 *
 *  where it all begins...
 */

int main(argc, argv)
    int argc;
    char **argv;
{
    char buf[MAXPATHLEN*2];		/* buffer used for copying files */
    char * Editor = EDITOR;		/* editor to use (default is EDITOR */
    int sudoers_fd;			/* sudoers file descriptor */
    int stmp_fd;			/* stmp file descriptor */
    int n;				/* length parameter */

    (void) setbuf(stderr, (char *)NULL);	/* unbuffered stderr */

    /*
     * Parse command line options
     */
    Argv = argv;

    /*
     * If passesd -V then print version, else print usage
     * if any other option...
     */
    if (argc == 2) {
	if (!strcmp(Argv[1], "-V")) {
	    (void) printf("visudo version %s\n", version);
	    exit(0);
	} else {
	    usage();
	}
    } else if (argc != 1) {
	usage();
    }

    /* user_pw_ent needs to point to something... */
    if ((user_pw_ent = getpwuid(getuid())) == NULL) {
	(void) fprintf(stderr, "%s: Can't find you in the passwd database: ",
	    Argv[0]);
	perror(stmp);
	exit(1);
    }

#ifdef ENV_EDITOR
    /*
     * If we are allowing EDITOR and VISUAL envariables set Editor
     * base on whichever exists...
     */
    if (!(Editor = getenv("EDITOR")))
	if (!(Editor = getenv("VISUAL")))
	    Editor = EDITOR;
#endif /* ENV_EDITOR */

    /*
     * Copy sudoers file to stmp
     */
    stmp_fd = open(stmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (stmp_fd < 0) {
	if (errno == EEXIST) {
	    (void) fprintf(stderr, "%s: sudoers file busy, try again later.\n",
		Argv[0]);
	    exit(1);
	}
	(void) fprintf(stderr, "%s: ", Argv[0]);
	perror(stmp);
	Exit(-1);
    }

    /* install signal handler to clean up stmp */
    setup_signals();

    sudoers_fd = open(sudoers, O_RDONLY);
    if (sudoers_fd < 0 && errno != ENOENT) {
	(void) fprintf(stderr, "%s: ", Argv[0]);
	perror(sudoers);
	Exit(-1);
    }

    /*
     * Copy the data
     */
    if (sudoers_fd >= 0) {
	while ((n = read(sudoers_fd, buf, sizeof(buf))) > 0)
	    if (write(stmp_fd, buf, n) != n) {
		(void) fprintf(stderr, "%s: Write failed: ", Argv[0]);
		perror("");
		Exit(-1);
	    }

	(void) close(sudoers_fd);
    }
    (void) close(stmp_fd);

    /*
     * Edit the temp file and parse it (for sanity checking)
     */
    do {
	/*
	 * Build up a buffer to execute
	 */
	if (strlen(Editor) + strlen(stmp) + 30 > sizeof(buf)) {
	    (void) fprintf(stderr, "%s: Buffer too short (line %d).\n",
			   Argv[0], __LINE__);
	    Exit(-1);
	}
	if (parse_error == TRUE)
	    (void) sprintf(buf, "%s +%d %s", Editor, errorlineno, stmp);
	else
	    (void) sprintf(buf, "%s %s", Editor, stmp);

	/* do the edit -- some SYSV editors return 256 instead of 0 */
	n = system(buf);
	if (n == 0 || n == 256) {
	    struct stat statbuf;	/* for sanity checking */

	    /* make sure stmp exists */
	    if (stat(stmp, &statbuf) < 0) {
		(void) fprintf(stderr,
		    "%s: Can't stat temporary file (%s), %s unchanged.\n",
		    Argv[0], stmp, sudoers);
		Exit(-1);
	    }

	    /* check for zero length file */
	    if (statbuf.st_size == 0) {
		(void) fprintf(stderr,
		    "%s: Zero length temporary file (%s), %s unchanged.\n",
		    Argv[0], stmp, sudoers);
		Exit(-1);
	    }

	    /*
	     * passed sanity checks so reopen stmp file and check
	     * for parse errors.
	     */
	    yyout = stdout;
	    if (parse_error)
		yyin = freopen(stmp, "r", yyin);
	    else
		yyin = fopen(stmp, "r");
	    if (yyin == NULL) {
		(void) fprintf(stderr,
		    "%s: Can't re-open temporary file (%s), %s unchanged.\n",
		    Argv[0], stmp, sudoers);
		Exit(-1);
	    }

	    /* clean slate for each parse */
	    init_parser();

	    /* parse the sudoers file */
	    if (yyparse() && parse_error != TRUE) {
		(void) fprintf(stderr,
		    "%s: Failed to parse temporary file (%s), unknown error.\n",
		    Argv[0], stmp);
		parse_error = TRUE;
	    }
	} else {
	    (void) fprintf(stderr, "%s: Editor (%s) failed, %s unchanged.\n",
		Argv[0], Editor, sudoers);
	    Exit(-1);
	}

	/*
	 * Prompt the user for what to do now
	 */
	if (parse_error == TRUE) {
	    switch (whatnow()) {
		case 'q' :	parse_error = FALSE;	/* ignore parse error */
				break;
		case 'x' :	Exit(0);
				break;
	    }
	}
    } while (parse_error == TRUE);

    /*
     * Change mode and ownership of temp file so when
     * we move it to sudoers things are kosher.
     */
    if (chown(stmp, SUDOERS_UID, SUDOERS_GID)) {
	(void) fprintf(stderr,
	    "%s: Unable to set (uid, gid) of %s to (%d, %d): ",
	    Argv[0], stmp, SUDOERS_UID, SUDOERS_GID);
	perror("");
	Exit(-1);
    }
    if (chmod(stmp, SUDOERS_MODE)) {
	(void) fprintf(stderr,
	    "%s: Unable to change mode of %s to %o: ",
	    Argv[0], stmp, SUDOERS_MODE);
	perror("");
	Exit(-1);
    }

    /*
     * Now that we have a sane stmp file (parse ok) it needs to be
     * rename(2)'d to sudoers.  If the rename(2) fails we try using
     * mv(1) in case stmp and sudoers are on different filesystems.
     */
    if (rename(stmp, sudoers)) {
	if (errno == EXDEV) {
	    char *tmpbuf;

	    (void) fprintf(stderr,
	      "%s: %s and %s not on the same filesystem, using mv to rename.\n",
	      Argv[0], stmp, sudoers);

	    /* Allocate just enough space for tmpbuf */
	    n = sizeof(char) * (strlen(_PATH_MV) + strlen(stmp) +
		  strlen(sudoers) + 4);
	    if ((tmpbuf = (char *) malloc(n)) == NULL) {
		(void) fprintf(stderr,
			      "%s: Cannot alocate memory, %s unchanged: ",
			      Argv[0], sudoers);
		perror("");
		Exit(-1);
	    }

	    /* Build up command and execute it */
	    (void) sprintf(tmpbuf, "%s %s %s", _PATH_MV, stmp, sudoers);
	    if (system(tmpbuf)) {
		(void) fprintf(stderr,
			       "%s: Command failed: '%s', %s unchanged.\n",
			       Argv[0], tmpbuf, sudoers);
		Exit(-1);
	    }
	    (void) free(tmpbuf);
	} else {
	    (void) fprintf(stderr, "%s: Error renaming %s, %s unchanged: ",
				   Argv[0], stmp, sudoers);
	    perror("");
	    Exit(-1);
	}
    }

    return(0);
}


/********************************************************************
 *
 *  dummy *_matches routines
 *
 *  These exist to allow us to use the same parser as sudo(8).
 */

int command_matches(cmnd, user_args, path, sudoers_args)
    char *cmnd;
    char *user_args;
    char *path;
    char *sudoers_args;
{
    return(TRUE);
}


int addr_matches(n)
    char *n;
{
    return(TRUE);
}

int usergr_matches(g, u)
    char *g, *u;
{
    return(TRUE);
}


int netgr_matches(n, h, u)
    char *n, *h, *u;
{
    return(TRUE);
}


/********************************************************************
 *
 *  usage()
 *
 *  Prints a help message and exits w/ exit value of 1.
 */

static void usage()
{
    (void) fprintf(stderr, "usage: %s [-V]\n", Argv[0]);
    Exit(-1);
}


/********************************************************************
 *
 *  Exit()
 *
 *  Unlinks the sudoers temp file (if it exists) and exits.
 *  Used in place of a normal exit() and as a signal handler.
 *  A positive parameter is considered to be a signal and is reported.
 */

static RETSIGTYPE Exit(sig)
    int sig;
{
    (void) unlink(stmp);

    if (sig > 0)
	(void) fprintf(stderr, "%s exiting, caught signal %d.\n", Argv[0], sig);

    exit(-sig);
}


/********************************************************************
 *
 *  whatnow()
 *
 *  Assuming a parse error occurred, prompt the user for what they want
 *  to do now.  Returns first letter of their choice (always lowercase).
 */

static char whatnow()
{
    char choice;
    int ok;

    do {
	ok = FALSE;
	(void) fputs("What now? ", stdout);
	if ((choice = getchar()) != '\n')
	    while (getchar() != '\n')
		;

	if (choice == 'e' || choice == 'x' || choice == 'Q')
	    ok = TRUE;

	/* help message if they gavce us garbage */
	if (!ok)
	    whatnow_help();

    } while (!ok);

    return(choice);
}


/********************************************************************
 *
 *  whatnow_help()
 *
 *  Print out a help message for whatnow().
 */

static void whatnow_help()
{
    (void) printf("Options are:\n");
    (void) printf("  (e)dit sudoers file again\n");
    (void) printf("  e(x)it without saving changes to sudoers file\n");
    (void) printf("  (Q)uit and save changes to sudoers file (DANGER!)\n\n");
}


/********************************************************************
 *
 *  setup_signals()
 *
 *  Install signal handlers for visudo.
 */

static void setup_signals()
{
#ifdef POSIX_SIGNALS
	struct sigaction action;		/* posix signal structure */
#endif /* POSIX_SIGNALS */

	/*
	 * Setup signal handlers
	 */
#ifdef POSIX_SIGNALS
	(void) memset((VOID *)&action, 0, sizeof(action));
	action.sa_handler = Exit;
	action.sa_flags = SA_RESETHAND;
	(void) sigaction(SIGILL, &action, NULL);
	(void) sigaction(SIGTRAP, &action, NULL);
	(void) sigaction(SIGBUS, &action, NULL);
	(void) sigaction(SIGSEGV, &action, NULL);
	(void) sigaction(SIGTERM, &action, NULL);

	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	(void) sigaction(SIGHUP, &action, NULL);
	(void) sigaction(SIGINT, &action, NULL);
	(void) sigaction(SIGQUIT, &action, NULL);
#else
	(void) signal(SIGILL, Exit);
	(void) signal(SIGTRAP, Exit);
	(void) signal(SIGBUS, Exit);
	(void) signal(SIGSEGV, Exit);
	(void) signal(SIGTERM, Exit);

	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
#endif /* POSIX_SIGNALS */
}

/*	$OpenBSD: cvs.c,v 1.13 2004/11/09 21:01:36 krapht Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "file.h"


extern char *__progname;


/* verbosity level: 0 = really quiet, 1 = quiet, 2 = verbose */
int verbosity = 2;



/* compression level used with zlib, 0 meaning no compression taking place */
int   cvs_compress = 0;
int   cvs_trace = 0;
int   cvs_nolog = 0;
int   cvs_readonly = 0;
int   cvs_nocase = 0;   /* set to 1 to disable case sensitivity on filenames */

/* name of the command we are running */
char *cvs_command;
int   cvs_cmdop;
char *cvs_rootstr;
char *cvs_rsh = CVS_RSH_DEFAULT;
char *cvs_editor = CVS_EDITOR_DEFAULT;

char *cvs_msg = NULL;


/* hierarchy of all the files affected by the command */
CVSFILE *cvs_files;



/*
 * Command dispatch table
 * ----------------------
 *
 * The synopsis field should only contain the list of arguments that the
 * command supports, without the actual command's name.
 *
 * Command handlers are expected to return 0 if no error occured, or one of
 * the values known in sysexits.h in case of an error.  In case the error
 * returned is EX_USAGE, the command's usage string is printed to standard
 * error before returning.
 */

static struct cvs_cmd {
	int     cmd_op;
	char    cmd_name[CVS_CMD_MAXNAMELEN];
	char    cmd_alias[CVS_CMD_MAXALIAS][CVS_CMD_MAXNAMELEN];
	int   (*cmd_hdlr)(int, char **);
	char   *cmd_synopsis;
	char   *cmd_opts;
	char    cmd_descr[CVS_CMD_MAXDESCRLEN];
} cvs_cdt[] = {
	{
		CVS_OP_ADD, "add",      { "ad",  "new" }, cvs_add,
		"[-m msg] file ...",
		"",
		"Add a new file/directory to the repository",
	},
	{
		-1, "admin",    { "adm", "rcs" }, NULL,
		"",
		"",
		"Administration front end for rcs",
	},
	{
		CVS_OP_ANNOTATE, "annotate", { "ann"        }, NULL,
		"",
		"",
		"Show last revision where each line was modified",
	},
	{
		CVS_OP_CHECKOUT, "checkout", { "co",  "get" }, cvs_checkout,
		"",
		"",
		"Checkout sources for editing",
	},
	{
		CVS_OP_COMMIT, "commit",   { "ci",  "com" }, cvs_commit,
		"[-flR] [-F logfile | -m msg] [-r rev] ...",
		"F:flm:Rr:",
		"Check files into the repository",
	},
	{
		CVS_OP_DIFF, "diff",     { "di",  "dif" }, cvs_diff,
		"[-cilu] [-D date] [-r rev] ...",
		"cD:ilur:",
		"Show differences between revisions",
	},
	{
		-1, "edit",     {              }, NULL,
		"",
		"",
		"Get ready to edit a watched file",
	},
	{
		-1, "editors",  {              }, NULL,
		"",
		"",
		"See who is editing a watched file",
	},
	{
		-1, "export",   { "ex",  "exp" }, NULL,
		"",
		"",
		"Export sources from CVS, similar to checkout",
	},
	{
		CVS_OP_HISTORY, "history",  { "hi",  "his" }, cvs_history,
		"",
		"",
		"Show repository access history",
	},
	{
		CVS_OP_IMPORT, "import",   { "im",  "imp" }, cvs_import,
		"[-d] [-b branch] [-I ign] [-k subst] [-m msg] "
		"repository vendor-tag release-tags ...",
		"b:dI:k:m:",
		"Import sources into CVS, using vendor branches",
	},
	{
		CVS_OP_INIT, "init",     {              }, cvs_init,
		"",
		"",
		"Create a CVS repository if it doesn't exist",
	},
#if defined(HAVE_KERBEROS)
	{
		"kserver",  {}, NULL
		"",
		"",
		"Start a Kerberos authentication CVS server",
	},
#endif
	{
		CVS_OP_LOG, "log",      { "lo"         }, cvs_getlog,
		"",
		"",
		"Print out history information for files",
	},
	{
		-1, "login",    {}, NULL,
		"",
		"",
		"Prompt for password for authenticating server",
	},
	{
		-1, "logout",   {}, NULL,
		"",
		"",
		"Removes entry in .cvspass for remote repository",
	},
	{
		-1, "rdiff",    {}, NULL,
		"",
		"",
		"Create 'patch' format diffs between releases",
	},
	{
		-1, "release",  {}, NULL,
		"",
		"",
		"Indicate that a Module is no longer in use",
	},
	{
		CVS_OP_REMOVE, "remove",   {}, NULL,
		"",
		"",
		"Remove an entry from the repository",
	},
	{
		-1, "rlog",     {}, NULL,
		"",
		"",
		"Print out history information for a module",
	},
	{
		-1, "rtag",     {}, NULL,
		"",
		"",
		"Add a symbolic tag to a module",
	},
	{
		CVS_OP_SERVER, "server",   {}, cvs_server,
		"",
		"",
		"Server mode",
	},
	{
		CVS_OP_STATUS, "status",   {}, cvs_status,
		"",
		"",
		"Display status information on checked out files",
	},
	{
		CVS_OP_TAG, "tag",      { "ta", }, NULL,
		"",
		"",
		"Add a symbolic tag to checked out version of files",
	},
	{
		-1, "unedit",   {}, NULL,
		"",
		"",
		"Undo an edit command",
	},
	{
		CVS_OP_UPDATE, "update",   {}, cvs_update,
		"",
		"",
		"Bring work tree in sync with repository",
	},
	{
		CVS_OP_VERSION, "version",  {}, cvs_version,
		"", "",
		"Show current CVS version(s)",
	},
	{
		-1, "watch",    {}, NULL,
		"",
		"",
		"Set watches",
	},
	{
		-1, "watchers", {}, NULL,
		"",
		"",
		"See who is watching a file",
	},
};

#define CVS_NBCMD  (sizeof(cvs_cdt)/sizeof(cvs_cdt[0]))



void             usage        (void);
void             sigchld_hdlr (int);
void             cvs_readrc   (void);
struct cvs_cmd*  cvs_findcmd  (const char *); 


/*
 * usage()
 *
 * Display usage information.
 */

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-lQqtvx] [-b bindir] [-d root] [-e editor] [-z level] "
	    "command [options] ...\n",
	    __progname);
}


int
main(int argc, char **argv)
{
	char *envstr, *ep;
	int ret;
	u_int i, readrc;
	struct cvs_cmd *cmdp;

	readrc = 1;

	if (cvs_log_init(LD_STD, 0) < 0)
		err(1, "failed to initialize logging");

	/* by default, be very verbose */
	(void)cvs_log_filter(LP_FILTER_UNSET, LP_INFO);

#ifdef DEBUG
	(void)cvs_log_filter(LP_FILTER_UNSET, LP_DEBUG);
#endif

	/* check environment so command-line options override it */
	if ((envstr = getenv("CVS_RSH")) != NULL)
		cvs_rsh = envstr;

	if (((envstr = getenv("CVSEDITOR")) != NULL) ||
	    ((envstr = getenv("VISUAL")) != NULL) ||
	    ((envstr = getenv("EDITOR")) != NULL))
		cvs_editor = envstr;

	while ((ret = getopt(argc, argv, "b:d:e:fHlnQqrtvz:")) != -1) {
		switch (ret) {
		case 'b':
			/*
			 * We do not care about the bin directory for RCS files
			 * as this program has no dependencies on RCS programs,
			 * so it is only here for backwards compatibility.
			 */
			cvs_log(LP_NOTICE, "the -b argument is obsolete");
			break;
		case 'd':
			cvs_rootstr = optarg;
			break;
		case 'e':
			cvs_editor = optarg;
			break;
		case 'f':
			readrc = 0;
			break;
		case 'l':
			cvs_nolog = 1;
			break;
		case 'n':
			break;
		case 'Q':
			verbosity = 0;
			break;
		case 'q':
			/* don't override -Q */
			if (verbosity > 1)
				verbosity = 1;
			break;
		case 'r':
			cvs_readonly = 1;
			break;
		case 't':
			cvs_trace = 1;
			break;
		case 'v':
			printf("%s\n", CVS_VERSION);
			exit(0);
			/* NOTREACHED */
			break;
		case 'x':
			/*
			 * Kerberos encryption support, kept for compatibility
			 */
			break;
		case 'z':
			cvs_compress = (int)strtol(optarg, &ep, 10); 
			if (*ep != '\0')
				errx(1, "error parsing compression level");
			if (cvs_compress < 0 || cvs_compress > 9)
				errx(1, "gzip compression level must be "
				    "between 0 and 9");
			break;
		default:
			usage();
			exit(EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	/* reset getopt() for use by commands */
	optind = 1;
	optreset = 1;

	if (argc == 0) {
		usage();
		exit(EX_USAGE);
	}

	/* setup signal handlers */
	signal(SIGPIPE, SIG_IGN);

	cvs_file_init();

	if (readrc)
		cvs_readrc();

	cvs_command = argv[0];
	ret = -1;

	cmdp = cvs_findcmd(cvs_command);
	if (cmdp == NULL) {
		fprintf(stderr, "Unknown command: `%s'\n\n", cvs_command);
		fprintf(stderr, "CVS commands are:\n");
		for (i = 0; i < CVS_NBCMD; i++)
			fprintf(stderr, "\t%-16s%s\n",
			    cvs_cdt[i].cmd_name, cvs_cdt[i].cmd_descr);
		exit(EX_USAGE);
	}

	if (cmdp->cmd_hdlr == NULL) {
		cvs_log(LP_ERR, "command `%s' not implemented", cvs_command);
		exit(1);
	}

	cvs_cmdop = cmdp->cmd_op;

	ret = (*cmdp->cmd_hdlr)(argc, argv);
	if (ret == EX_USAGE) {
		fprintf(stderr, "Usage: %s %s %s\n", __progname, cvs_command,
		    cmdp->cmd_synopsis);
	}

	if (cvs_files != NULL)
		cvs_file_free(cvs_files);

	return (ret);
}


/*
 * cvs_findcmd()
 *
 * Find the entry in the command dispatch table whose name or one of its
 * aliases matches <cmd>.
 * Returns a pointer to the command entry on success, NULL on failure.
 */

struct cvs_cmd*
cvs_findcmd(const char *cmd)
{
	u_int i, j;
	struct cvs_cmd *cmdp;

	cmdp = NULL;

	for (i = 0; (i < CVS_NBCMD) && (cmdp == NULL); i++) {
		if (strcmp(cmd, cvs_cdt[i].cmd_name) == 0)
			cmdp = &cvs_cdt[i];
		else {
			for (j = 0; j < CVS_CMD_MAXALIAS; j++) {
				if (strcmp(cmd, cvs_cdt[i].cmd_alias[j]) == 0) {
					cmdp = &cvs_cdt[i];
					break;
				}
			}
		}
	}

	return (cmdp);
}


/*
 * cvs_readrc()
 *
 * Read the CVS `.cvsrc' file in the user's home directory.  If the file
 * exists, it should contain a list of arguments that should always be given
 * implicitly to the specified commands.
 */

void
cvs_readrc(void)
{
	char rcpath[MAXPATHLEN], linebuf[128], *lp;
	struct cvs_cmd *cmdp;
	struct passwd *pw;
	FILE *fp;

	pw = getpwuid(getuid());
	if (pw == NULL) {
		cvs_log(LP_NOTICE, "failed to get user's password entry");
		return;
	}

	snprintf(rcpath, sizeof(rcpath), "%s/%s", pw->pw_dir, CVS_PATH_RC);

	fp = fopen(rcpath, "r");
	if (fp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s': %s", rcpath,
			    strerror(errno));
		return;
	}

	while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
		lp = strchr(linebuf, ' ');

		/* ignore lines with no arguments */
		if (lp == NULL)
			continue;

		*(lp++) = '\0';
		if (strcmp(linebuf, "cvs") == 0) {
			/* global options */
		}
		else {
			cmdp = cvs_findcmd(linebuf);
			if (cmdp == NULL) {
				cvs_log(LP_NOTICE,
				    "unknown command `%s' in cvsrc",
				    linebuf);
				continue;
			}
		}
	}
	if (ferror(fp)) {
		cvs_log(LP_NOTICE, "failed to read line from cvsrc");
	}

	(void)fclose(fp);
}

/*	$OpenBSD: cvs.c,v 1.23 2004/12/14 19:56:35 xsa Exp $	*/
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
int   cvs_readrc = 1;		/* read .cvsrc on startup */
int   cvs_trace = 0;
int   cvs_nolog = 0;
int   cvs_readonly = 0;
int   cvs_nocase = 0;   /* set to 1 to disable filename case sensitivity */

char *cvs_defargs;		/* default global arguments from .cvsrc */
char *cvs_command;		/* name of the command we are running */
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
	char   *cmd_defargs;
} cvs_cdt[] = {
	{
		CVS_OP_ADD, "add",      { "ad",  "new" }, cvs_add,
		"[-m msg] file ...",
		"",
		"Add a new file/directory to the repository",
		NULL,
	},
	{
		-1, "admin",    { "adm", "rcs" }, NULL,
		"",
		"",
		"Administration front end for rcs",
		NULL,
	},
	{
		CVS_OP_ANNOTATE, "annotate", { "ann"        }, cvs_annotate,
		"[-FflR] [-D date | -r rev] file ...",
		"",
		"Show last revision where each line was modified",
		NULL,
	},
	{
		CVS_OP_CHECKOUT, "checkout", { "co",  "get" }, cvs_checkout,
		"",
		"",
		"Checkout sources for editing",
		NULL,
	},
	{
		CVS_OP_COMMIT, "commit",   { "ci",  "com" }, cvs_commit,
		"[-flR] [-F logfile | -m msg] [-r rev] ...",
		"F:flm:Rr:",
		"Check files into the repository",
		NULL,
	},
	{
		CVS_OP_DIFF, "diff",     { "di",  "dif" }, cvs_diff,
		"[-cilu] [-D date] [-r rev] ...",
		"cD:ilur:",
		"Show differences between revisions",
		NULL,
	},
	{
		-1, "edit",     {              }, NULL,
		"",
		"",
		"Get ready to edit a watched file",
		NULL,
	},
	{
		-1, "editors",  {              }, NULL,
		"",
		"",
		"See who is editing a watched file",
		NULL,
	},
	{
		-1, "export",   { "ex",  "exp" }, NULL,
		"",
		"",
		"Export sources from CVS, similar to checkout",
		NULL,
	},
	{
		CVS_OP_HISTORY, "history",  { "hi",  "his" }, cvs_history,
		"",
		"",
		"Show repository access history",
		NULL,
	},
	{
		CVS_OP_IMPORT, "import",   { "im",  "imp" }, NULL,
		"[-d] [-b branch] [-I ign] [-k subst] [-m msg] "
		"repository vendor-tag release-tags ...",
		"b:dI:k:m:",
		"Import sources into CVS, using vendor branches",
		NULL,
	},
	{
		CVS_OP_INIT, "init",     {              }, cvs_init,
		"",
		"",
		"Create a CVS repository if it doesn't exist",
		NULL,
	},
#if defined(HAVE_KERBEROS)
	{
		"kserver",  {}, NULL
		"",
		"",
		"Start a Kerberos authentication CVS server",
		NULL,
	},
#endif
	{
		CVS_OP_LOG, "log",      { "lo"         }, cvs_getlog,
		"",
		"",
		"Print out history information for files",
		NULL,
	},
	{
		-1, "login",    {}, NULL,
		"",
		"",
		"Prompt for password for authenticating server",
		NULL,
	},
	{
		-1, "logout",   {}, NULL,
		"",
		"",
		"Removes entry in .cvspass for remote repository",
		NULL,
	},
	{
		-1, "rdiff",    {}, NULL,
		"",
		"",
		"Create 'patch' format diffs between releases",
		NULL,
	},
	{
		-1, "release",  {}, NULL,
		"",
		"",
		"Indicate that a Module is no longer in use",
		NULL,
	},
	{
		CVS_OP_REMOVE, "remove",   {}, NULL,
		"",
		"",
		"Remove an entry from the repository",
		NULL,
	},
	{
		-1, "rlog",     {}, NULL,
		"",
		"",
		"Print out history information for a module",
		NULL,
	},
	{
		-1, "rtag",     {}, NULL,
		"",
		"",
		"Add a symbolic tag to a module",
		NULL,
	},
	{
		CVS_OP_SERVER, "server",   {}, cvs_server,
		"",
		"",
		"Server mode",
		NULL,
	},
	{
		CVS_OP_STATUS, "status",   { "st", "stat" }, cvs_status,
		"",
		"",
		"Display status information on checked out files",
		NULL,
	},
	{
		CVS_OP_TAG, "tag",      { "ta", "freeze" }, cvs_tag,
		"",
		"",
		"Add a symbolic tag to checked out version of files",
		NULL,
	},
	{
		-1, "unedit",   {}, NULL,
		"",
		"",
		"Undo an edit command",
		NULL,
	},
	{
		CVS_OP_UPDATE, "update",   { "up", "upd" }, cvs_update,
		"",
		"",
		"Bring work tree in sync with repository",
		NULL,
	},
	{
		CVS_OP_VERSION, "version",  { "ve", "ver" }, cvs_version,
		"", "",
		"Show current CVS version(s)",
		NULL,
	},
	{
		-1, "watch",    {}, NULL,
		"",
		"",
		"Set watches",
		NULL,
	},
	{
		-1, "watchers", {}, NULL,
		"",
		"",
		"See who is watching a file",
		NULL,
	},
};

#define CVS_NBCMD  (sizeof(cvs_cdt)/sizeof(cvs_cdt[0]))



void             usage        (void);
void             sigchld_hdlr (int);
void             cvs_read_rcfile   (void);
struct cvs_cmd*  cvs_findcmd  (const char *); 
int              cvs_getopt   (int, char **); 


/*
 * usage()
 *
 * Display usage information.
 */
void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-flQqtv] [-d root] [-e editor] [-z level] command [...]\n", __progname);
}


int
main(int argc, char **argv)
{
	char *envstr, *cmd_argv[CVS_CMD_MAXARG], **targv;
	int i, ret, cmd_argc;
	struct cvs_cmd *cmdp;

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

	ret = cvs_getopt(argc, argv);

	argc -= ret;
	argv += ret;
	if (argc == 0) {
		usage();
		exit(EX_USAGE);
	}
	cvs_command = argv[0];

	if (cvs_readrc) {
		cvs_read_rcfile();

		if (cvs_defargs != NULL) {
			targv = cvs_makeargv(cvs_defargs, &i);
			if (targv == NULL) {
				cvs_log(LP_ERR,
				    "failed to load default arguments to %s",
				    __progname);
				exit(EX_OSERR);
			}

			cvs_getopt(i, targv);
			cvs_freeargv(targv, i);
			free(targv);
		}
	}

	/* setup signal handlers */
	signal(SIGPIPE, SIG_IGN);

	cvs_file_init();

	ret = -1;

	cmdp = cvs_findcmd(cvs_command);
	if (cmdp == NULL) {
		fprintf(stderr, "Unknown command: `%s'\n\n", cvs_command);
		fprintf(stderr, "CVS commands are:\n");
		for (i = 0; i < (int)CVS_NBCMD; i++)
			fprintf(stderr, "\t%-16s%s\n",
			    cvs_cdt[i].cmd_name, cvs_cdt[i].cmd_descr);
		exit(EX_USAGE);
	}

	if (cmdp->cmd_hdlr == NULL) {
		cvs_log(LP_ERR, "command `%s' not implemented", cvs_command);
		exit(1);
	}

	cvs_cmdop = cmdp->cmd_op;

	cmd_argc = 0;
	memset(cmd_argv, 0, sizeof(cmd_argv));

	cmd_argv[cmd_argc++] = argv[0];
	if (cmdp->cmd_defargs != NULL) {
		/* transform into a new argument vector */
		ret = cvs_getargv(cmdp->cmd_defargs, cmd_argv + 1,
		    CVS_CMD_MAXARG - 1);
		if (ret < 0) {
			cvs_log(LP_ERRNO, "failed to generate argument vector "
			    "from default arguments");
			exit(EX_DATAERR);
		}
		cmd_argc += ret;
	}
	for (ret = 1; ret < argc; ret++)
		cmd_argv[cmd_argc++] = argv[ret];

	ret = (*cmdp->cmd_hdlr)(cmd_argc, cmd_argv);
	if (ret == EX_USAGE) {
		fprintf(stderr, "Usage: %s %s %s\n", __progname, cvs_command,
		    cmdp->cmd_synopsis);
	}

	if (cvs_files != NULL)
		cvs_file_free(cvs_files);

	return (ret);
}


int
cvs_getopt(int argc, char **argv)
{
	int ret;
	char *ep;

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
			cvs_readrc = 0;
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

	ret = optind;
	optind = 1;
	optreset = 1;	/* for next call */

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
 * cvs_read_rcfile()
 *
 * Read the CVS `.cvsrc' file in the user's home directory.  If the file
 * exists, it should contain a list of arguments that should always be given
 * implicitly to the specified commands.
 */
void
cvs_read_rcfile(void)
{
	char rcpath[MAXPATHLEN], linebuf[128], *lp;
	int linenum = 0;
	size_t len;
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
		linenum++;
		if ((len = strlen(linebuf)) == 0)
			continue;
		if (linebuf[len - 1] != '\n') {
			cvs_log(LP_WARN, "line too long in `%s:%d'", rcpath,
				linenum);
			break;
		}
		linebuf[--len] = '\0';

		lp = strchr(linebuf, ' ');
		if (lp == NULL)
			continue;	/* ignore lines with no arguments */
		*lp = '\0';
		if (strcmp(linebuf, "cvs") == 0) {
			/*
			 * Global default options.  In the case of cvs only,
			 * we keep the 'cvs' string as first argument because
			 * getopt() does not like starting at index 0 for
			 * argument processing.
			 */
			*lp = ' ';
			cvs_defargs = strdup(linebuf);
			if (cvs_defargs == NULL)
				cvs_log(LP_ERRNO,
				    "failed to copy global arguments");
		} else {
			lp++;
			cmdp = cvs_findcmd(linebuf);
			if (cmdp == NULL) {
				cvs_log(LP_NOTICE,
				    "unknown command `%s' in `%s'",
				    linebuf, rcpath);
				continue;
			}

			cmdp->cmd_defargs = strdup(lp);
			if (cmdp->cmd_defargs == NULL)
				cvs_log(LP_ERRNO,
				    "failed to copy default arguments for %s",
				    cmdp->cmd_name);
		}
	}
	if (ferror(fp)) {
		cvs_log(LP_NOTICE, "failed to read line from `%s'", rcpath);
	}

	(void)fclose(fp);
}

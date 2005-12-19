/*	$OpenBSD: cvs.c,v 1.89 2005/12/19 17:43:01 xsa Exp $	*/
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
int   cvs_noexec = 0;	/* set to 1 to disable disk operations (-n option) */
int   cvs_error = -1;	/* set to the correct error code on failure */
char *cvs_defargs;		/* default global arguments from .cvsrc */
char *cvs_command;		/* name of the command we are running */
int   cvs_cmdop;
char *cvs_rootstr;
char *cvs_rsh = CVS_RSH_DEFAULT;
char *cvs_editor = CVS_EDITOR_DEFAULT;
char *cvs_homedir = NULL;
char *cvs_msg = NULL;
char *cvs_repo_base = NULL;
char *cvs_tmpdir = CVS_TMPDIR_DEFAULT;

/* hierarchy of all the files affected by the command */
CVSFILE *cvs_files;

static TAILQ_HEAD(, cvs_var) cvs_variables;


void		usage(void);
static void	cvs_read_rcfile(void);
int		cvs_getopt(int, char **);

/*
 * usage()
 *
 * Display usage information.
 */
void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-flnQqrtvw] [-d root] [-e editor] [-s var=val] "
	    "[-T tmpdir] [-z level] command [...]\n", __progname);
}


int
main(int argc, char **argv)
{
	char *envstr, *cmd_argv[CVS_CMD_MAXARG], **targv;
	int i, ret, cmd_argc;
	struct cvs_cmd *cmdp;
	struct passwd *pw;
	struct stat st;

	tzset();

	TAILQ_INIT(&cvs_variables);

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

	if ((envstr = getenv("CVSREAD")) != NULL)
		cvs_readonly = 1;

	if ((cvs_homedir = getenv("HOME")) == NULL) {
		if ((pw = getpwuid(getuid())) == NULL)
			fatal("getpwuid failed");
		cvs_homedir = pw->pw_dir;
	}

	if ((envstr = getenv("TMPDIR")) != NULL)
		cvs_tmpdir = envstr;

	ret = cvs_getopt(argc, argv);

	argc -= ret;
	argv += ret;
	if (argc == 0) {
		usage();
		exit(CVS_EX_USAGE);
	}

	cvs_command = argv[0];

	/*
	 * check the tmp dir, either specified through
	 * the environment variable TMPDIR, or via
	 * the global option -T <dir>
	 */
	if (stat(cvs_tmpdir, &st) == -1) {
		cvs_log(LP_ERR, "failed to stat `%s'", cvs_tmpdir);
		exit(CVS_EX_FILE);
	} else if (!S_ISDIR(st.st_mode)) {
		cvs_log(LP_ERR, "`%s' is not valid temporary directory",
		    cvs_tmpdir);
		exit(CVS_EX_FILE);
	}

	if (cvs_readrc == 1) {
		cvs_read_rcfile();

		if (cvs_defargs != NULL) {
			targv = cvs_makeargv(cvs_defargs, &i);
			if (targv == NULL) {
				cvs_log(LP_ERR,
				    "failed to load default arguments to %s",
				    __progname);
				exit(CVS_EX_DATA);
			}

			cvs_getopt(i, targv);
			cvs_freeargv(targv, i);
			xfree(targv);
		}
	}

	/* setup signal handlers */
	signal(SIGPIPE, SIG_IGN);

	if (cvs_file_init() < 0) {
		cvs_log(LP_ERR, "failed to initialize file support");
		exit(CVS_EX_FILE);
	}

	ret = -1;

	cmdp = cvs_findcmd(cvs_command);
	if (cmdp == NULL) {
		fprintf(stderr, "Unknown command: `%s'\n\n", cvs_command);
		fprintf(stderr, "CVS commands are:\n");
		for (i = 0; cvs_cdt[i] != NULL; i++)
			fprintf(stderr, "\t%-16s%s\n",
			    cvs_cdt[i]->cmd_name, cvs_cdt[i]->cmd_descr);
		exit(CVS_EX_USAGE);
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
			exit(CVS_EX_DATA);
		}
		cmd_argc += ret;
	}
	for (ret = 1; ret < argc; ret++)
		cmd_argv[cmd_argc++] = argv[ret];

	ret = cvs_startcmd(cmdp, cmd_argc, cmd_argv);
	switch (ret) {
	case CVS_EX_USAGE:
		fprintf(stderr, "Usage: %s %s %s\n", __progname, cvs_command,
		    cmdp->cmd_synopsis);
		break;
	case CVS_EX_DATA:
		cvs_log(LP_ABORT, "internal data error");
		break;
	case CVS_EX_PROTO:
		cvs_log(LP_ABORT, "protocol error");
		break;
	case CVS_EX_FILE:
		cvs_log(LP_ABORT, "an operation on a file or directory failed");
		break;
	case CVS_EX_BADROOT:
		/* match GNU CVS output, thus the LP_ERR and LP_ABORT codes. */
		cvs_log(LP_ERR,
		    "No CVSROOT specified! Please use the `-d' option");
		cvs_log(LP_ABORT,
		    "or set the CVSROOT enviroment variable.");
		break;
	case CVS_EX_ERR:
		cvs_log(LP_ABORT, "yeah, we failed, and we don't know why");
		break;
	default:
		break;
	}

	if (cvs_files != NULL)
		cvs_file_free(cvs_files);
	if (cvs_msg != NULL)
		xfree(cvs_msg);

	return (ret);
}


int
cvs_getopt(int argc, char **argv)
{
	int ret;
	char *ep;

	while ((ret = getopt(argc, argv, "b:d:e:fHlnQqrs:T:tvwz:")) != -1) {
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
			cvs_noexec = 1;
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
		case 's':
			ep = strchr(optarg, '=');
			if (ep == NULL) {
				cvs_log(LP_ERR, "no = in variable assignment");
				exit(CVS_EX_USAGE);
			}
			*(ep++) = '\0';
			if (cvs_var_set(optarg, ep) < 0)
				exit(CVS_EX_USAGE);
			break;
		case 'T':
			cvs_tmpdir = optarg;
			break;
		case 't':
			(void)cvs_log_filter(LP_FILTER_UNSET, LP_TRACE);
			cvs_trace = 1;
			break;
		case 'v':
			printf("%s\n", CVS_VERSION);
			exit(0);
			/* NOTREACHED */
			break;
		case 'w':
			cvs_readonly = 0;
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
			exit(CVS_EX_USAGE);
		}
	}

	ret = optind;
	optind = 1;
	optreset = 1;	/* for next call */

	return (ret);
}


/*
 * cvs_read_rcfile()
 *
 * Read the CVS `.cvsrc' file in the user's home directory.  If the file
 * exists, it should contain a list of arguments that should always be given
 * implicitly to the specified commands.
 */
static void
cvs_read_rcfile(void)
{
	char rcpath[MAXPATHLEN], linebuf[128], *lp, *p;
	int l, linenum = 0;
	size_t len;
	struct cvs_cmd *cmdp;
	FILE *fp;

	l = snprintf(rcpath, sizeof(rcpath), "%s/%s", cvs_homedir, CVS_PATH_RC);
	if (l == -1 || l >= (int)sizeof(rcpath)) {
		errno = ENAMETOOLONG;
		cvs_log(LP_ERRNO, "%s", rcpath);
		return;
	}

	fp = fopen(rcpath, "r");
	if (fp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s': %s", rcpath,
			    strerror(errno));
		return;
	}

	while (fgets(linebuf, (int)sizeof(linebuf), fp) != NULL) {
		linenum++;
		if ((len = strlen(linebuf)) == 0)
			continue;
		if (linebuf[len - 1] != '\n') {
			cvs_log(LP_WARN, "line too long in `%s:%d'", rcpath,
				linenum);
			break;
		}
		linebuf[--len] = '\0';

		/* skip any whitespaces */
		p = linebuf;
		while (*p == ' ')
			*p++;

		/* allow comments */
		if (*p == '#')
			continue;

		lp = strchr(p, ' ');
		if (lp == NULL)
			continue;	/* ignore lines with no arguments */
		*lp = '\0';
		if (strcmp(p, "cvs") == 0) {
			/*
			 * Global default options.  In the case of cvs only,
			 * we keep the 'cvs' string as first argument because
			 * getopt() does not like starting at index 0 for
			 * argument processing.
			 */
			*lp = ' ';
			cvs_defargs = xstrdup(p);
		} else {
			lp++;
			cmdp = cvs_findcmd(p);
			if (cmdp == NULL) {
				cvs_log(LP_NOTICE,
				    "unknown command `%s' in `%s:%d'",
				    p, rcpath, linenum);
				continue;
			}

			cmdp->cmd_defargs = xstrdup(lp);
		}
	}
	if (ferror(fp)) {
		cvs_log(LP_NOTICE, "failed to read line from `%s'", rcpath);
	}

	(void)fclose(fp);
}


/*
 * cvs_var_set()
 *
 * Set the value of the variable <var> to <val>.  If there is no such variable,
 * a new entry is created, otherwise the old value is overwritten.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_var_set(const char *var, const char *val)
{
	char *valcp;
	const char *cp;
	struct cvs_var *vp;

	if ((var == NULL) || (*var == '\0')) {
		cvs_log(LP_ERR, "no variable name");
		return (-1);
	}

	/* sanity check on the name */
	for (cp = var; *cp != '\0'; cp++)
		if (!isalnum(*cp) && (*cp != '_')) {
			cvs_log(LP_ERR,
			    "variable name `%s' contains invalid characters",
			    var);
			return (-1);
		}

	TAILQ_FOREACH(vp, &cvs_variables, cv_link)
		if (strcmp(vp->cv_name, var) == 0)
			break;

	valcp = xstrdup(val);
	if (vp == NULL) {
		vp = (struct cvs_var *)xmalloc(sizeof(*vp));
		memset(vp, 0, sizeof(*vp));

		vp->cv_name = xstrdup(var);
		TAILQ_INSERT_TAIL(&cvs_variables, vp, cv_link);

	} else	/* free the previous value */
		xfree(vp->cv_val);

	vp->cv_val = valcp;

	return (0);
}


/*
 * cvs_var_set()
 *
 * Remove any entry for the variable <var>.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_var_unset(const char *var)
{
	struct cvs_var *vp;

	TAILQ_FOREACH(vp, &cvs_variables, cv_link)
		if (strcmp(vp->cv_name, var) == 0) {
			TAILQ_REMOVE(&cvs_variables, vp, cv_link);
			xfree(vp->cv_name);
			xfree(vp->cv_val);
			xfree(vp);
			return (0);
		}

	return (-1);

}


/*
 * cvs_var_get()
 *
 * Get the value associated with the variable <var>.  Returns a pointer to the
 * value string on success, or NULL if the variable does not exist.
 */

const char *
cvs_var_get(const char *var)
{
	struct cvs_var *vp;

	TAILQ_FOREACH(vp, &cvs_variables, cv_link)
		if (strcmp(vp->cv_name, var) == 0)
			return (vp->cv_val);

	return (NULL);
}

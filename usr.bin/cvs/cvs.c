/*	$OpenBSD: cvs.c,v 1.151 2010/07/23 08:31:19 ray Exp $	*/
/*
 * Copyright (c) 2006, 2007 Joris Vink <joris@openbsd.org>
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

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"
#include "hash.h"

extern char *__progname;

/* verbosity level: 0 = really quiet, 1 = quiet, 2 = verbose */
int verbosity = 2;

/* compression level used with zlib, 0 meaning no compression taking place */
int	cvs_compress = 0;
int	cvs_readrc = 1;		/* read .cvsrc on startup */
int	cvs_trace = 0;
int	cvs_nolog = 0;
int	cvs_readonly = 0;
int	cvs_readonlyfs = 0;
int	cvs_nocase = 0;	/* set to 1 to disable filename case sensitivity */
int	cvs_noexec = 0;	/* set to 1 to disable disk operations (-n option) */
int	cvs_cmdop;
int	cvs_umask = CVS_UMASK_DEFAULT;
int	cvs_server_active = 0;

char	*cvs_tagname = NULL;
char	*cvs_defargs;		/* default global arguments from .cvsrc */
char	*cvs_rootstr;
char	*cvs_rsh = CVS_RSH_DEFAULT;
char	*cvs_editor = CVS_EDITOR_DEFAULT;
char	*cvs_homedir = NULL;
char	*cvs_msg = NULL;
char	*cvs_tmpdir = CVS_TMPDIR_DEFAULT;

struct cvsroot *current_cvsroot = NULL;
struct cvs_cmd *cmdp;			/* struct of command we are running */

int		cvs_getopt(int, char **);
__dead void	usage(void);
static void	cvs_read_rcfile(void);

struct wklhead temp_files;

void sighandler(int);
volatile sig_atomic_t cvs_quit = 0;
volatile sig_atomic_t sig_received = 0;

extern CVSENTRIES *current_list;

struct hash_table created_directories;
struct hash_table created_cvs_directories;

void
sighandler(int sig)
{
	sig_received = sig;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
	case SIGPIPE:
		cvs_quit = 1;
		break;
	default:
		break;
	}
}

void
cvs_cleanup(void)
{
	cvs_log(LP_TRACE, "cvs_cleanup: removing locks");
	worklist_run(&repo_locks, worklist_unlink);

	cvs_log(LP_TRACE, "cvs_cleanup: removing temp files");
	worklist_run(&temp_files, worklist_unlink);

	if (cvs_server_path != NULL) {
		if (cvs_rmdir(cvs_server_path) == -1)
			cvs_log(LP_ERR,
			    "warning: failed to remove server directory: %s",
			    cvs_server_path);
		xfree(cvs_server_path);
		cvs_server_path = NULL;
	}

	if (current_list != NULL)
		cvs_ent_close(current_list, ENT_SYNC);
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: %s [-flnQqRrtvw] [-d root] [-e editor] [-s var=val]\n"
	    "           [-T tmpdir] [-z level] command ...\n", __progname);
	exit(1);
}

int
cvs_build_cmd(char ***cmd_argv, char **argv, int argc)
{
	int cmd_argc, i, cur;
	char *cp, *linebuf, *lp;

	if (cmdp->cmd_defargs == NULL) {
		*cmd_argv = argv;
		return argc;
	}

	cur = argc + 2;
	cmd_argc = 0;
	*cmd_argv = xcalloc(cur, sizeof(char *));
	(*cmd_argv)[cmd_argc++] = argv[0];

	linebuf = xstrdup(cmdp->cmd_defargs);
	for (lp = linebuf; lp != NULL;) {
		cp = strsep(&lp, " \t\b\f\n\r\t\v");
		if (cp == NULL)
			break;
		if (*cp == '\0')
			continue;

		if (cmd_argc == cur) {
			cur += 8;
			*cmd_argv = xrealloc(*cmd_argv, cur,
			    sizeof(char *));
		}

		(*cmd_argv)[cmd_argc++] = cp;
	}

	if (cmd_argc + argc > cur) {
		cur = cmd_argc + argc + 1;
		*cmd_argv = xrealloc(*cmd_argv, cur,
		    sizeof(char *));
        }

	for (i = 1; i < argc; i++)
		(*cmd_argv)[cmd_argc++] = argv[i];

	(*cmd_argv)[cmd_argc] = NULL;

	return cmd_argc;
}

int
main(int argc, char **argv)
{
	char *envstr, **cmd_argv, **targv;
	int i, ret, cmd_argc;
	struct passwd *pw;
	struct stat st;
	char fpath[MAXPATHLEN];

	tzset();

	TAILQ_INIT(&cvs_variables);
	SLIST_INIT(&repo_locks);
	SLIST_INIT(&temp_files);

	hash_table_init(&created_directories, 100);
	hash_table_init(&created_cvs_directories, 100);

	/* check environment so command-line options override it */
	if ((envstr = getenv("CVS_RSH")) != NULL)
		cvs_rsh = envstr;

	if (((envstr = getenv("CVSEDITOR")) != NULL) ||
	    ((envstr = getenv("VISUAL")) != NULL) ||
	    ((envstr = getenv("EDITOR")) != NULL))
		cvs_editor = envstr;

	if ((envstr = getenv("CVSREAD")) != NULL)
		cvs_readonly = 1;

	if ((envstr = getenv("CVSREADONLYFS")) != NULL) {
		cvs_readonlyfs = 1;
		cvs_nolog = 1;
	}

	if ((cvs_homedir = getenv("HOME")) == NULL) {
		if ((pw = getpwuid(getuid())) != NULL)
			cvs_homedir = pw->pw_dir;
	}

	if ((envstr = getenv("TMPDIR")) != NULL)
		cvs_tmpdir = envstr;

	ret = cvs_getopt(argc, argv);

	argc -= ret;
	argv += ret;
	if (argc == 0)
		usage();

	cmdp = cvs_findcmd(argv[0]);
	if (cmdp == NULL) {
		fprintf(stderr, "Unknown command: `%s'\n\n", argv[0]);
		fprintf(stderr, "CVS commands are:\n");
		for (i = 0; cvs_cdt[i] != NULL; i++)
			fprintf(stderr, "\t%-16s%s\n",
			    cvs_cdt[i]->cmd_name, cvs_cdt[i]->cmd_descr);
		exit(1);
	}

	/*
	 * check the tmp dir, either specified through
	 * the environment variable TMPDIR, or via
	 * the global option -T <dir>
	 */
	if (stat(cvs_tmpdir, &st) == -1)
		fatal("stat failed on `%s': %s", cvs_tmpdir, strerror(errno));
	else if (!S_ISDIR(st.st_mode))
		fatal("`%s' is not valid temporary directory", cvs_tmpdir);

	if (cvs_readrc == 1 && cvs_homedir != NULL) {
		cvs_read_rcfile();

		if (cvs_defargs != NULL) {
			if ((targv = cvs_makeargv(cvs_defargs, &i)) == NULL)
				fatal("failed to load default arguments to %s",
				    __progname);

			cvs_getopt(i, targv);
			cvs_freeargv(targv, i);
			xfree(targv);
		}
	}

	/* setup signal handlers */
	signal(SIGTERM, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGHUP, sighandler);
	signal(SIGABRT, sighandler);
	signal(SIGALRM, sighandler);
	signal(SIGPIPE, sighandler);

	cvs_cmdop = cmdp->cmd_op;

	cmd_argc = cvs_build_cmd(&cmd_argv, argv, argc);

	cvs_file_init();

	if (cvs_cmdop == CVS_OP_SERVER) {
		cmdp->cmd(cmd_argc, cmd_argv);
		cvs_cleanup();
		return (0);
	}

	cvs_umask = umask(0);
	umask(cvs_umask);

	if ((current_cvsroot = cvsroot_get(".")) == NULL) {
		cvs_log(LP_ERR,
		    "No CVSROOT specified! Please use the '-d' option");
		fatal("or set the CVSROOT environment variable.");
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cmdp->cmd(cmd_argc, cmd_argv);
		cvs_cleanup();
		return (0);
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s",
	    current_cvsroot->cr_dir, CVS_PATH_ROOT);

	if (stat(fpath, &st) == -1 && cvs_cmdop != CVS_OP_INIT) {
		if (errno == ENOENT)
			fatal("repository '%s' does not exist",
			    current_cvsroot->cr_dir);
		else
			fatal("%s: %s", current_cvsroot->cr_dir,
			    strerror(errno));
	} else {
		if (!S_ISDIR(st.st_mode))
			fatal("'%s' is not a directory",
			    current_cvsroot->cr_dir);
	}

	if (cvs_cmdop != CVS_OP_INIT) {
		cvs_parse_configfile();
		cvs_parse_modules();
	}

	cmdp->cmd(cmd_argc, cmd_argv);
	cvs_cleanup();

	return (0);
}

int
cvs_getopt(int argc, char **argv)
{
	int ret;
	char *ep;
	const char *errstr;

	while ((ret = getopt(argc, argv, "b:d:e:flnQqRrs:T:tvwxz:")) != -1) {
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
			cvs_nolog = 1;
			break;
		case 'Q':
			verbosity = 0;
			break;
		case 'q':
			if (verbosity > 1)
				verbosity = 1;
			break;
		case 'R':
			cvs_readonlyfs = 1;
			cvs_nolog = 1;
			break;
		case 'r':
			cvs_readonly = 1;
			break;
		case 's':
			ep = strchr(optarg, '=');
			if (ep == NULL) {
				cvs_log(LP_ERR, "no = in variable assignment");
				exit(1);
			}
			*(ep++) = '\0';
			if (cvs_var_set(optarg, ep) < 0)
				exit(1);
			break;
		case 'T':
			cvs_tmpdir = optarg;
			break;
		case 't':
			cvs_trace = 1;
			break;
		case 'v':
			printf("%s\n", CVS_VERSION);
			exit(0);
			/* NOTREACHED */
		case 'w':
			cvs_readonly = 0;
			break;
		case 'x':
			/*
			 * Kerberos encryption support, kept for compatibility
			 */
			break;
		case 'z':
			cvs_compress = strtonum(optarg, 0, 9, &errstr);
			if (errstr != NULL)
				fatal("cvs_compress: %s", errstr);
			break;
		default:
			usage();
			/* NOTREACHED */
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
	char rcpath[MAXPATHLEN], *buf, *lbuf, *lp, *p;
	int cmd_parsed, cvs_parsed, i, linenum;
	size_t len, pos;
	struct cvs_cmd *tcmdp;
	FILE *fp;

	linenum = 0;

	i = snprintf(rcpath, MAXPATHLEN, "%s/%s", cvs_homedir, CVS_PATH_RC);
	if (i < 0 || i >= MAXPATHLEN) {
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

	cmd_parsed = cvs_parsed = 0;
	lbuf = NULL;
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		} else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		linenum++;

		/* skip any whitespaces */
		p = buf;
		while (*p == ' ')
			p++;

		/*
		 * Allow comments.
		 * GNU cvs stops parsing a line if it encounters a \t
		 * in front of a command, stick at this behaviour for
		 * compatibility.
		 */
		if (*p == '#' || *p == '\t')
			continue;

		pos = strcspn(p, " \t");
		if (pos == strlen(p)) {
			lp = NULL;
		} else {
			lp = p + pos;
			*lp = '\0';
		}

		if (strcmp(p, "cvs") == 0 && !cvs_parsed) {
			/*
			 * Global default options.  In the case of cvs only,
			 * we keep the 'cvs' string as first argument because
			 * getopt() does not like starting at index 0 for
			 * argument processing.
			 */
			if (lp != NULL) {
				*lp = ' ';
				cvs_defargs = xstrdup(p);
			}
			cvs_parsed = 1;
		} else {
			tcmdp = cvs_findcmd(p);
			if (tcmdp == NULL && verbosity == 2)
				cvs_log(LP_NOTICE,
				    "unknown command `%s' in `%s:%d'",
				    p, rcpath, linenum);

			if (tcmdp != cmdp || cmd_parsed)
				continue;

			if (lp != NULL) {
				lp++;
				cmdp->cmd_defargs = xstrdup(lp);
			}
			cmd_parsed = 1;
		}
	}
	if (lbuf != NULL)
		xfree(lbuf);

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
	const char *cp;
	struct cvs_var *vp;

	if (var == NULL || *var == '\0') {
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

	if (vp == NULL) {
		vp = xcalloc(1, sizeof(*vp));

		vp->cv_name = xstrdup(var);
		TAILQ_INSERT_TAIL(&cvs_variables, vp, cv_link);

	} else	/* free the previous value */
		xfree(vp->cv_val);

	vp->cv_val = xstrdup(val);

	return (0);
}

/*
 * cvs_var_unset()
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

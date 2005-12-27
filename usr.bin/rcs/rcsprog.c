/*	$OpenBSD: rcsprog.c,v 1.58 2005/12/27 16:05:21 niallo Exp $	*/
/*
 * Copyright (c) 2005 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

#define RCS_CMD_MAXARG	128
#define RCS_DEFAULT_SUFFIX	",v/"
#define RCSPROG_OPTSTRING	"A:a:b::c:e::hik:Lm:Mn:N:qt::TUVx:z:"

#define DESC_PROMPT	"enter description, terminated with single '.' "      \
			"or end of file:\nNOTE: This is NOT the log message!" \
			"\n>> "

const char rcs_version[] = "OpenCVS RCS version 3.6";
int verbose = 1;
int pipeout = 0;

#define RCS_NFLAG	1
#define RCS_TFLAG	2
static int rcsflags = 0;

int	 rcs_optind;
char	*rcs_optarg;
char	*rcs_suffixes;
char	*rcs_tmpdir = RCS_TMPDIR_DEFAULT;

struct rcs_prog {
	char	*prog_name;
	int	(*prog_hdlr)(int, char **);
	void	(*prog_usage)(void);
} programs[] = {
	{ "rcs",	rcs_main,	rcs_usage	},
	{ "ci",		checkin_main,	checkin_usage   },
	{ "co",		checkout_main,	checkout_usage  },
	{ "rcsclean",	rcsclean_main,	rcsclean_usage	},
	{ "rcsdiff",	rcsdiff_main,	rcsdiff_usage	},
	{ "rcsmerge",	rcsmerge_main,	rcsmerge_usage	},
	{ "rlog",	rlog_main,	rlog_usage	},
	{ "ident",	ident_main,	ident_usage	},
};

static void rcs_set_description(RCSFILE *, const char *);
static void rcs_attach_symbol(RCSFILE *, const char *);

void
rcs_set_rev(const char *str, RCSNUM **rev)
{
	if (str == NULL)
		return;

	if ((*rev != NULL) && (*rev != RCS_HEAD_REV))
		cvs_log(LP_WARN, "redefinition of revision number");

	if ((*rev = rcsnum_parse(str)) == NULL)
		fatal("bad revision number '%s'", str);
}

/*
 * rcs_get_mtime()
 *
 * Get <filename> last modified time.
 * Returns last modified time on success, or -1 on failure.
 */
time_t
rcs_get_mtime(const char *filename)
{
	struct stat st;
	time_t mtime;

	if (stat(filename, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", filename);
		return (-1);
	}
	mtime = (time_t)st.st_mtimespec.tv_sec;

	return (mtime);
}

/*
 * rcs_set_mtime()
 *
 * Set <filename> last modified time to <mtime> if it's not set to -1.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_set_mtime(const char *filename, time_t mtime)
{
	static struct timeval tv[2];

	if (mtime == -1)
		return (0);

	tv[0].tv_sec = mtime;
	tv[1].tv_sec = tv[0].tv_sec;

	if (utimes(filename, tv) == -1) {
		cvs_log(LP_ERRNO, "error setting utimes");
		return (-1);
	}

	return (0);
}

int
rcs_init(char *envstr, char **argv, int argvlen)
{
	u_int i;
	int argc, error;
	char linebuf[256],  *lp, *cp;

	strlcpy(linebuf, envstr, sizeof(linebuf));
	memset(argv, 0, argvlen * sizeof(char *));

	error = argc = 0;
	for (lp = linebuf; lp != NULL;) {
		cp = strsep(&lp, " \t\b\f\n\r\t\v");;
		if (cp == NULL)
			break;
		else if (*cp == '\0')
			continue;

		if (argc == argvlen) {
			error++;
			break;
		}

		argv[argc] = xstrdup(cp);
		argc++;
	}

	if (error != 0) {
		for (i = 0; i < (u_int)argc; i++)
			xfree(argv[i]);
		argc = -1;
	}

	return (argc);
}

int
rcs_getopt(int argc, char **argv, const char *optstr)
{
	char *a;
	const char *c;
	static int i = 1;
	int opt, hasargument, ret;

	hasargument = 0;
	rcs_optarg = NULL;

	if (i >= argc)
		return (-1);

	a = argv[i++];
	if (*a++ != '-')
		return (-1);

	ret = 0;
	opt = *a;
	for (c = optstr; *c != '\0'; c++) {
		if (*c == opt) {
			a++;
			ret = opt;

			if (*(c + 1) == ':') {
				if (*(c + 2) == ':') {
					if (*a != '\0')
						hasargument = 1;
				} else {
					if (*a != '\0') {
						hasargument = 1;
					} else {
						ret = 1;
						break;
					}
				}
			}

			if (hasargument == 1)
				rcs_optarg = a;

			if (ret == opt)
				rcs_optind++;
			break;
		}
	}

	if (ret == 0)
		cvs_log(LP_ERR, "unknown option -%c", opt);
	else if (ret == 1)
		cvs_log(LP_ERR, "missing argument for option -%c", opt);

	return (ret);
}

int
rcs_statfile(char *fname, char *out, size_t len)
{
	size_t len1;
	int l, found, strdir;
	char defaultsuffix[] = RCS_DEFAULT_SUFFIX;
	char filev[MAXPATHLEN], fpath[MAXPATHLEN];
	char *ext, *slash;
	struct stat st;

	strdir = found = 0;

	/* we might have gotten a RCS file as argument */
	if ((ext = strchr(fname, ',')) != NULL)
		*ext = '\0';

	/* we might have gotten the RCS/ dir in the argument string */
	if (strstr(fname, RCSDIR) != NULL)
		strdir = 1;

	if (rcs_suffixes != NULL)
		ext = rcs_suffixes;
	else
		ext = defaultsuffix;

	for (;;) {
		/*
		 * GNU documentation says -x,v/ specifies two suffixes,
		 * namely the ,v one and an empty one (which matches
		 * everything).
		 * The problem is that they don't follow this rule at
		 * all, and their documentation seems flawed.
		 * We try to be compatible, so let's do so.
		 */
		if (*ext == '\0')
			break;

		if ((slash = strchr(ext, '/')) != NULL)
			*slash = '\0';

		l = snprintf(filev, sizeof(filev), "%s%s", fname, ext);
		if (l == -1 || l >= (int)sizeof(filev))
			fatal("rcs_statfile: path truncation");

		if ((strdir == 0) &&
		    (stat(RCSDIR, &st) != -1) && (st.st_mode & S_IFDIR)) {
			l = snprintf(fpath, sizeof(fpath), "%s/%s",
			    RCSDIR, filev);
			if (l == -1 || l >= (int)sizeof(fpath))
				fatal("rcs_statfile: path truncation");
		} else {
			len1 = strlcpy(fpath, filev, sizeof(fpath));
			if (len1 >= sizeof(fpath))
				fatal("rcs_statfile: path truncation");
		}

		if ((stat(fpath, &st) != -1) || (rcsflags & RCS_CREATE)) {
			found++;
			break;
		}

		if (slash == NULL)
			break;

		*slash++ = '/';
		ext = slash;
	}

	if (found != 1) {
		if ((strcmp(__progname, "rcsclean") != 0)
		    && (strcmp(__progname, "ci") != 0))
			cvs_log(LP_ERRNO, "%s", fpath);
		return (-1);
	}

	len1 = strlcpy(out, fpath, len);
	if (len1 >= len)
		fatal("rcs_statfile: path truncation");

	return (0);
}

int
main(int argc, char **argv)
{
	u_int i;
	char *rcsinit, *cmd_argv[RCS_CMD_MAXARG];
	int ret, cmd_argc;

	ret = -1;
	rcs_optind = 1;
	cvs_log_init(LD_STD, 0);

	cmd_argc = 0;
	cmd_argv[cmd_argc++] = argv[0];
	if ((rcsinit = getenv("RCSINIT")) != NULL) {
		ret = rcs_init(rcsinit, cmd_argv + 1,
		    RCS_CMD_MAXARG - 1);
		if (ret < 0) {
			cvs_log(LP_ERRNO, "failed to prepend RCSINIT options");
			exit (1);
		}

		cmd_argc += ret;
	}

	if ((rcs_tmpdir = getenv("TMPDIR")) == NULL)
		rcs_tmpdir = RCS_TMPDIR_DEFAULT;

	for (ret = 1; ret < argc; ret++)
		cmd_argv[cmd_argc++] = argv[ret];

	for (i = 0; i < (sizeof(programs)/sizeof(programs[0])); i++)
		if (strcmp(__progname, programs[i].prog_name) == 0) {
			usage = programs[i].prog_usage;
			ret = programs[i].prog_hdlr(cmd_argc, cmd_argv);
			break;
		}

	exit(ret);
}


void
rcs_usage(void)
{
	fprintf(stderr,
	    "usage: rcs [-hiLMTUV] [-Aoldfile] [-ausers] [-b[rev]] [-cstring]\n"
	    "           [-eusers] [-kmode] [-mrev:msg] [-xsuffixes] [-ztz] file ...\n");
}

/*
 * rcs_main()
 *
 * Handler for the `rcs' program.
 * Returns 0 on success, or >0 on error.
 */
int
rcs_main(int argc, char **argv)
{
	int i, ch, flags, kflag, lkmode;
	char fpath[MAXPATHLEN], ofpath[MAXPATHLEN];
	char *logstr, *logmsg, *nflag, *descfile;
	char *alist, *comment, *elist, *unp, *sp;
	mode_t fmode;
	RCSFILE *file, *oldfile;
	RCSNUM *logrev;
	struct rcs_access *acp;
	time_t rcs_mtime = -1;

	kflag = lkmode = -1;
	fmode = 0;
	flags = RCS_RDWR|RCS_PARSE_FULLY;
	descfile = nflag = NULL;
	logstr = alist = comment = elist = NULL;

	while ((ch = rcs_getopt(argc, argv, RCSPROG_OPTSTRING)) != -1) {
		switch (ch) {
		case 'A':
			if (rcs_statfile(rcs_optarg, ofpath, sizeof(ofpath)) < 0)
				exit(1);
			rcsflags |= CO_ACLAPPEND;
			break;
		case 'a':
			alist = rcs_optarg;
			break;
		case 'c':
			comment = rcs_optarg;
			break;
		case 'e':
			elist = rcs_optarg;
			break;
		case 'h':
			(usage)();
			exit(0);
		case 'i':
			flags |= RCS_CREATE;
			rcsflags |= RCS_CREATE;
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid keyword substitution mode `%s'",
				    rcs_optarg);
				exit(1);
			}
			break;
		case 'L':
			if (lkmode == RCS_LOCK_LOOSE)
				cvs_log(LP_WARN, "-U overriden by -L");
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'm':
			logstr = xstrdup(rcs_optarg);
			break;
		case 'M':
			/* ignore for the moment */
			break;
		case 'n':
			nflag = xstrdup(rcs_optarg);
			break;
		case 'N':
			nflag = xstrdup(rcs_optarg);
			rcsflags |= RCS_NFLAG;
			break;
		case 'q':
			verbose = 0;
			break;
		case 't':
			descfile = rcs_optarg;
			rcsflags |= RCS_TFLAG;
			break;
		case 'T':
			rcsflags |= PRESERVETIME;
			break;
		case 'U':
			if (lkmode == RCS_LOCK_STRICT)
				cvs_log(LP_WARN, "-L overriden by -U");
			lkmode = RCS_LOCK_LOOSE;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'x':
			rcs_suffixes = rcs_optarg;
			break;
		case 'z':
			/*
			 * kept for compatibility
			 */
			break;
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if (verbose == 1)
			printf("RCS file: %s\n", fpath);

		if ((file = rcs_open(fpath, flags, fmode)) == NULL)
			continue;

		if (rcsflags & RCS_CREATE)
			rcs_set_description(file, NULL);

		if (rcsflags & RCS_TFLAG)
			rcs_set_description(file, descfile);

		if (rcsflags & PRESERVETIME)
			rcs_mtime = rcs_get_mtime(file->rf_path);

		if (nflag != NULL)
			rcs_attach_symbol(file, nflag);

		if (logstr != NULL) {
			if ((logmsg = strchr(logstr, ':')) == NULL) {
				cvs_log(LP_ERR, "missing log message");
				rcs_close(file);
				continue;
			}

			*logmsg++ = '\0';
			if ((logrev = rcsnum_parse(logstr)) == NULL) {
				cvs_log(LP_ERR,
				    "'%s' bad revision number", logstr);
				rcs_close(file);
				continue;
			}

			if (rcs_rev_setlog(file, logrev, logmsg) < 0) {
				cvs_log(LP_ERR,
				    "failed to set logmsg for '%s' to '%s'",
				    logstr, logmsg);
				rcs_close(file);
				rcsnum_free(logrev);
				continue;
			}

			rcsnum_free(logrev);
		}

		/* entries to add from <oldfile> */
		if (rcsflags & CO_ACLAPPEND) {
			/* XXX */
			if ((oldfile = rcs_open(ofpath, RCS_READ)) == NULL)
				exit(1);

			TAILQ_FOREACH(acp, &(oldfile->rf_access), ra_list)
				rcs_access_add(file, acp->ra_name);

			rcs_close(oldfile);
		}

		/* entries to add to the access list */
		if (alist != NULL) {
			unp = alist;
			do {
				sp = strchr(unp, ',');
				if (sp != NULL)
					*(sp++) = '\0';

				rcs_access_add(file, unp);

				unp = sp;
			} while (sp != NULL);
		}

		if (comment != NULL)
			rcs_comment_set(file, comment);

		if (kflag != -1)
			rcs_kwexp_set(file, kflag);

		if (lkmode != -1)
			rcs_lock_setmode(file, lkmode);

		rcs_close(file);

		if (rcsflags & PRESERVETIME)
			rcs_set_mtime(fpath, rcs_mtime);

		if (verbose == 1)
			printf("done\n");
	}

	if (logstr != NULL)
		xfree(logstr);

	if (nflag != NULL)
		xfree(nflag);

	return (0);
}

static void
rcs_attach_symbol(RCSFILE *file, const char *symname)
{
	char *rnum;
	RCSNUM *rev;
	char rbuf[16];
	int rm;

	rm = 0;
	rev = NULL;
	if ((rnum = strrchr(symname, ':')) != NULL) {
		if (rnum[1] == '\0')
			rev = file->rf_head;
		*(rnum++) = '\0';
	} else {
		rm = 1;
	}

	if (rev == NULL && rm != 1) {
		if ((rev = rcsnum_parse(rnum)) == NULL)
			fatal("bad revision %s", rnum);
	}

	if (rcsflags & RCS_NFLAG)
		rm = 1;

	if (rm == 1) {
		if (rcs_sym_remove(file, symname) < 0) {
			if ((rcs_errno == RCS_ERR_NOENT) &&
			    !(rcsflags & RCS_NFLAG))
				cvs_log(LP_WARN,
				    "can't delete nonexisting symbol %s", symname);
		} else {
			if (rcsflags & RCS_NFLAG)
				rm = 0;
		}
	}

	if (rm == 0) {
		if ((rcs_sym_add(file, symname, rev) < 0) &&
		    (rcs_errno == RCS_ERR_DUPENT)) {
			rcsnum_tostr(rcs_sym_getrev(file, symname),
			    rbuf, sizeof(rbuf));
			fatal("symbolic name %s already bound to %s",
			    symname, rbuf);
		}
	}
}

static void
rcs_set_description(RCSFILE *file, const char *in)
{
	BUF *bp;
	char *content, buf[128];

	content = NULL;
	if (in != NULL) {
		bp = cvs_buf_load(in, BUF_AUTOEXT);
	} else {
		bp = cvs_buf_alloc(64, BUF_AUTOEXT);

		printf(DESC_PROMPT);
		for (;;) {
			fgets(buf, sizeof(buf), stdin);
			if (feof(stdin) || ferror(stdin) || buf[0] == '.')
				break;
			cvs_buf_append(bp, buf, strlen(buf));
			printf(">> ");
		}
	}

	cvs_buf_putc(bp, '\0');
	content = cvs_buf_release(bp);

	rcs_desc_set(file, content);
}

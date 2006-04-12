/*	$OpenBSD: rcsprog.c,v 1.98 2006/04/12 22:54:23 ray Exp $	*/
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

#include "includes.h"

#include "rcsprog.h"

#define RCS_CMD_MAXARG	128
#define RCSPROG_OPTSTRING	"A:a:b::c:e::hik:Ll::m:Mn:N:qt::TUu::Vx::z::"

#define DESC_PROMPT	"enter description, terminated with single '.' "      \
			"or end of file:\nNOTE: This is NOT the log message!" \
			"\n>> "

const char rcs_version[] = "OpenCVS RCS version 3.6";
int verbose = 1;
int pipeout = 0;

int	 flags;
int	 rcsflags;
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

struct cvs_wklhead rcs_temp_files;

void sighdlr(int);
static void rcs_set_description(RCSFILE *, const char *);
static void rcs_attach_symbol(RCSFILE *, const char *);

/* ARGSUSED */
void
sighdlr(int sig)
{
	cvs_worklist_clean(&rcs_temp_files, cvs_worklist_unlink);
	_exit(1);
}

/*
 * Allocate an RCSNUM and store in <rev>.
 */
void
rcs_set_rev(const char *str, RCSNUM **rev)
{
	if (str == NULL || (*rev = rcsnum_parse(str)) == NULL)
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
 */
void
rcs_set_mtime(const char *filename, time_t mtime)
{
	static struct timeval tv[2];

	if (mtime == -1)
		return;

	tv[0].tv_sec = mtime;
	tv[1].tv_sec = tv[0].tv_sec;

	if (utimes(filename, tv) == -1)
		fatal("error setting utimes: %s", strerror(errno));
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
		cp = strsep(&lp, " \t\b\f\n\r\t\v");
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

/*
 * rcs_choosefile()
 *
 * Given a relative filename, decide where the corresponding RCS file
 * should be.  Tries each extension until a file is found.  If no file
 * was found, returns a path with the first extension.
 *
 * Returns pointer to a char array on success, NULL on failure.
 */
char *
rcs_choosefile(const char *filename)
{
	struct stat sb;
	char *ext, name[MAXPATHLEN], *next, *ptr, rcsdir[MAXPATHLEN],
	    *ret, *suffixes, rcspath[MAXPATHLEN];

	/* If -x flag was not given, use default. */
	if (rcs_suffixes == NULL)
		rcs_suffixes = RCS_DEFAULT_SUFFIX;

	/*
	 * If `filename' contains a directory, `rcspath' contains that
	 * directory, including a trailing slash.  Otherwise `rcspath'
	 * contains an empty string.
	 */
	if (strlcpy(rcspath, filename, sizeof(rcspath)) >= sizeof(rcspath))
		return (NULL);
	/* If `/' is found, end string after `/'. */
	if ((ptr = strrchr(rcspath, '/')) != NULL)
		*(++ptr) = '\0';
	else
		rcspath[0] = '\0';

	/* Append RCS/ to `rcspath' if it exists. */
	if (strlcpy(rcsdir, rcspath, sizeof(rcsdir)) >= sizeof(rcsdir) ||
	    strlcat(rcsdir, RCSDIR, sizeof(rcsdir)) >= sizeof(rcsdir))
		return (NULL);
	if ((stat(rcsdir, &sb) == 0) && (sb.st_mode & S_IFDIR))
		if (strlcpy(rcspath, rcsdir, sizeof(rcspath)) >= sizeof(rcspath) ||
		    strlcat(rcspath, "/", sizeof(rcspath)) >= sizeof(rcspath))
			return (NULL);

	/* Name of file without path. */
	if ((ptr = strrchr(filename, '/')) == NULL) {
		if (strlcpy(name, filename, sizeof(name)) >= sizeof(name))
			return (NULL);
	} else {
		/* Skip `/'. */
		if (strlcpy(name, ptr + 1, sizeof(name)) >= sizeof(name))
			return (NULL);
	}

	/* Name of RCS file without an extension. */
	if (strlcat(rcspath, name, sizeof(rcspath)) >= sizeof(rcspath))
		return (NULL);

	/*
	 * If only the empty suffix was given, use existing rcspath.
	 * This ensures that there is at least one suffix for strsep().
	 */
	if (strcmp(rcs_suffixes, "") == 0) {
		ret = xstrdup(rcspath);
		return (ret);
	}

	/*
	 * Cycle through slash-separated `rcs_suffixes', appending each
	 * extension to `rcspath' and testing if the file exists.  If it
	 * does, return that string.  Otherwise return path with first
	 * extension.
	 */
	suffixes = xstrdup(rcs_suffixes);
	for (ret = NULL, next = suffixes; (ext = strsep(&next, "/")) != NULL;) {
		char fpath[MAXPATHLEN];

		/* Construct RCS file path. */
		if (strlcpy(fpath, rcspath, sizeof(fpath)) >= sizeof(fpath) ||
		    strlcat(fpath, ext, sizeof(fpath)) >= sizeof(fpath))
			goto out;

		/* Don't use `filename' as RCS file. */
		if (strcmp(fpath, filename) == 0)
			continue;

		if (stat(fpath, &sb) == 0) {
			ret = xstrdup(fpath);
			goto out;
		}
	}

	/*
	 * `ret' is still NULL.  No RCS file with any extension exists
	 * so we use the first extension.
	 *
	 * `suffixes' should now be NUL separated, so the first
	 * extension can be read just by reading `suffixes'.
	 */
	if (strlcat(rcspath, suffixes, sizeof(rcspath)) >=
	    sizeof(rcspath))
		goto out;
	ret = xstrdup(rcspath);

out:
	/* `ret' may be NULL, which indicates an error. */
	xfree(suffixes);
	return (ret);
}

/*
 * Find the name of an RCS file, given a file name `fname'.  If an RCS
 * file is found, the name is copied to the `len' sized buffer `out'.
 * Returns 0 if RCS file was found, -1 otherwise.
 */
int
rcs_statfile(char *fname, char *out, size_t len)
{
	struct stat st;
	char *rcspath;

	/* XXX - do this in rcs_choosefile? */
	if ((rcspath = rcs_choosefile(fname)) == NULL)
		fatal("rcs_statfile: path truncation");

	/* Error out if file not found and we are not creating one. */
	if (stat(rcspath, &st) == -1 && !(flags & RCS_CREATE)) {
		if ((strcmp(__progname, "rcsclean") != 0)
		    && (strcmp(__progname, "ci") != 0))
			cvs_log(LP_ERRNO, "%s", rcspath);
		xfree(rcspath);
		return (-1);
	}

	if (strlcpy(out, rcspath, len) >= len)
		fatal("rcs_statfile: path truncation");

	xfree(rcspath);

	return (0);
}

/*
 * Set <str> to <new_str>.  Print warning if <str> is redefined.
 */
void
rcs_setrevstr(char **str, char *new_str)
{
	if (new_str == NULL)
		return;
	if (*str != NULL)
		cvs_log(LP_WARN, "redefinition of revision number");
	*str = new_str;
}

/*
 * Set <str1> or <str2> to <new_str>, depending on which is not set.
 * If both are set, error out.
 */
void
rcs_setrevstr2(char **str1, char **str2, char *new_str)
{
	if (new_str == NULL)
		return;
	if (*str1 == NULL)
		*str1 = new_str;
	else if (*str2 == NULL)
		*str2 = new_str;
	else
		fatal("too many revision numbers");
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
	SLIST_INIT(&rcs_temp_files);

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

	signal(SIGHUP, sighdlr);
	signal(SIGINT, sighdlr);
	signal(SIGQUIT, sighdlr);
	signal(SIGABRT, sighdlr);
	signal(SIGALRM, sighdlr);
	signal(SIGTERM, sighdlr);

	for (i = 0; i < (sizeof(programs)/sizeof(programs[0])); i++)
		if (strcmp(__progname, programs[i].prog_name) == 0) {
			usage = programs[i].prog_usage;
			ret = programs[i].prog_hdlr(cmd_argc, cmd_argv);
			break;
		}

	exit(ret);
	/* NOTREACHED */
}


void
rcs_usage(void)
{
	fprintf(stderr,
	    "usage: rcs [-ehIiLMqTUV] [-Aoldfile] [-ausers] [-b[rev]]\n"
	    "           [-cstring] [-e[users]] [-kmode] [-l[rev]] [-mrev:msg]\n"
	    "           [-orev] [-sstate[:rev]] [-tfile|str] [-u[rev]]\n"
	    "           [-xsuffixes] file ...\n");
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
	int i, j, ch, kflag, lkmode;
	char fpath[MAXPATHLEN], ofpath[MAXPATHLEN];
	char *logstr, *logmsg, *nflag, *descfile;
	char *alist, *comment, *elist, *lrev, *urev;
	mode_t fmode;
	RCSFILE *file, *oldfile;
	RCSNUM *logrev;
	struct rcs_access *acp;
	time_t rcs_mtime = -1;

	kflag = RCS_KWEXP_ERR;
	lkmode = -1;
	fmode =  S_IRUSR|S_IRGRP|S_IROTH;
	flags = RCS_RDWR|RCS_PARSE_FULLY;
	lrev = urev = descfile = nflag = NULL;
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
			rcsflags |= RCSPROG_EFLAG;
			break;
		case 'h':
			(usage)();
			exit(0);
			/* NOTREACHED */
		case 'i':
			flags |= RCS_CREATE;
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
		case 'l':
			/* XXX - Check with -u flag. */
			lrev = rcs_optarg;
			rcsflags |= RCSPROG_LFLAG;
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
			rcsflags |= RCSPROG_NFLAG;
			break;
		case 'q':
			verbose = 0;
			break;
		case 't':
			descfile = rcs_optarg;
			rcsflags |= RCSPROG_TFLAG;
			break;
		case 'T':
			rcsflags |= PRESERVETIME;
			break;
		case 'U':
			if (lkmode == RCS_LOCK_STRICT)
				cvs_log(LP_WARN, "-L overriden by -U");
			lkmode = RCS_LOCK_LOOSE;
			break;
		case 'u':
			/* XXX - Check with -l flag. */
			urev = rcs_optarg;
			rcsflags |= RCSPROG_UFLAG;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
			/* NOTREACHED */
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
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

		if (rcsflags & RCSPROG_TFLAG)
			rcs_set_description(file, descfile);
		else if (flags & RCS_CREATE)
			rcs_set_description(file, NULL);

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
			struct cvs_argvector *aargv;

			aargv = cvs_strsplit(alist, ",");
			for (j = 0; aargv->argv[j] != NULL; j++)
				rcs_access_add(file, aargv->argv[j]);

			cvs_argv_destroy(aargv);
		}

		if (comment != NULL)
			rcs_comment_set(file, comment);

		if (elist != NULL) {
			struct cvs_argvector *eargv;

			eargv = cvs_strsplit(elist, ",");
			for (j = 0; eargv->argv[j] != NULL; j++)
				rcs_access_remove(file, eargv->argv[j]);

			cvs_argv_destroy(eargv);
		} else if (rcsflags & RCSPROG_EFLAG) {
			struct rcs_access *rap;

			/* XXX rcs_access_remove(file, NULL); ?? */
			while (!TAILQ_EMPTY(&(file->rf_access))) {
				rap = TAILQ_FIRST(&(file->rf_access));
				TAILQ_REMOVE(&(file->rf_access), rap, ra_list);
				xfree(rap->ra_name);
				xfree(rap);
			}
			/* not synced anymore */
			file->rf_flags &= ~RCS_SYNCED;
		}

		rcs_kwexp_set(file, kflag);

		if (lkmode != -1)
			(void)rcs_lock_setmode(file, lkmode);

		if (rcsflags & RCSPROG_LFLAG) {
			RCSNUM *rev;
			const char *username;
			char rev_str[16];

			if ((username = getlogin()) == NULL)
				fatal("could not get username");
			if (lrev == NULL) {
				rev = rcsnum_alloc();
				rcsnum_cpy(file->rf_head, rev, 0);
			} else if ((rev = rcsnum_parse(lrev)) == NULL) {
				cvs_log(LP_ERR, "unable to unlock file");
				rcs_close(file);
				continue;
			}
			rcsnum_tostr(rev, rev_str, sizeof(rev_str));
			/* Make sure revision exists. */
			if (rcs_findrev(file, rev) == NULL)
				fatal("%s: can't lock nonexisting revision %s",
				    fpath, rev_str);
			if (rcs_lock_add(file, username, rev) != -1 &&
			    verbose == 1)
				printf("%s locked\n", rev_str);
			rcsnum_free(rev);
		}

		if (rcsflags & RCSPROG_UFLAG) {
			RCSNUM *rev;
			const char *username;
			char rev_str[16];

			if ((username = getlogin()) == NULL)
				fatal("could not get username");
			if (urev == NULL) {
				rev = rcsnum_alloc();
				rcsnum_cpy(file->rf_head, rev, 0);
			} else if ((rev = rcsnum_parse(urev)) == NULL) {
				cvs_log(LP_ERR, "unable to unlock file");
				rcs_close(file);
				continue;
			}
			rcsnum_tostr(rev, rev_str, sizeof(rev_str));
			/* Make sure revision exists. */
			if (rcs_findrev(file, rev) == NULL)
				fatal("%s: can't unlock nonexisting revision %s",
				    fpath, rev_str);
			if (rcs_lock_remove(file, username, rev) == -1 &&
			    verbose == 1)
				cvs_log(LP_ERR,
				    "%s: warning: No locks are set.", fpath);
			rcsnum_free(rev);
		}

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

	if (rcsflags & RCSPROG_NFLAG)
		rm = 1;

	if (rm == 1) {
		if (rcs_sym_remove(file, symname) < 0) {
			if ((rcs_errno == RCS_ERR_NOENT) &&
			    !(rcsflags & RCSPROG_NFLAG))
				cvs_log(LP_WARN,
				    "can't delete nonexisting symbol %s", symname);
		} else {
			if (rcsflags & RCSPROG_NFLAG)
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

/*
 * Load description from <in> to <file>.
 * If <in> starts with a `-', <in> is taken as the description.
 * Otherwise <in> is the name of the file containing the description.
 * If <in> is NULL, the description is read from stdin.
 */
static void
rcs_set_description(RCSFILE *file, const char *in)
{
	BUF *bp;
	char *content, buf[128];

	content = NULL;
	/* Description is in file <in>. */
	if (in != NULL && *in != '-')
		bp = cvs_buf_load(in, BUF_AUTOEXT);
	/* Description is in <in>. */
	else if (in != NULL) {
		size_t len;
		const char *desc;

		/* Skip leading `-'. */
		desc = in + 1;
		len = strlen(desc);

		bp = cvs_buf_alloc(len + 1, BUF_AUTOEXT);
		cvs_buf_append(bp, desc, len);
	/* Get description from stdin. */
	} else {
		bp = cvs_buf_alloc(64, BUF_AUTOEXT);

		if (isatty(STDIN_FILENO))
			(void)fprintf(stderr, "%s", DESC_PROMPT);
		for (;;) {
			/* XXX - fgetln() may be more elegant. */
			fgets(buf, sizeof(buf), stdin);
			if (feof(stdin) || ferror(stdin) ||
			    strcmp(buf, ".\n") == 0 ||
			    strcmp(buf, ".") == 0)
				break;
			cvs_buf_append(bp, buf, strlen(buf));
			if (isatty(STDIN_FILENO))
				(void)fprintf(stderr, ">> ");
		}
	}

	cvs_buf_putc(bp, '\0');
	content = cvs_buf_release(bp);

	rcs_desc_set(file, content);
	xfree(content);
}

/*	$OpenBSD: checkout.c,v 1.167 2010/07/30 21:47:18 ray Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

static void checkout_check_repository(int, char **);
static int checkout_classify(const char *, const char *);
static void checkout_repository(const char *, const char *);

extern int print_stdout;
extern int prune_dirs;
extern int build_dirs;

static int flags = CR_REPO | CR_RECURSE_DIRS;
static int Aflag = 0;
static char *dflag = NULL;
static char *koptstr = NULL;
static char *dateflag = NULL;

static int nflag = 0;

static char lastwd[MAXPATHLEN];
char *checkout_target_dir = NULL;

time_t cvs_specified_date = -1;
time_t cvs_directory_date = -1;
int disable_fast_checkout = 0;

struct cvs_cmd cvs_cmd_checkout = {
	CVS_OP_CHECKOUT, CVS_USE_WDIR, "checkout",
	{ "co", "get" },
	"Checkout a working copy of a repository",
	"[-AcflNnPpRs] [-D date | -r tag] [-d dir] [-j rev] [-k mode] "
	"[-t id] module ...",
	"AcD:d:fj:k:lNnPpRr:st:",
	NULL,
	cvs_checkout
};

struct cvs_cmd cvs_cmd_export = {
	CVS_OP_EXPORT, CVS_USE_WDIR, "export",
	{ "exp", "ex" },
	"Export sources from CVS, similar to checkout",
	"[-flNnR] [-d dir] [-k mode] -D date | -r rev module ...",
	"D:d:k:flNnRr:",
	NULL,
	cvs_export
};

int
cvs_checkout(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, cvs_cmd_checkout.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			if (koptstr == NULL)
				reset_option = 1;
			if (cvs_specified_tag == NULL)
				reset_tag = 1;
			break;
		case 'c':
			cvs_modules_list();
			exit(0);
		case 'D':
			dateflag = optarg;
			if ((cvs_specified_date = date_parse(dateflag)) == -1)
				fatal("invalid date: %s", dateflag);
			reset_tag = 0;
			break;
		case 'd':
			if (dflag != NULL)
				fatal("-d specified two or more times");
			dflag = optarg;
			checkout_target_dir = dflag;

			if (cvs_server_active == 1)
				disable_fast_checkout = 1;
			break;
		case 'j':
			if (cvs_join_rev1 == NULL)
				cvs_join_rev1 = optarg;
			else if (cvs_join_rev2 == NULL)
				cvs_join_rev2 = optarg;
			else
				fatal("too many -j options");
			break;
		case 'k':
			reset_option = 0;
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				fatal("%s", cvs_cmd_checkout.cmd_synopsis);
			}
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'N':
			break;
		case 'n':
			nflag = 1;
			break;
		case 'P':
			prune_dirs = 1;
			break;
		case 'p':
			cmdp->cmd_flags &= ~CVS_USE_WDIR;
			print_stdout = 1;
			cvs_noexec = 1;
			nflag = 1;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			reset_tag = 0;
			cvs_specified_tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_checkout.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_checkout.cmd_synopsis);

	if (cvs_server_active == 1 && disable_fast_checkout != 1) {
		cmdp->cmd_flags &= ~CVS_USE_WDIR;
		cvs_noexec = 1;
	}

	checkout_check_repository(argc, argv);

	if (cvs_server_active == 1 && disable_fast_checkout != 1)
		cvs_noexec = 0;

	return (0);
}

int
cvs_export(int argc, char **argv)
{
	int ch;

	prune_dirs = 1;

	while ((ch = getopt(argc, argv, cvs_cmd_export.cmd_opts)) != -1) {
		switch (ch) {
		case 'd':
			if (dflag != NULL)
				fatal("-d specified two or more times");
			dflag = optarg;
			checkout_target_dir = dflag;

			if (cvs_server_active == 1)
				disable_fast_checkout = 1;
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				fatal("%s", cvs_cmd_export.cmd_synopsis);
			}
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'N':
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			cvs_specified_tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_export.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (cvs_specified_tag == NULL)
		fatal("must specify a tag or date");

	if (argc == 0)
		fatal("%s", cvs_cmd_export.cmd_synopsis);

	checkout_check_repository(argc, argv);

	return (0);
}

static void
checkout_check_repository(int argc, char **argv)
{
	int i;
	char *wdir, *d;
	struct cvs_recursion cr;
	struct module_checkout *mc;
	struct cvs_ignpat *ip;
	struct cvs_filelist *fl, *nxt;
	char repo[MAXPATHLEN], fpath[MAXPATHLEN], *f[1];

	build_dirs = print_stdout ? 0 : 1;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();

		if (cvs_specified_tag != NULL)
			cvs_client_send_request("Argument -r%s",
			    cvs_specified_tag);
		if (Aflag)
			cvs_client_send_request("Argument -A");

		if (dateflag != NULL)
			cvs_client_send_request("Argument -D%s", dateflag);

		if (kflag)
			cvs_client_send_request("Argument -k%s", koptstr);

		if (dflag != NULL)
			cvs_client_send_request("Argument -d%s", dflag);

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (cvs_cmdop == CVS_OP_CHECKOUT && prune_dirs == 1)
			cvs_client_send_request("Argument -P");

		if (print_stdout == 1)
			cvs_client_send_request("Argument -p");

		if (nflag == 1)
			cvs_client_send_request("Argument -n");

		cr.enterdir = NULL;
		cr.leavedir = NULL;
		if (print_stdout)
			cr.fileproc = NULL;
		else
			cr.fileproc = cvs_client_sendfile;

		flags &= ~CR_REPO;
		cr.flags = flags;

		if (cvs_cmdop != CVS_OP_EXPORT)
			cvs_file_run(argc, argv, &cr);

		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request("%s",
		    (cvs_cmdop == CVS_OP_CHECKOUT) ? "co" : "export");

		cvs_client_get_responses();

		return;
	}

	for (i = 0; i < argc; i++) {
		mc = cvs_module_lookup(argv[i]);
		current_module = mc;

		RB_FOREACH(fl, cvs_flisthead, &(mc->mc_ignores))
			cvs_file_ignore(fl->file_path, &checkout_ign_pats);

		RB_FOREACH(fl, cvs_flisthead, &(mc->mc_modules)) {
			module_repo_root = NULL;

			(void)xsnprintf(repo, sizeof(repo), "%s/%s",
			    current_cvsroot->cr_dir, fl->file_path);

			if (!(mc->mc_flags & MODULE_ALIAS) || dflag != NULL)
				module_repo_root = xstrdup(fl->file_path);

			if (mc->mc_flags & MODULE_NORECURSE)
				flags &= ~CR_RECURSE_DIRS;

			if (dflag != NULL)
				wdir = dflag;
			else if (mc->mc_flags & MODULE_ALIAS)
				wdir = fl->file_path;
			else
				wdir = mc->mc_name;

			switch (checkout_classify(repo, fl->file_path)) {
			case CVS_FILE:
				cr.fileproc = cvs_update_local;
				cr.flags = flags;

				if (!(mc->mc_flags & MODULE_ALIAS)) {
					module_repo_root =
					    xstrdup(dirname(fl->file_path));
					d = wdir;
					(void)xsnprintf(fpath, sizeof(fpath),
					    "%s/%s", d,
					    basename(fl->file_path));
				} else {
					d = dirname(wdir);
					strlcpy(fpath, fl->file_path,
					    sizeof(fpath));
				}

				if (build_dirs == 1)
					cvs_mkpath(d, cvs_specified_tag);

				f[0] = fpath;
				cvs_file_run(1, f, &cr);
				break;
			case CVS_DIR:
				if (build_dirs == 1)
					cvs_mkpath(wdir, cvs_specified_tag);
				checkout_repository(repo, wdir);
				break;
			default:
				break;
			}

			if (nflag != 1 && mc->mc_prog != NULL &&
			    mc->mc_flags & MODULE_RUN_ON_CHECKOUT)
				cvs_exec(mc->mc_prog, NULL, 0);

			if (module_repo_root != NULL)
				xfree(module_repo_root);
		}

		if (mc->mc_canfree == 1) {
			for (fl = RB_MIN(cvs_flisthead, &(mc->mc_modules));
			    fl != NULL; fl = nxt) {
				nxt = RB_NEXT(cvs_flisthead,
				    &(mc->mc_modules), fl);
				RB_REMOVE(cvs_flisthead,
				    &(mc->mc_modules), fl);
				xfree(fl->file_path);
				xfree(fl);
			}
		}

		while ((ip = TAILQ_FIRST(&checkout_ign_pats)) != NULL) {
			TAILQ_REMOVE(&checkout_ign_pats, ip, ip_list);
			xfree(ip);
		}

		xfree(mc);
	}
}

static int
checkout_classify(const char *repo, const char *arg)
{
	char *d, *f, fpath[MAXPATHLEN];
	struct stat sb;

	if (stat(repo, &sb) == 0) {
		if (S_ISDIR(sb.st_mode))
			return CVS_DIR;
	}

	d = dirname(repo);
	f = basename(repo);

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s%s", d, f, RCS_FILE_EXT);
	if (stat(fpath, &sb) == 0) {
		if (!S_ISREG(sb.st_mode)) {
			cvs_log(LP_ERR, "ignoring %s: not a regular file", arg);
			return 0;
		}
		return CVS_FILE;
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s/%s%s",
	    d, CVS_PATH_ATTIC, f, RCS_FILE_EXT);
	if (stat(fpath, &sb) == 0) {
		if (!S_ISREG(sb.st_mode)) {
			cvs_log(LP_ERR, "ignoring %s: not a regular file", arg);
			return 0;
		}
		return CVS_FILE;
	}

	cvs_log(LP_ERR, "cannot find module `%s' - ignored", arg);
	return 0;
}

static void
checkout_repository(const char *repobase, const char *wdbase)
{
	struct cvs_flisthead fl, dl;
	struct cvs_recursion cr;

	RB_INIT(&fl);
	RB_INIT(&dl);

	cvs_history_add((cvs_cmdop == CVS_OP_CHECKOUT) ?
	    CVS_HISTORY_CHECKOUT : CVS_HISTORY_EXPORT, NULL, wdbase);

	if (print_stdout) {
		cr.enterdir = NULL;
		cr.leavedir = NULL;
	} else {
		cr.enterdir = cvs_update_enterdir;
		if (cvs_server_active == 1) {
			if (disable_fast_checkout != 1)
				cr.leavedir = NULL;
			else
				cr.leavedir = cvs_update_leavedir;
		} else {
			cr.leavedir = prune_dirs ? cvs_update_leavedir : NULL;
		}
	}
	cr.fileproc = cvs_update_local;
	cr.flags = flags;

	cvs_repository_lock(repobase, 0);
	cvs_repository_getdir(repobase, wdbase, &fl, &dl,
	    flags & CR_RECURSE_DIRS ? REPOSITORY_DODIRS : 0);

	cvs_file_walklist(&fl, &cr);
	cvs_file_freelist(&fl);

	cvs_repository_unlock(repobase);

	cvs_file_walklist(&dl, &cr);
	cvs_file_freelist(&dl);
}

void
cvs_checkout_file(struct cvs_file *cf, RCSNUM *rnum, char *tag, int co_flags)
{
	BUF *bp;
	mode_t mode;
	int cf_kflag, exists;
	time_t rcstime;
	CVSENTRIES *ent;
	struct timeval tv[2];
	struct tm datetm;
	char *entry, *tosend;
	char kbuf[8], sticky[CVS_REV_BUFSZ], rev[CVS_REV_BUFSZ];
	char timebuf[CVS_TIME_BUFSZ], tbuf[CVS_TIME_BUFSZ];

	exists = 0;
	tosend = NULL;

	if (!(co_flags & CO_REMOVE))
		rcsnum_tostr(rnum, rev, sizeof(rev));
	else
		rev[0] = '\0';

	cvs_log(LP_TRACE, "cvs_checkout_file(%s, %s, %d) -> %s",
	    cf->file_path, rev, co_flags,
	    (cvs_server_active) ? "to client" : "to disk");

	if (co_flags & CO_DUMP) {
		rcs_rev_write_fd(cf->file_rcs, rnum, STDOUT_FILENO, 0);
		return;
	}

	if (cvs_server_active == 0) {
		(void)unlink(cf->file_path);

		if (!(co_flags & CO_MERGE)) {
			if (cf->file_flags & FILE_ON_DISK) {
				exists = 1;
				(void)close(cf->fd);
			}

			cf->fd = open(cf->file_path,
			    O_CREAT | O_RDWR | O_TRUNC);
			if (cf->fd == -1)
				fatal("cvs_checkout_file: open: %s",
				    strerror(errno));

			rcs_rev_write_fd(cf->file_rcs, rnum, cf->fd, 0);
			cf->file_flags |= FILE_ON_DISK;
		} else {
			cvs_merge_file(cf, (cvs_join_rev1 == NULL));
		}

		mode = cf->file_rcs->rf_mode;
		mode |= S_IWUSR;

		if (fchmod(cf->fd, mode) == -1)
			fatal("cvs_checkout_file: fchmod: %s", strerror(errno));

		if ((exists == 0) && (cf->file_ent == NULL) &&
		    !(co_flags & CO_MERGE))
			rcstime = rcs_rev_getdate(cf->file_rcs, rnum);
		else
			time(&rcstime);

		tv[0].tv_sec = rcstime;
		tv[0].tv_usec = 0;
		tv[1] = tv[0];
		if (futimes(cf->fd, tv) == -1)
			fatal("cvs_checkout_file: futimes: %s",
			    strerror(errno));
	} else {
		time(&rcstime);
	}

	gmtime_r(&rcstime, &datetm);
	asctime_r(&datetm, tbuf);
	tbuf[strcspn(tbuf, "\n")] = '\0';

	if (co_flags & CO_MERGE) {
		(void)xsnprintf(timebuf, sizeof(timebuf), "Result of merge+%s",
		    tbuf);
	} else {
		strlcpy(timebuf, tbuf, sizeof(timebuf));
	}

	if (reset_tag) {
		sticky[0] = '\0';
	} else if (co_flags & CO_SETSTICKY)
		if (tag != NULL)
			(void)xsnprintf(sticky, sizeof(sticky), "T%s", tag);
		else if (cvs_specified_date != -1) {
			gmtime_r(&cvs_specified_date, &datetm);
			(void)strftime(sticky, sizeof(sticky),
			    "D"CVS_DATE_FMT, &datetm);
		} else if (cvs_directory_date != -1) {
			gmtime_r(&cvs_directory_date, &datetm);
			(void)strftime(sticky, sizeof(sticky),
			    "D"CVS_DATE_FMT, &datetm);
		} else
			(void)xsnprintf(sticky, sizeof(sticky), "T%s", rev);
	else if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL)
		(void)xsnprintf(sticky, sizeof(sticky), "T%s",
		    cf->file_ent->ce_tag);
	else
		sticky[0] = '\0';

	kbuf[0] = '\0';
	if (cf->file_rcs != NULL && cf->file_rcs->rf_expand != NULL) {
		cf_kflag = rcs_kflag_get(cf->file_rcs->rf_expand);
		if (kflag || cf_kflag != RCS_KWEXP_DEFAULT)
			(void)xsnprintf(kbuf, sizeof(kbuf),
			    "-k%s", cf->file_rcs->rf_expand);
	} else if (!reset_option && cf->file_ent != NULL) {
		if (cf->file_ent->ce_opts != NULL)
			strlcpy(kbuf, cf->file_ent->ce_opts, sizeof(kbuf));
	}

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	cvs_ent_line_str(cf->file_name, rev, timebuf, kbuf, sticky, 0, 0,
	    entry, CVS_ENT_MAXLINELEN);

	if (cvs_server_active == 0) {
		if (!(co_flags & CO_REMOVE) && cvs_cmdop != CVS_OP_EXPORT) {
			ent = cvs_ent_open(cf->file_wd);
			cvs_ent_add(ent, entry);
			cf->file_ent = cvs_ent_parse(entry);
		}
	} else {
		if (co_flags & CO_MERGE) {
			(void)unlink(cf->file_path);
			cvs_merge_file(cf, (cvs_join_rev1 == NULL));
			tosend = cf->file_path;
		}

		/*
		 * If this file has a tag, push out the Directory with the
		 * tag to the client. Except when this file was explicitly
		 * specified on the command line.
		 */
		if (tag != NULL && strcmp(cf->file_wd, lastwd) &&
		    !(cf->file_flags & FILE_USER_SUPPLIED)) {
			strlcpy(lastwd, cf->file_wd, MAXPATHLEN);
			cvs_server_set_sticky(cf->file_wd, sticky);
		}

		if (co_flags & CO_COMMIT)
			cvs_server_update_entry("Updated", cf);
		else if (co_flags & CO_MERGE)
			cvs_server_update_entry("Merged", cf);
		else if (co_flags & CO_REMOVE)
			cvs_server_update_entry("Removed", cf);
		else
			cvs_server_update_entry("Updated", cf);

		if (!(co_flags & CO_REMOVE)) {
			cvs_remote_output(entry);

			if (!(co_flags & CO_MERGE)) {
				mode = cf->file_rcs->rf_mode;
				mode |= S_IWUSR;
				bp = rcs_rev_getbuf(cf->file_rcs, rnum, 0);
				cvs_remote_send_file_buf(cf->file_path,
				    bp, mode);
			} else {
				cvs_remote_send_file(tosend, cf->fd);
			}
		}
	}

	xfree(entry);
}

/*	$OpenBSD: commit.c,v 1.151 2011/04/20 18:41:40 nicm Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

void			 cvs_commit_local(struct cvs_file *);
void			 cvs_commit_check_files(struct cvs_file *);
void			 cvs_commit_loginfo(char *);
void			 cvs_commit_lock_dirs(struct cvs_file *);

static BUF *commit_diff(struct cvs_file *, RCSNUM *, int);
static void commit_desc_set(struct cvs_file *);

struct	file_info_list	 files_info;
struct	trigger_list	*line_list;

struct cvs_flisthead	files_affected;
struct cvs_flisthead	files_added;
struct cvs_flisthead	files_removed;
struct cvs_flisthead	files_modified;

int	conflicts_found;
char	*logmsg = NULL;
char	*loginfo = NULL;

struct cvs_cmd cvs_cmd_commit = {
	CVS_OP_COMMIT, CVS_USE_WDIR | CVS_LOCK_REPO, "commit",
	{ "ci", "com" },
	"Check files into the repository",
	"[-flR] [-F logfile | -m msg] [-r rev] ...",
	"F:flm:Rr:",
	NULL,
	cvs_commit
};

int
cvs_commit(int argc, char **argv)
{
	int flags;
	int ch, Fflag, mflag;
	struct module_checkout *mc;
	struct cvs_recursion cr;
	struct cvs_filelist *l;
	struct file_info *fi;
	char *arg = ".", repo[MAXPATHLEN];

	flags = CR_RECURSE_DIRS;
	Fflag = mflag = 0;

	while ((ch = getopt(argc, argv, cvs_cmd_commit.cmd_opts)) != -1) {
		switch (ch) {
		case 'F':
			/* free previously assigned value */
			if (logmsg != NULL)
				xfree(logmsg);
			logmsg = cvs_logmsg_read(optarg);
			Fflag = 1;
			break;
		case 'f':
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'm':
			/* free previously assigned value */
			if (logmsg != NULL)
				xfree(logmsg);
			logmsg = xstrdup(optarg);
			mflag = 1;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			break;
		default:
			fatal("%s", cvs_cmd_commit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	/* -F and -m are mutually exclusive */
	if (Fflag && mflag)
		fatal("cannot specify both a log file and a message");

	RB_INIT(&files_affected);
	RB_INIT(&files_added);
	RB_INIT(&files_removed);
	RB_INIT(&files_modified);

	TAILQ_INIT(&files_info);
	conflicts_found = 0;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.fileproc = cvs_commit_check_files;
	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (conflicts_found != 0)
		fatal("%d conflicts found, please correct these first",
		    conflicts_found);

	if (RB_EMPTY(&files_affected))
		return (0);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		if (logmsg == NULL) {
			logmsg = cvs_logmsg_create(NULL, &files_added,
			    &files_removed, &files_modified);
			if (logmsg == NULL)
				fatal("This shouldnt happen, honestly!");
		}
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		cvs_client_send_logmsg(logmsg);
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("ci");
		cvs_client_get_responses();
	} else {
		cvs_get_repository_name(".", repo, MAXPATHLEN);

		line_list = cvs_trigger_getlines(CVS_PATH_COMMITINFO, repo);
		if (line_list != NULL) {
			RB_FOREACH(l, cvs_flisthead, &files_affected) {
				fi = xcalloc(1, sizeof(*fi));
				fi->file_path = xstrdup(l->file_path);
				TAILQ_INSERT_TAIL(&files_info, fi,
				    flist);
			}

			if (cvs_trigger_handle(CVS_TRIGGER_COMMITINFO,
			    repo, NULL, line_list, &files_info)) {
				cvs_log(LP_ERR,
				    "Pre-commit check failed");
				cvs_trigger_freelist(line_list);
				goto end;
			}

			cvs_trigger_freelist(line_list);
			cvs_trigger_freeinfo(&files_info);
		}

		if (cvs_server_active) {
			if (logmsg == NULL)
				fatal("no log message specified");
		} else if (logmsg == NULL) {
			logmsg = cvs_logmsg_create(NULL, &files_added,
			    &files_removed, &files_modified);
			if (logmsg == NULL)
				fatal("This shouldnt happen, honestly!");
		}

		if (cvs_logmsg_verify(logmsg))
			goto end;

		cr.fileproc = cvs_commit_lock_dirs;
		cvs_file_walklist(&files_affected, &cr);

		line_list = cvs_trigger_getlines(CVS_PATH_LOGINFO, repo);

		cr.fileproc = cvs_commit_local;
		cvs_file_walklist(&files_affected, &cr);

		if (line_list != NULL) {
			cvs_commit_loginfo(repo);

			cvs_trigger_handle(CVS_TRIGGER_LOGINFO, repo,
			    loginfo, line_list, &files_info);

			xfree(loginfo);
			cvs_trigger_freelist(line_list);
			cvs_trigger_freeinfo(&files_info);
		}

		mc = cvs_module_lookup(repo);
		if (mc->mc_prog != NULL &&
		    (mc->mc_flags & MODULE_RUN_ON_COMMIT))
			cvs_exec(mc->mc_prog, NULL, 0);
	}

end:
	cvs_trigger_freeinfo(&files_info);
	if (logmsg != NULL)
		xfree(logmsg);
	return (0);
}

void
cvs_commit_loginfo(char *repo)
{
	BUF *buf;
	char pwd[MAXPATHLEN];
	struct cvs_filelist *cf;

	if (getcwd(pwd, sizeof(pwd)) == NULL)
		fatal("Can't get working directory");

	buf = buf_alloc(1024);

	cvs_trigger_loginfo_header(buf, repo);

	if (!RB_EMPTY(&files_added)) {
		buf_puts(buf, "Added Files:");

		RB_FOREACH(cf, cvs_flisthead, &files_added) {
			buf_putc(buf, '\n');
			buf_putc(buf, '\t');
			buf_puts(buf, cf->file_path);
		}

		buf_putc(buf, '\n');
	}

	if (!RB_EMPTY(&files_modified)) {
		buf_puts(buf, "Modified Files:");

		RB_FOREACH(cf, cvs_flisthead, &files_modified) {
			buf_putc(buf, '\n');
			buf_putc(buf, '\t');
			buf_puts(buf, cf->file_path);
		}

		buf_putc(buf, '\n');
	}

	if (!RB_EMPTY(&files_removed)) {
		buf_puts(buf, "Removed Files:");

		RB_FOREACH(cf, cvs_flisthead, &files_removed) {
			buf_putc(buf, '\n');
			buf_putc(buf, '\t');
			buf_puts(buf, cf->file_path);
		}

		buf_putc(buf, '\n');
	}

	buf_puts(buf, "Log Message:\n");

	buf_puts(buf, logmsg);

	buf_putc(buf, '\n');
	buf_putc(buf, '\0');

	loginfo = buf_release(buf);
}

void
cvs_commit_lock_dirs(struct cvs_file *cf)
{
	char repo[MAXPATHLEN];

	cvs_get_repository_path(cf->file_wd, repo, sizeof(repo));
	cvs_log(LP_TRACE, "cvs_commit_lock_dirs: %s", repo);

	/* locks stay in place until we are fully done and exit */
	cvs_repository_lock(repo, 1);
}

void
cvs_commit_check_files(struct cvs_file *cf)
{
	char *tag;
	RCSNUM *branch, *brev;

	branch = brev = NULL;

	cvs_log(LP_TRACE, "cvs_commit_check_files(%s)", cf->file_path);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL)
		cvs_remote_classify_file(cf);
	else
		cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Examining %s", cf->file_path);
		return;
	}

	if (cf->file_status == FILE_UPTODATE)
		return;

	if (cf->file_status == FILE_MERGE ||
	    cf->file_status == FILE_PATCH ||
	    cf->file_status == FILE_CHECKOUT ||
	    cf->file_status == FILE_LOST ||
	    cf->file_status == FILE_UNLINK) {
		cvs_log(LP_ERR, "conflict: %s is not up-to-date",
		    cf->file_path);
		conflicts_found++;
		return;
	}

	if (cf->file_status == FILE_CONFLICT &&
	   cf->file_ent->ce_conflict != NULL) {
		cvs_log(LP_ERR, "conflict: unresolved conflicts in %s from "
		    "merging, please fix these first", cf->file_path);
		conflicts_found++;
		return;
	}

	if (cf->file_status == FILE_MODIFIED &&
	    cf->file_ent->ce_conflict != NULL &&
	    update_has_conflict_markers(cf)) {
		cvs_log(LP_ERR, "warning: file %s seems to still contain "
		    "conflict indicators", cf->file_path);
	}

	if (cf->file_ent != NULL && cf->file_ent->ce_date != -1) {
		cvs_log(LP_ERR, "conflict: cannot commit to sticky date for %s",
		    cf->file_path);
		conflicts_found++;
		return;
	}

	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		tag = cvs_directory_tag;
		if (cf->file_ent != NULL)
			tag = cf->file_ent->ce_tag;

		if (tag != NULL && cf->file_rcs != NULL) {
			brev = rcs_sym_getrev(cf->file_rcs, tag);
			if (brev != NULL) {
				if (RCSNUM_ISBRANCH(brev))
					goto next;
				rcsnum_free(brev);
			}

			brev = rcs_translate_tag(tag, cf->file_rcs);

			if (brev == NULL) {
				if (cf->file_status == FILE_ADDED)
					goto next;
				fatal("failed to resolve tag: %s",
				    cf->file_ent->ce_tag);
			}

			if ((branch = rcsnum_revtobr(brev)) == NULL) {
				cvs_log(LP_ERR, "sticky tag %s is not "
				    "a branch for file %s", tag,
				    cf->file_path);
				conflicts_found++;
				rcsnum_free(brev);
				return;
			}

			if (!RCSNUM_ISBRANCHREV(brev)) {
				cvs_log(LP_ERR, "sticky tag %s is not "
				    "a branch for file %s", tag,
				    cf->file_path);
				conflicts_found++;
				rcsnum_free(branch);
				rcsnum_free(brev);
				return;
			}

			if (!RCSNUM_ISBRANCH(branch)) {
				cvs_log(LP_ERR, "sticky tag %s is not "
				    "a branch for file %s", tag,
				    cf->file_path);
				conflicts_found++;
				rcsnum_free(branch);
				rcsnum_free(brev);
				return;
			}
		}
	}

next:
	if (branch != NULL)
		rcsnum_free(branch);
	if (brev != NULL)
		rcsnum_free(brev);

	if (cf->file_status != FILE_ADDED &&
	    cf->file_status != FILE_REMOVED &&
	    cf->file_status != FILE_MODIFIED)
		return;

	cvs_file_get(cf->file_path, 0, &files_affected, CVS_FILE);

	switch (cf->file_status) {
	case FILE_ADDED:
		cvs_file_get(cf->file_path, 0, &files_added, CVS_FILE);
		break;
	case FILE_REMOVED:
		cvs_file_get(cf->file_path, 0, &files_removed, CVS_FILE);
		break;
	case FILE_MODIFIED:
		cvs_file_get(cf->file_path, 0, &files_modified, CVS_FILE);
		break;
	}
}

void
cvs_commit_local(struct cvs_file *cf)
{
	char *tag;
	BUF *b, *d;
	int onbranch, isnew, histtype, branchadded;
	RCSNUM *nrev, *crev, *rrev, *brev;
	int openflags, rcsflags;
	char rbuf[CVS_REV_BUFSZ], nbuf[CVS_REV_BUFSZ];
	CVSENTRIES *entlist;
	char attic[MAXPATHLEN], repo[MAXPATHLEN], rcsfile[MAXPATHLEN];
	struct file_info *fi;

	cvs_log(LP_TRACE, "cvs_commit_local(%s)", cf->file_path);
	cvs_file_classify(cf, cvs_directory_tag);

	if (cvs_noexec == 1)
		return;

	if (cf->file_type != CVS_FILE)
		fatal("cvs_commit_local: '%s' is not a file", cf->file_path);

	tag = cvs_directory_tag;
	if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL)
		tag = cf->file_ent->ce_tag;

	branchadded = 0;
	switch (cf->file_status) {
	case FILE_ADDED:
		if (cf->file_rcs == NULL && tag != NULL) {
			branchadded = 1;
			cvs_add_tobranch(cf, tag);
		}
		break;
	case FILE_MODIFIED:
	case FILE_REMOVED:
		if (cf->file_rcs == NULL) {
			cvs_log(LP_ERR, "RCS file for %s got lost",
			    cf->file_path);
			return;
		}
		break;
	default:
		cvs_log(LP_ERR, "skipping bogus file `%s'", cf->file_path);
		return;
	}

	onbranch = 0;
	nrev = RCS_HEAD_REV;
	crev = NULL;
	rrev = NULL;
	d = NULL;

	if (cf->file_rcs != NULL && cf->file_rcs->rf_branch != NULL) {
		rcsnum_free(cf->file_rcs->rf_branch);
		cf->file_rcs->rf_branch = NULL;
	}

	if (cf->file_rcs != NULL) {
		rrev = rcs_head_get(cf->file_rcs);
		crev = rcs_head_get(cf->file_rcs);
		if (crev == NULL || rrev == NULL)
			fatal("no head revision in RCS file for %s",
			    cf->file_path);

		if (tag != NULL) {
			rcsnum_free(crev);
			rcsnum_free(rrev);
			brev = rcs_sym_getrev(cf->file_rcs, tag);
			crev = rcs_translate_tag(tag, cf->file_rcs);
			if (brev == NULL || crev == NULL) {
				fatal("failed to resolve existing tag: %s",
				    tag);
			}

			rrev = rcsnum_alloc();
			rcsnum_cpy(brev, rrev, brev->rn_len - 1);

			if (RCSNUM_ISBRANCHREV(crev) &&
			    rcsnum_cmp(crev, rrev, 0)) {
				nrev = rcsnum_alloc();
				rcsnum_cpy(crev, nrev, 0);
				rcsnum_inc(nrev);
			} else if (!RCSNUM_ISBRANCH(crev)) {
				nrev = rcsnum_brtorev(brev);
				if (nrev == NULL)
					fatal("failed to create branch rev");
			} else {
				fatal("this isnt suppose to happen, honestly");
			}

			rcsnum_free(brev);
			rcsnum_free(rrev);
			rrev = rcsnum_branch_root(nrev);

			/* branch stuff was checked in cvs_commit_check_files */
			onbranch = 1;
		}

		rcsnum_tostr(crev, rbuf, sizeof(rbuf));
	} else {
		strlcpy(rbuf, "Non-existent", sizeof(rbuf));
	}

	if (rrev != NULL)
		rcsnum_free(rrev);
	isnew = 0;
	if (cf->file_status == FILE_ADDED) {
		isnew = 1;
		rcsflags = RCS_CREATE;
		openflags = O_CREAT | O_RDONLY;
		if (cf->file_rcs != NULL) {
			if (!onbranch) {
				if (cf->in_attic == 0)
					cvs_log(LP_ERR, "warning: expected %s "
					    "to be in the Attic",
					    cf->file_path);

				if (cf->file_rcs->rf_dead == 0)
					cvs_log(LP_ERR, "warning: expected %s "
					    "to be dead", cf->file_path);

				cvs_get_repository_path(cf->file_wd, repo,
				    MAXPATHLEN);
				(void)xsnprintf(rcsfile, MAXPATHLEN, "%s/%s%s",
				    repo, cf->file_name, RCS_FILE_EXT);

				if (rename(cf->file_rpath, rcsfile) == -1)
					fatal("cvs_commit_local: failed to "
					    "move %s outside the Attic: %s",
					    cf->file_path, strerror(errno));

				xfree(cf->file_rpath);
				cf->file_rpath = xstrdup(rcsfile);
				isnew = 0;
			}

			rcsflags = RCS_READ | RCS_PARSE_FULLY;
			openflags = O_RDONLY;
			rcs_close(cf->file_rcs);
		}

		cf->repo_fd = open(cf->file_rpath, openflags);
		if (cf->repo_fd < 0)
			fatal("cvs_commit_local: %s", strerror(errno));

		cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd,
		    rcsflags, 0444);
		if (cf->file_rcs == NULL)
			fatal("cvs_commit_local: failed to create RCS file "
			    "for %s", cf->file_path);

		commit_desc_set(cf);

		if (branchadded)
			strlcpy(rbuf, "Non-existent", sizeof(rbuf));
	}

	if (verbosity > 1) {
		cvs_printf("Checking in %s:\n", cf->file_path);
		cvs_printf("%s <- %s\n", cf->file_rpath, cf->file_path);
		cvs_printf("old revision: %s; ", rbuf);
	}

	if (isnew == 0 && cf->file_rcs->rf_head == NULL)
		fatal("no head revision in RCS file for %s", cf->file_path);

	if (isnew == 0 && onbranch == 0)
		d = commit_diff(cf, cf->file_rcs->rf_head, 0);

	if (cf->file_status == FILE_REMOVED) {
		b = rcs_rev_getbuf(cf->file_rcs, crev, 0);
	} else if (onbranch == 1) {
		b = commit_diff(cf, crev, 1);
	} else {
		b = buf_load_fd(cf->fd);
	}

	if (isnew == 0 && onbranch == 0) {
		if (rcs_deltatext_set(cf->file_rcs, crev, d) == -1)
			fatal("cvs_commit_local: failed to set delta");
	}

	if (rcs_rev_add(cf->file_rcs, nrev, logmsg, -1, NULL) == -1)
		fatal("cvs_commit_local: failed to add new revision");

	if (nrev == RCS_HEAD_REV)
		nrev = cf->file_rcs->rf_head;

	if (rcs_deltatext_set(cf->file_rcs, nrev, b) == -1)
		fatal("cvs_commit_local: failed to set new HEAD delta");

	if (cf->file_status == FILE_REMOVED) {
		if (rcs_state_set(cf->file_rcs, nrev, RCS_STATE_DEAD) == -1)
			fatal("cvs_commit_local: failed to set state");
	}

	if (cf->file_status == FILE_ADDED && cf->file_ent->ce_opts != NULL) {
		int cf_kflag;

		cf_kflag = rcs_kflag_get(cf->file_ent->ce_opts + 2);
		rcs_kwexp_set(cf->file_rcs, cf_kflag);
	}

	rcs_write(cf->file_rcs);

	if (cf->file_status == FILE_REMOVED) {
		strlcpy(nbuf, "Removed", sizeof(nbuf));
	} else if (cf->file_status == FILE_ADDED) {
		if (cf->file_rcs->rf_dead == 1)
			strlcpy(nbuf, "Initial Revision", sizeof(nbuf));
		else
			rcsnum_tostr(nrev, nbuf, sizeof(nbuf));
	} else if (cf->file_status == FILE_MODIFIED) {
		rcsnum_tostr(nrev, nbuf, sizeof(nbuf));
	}

	if (verbosity > 1)
		cvs_printf("new revision: %s\n", nbuf);

	(void)unlink(cf->file_path);
	(void)close(cf->fd);
	cf->fd = -1;

	if (cf->file_status != FILE_REMOVED) {
		cvs_checkout_file(cf, nrev, NULL, CO_COMMIT);
	} else {
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);

		cvs_get_repository_path(cf->file_wd, repo, MAXPATHLEN);

		(void)xsnprintf(attic, MAXPATHLEN, "%s/%s",
		    repo, CVS_PATH_ATTIC);

		if (mkdir(attic, 0755) == -1 && errno != EEXIST)
			fatal("cvs_commit_local: failed to create Attic");

		(void)xsnprintf(attic, MAXPATHLEN, "%s/%s/%s%s", repo,
		    CVS_PATH_ATTIC, cf->file_name, RCS_FILE_EXT);

		if (rename(cf->file_rpath, attic) == -1)
			fatal("cvs_commit_local: failed to move %s to Attic",
			    cf->file_path);

		if (cvs_server_active == 1)
			cvs_server_update_entry("Remove-entry", cf);
	}

	if (verbosity > 1)
		cvs_printf("done\n");
	else {
		cvs_log(LP_NOTICE, "checking in '%s'; revision %s -> %s",
		    cf->file_path, rbuf, nbuf);
	}

	if (line_list != NULL) {
		fi = xcalloc(1, sizeof(*fi));
		fi->file_path = xstrdup(cf->file_path);
		fi->crevstr = xstrdup(rbuf);
		fi->nrevstr = xstrdup(nbuf);
		if (tag != NULL)
			fi->tag_new = xstrdup(tag);
		TAILQ_INSERT_TAIL(&files_info, fi, flist);
	}

	switch (cf->file_status) {
	case FILE_MODIFIED:
		histtype = CVS_HISTORY_COMMIT_MODIFIED;
		break;
	case FILE_ADDED:
		histtype = CVS_HISTORY_COMMIT_ADDED;
		break;
	case FILE_REMOVED:
		histtype = CVS_HISTORY_COMMIT_REMOVED;
		break;
	default:
		histtype = -1;
		break;
	}

	if (crev != NULL)
		rcsnum_free(crev);

	if (histtype != -1)
		cvs_history_add(histtype, cf, NULL);
	else
		cvs_log(LP_NOTICE, "histtype was -1 for %s", cf->file_path);
}

static BUF *
commit_diff(struct cvs_file *cf, RCSNUM *rev, int reverse)
{
	int fd1, fd2, d;
	char *p1, *p2;
	BUF *b;

	(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);

	if (cf->file_status == FILE_MODIFIED ||
	    cf->file_status == FILE_ADDED) {
		b = buf_load_fd(cf->fd);
		fd1 = buf_write_stmp(b, p1, NULL);
		buf_free(b);
	} else {
		fd1 = rcs_rev_write_stmp(cf->file_rcs, rev, p1, 0);
	}

	(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);
	fd2 = rcs_rev_write_stmp(cf->file_rcs, rev, p2, RCS_KWEXP_NONE);

	b = buf_alloc(128);

	diff_format = D_RCSDIFF;

	if (reverse == 1)
		d = diffreg(p2, p1, fd2, fd1, b, D_FORCEASCII);
	else
		d = diffreg(p1, p2, fd1, fd2, b, D_FORCEASCII);
	if (d == D_ERROR)
		fatal("commit_diff: failed to get RCS patch");

	close(fd1);
	close(fd2);

	xfree(p1);
	xfree(p2);

	return (b);
}

static void
commit_desc_set(struct cvs_file *cf)
{
	BUF *bp;
	int fd;
	char desc_path[MAXPATHLEN], *desc;

	(void)xsnprintf(desc_path, MAXPATHLEN, "%s/%s/%s%s",
	    cf->file_wd, CVS_PATH_CVSDIR, cf->file_name, CVS_DESCR_FILE_EXT);

	if ((fd = open(desc_path, O_RDONLY)) == -1)
		return;

	bp = buf_load_fd(fd);
	buf_putc(bp, '\0');
	desc = buf_release(bp);

	rcs_desc_set(cf->file_rcs, desc);

	(void)close(fd);
	(void)cvs_unlink(desc_path);

	xfree(desc);
}

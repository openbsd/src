/*	$OpenBSD: update.c,v 1.163 2010/07/30 21:47:18 ray Exp $	*/
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

int	prune_dirs = 0;
int	print_stdout = 0;
int	build_dirs = 0;
int	reset_option = 0;
int	reset_tag = 0;
char *cvs_specified_tag = NULL;
char *cvs_join_rev1 = NULL;
char *cvs_join_rev2 = NULL;

static char *koptstr;
static char *dateflag = NULL;
static int Aflag = 0;

static void update_clear_conflict(struct cvs_file *);
static void update_join_file(struct cvs_file *);

extern CVSENTRIES *current_list;

struct cvs_cmd cvs_cmd_update = {
	CVS_OP_UPDATE, CVS_USE_WDIR, "update",
	{ "up", "upd" },
	"Bring work tree in sync with repository",
	"[-ACdflPpR] [-D date | -r rev] [-I ign] [-j rev] [-k mode] "
	"[-t id] ...",
	"ACD:dfI:j:k:lPpQqRr:t:u",
	NULL,
	cvs_update
};

int
cvs_update(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_update.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			Aflag = 1;
			if (koptstr == NULL)
				reset_option = 1;
			if (cvs_specified_tag == NULL)
				reset_tag = 1;
			break;
		case 'C':
			break;
		case 'D':
			dateflag = optarg;
			if ((cvs_specified_date = date_parse(dateflag)) == -1)
				fatal("invalid date: %s", dateflag);
			reset_tag = 0;
			break;
		case 'd':
			build_dirs = 1;
			break;
		case 'f':
			break;
		case 'I':
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
				fatal("%s", cvs_cmd_update.cmd_synopsis);
			}
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'P':
			prune_dirs = 1;
			break;
		case 'p':
			print_stdout = 1;
			cvs_noexec = 1;
			break;
		case 'Q':
		case 'q':
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			reset_tag = 0;
			cvs_specified_tag = optarg;
			break;
		case 'u':
			break;
		default:
			fatal("%s", cvs_cmd_update.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		cr.enterdir = cvs_update_enterdir;
		cr.leavedir = prune_dirs ? cvs_update_leavedir : NULL;
		cr.fileproc = cvs_update_local;
		flags |= CR_REPO;
	} else {
		cvs_client_connect_to_server();
		if (Aflag)
			cvs_client_send_request("Argument -A");
		if (dateflag != NULL)
			cvs_client_send_request("Argument -D%s", dateflag);
		if (build_dirs)
			cvs_client_send_request("Argument -d");
		if (kflag)
			cvs_client_send_request("Argument -k%s", koptstr);
		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
		if (prune_dirs)
			cvs_client_send_request("Argument -P");
		if (print_stdout)
			cvs_client_send_request("Argument -p");

		if (cvs_specified_tag != NULL)
			cvs_client_send_request("Argument -r%s",
			    cvs_specified_tag);

		cr.enterdir = NULL;
		cr.leavedir = NULL;
		cr.fileproc = cvs_client_sendfile;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("update");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_update_enterdir(struct cvs_file *cf)
{
	CVSENTRIES *entlist;
	char *dirtag, *entry, fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_update_enterdir(%s)", cf->file_path);

	cvs_file_classify(cf, NULL);

	if (cf->file_status == DIR_CREATE && build_dirs == 1) {
		cvs_parse_tagfile(cf->file_wd, &dirtag, NULL, NULL);
		cvs_mkpath(cf->file_path, cvs_specified_tag != NULL ?
		    cvs_specified_tag : dirtag);
		if (dirtag != NULL)
			xfree(dirtag);

		if ((cf->fd = open(cf->file_path, O_RDONLY)) == -1)
			fatal("cvs_update_enterdir: `%s': %s",
			    cf->file_path, strerror(errno));

		if (cvs_server_active == 1 && cvs_cmdop != CVS_OP_CHECKOUT)
			cvs_server_clear_sticky(cf->file_path);

		if (cvs_cmdop != CVS_OP_EXPORT) {
			(void)xasprintf(&entry, "D/%s////", cf->file_name);

			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, entry);
			xfree(entry);
		}
	} else if ((cf->file_status == DIR_CREATE && build_dirs == 0) ||
		    cf->file_status == FILE_UNKNOWN) {
		cf->file_status = FILE_SKIP;
	} else if (reset_tag) {
		(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
		    cf->file_path, CVS_PATH_TAG);
		(void)unlink(fpath);
	} else {
		if (cvs_specified_tag != NULL || cvs_specified_date != -1)
			cvs_write_tagfile(cf->file_path,
				    cvs_specified_tag, NULL);
	}
}

void
cvs_update_leavedir(struct cvs_file *cf)
{
	long base;
	int nbytes;
	int isempty;
	size_t bufsize;
	struct stat st;
	struct dirent *dp;
	char *buf, *ebuf, *cp;
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_leavedir(%s)", cf->file_path);

	if (cvs_server_active == 1 && !strcmp(cf->file_name, "."))
		return;

	entlist = cvs_ent_open(cf->file_path);
	if (!TAILQ_EMPTY(&(entlist->cef_ent))) {
		isempty = 0;
		goto prune_it;
	}

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	bufsize = st.st_size;
	if (bufsize < st.st_blksize)
		bufsize = st.st_blksize;

	if (st.st_size > SIZE_MAX)
		fatal("cvs_update_leavedir: %s: file size too big",
		    cf->file_name);

	isempty = 1;
	buf = xmalloc(bufsize);

	if (lseek(cf->fd, 0, SEEK_SET) == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	while ((nbytes = getdirentries(cf->fd, buf, bufsize, &base)) > 0) {
		ebuf = buf + nbytes;
		cp = buf;

		while (cp < ebuf) {
			dp = (struct dirent *)cp;
			if (!strcmp(dp->d_name, ".") ||
			    !strcmp(dp->d_name, "..") ||
			    dp->d_fileno == 0) {
				cp += dp->d_reclen;
				continue;
			}

			if (strcmp(dp->d_name, CVS_PATH_CVSDIR)) 
				isempty = 0;

			if (isempty == 0)
				break;

			cp += dp->d_reclen;
		}
	}

	if (nbytes == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	xfree(buf);

prune_it:
	if ((isempty == 1 && prune_dirs == 1) ||
	    (cvs_server_active == 1 && cvs_cmdop == CVS_OP_CHECKOUT)) {
		/* XXX */
		cvs_rmdir(cf->file_path);

		if (cvs_server_active == 0 && cvs_cmdop != CVS_OP_EXPORT) {
			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_remove(entlist, cf->file_name);
		}
	}
}

void
cvs_update_local(struct cvs_file *cf)
{
	CVSENTRIES *entlist;
	int ent_kflag, rcs_kflag, ret, flags;
	char *tag, rbuf[CVS_REV_BUFSZ];

	cvs_log(LP_TRACE, "cvs_update_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (cf->file_status == FILE_SKIP) {
			if (cvs_cmdop == CVS_OP_EXPORT && verbosity > 0)
				cvs_printf("? %s\n", cf->file_path);
			return;
		}

		if (cf->file_status != FILE_UNKNOWN &&
		    verbosity > 1)
			cvs_log(LP_ERR, "Updating %s", cf->file_path);
		return;
	}

	flags = 0;
	if (cvs_specified_tag != NULL)
		tag = cvs_specified_tag;
	else if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL)
		tag = cf->file_ent->ce_tag;
	else
		tag = cvs_directory_tag;

	cvs_file_classify(cf, tag);

	if (kflag && cf->file_rcs != NULL)
		rcs_kwexp_set(cf->file_rcs, kflag);

	if ((cf->file_status == FILE_UPTODATE ||
	    cf->file_status == FILE_MODIFIED) && cf->file_ent != NULL &&
	    cf->file_ent->ce_tag != NULL && reset_tag) {
		if (cf->file_status == FILE_MODIFIED)
			cf->file_status = FILE_MERGE;
		else
			cf->file_status = FILE_CHECKOUT;

		if ((cf->file_rcsrev = rcs_head_get(cf->file_rcs)) == NULL)
			fatal("no head revision in RCS file for %s",
			    cf->file_path);

		/* might be a bit overkill */
		if (cvs_server_active == 1)
			cvs_server_clear_sticky(cf->file_wd);
	}

	if (print_stdout) {
		if (cf->file_status != FILE_UNKNOWN && cf->file_rcs != NULL &&
		    cf->file_rcsrev != NULL && !cf->file_rcs->rf_dead &&
		    (cf->file_flags & FILE_HAS_TAG)) {
			rcsnum_tostr(cf->file_rcsrev, rbuf, sizeof(rbuf));
			if (verbosity > 1) {
				cvs_log(LP_RCS, RCS_DIFF_DIV);
				cvs_log(LP_RCS, "Checking out %s",
				    cf->file_path);
				cvs_log(LP_RCS, "RCS:  %s", cf->file_rpath);
				cvs_log(LP_RCS, "VERS: %s", rbuf);
				cvs_log(LP_RCS, "***************");
			}
			cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_DUMP);
		}
		return;
	}

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_opts == NULL) {
			if (kflag)
				cf->file_status = FILE_CHECKOUT;
		} else if (cf->file_rcs != NULL) {
			if (strlen(cf->file_ent->ce_opts) < 3)
				fatal("malformed option for file %s",
				    cf->file_path);

			ent_kflag = rcs_kflag_get(cf->file_ent->ce_opts + 2);
			rcs_kflag = rcs_kwexp_get(cf->file_rcs);

			if ((kflag && (kflag != ent_kflag)) ||
			    (reset_option && (ent_kflag != rcs_kflag)))
				cf->file_status = FILE_CHECKOUT;
		}
	}

	switch (cf->file_status) {
	case FILE_UNKNOWN:
		cvs_printf("? %s\n", cf->file_path);
		break;
	case FILE_MODIFIED:
		ret = update_has_conflict_markers(cf);
		if (cf->file_ent->ce_conflict != NULL && ret == 1) {
			cvs_printf("C %s\n", cf->file_path);
		} else {
			if (cf->file_ent->ce_conflict != NULL && ret == 0)
				update_clear_conflict(cf);
			cvs_printf("M %s\n", cf->file_path);
		}
		break;
	case FILE_ADDED:
		cvs_printf("A %s\n", cf->file_path);
		break;
	case FILE_REMOVED:
		cvs_printf("R %s\n", cf->file_path);
		break;
	case FILE_CONFLICT:
		cvs_printf("C %s\n", cf->file_path);
		break;
	case FILE_LOST:
	case FILE_CHECKOUT:
	case FILE_PATCH:
		if (!reset_tag && (tag != NULL || cvs_specified_date != -1 ||
		    cvs_directory_date != -1 || (cf->file_ent != NULL &&
		    cf->file_ent->ce_tag != NULL)))
			flags = CO_SETSTICKY;

		if (cf->file_flags & FILE_ON_DISK && (cf->file_ent == NULL ||
		    cf->file_ent->ce_type == CVS_ENT_NONE)) {
			cvs_log(LP_ERR, "move away %s; it is in the way",
			    cf->file_path);
			cvs_printf("C %s\n", cf->file_path);
		} else {
			cvs_checkout_file(cf, cf->file_rcsrev, tag, flags);
			cvs_printf("U %s\n", cf->file_path);
			cvs_history_add(CVS_HISTORY_UPDATE_CO, cf, NULL);
		}
		break;
	case FILE_MERGE:
		d3rev1 = cf->file_ent->ce_rev;
		d3rev2 = cf->file_rcsrev;
		cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_MERGE);

		if (diff3_conflicts != 0) {
			cvs_printf("C %s\n", cf->file_path);
			cvs_history_add(CVS_HISTORY_UPDATE_MERGED_ERR,
			    cf, NULL);
		} else {
			update_clear_conflict(cf);
			cvs_printf("M %s\n", cf->file_path);
			cvs_history_add(CVS_HISTORY_UPDATE_MERGED, cf, NULL);
		}
		break;
	case FILE_UNLINK:
		(void)unlink(cf->file_path);
	case FILE_REMOVE_ENTRY:
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);
		cvs_history_add(CVS_HISTORY_UPDATE_REMOVE, cf, NULL);

		if (cvs_server_active == 1)
			cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_REMOVE);
		break;
	case FILE_UPTODATE:
		if (cvs_cmdop != CVS_OP_UPDATE)
			break;

		if (reset_tag != 1 && reset_option != 1)
			break;

		if (cf->file_ent != NULL && cf->file_ent->ce_tag == NULL)
			break;

		if (cf->file_rcs->rf_dead != 1 &&
		    (cf->file_flags & FILE_HAS_TAG))
			cvs_checkout_file(cf, cf->file_rcsrev,
			    tag, CO_SETSTICKY);
		break;
	default:
		break;
	}

	if (cvs_join_rev1 != NULL)
		update_join_file(cf);
}

static void
update_clear_conflict(struct cvs_file *cf)
{
	CVSENTRIES *entlist;
	char *entry, revbuf[CVS_REV_BUFSZ];
	char sticky[CVS_ENT_MAXLINELEN], opt[4];

	cvs_log(LP_TRACE, "update_clear_conflict(%s)", cf->file_path);

	rcsnum_tostr(cf->file_rcsrev, revbuf, sizeof(revbuf));

	sticky[0] = '\0';
	if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL)
		(void)xsnprintf(sticky, sizeof(sticky), "T%s",
		    cf->file_ent->ce_tag);

	opt[0] = '\0';
	if (cf->file_ent != NULL && cf->file_ent->ce_opts != NULL)
		strlcpy(opt, cf->file_ent->ce_opts, sizeof(opt));

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	cvs_ent_line_str(cf->file_name, revbuf, "Result of merge",
	    opt[0] != '\0' ? opt : "", sticky, 0, 0,
	    entry, CVS_ENT_MAXLINELEN);

	entlist = cvs_ent_open(cf->file_wd);
	cvs_ent_add(entlist, entry);
	xfree(entry);
}

/*
 * XXX - this is the way GNU cvs checks for outstanding conflicts
 * in a file after a merge. It is a very very bad approach and
 * should be looked at once opencvs is working decently.
 */
int
update_has_conflict_markers(struct cvs_file *cf)
{
	BUF *bp;
	int conflict;
	char *content;
	struct rcs_line *lp;
	struct rcs_lines *lines;
	size_t len;

	cvs_log(LP_TRACE, "update_has_conflict_markers(%s)", cf->file_path);

	if (!(cf->file_flags & FILE_ON_DISK) || cf->file_ent == NULL)
		return (0);

	bp = buf_load_fd(cf->fd);

	buf_putc(bp, '\0');
	len = buf_len(bp);
	content = buf_release(bp);
	if ((lines = cvs_splitlines(content, len)) == NULL)
		fatal("update_has_conflict_markers: failed to split lines");

	conflict = 0;
	TAILQ_FOREACH(lp, &(lines->l_lines), l_list) {
		if (lp->l_line == NULL)
			continue;

		if (!strncmp(lp->l_line, RCS_CONFLICT_MARKER1,
		    sizeof(RCS_CONFLICT_MARKER1) - 1) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER2,
		    sizeof(RCS_CONFLICT_MARKER2) - 1) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER3,
		    sizeof(RCS_CONFLICT_MARKER3) - 1)) {
			conflict = 1;
			break;
		}
	}

	cvs_freelines(lines);
	xfree(content);
	return (conflict);
}

void
update_join_file(struct cvs_file *cf)
{
	int flag;
	time_t told;
	RCSNUM *rev1, *rev2;
	const char *state1, *state2;
	char rbuf[CVS_REV_BUFSZ], *jrev1, *jrev2, *p;

	rev1 = rev2 = NULL;
	jrev1 = jrev2 = NULL;

	jrev1 = xstrdup(cvs_join_rev1);
	if (cvs_join_rev2 != NULL)
		jrev2 = xstrdup(cvs_join_rev2);

	if (jrev2 == NULL) {
		jrev2 = jrev1;
		jrev1 = NULL;
	}

	told = cvs_specified_date;

	if ((p = strchr(jrev2, ':')) != NULL) {
		(*p++) = '\0';
		if ((cvs_specified_date = date_parse(p)) == -1) {
			cvs_printf("invalid date: %s", p);
			goto out;
		}
	}

	rev2 = rcs_translate_tag(jrev2, cf->file_rcs);
	cvs_specified_date = told;

	if (jrev1 != NULL) {
		if ((p = strchr(jrev1, ':')) != NULL) {
			(*p++) = '\0';
			if ((cvs_specified_date = date_parse(p)) == -1) {
				cvs_printf("invalid date: %s", p);
				goto out;
			}
		}

		rev1 = rcs_translate_tag(jrev1, cf->file_rcs);
		cvs_specified_date = told;
	} else {
		if (rev2 == NULL)
			goto out;

		rev1 = rcsnum_alloc();
		rcsnum_cpy(cf->file_rcsrev, rev1, 0);
	}

	state1 = state2 = RCS_STATE_DEAD;

	if (rev1 != NULL)
		state1 = rcs_state_get(cf->file_rcs, rev1);
	if (rev2 != NULL)
		state2 = rcs_state_get(cf->file_rcs, rev2);

	if (rev2 == NULL || !strcmp(state2, RCS_STATE_DEAD)) {
		if (rev1 == NULL || !strcmp(state1, RCS_STATE_DEAD))
			goto out;

		if (cf->file_status == FILE_REMOVED ||
		    cf->file_rcs->rf_dead == 1)
			goto out;

		if (cf->file_status == FILE_MODIFIED ||
		    cf->file_status == FILE_ADDED)
			goto out;

		(void)unlink(cf->file_path);
		(void)close(cf->fd);
		cf->fd = -1;
		cvs_remove_local(cf);
		goto out;
	}

	if (cf->file_ent != NULL) {
		if (!rcsnum_cmp(cf->file_ent->ce_rev, rev2, 0))
			goto out;
	}

	if (cf->file_rcsrev == NULL) {
		cvs_printf("non-mergable file: %s has no head revision!\n",
		    cf->file_path);
		goto out;
	}

	if (rev1 == NULL || !strcmp(state1, RCS_STATE_DEAD)) {
		if (cf->file_flags & FILE_ON_DISK) {
			cvs_printf("%s exists but has been added in %s\n",
			    cf->file_path, jrev2);
		} else {
			cvs_printf("A %s\n", cf->file_path);
			cvs_checkout_file(cf, cf->file_rcsrev, NULL, 0);
			cvs_add_local(cf);
		}
		goto out;
	}

	if (!rcsnum_cmp(rev1, rev2, 0))
		goto out;

	if (!(cf->file_flags & FILE_ON_DISK)) {
		cvs_printf("%s does not exist but is present in %s\n",
		    cf->file_path, jrev2);
		goto out;
	}

	flag = rcs_kwexp_get(cf->file_rcs);
	if (flag & RCS_KWEXP_NONE) {
		cvs_printf("non-mergable file: %s needs merge!\n",
		    cf->file_path);
		goto out;
	}

	cvs_printf("joining ");
	rcsnum_tostr(rev1, rbuf, sizeof(rbuf));
	cvs_printf("%s ", rbuf);

	rcsnum_tostr(rev2, rbuf, sizeof(rbuf));
	cvs_printf("%s ", rbuf);

	rcsnum_tostr(cf->file_rcsrev, rbuf, sizeof(rbuf));
	cvs_printf("into %s (%s)\n", cf->file_path, rbuf);

	d3rev1 = rev1;
	d3rev2 = rev2;
	cvs_checkout_file(cf, cf->file_rcsrev, NULL, CO_MERGE);

	if (diff3_conflicts == 0)
		update_clear_conflict(cf);

out:
	if (rev1 != NULL)
		rcsnum_free(rev1);
	if (rev2 != NULL)
		rcsnum_free(rev2);

	if (jrev1 != NULL)
		xfree(jrev1);
	if (jrev2 != NULL)
		xfree(jrev2);
}

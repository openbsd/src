/*	$OpenBSD: update.c,v 1.74 2006/06/16 14:07:42 joris Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "diff.h"

int	cvs_update(int, char **);
int	prune_dirs = 0;
int	build_dirs = 0;
int	reset_stickies = 0;
static char *tag = NULL;

static void update_clear_conflict(struct cvs_file *);

struct cvs_cmd cvs_cmd_update = {
	CVS_OP_UPDATE, 0, "update",
	{ "up", "upd" },
	"Bring work tree in sync with repository",
	"[-ACdflPpR] [-D date | -r rev] [-I ign] [-j rev] [-k mode] "
	"[-t id] ...",
	"ACD:dfI:j:k:lPpQqRr:t:",
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

	flags = CR_REPO | CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_update.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			reset_stickies = 1;
			break;
		case 'C':
		case 'D':
			tag = optarg;
			break;
		case 'd':
			build_dirs = 1;
			break;
		case 'f':
			break;
		case 'I':
			break;
		case 'j':
			break;
		case 'k':
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'P':
			prune_dirs = 1;
			break;
		case 'p':
			break;
		case 'Q':
		case 'q':
			break;
		case 'R':
			break;
		case 'r':
			tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_update.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = cvs_update_enterdir;
	cr.leavedir = cvs_update_leavedir;
	cr.local = cvs_update_local;
	cr.remote = NULL;
	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_update_enterdir(struct cvs_file *cf)
{
	int l;
	char *entry;
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_enterdir(%s)", cf->file_path);

	cvs_file_classify(cf, NULL, 0);

	if (cf->file_status == DIR_CREATE && build_dirs == 1) {
		cvs_mkpath(cf->file_path);
		if ((cf->fd = open(cf->file_path, O_RDONLY)) == -1)
			fatal("cvs_update_enterdir: %s", strerror(errno));

		entry = xmalloc(CVS_ENT_MAXLINELEN);
		l = snprintf(entry, CVS_ENT_MAXLINELEN, "D/%s////",
		    cf->file_name);
		if (l == -1 || l >= CVS_ENT_MAXLINELEN)
			fatal("cvs_update_enterdir: overflow");

		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_add(entlist, entry);
		cvs_ent_close(entlist, ENT_SYNC);
		xfree(entry);
	} else if (cf->file_status == DIR_CREATE && build_dirs == 0) {
		cf->file_status = FILE_SKIP;
	}
}

void
cvs_update_leavedir(struct cvs_file *cf)
{
	long base;
	int nbytes;
	int isempty, l;
	size_t bufsize;
	struct stat st;
	struct dirent *dp;
	char *buf, *ebuf, *cp;
	struct cvs_ent *ent;
	struct cvs_ent_line *line;
	CVSENTRIES *entlist;
	char *export;

	cvs_log(LP_TRACE, "cvs_update_leavedir(%s)", cf->file_path);

	if (cvs_cmdop == CVS_OP_EXPORT) {
		export = xmalloc(MAXPATHLEN);
		l = snprintf(export, MAXPATHLEN, "%s/%s", cf->file_path,
		    CVS_PATH_CVSDIR);
		if (l == -1 || l >= MAXPATHLEN)
			fatal("cvs_update_leavedir: overflow");

		/* XXX */
		if (cvs_rmdir(export) == -1)
			fatal("cvs_update_leavedir: %s: %s:", export,
			    strerror(errno));

		xfree(export);
		return;
	}

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	bufsize = st.st_size;
	if (bufsize < st.st_blksize)
		bufsize = st.st_blksize;

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
			    dp->d_reclen == 0) {
				cp += dp->d_reclen;
				continue;
			}

			if (!strcmp(dp->d_name, CVS_PATH_CVSDIR)) {
				entlist = cvs_ent_open(cf->file_path);
				TAILQ_FOREACH(line, &(entlist->cef_ent),
				    entries_list) {
					ent = cvs_ent_parse(line->buf);

					if (ent->ce_status == CVS_ENT_REMOVED) {
						isempty = 0;
						cvs_ent_free(ent);
						break;
					}

					cvs_ent_free(ent);
				}
				cvs_ent_close(entlist, ENT_NOSYNC);
			} else {
				isempty = 0;
			}

			if (isempty == 0)
				break;

			cp += dp->d_reclen;
		}
	}

	if (nbytes == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	xfree(buf);

	if (isempty == 1 && prune_dirs == 1) {
		/* XXX */
		cvs_rmdir(cf->file_path);

		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);
		cvs_ent_close(entlist, ENT_SYNC);
	}
}

void
cvs_update_local(struct cvs_file *cf)
{
	BUF *bp;
	int ret, flags;
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (cf->file_status == FILE_SKIP)
			return;

		if (cf->file_status != FILE_UNKNOWN &&
		    verbosity > 1)
			cvs_log(LP_NOTICE, "Updating %s", cf->file_path);
		return;
	}

	/*
	 * the bp buffer will be released inside rcs_kwexp_buf,
	 * which is called from cvs_checkout_file().
	 */
	flags = 0;
	bp = NULL;
	cvs_file_classify(cf, tag, 1);

	if (cf->file_status == FILE_UPTODATE && cf->file_ent != NULL &&
	    cf->file_ent->ce_tag != NULL && reset_stickies == 1) {
		cf->file_status = FILE_CHECKOUT;
		cf->file_rcsrev = rcs_head_get(cf->file_rcs);
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
		bp = rcs_getrev(cf->file_rcs, cf->file_rcsrev);
		if (bp == NULL)
			fatal("cvs_update_local: failed to get HEAD");

		if (tag != NULL)
			flags = CO_SETSTICKY;

		cvs_checkout_file(cf, cf->file_rcsrev, bp, flags);
		cvs_printf("U %s\n", cf->file_path);
		break;
	case FILE_MERGE:
		bp = cvs_diff3(cf->file_rcs, cf->file_path, cf->fd,
		    cf->file_ent->ce_rev, cf->file_rcsrev, 1);
		if (bp == NULL)
			fatal("cvs_update_local: failed to merge");

		cvs_checkout_file(cf, cf->file_rcsrev, bp, CO_MERGE);

		if (diff3_conflicts != 0) {
			cvs_printf("C %s\n", cf->file_path);
		} else {
			update_clear_conflict(cf);
			cvs_printf("M %s\n", cf->file_path);
		}
		break;
	case FILE_UNLINK:
		(void)unlink(cf->file_path);
	case FILE_REMOVE_ENTRY:
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);
		cvs_ent_close(entlist, ENT_SYNC);
		break;
	default:
		break;
	}
}

static void
update_clear_conflict(struct cvs_file *cf)
{
	int l;
	time_t now;
	CVSENTRIES *entlist;
	char *entry, revbuf[16], timebuf[32];

	cvs_log(LP_TRACE, "update_clear_conflict(%s)", cf->file_path);

	time(&now);
	ctime_r(&now, timebuf);
	if (timebuf[strlen(timebuf) - 1] == '\n')
		timebuf[strlen(timebuf) - 1] = '\0';

	rcsnum_tostr(cf->file_ent->ce_rev, revbuf, sizeof(revbuf));

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	l = snprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s/%s//",
	    cf->file_name, revbuf, timebuf);
	if (l == -1 || l >= CVS_ENT_MAXLINELEN)
		fatal("update_clear_conflict: overflow");

	entlist = cvs_ent_open(cf->file_wd);
	cvs_ent_add(entlist, entry);
	cvs_ent_close(entlist, ENT_SYNC);
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
	struct cvs_line *lp;
	struct cvs_lines *lines;

	cvs_log(LP_TRACE, "update_has_conflict_markers(%s)", cf->file_path);

	if ((bp = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
		fatal("update_has_conflict_markers: failed to load %s",
		    cf->file_path);

	cvs_buf_putc(bp, '\0');
	content = cvs_buf_release(bp);
	if ((lines = cvs_splitlines(content)) == NULL)
		fatal("update_has_conflict_markers: failed to split lines");

	xfree(content);

	conflict = 0;
	TAILQ_FOREACH(lp, &(lines->l_lines), l_list) {
		if (lp->l_line == NULL)
			continue;

		if (!strncmp(lp->l_line, RCS_CONFLICT_MARKER1,
		    strlen(RCS_CONFLICT_MARKER1)) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER2,
		    strlen(RCS_CONFLICT_MARKER2)) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER3,
		    strlen(RCS_CONFLICT_MARKER3))) {
			conflict = 1;
			break;
		}
	}

	cvs_freelines(lines);
	return (conflict);
}

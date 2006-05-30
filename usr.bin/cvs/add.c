/*	$OpenBSD: add.c,v 1.49 2006/05/30 08:23:31 xsa Exp $	*/
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
#include "diff.h"
#include "log.h"
#include "proto.h"

extern char *__progname;

int	cvs_add(int, char **);
void	cvs_add_local(struct cvs_file *);

static void add_directory(struct cvs_file *);
static void add_file(struct cvs_file *);

char	*logmsg;

struct cvs_cmd cvs_cmd_add = {
	CVS_OP_ADD, CVS_REQ_ADD, "add",
	{ "ad", "new" },
	"Add a new file or directory to the repository",
	"[-m message] ...",
	"m:",
	NULL,
	cvs_add
};

int
cvs_add(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_REPO;

	while ((ch = getopt(argc, argv, cvs_cmd_add.cmd_opts)) != -1) {
		switch (ch) {
		case 'm':
			logmsg = xstrdup(optarg);
			break;
		default:
			fatal("%s", cvs_cmd_add.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_add.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_add_local;
	cr.remote = NULL;
	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);
	return (0);
}

void
cvs_add_local(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_add_local(%s)", cf->file_path);

	cvs_file_classify(cf, 0);

	/* dont use `cvs add *' */
	if (strcmp(cf->file_name, ".") == 0 ||
	    strcmp(cf->file_name, "..") == 0 ||
	    strcmp(cf->file_name, CVS_PATH_CVSDIR) == 0) {
		if (verbosity > 1)
			cvs_log(LP_ERR,
			    "cannot add special file `%s'; skipping",
			    cf->file_name);
		return;
	}

	if (cf->file_type == CVS_DIR)
		add_directory(cf);
	else
		add_file(cf);
}

static void
add_directory(struct cvs_file *cf)
{
	int l;
	struct stat st;
	CVSENTRIES *entlist;
	char *entry, *repo;

	cvs_log(LP_TRACE, "add_directory(%s)", cf->file_path);

	entry = xmalloc(MAXPATHLEN);
	l = snprintf(entry, MAXPATHLEN, "%s%s", cf->file_rpath, RCS_FILE_EXT);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_add_local: overflow");

	if (stat(entry, &st) != -1) {
		cvs_log(LP_NOTICE, "cannot add directory %s: "
		    "a file with that name already exists",
		    cf->file_path);
	} else {
		l = snprintf(entry, MAXPATHLEN, "%s/%s", cf->file_path,
		    CVS_PATH_CVSDIR);
		if (l == -1 || l >= MAXPATHLEN)
			fatal("add_directory: overflow");

		if (stat(entry, &st) != -1) {
			if (!S_ISDIR(st.st_mode)) {
				cvs_log(LP_ERR, "%s exists but is not "
				    "directory", entry);
			} else {
				cvs_log(LP_NOTICE, "%s already exists",
				    entry);
			}
		} else {
			if (mkdir(cf->file_rpath, 0755) == -1 &&
			    errno != EEXIST)
				fatal("add_directory: %s: %s", cf->file_path,
				    strerror(errno));

			repo = xmalloc(MAXPATHLEN);
			cvs_get_repository_name(cf->file_wd, repo,
			    MAXPATHLEN);

			l = snprintf(entry, MAXPATHLEN, "%s/%s", repo,
			    cf->file_path);

			cvs_mkadmin(cf->file_path, current_cvsroot->cr_dir,
			    entry);

			xfree(repo);
			xfree(entry);

			entry = xmalloc(CVS_ENT_MAXLINELEN);
			l = snprintf(entry, CVS_ENT_MAXLINELEN,
			    "D/%s/////", cf->file_name);
			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, entry);
			cvs_ent_close(entlist, ENT_SYNC);

			cvs_printf("Directory %s added to the repository\n",
			    cf->file_rpath);
		}
	}

	cf->file_status = FILE_SKIP;
	xfree(entry);
}

static void
add_file(struct cvs_file *cf)
{
	BUF *b;
	int added, l, stop;
	char *entry, revbuf[16], tbuf[32];
	CVSENTRIES *entlist;

	if (cf->file_rcs != NULL)
		rcsnum_tostr(cf->file_rcs->rf_head, revbuf, sizeof(revbuf));

	added = stop = 0;
	switch (cf->file_status) {
	case FILE_ADDED:
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "%s has already been entered",
			    cf->file_path);
		stop = 1;
		break;
	case FILE_REMOVED:
		if (cf->file_rcs == NULL) {
			cvs_log(LP_NOTICE, "cannot resurrect %s; "
			    "RCS file removed by second party", cf->file_name);
		} else {
			/*
			 * Remove the '-' prefixing the version number and
			 * restore the file.
			 */
			rcsnum_tostr(cf->file_ent->ce_rev, revbuf,
			    sizeof(revbuf));

			ctime_r(&cf->file_ent->ce_mtime, tbuf);
			if (tbuf[strlen(tbuf) - 1] == '\n')
				tbuf[strlen(tbuf) - 1] = '\0';

			entry = xmalloc(CVS_ENT_MAXLINELEN);
			l = snprintf(entry, CVS_ENT_MAXLINELEN,
			    "/%s/%s/%s//", cf->file_name, revbuf, tbuf);
			if (l == -1 || l >= CVS_ENT_MAXLINELEN)
				fatal("cvs_add_local: overflow");

			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, entry);
			cvs_ent_close(entlist, ENT_SYNC);

			xfree(entry);

			b = rcs_getrev(cf->file_rcs, cf->file_rcs->rf_head);
			if (b == NULL)
				fatal("cvs_add_local: failed to get HEAD");

			cvs_checkout_file(cf, cf->file_rcs->rf_head, b, 0);
			cvs_printf("U %s\n", cf->file_path);

			cvs_log(LP_NOTICE, "%s, version %s, resurrected",
			    cf->file_name, revbuf);

			cf->file_status = FILE_UPTODATE;
		}
		stop = 1;
		break;
	case FILE_UPTODATE:
		if (cf->file_rcs != NULL && cf->file_rcs->rf_dead == 0) {
			cvs_log(LP_NOTICE, "%s already exists, with version "
			     "number %s", cf->file_path, revbuf);
			stop = 1;
		}
		break;
	case FILE_UNKNOWN:
		if (cf->file_rcs != NULL && cf->file_rcs->rf_dead == 1) {
			cvs_log(LP_NOTICE, "re-adding file %s "
			    "(instead of dead revision %s)",
			    cf->file_path, revbuf);
		} else {
			added++;
			cvs_log(LP_NOTICE, "scheduling file '%s' for addition",
			    cf->file_path);
		}
		break;
	default:
		break;
	}

	if (stop == 1)
		return;

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	l = snprintf(entry, CVS_ENT_MAXLINELEN, "/%s/0/Initial %s//",
	    cf->file_name, cf->file_name);

	entlist = cvs_ent_open(cf->file_wd);
	cvs_ent_add(entlist, entry);
	cvs_ent_close(entlist, ENT_SYNC);

	xfree(entry);

	if (added != 0) {
		if (verbosity > 0)
			cvs_log(LP_NOTICE, "use '%s commit' to add %s "
			    "permanently", __progname,
			    (added == 1) ? "this file" : "these files");
	}
}

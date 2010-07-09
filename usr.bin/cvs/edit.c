/*	$OpenBSD: edit.c,v 1.49 2010/07/09 18:42:14 zinovik Exp $	*/
/*
 * Copyright (c) 2006, 2007 Xavier Santolaria <xsa@openbsd.org>
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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define E_COMMIT	0x01
#define E_EDIT		0x02
#define E_UNEDIT	0x04
#define E_ALL		(E_EDIT|E_COMMIT|E_UNEDIT)

#define BASE_ADD	0x01
#define BASE_GET	0x02
#define BASE_REMOVE	0x04

static void	cvs_edit_local(struct cvs_file *);
static void	cvs_editors_local(struct cvs_file *);
static void	cvs_unedit_local(struct cvs_file *);

static RCSNUM	*cvs_base_handle(struct cvs_file *, int);

static int	edit_aflags = 0;

struct cvs_cmd cvs_cmd_edit = {
	CVS_OP_EDIT, CVS_USE_WDIR, "edit",
	{ { 0 }, { 0 } },
	"Get ready to edit a watched file",
	"[-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	cvs_edit
};

struct cvs_cmd cvs_cmd_editors = {
	CVS_OP_EDITORS, CVS_USE_WDIR, "editors",
	{ { 0 }, { 0 } },
	"See who is editing a watched file",
	"[-lR] [file ...]",
	"lR",
	NULL,
	cvs_editors
};

struct cvs_cmd cvs_cmd_unedit = {
	CVS_OP_UNEDIT, CVS_USE_WDIR, "unedit",
	{ { 0 }, { 0 } },
	"Undo an edit command",
	"[-lR] [file ...]",
	"lR",
	NULL,
	cvs_unedit
};

int
cvs_edit(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_edit.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			if (strcmp(optarg, "edit") == 0)
				edit_aflags |= E_EDIT;
			else if (strcmp(optarg, "unedit") == 0)
				edit_aflags |= E_UNEDIT;
			else if (strcmp(optarg, "commit") == 0)
				edit_aflags |= E_COMMIT;
			else if (strcmp(optarg, "all") == 0)
				edit_aflags |= E_ALL;
			else if (strcmp(optarg, "none") == 0)
				edit_aflags &= ~E_ALL;
			else
				fatal("%s", cvs_cmd_edit.cmd_synopsis);
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		default:
			fatal("%s", cvs_cmd_edit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_edit.cmd_synopsis);

	if (edit_aflags == 0)
		edit_aflags |= E_ALL;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_edit_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("edit");
		cvs_client_get_responses();
	}

	return (0);
}

int
cvs_editors(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_editors.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		default:
			fatal("%s", cvs_cmd_editors.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_editors.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_editors_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("editors");
		cvs_client_get_responses();
	}

	return (0);
}

int
cvs_unedit(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_unedit.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		default:
			fatal("%s", cvs_cmd_unedit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_unedit.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_unedit_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("unedit");
		cvs_client_get_responses();
	}

	return (0);
}

static void
cvs_edit_local(struct cvs_file *cf)
{
	FILE *fp;
	struct tm t;
	time_t now;
	char timebuf[CVS_TIME_BUFSZ], thishost[MAXHOSTNAMELEN];
	char bfpath[MAXPATHLEN], wdir[MAXPATHLEN];

	if (cvs_noexec == 1)
		return;

	cvs_log(LP_TRACE, "cvs_edit_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if ((fp = fopen(CVS_PATH_NOTIFY, "a")) == NULL)
		fatal("cvs_edit_local: fopen: `%s': %s",
		    CVS_PATH_NOTIFY, strerror(errno));

	(void)time(&now);
	gmtime_r(&now, &t);
	asctime_r(&t, timebuf);
	timebuf[strcspn(timebuf, "\n")] = '\0';

	if (gethostname(thishost, sizeof(thishost)) == -1)
		fatal("gethostname failed");

	if (getcwd(wdir, sizeof(wdir)) == NULL)
		fatal("getcwd failed");

	(void)fprintf(fp, "E%s\t%s GMT\t%s\t%s\t",
	    cf->file_name, timebuf, thishost, wdir);

	if (edit_aflags & E_EDIT)
		(void)fprintf(fp, "E");
	if (edit_aflags & E_UNEDIT)
		(void)fprintf(fp, "U");
	if (edit_aflags & E_COMMIT)
		(void)fprintf(fp, "C");

	(void)fprintf(fp, "\n");

	(void)fclose(fp);

	if (fchmod(cf->fd, 0644) == -1)
		fatal("cvs_edit_local: fchmod %s", strerror(errno));

	(void)xsnprintf(bfpath, MAXPATHLEN, "%s/%s",
	    CVS_PATH_BASEDIR, cf->file_name);

	if (mkdir(CVS_PATH_BASEDIR, 0755) == -1 && errno != EEXIST)
		fatal("cvs_edit_local: `%s': %s", CVS_PATH_BASEDIR,
		    strerror(errno));

	if (cvs_file_copy(cf->file_path, bfpath) == -1)
		fatal("cvs_edit_local: cvs_file_copy failed");

	(void)cvs_base_handle(cf, BASE_ADD);
}

static void
cvs_editors_local(struct cvs_file *cf)
{
}

static void
cvs_unedit_local(struct cvs_file *cf)
{
	FILE *fp;
	struct stat st;
	struct tm t;
	time_t now;
	char bfpath[MAXPATHLEN], timebuf[64], thishost[MAXHOSTNAMELEN];
	char wdir[MAXPATHLEN], sticky[CVS_ENT_MAXLINELEN];
	RCSNUM *ba_rev;

	cvs_log(LP_TRACE, "cvs_unedit_local(%s)", cf->file_path);

	if (cvs_noexec == 1)
		return;

	cvs_file_classify(cf, cvs_directory_tag);

	(void)xsnprintf(bfpath, MAXPATHLEN, "%s/%s",
	    CVS_PATH_BASEDIR, cf->file_name);

	if (stat(bfpath, &st) == -1)
		return;

	if (cvs_file_cmp(cf->file_path, bfpath) != 0) {
		cvs_printf("%s has been modified; revert changes? ",
		    cf->file_name);

		if (cvs_yesno() == -1)
			return;
	}

	cvs_rename(bfpath, cf->file_path);

	if ((fp = fopen(CVS_PATH_NOTIFY, "a")) == NULL)
		fatal("cvs_unedit_local: fopen: `%s': %s",
		    CVS_PATH_NOTIFY, strerror(errno));

	(void)time(&now);
	gmtime_r(&now, &t);
	asctime_r(&t, timebuf);
	timebuf[strcspn(timebuf, "\n")] = '\0';

	if (gethostname(thishost, sizeof(thishost)) == -1)
		fatal("gethostname failed");

	if (getcwd(wdir, sizeof(wdir)) == NULL)
		fatal("getcwd failed");

	(void)fprintf(fp, "U%s\t%s GMT\t%s\t%s\t\n",
	    cf->file_name, timebuf, thishost, wdir);

	(void)fclose(fp);

	if ((ba_rev = cvs_base_handle(cf, BASE_GET)) == NULL) {
		cvs_log(LP_ERR, "%s not mentioned in %s",
		    cf->file_name, CVS_PATH_BASEREV);
		return;
	}

	if (cf->file_ent != NULL) {
		CVSENTRIES *entlist;
		struct cvs_ent *ent;
		char *entry, rbuf[CVS_REV_BUFSZ];

		entlist = cvs_ent_open(cf->file_wd);

		if ((ent = cvs_ent_get(entlist, cf->file_name)) == NULL)
			fatal("cvs_unedit_local: cvs_ent_get failed");

		(void)rcsnum_tostr(ba_rev, rbuf, sizeof(rbuf));

		memset(timebuf, 0, sizeof(timebuf));
		ctime_r(&cf->file_ent->ce_mtime, timebuf);
		timebuf[strcspn(timebuf, "\n")] = '\0';

		sticky[0] = '\0';
		if (cf->file_ent->ce_tag != NULL)
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    cf->file_ent->ce_tag);

		(void)xasprintf(&entry, "/%s/%s/%s/%s/%s",
		    cf->file_name, rbuf, timebuf, cf->file_ent->ce_opts ? 
		    cf->file_ent->ce_opts : "", sticky);

		cvs_ent_add(entlist, entry);

		cvs_ent_free(ent);

		xfree(entry);
	}

	rcsnum_free(ba_rev);

	(void)cvs_base_handle(cf, BASE_REMOVE);

	if (fchmod(cf->fd, 0644) == -1)
		fatal("cvs_unedit_local: fchmod %s", strerror(errno));
}

static RCSNUM *
cvs_base_handle(struct cvs_file *cf, int flags)
{
	FILE *fp, *tfp;
	RCSNUM *ba_rev;
	int i;
	char *dp, *sp;
	char buf[MAXPATHLEN], *fields[2], rbuf[CVS_REV_BUFSZ];

	cvs_log(LP_TRACE, "cvs_base_handle(%s)", cf->file_path);

	tfp = NULL;
	ba_rev = NULL;

	if (((fp = fopen(CVS_PATH_BASEREV, "r")) == NULL) && errno != ENOENT) {
		cvs_log(LP_ERRNO, "%s", CVS_PATH_BASEREV);
		goto out;
	}

	if (flags & (BASE_ADD|BASE_REMOVE)) {
		if ((tfp = fopen(CVS_PATH_BASEREVTMP, "w")) == NULL) {
			cvs_log(LP_ERRNO, "%s", CVS_PATH_BASEREVTMP);
			goto out;
		}
	}

	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\n")] = '\0';

			if (buf[0] != 'B')
				continue;

			sp = buf + 1;
			i = 0;
			do {
				if ((dp = strchr(sp, '/')) != NULL)
					*(dp++) = '\0';
				fields[i++] = sp;
				sp = dp;
			} while (dp != NULL && i < 2);

			if (cvs_file_cmpname(fields[0], cf->file_path) == 0) {
				if (flags & BASE_GET) {
					ba_rev = rcsnum_parse(fields[1]);
					if (ba_rev == NULL)
						fatal("cvs_base_handle: "
						    "rcsnum_parse");
					goto got_rev;
				}
			} else {
				if (flags & (BASE_ADD|BASE_REMOVE))
					(void)fprintf(tfp, "%s\n", buf);
			}
		}
	}

got_rev:
	if (flags & (BASE_ADD)) {
		(void)rcsnum_tostr(cf->file_ent->ce_rev, rbuf, sizeof(rbuf));
		(void)fprintf(tfp, "B%s/%s/\n", cf->file_path, rbuf);
	}

out:
	if (fp != NULL)
		(void)fclose(fp);

	if (tfp != NULL) {
		(void)fclose(tfp);
		(void)cvs_rename(CVS_PATH_BASEREVTMP, CVS_PATH_BASEREV);
	}

	return (ba_rev);
}

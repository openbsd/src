/*	$OpenBSD: add.c,v 1.109 2010/07/23 21:46:04 ray Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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
#include "remote.h"

extern char *__progname;

void	cvs_add_loginfo(char *);
void	cvs_add_entry(struct cvs_file *);
void	cvs_add_remote(struct cvs_file *);

static void add_directory(struct cvs_file *);
static void add_file(struct cvs_file *);
static void add_entry(struct cvs_file *);

int		kflag = 0;
static u_int	added_files = 0;
static char	kbuf[8];

extern char	*logmsg;
extern char	*loginfo;

struct cvs_cmd cvs_cmd_add = {
	CVS_OP_ADD, CVS_USE_WDIR, "add",
	{ "ad", "new" },
	"Add a new file or directory to the repository",
	"[-k mode] [-m message] ...",
	"k:m:",
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
		case 'k':
			kflag = rcs_kflag_get(optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				fatal("%s", cvs_cmd_add.cmd_synopsis);
			}
			(void)xsnprintf(kbuf, sizeof(kbuf), "-k%s", optarg);
			break;
		case 'm':
			logmsg = optarg;
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

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_add_remote;
		flags = 0;

		if (kflag)
			cvs_client_send_request("Argument %s", kbuf);

		if (logmsg != NULL)
			cvs_client_send_logmsg(logmsg);
	} else {
		if (logmsg != NULL && cvs_logmsg_verify(logmsg))
			return (0);

		cr.fileproc = cvs_add_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (added_files != 0) {
		cvs_log(LP_NOTICE, "use '%s commit' to add %s "
		    "permanently", __progname,
		    (added_files == 1) ? "this file" : "these files");
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_senddir(".");
		cvs_client_send_files(argv, argc);
		cvs_client_send_request("add");
		cvs_client_get_responses();

		if (server_response == SERVER_OK) {
			cr.fileproc = cvs_add_entry;
			cvs_file_run(argc, argv, &cr);
		}
	}

	return (0);
}

void
cvs_add_entry(struct cvs_file *cf)
{
	char *entry;
	CVSENTRIES *entlist;

	if (cf->file_type == CVS_DIR) {
		entry = xmalloc(CVS_ENT_MAXLINELEN);
		cvs_ent_line_str(cf->file_name, NULL, NULL, NULL, NULL, 1, 0,
		    entry, CVS_ENT_MAXLINELEN);

		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_add(entlist, entry);

		xfree(entry);
	} else {
		add_entry(cf);
	}
}

void
cvs_add_local(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_add_local(%s)", cf->file_path);

	if (cvs_cmdop != CVS_OP_CHECKOUT && cvs_cmdop != CVS_OP_UPDATE)
		cvs_file_classify(cf, cvs_directory_tag);

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

void
cvs_add_remote(struct cvs_file *cf)
{
	char path[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_add_remote(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_type == CVS_DIR) {
		cvs_get_repository_path(cf->file_wd, path, MAXPATHLEN);
		if (strlcat(path, "/", sizeof(path)) >= sizeof(path))
			fatal("cvs_add_remote: truncation");
		if (strlcat(path, cf->file_path, sizeof(path)) >= sizeof(path))
			fatal("cvs_add_remote: truncation");
		cvs_client_send_request("Directory %s\n%s", cf->file_path,
		    path);

		add_directory(cf);
	} else {
		cvs_client_sendfile(cf);
	}
}

void
cvs_add_loginfo(char *repo)
{
	BUF *buf;
	char pwd[MAXPATHLEN];

	if (getcwd(pwd, sizeof(pwd)) == NULL)
		fatal("Can't get working directory");

	buf = buf_alloc(1024);

	cvs_trigger_loginfo_header(buf, repo);

	buf_puts(buf, "Log Message:\nDirectory ");
	buf_puts(buf, current_cvsroot->cr_dir);
	buf_putc(buf, '/');
	buf_puts(buf, repo);
	buf_puts(buf, " added to the repository\n");

	buf_putc(buf, '\0');

	loginfo = buf_release(buf);
}

void
cvs_add_tobranch(struct cvs_file *cf, char *tag)
{
	BUF *bp;
	char attic[MAXPATHLEN], repo[MAXPATHLEN];
	char *msg;
	struct stat st;
	struct rcs_delta *rdp;
	RCSNUM *branch;

	cvs_log(LP_TRACE, "cvs_add_tobranch(%s)", cf->file_name);

	if (cvs_noexec == 1)
		return;

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_add_tobranch: %s", strerror(errno));

	cvs_get_repository_path(cf->file_wd, repo, MAXPATHLEN);
	(void)xsnprintf(attic, MAXPATHLEN, "%s/%s",
	    repo, CVS_PATH_ATTIC);

	if (mkdir(attic, 0755) == -1 && errno != EEXIST)
		fatal("cvs_add_tobranch: failed to create Attic");

	(void)xsnprintf(attic, MAXPATHLEN, "%s/%s/%s%s", repo,
	    CVS_PATH_ATTIC, cf->file_name, RCS_FILE_EXT);

	xfree(cf->file_rpath);
	cf->file_rpath = xstrdup(attic);

	cf->repo_fd = open(cf->file_rpath, O_CREAT|O_RDONLY);
	if (cf->repo_fd < 0)
		fatal("cvs_add_tobranch: %s: %s", cf->file_rpath,
		    strerror(errno));

	cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd,
	    RCS_CREATE|RCS_WRITE, 0444);
	if (cf->file_rcs == NULL)
		fatal("cvs_add_tobranch: failed to create RCS file for %s",
		    cf->file_path);

	if ((branch = rcsnum_parse("1.1.2")) == NULL)
		fatal("cvs_add_tobranch: failed to parse branch");

	if (rcs_sym_add(cf->file_rcs, tag, branch) == -1)
		fatal("cvs_add_tobranch: failed to add vendor tag");

	(void)xasprintf(&msg, "file %s was initially added on branch %s.",
	    cf->file_name, tag);
	if (rcs_rev_add(cf->file_rcs, RCS_HEAD_REV, msg, -1, NULL) == -1)
		fatal("cvs_add_tobranch: failed to create first branch "
		    "revision");
	xfree(msg);

	if ((rdp = rcs_findrev(cf->file_rcs, cf->file_rcs->rf_head)) == NULL)
		fatal("cvs_add_tobranch: cannot find newly added revision");

	bp = buf_alloc(1);

	if (rcs_deltatext_set(cf->file_rcs,
	    cf->file_rcs->rf_head, bp) == -1)
		fatal("cvs_add_tobranch: failed to set deltatext");

	rcs_comment_set(cf->file_rcs, " * ");

	if (rcs_state_set(cf->file_rcs, cf->file_rcs->rf_head, RCS_STATE_DEAD)
	    == -1)
		fatal("cvs_add_tobranch: failed to set state");
}

static void
add_directory(struct cvs_file *cf)
{
	int added, nb;
	struct stat st;
	CVSENTRIES *entlist;
	char *date, entry[MAXPATHLEN], msg[1024], repo[MAXPATHLEN], *tag, *p;
	struct file_info_list files_info;
	struct file_info *fi;
	struct trigger_list *line_list;

	cvs_log(LP_TRACE, "add_directory(%s)", cf->file_path);

	(void)xsnprintf(entry, MAXPATHLEN, "%s%s",
	    cf->file_rpath, RCS_FILE_EXT);

	added = 1;
	if (stat(entry, &st) != -1) {
		cvs_log(LP_NOTICE, "cannot add directory %s: "
		    "a file with that name already exists",
		    cf->file_path);
		added = 0;
	} else {
		/* Let's see if we have any per-directory tags first. */
		cvs_parse_tagfile(cf->file_wd, &tag, &date, &nb);

		(void)xsnprintf(entry, MAXPATHLEN, "%s/%s",
		    cf->file_path, CVS_PATH_CVSDIR);

		if (cvs_server_active) {
			if (mkdir(cf->file_rpath, 0755) == -1 &&
			    errno != EEXIST)
				fatal("add_directory: %s: %s", cf->file_rpath,
				    strerror(errno));
		} else if (stat(entry, &st) != -1) {
			if (!S_ISDIR(st.st_mode)) {
				cvs_log(LP_ERR, "%s exists but is not "
				    "directory", entry);
			} else {
				cvs_log(LP_NOTICE, "%s already exists",
				    entry);
			}
			added = 0;
		} else if (cvs_noexec != 1) {
			if (mkdir(cf->file_rpath, 0755) == -1 &&
			    errno != EEXIST)
				fatal("add_directory: %s: %s", cf->file_rpath,
				    strerror(errno));

			cvs_get_repository_name(cf->file_wd, repo,
			    MAXPATHLEN);

			(void)xsnprintf(entry, MAXPATHLEN, "%s/%s",
			    repo, cf->file_name);

			cvs_mkadmin(cf->file_path, current_cvsroot->cr_dir,
			    entry, tag, date);

			p = xmalloc(CVS_ENT_MAXLINELEN);
			cvs_ent_line_str(cf->file_name, NULL, NULL, NULL,
			    NULL, 1, 0, p, CVS_ENT_MAXLINELEN);

			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, p);
			xfree(p);
		}
	}

	if (added == 1 && current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		(void)xsnprintf(msg, sizeof(msg),
		    "Directory %s added to the repository", cf->file_rpath);

		if (tag != NULL) {
			(void)strlcat(msg,
			    "\n--> Using per-directory sticky tag ",
			    sizeof(msg));
			(void)strlcat(msg, tag, sizeof(msg));
		}
		if (date != NULL) {
			(void)strlcat(msg,
			    "\n--> Using per-directory sticky date ",
			    sizeof(msg));
			(void)strlcat(msg, date, sizeof(msg));
		}
		cvs_printf("%s\n", msg);

		if (tag != NULL)
			xfree(tag);
		if (date != NULL)
			xfree(date);

		cvs_get_repository_name(cf->file_path, repo, MAXPATHLEN);
		line_list = cvs_trigger_getlines(CVS_PATH_LOGINFO, repo);
		if (line_list != NULL) {
			TAILQ_INIT(&files_info);
			fi = xcalloc(1, sizeof(*fi));
			fi->file_path = xstrdup(cf->file_path);
			TAILQ_INSERT_TAIL(&files_info, fi, flist);

			cvs_add_loginfo(repo);
			cvs_trigger_handle(CVS_TRIGGER_LOGINFO, repo,
			    loginfo, line_list, &files_info);

			cvs_trigger_freeinfo(&files_info);
			cvs_trigger_freelist(line_list);
			if (loginfo != NULL)
				xfree(loginfo);
		}
	}

	cf->file_status = FILE_SKIP;
}

static void
add_file(struct cvs_file *cf)
{
	int nb, stop;
	char revbuf[CVS_REV_BUFSZ];
	RCSNUM *head = NULL;
	char *tag;

	cvs_parse_tagfile(cf->file_wd, &tag, NULL, &nb);
	if (nb) {
		cvs_log(LP_ERR, "cannot add file on non-branch tag %s", tag);
		return;
	}

	if (cf->file_rcs != NULL) {
		head = rcs_head_get(cf->file_rcs);
		if (head == NULL) {
			cvs_log(LP_NOTICE, "no head revision in RCS file for "
			    "%s", cf->file_path);
		}
		rcsnum_tostr(head, revbuf, sizeof(revbuf));
	}

	stop = 0;
	switch (cf->file_status) {
	case FILE_ADDED:
	case FILE_CHECKOUT:
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "%s has already been entered",
			    cf->file_path);
		stop = 1;
		break;
	case FILE_REMOVED:
		if (cf->file_rcs == NULL) {
			cvs_log(LP_NOTICE, "cannot resurrect %s; "
			    "RCS file removed by second party", cf->file_name);
		} else if (!(cf->file_flags & FILE_ON_DISK)) {
			add_entry(cf);

			/* Restore the file. */
			cvs_checkout_file(cf, head, NULL, 0);

			cvs_printf("U %s\n", cf->file_path);

			cvs_log(LP_NOTICE, "%s, version %s, resurrected",
			    cf->file_name, revbuf);

			cf->file_status = FILE_UPTODATE;
		}
		stop = 1;
		break;
	case FILE_CONFLICT:
	case FILE_LOST:
	case FILE_MODIFIED:
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
			added_files++;
		} else if (cf->file_flags & FILE_ON_DISK) {
			cvs_log(LP_NOTICE, "scheduling file '%s' for addition",
			    cf->file_path);
			added_files++;
		} else {
			stop = 1;
		}
		break;
	default:
		break;
	}

	if (head != NULL)
		rcsnum_free(head);

	if (stop == 1)
		return;

	add_entry(cf);
}

static void
add_entry(struct cvs_file *cf)
{
	FILE *fp;
	char *entry, path[MAXPATHLEN];
	char revbuf[CVS_REV_BUFSZ], tbuf[CVS_TIME_BUFSZ];
	char sticky[CVS_ENT_MAXLINELEN];
	CVSENTRIES *entlist;

	if (cvs_noexec == 1)
		return;

	sticky[0] = '\0';
	entry = xmalloc(CVS_ENT_MAXLINELEN);

	if (cf->file_status == FILE_REMOVED) {
		rcsnum_tostr(cf->file_ent->ce_rev, revbuf, sizeof(revbuf));

		ctime_r(&cf->file_ent->ce_mtime, tbuf);
		tbuf[strcspn(tbuf, "\n")] = '\0';

		if (cf->file_ent->ce_tag != NULL)
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    cf->file_ent->ce_tag);

		/* Remove the '-' prefixing the version number. */
		cvs_ent_line_str(cf->file_name, revbuf, tbuf,
		    cf->file_ent->ce_opts ? cf->file_ent->ce_opts : "", sticky,
		    0, 0, entry, CVS_ENT_MAXLINELEN);
	} else {
		if (logmsg != NULL) {
			(void)xsnprintf(path, MAXPATHLEN, "%s/%s/%s%s",
			    cf->file_wd, CVS_PATH_CVSDIR, cf->file_name,
			    CVS_DESCR_FILE_EXT);

			if ((fp = fopen(path, "w+")) == NULL)
				fatal("add_entry: fopen `%s': %s",
				    path, strerror(errno));

			if (fputs(logmsg, fp) == EOF) {
				(void)unlink(path);
				fatal("add_entry: fputs `%s': %s",
				    path, strerror(errno));
			}
			(void)fclose(fp);
		}

		if (cvs_directory_tag != NULL)
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    cvs_directory_tag);

		tbuf[0] = '\0';
		if (!cvs_server_active)
			(void)xsnprintf(tbuf, sizeof(tbuf), "Initial %s",
			    cf->file_name);

		cvs_ent_line_str(cf->file_name, "0", tbuf, kflag ? kbuf : "",
		    sticky, 0, 0, entry, CVS_ENT_MAXLINELEN);
	}

	if (cvs_server_active) {
		cvs_server_send_response("Checked-in %s/", cf->file_wd);
		cvs_server_send_response("%s", cf->file_path);
		cvs_server_send_response("%s", entry);
	} else {
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_add(entlist, entry);
	}
	xfree(entry);
}

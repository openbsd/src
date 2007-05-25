/*	$OpenBSD: client.c,v 1.62 2007/05/25 22:36:47 ray Exp $	*/
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

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

struct cvs_req cvs_requests[] = {
	/* this is what our client will use, the server should support it */
	{ "Root",		1,	cvs_server_root, REQ_NEEDED },
	{ "Valid-responses",	1,	cvs_server_validresp, REQ_NEEDED },
	{ "valid-requests",	1,	cvs_server_validreq, REQ_NEEDED },
	{ "Directory",		0,	cvs_server_directory, REQ_NEEDED },
	{ "Static-directory",	0,	cvs_server_static_directory, REQ_NEEDED },
	{ "Sticky",		0,	cvs_server_sticky, REQ_NEEDED },
	{ "Entry",		0,	cvs_server_entry, REQ_NEEDED },
	{ "Modified",		0,	cvs_server_modified, REQ_NEEDED },
	{ "UseUnchanged",	0,	cvs_server_useunchanged, REQ_NEEDED },
	{ "Unchanged",		0,	cvs_server_unchanged, REQ_NEEDED },
	{ "Questionable",	0,	cvs_server_questionable, REQ_NEEDED },
	{ "Argument",		0,	cvs_server_argument, REQ_NEEDED },
	{ "Argumentx",		0,	cvs_server_argumentx, REQ_NEEDED },
	{ "Global_option",	0,	cvs_server_globalopt, REQ_NEEDED },
	{ "Set",		0,	cvs_server_set, REQ_NEEDED },

	/*
	 * used to tell the server what is going on in our
	 * working copy, unsupported until we are told otherwise
	 */
	{ "Max-dotdot",			0,	NULL, 0 },
	{ "Checkin-prog",		0,	NULL, 0 },
	{ "Update-prog",		0,	NULL, 0 },
	{ "Kopt",			0,	NULL, 0 },
	{ "Checkin-time",		0,	NULL, 0 },
	{ "Is-modified",		0,	NULL, 0 },
	{ "Notify",			0,	NULL, 0 },
	{ "Case",			0,	NULL, 0 },
	{ "Gzip-stream",		0,	NULL, 0 },
	{ "wrapper-sendme-rcsOptions",	0,	NULL, 0 },
	{ "Kerberos-encrypt",		0,	NULL, 0 },
	{ "Gssapi-encrypt",		0,	NULL, 0 },
	{ "Gssapi-authenticate",	0,	NULL, 0 },
	{ "expand-modules",		0,	NULL, 0 },

	/* commands that might be supported */
	{ "ci",				0,	cvs_server_commit, 0 },
	{ "co",				0,	cvs_server_checkout, 0 },
	{ "update",			0,	cvs_server_update, 0 },
	{ "diff",			0,	cvs_server_diff, 0 },
	{ "log",			0,	cvs_server_log, 0 },
	{ "rlog",			0,	NULL, 0 },
	{ "add",			0,	cvs_server_add, 0 },
	{ "remove",			0,	cvs_server_remove, 0 },
	{ "update-patches",		0,	cvs_server_update_patches, 0 },
	{ "gzip-file-contents",		0,	NULL, 0 },
	{ "status",			0,	cvs_server_status, 0 },
	{ "rdiff",			0,	NULL, 0 },
	{ "tag",			0,	cvs_server_tag, 0 },
	{ "rtag",			0,	NULL, 0 },
	{ "import",			0,	cvs_server_import, 0 },
	{ "admin",			0,	cvs_server_admin, 0 },
	{ "export",			0,	NULL, 0 },
	{ "history",			0,	NULL, 0 },
	{ "release",			0,	NULL, 0 },
	{ "watch-on",			0,	NULL, 0 },
	{ "watch-off",			0,	NULL, 0 },
	{ "watch-add",			0,	NULL, 0 },
	{ "watch-remove",		0,	NULL, 0 },
	{ "watchers",			0,	NULL, 0 },
	{ "editors",			0,	NULL, 0 },
	{ "init",			0,	cvs_server_init, 0 },
	{ "annotate",			0,	cvs_server_annotate, 0 },
	{ "rannotate",			0,	NULL, 0 },
	{ "noop",			0,	NULL, 0 },
	{ "version",			0,	cvs_server_version, 0 },
	{ "",				-1,	NULL, 0 }
};

static void	 client_check_directory(char *);
static char	*client_get_supported_responses(void);
static char	*lastdir = NULL;
static int	 end_of_response = 0;

static void	cvs_client_initlog(void);

/*
 * File descriptors for protocol logging when the CVS_CLIENT_LOG environment
 * variable is set.
 */
static int	cvs_client_logon = 0;
int	cvs_client_inlog_fd = -1;
int	cvs_client_outlog_fd = -1;


int server_response = SERVER_OK;

static char *
client_get_supported_responses(void)
{
	BUF *bp;
	char *d;
	int i, first;

	first = 0;
	bp = cvs_buf_alloc(512, BUF_AUTOEXT);
	for (i = 0; cvs_responses[i].supported != -1; i++) {
		if (cvs_responses[i].hdlr == NULL)
			continue;

		if (first != 0)
			cvs_buf_append(bp, " ", 1);
		else
			first++;
		cvs_buf_append(bp, cvs_responses[i].name,
		    strlen(cvs_responses[i].name));
	}

	cvs_buf_putc(bp, '\0');
	d = cvs_buf_release(bp);
	return (d);
}

static void
client_check_directory(char *data)
{
	CVSENTRIES *entlist;
	char entry[CVS_ENT_MAXLINELEN], *parent, *base;

	STRIP_SLASH(data);

	cvs_mkpath(data);

	if ((base = basename(data)) == NULL)
		fatal("client_check_directory: overflow");

	if ((parent = dirname(data)) == NULL)
		fatal("client_check_directory: overflow");

	if (!strcmp(parent, "."))
		return;

	(void)xsnprintf(entry, CVS_ENT_MAXLINELEN, "D/%s////", base);

	entlist = cvs_ent_open(parent);
	cvs_ent_add(entlist, entry);
	cvs_ent_close(entlist, ENT_SYNC);
}

void
cvs_client_connect_to_server(void)
{
	struct cvs_var *vp;
	char *cmd, *argv[9], *resp;
	int ifd[2], ofd[2], argc;

	if (cvs_server_active == 1)
		fatal("cvs_client_connect: I was already connected to server");

	switch (current_cvsroot->cr_method) {
	case CVS_METHOD_PSERVER:
	case CVS_METHOD_KSERVER:
	case CVS_METHOD_GSERVER:
	case CVS_METHOD_FORK:
	case CVS_METHOD_EXT:
		fatal("the specified connection method is not supported");
	default:
		break;
	}

	if (pipe(ifd) == -1)
		fatal("cvs_client_connect: %s", strerror(errno));
	if (pipe(ofd) == -1)
		fatal("cvs_client_connect: %s", strerror(errno));

	switch (fork()) {
	case -1:
		fatal("cvs_client_connect: fork failed: %s", strerror(errno));
	case 0:
		if (dup2(ifd[0], STDIN_FILENO) == -1)
			fatal("cvs_client_connect: %s", strerror(errno));
		if (dup2(ofd[1], STDOUT_FILENO) == -1)
			fatal("cvs_client_connect: %s", strerror(errno));

		close(ifd[1]);
		close(ofd[0]);

		if ((cmd = getenv("CVS_SERVER")) == NULL)
			cmd = CVS_SERVER_DEFAULT;

		argc = 0;
		argv[argc++] = cvs_rsh;

		if (current_cvsroot->cr_user != NULL) {
			argv[argc++] = "-l";
			argv[argc++] = current_cvsroot->cr_user;
		}

		argv[argc++] = current_cvsroot->cr_host;
		argv[argc++] = cmd;
		argv[argc++] = "server";
		argv[argc] = NULL;

		cvs_log(LP_TRACE, "connecting to server %s",
		    current_cvsroot->cr_host);

		execvp(argv[0], argv);
		fatal("cvs_client_connect: failed to execute cvs server");
	default:
		break;
	}

	close(ifd[0]);
	close(ofd[1]);

	if ((current_cvsroot->cr_srvin = fdopen(ifd[1], "w")) == NULL)
		fatal("cvs_client_connect: %s", strerror(errno));
	if ((current_cvsroot->cr_srvout = fdopen(ofd[0], "r")) == NULL)
		fatal("cvs_client_connect: %s", strerror(errno));

	setvbuf(current_cvsroot->cr_srvin, NULL,_IOLBF, 0);
	setvbuf(current_cvsroot->cr_srvout, NULL, _IOLBF, 0);

	cvs_client_initlog();

	if (cvs_cmdop != CVS_OP_INIT)
		cvs_client_send_request("Root %s", current_cvsroot->cr_dir);

	resp = client_get_supported_responses();
	cvs_client_send_request("Valid-responses %s", resp);
	xfree(resp);

	cvs_client_send_request("valid-requests");
	cvs_client_get_responses();

	cvs_client_send_request("UseUnchanged");

	if (cvs_nolog == 1)
		cvs_client_send_request("Global_option -l");

	if (cvs_noexec == 1)
		cvs_client_send_request("Global_option -n");

	if (verbosity == 0)
		cvs_client_send_request("Global_option -Q");

	/* Be quiet. This is the default in OpenCVS. */
	cvs_client_send_request("Global_option -q");

	if (cvs_readonly == 1)
		cvs_client_send_request("Global_option -r");

	if (cvs_trace == 1)
		cvs_client_send_request("Global_option -t");

	if (verbosity == 2)
		cvs_client_send_request("Global_option -V");

	/* XXX: If 'Set' is supported? */
	TAILQ_FOREACH(vp, &cvs_variables, cv_link)
		cvs_client_send_request("Set %s=%s", vp->cv_name, vp->cv_val);
}

void
cvs_client_send_request(char *fmt, ...)
{
	va_list ap;
	char *data, *s;
	struct cvs_req *req;

	va_start(ap, fmt);
	vasprintf(&data, fmt, ap);
	va_end(ap);

	if ((s = strchr(data, ' ')) != NULL)
		*s = '\0';

	req = cvs_remote_get_request_info(data);
	if (req == NULL)
		fatal("'%s' is an unknown request", data);

	if (req->supported != 1)
		fatal("remote cvs server does not support '%s'", data);

	if (s != NULL)
		*s = ' ';

	cvs_log(LP_TRACE, "%s", data);

	if (cvs_client_inlog_fd != -1) {
		BUF *bp;

		bp = cvs_buf_alloc(strlen(data), BUF_AUTOEXT);

		if (cvs_buf_append(bp, data, strlen(data)) < 0)
			fatal("cvs_client_send_request: cvs_buf_append");

		cvs_buf_putc(bp, '\n');

		if (cvs_buf_write_fd(bp, cvs_client_inlog_fd) < 0)
			fatal("cvs_client_send_request: cvs_buf_write_fd");

		cvs_buf_free(bp);
	}

	cvs_remote_output(data);
	xfree(data);
}

void
cvs_client_read_response(void)
{
	char *cmd, *data;
	struct cvs_resp *resp;

	cmd = cvs_remote_input();
	if ((data = strchr(cmd, ' ')) != NULL)
		(*data++) = '\0';

	resp = cvs_remote_get_response_info(cmd);
	if (resp == NULL)
		fatal("response '%s' is not supported by our client", cmd);

	if (resp->hdlr == NULL)
		fatal("opencvs client does not support '%s'", cmd);

	(*resp->hdlr)(data);

	xfree(cmd);
}

void
cvs_client_get_responses(void)
{
	while (end_of_response != 1)
		cvs_client_read_response();

	end_of_response = 0;
}

void
cvs_client_senddir(const char *dir)
{
	struct stat st;
	int nb;
	char *d, *date, fpath[MAXPATHLEN], repo[MAXPATHLEN], *tag;

	d = NULL;

	if (lastdir != NULL && !strcmp(dir, lastdir))
		return;

	cvs_get_repository_path(dir, repo, MAXPATHLEN);

	cvs_client_send_request("Directory %s\n%s", dir, repo);

	(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
	    dir, CVS_PATH_STATICENTRIES);

	if (stat(fpath, &st) == 0 && (st.st_mode & (S_IRUSR|S_IRGRP|S_IROTH)))
		cvs_client_send_request("Static-directory");

	d = xstrdup(dir);
	cvs_parse_tagfile(d, &tag, &date, &nb);

	if (tag != NULL || date != NULL) {
		char buf[128];

		if (tag != NULL && nb != NULL) {
			if (strlcpy(buf, "N", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		} else if (tag != NULL) {
			if (strlcpy(buf, "T", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		} else {
			if (strlcpy(buf, "D", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		}

		if (strlcat(buf, tag ? tag : date, sizeof(buf)) >= sizeof(buf))
			fatal("cvs_client_senddir: truncation");

		cvs_client_send_request("Sticky %s", buf);

		if (tag != NULL)
			xfree(tag);
		if (date != NULL)
			xfree(date);
	}
	if (d != NULL)
		xfree(d);

	if (lastdir != NULL)
		xfree(lastdir);
	lastdir = xstrdup(dir);
}

void
cvs_client_sendfile(struct cvs_file *cf)
{
	size_t len;
	char rev[16], timebuf[64], sticky[32];

	if (cf->file_type != CVS_FILE)
		return;

	cvs_client_senddir(cf->file_wd);
	cvs_remote_classify_file(cf);

	if (cf->file_type == CVS_DIR)
		return;

	if (cf->file_ent != NULL) {
		if (cf->file_status == FILE_ADDED) {
			len = strlcpy(rev, "0", sizeof(rev));
			if (len >= sizeof(rev))
				fatal("cvs_client_sendfile: truncation");

			len = strlcpy(timebuf, "Initial ", sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");

			len = strlcat(timebuf, cf->file_name, sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
		} else {
			rcsnum_tostr(cf->file_ent->ce_rev, rev, sizeof(rev));
			ctime_r(&cf->file_ent->ce_mtime, timebuf);
		}

		if (cf->file_ent->ce_conflict == NULL) {
			if (timebuf[strlen(timebuf) - 1] == '\n')
				timebuf[strlen(timebuf) - 1] = '\0';
		} else {
			len = strlcpy(timebuf, cf->file_ent->ce_conflict,
			    sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
			len = strlcat(timebuf, "+", sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
		}

		sticky[0] = '\0';
		if (cf->file_ent->ce_tag != NULL) {
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    cf->file_ent->ce_tag);
		}

		cvs_client_send_request("Entry /%s/%s%s/%s/%s/%s",
		    cf->file_name, (cf->file_status == FILE_REMOVED) ? "-" : "",
		    rev, timebuf, cf->file_ent->ce_opts ?
		    cf->file_ent->ce_opts : "", sticky);
	}

	switch (cf->file_status) {
	case FILE_UNKNOWN:
		if (cf->fd != -1)
			cvs_client_send_request("Questionable %s",
			    cf->file_name);
		break;
	case FILE_ADDED:
	case FILE_MODIFIED:
		cvs_client_send_request("Modified %s", cf->file_name);
		cvs_remote_send_file(cf->file_path);
		break;
	case FILE_UPTODATE:
		cvs_client_send_request("Unchanged %s", cf->file_name);
		break;
	}
}

void
cvs_client_send_files(char **argv, int argc)
{
	int i;

	for (i = 0; i < argc; i++)
		cvs_client_send_request("Argument %s", argv[i]);
}

void
cvs_client_ok(char *data)
{
	end_of_response = 1;
	server_response = SERVER_OK;
}

void
cvs_client_error(char *data)
{
	end_of_response = 1;
	server_response = SERVER_ERROR;
}

void
cvs_client_validreq(char *data)
{
	int i;
	char *sp, *ep;
	struct cvs_req *req;

	if ((sp = data) == NULL)
		fatal("Missing argument for Valid-requests");

	do {
		if ((ep = strchr(sp, ' ')) != NULL)
			*ep = '\0';

		req = cvs_remote_get_request_info(sp);
		if (req != NULL)
			req->supported = 1;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	for (i = 0; cvs_requests[i].supported != -1; i++) {
		req = &cvs_requests[i];
		if ((req->flags & REQ_NEEDED) &&
		    req->supported != 1) {
			fatal("server does not support required '%s'",
			    req->name);
		}
	}
}

void
cvs_client_e(char *data)
{
	if (data == NULL)
		fatal("Missing argument for E");

	cvs_printf("%s\n", data);
}

void
cvs_client_m(char *data)
{
	if (data == NULL)
		fatal("Missing argument for M");

	cvs_printf("%s\n", data);
}

void
cvs_client_checkedin(char *data)
{
	CVSENTRIES *entlist;
	struct cvs_ent *ent, *newent;
	char *dir, *e, entry[CVS_ENT_MAXLINELEN], rev[16], timebuf[64];
	char sticky[16];

	if (data == NULL)
		fatal("Missing argument for Checked-in");

	dir = cvs_remote_input();
	e = cvs_remote_input();
	xfree(dir);

	entlist = cvs_ent_open(data);
	newent = cvs_ent_parse(e);
	ent = cvs_ent_get(entlist, newent->ce_name);
	xfree(e);

	rcsnum_tostr(newent->ce_rev, rev, sizeof(rev));
	ctime_r(&ent->ce_mtime, timebuf);
	if (timebuf[strlen(timebuf) - 1] == '\n')
		timebuf[strlen(timebuf) - 1] = '\0';

	sticky[0] = '\0';
	if (ent->ce_tag != NULL)
		(void)xsnprintf(sticky, sizeof(sticky), "T%s", ent->ce_tag);

	(void)xsnprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s%s/%s/%s/%s",
	    newent->ce_name, (newent->ce_status == CVS_ENT_REMOVED) ? "-" : "",
	    rev, timebuf, ent->ce_opts ? ent->ce_opts : "", sticky);

	cvs_ent_free(ent);
	cvs_ent_free(newent);
	cvs_ent_add(entlist, entry);
	cvs_ent_close(entlist, ENT_SYNC);
}

void
cvs_client_updated(char *data)
{
	int fd;
	time_t now;
	mode_t fmode, mask;
	size_t flen;
	CVSENTRIES *ent;
	struct cvs_ent *e;
	const char *errstr;
	struct timeval tv[2];
	char timebuf[32], repo[MAXPATHLEN], *rpath, entry[CVS_ENT_MAXLINELEN];
	char *en, *mode, revbuf[32], *len, *fpath, *wdir;

	if (data == NULL)
		fatal("Missing argument for Updated");

	client_check_directory(data);

	rpath = cvs_remote_input();
	en = cvs_remote_input();
	mode = cvs_remote_input();
	len = cvs_remote_input();

	cvs_get_repository_path(".", repo, MAXPATHLEN);

	STRIP_SLASH(repo);

	if (strlen(repo) + 1 > strlen(rpath))
		fatal("received a repository path that is too short");

	fpath = rpath + strlen(repo) + 1;
	if ((wdir = dirname(fpath)) == NULL)
		fatal("cvs_client_updated: dirname: %s", strerror(errno));

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_client_updated: %s: %s", len, errstr);
	xfree(len);

	cvs_strtomode(mode, &fmode);
	xfree(mode);
	mask = umask(0);
	umask(mask);
	fmode &= ~mask;

	time(&now);
	asctime_r(gmtime(&now), timebuf);
	if (timebuf[strlen(timebuf) - 1] == '\n')
		timebuf[strlen(timebuf) - 1] = '\0';

	e = cvs_ent_parse(en);
	xfree(en);
	rcsnum_tostr(e->ce_rev, revbuf, sizeof(revbuf));
	(void)xsnprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s/%s/%s/", e->ce_name,
	    revbuf, timebuf, e->ce_opts ? e->ce_opts : "");

	cvs_ent_free(e);
	ent = cvs_ent_open(wdir);
	cvs_ent_add(ent, entry);
	cvs_ent_close(ent, ENT_SYNC);

	if ((fd = open(fpath, O_CREAT | O_WRONLY | O_TRUNC)) == -1)
		fatal("cvs_client_updated: open: %s: %s",
		    fpath, strerror(errno));

	cvs_remote_receive_file(fd, flen);

	tv[0].tv_sec = now;
	tv[0].tv_usec = 0;
	tv[1] = tv[0];

	if (futimes(fd, tv) == -1)
		fatal("cvs_client_updated: futimes: %s", strerror(errno));

	if (fchmod(fd, fmode) == -1)
		fatal("cvs_client_updated: fchmod: %s", strerror(errno));

	(void)close(fd);

	xfree(rpath);
}

void
cvs_client_merged(char *data)
{
	int fd;
	time_t now;
	mode_t fmode;
	size_t flen;
	CVSENTRIES *ent;
	const char *errstr;
	struct timeval tv[2];
	char timebuf[32], *repo, *rpath, *entry, *mode;
	char *len, *fpath, *wdir;

	if (data == NULL)
		fatal("Missing argument for Merged");

	client_check_directory(data);

	rpath = cvs_remote_input();
	entry = cvs_remote_input();
	mode = cvs_remote_input();
	len = cvs_remote_input();

	repo = xmalloc(MAXPATHLEN);
	cvs_get_repository_path(".", repo, MAXPATHLEN);

	STRIP_SLASH(repo);

	if (strlen(repo) + 1 > strlen(rpath))
		fatal("received a repository path that is too short");

	fpath = rpath + strlen(repo) + 1;
	if ((wdir = dirname(fpath)) == NULL)
		fatal("cvs_client_merged: dirname: %s", strerror(errno));
	xfree(repo);

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_client_merged: %s: %s", len, errstr);
	xfree(len);

	cvs_strtomode(mode, &fmode);
	xfree(mode);

	time(&now);
	asctime_r(gmtime(&now), timebuf);
	if (timebuf[strlen(timebuf) - 1] == '\n')
		timebuf[strlen(timebuf) - 1] = '\0';

	ent = cvs_ent_open(wdir);
	cvs_ent_add(ent, entry);
	cvs_ent_close(ent, ENT_SYNC);

	if ((fd = open(fpath, O_CREAT | O_WRONLY | O_TRUNC)) == -1)
		fatal("cvs_client_merged: open: %s: %s",
		    fpath, strerror(errno));

	cvs_remote_receive_file(fd, flen);

	tv[0].tv_sec = now;
	tv[0].tv_usec = 0;
	tv[1] = tv[0];

	if (futimes(fd, tv) == -1)
		fatal("cvs_client_merged: futimes: %s", strerror(errno));

	if (fchmod(fd, fmode) == -1)
		fatal("cvs_client_merged: fchmod: %s", strerror(errno));

	(void)close(fd);

	xfree(rpath);
}

void
cvs_client_removed(char *data)
{
	char *dir;

	dir = cvs_remote_input();
	xfree(dir);
}

void
cvs_client_remove_entry(char *data)
{
	CVSENTRIES *entlist;
	char *filename, *rpath;

	if (data == NULL)
		fatal("Missing argument for Remove-entry");

	rpath = cvs_remote_input();
	if ((filename = strrchr(rpath, '/')) == NULL)
		fatal("bad rpath in cvs_client_remove_entry: %s", rpath);
	filename++;

	entlist = cvs_ent_open(data);
	cvs_ent_remove(entlist, filename);
	cvs_ent_close(entlist, ENT_SYNC);

	xfree(rpath);
}

void
cvs_client_set_static_directory(char *data)
{
	FILE *fp;
	char *dir, fpath[MAXPATHLEN];

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	if (data == NULL)
		fatal("Missing argument for Set-static-directory");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	xfree(dir);

	(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
	    data, CVS_PATH_STATICENTRIES);

	if ((fp = fopen(fpath, "w+")) == NULL) {
		cvs_log(LP_ERRNO, "%s", fpath);
		return;
	}
	(void)fclose(fp);
}

void
cvs_client_clear_static_directory(char *data)
{
	char *dir, fpath[MAXPATHLEN];

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	if (data == NULL)
		fatal("Missing argument for Clear-static-directory");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	xfree(dir);

	(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
	    data, CVS_PATH_STATICENTRIES);

	(void)cvs_unlink(fpath);
}

void
cvs_client_set_sticky(char *data)
{
	FILE *fp;
	char *dir, *tag, tagpath[MAXPATHLEN];

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	if (data == NULL)
		fatal("Missing argument for Set-sticky");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	xfree(dir);
	tag = cvs_remote_input();

	(void)xsnprintf(tagpath, MAXPATHLEN, "%s/%s", data, CVS_PATH_TAG);

	if ((fp = fopen(tagpath, "w+")) == NULL) {
		cvs_log(LP_ERRNO, "%s", tagpath);
		goto out;
	}

	(void)fprintf(fp, "%s\n", tag);
	(void)fclose(fp);
out:
	xfree(tag);
}

void
cvs_client_clear_sticky(char *data)
{
	char *dir, tagpath[MAXPATHLEN];

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	if (data == NULL)
		fatal("Missing argument for Clear-sticky");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	xfree(dir);

	(void)xsnprintf(tagpath, MAXPATHLEN, "%s/%s", data, CVS_PATH_TAG);
	(void)cvs_unlink(tagpath);
}


/*
 * cvs_client_initlog()
 *
 * Initialize protocol logging if the CVS_CLIENT_LOG environment variable is
 * set.  In this case, the variable's value is used as a path to which the
 * appropriate suffix is added (".in" for client input and ".out" for server
 * output).
 */
static void
cvs_client_initlog(void)
{
	u_int i;
	char *env, *envdup, buf[MAXPATHLEN], fpath[MAXPATHLEN];
	char rpath[MAXPATHLEN], *s;
	struct stat st;
	time_t now;
	struct passwd *pwd;

	/* avoid doing it more than once */
	if (cvs_client_logon)
		return;

	if ((env = getenv("CVS_CLIENT_LOG")) == NULL)
		return;

	envdup = xstrdup(env);
	if ((s = strchr(envdup, '%')) != NULL)
		*s = '\0';

	if (strlcpy(buf, env, sizeof(buf)) >= sizeof(buf))
		fatal("cvs_client_initlog: truncation");

	if (strlcpy(rpath, envdup, sizeof(rpath)) >= sizeof(rpath))
		fatal("cvs_client_initlog: truncation");

	xfree(envdup);

	s = buf;
	while ((s = strchr(s, '%')) != NULL) {
		s++;
		switch (*s) {
		case 'c':
			if (strlcpy(fpath, cvs_command, sizeof(fpath)) >=
			    sizeof(fpath))
				fatal("cvs_client_initlog: truncation");
			break;
		case 'd':
			time(&now);
			if (strlcpy(fpath, ctime(&now), sizeof(fpath)) >=
			    sizeof(fpath))
				fatal("cvs_client_initlog: truncation");
			break;
		case 'p':
			(void)xsnprintf(fpath, sizeof(fpath), "%d", getpid());
			break;
		case 'u':
			if ((pwd = getpwuid(getuid())) != NULL) {
				if (strlcpy(fpath, pwd->pw_name,
				    sizeof(fpath)) >= sizeof(fpath))
					fatal("cvs_client_initlog: truncation");
			} else {
				fpath[0] = '\0';
			}
			endpwent();
			break;
		default:
			fpath[0] = '\0';
			break;
		}

		if (fpath[0] != '\0') {
			if (strlcat(rpath, "-", sizeof(rpath)) >= sizeof(rpath))
				fatal("cvs_client_initlog: truncation");

			if (strlcat(rpath, fpath, sizeof(rpath))
			    >= sizeof(rpath))
				fatal("cvs_client_initlog: truncation");
		}
	}

	for (i = 0; i < UINT_MAX; i++) {
		(void)xsnprintf(fpath, sizeof(fpath), "%s-%d.in", rpath, i);

		if (stat(fpath, &st) != -1)
			continue;

		if (errno != ENOENT)
			fatal("cvs_client_initlog() stat failed '%s'",
			    strerror(errno));

		break;
	}

	if ((cvs_client_inlog_fd = open(fpath,
	    O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1) {
		fatal("cvs_client_initlog: open `%s': %s",
		    fpath, strerror(errno));
	}

	for (i = 0; i < UINT_MAX; i++) {
		(void)xsnprintf(fpath, sizeof(fpath), "%s-%d.out", rpath, i);

		if (stat(fpath, &st) != -1)
			continue;

		if (errno != ENOENT)
			fatal("cvs_client_initlog() stat failed '%s'",
			    strerror(errno));

		break;
	}

	if ((cvs_client_outlog_fd = open(fpath, 
	    O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1) {
		fatal("cvs_client_initlog: open `%s': %s",
		    fpath, strerror(errno));
	}

	cvs_client_logon = 1;
}

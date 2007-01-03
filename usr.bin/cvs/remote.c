/*	$OpenBSD: remote.c,v 1.5 2007/01/03 19:27:28 joris Exp $	*/
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
#include "remote.h"

struct cvs_resp *
cvs_remote_get_response_info(const char *response)
{
	int i;

	for (i = 0; cvs_responses[i].supported != -1; i++) {
		if (!strcmp(cvs_responses[i].name, response))
			return (&(cvs_responses[i]));
	}

	return (NULL);
}

struct cvs_req *
cvs_remote_get_request_info(const char *request)
{
	int i;

	for (i = 0; cvs_requests[i].supported != -1; i++) {
		if (!strcmp(cvs_requests[i].name, request))
			return (&(cvs_requests[i]));
	}

	return (NULL);
}

void
cvs_remote_output(const char *data)
{
	FILE *out;

	if (cvs_server_active)
		out = stdout;
	else
		out = current_cvsroot->cr_srvin;

	fputs(data, out);
	fputs("\n", out);
}

char *
cvs_remote_input(void)
{
	FILE *in;
	size_t len;
	char *data, *ldata;

	if (cvs_server_active)
		in = stdin;
	else
		in = current_cvsroot->cr_srvout;

	data = fgetln(in, &len);
	if (data == NULL) {
		if (sig_received != 0)
			fatal("received signal %d", sig_received);

		if (cvs_server_active) {
			cvs_cleanup();
			exit(0);
		}

		fatal("the connection has been closed by the server");
	}

	if (data[len - 1] == '\n') {
		data[len - 1] = '\0';
	} else {
		ldata = xmalloc(len + 1);
		if (strlcpy(ldata, data, len) >= len)
			fatal("cvs_remote_input: truncation");
		data = ldata;
	}

	ldata = xstrdup(data);

	if (cvs_server_active == 0 && cvs_client_outlog_fd != -1) {
		BUF *bp;

		bp = cvs_buf_alloc(strlen(ldata), BUF_AUTOEXT);

		if (cvs_buf_append(bp, ldata, strlen(ldata)) < 0)
			fatal("cvs_remote_input: cvs_buf_append");

		cvs_buf_putc(bp, '\n');

		if (cvs_buf_write_fd(bp, cvs_client_outlog_fd) < 0)
			fatal("cvs_remote_input: cvs_buf_write_fd");

		cvs_buf_free(bp);
	}

	return (ldata);
}

BUF *
cvs_remote_receive_file(size_t len)
{
	BUF *bp;
	FILE *in;
	size_t ret;
	char *data;

	if (cvs_server_active)
		in = stdin;
	else
		in = current_cvsroot->cr_srvout;

	bp = cvs_buf_alloc(len, BUF_AUTOEXT);

	if (len != 0) {
		data = xmalloc(len);
		ret = fread(data, sizeof(char), len, in);
		if (ret != len)
			fatal("length mismatch, expected %ld, got %ld",
			    len, ret);
		cvs_buf_set(bp, data, len, 0);
		xfree(data);
	}

	if (cvs_server_active == 0 && cvs_client_outlog_fd != -1) {
		if (cvs_buf_write_fd(bp, cvs_client_outlog_fd) < 0)
			fatal("cvs_remote_receive_file: cvs_buf_write_fd");
	}

	return (bp);
}

void
cvs_remote_send_file(const char *path)
{
	BUF *bp;
	int l, fd;
	FILE *out;
	size_t ret;
	struct stat st;
	char buf[16], *fcont;

	if (cvs_server_active)
		out = stdout;
	else
		out = current_cvsroot->cr_srvin;

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("cvs_remote_send_file: %s: %s", path, strerror(errno));

	if (fstat(fd, &st) == -1)
		fatal("cvs_remote_send_file: %s: %s", path, strerror(errno));

	cvs_modetostr(st.st_mode, buf, sizeof(buf));
	cvs_remote_output(buf);

	l = snprintf(buf, sizeof(buf), "%lld", st.st_size);
	if (l == -1 || l >= (int)sizeof(buf))
		fatal("cvs_remote_send_file: overflow");
	cvs_remote_output(buf);

	bp = cvs_buf_load_fd(fd, BUF_AUTOEXT);

	if (cvs_server_active == 0 && cvs_client_inlog_fd != -1) {
		if (cvs_buf_write_fd(bp, cvs_client_inlog_fd) < 0)
			fatal("cvs_remote_send_file: cvs_buf_write");
	}

	fcont = cvs_buf_release(bp);

	if (fcont != NULL) {
		ret = fwrite(fcont, sizeof(char), st.st_size, out);
		if (ret != st.st_size)
			fatal("tried to write %lld only wrote %ld",
			    st.st_size, ret);
		xfree(fcont);
	}

	(void)close(fd);
}

void
cvs_remote_classify_file(struct cvs_file *cf)
{
	time_t mtime;
	struct stat st;
	CVSENTRIES *entlist;

	entlist = cvs_ent_open(cf->file_wd);
	cf->file_ent = cvs_ent_get(entlist, cf->file_name);
	cvs_ent_close(entlist, ENT_NOSYNC);

	if (cf->file_ent != NULL && cf->file_ent->ce_status != CVS_ENT_REG) {
		if (cf->file_ent->ce_status == CVS_ENT_ADDED)
			cf->file_status = FILE_ADDED;
		else
			cf->file_status = FILE_REMOVED;
		return;
	}

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_type == CVS_ENT_DIR)
			cf->file_type = CVS_DIR;
		else
			cf->file_type = CVS_FILE;
	}

	if (cf->fd != -1 && cf->file_ent != NULL) {
		if (fstat(cf->fd, &st) == -1)
			fatal("cvs_remote_classify_file(%s): %s", cf->file_path,
			    strerror(errno));

		mtime = cvs_hack_time(st.st_mtime, 1);
		if (mtime != cf->file_ent->ce_mtime)
			cf->file_status = FILE_MODIFIED;
		else
			cf->file_status = FILE_UPTODATE;
	} else if (cf->fd == -1) {
		cf->file_status = FILE_UNKNOWN;
	}
}


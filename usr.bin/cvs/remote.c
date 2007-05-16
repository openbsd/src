/*	$OpenBSD: remote.c,v 1.15 2007/05/16 19:40:45 xsa Exp $	*/
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
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
		ldata = xstrdup(data);
	} else {
		ldata = xmalloc(len + 1);
		memcpy(ldata, data, len);
		ldata[len] = '\0';
	}

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

void
cvs_remote_receive_file(int fd, size_t len)
{
	FILE *in;
	char data[MAXBSIZE];
	size_t nread, nwrite, nleft, toread;

	if (cvs_server_active)
		in = stdin;
	else
		in = current_cvsroot->cr_srvout;

	nleft = len;

	while (nleft > 0) {
		toread = MIN(nleft, MAXBSIZE);

		nread = fread(data, sizeof(char), toread, in);
		if (nread == 0)
			fatal("error receiving file");

		nwrite = write(fd, data, nread);
		if (nwrite != nread)
			fatal("failed to write %zu bytes", nread);

		if (cvs_server_active == 0 &&
		    cvs_client_outlog_fd != -1)
			(void)write(cvs_client_outlog_fd, data, nread);

		nleft -= nread;
	}
}

void
cvs_remote_send_file(const char *path)
{
	int fd;
	FILE *out, *in;
	size_t ret, rw;
	off_t total;
	struct stat st;
	char buf[18], data[MAXBSIZE];

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

	(void)xsnprintf(buf, sizeof(buf), "%lld", st.st_size);
	cvs_remote_output(buf);

	if ((in = fdopen(fd, "r")) == NULL)
		fatal("cvs_remote_send_file: fdopen %s", strerror(errno));

	total = 0;
	while ((ret = fread(data, sizeof(char), MAXBSIZE, in)) != 0) {
		rw = fwrite(data, sizeof(char), ret, out);
		if (rw != ret)
			fatal("failed to write %zu bytes", ret);

		if (cvs_server_active == 0 && cvs_client_inlog_fd != -1)
			(void)write(cvs_client_inlog_fd, data, ret);

		total += ret;
	}

	if (total != st.st_size)
		fatal("length mismatch, %lld vs %lld", total, st.st_size);

	(void)fclose(in);
}

void
cvs_remote_classify_file(struct cvs_file *cf)
{
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

		if (st.st_mtime != cf->file_ent->ce_mtime)
			cf->file_status = FILE_MODIFIED;
		else
			cf->file_status = FILE_UPTODATE;
	} else if (cf->fd == -1) {
		cf->file_status = FILE_UNKNOWN;
	}

	if (cvs_cmdop == CVS_OP_IMPORT && cf->file_type == CVS_FILE)
		cf->file_status = FILE_MODIFIED;
}


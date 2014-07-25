/*	$OpenBSD: server_file.c,v 1.12 2014/07/25 12:46:23 reyk Exp $	*/

/*
 * Copyright (c) 2006 - 2014 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <event.h>

#include <openssl/ssl.h>

#include "httpd.h"
#include "http.h"

int	 server_file_access(struct http_descriptor *, char *, size_t,
	    struct stat *);
void	 server_file_error(struct bufferevent *, short, void *);

int
server_file_access(struct http_descriptor *desc, char *path, size_t len,
    struct stat *st)
{
	errno = 0;
	if (access(path, R_OK) == -1) {
		goto fail;
	} else if (stat(path, st) == -1) {
		goto fail;
	} else if (S_ISDIR(st->st_mode)) {
		/* XXX Should we support directory listing? */

		if (!len) {
			/* Recursion - the index "file" is a directory? */
			errno = EINVAL;
			goto fail;
		}

		/* Redirect to path with trailing "/" */
		if (path[strlen(path) - 1] != '/') {
			/* Remove the document root to get the relative URL */
			if (canonicalize_path(NULL,
			    desc->http_path, path, len) == NULL ||
			    strlcat(path, "/", len) >= len) {
				errno = EINVAL;
				goto fail;
			}

			/* Indicate that the file has been moved */
			return (301);
		}

		/* Otherwise append the default index file */
		if (strlcat(path, HTTPD_INDEX, len) >= len) {
			errno = EACCES;
			goto fail;
		}

		/* Check again but set len to 0 to avoid recursion */
		return (server_file_access(desc, path, 0, st));
	} else if (!S_ISREG(st->st_mode)) {
		/* Don't follow symlinks and ignore special files */
		errno = EACCES;
		goto fail;
	}

	return (0);

 fail:
	/* Remove the document root */
	if (len && canonicalize_path(NULL, desc->http_path, path, len) == NULL)
		return (500);

	switch (errno) {
	case ENOENT:
		return (404);
	case EACCES:
		return (403);
	default:
		return (500);
	}

	/* NOTREACHED */
}

int
server_file(struct httpd *env, struct client *clt)
{
	struct http_descriptor	*desc = clt->clt_desc;
	struct server		*srv = clt->clt_srv;
	struct media_type	*media;
	const char		*errstr = NULL;
	int			 fd = -1, ret;
	char			 path[MAXPATHLEN];
	struct stat		 st;

	if (canonicalize_path(srv->srv_conf.docroot,
	    desc->http_path, path, sizeof(path)) == NULL) {
		/* Do not echo the uncanonicalized path */
		server_abort_http(clt, 500, "invalid request path");
		return (-1);
	}

	/* Returns HTTP status code on error */
	if ((ret = server_file_access(desc, path, sizeof(path), &st)) != 0) {
		server_abort_http(clt, ret, path);
		return (-1);
	}

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto fail;

	/* File descriptor is opened, decrement inflight counter */
	server_inflight_dec(clt, __func__);

	media = media_find(env->sc_mediatypes, path);
	ret = server_response_http(clt, 200, media, st.st_size);
	switch (ret) {
	case -1:
		goto fail;
	case 0:
		/* Connection is already finished */
		close(fd);
		return (0);
	default:
		break;
	}

	clt->clt_fd = fd;
	if (clt->clt_file != NULL)
		bufferevent_free(clt->clt_file);

	clt->clt_file = bufferevent_new(clt->clt_fd, server_read,
	    server_write, server_file_error, clt);
	if (clt->clt_file == NULL) {
		errstr = "failed to allocate file buffer event";
		goto fail;
	}

	bufferevent_settimeout(clt->clt_file,
	    srv->srv_conf.timeout.tv_sec, srv->srv_conf.timeout.tv_sec);
	bufferevent_enable(clt->clt_file, EV_READ);
	bufferevent_disable(clt->clt_bev, EV_READ);

	return (0);
 fail:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, 500, errstr);
	return (-1);
}

void
server_file_error(struct bufferevent *bev, short error, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*dst;

	if (error & EVBUFFER_TIMEOUT) {
		server_close(clt, "buffer event timeout");
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ);

		clt->clt_done = 1;

		dst = EVBUFFER_OUTPUT(clt->clt_bev);
		if (EVBUFFER_LENGTH(dst)) {
			/* Finish writing all data first */
			bufferevent_enable(clt->clt_bev, EV_WRITE);
			return;
		}

		if (clt->clt_persist) {
			/* Close input file and wait for next HTTP request */
			if (clt->clt_fd != -1)
				close(clt->clt_fd);
			clt->clt_fd = -1;
			clt->clt_toread = TOREAD_HTTP_HEADER;
			server_reset_http(clt);
			bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
			return;
		}
		server_close(clt, "done");
		return;
	}
	if (error & EVBUFFER_ERROR && errno == EFBIG) {
		bufferevent_enable(bev, EV_READ);
		return;
	}
	server_close(clt, "buffer event error");
	return;
}

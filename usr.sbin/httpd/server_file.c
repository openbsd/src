/*	$OpenBSD: server_file.c,v 1.1 2014/07/12 23:34:54 reyk Exp $	*/

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

int
server_response(struct client *clt)
{
	struct http_descriptor	*desc = clt->clt_desc;
	struct server		*srv = clt->clt_server;
	struct kv		*kv;
	const char		*errstr = NULL;
	int			 fd = -1;
	char			 path[MAXPATHLEN];
	struct stat		 st;

	if (desc->http_path == NULL)
		goto fail;

	/* 
	 * XXX This is not ready XXX
	 * XXX Don't expect anything from this code yet,
	 */

	strlcpy(path, "/htdocs", sizeof(path));
	if (desc->http_path[0] != '/')
		strlcat(path, "/", sizeof(path));
	strlcat(path, desc->http_path, sizeof(path));
	if (desc->http_path[strlen(desc->http_path) - 1] == '/')
		strlcat(path, "index.html", sizeof(path));

	if (access(path, R_OK) == -1) {
		switch (errno) {
		case EACCES:
			server_abort_http(clt, 403, path);
			break;
		case ENOENT:
			server_abort_http(clt, 404, path);
			break;
		default:
			server_abort_http(clt, 500, path);
			break;
		}
		return (-1);
	}

	if ((fd = open(path, O_RDONLY)) == -1 || fstat(fd, &st) == -1)
		goto fail;

	/* XXX verify results XXX */
	kv_purge(&desc->http_headers);
	kv_add(&desc->http_headers, "Server", HTTPD_SERVERNAME);
	kv_add(&desc->http_headers, "Connection", "close");
	if ((kv = kv_add(&desc->http_headers, "Content-Length", NULL)) != NULL)
		kv_set(kv, "%ld", st.st_size);
	kv_setkey(&desc->http_pathquery, "200");
	kv_set(&desc->http_pathquery, "%s", server_httperror_byid(200));

	if (server_writeresponse_http(clt) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1 ||
	    server_writeheader_http(clt) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1)
		goto fail;

	clt->clt_fd = fd;
	clt->clt_file = bufferevent_new(clt->clt_fd, server_read,
	    server_write, server_error, clt);
	if (clt->clt_file == NULL) {
		errstr = "failed to allocate file buffer event";
		goto fail;
	}

	bufferevent_settimeout(clt->clt_file,
	    srv->srv_conf.timeout.tv_sec, srv->srv_conf.timeout.tv_sec);
	bufferevent_enable(clt->clt_file, EV_READ|EV_WRITE);

	return (0);
 fail:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, 500, errstr);
	return (-1);
}

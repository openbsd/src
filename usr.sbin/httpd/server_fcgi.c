/*	$OpenBSD: server_fcgi.c,v 1.6 2014/08/01 08:34:46 florian Exp $	*/

/*
 * Copyright (c) 2014 Florian Obser <florian@openbsd.org>
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

#define FCGI_CONTENT_SIZE	 65535
#define FCGI_PADDING_SIZE	 255
#define FCGI_RECORD_SIZE	 \
    (sizeof(struct fcgi_record_header) + FCGI_CONTENT_SIZE + FCGI_PADDING_SIZE)

#define FCGI_BEGIN_REQUEST	 1
#define FCGI_ABORT_REQUEST	 2
#define FCGI_END_REQUEST	 3
#define FCGI_PARAMS		 4
#define FCGI_STDIN		 5
#define FCGI_STDOUT		 6
#define FCGI_STDERR		 7
#define FCGI_DATA		 8
#define FCGI_GET_VALUES		 9
#define FCGI_GET_VALUES_RESULT	10
#define FCGI_UNKNOWN_TYPE	11
#define FCGI_MAXTYPE		(FCGI_UNKNOWN_TYPE)

#define FCGI_RESPONDER		 1

struct fcgi_record_header {
	uint8_t		version;
	uint8_t		type;
	uint16_t	id;
	uint16_t	content_len;
	uint8_t		padding_len;
	uint8_t		reserved;
} __packed;

struct fcgi_begin_request_body {
	uint16_t	role;
	uint8_t		flags;
	uint8_t		reserved[5];
} __packed;

int	server_fcgi_header(struct client *, u_int);
void	server_fcgi_read(struct bufferevent *, void *);
int	fcgi_add_param(uint8_t *, char *, char *, int);

int
server_fcgi(struct httpd *env, struct client *clt)
{
	struct server_config		*srv_conf = clt->clt_srv_conf;
	struct http_descriptor		*desc	= clt->clt_desc;
	struct sockaddr_un		 sun;
	struct fcgi_record_header 	*h;
	struct fcgi_begin_request_body	*begin;
	size_t				 len, total_len;
	int				 fd;
	const char			*errstr = NULL;
	uint8_t				 buf[FCGI_RECORD_SIZE];
	uint8_t				*params;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		goto fail;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	len = strlcpy(sun.sun_path, srv_conf->path, sizeof(sun.sun_path));
	if (len >= sizeof(sun.sun_path)) {
		errstr = "socket path to long";
		goto fail;
	}
	sun.sun_len = len;

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		goto fail;

	clt->clt_fcgi_state = FCGI_READ_HEADER;
	clt->clt_fcgi_toread = sizeof(struct fcgi_record_header);

	if (clt->clt_srvevb != NULL)
		evbuffer_free(clt->clt_srvevb);

	clt->clt_srvevb = evbuffer_new();
	if (clt->clt_srvevb == NULL) {
		errstr = "failed to allocate evbuffer";
		goto fail;
	}

	if (clt->clt_srvbev != NULL)
		bufferevent_free(clt->clt_srvbev);

	clt->clt_srvbev = bufferevent_new(fd, server_fcgi_read,
	    NULL, server_file_error, clt);
	if (clt->clt_srvbev == NULL) {
		errstr = "failed to allocate fcgi buffer event";
		goto fail;
	}

	bzero(&buf, sizeof(buf));

	h = (struct fcgi_record_header *) &buf;
	h->version = 1;
	h->type = FCGI_BEGIN_REQUEST;
	h->id = htons(1);
	h->content_len = htons(sizeof(struct fcgi_begin_request_body));
	h->padding_len = 0;

	begin = (struct fcgi_begin_request_body *) &buf[sizeof(struct
	    fcgi_record_header)];
	begin->role = htons(FCGI_RESPONDER);

	bufferevent_write(clt->clt_srvbev, &buf,
	    sizeof(struct fcgi_record_header) +
	    sizeof(struct fcgi_begin_request_body));

	h->type = FCGI_PARAMS;
	h->content_len = 0;
	params = &buf[sizeof(struct fcgi_record_header)];

	total_len = 0;

	len = fcgi_add_param(params, "SCRIPT_NAME", desc->http_path,
	    FCGI_CONTENT_SIZE);
	params += len;
	total_len += len;

	if (desc->http_query) {
		len = fcgi_add_param(params, "QUERY_STRING", desc->http_query,
		    FCGI_CONTENT_SIZE);
		params += len;
		total_len += len;
	}

	h->content_len = htons(total_len);

	bufferevent_write(clt->clt_srvbev, &buf,
	    sizeof(struct fcgi_record_header) +
	    ntohs(h->content_len));

	h->content_len = 0;

	bufferevent_write(clt->clt_srvbev, &buf,
	    sizeof(struct fcgi_record_header));

	h->type = FCGI_STDIN;

	bufferevent_write(clt->clt_srvbev, &buf,
	    sizeof(struct fcgi_record_header));

	bufferevent_settimeout(clt->clt_srvbev,
	    srv_conf->timeout.tv_sec, srv_conf->timeout.tv_sec);
	bufferevent_enable(clt->clt_srvbev, EV_READ|EV_WRITE);
	bufferevent_disable(clt->clt_bev, EV_READ);

	/*
	 * persist is not supported yet because we don't get the
	 * Content-Length from slowcgi and don't support chunked encoding.
	 */
	clt->clt_persist = 0;
	clt->clt_done = 0;

	return (0);
 fail:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, 500, errstr);
	return (-1);
}

int
fcgi_add_param(uint8_t *buf, char *key, char *val, int size)
{
	int len = 0;
	DPRINTF("%s: %s => %s", __func__, key, val);
	buf[0] = strlen(key);
	len++;
	buf[1] = strlen(val);
	len++;
	len += strlcpy(buf + len, key, size - len);
	len += strlcpy(buf + len, val, size - len);

	return len;
}

void
server_fcgi_read(struct bufferevent *bev, void *arg)
{
	struct client *clt = (struct client *) arg;
	struct fcgi_record_header 	*h;
	uint8_t	 buf[FCGI_RECORD_SIZE];
	size_t	 len;

	len = bufferevent_read(bev, &buf, clt->clt_fcgi_toread);
	/* XXX error handling */
	evbuffer_add(clt->clt_srvevb, &buf, len);
	clt->clt_fcgi_toread -= len;
	DPRINTF("%s: len: %lu toread: %d state: %d", __func__, len,
	    clt->clt_fcgi_toread, clt->clt_fcgi_state);

	if (clt->clt_fcgi_toread != 0)
		return;

	switch (clt->clt_fcgi_state) {
	case FCGI_READ_HEADER:
		clt->clt_fcgi_state = FCGI_READ_CONTENT;
		h = (struct fcgi_record_header *)
		    EVBUFFER_DATA(clt->clt_srvevb);
		DPRINTF("%s: record header: version %d type %d id %d "
		    "content len %d", __func__, h->version, h->type,
		    ntohs(h->id), ntohs(h->content_len));
		clt->clt_fcgi_type = h->type;
		clt->clt_fcgi_toread = ntohs(h->content_len);
		evbuffer_drain(clt->clt_srvevb,
		    EVBUFFER_LENGTH(clt->clt_srvevb));
		if (clt->clt_fcgi_toread != 0)
			break;

		/* fallthrough if content_len == 0 */
	case FCGI_READ_CONTENT:
		if (clt->clt_fcgi_type == FCGI_STDOUT &&
		    EVBUFFER_LENGTH(clt->clt_srvevb) > 0) {
			if (++clt->clt_chunk == 1)
				server_fcgi_header(clt, 200);
			server_bufferevent_write_buffer(clt,
			    clt->clt_srvevb);
		}
		evbuffer_drain(clt->clt_srvevb,
		    EVBUFFER_LENGTH(clt->clt_srvevb));
		clt->clt_fcgi_state = FCGI_READ_HEADER;
		clt->clt_fcgi_toread =
		    sizeof(struct fcgi_record_header);
	}
}

int
server_fcgi_header(struct client *clt, u_int code)
{
	struct http_descriptor	*desc = clt->clt_desc;
	const char		*error;
	char			 tmbuf[32];

	if (desc == NULL || (error = server_httperror_byid(code)) == NULL)
		return (-1);

	kv_purge(&desc->http_headers);

	/* Add error codes */
	if (kv_setkey(&desc->http_pathquery, "%lu", code) == -1 ||
	    kv_set(&desc->http_pathquery, "%s", error) == -1)
		return (-1);

	/* Add headers */
	if (kv_add(&desc->http_headers, "Server", HTTPD_SERVERNAME) == NULL)
		return (-1);

	/* Is it a persistent connection? */
	if (clt->clt_persist) {
		if (kv_add(&desc->http_headers,
		    "Connection", "keep-alive") == NULL)
			return (-1);
	} else if (kv_add(&desc->http_headers, "Connection", "close") == NULL)
		return (-1);

	/* Date header is mandatory and should be added last */
	server_http_date(tmbuf, sizeof(tmbuf));
	if (kv_add(&desc->http_headers, "Date", tmbuf) == NULL)
		return (-1);

	/* Write initial header (fcgi might append more) */
	if (server_writeresponse_http(clt) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1 ||
	    server_writeheader_http(clt) == -1)
		return (-1);

	return (0);
}

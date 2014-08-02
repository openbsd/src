/*	$OpenBSD: server_fcgi.c,v 1.11 2014/08/02 17:05:18 florian Exp $	*/

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
int	fcgi_add_param(uint8_t *, const char *, const char *, int *,
    struct client *);

int
server_fcgi(struct httpd *env, struct client *clt)
{
	uint8_t				 buf[FCGI_RECORD_SIZE];
	char				 hbuf[MAXHOSTNAMELEN];
	struct server_config		*srv_conf = clt->clt_srv_conf;
	struct http_descriptor		*desc	= clt->clt_desc;
	struct sockaddr_un		 sun;
	struct fcgi_record_header	*h;
	struct fcgi_begin_request_body	*begin;
	struct kv			*kv, key;
	size_t				 len;
	int				 fd = -1, total_len;
	const char			*errstr = NULL;
	char				*request_uri, *p;
	in_port_t			 port;
	struct sockaddr_storage		 ss;

	if (srv_conf->path[0] == ':') {
		p = srv_conf->path + 1;

		port = strtonum(p, 0, 0xffff, &errstr);
		if (errstr != NULL) {
			log_warn("%s: strtonum %s, %s", __func__, p, errstr);
			goto fail;
		}
		memset(&ss, 0, sizeof(ss));
		ss.ss_family = AF_INET;
		((struct sockaddr_in *)
		    &ss)->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		port = htons(port);

		if ((fd = server_socket_connect(&ss, port, srv_conf)) == -1)
			goto fail;
	} else {
		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
			goto fail;

		memset(&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		len = strlcpy(sun.sun_path,
		    srv_conf->path, sizeof(sun.sun_path));
		if (len >= sizeof(sun.sun_path)) {
			errstr = "socket path to long";
			goto fail;
		}
		sun.sun_len = len;

		if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
			goto fail;
	}

	socket_set_blockmode(fd, BM_NONBLOCK);

	memset(&hbuf, 0, sizeof(hbuf));
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

	memset(&buf, 0, sizeof(buf));

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
	h->content_len = total_len = 0;

	if (fcgi_add_param(buf, "SCRIPT_NAME", desc->http_path, &total_len,
	    clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (desc->http_query)
		if (fcgi_add_param(buf, "QUERY_STRING", desc->http_query,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	if (fcgi_add_param(buf, "DOCUMENT_URI", desc->http_path, &total_len,
	    clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}
	if (fcgi_add_param(buf, "GATEWAY_INTERFACE", "CGI/1.1", &total_len,
	    clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	key.kv_key = "Accept";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_ACCEPT", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "Accept-Encoding";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_ACCEPT_ENCODING", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "Accept-Language";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_ACCEPT_LANGUAGE", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "Connection";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_CONNECTION", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "Cookie";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_COOKIE", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "Host";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_HOST", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	key.kv_key = "User-Agent";
	if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
	    kv->kv_value != NULL)
		if (fcgi_add_param(buf, "HTTP_USER_AGENT", kv->kv_value,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}

	(void)print_host(&clt->clt_ss, hbuf, sizeof(hbuf));
	if (fcgi_add_param(buf, "REMOTE_ADDR", hbuf, &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	(void)snprintf(hbuf, sizeof(hbuf), "%d", ntohs(clt->clt_port));
	if (fcgi_add_param(buf, "REMOTE_PORT", hbuf, &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (fcgi_add_param(buf, "REQUEST_METHOD",
	    server_httpmethod_byid(desc->http_method), &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (!desc->http_query) {
		if (fcgi_add_param(buf, "REQUEST_URI", desc->http_path,
		    &total_len, clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}
	} else if (asprintf(&request_uri, "%s?%s", desc->http_path,
	    desc->http_query) != -1) {
		if (fcgi_add_param(buf, "REQUEST_URI", request_uri, &total_len,
		    clt) == -1) {
			errstr = "failed to encode param";
			goto fail;
		}
		free(request_uri);
	}

	(void)print_host(&clt->clt_srv_ss, hbuf, sizeof(hbuf));
	if (fcgi_add_param(buf, "SERVER_ADDR", hbuf, &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	(void)snprintf(hbuf, sizeof(hbuf), "%d",
	    ntohs(server_socket_getport(&clt->clt_srv_ss)));
	if (fcgi_add_param(buf, "SERVER_PORT", hbuf, &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (fcgi_add_param(buf, "SERVER_NAME", srv_conf->name, &total_len,
	    clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (fcgi_add_param(buf, "SERVER_PROTOCOL", desc->http_version,
	    &total_len, clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (fcgi_add_param(buf, "SERVER_SOFTWARE", HTTPD_SERVERNAME, &total_len,
	    clt) == -1) {
		errstr = "failed to encode param";
		goto fail;
	}

	if (total_len != 0) {	/* send last params record */
		bufferevent_write(clt->clt_srvbev, &buf,
		    sizeof(struct fcgi_record_header) +
		    ntohs(h->content_len));
	}

	/* send "no more params" message */
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
fcgi_add_param(uint8_t *buf, const char *key, const char *val, int *total_len,
    struct client *clt)
{
	struct fcgi_record_header	*h;
	int				 len = 0;
	int				 key_len = strlen(key);
	int				 val_len = strlen(val);
	uint8_t				*param;

	len += key_len + val_len;
	len += key_len > 127 ? 4 : 1;
	len += val_len > 127 ? 4 : 1;

	DPRINTF("%s: %s[%d] => %s[%d], total_len: %d", __func__, key, key_len,
	    val, val_len, *total_len);

	if (len > FCGI_CONTENT_SIZE)
		return (-1);

	if (*total_len + len > FCGI_CONTENT_SIZE) {
		bufferevent_write(clt->clt_srvbev, buf,
		    sizeof(struct fcgi_record_header) + *total_len);
		*total_len = 0;
	}

	h = (struct fcgi_record_header *) buf;
	param = buf + sizeof(struct fcgi_record_header) + *total_len;

	if (key_len > 127) {
		*param++ = ((key_len >> 24) & 0xff) | 0x80;
		*param++ = ((key_len >> 16) & 0xff);
		*param++ = ((key_len >> 8) & 0xff);
		*param++ = (key_len & 0xff);
	} else
		*param++ = key_len;

	if (val_len > 127) {
		*param++ = ((val_len >> 24) & 0xff) | 0x80;
		*param++ = ((val_len >> 16) & 0xff);
		*param++ = ((val_len >> 8) & 0xff);
		*param++ = (val_len & 0xff);
	} else
		*param++ = val_len;

	memcpy(param, key, key_len);
	param += key_len;
	memcpy(param, val, val_len);

	*total_len += len;

	h->content_len = htons(*total_len);
	return (0);
}

void
server_fcgi_read(struct bufferevent *bev, void *arg)
{
	uint8_t				 buf[FCGI_RECORD_SIZE];
	struct client			*clt = (struct client *) arg;
	struct fcgi_record_header	*h;
	size_t				 len;

	do {
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
			    "content len %d padding %d", __func__,
			     h->version, h->type, ntohs(h->id),
			     ntohs(h->content_len), h->padding_len);
			clt->clt_fcgi_type = h->type;
			clt->clt_fcgi_toread = ntohs(h->content_len);
			clt->clt_fcgi_padding_len = h->padding_len;
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
			if (!clt->clt_fcgi_padding_len) {
				clt->clt_fcgi_state = FCGI_READ_HEADER;
				clt->clt_fcgi_toread =
				    sizeof(struct fcgi_record_header);
			} else {
				clt->clt_fcgi_state = FCGI_READ_PADDING;
				clt->clt_fcgi_toread =
				    clt->clt_fcgi_padding_len;
			}
			break;
		case FCGI_READ_PADDING:
			evbuffer_drain(clt->clt_srvevb,
			    EVBUFFER_LENGTH(clt->clt_srvevb));
			clt->clt_fcgi_state = FCGI_READ_HEADER;
			clt->clt_fcgi_toread =
			    sizeof(struct fcgi_record_header);
			break;
		}
	} while (len > 0);
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

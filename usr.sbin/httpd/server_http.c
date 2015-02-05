/*	$OpenBSD: server_http.c,v 1.70 2015/02/05 10:47:53 reyk Exp $	*/

/*
 * Copyright (c) 2006 - 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/socket.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <resolv.h>
#include <event.h>
#include <fnmatch.h>

#include "httpd.h"
#include "http.h"

static int	 server_httpmethod_cmp(const void *, const void *);
static int	 server_httperror_cmp(const void *, const void *);
void		 server_httpdesc_free(struct http_descriptor *);
int		 server_http_authenticate(struct server_config *,
    struct client *);

static struct httpd	*env = NULL;

static struct http_method	 http_methods[] = HTTP_METHODS;
static struct http_error	 http_errors[] = HTTP_ERRORS;

void
server_http(struct httpd *x_env)
{
	if (x_env != NULL)
		env = x_env;

	DPRINTF("%s: sorting lookup tables, pid %d", __func__, getpid());

	/* Sort the HTTP lookup arrays */
	qsort(http_methods, sizeof(http_methods) /
	    sizeof(http_methods[0]) - 1,
	    sizeof(http_methods[0]), server_httpmethod_cmp);
	qsort(http_errors, sizeof(http_errors) /
	    sizeof(http_errors[0]) - 1,
	    sizeof(http_errors[0]), server_httperror_cmp);
}

void
server_http_init(struct server *srv)
{
	/* nothing */
}

int
server_httpdesc_init(struct client *clt)
{
	struct http_descriptor	*desc;

	if ((desc = calloc(1, sizeof(*desc))) == NULL)
		return (-1);
	RB_INIT(&desc->http_headers);
	clt->clt_descreq = desc;

	if ((desc = calloc(1, sizeof(*desc))) == NULL) {
		/* req will be cleaned up later */
		return (-1);
	}
	RB_INIT(&desc->http_headers);
	clt->clt_descresp = desc;

	return (0);
}

void
server_httpdesc_free(struct http_descriptor *desc)
{
	if (desc == NULL)
		return;
	if (desc->http_path != NULL) {
		free(desc->http_path);
		desc->http_path = NULL;
	}
	if (desc->http_path_alias != NULL) {
		free(desc->http_path_alias);
		desc->http_path_alias = NULL;
	}
	if (desc->http_query != NULL) {
		free(desc->http_query);
		desc->http_query = NULL;
	}
	if (desc->http_version != NULL) {
		free(desc->http_version);
		desc->http_version = NULL;
	}
	if (desc->http_host != NULL) {
		free(desc->http_host);
		desc->http_host = NULL;
	}
	kv_purge(&desc->http_headers);
	desc->http_lastheader = NULL;
	desc->http_method = 0;
	desc->http_chunked = 0;
}

int
server_http_authenticate(struct server_config *srv_conf, struct client *clt)
{
	FILE *fp = NULL;
	struct http_descriptor *desc = clt->clt_descreq;
	struct auth *auth = srv_conf->auth;
	struct kv *ba, key;
	size_t linesize = 0;
	ssize_t linelen;
	int ret = -1;
	char *line = NULL, decoded[1024];
	char *clt_user = NULL, *clt_pass = NULL, *user = NULL, *pass = NULL;

	memset(decoded, 0, sizeof(decoded));
	key.kv_key = "Authorization";

	if ((ba = kv_find(&desc->http_headers, &key)) == NULL ||
	    ba->kv_value == NULL)
		goto done;

	if (strncmp(ba->kv_value, "Basic ", strlen("Basic ")) != 0)
		goto done;

	if (b64_pton(strchr(ba->kv_value, ' ') + 1, decoded,
	    sizeof(decoded)) <= 0)
		goto done;

	if ((clt_pass = strchr(decoded, ':')) == NULL)
		goto done;

	clt_user = decoded;
	*clt_pass++ = '\0';
	if ((clt->clt_remote_user = strdup(clt_user)) == NULL)
		goto done;

	if (clt_pass == NULL)
		goto done;

	if ((fp = fopen(auth->auth_htpasswd, "r")) == NULL)
		goto done;

	while ((linelen = getline(&line, &linesize, fp)) != -1) {
		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';
		user = line;
		pass = strchr(line, ':');

		if (pass == NULL) {
			explicit_bzero(line, linelen);
			continue;
		}

		*pass++ = '\0';

		if (strcmp(clt_user, user) != 0) {
			explicit_bzero(line, linelen);
			continue;
		}

		if (crypt_checkpass(clt_pass, pass) == 0) {
			explicit_bzero(line, linelen);
			ret = 0;
			break;
		}
	}
done:
	if (fp != NULL)
		fclose(fp);

	if (ba != NULL && ba->kv_value != NULL) {
		explicit_bzero(ba->kv_value, strlen(ba->kv_value));
		explicit_bzero(decoded, sizeof(decoded));
	}

	return (ret);
}

void
server_read_http(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct http_descriptor	*desc = clt->clt_descreq;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	char			*line = NULL, *key, *value;
	const char		*errstr;
	size_t			 size, linelen;
	struct kv		*hdr = NULL;

	getmonotime(&clt->clt_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: session %d: size %lu, to read %lld",
	    __func__, clt->clt_id, size, clt->clt_toread);
	if (!size) {
		clt->clt_toread = TOREAD_HTTP_HEADER;
		goto done;
	}

	while (!clt->clt_done && (line = evbuffer_readline(src)) != NULL) {
		linelen = strlen(line);

		/*
		 * An empty line indicates the end of the request.
		 * libevent already stripped the \r\n for us.
		 */
		if (!linelen) {
			clt->clt_done = 1;
			free(line);
			break;
		}
		key = line;

		/* Limit the total header length minus \r\n */
		clt->clt_headerlen += linelen;
		if (clt->clt_headerlen > SERVER_MAXHEADERLENGTH) {
			server_abort_http(clt, 413, "request too large");
			goto abort;
		}

		/*
		 * The first line is the GET/POST/PUT/... request,
		 * subsequent lines are HTTP headers.
		 */
		if (++clt->clt_line == 1)
			value = strchr(key, ' ');
		else if (*key == ' ' || *key == '\t')
			/* Multiline headers wrap with a space or tab */
			value = NULL;
		else
			value = strchr(key, ':');
		if (value == NULL) {
			if (clt->clt_line == 1) {
				server_abort_http(clt, 400, "malformed");
				goto abort;
			}

			/* Append line to the last header, if present */
			if (kv_extend(&desc->http_headers,
			    desc->http_lastheader, line) == NULL)
				goto fail;

			free(line);
			continue;
		}
		if (*value == ':') {
			*value++ = '\0';
			value += strspn(value, " \t\r\n");
		} else {
			*value++ = '\0';
		}

		DPRINTF("%s: session %d: header '%s: %s'", __func__,
		    clt->clt_id, key, value);

		/*
		 * Identify and handle specific HTTP request methods
		 */
		if (clt->clt_line == 1) {
			if ((desc->http_method = server_httpmethod_byname(key))
			    == HTTP_METHOD_NONE) {
				server_abort_http(clt, 400, "malformed");
				goto abort;
			}

			/*
			 * Decode request path and query
			 */
			desc->http_path = strdup(value);
			if (desc->http_path == NULL)
				goto fail;

			desc->http_version = strchr(desc->http_path, ' ');
			if (desc->http_version == NULL)
				goto fail;

			*desc->http_version++ = '\0';
			desc->http_query = strchr(desc->http_path, '?');
			if (desc->http_query != NULL)
				*desc->http_query++ = '\0';

			/*
			 * Have to allocate the strings because they could
			 * be changed independently by the filters later.
			 */
			if ((desc->http_version =
			    strdup(desc->http_version)) == NULL)
				goto fail;

			if (desc->http_query != NULL &&
			    (desc->http_query =
			    strdup(desc->http_query)) == NULL)
				goto fail;

		} else if (desc->http_method != HTTP_METHOD_NONE &&
		    strcasecmp("Content-Length", key) == 0) {
			if (desc->http_method == HTTP_METHOD_TRACE ||
			    desc->http_method == HTTP_METHOD_CONNECT) {
				/*
				 * These method should not have a body
				 * and thus no Content-Length header.
				 */
				server_abort_http(clt, 400, "malformed");
				goto abort;
			}

			/*
			 * Need to read data from the client after the
			 * HTTP header.
			 * XXX What about non-standard clients not using
			 * the carriage return? And some browsers seem to
			 * include the line length in the content-length.
			 */
			clt->clt_toread = strtonum(value, 0, LLONG_MAX,
			    &errstr);
			if (errstr) {
				server_abort_http(clt, 500, errstr);
				goto abort;
			}
			if ((size_t)clt->clt_toread >
			    srv_conf->maxrequestbody) {
				server_abort_http(clt, 413, NULL);
				goto abort;
			}
		}

		if (strcasecmp("Transfer-Encoding", key) == 0 &&
		    strcasecmp("chunked", value) == 0)
			desc->http_chunked = 1;

		if (clt->clt_line != 1) {
			if ((hdr = kv_add(&desc->http_headers, key,
			    value)) == NULL)
				goto fail;

			desc->http_lastheader = hdr;
		}

		free(line);
	}
	if (clt->clt_done) {
		if (desc->http_method == HTTP_METHOD_NONE) {
			server_abort_http(clt, 406, "no method");
			return;
		}

		switch (desc->http_method) {
		case HTTP_METHOD_CONNECT:
			/* Data stream */
			clt->clt_toread = TOREAD_UNLIMITED;
			bev->readcb = server_read;
			break;
		case HTTP_METHOD_DELETE:
		case HTTP_METHOD_GET:
		case HTTP_METHOD_HEAD:
		case HTTP_METHOD_OPTIONS:
		/* WebDAV methods */
		case HTTP_METHOD_COPY:
			clt->clt_toread = 0;
			break;
		case HTTP_METHOD_POST:
		case HTTP_METHOD_PUT:
		case HTTP_METHOD_RESPONSE:
		/* WebDAV methods */
		case HTTP_METHOD_PROPFIND:
		case HTTP_METHOD_PROPPATCH:
		case HTTP_METHOD_MKCOL:
		case HTTP_METHOD_LOCK:
		case HTTP_METHOD_UNLOCK:
		case HTTP_METHOD_VERSION_CONTROL:
		case HTTP_METHOD_REPORT:
		case HTTP_METHOD_CHECKOUT:
		case HTTP_METHOD_CHECKIN:
		case HTTP_METHOD_UNCHECKOUT:
		case HTTP_METHOD_MKWORKSPACE:
		case HTTP_METHOD_UPDATE:
		case HTTP_METHOD_LABEL:
		case HTTP_METHOD_MERGE:
		case HTTP_METHOD_BASELINE_CONTROL:
		case HTTP_METHOD_MKACTIVITY:
		case HTTP_METHOD_ORDERPATCH:
		case HTTP_METHOD_ACL:
		case HTTP_METHOD_MKREDIRECTREF:
		case HTTP_METHOD_UPDATEREDIRECTREF:
		case HTTP_METHOD_SEARCH:
		case HTTP_METHOD_PATCH:
			/* HTTP request payload */
			if (clt->clt_toread > 0)
				bev->readcb = server_read_httpcontent;

			/* Single-pass HTTP body */
			if (clt->clt_toread < 0) {
				clt->clt_toread = TOREAD_UNLIMITED;
				bev->readcb = server_read;
			}
			break;
		default:
			server_abort_http(clt, 405, "method not allowed");
			return;
		}
		if (desc->http_chunked) {
			/* Chunked transfer encoding */
			clt->clt_toread = TOREAD_HTTP_CHUNK_LENGTH;
			bev->readcb = server_read_httpchunks;
		}

 done:
		if (clt->clt_toread != 0)
			bufferevent_disable(bev, EV_READ);
		server_response(env, clt);
		return;
	}
	if (clt->clt_done) {
		server_close(clt, "done");
		return;
	}
	if (EVBUFFER_LENGTH(src) && bev->readcb != server_read_http)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 fail:
	server_abort_http(clt, 500, strerror(errno));
 abort:
	free(line);
}

void
server_read_httpcontent(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	size_t			 size;

	getmonotime(&clt->clt_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: session %d: size %lu, to read %lld", __func__,
	    clt->clt_id, size, clt->clt_toread);
	if (!size)
		return;

	if (clt->clt_toread > 0) {
		/* Read content data */
		if ((off_t)size > clt->clt_toread) {
			size = clt->clt_toread;
			if (fcgi_add_stdin(clt, src) == -1)
				goto fail;
			clt->clt_toread = 0;
		} else {
			if (fcgi_add_stdin(clt, src) == -1)
				goto fail;
			clt->clt_toread -= size;
		}
		DPRINTF("%s: done, size %lu, to read %lld", __func__,
		    size, clt->clt_toread);
	}
	if (clt->clt_toread == 0) {
		fcgi_add_stdin(clt, NULL);
		clt->clt_toread = TOREAD_HTTP_HEADER;
		bufferevent_disable(bev, EV_READ);
		bev->readcb = server_read_http;
		return;
	}
	if (clt->clt_done)
		goto done;
	if (bev->readcb != server_read_httpcontent)
		bev->readcb(bev, arg);

	return;
 done:
	return;
 fail:
	server_close(clt, strerror(errno));
}

void
server_read_httpchunks(struct bufferevent *bev, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	char			*line;
	long long		 llval;
	size_t			 size;

	getmonotime(&clt->clt_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: session %d: size %lu, to read %lld", __func__,
	    clt->clt_id, size, clt->clt_toread);
	if (!size)
		return;

	if (clt->clt_toread > 0) {
		/* Read chunk data */
		if ((off_t)size > clt->clt_toread) {
			size = clt->clt_toread;
			if (server_bufferevent_write_chunk(clt, src, size)
			    == -1)
				goto fail;
			clt->clt_toread = 0;
		} else {
			if (server_bufferevent_write_buffer(clt, src) == -1)
				goto fail;
			clt->clt_toread -= size;
		}
		DPRINTF("%s: done, size %lu, to read %lld", __func__,
		    size, clt->clt_toread);
	}
	switch (clt->clt_toread) {
	case TOREAD_HTTP_CHUNK_LENGTH:
		line = evbuffer_readline(src);
		if (line == NULL) {
			/* Ignore empty line, continue */
			bufferevent_enable(bev, EV_READ);
			return;
		}
		if (strlen(line) == 0) {
			free(line);
			goto next;
		}

		/*
		 * Read prepended chunk size in hex, ignore the trailer.
		 * The returned signed value must not be negative.
		 */
		if (sscanf(line, "%llx", &llval) != 1 || llval < 0) {
			free(line);
			server_close(clt, "invalid chunk size");
			return;
		}

		if (server_bufferevent_print(clt, line) == -1 ||
		    server_bufferevent_print(clt, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		free(line);

		if ((clt->clt_toread = llval) == 0) {
			DPRINTF("%s: last chunk", __func__);
			clt->clt_toread = TOREAD_HTTP_CHUNK_TRAILER;
		}
		break;
	case TOREAD_HTTP_CHUNK_TRAILER:
		/* Last chunk is 0 bytes followed by trailer and empty line */
		line = evbuffer_readline(src);
		if (line == NULL) {
			/* Ignore empty line, continue */
			bufferevent_enable(bev, EV_READ);
			return;
		}
		if (server_bufferevent_print(clt, line) == -1 ||
		    server_bufferevent_print(clt, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		if (strlen(line) == 0) {
			/* Switch to HTTP header mode */
			clt->clt_toread = TOREAD_HTTP_HEADER;
			bev->readcb = server_read_http;
		}
		free(line);
		break;
	case 0:
		/* Chunk is terminated by an empty newline */
		line = evbuffer_readline(src);
		if (line != NULL)
			free(line);
		if (server_bufferevent_print(clt, "\r\n") == -1)
			goto fail;
		clt->clt_toread = TOREAD_HTTP_CHUNK_LENGTH;
		break;
	}

 next:
	if (clt->clt_done)
		goto done;
	if (EVBUFFER_LENGTH(src))
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;

 done:
	server_close(clt, "last http chunk read (done)");
	return;
 fail:
	server_close(clt, strerror(errno));
}

void
server_reset_http(struct client *clt)
{
	struct server		*srv = clt->clt_srv;

	server_httpdesc_free(clt->clt_descreq);
	server_httpdesc_free(clt->clt_descresp);
	clt->clt_headerlen = 0;
	clt->clt_line = 0;
	clt->clt_done = 0;
	clt->clt_chunk = 0;
	free(clt->clt_remote_user);
	clt->clt_remote_user = NULL;
	clt->clt_bev->readcb = server_read_http;
	clt->clt_srv_conf = &srv->srv_conf;

	server_log(clt, NULL);
}

ssize_t
server_http_time(time_t t, char *tmbuf, size_t len)
{
	struct tm		 tm;

	/* New HTTP/1.1 RFC 7231 prefers IMF-fixdate from RFC 5322 */
	if (t == -1 || gmtime_r(&t, &tm) == NULL)
		return (-1);
	else
		return (strftime(tmbuf, len, "%a, %d %h %Y %T %Z", &tm));
}

const char *
server_http_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	char		hbuf[HOST_NAME_MAX+1];
	in_port_t	port;

	if (print_host(ss, buf, len) == NULL)
		return (NULL);

	port = ntohs(server_socket_getport(ss));
	if (port == HTTP_PORT)
		return (buf);

	switch (ss->ss_family) {
	case AF_INET:
		if ((size_t)snprintf(hbuf, sizeof(hbuf),
		    "%s:%u", buf, port) >= sizeof(hbuf))
			return (NULL);
		break;
	case AF_INET6:
		if ((size_t)snprintf(hbuf, sizeof(hbuf),
		    "[%s]:%u", buf, port) >= sizeof(hbuf))
			return (NULL);
		break;
	}

	if (strlcpy(buf, hbuf, len) >= len)
		return (NULL);

	return (buf);
}

char *
server_http_parsehost(char *host, char *buf, size_t len, int *portval)
{
	char		*start, *end, *port;
	const char	*errstr = NULL;

	if (strlcpy(buf, host, len) >= len) {
		log_debug("%s: host name too long", __func__);
		return (NULL);
	}

	start = buf;
	end = port = NULL;

	if (*start == '[' && (end = strchr(start, ']')) != NULL) {
		/* Address enclosed in [] with port, eg. [2001:db8::1]:80 */
		start++;
		*end++ = '\0';
		if ((port = strchr(end, ':')) == NULL || *port == '\0')
			port = NULL;
		else
			port++;
		memmove(buf, start, strlen(start) + 1);
	} else if ((end = strchr(start, ':')) != NULL) {
		/* Name or address with port, eg. www.example.com:80 */
		*end++ = '\0';
		port = end;
	} else {
		/* Name or address with default port, eg. www.example.com */
		port = NULL;
	}

	if (port != NULL) {
		/* Save the requested port */
		*portval = strtonum(port, 0, 0xffff, &errstr);
		if (errstr != NULL) {
			log_debug("%s: invalid port: %s", __func__,
			    strerror(errno));
			return (NULL);
		}
		*portval = htons(*portval);
	} else {
		/* Port not given, indicate the default port */
		*portval = -1;
	}

	return (start);
}

void
server_abort_http(struct client *clt, u_int code, const char *msg)
{
	struct server		*srv = clt->clt_srv;
	struct server_config	*srv_conf = &srv->srv_conf;
	struct bufferevent	*bev = clt->clt_bev;
	struct http_descriptor	*desc = clt->clt_descreq;
	const char		*httperr = NULL, *style;
	char			*httpmsg, *body = NULL, *extraheader = NULL;
	char			 tmbuf[32], hbuf[128];
	int			 bodylen;

	if ((httperr = server_httperror_byid(code)) == NULL)
		httperr = "Unknown Error";

	if (bev == NULL)
		goto done;

	if (server_log_http(clt, code, 0) == -1)
		goto done;

	/* Some system information */
	if (print_host(&srv_conf->ss, hbuf, sizeof(hbuf)) == NULL)
		goto done;

	if (server_http_time(time(NULL), tmbuf, sizeof(tmbuf)) <= 0)
		goto done;

	/* Do not send details of the Internal Server Error */
	switch (code) {
	case 301:
	case 302:
		if (asprintf(&extraheader, "Location: %s\r\n", msg) == -1) {
			code = 500;
			extraheader = NULL;
		}
		break;
	case 401:
		if (asprintf(&extraheader,
		    "WWW-Authenticate: Basic realm=\"%s\"\r\n", msg) == -1) {
			code = 500;
			extraheader = NULL;
		}
		break;
	default:
		/*
		 * Do not send details of the error.  Traditionally,
		 * web servers responsed with the request path on 40x
		 * errors which could be abused to inject JavaScript etc.
		 * Instead of sanitizing the path here, we just don't
		 * reprint it.
		 */
		break;
	}

	/* A CSS stylesheet allows minimal customization by the user */
	style = "body { background-color: white; color: black; font-family: "
	    "'Comic Sans MS', 'Chalkboard SE', 'Comic Neue', sans-serif; }\n"
	    "hr { border: 0; border-bottom: 1px dashed; }\n";

	/* Generate simple HTML error document */
	if ((bodylen = asprintf(&body,
	    "<!DOCTYPE html>\n"
	    "<html>\n"
	    "<head>\n"
	    "<title>%03d %s</title>\n"
	    "<style type=\"text/css\"><!--\n%s\n--></style>\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>%03d %s</h1>\n"
	    "<hr>\n<address>%s</address>\n"
	    "</body>\n"
	    "</html>\n",
	    code, httperr, style, code, httperr, HTTPD_SERVERNAME)) == -1)
		goto done;

	/* Add basic HTTP headers */
	if (asprintf(&httpmsg,
	    "HTTP/1.0 %03d %s\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Connection: close\r\n"
	    "Content-Type: text/html\r\n"
	    "Content-Length: %d\r\n"
	    "%s"
	    "\r\n"
	    "%s",
	    code, httperr, tmbuf, HTTPD_SERVERNAME, bodylen,
	    extraheader == NULL ? "" : extraheader,
	    desc->http_method == HTTP_METHOD_HEAD ? "" : body) == -1)
		goto done;

	/* Dump the message without checking for success */
	server_dump(clt, httpmsg, strlen(httpmsg));
	free(httpmsg);

 done:
	free(body);
	free(extraheader);
	if (asprintf(&httpmsg, "%s (%03d %s)", msg, code, httperr) == -1) {
		server_close(clt, msg);
	} else {
		server_close(clt, httpmsg);
		free(httpmsg);
	}
}

void
server_close_http(struct client *clt)
{
	struct http_descriptor *desc;

	desc = clt->clt_descreq;
	server_httpdesc_free(desc);
	free(desc);
	clt->clt_descreq = NULL;

	desc = clt->clt_descresp;
	server_httpdesc_free(desc);
	free(desc);
	clt->clt_descresp = NULL;
	free(clt->clt_remote_user);
	clt->clt_remote_user = NULL;
}

int
server_response(struct httpd *httpd, struct client *clt)
{
	char			 path[PATH_MAX];
	char			 hostname[HOST_NAME_MAX+1];
	struct http_descriptor	*desc = clt->clt_descreq;
	struct http_descriptor	*resp = clt->clt_descresp;
	struct server		*srv = clt->clt_srv;
	struct server_config	*srv_conf = &srv->srv_conf;
	struct kv		*kv, key, *host;
	int			 portval = -1;
	char			*hostval;

	/* Canonicalize the request path */
	if (desc->http_path == NULL ||
	    url_decode(desc->http_path) == NULL ||
	    canonicalize_path(desc->http_path, path, sizeof(path)) == NULL)
		goto fail;
	free(desc->http_path);
	if ((desc->http_path = strdup(path)) == NULL)
		goto fail;

	key.kv_key = "Host";
	if ((host = kv_find(&desc->http_headers, &key)) != NULL &&
	    host->kv_value == NULL)
		host = NULL;

	if (strcmp(desc->http_version, "HTTP/1.1") == 0) {
		/* Host header is mandatory */
		if (host == NULL)
			goto fail;

		/* Is the connection persistent? */
		key.kv_key = "Connection";
		if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
		    strcasecmp("close", kv->kv_value) == 0)
			clt->clt_persist = 0;
		else
			clt->clt_persist++;
	} else {
		/* Is the connection persistent? */
		key.kv_key = "Connection";
		if ((kv = kv_find(&desc->http_headers, &key)) != NULL &&
		    strcasecmp("keep-alive", kv->kv_value) == 0)
			clt->clt_persist++;
		else
			clt->clt_persist = 0;
	}

	if (clt->clt_persist >= srv_conf->maxrequests)
		clt->clt_persist = 0;

	/*
	 * Do we have a Host header and matching configuration?
	 * XXX the Host can also appear in the URL path.
	 */
	if (host != NULL) {
		if ((hostval = server_http_parsehost(host->kv_value,
		    hostname, sizeof(hostname), &portval)) == NULL)
			goto fail;

		TAILQ_FOREACH(srv_conf, &srv->srv_hosts, entry) {
#ifdef DEBUG
			if ((srv_conf->flags & SRVFLAG_LOCATION) == 0) {
				DPRINTF("%s: virtual host \"%s:%u\""
				    " host \"%s\" (\"%s\")",
				    __func__, srv_conf->name,
				    ntohs(srv_conf->port), host->kv_value,
				    hostname);
			}
#endif
			if ((srv_conf->flags & SRVFLAG_LOCATION) == 0 &&
			    fnmatch(srv_conf->name, hostname,
			    FNM_CASEFOLD) == 0 &&
			    (portval == -1 ||
			    (portval != -1 && portval == srv_conf->port))) {
				/* Replace host configuration */
				clt->clt_srv_conf = srv_conf;
				srv_conf = NULL;
				break;
			}
		}
	}

	if (srv_conf != NULL) {
		/* Use the actual server IP address */
		if (server_http_host(&clt->clt_srv_ss, hostname,
		    sizeof(hostname)) == NULL)
			goto fail;
	} else {
		/* Host header was valid and found */
		if (strlcpy(hostname, host->kv_value, sizeof(hostname)) >=
		    sizeof(hostname))
			goto fail;
		srv_conf = clt->clt_srv_conf;
	}

	if ((desc->http_host = strdup(hostname)) == NULL)
		goto fail;

	/* Now fill in the mandatory parts of the response descriptor */
	resp->http_method = desc->http_method;
	if ((resp->http_version = strdup(desc->http_version)) == NULL)
		goto fail;

	/* Now search for the location */
	srv_conf = server_getlocation(clt, desc->http_path);

	if (srv_conf->flags & SRVFLAG_AUTH &&
	    server_http_authenticate(srv_conf, clt) == -1) {
		server_abort_http(clt, 401, srv_conf->auth_realm);
		return (-1);
	} else
		return (server_file(httpd, clt));
 fail:
	server_abort_http(clt, 400, "bad request");
	return (-1);
}

const char *
server_root_strip(const char *path, int n)
{
	const char *p;

	/* Strip strip leading directories. Leading '/' is ignored. */
	for (; n > 0 && *path != '\0'; n--)
		if ((p = strchr(++path, '/')) == NULL)
			path = strchr(path, '\0');
		else
			path = p;

	return (path);
}

struct server_config *
server_getlocation(struct client *clt, const char *path)
{
	struct server		*srv = clt->clt_srv;
	struct server_config	*srv_conf = clt->clt_srv_conf, *location;

	/* Now search for the location */
	TAILQ_FOREACH(location, &srv->srv_hosts, entry) {
#ifdef DEBUG
		if (location->flags & SRVFLAG_LOCATION) {
			DPRINTF("%s: location \"%s\" path \"%s\"",
			    __func__, location->location, path);
		}
#endif
		if ((location->flags & SRVFLAG_LOCATION) &&
		    location->id == srv_conf->id &&
		    fnmatch(location->location, path, FNM_CASEFOLD) == 0) {
			/* Replace host configuration */
			clt->clt_srv_conf = srv_conf = location;
			break;
		}
	}

	return (srv_conf);
}

int
server_response_http(struct client *clt, u_int code,
    struct media_type *media, size_t size, time_t mtime)
{
	struct http_descriptor	*desc = clt->clt_descreq;
	struct http_descriptor	*resp = clt->clt_descresp;
	const char		*error;
	struct kv		*ct, *cl;
	char			 tmbuf[32];

	if (desc == NULL || (error = server_httperror_byid(code)) == NULL)
		return (-1);

	if (server_log_http(clt, code, size) == -1)
		return (-1);

	/* Add error codes */
	if (kv_setkey(&resp->http_pathquery, "%lu", code) == -1 ||
	    kv_set(&resp->http_pathquery, "%s", error) == -1)
		return (-1);

	/* Add headers */
	if (kv_add(&resp->http_headers, "Server", HTTPD_SERVERNAME) == NULL)
		return (-1);

	/* Is it a persistent connection? */
	if (clt->clt_persist) {
		if (kv_add(&resp->http_headers,
		    "Connection", "keep-alive") == NULL)
			return (-1);
	} else if (kv_add(&resp->http_headers, "Connection", "close") == NULL)
		return (-1);

	/* Set media type */
	if ((ct = kv_add(&resp->http_headers, "Content-Type", NULL)) == NULL ||
	    kv_set(ct, "%s/%s",
	    media == NULL ? "application" : media->media_type,
	    media == NULL ? "octet-stream" : media->media_subtype) == -1)
		return (-1);

	/* Set content length, if specified */
	if ((cl =
	    kv_add(&resp->http_headers, "Content-Length", NULL)) == NULL ||
	    kv_set(cl, "%ld", size) == -1)
		return (-1);

	/* Set last modification time */
	if (server_http_time(mtime, tmbuf, sizeof(tmbuf)) <= 0 ||
	    kv_add(&resp->http_headers, "Last-Modified", tmbuf) == NULL)
		return (-1);

	/* Date header is mandatory and should be added as late as possible */
	if (server_http_time(time(NULL), tmbuf, sizeof(tmbuf)) <= 0 ||
	    kv_add(&resp->http_headers, "Date", tmbuf) == NULL)
		return (-1);

	/* Write completed header */
	if (server_writeresponse_http(clt) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1 ||
	    server_headers(clt, resp, server_writeheader_http, NULL) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1)
		return (-1);

	if (size == 0 || resp->http_method == HTTP_METHOD_HEAD) {
		bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
		if (clt->clt_persist)
			clt->clt_toread = TOREAD_HTTP_HEADER;
		else
			clt->clt_toread = TOREAD_HTTP_NONE;
		clt->clt_done = 0;
		return (0);
	}

	return (1);
}

int
server_writeresponse_http(struct client *clt)
{
	struct http_descriptor	*desc = clt->clt_descresp;

	DPRINTF("version: %s rescode: %s resmsg: %s", desc->http_version,
	    desc->http_rescode, desc->http_resmesg);

	if (server_bufferevent_print(clt, desc->http_version) == -1 ||
	    server_bufferevent_print(clt, " ") == -1 ||
	    server_bufferevent_print(clt, desc->http_rescode) == -1 ||
	    server_bufferevent_print(clt, " ") == -1 ||
	    server_bufferevent_print(clt, desc->http_resmesg) == -1)
		return (-1);

	return (0);
}

int
server_writeheader_http(struct client *clt, struct kv *hdr, void *arg)
{
	char			*ptr;
	const char		*key;

	if (hdr->kv_flags & KV_FLAG_INVALID)
		return (0);

	/* The key might have been updated in the parent */
	if (hdr->kv_parent != NULL && hdr->kv_parent->kv_key != NULL)
		key = hdr->kv_parent->kv_key;
	else
		key = hdr->kv_key;

	ptr = hdr->kv_value;
	if (server_bufferevent_print(clt, key) == -1 ||
	    (ptr != NULL &&
	    (server_bufferevent_print(clt, ": ") == -1 ||
	    server_bufferevent_print(clt, ptr) == -1 ||
	    server_bufferevent_print(clt, "\r\n") == -1)))
		return (-1);
	DPRINTF("%s: %s: %s", __func__, key,
	    hdr->kv_value == NULL ? "" : hdr->kv_value);

	return (0);
}

int
server_headers(struct client *clt, void *descp,
    int (*hdr_cb)(struct client *, struct kv *, void *), void *arg)
{
	struct kv		*hdr, *kv;
	struct http_descriptor	*desc = descp;

	RB_FOREACH(hdr, kvtree, &desc->http_headers) {
		if ((hdr_cb)(clt, hdr, arg) == -1)
			return (-1);
		TAILQ_FOREACH(kv, &hdr->kv_children, kv_entry) {
			if ((hdr_cb)(clt, kv, arg) == -1)
				return (-1);
		}
	}

	return (0);
}

enum httpmethod
server_httpmethod_byname(const char *name)
{
	enum httpmethod		 id = HTTP_METHOD_NONE;
	struct http_method	 method, *res = NULL;

	/* Set up key */
	method.method_name = name;

	if ((res = bsearch(&method, http_methods,
	    sizeof(http_methods) / sizeof(http_methods[0]) - 1,
	    sizeof(http_methods[0]), server_httpmethod_cmp)) != NULL)
		id = res->method_id;

	return (id);
}

const char *
server_httpmethod_byid(u_int id)
{
	const char	*name = "<UNKNOWN>";
	int		 i;

	for (i = 0; http_methods[i].method_name != NULL; i++) {
		if (http_methods[i].method_id == id) {
			name = http_methods[i].method_name;
			break;
		}
	}

	return (name);
}

static int
server_httpmethod_cmp(const void *a, const void *b)
{
	const struct http_method *ma = a;
	const struct http_method *mb = b;

	/*
	 * RFC 2616 section 5.1.1 says that the method is case
	 * sensitive so we don't do a strcasecmp here.
	 */
	return (strcmp(ma->method_name, mb->method_name));
}

const char *
server_httperror_byid(u_int id)
{
	struct http_error	 error, *res;

	/* Set up key */
	error.error_code = (int)id;

	if ((res = bsearch(&error, http_errors,
	    sizeof(http_errors) / sizeof(http_errors[0]) - 1,
	    sizeof(http_errors[0]), server_httperror_cmp)) != NULL)
		return (res->error_name);

	return (NULL);
}

static int
server_httperror_cmp(const void *a, const void *b)
{
	const struct http_error *ea = a;
	const struct http_error *eb = b;
	return (ea->error_code - eb->error_code);
}

int
server_log_http(struct client *clt, u_int code, size_t len)
{
	static char		 tstamp[64];
	static char		 ip[INET6_ADDRSTRLEN];
	time_t			 t;
	struct kv		 key, *agent, *referrer;
	struct tm		*tm;
	struct server_config	*srv_conf;
	struct http_descriptor	*desc;

	if ((srv_conf = clt->clt_srv_conf) == NULL)
		return (-1);
	if ((srv_conf->flags & SRVFLAG_LOG) == 0)
		return (0);
	if ((desc = clt->clt_descreq) == NULL)
		return (-1);

	if ((t = time(NULL)) == -1)
		return (-1);
	if ((tm = localtime(&t)) == NULL)
		return (-1);
	if (strftime(tstamp, sizeof(tstamp), "%d/%b/%Y:%H:%M:%S %z", tm) == 0)
		return (-1);

	if (print_host(&clt->clt_ss, ip, sizeof(ip)) == NULL)
		return (-1);

	/*
	 * For details on common log format, see:
	 * https://httpd.apache.org/docs/current/mod/mod_log_config.html
	 *
	 * httpd's format is similar to these Apache LogFormats:
	 * "%v %h %l %u %t \"%r\" %>s %B"
	 * "%v %h %l %u %t \"%r\" %>s %B \"%{Referer}i\" \"%{User-agent}i\""
	 */
	switch (srv_conf->logformat) {
	case LOG_FORMAT_COMMON:
		if (evbuffer_add_printf(clt->clt_log,
		    "%s %s - %s [%s] \"%s %s%s%s%s%s\" %03d %zu\n",
		    srv_conf->name, ip, clt->clt_remote_user == NULL ? "-" :
		    clt->clt_remote_user, tstamp,
		    server_httpmethod_byid(desc->http_method),
		    desc->http_path == NULL ? "" : desc->http_path,
		    desc->http_query == NULL ? "" : "?",
		    desc->http_query == NULL ? "" : desc->http_query,
		    desc->http_version == NULL ? "" : " ",
		    desc->http_version == NULL ? "" : desc->http_version,
		    code, len) == -1)
			return (-1);
		break;

	case LOG_FORMAT_COMBINED:
		key.kv_key = "Referer"; /* sic */
		if ((referrer = kv_find(&desc->http_headers, &key)) != NULL &&
		    referrer->kv_value == NULL)
			referrer = NULL;

		key.kv_key = "User-Agent";
		if ((agent = kv_find(&desc->http_headers, &key)) != NULL &&
		    agent->kv_value == NULL)
			agent = NULL;

		if (evbuffer_add_printf(clt->clt_log,
		    "%s %s - %s [%s] \"%s %s%s%s%s%s\" %03d %zu \"%s\" \"%s\"\n",
		    srv_conf->name, ip, clt->clt_remote_user == NULL ? "-" :
		    clt->clt_remote_user, tstamp,
		    server_httpmethod_byid(desc->http_method),
		    desc->http_path == NULL ? "" : desc->http_path,
		    desc->http_query == NULL ? "" : "?",
		    desc->http_query == NULL ? "" : desc->http_query,
		    desc->http_version == NULL ? "" : " ",
		    desc->http_version == NULL ? "" : desc->http_version,
		    code, len,
		    referrer == NULL ? "" : referrer->kv_value,
		    agent == NULL ? "" : agent->kv_value) == -1)
			return (-1);
		break;

	case LOG_FORMAT_CONNECTION:
		if (evbuffer_add_printf(clt->clt_log, " [%s]",
		    desc->http_path == NULL ? "" : desc->http_path) == -1)
			return (-1);
		break;
	}

	return (0);
}

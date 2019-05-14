/*	$OpenBSD: http.c,v 1.7 2019/05/14 02:30:00 sunil Exp $ */

/*
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@openbsd.org>
 * Copyright (c) 2012 - 2015 Reyk Floeter <reyk@openbsd.org>
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

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifndef NOSSL
#include <tls.h>
#endif

#include "ftp.h"
#include "xmalloc.h"

#define MAX_REDIRECTS	10

#ifndef NOSSL
#define MINBUF		128

static struct tls_config	*tls_config;
static struct tls		*ctx;
static int			 tls_session_fd = -1;
static char * const		 tls_verify_opts[] = {
#define HTTP_TLS_CAFILE		0
	"cafile",
#define HTTP_TLS_CAPATH		1
	"capath",
#define HTTP_TLS_CIPHERS	2
	"ciphers",
#define HTTP_TLS_DONTVERIFY	3
	"dont",
#define HTTP_TLS_VERIFYDEPTH	4
	"depth",
#define HTTP_TLS_MUSTSTAPLE	5
	"muststaple",
#define HTTP_TLS_NOVERIFYTIME	6
	"noverifytime",
#define HTTP_TLS_SESSION	7
	"session",
#define HTTP_TLS_DOVERIFY	8
	"do",
	NULL
};
#endif /* NOSSL */

/*
 * HTTP status codes based on IANA assignments (2014-06-11 version):
 * https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
 * plus legacy (306) and non-standard (420).
 */
static struct http_status {
	int		 code;
	const char	*name;
} http_status[] = {
	{ 100,	"Continue" },
	{ 101,	"Switching Protocols" },
	{ 102,	"Processing" },
	/* 103-199 unassigned */
	{ 200,	"OK" },
	{ 201,	"Created" },
	{ 202,	"Accepted" },
	{ 203,	"Non-Authoritative Information" },
	{ 204,	"No Content" },
	{ 205,	"Reset Content" },
	{ 206,	"Partial Content" },
	{ 207,	"Multi-Status" },
	{ 208,	"Already Reported" },
	/* 209-225 unassigned */
	{ 226,	"IM Used" },
	/* 227-299 unassigned */
	{ 300,	"Multiple Choices" },
	{ 301,	"Moved Permanently" },
	{ 302,	"Found" },
	{ 303,	"See Other" },
	{ 304,	"Not Modified" },
	{ 305,	"Use Proxy" },
	{ 306,	"Switch Proxy" },
	{ 307,	"Temporary Redirect" },
	{ 308,	"Permanent Redirect" },
	/* 309-399 unassigned */
	{ 400,	"Bad Request" },
	{ 401,	"Unauthorized" },
	{ 402,	"Payment Required" },
	{ 403,	"Forbidden" },
	{ 404,	"Not Found" },
	{ 405,	"Method Not Allowed" },
	{ 406,	"Not Acceptable" },
	{ 407,	"Proxy Authentication Required" },
	{ 408,	"Request Timeout" },
	{ 409,	"Conflict" },
	{ 410,	"Gone" },
	{ 411,	"Length Required" },
	{ 412,	"Precondition Failed" },
	{ 413,	"Payload Too Large" },
	{ 414,	"URI Too Long" },
	{ 415,	"Unsupported Media Type" },
	{ 416,	"Range Not Satisfiable" },
	{ 417,	"Expectation Failed" },
	{ 418,	"I'm a teapot" },
	/* 419-421 unassigned */
	{ 420,	"Enhance Your Calm" },
	{ 422,	"Unprocessable Entity" },
	{ 423,	"Locked" },
	{ 424,	"Failed Dependency" },
	/* 425 unassigned */
	{ 426,	"Upgrade Required" },
	/* 427 unassigned */
	{ 428,	"Precondition Required" },
	{ 429,	"Too Many Requests" },
	/* 430 unassigned */
	{ 431,	"Request Header Fields Too Large" },
	/* 432-450 unassigned */
	{ 451,	"Unavailable For Legal Reasons" },
	/* 452-499 unassigned */
	{ 500,	"Internal Server Error" },
	{ 501,	"Not Implemented" },
	{ 502,	"Bad Gateway" },
	{ 503,	"Service Unavailable" },
	{ 504,	"Gateway Timeout" },
	{ 505,	"HTTP Version Not Supported" },
	{ 506,	"Variant Also Negotiates" },
	{ 507,	"Insufficient Storage" },
	{ 508,	"Loop Detected" },
	/* 509 unassigned */
	{ 510,	"Not Extended" },
	{ 511,	"Network Authentication Required" },
	/* 512-599 unassigned */
	{ 0,	NULL },
	};

struct http_headers {
	char	*location;
	off_t	 content_length;
	int	 chunked;
};

static void		 decode_chunk(int, uint, FILE *);
static char		*header_lookup(const char *, const char *);
static const char	*http_error(int);
static void		 http_headers_free(struct http_headers *);
static ssize_t		 http_getline(int, char **, size_t *);
static size_t		 http_read(int, char *, size_t);
static struct url	*http_redirect(struct url *, char *);
static void		 http_save_chunks(struct url *, FILE *, off_t *);
static int		 http_status_cmp(const void *, const void *);
static int		 http_request(int, const char *,
			    struct http_headers **);
static char		*relative_path_resolve(const char *, const char *);

#ifndef NOSSL
static void		 tls_copy_file(struct url *, FILE *, off_t *);
static ssize_t		 tls_getline(char **, size_t *, struct tls *);
#endif

static FILE	*fp;

void
http_connect(struct url *url, struct url *proxy, int timeout)
{
	const char	*host, *port;
	int		 sock;

	host = proxy ? proxy->host : url->host;
	port = proxy ? proxy->port : url->port;
	if ((sock = tcp_connect(host, port, timeout)) == -1)
		exit(1);

	if ((fp = fdopen(sock, "r+")) == NULL)
		err(1, "%s: fdopen", __func__);

#ifndef NOSSL
	struct http_headers	*headers;
	char			*auth = NULL, *req;
	int			 authlen = 0, code;

	if (url->scheme != S_HTTPS)
		return;

	if (proxy) {
		if (url->basic_auth)
			authlen = xasprintf(&auth,
			    "Proxy-Authorization: Basic %s\r\n",
			    url->basic_auth);

		xasprintf(&req,
		    "CONNECT %s:%s HTTP/1.0\r\n"
		    "User-Agent: %s\r\n"
		    "%s"
		    "\r\n",
		    url->host, url->port,
		    useragent,
		    url->basic_auth ? auth : "");

		freezero(auth, authlen);
		if ((code = http_request(S_HTTP, req, &headers)) != 200)
			errx(1, "%s: failed to CONNECT to %s:%s: %s",
			    __func__, url->host, url->port, http_error(code));

		free(req);
		http_headers_free(headers);
	}

	if ((ctx = tls_client()) == NULL)
		errx(1, "failed to create tls client");

	if (tls_configure(ctx, tls_config) != 0)
		errx(1, "%s: %s", __func__, tls_error(ctx));

	if (tls_connect_socket(ctx, sock, url->host) != 0)
		errx(1, "%s: %s", __func__, tls_error(ctx));
#endif /* NOSSL */
}

struct url *
http_get(struct url *url, struct url *proxy, off_t *offset, off_t *sz)
{
	struct http_headers	*headers;
	char			*auth = NULL, *path = NULL, *range = NULL, *req;
	int			 authlen = 0, code, redirects = 0;

 redirected:
	log_request("Requesting", url, proxy);
	if (*offset)
		xasprintf(&range, "Range: bytes=%lld-\r\n", *offset);

	if (url->basic_auth)
		authlen = xasprintf(&auth, "Authorization: Basic %s\r\n",
		    url->basic_auth);

	if (proxy && url->scheme != S_HTTPS)
		path = url_str(url);
	else if (url->path)
		path = url_encode(url->path);

	xasprintf(&req,
	    "GET %s HTTP/1.1\r\n"
	    "Host: %s\r\n"
	    "%s"
	    "%s"
	    "Connection: close\r\n"
	    "User-Agent: %s\r\n"
	    "\r\n",
	    path ? path : "/",
	    url->host,
	    *offset ? range : "",
	    url->basic_auth ? auth : "",
	    useragent);
	code = http_request(url->scheme, req, &headers);
	freezero(auth, authlen);
	auth = NULL;
	authlen = 0;
	free(range);
	range = NULL;
	free(path);
	path = NULL;
	free(req);
	req = NULL;
	switch (code) {
	case 200:
		if (*offset) {
			warnx("Server does not support resume.");
			*offset = 0;
		}
		break;
	case 206:
		break;
	case 301:
	case 302:
	case 303:
	case 307:
		http_close(url);
		if (++redirects > MAX_REDIRECTS)
			errx(1, "Too many redirections requested");

		if (headers->location == NULL)
			errx(1, "%s: Location header missing", __func__);

		url = http_redirect(url, headers->location);
		http_headers_free(headers);
		log_request("Redirected to", url, proxy);
		http_connect(url, proxy, 0);
		goto redirected;
	case 416:
		errx(1, "File is already fully retrieved.");
		break;
	default:
		errx(1, "Error retrieving file: %d %s", code, http_error(code));
	}

	*sz = headers->content_length + *offset;
	url->chunked = headers->chunked;
	http_headers_free(headers);
	return url;
}

void
http_save(struct url *url, FILE *dst_fp, off_t *offset)
{
	if (url->chunked)
		http_save_chunks(url, dst_fp, offset);
#ifndef NOSSL
	else if (url->scheme == S_HTTPS)
		tls_copy_file(url, dst_fp, offset);
#endif
	else
		copy_file(dst_fp, fp, offset);
}

static struct url *
http_redirect(struct url *old_url, char *location)
{
	struct url	*new_url;

	/* absolute uri reference */
	if (strncasecmp(location, "http", 4) == 0 ||
	    strncasecmp(location, "https", 5) == 0) {
		if ((new_url = url_parse(location)) == NULL)
			exit(1);

		goto done;
	}

	/* relative uri reference */
	new_url = xcalloc(1, sizeof *new_url);
	new_url->scheme = old_url->scheme;
	new_url->host = xstrdup(old_url->host);
	new_url->port = xstrdup(old_url->port);

	/* absolute-path reference */
	if (location[0] == '/')
		new_url->path = xstrdup(location);
	else
		new_url->path = relative_path_resolve(old_url->path, location);

 done:
	new_url->fname = xstrdup(old_url->fname);
	url_free(old_url);
	return new_url;
}

static char *
relative_path_resolve(const char *base_path, const char *location)
{
	char	*new_path, *p;

	/* trim fragment component from both uri */
	if ((p = strchr(location, '#')) != NULL)
		*p = '\0';
	if (base_path && (p = strchr(base_path, '#')) != NULL)
		*p = '\0';

	if (base_path == NULL)
		xasprintf(&new_path, "/%s", location);
	else if (base_path[strlen(base_path) - 1] == '/')
		xasprintf(&new_path, "%s%s", base_path, location);
	else {
		p = dirname(base_path);
		xasprintf(&new_path, "%s/%s",
		    strcmp(p, ".") == 0 ? "" : p, location);
	}

	return new_path;
}

static void
http_save_chunks(struct url *url, FILE *dst_fp, off_t *offset)
{
	char	*buf = NULL;
	size_t	 n = 0;
	uint	 chunk_sz;

	http_getline(url->scheme, &buf, &n);
	if (sscanf(buf, "%x", &chunk_sz) != 1)
		errx(1, "%s: Failed to get chunk size", __func__);

	while (chunk_sz > 0) {
		decode_chunk(url->scheme, chunk_sz, dst_fp);
		*offset += chunk_sz;
		http_getline(url->scheme, &buf, &n);
		if (sscanf(buf, "%x", &chunk_sz) != 1)
			errx(1, "%s: Failed to get chunk size", __func__);
	}

	free(buf);
}

static void
decode_chunk(int scheme, uint sz, FILE *dst_fp)
{
	size_t	bufsz;
	size_t	r;
	char	buf[BUFSIZ], crlf[2];

	bufsz = sizeof(buf);
	while (sz > 0) {
		if (sz < bufsz)
			bufsz = sz;

		r = http_read(scheme, buf, bufsz);
		if (fwrite(buf, 1, r, dst_fp) != r)
			errx(1, "%s: fwrite", __func__);

		sz -= r;
	}

	/* CRLF terminating the chunk */
	if (http_read(scheme, crlf, sizeof(crlf)) != sizeof(crlf))
		errx(1, "%s: Failed to read terminal crlf", __func__);

	if (crlf[0] != '\r' || crlf[1] != '\n')
		errx(1, "%s: Invalid chunked encoding", __func__);
}

void
http_close(struct url *url)
{
#ifndef NOSSL
	ssize_t	r;

	if (url->scheme == S_HTTPS) {
		if (tls_session_fd != -1)
			dprintf(STDERR_FILENO, "tls session resumed: %s\n",
			    tls_conn_session_resumed(ctx) ? "yes" : "no");

		do {
			r = tls_close(ctx);
		} while (r == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT);
		tls_free(ctx);
	}

#endif
	fclose(fp);
}

static int
http_request(int scheme, const char *req, struct http_headers **hdrs)
{
	struct http_headers	*headers;
	const char		*e;
	char			*buf = NULL, *p;
	size_t			 n = 0;
	ssize_t			 buflen;
	uint			 code;
#ifndef NOSSL
	size_t			 len;
	ssize_t			 nw;
#endif

	if (io_debug)
		fprintf(stderr, "<<< %s", req);

	switch (scheme) {
#ifndef NOSSL
	case S_HTTPS:
		len = strlen(req);
		while (len > 0) {
			nw = tls_write(ctx, req, len);
			if (nw == TLS_WANT_POLLIN || nw == TLS_WANT_POLLOUT)
				continue;
			if (nw < 0)
				errx(1, "tls_write: %s", tls_error(ctx));
			req += nw;
			len -= nw;
		}
		break;
#endif
	case S_FTP:
	case S_HTTP:
		if (fprintf(fp, "%s", req) < 0)
			errx(1, "%s: fprintf", __func__);
		(void)fflush(fp);
		break;
	}

	http_getline(scheme, &buf, &n);
	if (io_debug)
		fprintf(stderr, ">>> %s", buf);

	if (sscanf(buf, "%*s %u %*s", &code) != 1)
		errx(1, "%s: failed to extract status code", __func__);

	if (code < 100 || code > 511)
		errx(1, "%s: invalid status code %d", __func__, code);

	headers = xcalloc(1, sizeof *headers);
	for (;;) {
		buflen = http_getline(scheme, &buf, &n);
		buflen -= 1;
		if (buflen > 0 && buf[buflen - 1] == '\r')
			buflen -= 1;
		buf[buflen] = '\0';

		if (io_debug)
			fprintf(stderr, ">>> %s\n", buf);

		if (buflen == 0)
			break; /* end of headers */

		if ((p = header_lookup(buf, "Content-Length:")) != NULL) {
			headers->content_length = strtonum(p, 0, INT64_MAX, &e);
			if (e)
				err(1, "%s: Content-Length is %s: %lld",
				    __func__, e, headers->content_length);
		}

		if ((p = header_lookup(buf, "Location:")) != NULL)
			headers->location = xstrdup(p);

		if ((p = header_lookup(buf, "Transfer-Encoding:")) != NULL)
			if (strcasestr(p, "chunked") != NULL)
				headers->chunked = 1;

	}

	*hdrs = headers;
	free(buf);
	return code;
}

static void
http_headers_free(struct http_headers *headers)
{
	if (headers == NULL)
		return;

	free(headers->location);
	free(headers);
}

static char *
header_lookup(const char *buf, const char *key)
{
	char	*p;

	if (strncasecmp(buf, key, strlen(key)) == 0) {
		if ((p = strchr(buf, ' ')) == NULL)
			errx(1, "Failed to parse %s", key);
		return ++p;
	}

	return NULL;
}

static const char *
http_error(int code)
{
	struct http_status	error, *res;

	/* Set up key */
	error.code = code;

	if ((res = bsearch(&error, http_status,
	    sizeof(http_status) / sizeof(http_status[0]) - 1,
	    sizeof(http_status[0]), http_status_cmp)) != NULL)
		return (res->name);

	return (NULL);
}

static int
http_status_cmp(const void *a, const void *b)
{
	const struct http_status *ea = a;
	const struct http_status *eb = b;

	return (ea->code - eb->code);
}


static ssize_t
http_getline(int scheme, char **buf, size_t *n)
{
	ssize_t	buflen;

	switch (scheme) {
#ifndef NOSSL
	case S_HTTPS:
		if ((buflen = tls_getline(buf, n, ctx)) == -1)
			errx(1, "%s: tls_getline", __func__);
		break;
#endif
	case S_FTP:
	case S_HTTP:
		if ((buflen = getline(buf, n, fp)) == -1)
			err(1, "%s: getline", __func__);
		break;
	default:
		errx(1, "%s: invalid scheme", __func__);
	}

	return buflen;
}

static size_t
http_read(int scheme, char *buf, size_t size)
{
	size_t	r;
#ifndef NOSSL
	ssize_t	rs;
#endif

	switch (scheme) {
#ifndef NOSSL
	case S_HTTPS:
		do {
			rs = tls_read(ctx, buf, size);
		} while (rs == TLS_WANT_POLLIN || rs == TLS_WANT_POLLOUT);
		if (rs == -1)
			errx(1, "%s: tls_read: %s", __func__, tls_error(ctx));
		r = rs;
		break;
#endif
	case S_HTTP:
		if ((r = fread(buf, 1, size, fp)) < size)
			if (!feof(fp))
				errx(1, "%s: fread", __func__);
		break;
	default:
		errx(1, "%s: invalid scheme", __func__);
	}

	return r;
}

#ifndef NOSSL
void
https_init(char *tls_options)
{
	char		*str;
	int		 depth;
	const char	*ca_file, *errstr;

	if (tls_init() != 0)
		errx(1, "tls_init failed");

	if ((tls_config = tls_config_new()) == NULL)
		errx(1, "tls_config_new failed");

	if (tls_config_set_ciphers(tls_config, "legacy") != 0)
		errx(1, "tls set ciphers failed: %s",
		    tls_config_error(tls_config));

	ca_file = tls_default_ca_cert_file();
	while (tls_options && *tls_options) {
		switch (getsubopt(&tls_options, tls_verify_opts, &str)) {
		case HTTP_TLS_CAFILE:
			if (str == NULL)
				errx(1, "missing CA file");
			ca_file = str;
			break;
		case HTTP_TLS_CAPATH:
			if (str == NULL)
				errx(1, "missing ca path");
			if (tls_config_set_ca_path(tls_config, str) != 0)
				errx(1, "tls ca path failed");
			break;
		case HTTP_TLS_CIPHERS:
			if (str == NULL)
				errx(1, "missing cipher list");
			if (tls_config_set_ciphers(tls_config, str) != 0)
				errx(1, "tls set ciphers failed");
			break;
		case HTTP_TLS_DONTVERIFY:
			tls_config_insecure_noverifycert(tls_config);
			tls_config_insecure_noverifyname(tls_config);
			break;
		case HTTP_TLS_VERIFYDEPTH:
			if (str == NULL)
				errx(1, "missing depth");
			depth = strtonum(str, 0, INT_MAX, &errstr);
			if (errstr)
				errx(1, "Cert validation depth is %s", errstr);
			tls_config_set_verify_depth(tls_config, depth);
			break;
		case HTTP_TLS_MUSTSTAPLE:
			tls_config_ocsp_require_stapling(tls_config);
			break;
		case HTTP_TLS_NOVERIFYTIME:
			tls_config_insecure_noverifytime(tls_config);
			break;
		case HTTP_TLS_SESSION:
			if (str == NULL)
				errx(1, "missing session file");
			tls_session_fd = open(str, O_RDWR|O_CREAT, 0600);
			if (tls_session_fd == -1)
				err(1, "failed to open or create session file "
				    "'%s'", str);
			if (tls_config_set_session_fd(tls_config,
			    tls_session_fd) == -1)
				errx(1, "failed to set session: %s",
				    tls_config_error(tls_config));
			break;
		case HTTP_TLS_DOVERIFY:
			/* For compatibility, we do verify by default */
			break;
		default:
			errx(1, "Unknown -S suboption `%s'",
			    suboptarg ? suboptarg : "");
		}
	}

	if (tls_config_set_ca_file(tls_config, ca_file) == -1)
		errx(1, "tls_config_set_ca_file failed");
}

static ssize_t
tls_getline(char **buf, size_t *buflen, struct tls *tls)
{
	char		*newb;
	size_t		 newlen, off;
	int		 ret;
	unsigned char	 c;

	if (buf == NULL || buflen == NULL)
		return -1;

	/* If buf is NULL, we have to assume a size of zero */
	if (*buf == NULL)
		*buflen = 0;

	off = 0;
	do {
		do {
			ret = tls_read(tls, &c, 1);
		} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
		if (ret == -1)
			return -1;

		/* Ensure we can handle it */
		if (off + 2 > SSIZE_MAX)
			return -1;

		newlen = off + 2; /* reserve space for NUL terminator */
		if (newlen > *buflen) {
			newlen = newlen < MINBUF ? MINBUF : *buflen * 2;
			newb = recallocarray(*buf, *buflen, newlen, 1);
			if (newb == NULL)
				return -1;

			*buf = newb;
			*buflen = newlen;
		}

		*(*buf + off) = c;
		off += 1;
	} while (c != '\n');

	*(*buf + off) = '\0';
	return off;
}

static void
tls_copy_file(struct url *url, FILE *dst_fp, off_t *offset)
{
	char	*tmp_buf;
	ssize_t	 r;

	tmp_buf = xmalloc(TMPBUF_LEN);
	for (;;) {
		do {
			r = tls_read(ctx, tmp_buf, TMPBUF_LEN);
		} while (r == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT);

		if (r == -1)
			errx(1, "%s: tls_read: %s", __func__, tls_error(ctx));
		else if (r == 0)
			break;

		*offset += r;
		if (fwrite(tmp_buf, 1, r, dst_fp) != (size_t)r)
			err(1, "%s: fwrite", __func__);
	}
	free(tmp_buf);
}
#endif /* NOSSL */

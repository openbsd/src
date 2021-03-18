/*      $OpenBSD: http.c,v 1.8 2021/03/18 16:15:19 tb Exp $  */
/*
 * Copyright (c) 2020 Nils Fisher <nils_fisher@hotmail.com>
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.com>
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

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason Thorpe and Luke Mewburn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <imsg.h>

#include <tls.h>

#include "extern.h"

#define HTTP_USER_AGENT	"OpenBSD rpki-client"
#define HTTP_BUF_SIZE	(32 * 1024)
#define MAX_CONNECTIONS	12

#define WANT_POLLIN	1
#define WANT_POLLOUT	2

enum http_state {
	STATE_FREE,
	STATE_INIT,
	STATE_CONNECT,
	STATE_TLSCONNECT,
	STATE_REQUEST,
	STATE_RESPONSE_STATUS,
	STATE_RESPONSE_HEADER,
	STATE_RESPONSE_DATA,
	STATE_RESPONSE_CHUNKED,
	STATE_WRITE_DATA,
	STATE_DONE,
};

struct http_proxy {
	char	*proxyhost;
	char	*proxyuser;
	char	*proxypw;
};

struct http_connection {
	char		*url;
	char		*host;
	char		*port;
	const char	*path;	/* points into url */
	char		*modified_since;
	char		*last_modified;
	struct addrinfo *res0;
	struct addrinfo *res;
	struct tls	*tls;
	char		*buf;
	size_t		bufsz;
	size_t		bufpos;
	size_t		id;
	size_t		chunksz;
	off_t		filesize;
	off_t		bytes;
	int		status;
	int		redirect_loop;
	int		fd;
	int		outfd;
	short		events;
	short		chunked;
	enum http_state	state;
};

struct msgbuf msgq;
struct sockaddr_storage http_bindaddr;
struct tls_config *tls_config;
uint8_t *tls_ca_mem;
size_t tls_ca_size;

char *resp_buf[MAX_CONNECTIONS];
size_t resp_bsz[MAX_CONNECTIONS];
size_t resp_idx;

/*
 * Return a string that can be used in error message to identify the
 * connection.
 */
static const char *
http_info(const char *url)
{
	static char buf[64];

	if (strnvis(buf, url, sizeof buf, VIS_SAFE) >= (int)sizeof buf) {
		/* overflow, add indicator */
		memcpy(buf + sizeof buf - 4, "...", 4);
	}

	return buf;
}

/*
 * Determine whether the character needs encoding, per RFC1738:
 *	- No corresponding graphic US-ASCII.
 *	- Unsafe characters.
 */
static int
unsafe_char(const char *c0)
{
	const char *unsafe_chars = " <>\"#{}|\\^~[]`";
	const unsigned char *c = (const unsigned char *)c0;

	/*
	 * No corresponding graphic US-ASCII.
	 * Control characters and octets not used in US-ASCII.
	 */
	return (iscntrl(*c) || !isascii(*c) ||

	    /*
	     * Unsafe characters.
	     * '%' is also unsafe, if is not followed by two
	     * hexadecimal digits.
	     */
	    strchr(unsafe_chars, *c) != NULL ||
	    (*c == '%' && (!isxdigit(*++c) || !isxdigit(*++c))));
}

/*
 * Encode given URL, per RFC1738.
 * Allocate and return string to the caller.
 */
static char *
url_encode(const char *path)
{
	size_t i, length, new_length;
	char *epath, *epathp;

	length = new_length = strlen(path);

	/*
	 * First pass:
	 * Count unsafe characters, and determine length of the
	 * final URL.
	 */
	for (i = 0; i < length; i++)
		if (unsafe_char(path + i))
			new_length += 2;

	epath = epathp = malloc(new_length + 1);	/* One more for '\0'. */
	if (epath == NULL)
		err(1, NULL);

	/*
	 * Second pass:
	 * Encode, and copy final URL.
	 */
	for (i = 0; i < length; i++)
		if (unsafe_char(path + i)) {
			snprintf(epathp, 4, "%%" "%02x",
			    (unsigned char)path[i]);
			epathp += 3;
		} else
			*(epathp++) = path[i];

	*epathp = '\0';
	return (epath);
}

static void
http_setup(void)
{
	tls_config = tls_config_new();
	if (tls_config == NULL)
		errx(1, "tls config failed");
#if 0
	/* TODO Should we allow extra protos and ciphers? */
	if (tls_config_set_protocols(tls_config, TLS_PROTOCOLS_ALL) == -1)
		errx(1, "tls set protocols failed: %s",
		    tls_config_error(tls_config));
	if (tls_config_set_ciphers(tls_config, "legacy") == -1)
		errx(1, "tls set ciphers failed: %s",
		    tls_config_error(tls_config));
#endif

	/* load cert file from disk now */
	tls_ca_mem = tls_load_file(tls_default_ca_cert_file(),
	    &tls_ca_size, NULL);
	if (tls_ca_mem == NULL)
		err(1, "tls_load_file");
	tls_config_set_ca_mem(tls_config, tls_ca_mem, tls_ca_size);

	/* TODO initalize proxy settings */

}

static int
http_resolv(struct http_connection *conn, const char *host, const char *port)
{
	struct addrinfo hints;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &conn->res0);
	/*
	 * If the services file is corrupt/missing, fall back
	 * on our hard-coded defines.
	 */
	if (error == EAI_SERVICE)
		error = getaddrinfo(host, "443", &hints, &conn->res0);
	if (error != 0) {
		warnx("%s: %s", host, gai_strerror(error));
		return -1;
	}

	return 0;
}

static void
http_done(struct http_connection *conn, int ok)
{
	struct ibuf *b;

	conn->state = STATE_DONE;

	if ((b = ibuf_dynamic(64, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &conn->id, sizeof(conn->id));
	io_simple_buffer(b, &ok, sizeof(ok));
#if 0	/* TODO: cache last_modified */
	io_str_buffer(b, conn->last_modified);
#endif
	ibuf_close(&msgq, b);
}

static void
http_fail(size_t id)
{
	struct ibuf *b;
	int ok = 0;

	if ((b = ibuf_dynamic(8, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &ok, sizeof(ok));
	ibuf_close(&msgq, b);
}

static void
http_free(struct http_connection *conn)
{
	if (conn->state != STATE_DONE)
		http_fail(conn->id);

	free(conn->url);
	free(conn->host);
	free(conn->port);
	/* no need to free conn->path it points into conn->url */
	free(conn->modified_since);
	free(conn->last_modified);
	free(conn->buf);

	if (conn->res0 != NULL)
		freeaddrinfo(conn->res0);

	tls_free(conn->tls);

	if (conn->fd != -1)
		close(conn->fd);
	close(conn->outfd);
	free(conn);
}

static int
http_close(struct http_connection *conn)
{
	if (conn->tls != NULL) {
		switch (tls_close(conn->tls)) {
		case TLS_WANT_POLLIN:
			return WANT_POLLIN;
		case TLS_WANT_POLLOUT:
			return WANT_POLLOUT;
		case 0:
		case -1:
			break;
		}
	}

	return -1;
}

static int
http_parse_uri(char *uri, char **ohost, char **oport, char **opath)
{
	char *host, *port = NULL, *path;
	char *hosttail;

	if (strncasecmp(uri, "https://", 8) != 0) {
		warnx("%s: not using https schema", http_info(uri));
		return -1;
	}
	host = uri + 8;
	if ((path = strchr(host, '/')) == NULL) {
		warnx("%s: missing https path", http_info(uri));
		return -1;
	}
	if (path - uri > INT_MAX - 1) {
		warnx("%s: preposterous host length", http_info(uri));
		return -1;
	}
	if (*host == '[') {
		char *scope;
		if ((hosttail = memrchr(host, ']', path - host)) == NULL) {
			warnx("%s: unmatched opening bracket", http_info(uri));
			return -1;
		}
		if (hosttail[1] == '/' || hosttail[1] == ':')
			host++;
		if (hosttail[1] == ':')
			port = hosttail + 2;
		if ((scope = memchr(host, '%', hosttail - host)) != NULL)
			hosttail = scope;
	} else {
		if ((hosttail = memrchr(host, ':', path - host)) != NULL)
			port = hosttail + 1;
		else
			hosttail = path;
	}

	if ((host = strndup(host, hosttail - host)) == NULL)
		err(1, NULL);
	if (port != NULL) {
		if ((port = strndup(port, path - port)) == NULL)
			err(1, NULL);
	} else {
		if ((port = strdup("443")) == NULL)
			err(1, NULL);
	}
	/* do not include the initial / in path */
	path++;

	*ohost = host;
	*oport = port;
	*opath = path;

	return 0;
}


static struct http_connection *
http_new(size_t id, char *uri, char *modified_since, int outfd)
{
	struct http_connection *conn;
	char *host, *port, *path;

	if (http_parse_uri(uri, &host, &port, &path) == -1) {
		free(uri);
		free(modified_since);
		return NULL;
	}

	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		err(1, NULL);

	conn->id = id;
	conn->fd = -1;
	conn->outfd = outfd;
	conn->host = host;
	conn->port = port;
	conn->path = path;
	conn->url = uri;
	conn->modified_since = modified_since;
	conn->state = STATE_INIT;

	/* TODO proxy support (overload of host and port) */

	if (http_resolv(conn, host, port) == -1) {
		http_free(conn);
		return NULL;
	}

	return conn;
}

static int
http_redirect(struct http_connection *conn, char *uri)
{
	char *host, *port, *path;

	warnx("redirect to %s", http_info(uri));

	if (http_parse_uri(uri, &host, &port, &path) == -1) {
		free(uri);
		return -1;
	}

	free(conn->url);
	conn->url = uri;
	free(conn->host);
	conn->host = host;
	free(conn->port);
	conn->port = port;
	conn->path = path;
	/* keep modified_since since that is part of the request */
	free(conn->last_modified);
	conn->last_modified = NULL;
	free(conn->buf);
	conn->buf = NULL;
	conn->bufpos = 0;
	conn->bufsz = 0;
	tls_close(conn->tls);
	tls_free(conn->tls);
	conn->tls = NULL;
	close(conn->fd);
	conn->state = STATE_INIT;

	/* TODO proxy support (overload of host and port) */

	if (http_resolv(conn, host, port) == -1)
		return -1;

	return 0;
}

static int
http_connect(struct http_connection *conn)
{
	char pbuf[NI_MAXSERV], hbuf[NI_MAXHOST];
	char *cause = "unknown";

	if (conn->fd != -1) {
		close(conn->fd);
		conn->fd = -1;
	}

	/* start the loop below with first or next address */
	if (conn->res == NULL)
		conn->res = conn->res0;
	else
		conn->res = conn->res->ai_next;
	for (; conn->res != NULL; conn->res = conn->res->ai_next) {
		struct addrinfo *res = conn->res;
		int fd, error, save_errno;

		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), pbuf, sizeof(pbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
			strlcpy(hbuf, "(unknown)", sizeof(hbuf));

		fd = socket(res->ai_family,
		    res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol);
		if (fd == -1) {
			cause = "socket";
			continue;
		}
		conn->fd = fd;

		if (http_bindaddr.ss_family == res->ai_family) {
			if (bind(conn->fd, (struct sockaddr *)&http_bindaddr,
			    res->ai_addrlen) == -1)
				warn("%s: bind", http_info(conn->url));
		}

		error = connect(conn->fd, res->ai_addr, res->ai_addrlen);
		if (error == -1) {
			if (errno == EINPROGRESS) {
				/* waiting for connect to finish. */
				return WANT_POLLOUT;
			} else {
				save_errno = errno;
				close(conn->fd);
				conn->fd = -1;
				errno = save_errno;
				cause = "connect";
				continue;
			}
		}

		/* TODO proxy connect */
#if 0
		if (proxyenv)
			proxy_connect(conn->fd, sslhost, proxy_credentials); */
#endif
	}
	freeaddrinfo(conn->res0);
	conn->res0 = NULL;
	if (conn->fd == -1) {
		warn("%s: %s", http_info(conn->url), cause);
		return -1;
	}
	return 0;
}

static int
http_finish_connect(struct http_connection *conn)
{
	int error = 0;
	socklen_t len;

	len = sizeof(error);
	if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
		warn("%s: getsockopt SO_ERROR", http_info(conn->url));
		/* connection will be closed by http_connect() */
		return -1;
	}
	if (error != 0) {
		errno = error;
		warn("%s: connect", http_info(conn->url));
		return -1;
	}

	return 0;
}

static int
http_tls_handshake(struct http_connection *conn)
{
	switch (tls_handshake(conn->tls)) {
	case 0:
		return 0;
	case TLS_WANT_POLLIN:
		return WANT_POLLIN;
	case TLS_WANT_POLLOUT:
		return WANT_POLLOUT;
	}
	warnx("%s: TLS handshake: %s", http_info(conn->url),
	    tls_error(conn->tls));
	return -1;
}

static int
http_tls_connect(struct http_connection *conn)
{
	if ((conn->tls = tls_client()) == NULL) {
		warn("tls_client");
		return -1;
	}
	if (tls_configure(conn->tls, tls_config) == -1) {
		warnx("%s: TLS configuration: %s\n", http_info(conn->url),
		    tls_error(conn->tls));
		return -1;
	}
	if (tls_connect_socket(conn->tls, conn->fd, conn->host) == -1) {
		warnx("%s: TLS connect: %s\n", http_info(conn->url),
		    tls_error(conn->tls));
		return -1;
	}
	return http_tls_handshake(conn);
}

static int
http_request(struct http_connection *conn)
{
	char *host, *epath, *modified_since;
	int r, with_port = 0;

	/* TODO adjust request for HTTP proxy setups */

	/*
	 * Send port number only if it's specified and does not equal
	 * the default. Some broken HTTP servers get confused if you explicitly
	 * send them the port number.
	 */
	if (conn->port && strcmp(conn->port, "443") != 0)
		with_port = 1;

	/* Construct the Host header from host and port info */
	if (strchr(conn->host, ':')) {
		if (asprintf(&host, "[%s]%s%s", conn->host,
		    with_port ? ":" : "", with_port ? conn->port : "") == -1)
			err(1, NULL);

	} else {
		if (asprintf(&host, "%s%s%s", conn->host,
		    with_port ? ":" : "", with_port ? conn->port : "") == -1)
			err(1, NULL);
	}

	/*
	 * Construct and send the request. Proxy requests don't want leading /.
	 */
	epath = url_encode(conn->path);

	modified_since = NULL;
	if (conn->modified_since) {
		if (asprintf(&modified_since, "If-Modified-Since: %s\r\n",
		    conn->modified_since) == -1)
			err(1, NULL);
	}

	free(conn->buf);
	conn->bufpos = 0;
	if ((r = asprintf(&conn->buf,
	    "GET /%s HTTP/1.1\r\n"
	    "Connection: close\r\n"
	    "User-Agent: " HTTP_USER_AGENT "\r\n"
	    "Host: %s\r\n%s\r\n",
	    epath, host,
	    modified_since ? modified_since : "")) == -1)
		err(1, NULL);
	conn->bufsz = r;

	free(epath);
	free(host);
	free(modified_since);

	return WANT_POLLOUT;
}

static int
http_write(struct http_connection *conn)
{
	ssize_t s;

	s = tls_write(conn->tls, conn->buf + conn->bufpos,
	    conn->bufsz - conn->bufpos);
	if (s == -1) {
		warnx("%s: TLS write: %s", http_info(conn->url),
		    tls_error(conn->tls));
		return -1;
	} else if (s == TLS_WANT_POLLIN) {
		return WANT_POLLIN;
	} else if (s == TLS_WANT_POLLOUT) {
		return WANT_POLLOUT;
	}

	conn->bufpos += s;
	if (conn->bufpos == conn->bufsz)
		return 0;
	return WANT_POLLOUT;
}

static int
http_parse_status(struct http_connection *conn, char *buf)
{
	const char *errstr;
	char *cp, ststr[4];
	char gerror[200];
	int status;

	cp = strchr(buf, ' ');
	if (cp == NULL) {
		warnx("Improper response from %s", http_info(conn->url));
		return -1;
	} else
		cp++;

	strlcpy(ststr, cp, sizeof(ststr));
	status = strtonum(ststr, 200, 599, &errstr);
	if (errstr != NULL) {
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", http_info(conn->url), gerror);
		return -1;
	}

	switch (status) {
	case 301:
	case 302:
	case 303:
	case 307:
		if (conn->redirect_loop++ > 10) {
			warnx("%s: Too many redirections requested",
			    http_info(conn->url));
			return -1;
		}
		/* FALLTHROUGH */
	case 200:
	case 206:
	case 304:
		conn->status = status;
		break;
	default:
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", http_info(conn->url), gerror);
		break;
	}

	return 0;
}

static inline int
http_isredirect(struct http_connection *conn)
{
	if ((conn->status >= 301 && conn->status <= 303) ||
	    conn->status == 307)
		return 1;
	return 0;
}

static int
http_parse_header(struct http_connection *conn, char *buf)
{
#define CONTENTLEN "Content-Length: "
#define LOCATION "Location: "
#define TRANSFER_ENCODING "Transfer-Encoding: "
#define LAST_MODIFIED "Last-Modified: "
	const char *errstr;
	char *cp, *redirurl;
	char *locbase, *loctail;

	cp = buf;
	/* empty line, end of header */
	if (*cp == '\0')
		return 0;
	else if (strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0) {
		size_t s;
		cp += sizeof(CONTENTLEN) - 1;
		if ((s = strcspn(cp, " \t")) != 0)
			*(cp+s) = 0;
		conn->filesize = strtonum(cp, 0, LLONG_MAX, &errstr);
		if (errstr != NULL) {
			warnx("Improper response from %s",
			    http_info(conn->url));
			return -1;
		}
	} else if (http_isredirect(conn) &&
	    strncasecmp(cp, LOCATION, sizeof(LOCATION) - 1) == 0) {
		cp += sizeof(LOCATION) - 1;
		/*
		 * If there is a colon before the first slash, this URI
		 * is not relative. RFC 3986 4.2
		 */
		if (cp[strcspn(cp, ":/")] != ':') {
			/* XXX doesn't handle protocol-relative URIs */
			if (*cp == '/') {
				locbase = NULL;
				cp++;
			} else {
				locbase = strdup(conn->path);
				if (locbase == NULL)
					err(1, NULL);
				loctail = strchr(locbase, '#');
				if (loctail != NULL)
					*loctail = '\0';
				loctail = strchr(locbase, '?');
				if (loctail != NULL)
					*loctail = '\0';
				loctail = strrchr(locbase, '/');
				if (loctail == NULL) {
					free(locbase);
					locbase = NULL;
				} else
					loctail[1] = '\0';
			}
			/* Construct URL from relative redirect */
			if (asprintf(&redirurl, "%.*s/%s%s",
			    (int)(conn->path - conn->url), conn->url,
			    locbase ? locbase : "",
			    cp) == -1)
				err(1, "Cannot build redirect URL");
			free(locbase);
		} else if ((redirurl = strdup(cp)) == NULL)
			err(1, "Cannot build redirect URL");
		loctail = strchr(redirurl, '#');
		if (loctail != NULL)
			*loctail = '\0';
		return http_redirect(conn, redirurl);
	} else if (strncasecmp(cp, TRANSFER_ENCODING,
	    sizeof(TRANSFER_ENCODING) - 1) == 0) {
		cp += sizeof(TRANSFER_ENCODING) - 1;
		cp[strcspn(cp, " \t")] = '\0';
		if (strcasecmp(cp, "chunked") == 0)
			conn->chunked = 1;
	} else if (strncasecmp(cp, LAST_MODIFIED,
	    sizeof(LAST_MODIFIED) - 1) == 0) {
		cp += sizeof(LAST_MODIFIED) - 1;
		if ((conn->last_modified = strdup(cp)) == NULL)
			err(1, NULL);
	}

	return 1;
}

static char *
http_get_line(struct http_connection *conn)
{
	char *end, *line;
	size_t len;

	end = memchr(conn->buf, '\n', conn->bufpos);
	if (end == NULL)
		return NULL;

	len = end - conn->buf;
	while (len > 0 && conn->buf[len - 1] == '\r')
		--len;
	if ((line = strndup(conn->buf, len)) == NULL)
		err(1, NULL);

	/* consume line including \n */
	end++;
	conn->bufpos -= end - conn->buf;
	memmove(conn->buf, end, conn->bufpos);

	return line;
}

static int
http_parse_chunked(struct http_connection *conn, char *buf)
{
	char *header = buf;
	char *end;
	int chunksize;

	/* ignore empty lines, used between chunk and next header */
	if (*header == '\0')
		return 1;

	/* strip CRLF and any optional chunk extension */
	header[strcspn(header, ";\r\n")] = '\0';
	errno = 0;
	chunksize = strtoul(header, &end, 16);
	if (errno != 0 || header[0] == '\0' || *end != '\0' ||
	    chunksize > INT_MAX) {
		warnx("%s: Invalid chunk size", http_info(conn->url));
		return -1;
	}
	conn->chunksz = chunksize;

	if (conn->chunksz == 0) {
		http_done(conn, 1);
		return 0;
	}

	return 1;
}

static int
http_read(struct http_connection *conn)
{
	ssize_t s;
	char *buf;

	s = tls_read(conn->tls, conn->buf + conn->bufpos,
	    conn->bufsz - conn->bufpos);
	if (s == -1) {
		warn("%s: TLS read: %s", http_info(conn->url),
		    tls_error(conn->tls));
		return -1;
	} else if (s == TLS_WANT_POLLIN) {
		return WANT_POLLIN;
	} else if (s == TLS_WANT_POLLOUT) {
		return WANT_POLLOUT;
	}

	if (s == 0 && conn->bufpos == 0) {
		warnx("%s: short read, connection closed",
		    http_info(conn->url));
		return -1;
	}

	conn->bufpos += s;

	switch (conn->state) {
	case STATE_RESPONSE_STATUS:
		buf = http_get_line(conn);
		if (buf == NULL)
			return WANT_POLLIN;
		if (http_parse_status(conn, buf) == -1) {
			free(buf);
			return -1;
		}
		free(buf);
		conn->state = STATE_RESPONSE_HEADER;
		/* FALLTHROUGH */
	case STATE_RESPONSE_HEADER:
		while ((buf = http_get_line(conn)) != NULL) {
			int rv;

			rv = http_parse_header(conn, buf);
			free(buf);
			if (rv != 1)
				return rv;
		}
		return WANT_POLLIN;
	case STATE_RESPONSE_DATA:
		if (conn->bufpos == conn->bufsz ||
		    conn->filesize - conn->bytes <= (off_t)conn->bufpos)
			return 0;
		return WANT_POLLIN;
	case STATE_RESPONSE_CHUNKED:
		while (conn->chunksz == 0) {
			buf = http_get_line(conn);
			if (buf == NULL)
				return WANT_POLLIN;
			switch (http_parse_chunked(conn, buf)) {
			case -1:
				free(buf);
				return -1;
			case 0:
				free(buf);
				return 0;
			}
			free(buf);
		}

		if (conn->bufpos == conn->bufsz ||
		    conn->chunksz <= conn->bufpos)
			return 0;
		return WANT_POLLIN;
	default:
		errx(1, "unexpected http state");
	}
}

static int
data_write(struct http_connection *conn)
{
	ssize_t s;
	size_t bsz = conn->bufpos;

	if (conn->filesize - conn->bytes < (off_t)bsz)
		bsz = conn->filesize - conn->bytes;

	s = write(conn->outfd, conn->buf, bsz);
	if (s == -1) {
		warn("%s: Data write", http_info(conn->url));
		return -1;
	}

	conn->bufpos -= s;
	conn->bytes += s;
	memmove(conn->buf, conn->buf + s, conn->bufpos);

	if (conn->bytes == conn->filesize) {
		http_done(conn, 1);
		return 0;
	}

	if (conn->bufpos == 0)
		return 0;

	return WANT_POLLOUT;
}

static int
chunk_write(struct http_connection *conn)
{
	ssize_t s;
	size_t bsz = conn->bufpos;

	if (bsz > conn->chunksz)
		bsz = conn->chunksz;

	s = write(conn->outfd, conn->buf, bsz);
	if (s == -1) {
		warn("%s: Chunk write", http_info(conn->url));
		return -1;
	}

	conn->bufpos -= s;
	conn->chunksz -= s;
	conn->bytes += s;
	memmove(conn->buf, conn->buf + s, conn->bufpos);

	if (conn->bufpos == 0 || conn->chunksz == 0)
		return 0;

	return WANT_POLLOUT;
}

static int
http_handle(struct http_connection *conn)
{
	switch (conn->state) {
	case STATE_INIT:
		return 0;
	case STATE_CONNECT:
		if (http_finish_connect(conn) == -1)
			/* something went wrong, try other host */
			return http_connect(conn);
		return 0;
	case STATE_TLSCONNECT:
		return http_tls_handshake(conn);
	case STATE_REQUEST:
		return http_write(conn);
	case STATE_RESPONSE_STATUS:
	case STATE_RESPONSE_HEADER:
	case STATE_RESPONSE_DATA:
	case STATE_RESPONSE_CHUNKED:
		return http_read(conn);
	case STATE_WRITE_DATA:
		if (conn->chunked)
			return chunk_write(conn);
		else
			return data_write(conn);
	case STATE_DONE:
		return http_close(conn);
	case STATE_FREE:
		errx(1, "bad http state");
	}
	errx(1, "unknown http state");
}

static int
http_nextstep(struct http_connection *conn)
{
	switch (conn->state) {
	case STATE_INIT:
		conn->state = STATE_CONNECT;
		return http_connect(conn);
	case STATE_CONNECT:
		conn->state = STATE_TLSCONNECT;
		return http_tls_connect(conn);
	case STATE_TLSCONNECT:
		conn->state = STATE_REQUEST;
		return http_request(conn);
	case STATE_REQUEST:
		conn->state = STATE_RESPONSE_STATUS;
		free(conn->buf);
		/* allocate the read buffer */
		if ((conn->buf = malloc(HTTP_BUF_SIZE)) == NULL)
			err(1, NULL);
		conn->bufpos = 0;
		conn->bufsz = HTTP_BUF_SIZE;
		return WANT_POLLIN;
	case STATE_RESPONSE_STATUS:
		conn->state = STATE_RESPONSE_HEADER;
		return WANT_POLLIN;
	case STATE_RESPONSE_HEADER:
		if (conn->status == 200)
			conn->state = STATE_RESPONSE_DATA;
		else {
			http_done(conn, 0);
			return http_close(conn);
		}
		return WANT_POLLIN;
	case STATE_RESPONSE_DATA:
		conn->state = STATE_WRITE_DATA;
		return WANT_POLLOUT;
	case STATE_RESPONSE_CHUNKED:
		conn->state = STATE_WRITE_DATA;
		return WANT_POLLOUT;
	case STATE_WRITE_DATA:
		if (conn->chunked)
			conn->state = STATE_RESPONSE_CHUNKED;
		else
			conn->state = STATE_RESPONSE_DATA;
		return WANT_POLLIN;
	case STATE_DONE:
		return http_close(conn);
	case STATE_FREE:
		errx(1, "bad http state");
	}
	errx(1, "unknown http state");
}

static int
http_do(struct http_connection *conn)
{
	switch (http_handle(conn)) {
	case -1:
		/* connection failure */
		http_free(conn);
		return -1;
	case 0:
		switch (http_nextstep(conn)) {
		case WANT_POLLIN:
			conn->events = POLLIN;
			break;
		case WANT_POLLOUT:
			conn->events = POLLOUT;
			break;
		case -1:
			http_free(conn);
			return -1;
		}
		break;
	case WANT_POLLIN:
		conn->events = POLLIN;
		break;
	case WANT_POLLOUT:
		conn->events = POLLOUT;
		break;
	}
	return 0;
}

void
proc_http(char *bind_addr, int fd)
{
	struct http_connection *http_conns[MAX_CONNECTIONS];
	struct pollfd pfds[MAX_CONNECTIONS + 1];
	size_t i;
	int active_connections;

	if (bind_addr != NULL) {
		struct addrinfo hints, *res;

		bzero(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM; /*dummy*/
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(bind_addr, NULL, &hints, &res) == 0) {
			memcpy(&http_bindaddr, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		}
	}
	http_setup();

	if (pledge("stdio inet dns recvfd", NULL) == -1)
		err(1, "pledge");

	memset(&http_conns, 0, sizeof(http_conns));
	memset(&pfds, 0, sizeof(pfds));
	pfds[MAX_CONNECTIONS].fd = fd;

	msgbuf_init(&msgq);
	msgq.fd = fd;

	for (;;) {
		active_connections = 0;
		for (i = 0; i < MAX_CONNECTIONS; i++) {
			struct http_connection *conn = http_conns[i];
			if (conn == NULL) {
				pfds[i].fd = -1;
				continue;
			}
			if (conn->state == STATE_WRITE_DATA)
				pfds[i].fd = conn->outfd;
			else
				pfds[i].fd = conn->fd;

			pfds[i].events = conn->events;
			active_connections++;
		}
		pfds[MAX_CONNECTIONS].events = 0;
		if (active_connections < MAX_CONNECTIONS)
			pfds[MAX_CONNECTIONS].events |= POLLIN;
		if (msgq.queued)
			pfds[MAX_CONNECTIONS].events |= POLLOUT;

		if (poll(pfds, sizeof(pfds) / sizeof(pfds[0]), INFTIM) == -1)
			err(1, "poll");

		if (pfds[MAX_CONNECTIONS].revents & POLLHUP)
			break;

		if (pfds[MAX_CONNECTIONS].revents & POLLIN) {
			struct http_connection *h;
			size_t id;
			int outfd;
			char *uri;
			char *mod;

			outfd = io_recvfd(fd, &id, sizeof(id));
			io_str_read(fd, &uri);
			io_str_read(fd, &mod);

			h = http_new(id, uri, mod, outfd);
			if (h == NULL) {
				close(outfd);
				http_fail(id);
			} else
				for (i = 0; i < MAX_CONNECTIONS; i++) {
					if (http_conns[i] != NULL)
						continue;
					http_conns[i] = h;
					if (http_do(h) == -1)
						http_conns[i] = NULL;
					break;
				}
		}
		if (pfds[MAX_CONNECTIONS].revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}
		for (i = 0; i < MAX_CONNECTIONS; i++) {
			struct http_connection *conn = http_conns[i];

			if (conn == NULL)
				continue;
			/* event not ready */
			if (!(pfds[i].revents & (conn->events | POLLHUP)))
				continue;

			if (http_do(conn) == -1)
				http_conns[i] = NULL;
		}
	}

	exit(0);
}

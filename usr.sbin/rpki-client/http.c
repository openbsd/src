/*	$OpenBSD: http.c,v 1.32 2021/04/20 14:32:49 claudio Exp $  */
/*
 * Copyright (c) 2020 Nils Fisher <nils_fisher@hotmail.com>
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <imsg.h>

#include <tls.h>

#include "extern.h"

#define HTTP_USER_AGENT		"OpenBSD rpki-client"
#define HTTP_BUF_SIZE		(32 * 1024)
#define HTTP_IDLE_TIMEOUT	10
#define MAX_CONNECTIONS		64
#define NPFDS			(MAX_CONNECTIONS + 1)

enum res {
	DONE,
	WANT_POLLIN,
	WANT_POLLOUT,
};

enum http_state {
	STATE_FREE,
	STATE_CONNECT,
	STATE_TLSCONNECT,
	STATE_REQUEST,
	STATE_RESPONSE_STATUS,
	STATE_RESPONSE_HEADER,
	STATE_RESPONSE_DATA,
	STATE_RESPONSE_CHUNKED_HEADER,
	STATE_RESPONSE_CHUNKED_TRAILER,
	STATE_WRITE_DATA,
	STATE_IDLE,
	STATE_CLOSE,
};

struct http_proxy {
	char	*proxyhost;
	char	*proxyuser;
	char	*proxypw;
};

struct http_connection {
	LIST_ENTRY(http_connection)	entry;
	char			*host;
	char			*port;
	char			*last_modified;
	char			*redir_uri;
	struct http_request	*req;
	struct pollfd		*pfd;
	struct addrinfo		*res0;
	struct addrinfo		*res;
	struct tls		*tls;
	char			*buf;
	size_t			bufsz;
	size_t			bufpos;
	off_t			iosz;
	time_t			idle_time;
	int			status;
	int			fd;
	int			chunked;
	int			keep_alive;
	short			events;
	enum http_state		state;
};

LIST_HEAD(http_conn_list, http_connection);

struct http_request {
	TAILQ_ENTRY(http_request)	entry;
	char			*uri;
	char			*modified_since;
	char			*host;
	char			*port;
	const char		*path;	/* points into uri */
	size_t			 id;
	int			 outfd;
	int			 redirect_loop;
};

TAILQ_HEAD(http_req_queue, http_request);

static struct http_conn_list	active = LIST_HEAD_INITIALIZER(active);
static struct http_conn_list	idle = LIST_HEAD_INITIALIZER(idle);
static struct http_req_queue	queue = TAILQ_HEAD_INITIALIZER(queue);
static size_t http_conn_count;

static struct msgbuf msgq;
static struct sockaddr_storage http_bindaddr;
static struct tls_config *tls_config;
static uint8_t *tls_ca_mem;
static size_t tls_ca_size;

/* HTTP request API */
static void	http_req_new(size_t, char *, char *, int);
static void	http_req_free(struct http_request *);
static void	http_req_done(size_t, enum http_result, const char *);
static void	http_req_fail(size_t);
static int	http_req_schedule(struct http_request *);

/* HTTP connection API */
static void	http_new(struct http_request *);
static void	http_free(struct http_connection *);

static enum res http_done(struct http_connection *, enum http_result);
static enum res http_failed(struct http_connection *);

/* HTTP connection FSM functions */
static void	http_do(struct http_connection *,
		    enum res (*)(struct http_connection *));

/* These functions can be used with http_do() */
static enum res	http_connect(struct http_connection *);
static enum res	http_request(struct http_connection *);
static enum res	http_close(struct http_connection *);
static enum res http_handle(struct http_connection *);

/* Internal state functions used by the above functions */
static enum res	http_finish_connect(struct http_connection *);
static enum res	http_tls_connect(struct http_connection *);
static enum res	http_tls_handshake(struct http_connection *);
static enum res	http_read(struct http_connection *);
static enum res	http_write(struct http_connection *);
static enum res	data_write(struct http_connection *);

static time_t
getmonotime(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		err(1, "clock_gettime");
	return (ts.tv_sec);
}

/*
 * Return a string that can be used in error message to identify the
 * connection.
 */
static const char *
http_info(const char *uri)
{
	static char buf[80];

	if (strnvis(buf, uri, sizeof buf, VIS_SAFE) >= (int)sizeof buf) {
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
	    (*c == '%' && (!isxdigit(c[1]) || !isxdigit(c[2]))));
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

/*
 * Parse a URI and split it up into host, port and path.
 * Does some basic URI validation. Both host and port need to be freed
 * by the caller whereas path points into the uri.
 */
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

/*
 * Lookup the IP addresses for host:port.
 * Returns 0 on success and -1 on failure.
 */
static int
http_resolv(struct addrinfo **res, const char *host, const char *port)
{
	struct addrinfo hints;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, res);
	/*
	 * If the services file is corrupt/missing, fall back
	 * on our hard-coded defines.
	 */
	if (error == EAI_SERVICE)
		error = getaddrinfo(host, "443", &hints, res);
	if (error != 0) {
		warnx("%s: %s", host, gai_strerror(error));
		return -1;
	}

	return 0;
}

/*
 * Create and queue a new request.
 */
static void
http_req_new(size_t id, char *uri, char *modified_since, int outfd)
{
	struct http_request *req;
	char *host, *port, *path;

	if (http_parse_uri(uri, &host, &port, &path) == -1) {
		free(uri);
		free(modified_since);
		close(outfd);
		http_req_fail(id);
		return;
	}

	if ((req = calloc(1, sizeof(*req))) == NULL)
		err(1, NULL);

	req->id = id;
	req->outfd = outfd;
	req->host = host;
	req->port = port;
	req->path = path;
	req->uri = uri;
	req->modified_since = modified_since;

	TAILQ_INSERT_TAIL(&queue, req, entry);
}

/*
 * Free a request, request is not allowed to be on the req queue.
 */
static void
http_req_free(struct http_request *req)
{
	if (req == NULL)
		return;

	free(req->host);
	free(req->port);
	/* no need to free req->path it points into req->uri */
	free(req->uri);
	free(req->modified_since);

	if (req->outfd != -1)
		close(req->outfd);
}

/*
 * Enqueue request response
 */
static void
http_req_done(size_t id, enum http_result res, const char *last_modified)
{
	struct ibuf *b;

	if ((b = ibuf_dynamic(64, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &res, sizeof(res));
	io_str_buffer(b, last_modified);
	ibuf_close(&msgq, b);
}

/*
 * Enqueue request failure response
 */
static void
http_req_fail(size_t id)
{
	struct ibuf *b;
	enum http_result res = HTTP_FAILED;

	if ((b = ibuf_dynamic(8, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &res, sizeof(res));
	io_str_buffer(b, NULL);
	ibuf_close(&msgq, b);
}

/*
 * Schedule new requests until maximum number of connections is reached.
 * Try to reuse an idle connection if one exists that matches host and port.
 */
static int
http_req_schedule(struct http_request *req)
{
	struct http_connection *conn;

	TAILQ_REMOVE(&queue, req, entry);

	/* check list of idle connections first */
	LIST_FOREACH(conn, &idle, entry) {
		if (strcmp(conn->host, req->host) != 0)
			continue;
		if (strcmp(conn->port, req->port) != 0)
			continue;

		LIST_REMOVE(conn, entry);
		LIST_INSERT_HEAD(&active, conn, entry);

		/* use established connection */
		conn->req = req;
		conn->idle_time = 0;

		/* start request */
		http_do(conn, http_request);
		if (conn->state == STATE_FREE)
			http_free(conn);
		return 1;
	}

	if (http_conn_count < MAX_CONNECTIONS) {
		http_new(req);
		return 1;
	}

	/* no more slots free, requeue */
	TAILQ_INSERT_HEAD(&queue, req, entry);
	return 0;
}

/*
 * Create a new HTTP connection which will be used for the HTTP request req.
 * On errors a req faulure is issued and both connection and request are freed.
 */ 
static void
http_new(struct http_request *req)
{
	struct http_connection *conn;

	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		err(1, NULL);

	conn->fd = -1;
	conn->req = req;
	if ((conn->host = strdup(req->host)) == NULL)
		err(1, NULL);
	if ((conn->port = strdup(req->port)) == NULL)
		err(1, NULL);

	LIST_INSERT_HEAD(&active, conn, entry);
	http_conn_count++;

	/* TODO proxy support (overload of host and port) */

	if (http_resolv(&conn->res0, conn->host, conn->port) == -1) {
		http_req_fail(req->id);
		http_free(conn);
		return;
	}

	/* connect and start request */
	http_do(conn, http_connect);
	if (conn->state == STATE_FREE)
		http_free(conn);
}

/*
 * Free a no longer active connection, releasing all memory and closing
 * any open file descriptor.
 */
static void
http_free(struct http_connection *conn)
{
	assert(conn->state == STATE_FREE);

	LIST_REMOVE(conn, entry);
	http_conn_count--;

	http_req_free(conn->req);
	free(conn->host);
	free(conn->port);
	free(conn->last_modified);
	free(conn->redir_uri);
	free(conn->buf);

	if (conn->res0 != NULL)
		freeaddrinfo(conn->res0);

	tls_free(conn->tls);

	if (conn->fd != -1)
		close(conn->fd);
	free(conn);
}

/*
 * Called when a request on this connection is finished.
 * Move connection into idle state and onto idle queue.
 * If there is a request connected to it send back a response
 * with http_result res, else ignore the res.
 */
static enum res
http_done(struct http_connection *conn, enum http_result res)
{
	assert(conn->bufpos == 0);
	assert(conn->iosz == 0);
	assert(conn->chunked == 0);
	assert(conn->redir_uri == NULL);

	conn->state = STATE_IDLE;
	conn->idle_time = getmonotime() + HTTP_IDLE_TIMEOUT;

	if (conn->req) {
		http_req_done(conn->req->id, res, conn->last_modified);
		http_req_free(conn->req);
		conn->req = NULL;
	}

	if (!conn->keep_alive)
		return http_close(conn);

	LIST_REMOVE(conn, entry);
	LIST_INSERT_HEAD(&idle, conn, entry);

	/* reset status and keep-alive for good measures */
	conn->status = 0;
	conn->keep_alive = 0;

	return WANT_POLLIN;
}

/*
 * Called in case of error, moves connection into free state.
 * This will skip proper shutdown of the TLS session.
 * If a request is pending fail and free the request.
 */
static enum res
http_failed(struct http_connection *conn)
{
	conn->state = STATE_FREE;

	if (conn->req) {
		http_req_fail(conn->req->id);
		http_req_free(conn->req);
		conn->req = NULL;
	}

	return DONE;
}

/*
 * Call the function f and update the connection events based
 * on the return value.
 */
static void
http_do(struct http_connection *conn, enum res (*f)(struct http_connection *))
{
	switch (f(conn)) {
	case DONE:
		conn->events = 0;
		break;
	case WANT_POLLIN:
		conn->events = POLLIN;
		break;
	case WANT_POLLOUT:
		conn->events = POLLOUT;
		break;
	default:
		errx(1, "%s: unexpected function return",
		    http_info(conn->host));
	}
}

/*
 * Connection successfully establish, initiate TLS handshake.
 */
static enum res
http_connect_done(struct http_connection *conn)
{
	freeaddrinfo(conn->res0);
	conn->res0 = NULL;
	conn->res = NULL;

#if 0
	/* TODO proxy connect */
	if (proxyenv)
		proxy_connect(conn->fd, sslhost, proxy_credentials); */
#endif

	return http_tls_connect(conn);
}

/*
 * Start an asynchronous connect.
 */
static enum res
http_connect(struct http_connection *conn)
{
	const char *cause = NULL;

	assert(conn->fd == -1); 
	conn->state = STATE_CONNECT;

	/* start the loop below with first or next address */
	if (conn->res == NULL)
		conn->res = conn->res0;
	else
		conn->res = conn->res->ai_next;
	for (; conn->res != NULL; conn->res = conn->res->ai_next) {
		struct addrinfo *res = conn->res;
		int fd, save_errno;

		fd = socket(res->ai_family,
		    res->ai_socktype | SOCK_NONBLOCK, res->ai_protocol);
		if (fd == -1) {
			cause = "socket";
			continue;
		}
		conn->fd = fd;

		if (http_bindaddr.ss_family == res->ai_family) {
			if (bind(conn->fd, (struct sockaddr *)&http_bindaddr,
			    res->ai_addrlen) == -1) {
				save_errno = errno;
				close(conn->fd);
				conn->fd = -1;
				errno = save_errno;
				cause = "bind";
				continue;
			}
		}

		if (connect(conn->fd, res->ai_addr, res->ai_addrlen) == -1) {
			if (errno == EINPROGRESS) {
				/* wait for async connect to finish. */
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

		break;	/* okay we got one */
	}

	if (conn->fd == -1) {
		if (cause != NULL)
			warn("%s: %s", http_info(conn->req->uri), cause);
		return http_failed(conn);
	}

	return http_connect_done(conn);
}

/*
 * Called once an asynchronus connect request finished.
 */
static enum res
http_finish_connect(struct http_connection *conn)
{
	int error = 0;
	socklen_t len;

	len = sizeof(error);
	if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
		warn("%s: getsockopt SO_ERROR", http_info(conn->req->uri));
		goto fail;
	}
	if (error != 0) {
		errno = error;
		warn("%s: connect", http_info(conn->req->uri));
		goto fail;
	}

	return http_connect_done(conn);

fail:
	close(conn->fd);
	conn->fd = -1;

	return http_connect(conn);
}

/*
 * Initiate TLS session on a new connection.
 */
static enum res
http_tls_connect(struct http_connection *conn)
{
	assert(conn->state == STATE_CONNECT);
	conn->state = STATE_TLSCONNECT;

	if ((conn->tls = tls_client()) == NULL) {
		warn("tls_client");
		return http_failed(conn);
	}
	if (tls_configure(conn->tls, tls_config) == -1) {
		warnx("%s: TLS configuration: %s\n", http_info(conn->req->uri),
		    tls_error(conn->tls));
		return http_failed(conn);
	}
	if (tls_connect_socket(conn->tls, conn->fd, conn->host) == -1) {
		warnx("%s: TLS connect: %s\n", http_info(conn->req->uri),
		    tls_error(conn->tls));
		return http_failed(conn);
	}

	return http_tls_handshake(conn);
}

/*
 * Do the tls_handshake and then send out the HTTP request.
 */
static enum res
http_tls_handshake(struct http_connection *conn)
{
	switch (tls_handshake(conn->tls)) {
	case -1:
		warnx("%s: TLS handshake: %s", http_info(conn->req->uri),
		    tls_error(conn->tls));
		return http_failed(conn);
	case TLS_WANT_POLLIN:
		return WANT_POLLIN;
	case TLS_WANT_POLLOUT:
		return WANT_POLLOUT;
	}

	/* ready to send request */
	return http_request(conn);
}

/*
 * Build the HTTP request and send it out.
 */
static enum res
http_request(struct http_connection *conn)
{
	char *host, *epath, *modified_since;
	int r, with_port = 0;

	assert(conn->state == STATE_IDLE || conn->state == STATE_TLSCONNECT);
	conn->state = STATE_REQUEST;

	/* TODO adjust request for HTTP proxy setups */

	/*
	 * Send port number only if it's specified and does not equal
	 * the default. Some broken HTTP servers get confused if you explicitly
	 * send them the port number.
	 */
	if (strcmp(conn->port, "443") != 0)
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
	epath = url_encode(conn->req->path);

	modified_since = NULL;
	if (conn->req->modified_since != NULL) {
		if (asprintf(&modified_since, "If-Modified-Since: %s\r\n",
		    conn->req->modified_since) == -1)
			err(1, NULL);
	}

	free(conn->buf);
	conn->bufpos = 0;
	if ((r = asprintf(&conn->buf,
	    "GET /%s HTTP/1.1\r\n"
	    "Connection: keep-alive\r\n"
	    "User-Agent: " HTTP_USER_AGENT "\r\n"
	    "Host: %s\r\n%s\r\n",
	    epath, host,
	    modified_since ? modified_since : "")) == -1)
		err(1, NULL);
	conn->bufsz = r;

	free(epath);
	free(host);
	free(modified_since);

	return http_write(conn);
}

/*
 * Parse the HTTP status line.
 * Return 0 for status codes 200, 301-304, 307-308.
 * Failure codes and other errors return -1.
 * The redirect loop limit is enforced here.
 */
static int
http_parse_status(struct http_connection *conn, char *buf)
{
	const char *errstr;
	char *cp, ststr[4];
	char gerror[200];
	int status;

	cp = strchr(buf, ' ');
	if (cp == NULL) {
		warnx("Improper response from %s", http_info(conn->host));
		return -1;
	} else
		cp++;

	strlcpy(ststr, cp, sizeof(ststr));
	status = strtonum(ststr, 200, 599, &errstr);
	if (errstr != NULL) {
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", http_info(conn->host),
		    gerror);
		return -1;
	}

	switch (status) {
	case 301:
	case 302:
	case 303:
	case 307:
	case 308:
		if (conn->req->redirect_loop++ > 10) {
			warnx("%s: Too many redirections requested",
			    http_info(conn->host));
			return -1;
		}
		/* FALLTHROUGH */
	case 200:
	case 304:
		conn->status = status;
		break;
	default:
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", http_info(conn->host),
		    gerror);
		return -1;
	}

	return 0;
}

/*
 * Returns true if the connection status is any of the redirect codes.
 */
static inline int
http_isredirect(struct http_connection *conn)
{
	if ((conn->status >= 301 && conn->status <= 303) ||
	    conn->status == 307 || conn->status == 308)
		return 1;
	return 0;
}

static void
http_redirect(struct http_connection *conn)
{
	char *uri, *mod_since = NULL;
	int outfd;

	/* move uri and fd out for new request */
	outfd = conn->req->outfd;
	conn->req->outfd = -1;

	uri = conn->redir_uri;
	conn->redir_uri = NULL;

	if (conn->req->modified_since)
		if ((mod_since = strdup(conn->req->modified_since)) == NULL)
			err(1, NULL);

	logx("redirect to %s", http_info(uri));
	http_req_new(conn->req->id, uri, mod_since, outfd);	

	/* clear request before moving connection to idle */
	http_req_free(conn->req);
	conn->req = NULL;
}

static int
http_parse_header(struct http_connection *conn, char *buf)
{
#define CONTENTLEN "Content-Length: "
#define LOCATION "Location: "
#define CONNECTION "Connection: "
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
		conn->iosz = strtonum(cp, 0, LLONG_MAX, &errstr);
		if (errstr != NULL) {
			warnx("Content-Length of %s is %s",
			    http_info(conn->req->uri), errstr);
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
				locbase = strdup(conn->req->path);
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
			    (int)(conn->req->path - conn->req->uri),
			    conn->req->uri, locbase ? locbase : "", cp) == -1)
				err(1, "Cannot build redirect URL");
			free(locbase);
		} else if ((redirurl = strdup(cp)) == NULL)
			err(1, "Cannot build redirect URL");
		loctail = strchr(redirurl, '#');
		if (loctail != NULL)
			*loctail = '\0';
		conn->redir_uri = redirurl;
	} else if (strncasecmp(cp, TRANSFER_ENCODING,
	    sizeof(TRANSFER_ENCODING) - 1) == 0) {
		cp += sizeof(TRANSFER_ENCODING) - 1;
		cp[strcspn(cp, " \t")] = '\0';
		if (strcasecmp(cp, "chunked") == 0)
			conn->chunked = 1;
	} else if (strncasecmp(cp, CONNECTION, sizeof(CONNECTION) - 1) == 0) {
		cp += sizeof(CONNECTION) - 1;
		cp[strcspn(cp, " \t")] = '\0';
		if (strcasecmp(cp, "keep-alive") == 0)
			conn->keep_alive = 1;
	} else if (strncasecmp(cp, LAST_MODIFIED,
	    sizeof(LAST_MODIFIED) - 1) == 0) {
		cp += sizeof(LAST_MODIFIED) - 1;
		if ((conn->last_modified = strdup(cp)) == NULL)
			err(1, NULL);
	}

	return 1;
}

/*
 * Return one line from the HTTP response.
 * The line returned has any possible '\r' and '\n' at the end stripped.
 * The buffer is advanced to the start of the next line.
 * If there is currently no full line in the buffer NULL is returned.
 */ 
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

/*
 * Parse the header between data chunks during chunked transfers.
 * Returns 0 if a new chunk size could be correctly read.
 * Returns 1 for the empty trailer lines.
 * If the chuck size could not be converted properly -1 is returned.
 */
static int
http_parse_chunked(struct http_connection *conn, char *buf)
{
	char *header = buf;
	char *end;
	unsigned long chunksize;

	/* empty lines are used as trailer */
	if (*header == '\0')
		return 1;

	/* strip CRLF and any optional chunk extension */
	header[strcspn(header, ";\r\n")] = '\0';
	errno = 0;
	chunksize = strtoul(header, &end, 16);
	if (header[0] == '\0' || *end != '\0' || (errno == ERANGE &&
	    chunksize == ULONG_MAX) || chunksize > INT_MAX)
		return -1;

	conn->iosz = chunksize;
	return 0;
}

static enum res
http_read(struct http_connection *conn)
{
	ssize_t s;
	char *buf;
	int done;

read_more:
	s = tls_read(conn->tls, conn->buf + conn->bufpos,
	    conn->bufsz - conn->bufpos);
	if (s == -1) {
		warn("%s: TLS read: %s", http_info(conn->host),
		    tls_error(conn->tls));
		return http_failed(conn);
	} else if (s == TLS_WANT_POLLIN) {
		return WANT_POLLIN;
	} else if (s == TLS_WANT_POLLOUT) {
		return WANT_POLLOUT;
	}

	if (s == 0 && conn->bufpos == 0) {
		if (conn->req)
			warnx("%s: short read, connection closed",
			    http_info(conn->req->uri));
		return http_failed(conn);
	}

	conn->bufpos += s;

again:
	switch (conn->state) {
	case STATE_RESPONSE_STATUS:
		buf = http_get_line(conn);
		if (buf == NULL)
			goto read_more;
		if (http_parse_status(conn, buf) == -1) {
			free(buf);
			return http_failed(conn);
		}
		free(buf);
		conn->state = STATE_RESPONSE_HEADER;
		goto again;
	case STATE_RESPONSE_HEADER:
		done = 0;
		while (!done) {
			int rv;

			buf = http_get_line(conn);
			if (buf == NULL)
				goto read_more;

			rv = http_parse_header(conn, buf);
			free(buf);

			if (rv == -1)
				return http_failed(conn);
			if (rv ==  0)
				done = 1;
		}

		/* Check status header and decide what to do next */
		if (conn->status == 200 || http_isredirect(conn)) {
			if (http_isredirect(conn))
				http_redirect(conn);

			if (conn->chunked)
				conn->state = STATE_RESPONSE_CHUNKED_HEADER;
			else
				conn->state = STATE_RESPONSE_DATA;
			goto again;
		} else if (conn->status == 304) {
			return http_done(conn, HTTP_NOT_MOD);
		}
		
		return http_failed(conn);
	case STATE_RESPONSE_DATA:
		if (conn->bufpos != conn->bufsz &&
		    conn->iosz > (off_t)conn->bufpos)
			goto read_more;

		/* got a full buffer full of data */
		if (conn->req == NULL) {
			/*
			 * After redirects all data needs to be discarded.
			 */
			if (conn->iosz < (off_t)conn->bufpos) {
				conn->bufpos  -= conn->iosz;
				conn->iosz = 0;
			} else {
				conn->iosz  -= conn->bufpos;
				conn->bufpos = 0;
			}
			if (conn->chunked)
				conn->state = STATE_RESPONSE_CHUNKED_TRAILER;
			else
				conn->state = STATE_RESPONSE_DATA;
			goto read_more;
		}

		conn->state = STATE_WRITE_DATA;
		return WANT_POLLOUT;
	case STATE_RESPONSE_CHUNKED_HEADER:
		assert(conn->iosz == 0);

		buf = http_get_line(conn);
		if (buf == NULL)
			goto read_more;
		if (http_parse_chunked(conn, buf) != 0) {
			warnx("%s: bad chunk encoding", http_info(conn->host));
			free(buf);
			return http_failed(conn);
		}

		/*
		 * check if transfer is done, in which case the last trailer
		 * still needs to be processed.
		 */
		if (conn->iosz == 0) {
			conn->chunked = 0;
			conn->state = STATE_RESPONSE_CHUNKED_TRAILER;
			goto again;
		}

		conn->state = STATE_RESPONSE_DATA;
		goto again;
	case STATE_RESPONSE_CHUNKED_TRAILER:
		buf = http_get_line(conn);
		if (buf == NULL)
			goto read_more;
		if (http_parse_chunked(conn, buf) != 1) {
			warnx("%s: bad chunk encoding", http_info(conn->host));
			free(buf);
			return http_failed(conn);
		}
		free(buf);

		/* if chunked got cleared then the transfer is over */
		if (conn->chunked == 0)
			return http_done(conn, HTTP_OK);

		conn->state = STATE_RESPONSE_CHUNKED_HEADER;
		goto again;
	default:
		errx(1, "unexpected http state");
	}
}

/*
 * Send out the HTTP request. When done, replace buffer with the read buffer.
 */
static enum res
http_write(struct http_connection *conn)
{
	ssize_t s;

	assert(conn->state == STATE_REQUEST);

	while (conn->bufpos < conn->bufsz) {
		s = tls_write(conn->tls, conn->buf + conn->bufpos,
		    conn->bufsz - conn->bufpos);
		if (s == -1) {
			warnx("%s: TLS write: %s", http_info(conn->host),
			    tls_error(conn->tls));
			return http_failed(conn);
		} else if (s == TLS_WANT_POLLIN) {
			return WANT_POLLIN;
		} else if (s == TLS_WANT_POLLOUT) {
			return WANT_POLLOUT;
		}

		conn->bufpos += s;
	}

	/* done writing, first thing we need the status */
	conn->state = STATE_RESPONSE_STATUS;

	/* free write buffer and allocate the read buffer */
	free(conn->buf);
	conn->bufpos = 0;
	conn->bufsz = HTTP_BUF_SIZE;
	if ((conn->buf = malloc(conn->bufsz)) == NULL)
		err(1, NULL);

	return http_read(conn);
}

/*
 * Properly shutdown the TLS session else move connection into free state.
 */
static enum res
http_close(struct http_connection *conn)
{
	assert(conn->state == STATE_IDLE || conn->state == STATE_CLOSE);

	conn->state = STATE_CLOSE;

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

	conn->state = STATE_FREE;
	return DONE;
}

/*
 * Write data into provided file descriptor. If all data got written
 * the connection may change into idle state.
 */
static enum res
data_write(struct http_connection *conn)
{
	ssize_t s;
	size_t bsz = conn->bufpos;

	assert(conn->state == STATE_WRITE_DATA);

	if (conn->iosz < (off_t)bsz)
		bsz = conn->iosz;

	s = write(conn->req->outfd, conn->buf, bsz);

	if (s == -1) {
		warn("%s: data write", http_info(conn->req->uri));
		return http_failed(conn);
	}

	conn->bufpos -= s;
	conn->iosz -= s;
	memmove(conn->buf, conn->buf + s, conn->bufpos);

	/* check if regular file transfer is finished */
	if (!conn->chunked && conn->iosz == 0)
		return http_done(conn, HTTP_OK);

	/* all data written, switch back to read */
	if (conn->bufpos == 0 || conn->iosz == 0) {
		if (conn->chunked)
			conn->state = STATE_RESPONSE_CHUNKED_TRAILER;
		else
			conn->state = STATE_RESPONSE_DATA;
		return http_read(conn);
	}

	/* still more data to write in buffer */
	return WANT_POLLOUT;
}

/*
 * Do one IO call depending on the connection state.
 * Return WANT_POLLIN or WANT_POLLOUT to poll for more data.
 * If 0 is returned this stage is finished and the protocol should move
 * to the next stage by calling http_nextstep(). On error return -1.
 */
static enum res
http_handle(struct http_connection *conn)
{
	assert (conn->pfd != NULL && conn->pfd->revents != 0);

	switch (conn->state) {
	case STATE_CONNECT:
		return http_finish_connect(conn);
	case STATE_TLSCONNECT:
		return http_tls_handshake(conn);
	case STATE_REQUEST:
		return http_write(conn);
	case STATE_RESPONSE_STATUS:
	case STATE_RESPONSE_HEADER:
	case STATE_RESPONSE_DATA:
	case STATE_RESPONSE_CHUNKED_HEADER:
	case STATE_RESPONSE_CHUNKED_TRAILER:
		return http_read(conn);
	case STATE_WRITE_DATA:
		return data_write(conn);
	case STATE_CLOSE:
		return http_close(conn);
	case STATE_IDLE:
		conn->state = STATE_RESPONSE_HEADER;
		return http_read(conn);
	case STATE_FREE:
		errx(1, "bad http state");
	}
	errx(1, "unknown http state");
}

/*
 * Initialisation done before pledge() call to load certificates.
 */
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
		err(1, "tls_load_file: %s", tls_default_ca_cert_file());
	tls_config_set_ca_mem(tls_config, tls_ca_mem, tls_ca_size);

	/* TODO initalize proxy settings */
}

void
proc_http(char *bind_addr, int fd)
{
	struct pollfd pfds[NPFDS];
	struct http_connection *conn, *nc;
	struct http_request *req, *nr;

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

	memset(&pfds, 0, sizeof(pfds));

	msgbuf_init(&msgq);
	msgq.fd = fd;

	for (;;) {
		time_t now;
		int timeout;
		size_t i;

		pfds[0].fd = fd;
		pfds[0].events = POLLIN;
		if (msgq.queued)
			pfds[0].events |= POLLOUT;

		i = 1;
		timeout = INFTIM;
		now = getmonotime();
		LIST_FOREACH(conn, &active, entry) {
			if (conn->state == STATE_WRITE_DATA)
				pfds[i].fd = conn->req->outfd;
			else
				pfds[i].fd = conn->fd;

			pfds[i].events = conn->events;
			conn->pfd = &pfds[i];
			i++;
			if (i > NPFDS)
				errx(1, "too many connections");
		}
		LIST_FOREACH(conn, &idle, entry) {
			if (conn->idle_time <= now)
				timeout = 0;
			else {
				int diff = conn->idle_time - now; 
				diff *= 1000;
				if (timeout == INFTIM || diff < timeout)
					timeout = diff;
			}
			pfds[i].fd = conn->fd;
			pfds[i].events = POLLIN;
			conn->pfd = &pfds[i];
			i++;
			if (i > NPFDS)
				errx(1, "too many connections");
		}

		if (poll(pfds, i, timeout) == -1)
			err(1, "poll");

		if (pfds[0].revents & POLLHUP)
			break;
		if (pfds[0].revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}
		if (pfds[0].revents & POLLIN) {
			size_t id;
			int outfd;
			char *uri;
			char *mod;

			outfd = io_recvfd(fd, &id, sizeof(id));
			io_str_read(fd, &uri);
			io_str_read(fd, &mod);

			/* queue up new requests */
			http_req_new(id, uri, mod, outfd);
		}

		now = getmonotime();
		/* process idle connections */
		LIST_FOREACH_SAFE(conn, &idle, entry, nc) {
			if (conn->pfd != NULL && conn->pfd->revents != 0)
				http_do(conn, http_handle);
			else if (conn->idle_time <= now)
				http_do(conn, http_close);

			if (conn->state == STATE_FREE)
				http_free(conn);
		}

		/* then active http requests */
		LIST_FOREACH_SAFE(conn, &active, entry, nc) {
			/* check if event is ready */
			if (conn->pfd != NULL && conn->pfd->revents != 0)
				http_do(conn, http_handle);

			if (conn->state == STATE_FREE)
				http_free(conn);
		}


		TAILQ_FOREACH_SAFE(req, &queue, entry, nr)
			if (!http_req_schedule(req))
				break;
	}

	exit(0);
}

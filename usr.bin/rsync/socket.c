/*	$Id: socket.c,v 1.3 2019/02/10 23:43:31 benno Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <poll.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Defines a resolved IP address for the host
 * There can be many, IPV4 or IPV6.
 */
struct	source {
	int		 family; /* PF_INET or PF_INET6 */
	char		 ip[INET6_ADDRSTRLEN]; /* formatted string */
	struct sockaddr_storage sa; /* socket */
	socklen_t	 salen; /* length of socket buffer */
};

/*
 * Connect to an IP address representing a host.
 * Return <0 on failure, 0 on try another address, >0 on success.
 */
static int
inet_connect(struct sess *sess, int *sd,
	const struct source *src, const char *host)
{
	int	 c, flags;

	if (-1 != *sd)
		close(*sd);

	LOG2(sess, "trying: %s, %s", src->ip, host);

	if (-1 == (*sd = socket(src->family, SOCK_STREAM, 0))) {
		ERR(sess, "socket");
		return -1;
	}

	/*
	 * Initiate blocking connection.
	 * We use the blocking connect() instead of passing NONBLOCK to
	 * the socket() function because we don't need to do anything
	 * while waiting for this to finish.
	 */

	c = connect(*sd,
		(const struct sockaddr *)&src->sa,
		src->salen);
	if (-1 == c) {
		if (ECONNREFUSED == errno ||
		    EHOSTUNREACH == errno) {
			WARNX(sess, "connect refused: "
				"%s, %s", src->ip, host);
			return 0;
		}
		ERR(sess, "connect");
		return -1;
	}

	/* Set up non-blocking mode. */

	if (-1 == (flags = fcntl(*sd, F_GETFL, 0))) {
		ERR(sess, "fcntl");
		return -1;
	} else if (-1 == fcntl(*sd, F_SETFL, flags|O_NONBLOCK)) {
		ERR(sess, "fcntl");
		return -1;
	}

	return 1;
}

/*
 * Resolve the socket addresses for host, both in IPV4 and IPV6.
 * Once completed, the "dns" pledge may be dropped.
 * Returns the addresses on success, NULL on failure (sz is always zero,
 * in this case).
 */
static struct source *
inet_resolve(struct sess *sess, const char *host, size_t *sz)
{
	struct addrinfo	 hints, *res0, *res;
	struct sockaddr	*sa;
	struct source	*src = NULL;
	size_t		 i, srcsz = 0;
	int		 error;

	*sz = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */

	error = getaddrinfo(host, "873", &hints, &res0);

	LOG2(sess, "resolving: %s", host);

	if (error == EAI_AGAIN || error == EAI_NONAME) {
		ERRX(sess, "DNS resolve error: %s: %s",
			host, gai_strerror(error));
		return NULL;
	} else if (error) {
		ERRX(sess, "DNS parse error: %s: %s",
			host, gai_strerror(error));
		return NULL;
	}

	/* Allocate for all available addresses. */

	for (res = res0; NULL != res; res = res->ai_next)
		if (res->ai_family == AF_INET ||
		    res->ai_family == AF_INET6)
			srcsz++;

	if (0 == srcsz) {
		ERRX(sess, "no addresses resolved: %s", host);
		freeaddrinfo(res0);
		return NULL;
	}

	src = calloc(srcsz, sizeof(struct source));
	if (NULL == src) {
		ERRX(sess, "calloc");
		freeaddrinfo(res0);
		return NULL;
	}

	for (i = 0, res = res0; NULL != res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;

		assert(i < srcsz);

		/* Copy the socket address. */

		src[i].salen = res->ai_addrlen;
		memcpy(&src[i].sa, res->ai_addr, src[i].salen);

		/* Format as a string, too. */

		sa = res->ai_addr;
		if (AF_INET == res->ai_family) {
			src[i].family = PF_INET;
			inet_ntop(AF_INET,
				&(((struct sockaddr_in *)sa)->sin_addr),
				src[i].ip, INET6_ADDRSTRLEN);
		} else {
			src[i].family = PF_INET6;
			inet_ntop(AF_INET6,
				&(((struct sockaddr_in6 *)sa)->sin6_addr),
				src[i].ip, INET6_ADDRSTRLEN);
		}

		LOG2(sess, "DNS resolved: %s: %s", host, src[i].ip);
		i++;
	}

	freeaddrinfo(res0);
	*sz = srcsz;
	return src;
}

/*
 * Process an rsyncd preamble line.
 * This is either free-form text or @RSYNCD commands.
 * Return <0 on failure, 0 on try more lines, >0 on finished.
 */
static int
protocol_line(struct sess *sess, const char *host, const char *cp)
{
	int	major, minor;

	if (strncmp(cp, "@RSYNCD: ", 9)) {
		LOG0(sess, "%s", cp);
		return 0;
	}

	cp += 9;
	while (isspace((unsigned char)*cp))
		cp++;

	/* @RSYNCD: OK indicates that we're finished. */

	if (0 == strcmp(cp, "OK"))
		return 1;

	/*
	 * Otherwise, all we have left is our version.
	 * There are two formats: x.y (w/submodule) and x.
	 */

	if (2 == sscanf(cp, "%d.%d", &major, &minor)) {
		sess->rver = major;
		return 0;
	} else if (1 == sscanf(cp, "%d", &major)) {
		sess->rver = major;
		return 0;
	}

	ERRX(sess, "rsyncd protocol error: unknown command");
	return -1;
}

/*
 * Pledges: dns, inet, unveil, rpath, cpath, wpath, stdio, fattr.
 *
 * Pledges (dry-run): -cpath, -wpath, -fattr.
 * Pledges (!preserve_times): -fattr.
 */
int
rsync_socket(const struct opts *opts, const struct fargs *f)
{
	struct sess	  sess;
	struct source	 *src = NULL;
	size_t		  i, srcsz = 0;
	int		  sd = -1, rc = 0, c;
	char		**args, buf[BUFSIZ];
	uint8_t		  byte;

	memset(&sess, 0, sizeof(struct sess));
	sess.lver = RSYNC_PROTOCOL;
	sess.opts = opts;

	assert(NULL != f->host);
	assert(NULL != f->module);

	if (NULL == (args = fargs_cmdline(&sess, f))) {
		ERRX1(&sess, "fargs_cmdline");
		return 0;
	}

	/* Resolve all IP addresses from the host. */

	if (NULL == (src = inet_resolve(&sess, f->host, &srcsz))) {
		ERRX1(&sess, "inet_resolve");
		free(args);
		return 0;
	}

	/* Drop the DNS pledge. */

	if (-1 == pledge("stdio rpath wpath cpath fattr inet unveil", NULL)) {
		ERR(&sess, "pledge");
		goto out;
	}

	/*
	 * Iterate over all addresses, trying to connect.
	 * When we succeed, then continue using the connected socket.
	 */

	assert(srcsz);
	for (i = 0; i < srcsz; i++) {
		c = inet_connect(&sess, &sd, &src[i], f->host);
		if (c < 0) {
			ERRX1(&sess, "inet_connect");
			goto out;
		} else if (c > 0)
			break;
	}

	/* Drop the inet pledge. */

	if (-1 == pledge("stdio rpath wpath cpath fattr unveil", NULL)) {
		ERR(&sess, "pledge");
		goto out;
	}

	if (i == srcsz) {
		ERRX(&sess, "cannot connect to host: %s", f->host);
		goto out;
	}

	/* Initiate with the rsyncd version and module request. */

	LOG2(&sess, "connected: %s, %s", src[i].ip, f->host);

	(void)snprintf(buf, sizeof(buf), "@RSYNCD: %d", sess.lver);
	if ( ! io_write_line(&sess, sd, buf)) {
		ERRX1(&sess, "io_write_line");
		goto out;
	}

	LOG2(&sess, "requesting module: %s, %s", f->module, f->host);

	if ( ! io_write_line(&sess, sd, f->module)) {
		ERRX1(&sess, "io_write_line");
		goto out;
	}

	/*
	 * Now we read the server's response, byte-by-byte, one newline
	 * terminated at a time, limited to BUFSIZ line length.
	 * For this protocol version, this consists of either @RSYNCD
	 * followed by some text (just "ok" and the remote version) or
	 * the message of the day.
	 */

	for (;;) {
		for (i = 0; i < sizeof(buf); i++) {
			if ( ! io_read_byte(&sess, sd, &byte)) {
				ERRX1(&sess, "io_read_byte");
				goto out;
			}
			if ('\n' == (buf[i] = byte))
				break;
		}
		if (i == sizeof(buf)) {
			ERRX(&sess, "line buffer overrun");
			goto out;
		} else if (0 == i)
			continue;

		/*
		 * The rsyncd protocol isn't very clear as to whether we
		 * get a CRLF or not: I don't actually see this being
		 * transmitted over the wire.
		 */

		assert(i > 0);
		buf[i] = '\0';
		if ('\r' == buf[i - 1])
			buf[i - 1] = '\0';

		if ((c = protocol_line(&sess, f->host, buf)) < 0) {
			ERRX1(&sess, "protocol_line");
			goto out;
		} else if (c > 0)
			break;
	}

	/*
	 * Now we've exchanged all of our protocol information.
	 * We want to send our command-line arguments over the wire,
	 * each with a newline termination.
	 * Use the same arguments when invoking the server, but leave
	 * off the binary name(s).
	 * Emit a standalone newline afterward.
	 */

	if (FARGS_RECEIVER == f->mode || FARGS_SENDER == f->mode)
		i = 3; /* ssh host rsync... */
	else
		i = 1; /* rsync... */

	for ( ; NULL != args[i]; i++)
		if ( ! io_write_line(&sess, sd, args[i])) {
			ERRX1(&sess, "io_write_line");
			goto out;
		}
	if ( ! io_write_byte(&sess, sd, '\n')) {
		ERRX1(&sess, "io_write_line");
		goto out;
	}

	/*
	 * All data after this point is going to be multiplexed, so turn
	 * on the multiplexer for our reads and writes.
	 */

	/* Protocol exchange: get the random seed. */

	if ( ! io_read_int(&sess, sd, &sess.seed)) {
		ERRX1(&sess, "io_read_int");
		goto out;
	}

	/* Now we've completed the handshake. */

	if (sess.rver < sess.lver) {
		ERRX(&sess, "remote protocol is older "
			"than our own (%" PRId32 " < %" PRId32 "): "
			"this is not supported",
			sess.rver, sess.lver);
		goto out;
	}

	sess.mplex_reads = 1;
	LOG2(&sess, "read multiplexing enabled");

	LOG2(&sess, "socket detected client version %" PRId32
		", server version %" PRId32 ", seed %" PRId32,
		sess.lver, sess.rver, sess.seed);

	assert(FARGS_RECEIVER == f->mode);

	LOG2(&sess, "client starting receiver: %s", f->host);
	if ( ! rsync_receiver(&sess, sd, sd, f->sink)) {
		ERRX1(&sess, "rsync_receiver");
		goto out;
	}

#if 0
	/* Probably the EOF. */
	if (io_read_check(&sess, sd))
		WARNX(&sess, "data remains in read pipe");
#endif

	rc = 1;
out:
	free(src);
	free(args);
	if (-1 != sd)
		close(sd);
	return rc;
}

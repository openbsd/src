/*	$OpenBSD: socket.c,v 1.32 2022/08/02 18:09:20 job Exp $ */
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
#include <err.h>

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
 * Try to bind to a local IP address matching the addres family passed.
 * Return -1 on failure to bind to any address, 0 on success.
 */
static int
inet_bind(int s, sa_family_t af, const struct source *bsrc, size_t bsrcsz)
{
	size_t i;

	if (bsrc == NULL)
		return 0;
	for (i = 0; i < bsrcsz; i++) {
		if (bsrc[i].family != af)
			continue;
		if (bind(s, (const struct sockaddr *)&bsrc[i].sa,
		    bsrc[i].salen) == -1)
			continue;
		return 0;
	}
	return -1;
}

/*
 * Connect to an IP address representing a host.
 * Return <0 on failure, 0 on try another address, >0 on success.
 */
static int
inet_connect(int *sd, const struct source *src, const char *host,
    const struct source *bsrc, size_t bsrcsz)
{
	struct pollfd	pfd;
	socklen_t	optlen;
	int		c;
	int		optval;

	if (*sd != -1)
		close(*sd);

	LOG2("trying: %s, %s", src->ip, host);

	if ((*sd = socket(src->family, SOCK_STREAM | SOCK_NONBLOCK, 0))
	    == -1) {
		ERR("socket");
		return -1;
	}

	if (inet_bind(*sd, src->family, bsrc, bsrcsz) == -1) {
		ERR("bind");
		return -1;
	}

	/*
	 * Initiate blocking connection.
	 * We use non-blocking connect() so we can poll() for contimeout.
	 */

	if ((c = connect(*sd, (const struct sockaddr *)&src->sa, src->salen))
	    != 0 && errno == EINPROGRESS) {
		pfd.fd = *sd;
		pfd.events = POLLOUT;
		switch (c = poll(&pfd, 1, poll_contimeout)) {
		case 1:
			optlen = sizeof(optval);
			if ((c = getsockopt(*sd, SOL_SOCKET, SO_ERROR, &optval,
			    &optlen)) == 0) {
				errno = optval;
				if (optval != 0)
					c = -1;
			}
			break;
		case 0:
			errno = ETIMEDOUT;
			WARNX("connect timeout: %s, %s", src->ip, host);
			return 0;
		default:
			ERR("poll failed");
			return -1;
		}
	}
	if (c == -1) {
		if (errno == EADDRNOTAVAIL)
			return 0;
		if (errno == ECONNREFUSED || errno == EHOSTUNREACH) {
			WARNX("connect refused: %s, %s", src->ip, host);
			return 0;
		}
		ERR("connect");
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
inet_resolve(struct sess *sess, const char *host, size_t *sz, int passive)
{
	struct addrinfo	 hints, *res0, *res;
	struct sockaddr	*sa;
	struct source	*src = NULL;
	const char	*port = sess->opts->port;
	size_t		 i, srcsz = 0;
	int		 error;

	*sz = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (passive) {
		hints.ai_flags = SOCK_STREAM;
		port = NULL;
	}

	error = getaddrinfo(host, port, &hints, &res0);

	LOG2("resolving: %s", host);

	if (error == EAI_AGAIN || error == EAI_NONAME) {
		ERRX("could not resolve hostname %s: %s",
		    host, gai_strerror(error));
		return NULL;
	} else if (error == EAI_SERVICE) {
		ERRX("could not resolve service rsync: %s",
		    gai_strerror(error));
		return NULL;
	} else if (error) {
		ERRX("getaddrinfo: %s: %s", host, gai_strerror(error));
		return NULL;
	}

	/* Allocate for all available addresses. */

	for (res = res0; res != NULL; res = res->ai_next)
		if (res->ai_family == AF_INET ||
		    res->ai_family == AF_INET6)
			srcsz++;

	if (srcsz == 0) {
		ERRX("no addresses resolved: %s", host);
		freeaddrinfo(res0);
		return NULL;
	}

	src = calloc(srcsz, sizeof(struct source));
	if (src == NULL) {
		ERRX("calloc");
		freeaddrinfo(res0);
		return NULL;
	}

	for (i = 0, res = res0; res != NULL; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;

		assert(i < srcsz);

		/* Copy the socket address. */

		src[i].salen = res->ai_addrlen;
		memcpy(&src[i].sa, res->ai_addr, src[i].salen);

		/* Format as a string, too. */

		sa = res->ai_addr;
		if (res->ai_family == AF_INET) {
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

		LOG2("hostname resolved: %s: %s", host, src[i].ip);
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
protocol_line(struct sess *sess, __attribute__((unused)) const char *host,
    const char *cp)
{
	int	major, minor;

	if (strncmp(cp, "@RSYNCD: ", 9)) {
		if (sess->opts->no_motd == 0)
			LOG1("%s", cp);
		return 0;
	}

	cp += 9;
	while (isspace((unsigned char)*cp))
		cp++;

	/* @RSYNCD: OK indicates that we're finished. */

	if (strcmp(cp, "OK") == 0)
		return 1;

	/*
	 * Otherwise, all we have left is our version.
	 * There are two formats: x.y (w/submodule) and x.
	 */

	if (sscanf(cp, "%d.%d", &major, &minor) == 2) {
		sess->rver = major;
		return 0;
	} else if (sscanf(cp, "%d", &major) == 1) {
		sess->rver = major;
		return 0;
	}

	ERRX("rsyncd protocol error: unknown command");
	return -1;
}

/*
 * Connect to a remote rsync://-enabled server sender.
 * Returns exit code 0 on success, 1 on failure.
 */
int
rsync_connect(const struct opts *opts, int *sd, const struct fargs *f)
{
	struct sess	  sess;
	struct source	 *src = NULL, *bsrc = NULL;
	size_t		  i, srcsz = 0, bsrcsz = 0;
	int		  c, rc = 1;

	if (pledge("stdio unix rpath wpath cpath dpath inet fattr chown dns getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");

	memset(&sess, 0, sizeof(struct sess));
	sess.opts = opts;

	assert(f->host != NULL);

	/* Resolve all IP addresses from the host. */

	if ((src = inet_resolve(&sess, f->host, &srcsz, 0)) == NULL) {
		ERRX1("inet_resolve");
		exit(1);
	}
	if (opts->address != NULL)
		if ((bsrc = inet_resolve(&sess, opts->address, &bsrcsz, 1)) ==
		    NULL) {
			ERRX1("inet_resolve bind");
			exit(1);
		}

	/* Drop the DNS pledge. */

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw inet unveil",
	    NULL) == -1) {
		ERR("pledge");
		exit(1);
	}

	/*
	 * Iterate over all addresses, trying to connect.
	 * When we succeed, then continue using the connected socket.
	 */

	assert(srcsz);
	for (i = 0; i < srcsz; i++) {
		c = inet_connect(sd, &src[i], f->host, bsrc, bsrcsz);
		if (c < 0) {
			ERRX1("inet_connect");
			goto out;
		} else if (c > 0)
			break;
	}

	/* Drop the inet pledge. */
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1) {
		ERR("pledge");
		goto out;
	}

	if (i == srcsz) {
		ERRX("cannot connect to host: %s", f->host);
		goto out;
	}

	LOG2("connected: %s, %s", src[i].ip, f->host);

	free(src);
	free(bsrc);
	return 0;
out:
	free(src);
	free(bsrc);
	if (*sd != -1)
		close(*sd);
	return rc;
}

/*
 * Talk to a remote rsync://-enabled server sender.
 * Returns exit code 0 on success, 1 on failure, 2 on failure with
 * incompatible protocols.
 */
int
rsync_socket(const struct opts *opts, int sd, const struct fargs *f)
{
	struct sess	  sess;
	size_t		  i, skip;
	int		  c, rc = 1;
	char		**args, buf[BUFSIZ];
	uint8_t		  byte;

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil",
	    NULL) == -1)
		err(ERR_IPC, "pledge");

	memset(&sess, 0, sizeof(struct sess));
	sess.lver = RSYNC_PROTOCOL;
	sess.opts = opts;

	assert(f->host != NULL);
	assert(f->module != NULL);

	args = fargs_cmdline(&sess, f, &skip);

	/* Initiate with the rsyncd version and module request. */

	(void)snprintf(buf, sizeof(buf), "@RSYNCD: %d", sess.lver);
	if (!io_write_line(&sess, sd, buf)) {
		ERRX1("io_write_line");
		goto out;
	}

	LOG2("requesting module: %s, %s", f->module, f->host);

	if (!io_write_line(&sess, sd, f->module)) {
		ERRX1("io_write_line");
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
			if (!io_read_byte(&sess, sd, &byte)) {
				ERRX1("io_read_byte");
				goto out;
			}
			if ((buf[i] = byte) == '\n')
				break;
		}
		if (i == sizeof(buf)) {
			ERRX("line buffer overrun");
			goto out;
		} else if (i == 0)
			continue;

		/*
		 * The rsyncd protocol isn't very clear as to whether we
		 * get a CRLF or not: I don't actually see this being
		 * transmitted over the wire.
		 */

		assert(i > 0);
		buf[i] = '\0';
		if (buf[i - 1] == '\r')
			buf[i - 1] = '\0';

		if ((c = protocol_line(&sess, f->host, buf)) < 0) {
			ERRX1("protocol_line");
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

	for (i = skip ; args[i] != NULL; i++)
		if (!io_write_line(&sess, sd, args[i])) {
			ERRX1("io_write_line");
			goto out;
		}
	if (!io_write_byte(&sess, sd, '\n')) {
		ERRX1("io_write_line");
		goto out;
	}

	/*
	 * All data after this point is going to be multiplexed, so turn
	 * on the multiplexer for our reads and writes.
	 */

	/* Protocol exchange: get the random seed. */

	if (!io_read_int(&sess, sd, &sess.seed)) {
		ERRX1("io_read_int");
		goto out;
	}

	/* Now we've completed the handshake. */

	if (sess.rver < sess.lver) {
		ERRX("remote protocol is older than our own (%d < %d): "
		    "this is not supported",
		    sess.rver, sess.lver);
		rc = 2;
		goto out;
	}

	sess.mplex_reads = 1;
	LOG2("read multiplexing enabled");

	LOG2("socket detected client version %d, server version %d, seed %d",
	    sess.lver, sess.rver, sess.seed);

	assert(f->mode == FARGS_RECEIVER);

	LOG2("client starting receiver: %s", f->host);
	if (!rsync_receiver(&sess, sd, sd, f->sink)) {
		ERRX1("rsync_receiver");
		goto out;
	}

#if 0
	/* Probably the EOF. */
	if (io_read_check(&sess, sd))
		WARNX("data remains in read pipe");
#endif

	rc = 0;
out:
	free(args);
	return rc;
}

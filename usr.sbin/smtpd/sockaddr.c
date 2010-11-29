/*
 * Copyright (c) 2010		Eric Faurot	<eric@faurot.net>
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
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "dnsutil.h"

int
sockaddr_from_rr(struct sockaddr *sa, struct rr *rr)
{
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;

	if (rr->rr_class != C_IN)
		return (-1);

	switch (rr->rr_type) {
	case T_A:
		sin = (struct sockaddr_in*)sa;
		memset(sin, 0, sizeof *sin);
		sin->sin_len = sizeof *sin;
		sin->sin_family = PF_INET;
		sin->sin_addr = rr->rr.in_a.addr;
		sin->sin_port = 0;
		return (0);
	case T_AAAA:
		sin6 = (struct sockaddr_in6*)sa;
		memset(sin6, 0, sizeof *sin6);
		sin6->sin6_len = sizeof *sin6;
		sin6->sin6_family = PF_INET6;
		sin6->sin6_addr = rr->rr.in_aaaa.addr6;
		sin6->sin6_port = 0;
		return (0);

	default:
		break;
	}

	return (-1);
}

int
sockaddr_from_str(struct sockaddr *sa, int family, const char *str)
{
	struct in_addr		 ina;
	struct in6_addr		 in6a;
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;

	switch (family) {
	case PF_UNSPEC:
		if (sockaddr_from_str(sa, PF_INET, str) == 0)
			return (0);
		return sockaddr_from_str(sa, PF_INET6, str);

	case PF_INET:
		if (inet_pton(PF_INET, str, &ina) != 1)
			return (-1);

		sin = (struct sockaddr_in *)sa;
		memset(sin, 0, sizeof *sin);
		sin->sin_len = sizeof(struct sockaddr_in);
		sin->sin_family = PF_INET;
		sin->sin_addr.s_addr = ina.s_addr;
		return (0);

	case PF_INET6:
		if (inet_pton(PF_INET6, str, &in6a) != 1)
			return (-1);

		sin6 = (struct sockaddr_in6 *)sa;
		memset(sin6, 0, sizeof *sin6);
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_family = PF_INET6;
		sin6->sin6_addr = in6a;
		return (0);

	default:
		break;
	}

	return (-1);
}

void
sockaddr_set_port(struct sockaddr *sa, int portno)
{
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;

	switch (sa->sa_family) {
	case PF_INET:
		sin = (struct sockaddr_in *)sa;
		sin->sin_port = htons(portno);
		break;
	case PF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		sin6->sin6_port = htons(portno);
		break;
	}
}

int
sockaddr_connect(const struct sockaddr *sa, int socktype)
{
	int errno_save, flags, sock;

	if ((sock = socket(sa->sa_family, socktype, 0)) == -1)
		goto fail;

	if ((flags = fcntl(sock, F_GETFL, 0)) == -1)
		goto fail;

	flags |= O_NONBLOCK;

	if ((flags = fcntl(sock, F_SETFL, flags)) == -1)
		goto fail;

	if (connect(sock, sa, sa->sa_len) == -1) {
		if (errno == EINPROGRESS)
			return (sock);
		goto fail;
	}

	return (sock);

    fail:

	if (sock != -1) {
		errno_save = errno;
		close(sock);
		errno = errno_save;
	}

	return (-1);
}

int
sockaddr_listen(const struct sockaddr *sa, int socktype)
{
	int errno_save, sock;

	if ((sock = socket(sa->sa_family, socktype, 0)) == -1)
		return (-1);

	if (bind(sock, sa, sa->sa_len) == -1) {
		errno_save = errno;
		close(sock);
		errno = errno_save;
		return (-1);
	}

        return (sock);
}

/*	$OpenBSD: config.c,v 1.2 2004/06/02 10:08:59 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

int		host_v4(const char *, struct sockaddr *, u_int8_t *);
int		host_v6(const char *, struct sockaddr *);

int
check_file_secrecy(int fd, const char *fname)
{
	struct stat	st;

	if (fstat(fd, &st)) {
		log_warn("cannot stat %s", fname);
		return (-1);
	}

	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("%s: owner not root or current user", fname);
		return (-1);
	}

	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}

	return (0);
}

int
host(const char *s, struct sockaddr *sa, u_int8_t *len)
{
	int			 done = 0;
	int			 mask;
	char			*p, *q, *ps;

	if ((p = strrchr(s, '/')) != NULL) {
		errno = 0;
		mask = strtol(p+1, &q, 0);
		if (errno == ERANGE || !q || *q || mask > 128 || q == (p+1)) {
			log_warnx("invalid netmask");
			return (0);
		}
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			fatal("host: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
	} else {
		if ((ps = strdup(s)) == NULL)
			fatal("host: strdup");
		mask = 128;
	}

	/* IPv4 address? */
	if (!done)
		done = host_v4(s, sa, len);

	/* IPv6 address? */
	if (!done) {
		done = host_v6(ps, sa);
		*len = mask;
	}

	free(ps);

	return (done);
}

int
host_v4(const char *s, struct sockaddr *sa, u_int8_t *len)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sa_in;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (0);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (0);
	}

	sa_in = (struct sockaddr_in *)sa;
	sa_in->sin_len = sizeof(struct sockaddr_in);
	sa_in->sin_family = AF_INET;
	sa_in->sin_addr.s_addr = ina.s_addr;
	*len = bits;

	return (1);
}

int
host_v6(const char *s, struct sockaddr *sa)
{
	struct addrinfo		 hints, *res;
	struct sockaddr_in6	*sa_in6;

	sa_in6 = (struct sockaddr_in6 *)sa;
	sa_in6->sin6_len = sizeof(struct sockaddr_in6);
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		sa_in6->sin6_family = AF_INET6;
		memcpy(&sa_in6->sin6_addr,
		    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_scope_id =
		    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;

		freeaddrinfo(res);
		return (1);
	}

	return (0);
}

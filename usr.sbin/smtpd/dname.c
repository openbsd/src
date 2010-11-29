/*
 * Copyright (c) 2009,2010	Eric Faurot	<eric@faurot.net>
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dnsutil.h"

size_t
dname_len(const char *dname)
{
	size_t	l;

	l = 0;

	while(dname[l])
		l += dname[l] + 1;

	return l;
}

size_t
dname_depth(const char *dname)
{
	size_t	l;

	l = 0;

	while(*dname) {
		l += 1;
		dname += *dname + 1;
	}

	return l;
}

const char*
dname_up(const char *dname, unsigned int n)
{
	while(n--) {
		if (dname[0] == '\0')
			return (NULL);
		dname += *dname + 1;
	}
	return (dname);
}


int
dname_is_in(const char *dname, const char *domain)
{
	size_t	l, ld;

	l = dname_depth(dname);
	ld = dname_depth(domain);

	if (ld > l)
		return (0);

	dname = dname_up(dname, l - ld);

	if (strcasecmp(dname, domain) == 0)
		return (1);

	return (0);
}

int
dname_is_reverse(const char *dname)
{
	static int	init = 0;
	static char	arpa[15];

	if (init == 0) {
		init = 1;
		dname_from_fqdn("in-addr.arpa.", arpa, sizeof arpa);
	}

	return (dname_is_in(dname, arpa));
}

int
dname_is_wildcard(const char *dname)
{
	return (dname[0] == 1 && dname[1] == '*');
}

int
dname_check_label(const char *s, size_t l)
{
	if (l == 0 || l > 63)
		return (-1);

	for(l--; l; l--, s++)
		if (!(isalnum(*s) || *s == '_' || *s == '-'))
			return (-1);

	return (0);
}

ssize_t
dname_from_fqdn(const char *str, char *dst, size_t max)
{
	ssize_t	 res;
	size_t	 l, n;
	char	*d;

	res = 0;
	for(;;) {

		d = strchr(str, '.');
		if (d == NULL)
			return (-1);

		l = (d - str);
		if (l > 63)
			return (-1);
		if ((res || l) && (dname_check_label(str, l) == -1))
			return (-1);

		res += l + 1;

		if (dst) {
			*dst++ = l;
			max -= 1;
			n = (l > max) ? max : l;
			if (n)
				memmove(dst, str, n);
			max -= n;
			if (max == 0)
				dst = NULL;
			else
				dst += n;
		}

		str = d + 1;
		if (*str == '\0')
			break;
	}

	return (res);
}

ssize_t
dname_from_sockaddr(const struct sockaddr *sa, char *dst, size_t max)
{
	const struct in6_addr	*in6_addr;
	in_addr_t		 addr;
	char			 buf[80];

	switch (sa->sa_family) {
	case AF_INET:
		addr = ((const struct sockaddr_in *)sa)->sin_addr.s_addr;
		snprintf(buf, sizeof (buf),
		    "%d.%d.%d.%d.in-addr.arpa.",
		    (addr >> 24) & 0xff,
		    (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff,
		    addr & 0xff);
		break;
	case AF_INET6:
		in6_addr = &((const struct sockaddr_in6 *)sa)->sin6_addr;
		snprintf(buf, sizeof (buf),
		    "%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d."
		    "%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d."
		    "ip6.arpa.",
		    in6_addr->s6_addr[15] & 0xf,
		    (in6_addr->s6_addr[15] >> 4) & 0xf,
		    in6_addr->s6_addr[14] & 0xf,
		    (in6_addr->s6_addr[14] >> 4) & 0xf,
		    in6_addr->s6_addr[13] & 0xf,
		    (in6_addr->s6_addr[13] >> 4) & 0xf,
		    in6_addr->s6_addr[12] & 0xf,
		    (in6_addr->s6_addr[12] >> 4) & 0xf,
		    in6_addr->s6_addr[11] & 0xf,
		    (in6_addr->s6_addr[11] >> 4) & 0xf,
		    in6_addr->s6_addr[10] & 0xf,
		    (in6_addr->s6_addr[10] >> 4) & 0xf,
		    in6_addr->s6_addr[9] & 0xf,
		    (in6_addr->s6_addr[9] >> 4) & 0xf,
		    in6_addr->s6_addr[8] & 0xf,
		    (in6_addr->s6_addr[8] >> 4) & 0xf,
		    in6_addr->s6_addr[7] & 0xf,
		    (in6_addr->s6_addr[7] >> 4) & 0xf,
		    in6_addr->s6_addr[6] & 0xf,
		    (in6_addr->s6_addr[6] >> 4) & 0xf,
		    in6_addr->s6_addr[5] & 0xf,
		    (in6_addr->s6_addr[5] >> 4) & 0xf,
		    in6_addr->s6_addr[4] & 0xf,
		    (in6_addr->s6_addr[4] >> 4) & 0xf,
		    in6_addr->s6_addr[3] & 0xf,
		    (in6_addr->s6_addr[3] >> 4) & 0xf,
		    in6_addr->s6_addr[2] & 0xf,
		    (in6_addr->s6_addr[2] >> 4) & 0xf,
		    in6_addr->s6_addr[1] & 0xf,
		    (in6_addr->s6_addr[1] >> 4) & 0xf,
		    in6_addr->s6_addr[0] & 0xf,
		    (in6_addr->s6_addr[0] >> 4) & 0xf);
		break;
	default:
		return (-1);
	}

	return dname_from_fqdn(buf, dst, max);
}

/*	$OpenBSD: rde_trie_test.c,v 1.5 2018/09/18 15:15:32 claudio Exp $ */

/*
 * Copyright (c) 2018 Claudio Jeker <claudio@openbsd.org>
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

#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>

#include "bgpd.h"
#include "rde.h"

int roa;
int orlonger;

static int
host_v4(const char *s, struct bgpd_addr *h, u_int8_t *len)
{
	struct in_addr ina = { 0 };
	int bits = 32;

	memset(h, 0, sizeof(*h));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (0);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (0);
	}
	h->aid = AID_INET;
	h->v4.s_addr = ina.s_addr;
	*len = bits;
	return (1);
}

static int
host_v6(const char *s, struct bgpd_addr *h, u_int8_t *len)
{
	struct addrinfo hints, *res;
	const char *errstr;
	char *p;
	int mask = 128;

	memset(h, 0, sizeof(*h));
	if ((p = strrchr(s, '/')) != NULL) {
		mask = strtonum(p + 1, 0, 128, &errstr);
		if (errstr)
			return (0);
		*p = '\0';
	}

        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_DGRAM; /*dummy*/
        hints.ai_flags = AI_NUMERICHOST;
        if (getaddrinfo(s, "0", &hints, &res) == 0) {
		h->aid = AID_INET6;
		memcpy(&h->v6, &res->ai_addr->sa_data[6], sizeof(h->v6));
                freeaddrinfo(res);
		*len = mask;
		if (p)
			*p = '/';
                return (1);
        }
	if (p)
		*p = '/';

        return (0);
}

static int
host_l(char *s, struct bgpd_addr *h, u_int8_t *len)
{
	if (host_v4(s, h, len))
		return (1);
	if (host_v6(s, h, len))
		return (1);
	return (0);
}

static const char *
print_prefix(struct bgpd_addr *p)
{
	static char buf[48];

	if (p->aid == AID_INET) {
		if (inet_ntop(AF_INET, &p->ba, buf, sizeof(buf)) == NULL)
			return "?";
	} else if (p->aid == AID_INET6) {
		if (inet_ntop(AF_INET6, &p->ba, buf, sizeof(buf)) == NULL)
			return "?";
	} else {
		return "???";
	}
	return buf;
}

static void
parse_file(FILE *in, struct trie_head *th)
{
	const char *errstr;
	char *line, *s;
	struct bgpd_addr prefix;
	u_int8_t plen;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		int state = 0;
		u_int8_t min = 255, max = 255, maskmax = 0;

		while ((s = strsep(&line, " \t\n"))) {
			if (*s == '\0')
				continue;
			switch (state) {
			case 0:
				if (!host_l(s, &prefix, &plen))
					errx(1, "%s: could not parse "
					    "prefix \"%s\"", __func__, s);
				if (prefix.aid == AID_INET6)
					maskmax = 128;
				else
					maskmax = 32;
				break;
			case 1:	
				min = strtonum(s, 0, maskmax, &errstr);
				if (errstr != NULL)
					errx(1, "min is %s: %s", errstr, s);
				break;
			case 2:
				max = strtonum(s, 0, maskmax, &errstr);
				if (errstr != NULL)
					errx(1, "max is %s: %s", errstr, s);
				break;
			default:
				errx(1, "could not parse \"%s\", confused", s);
			}
			state++;
		}
		if (state == 0)
			continue;
		if (max == 255)
			max = maskmax;
		if (min == 255)
			min = plen;

		if (trie_add(th, &prefix, plen, min, max) != 0)
			errx(1, "trie_add(%s, %u, %u, %u) failed",
			    print_prefix(&prefix), plen, min, max);

		free(line);
	}
}

static void
parse_roa_file(FILE *in, struct trie_head *th)
{
	const char *errstr;
	char *line, *s;
	struct as_set *aset = NULL;
	struct roa_set rs;
	struct bgpd_addr prefix;
	u_int8_t plen;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		int state = 0;
		u_int32_t as;
		u_int8_t max = 0;

		while ((s = strsep(&line, " \t\n"))) {
			if (*s == '\0')
				continue;
			if (strcmp(s, "source-as") == 0) {
				state = 4;
				continue;
			}
			if (strcmp(s, "maxlen") == 0) {
				state = 2;
				continue;
			}
			if (strcmp(s, "prefix") == 0) {
				state = 0;
				continue;
			}
			switch (state) {
			case 0:
				if (!host_l(s, &prefix, &plen))
					errx(1, "%s: could not parse "
					    "prefix \"%s\"", __func__, s);
				break;
			case 2:
				max = strtonum(s, 0, 128, &errstr);
				if (errstr != NULL)
					errx(1, "max is %s: %s", errstr, s);
				break;
			case 4:
				as = strtonum(s, 0, UINT_MAX, &errstr);
				if (errstr != NULL)
					errx(1, "source-as is %s: %s", errstr,
					    s);
				break;
			default:
				errx(1, "could not parse \"%s\", confused", s);
			}
		}

		if (state == 0) {
			as_set_prep(aset);
			if (trie_roa_add(th, &prefix, plen, aset) != 0)
				errx(1, "trie_roa_add(%s, %u) failed",
				    print_prefix(&prefix), plen);
			aset = NULL;
		} else {
			if (aset == NULL) {
				if ((aset = as_set_new("", 1, sizeof(rs))) ==
				    NULL)
					err(1, "as_set_new");
			}
			rs.as = as;
			rs.maxlen = max;
			if (as_set_add(aset, &rs, 1) != 0)
				err(1, "as_set_add");
		}

		free(line);
	}
}

static void
test_file(FILE *in, struct trie_head *th)
{
	char *line;
	struct bgpd_addr prefix;
	u_int8_t plen;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		if (!host_l(line, &prefix, &plen))
			errx(1, "%s: could not parse prefix \"%s\"",
			    __func__, line);
		printf("%s/%u ", print_prefix(&prefix), plen);
		if (trie_match(th, &prefix, plen, orlonger))
			printf("MATCH\n");
		else
			printf("miss\n");
		free(line);
	}
}

static void
test_roa_file(FILE *in, struct trie_head *th)
{
	const char *errstr;
	char *line, *s;
	struct bgpd_addr prefix;
	u_int8_t plen;
	u_int32_t as;
	int r;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		s = strchr(line, ' ');
		if (s)
			*s++ = '\0';
		if (!host_l(line, &prefix, &plen))
			errx(1, "%s: could not parse prefix \"%s\"",
			    __func__, line);
		if (s)
			s = strstr(s, "source-as");
		if (s) {
			s += strlen("source-as");
			as = strtonum(s, 0, UINT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "source-as is %s: %s", errstr, s);
		} else
			as = 0;
		printf("%s/%u source-as %u is ",
		    print_prefix(&prefix), plen, as);
		r = trie_roa_check(th, &prefix, plen, as);
		switch (r) {
		case ROA_UNKNOWN:
			printf("not found\n");
			break;
		case ROA_VALID:
			printf("VALID\n");
			break;
		case ROA_INVALID:
			printf("invalid\n");
			break;
		default:
			printf("UNEXPECTED %d\n", r);
			break;
		}
		free(line);
	}
}

static void
usage(void)
{
        extern char *__progname;
	fprintf(stderr, "usage: %s [-or] prefixfile testfile\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct trie_head th = { 0 };
	FILE *in, *tin;
	int ch;

	while ((ch = getopt(argc, argv, "or")) != -1) {
		switch (ch) {
		case 'o':
			orlonger = 1;
			break;
		case 'r':
			roa = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	in = fopen(argv[0], "r");
	if (in == NULL)
		err(1, "fopen(%s)", argv[0]);
	tin = fopen(argv[1], "r");
	if (tin == NULL)
		err(1, "fopen(%s)", argv[1]);

	if (roa)
		parse_roa_file(in, &th);
	else
		parse_file(in, &th);
	/* trie_dump(&th); */
	if (trie_equal(&th, &th) == 0)
		errx(1, "trie_equal failure");
	if (roa)
		test_roa_file(tin, &th);
	else
		test_file(tin, &th);

	trie_free(&th);
}

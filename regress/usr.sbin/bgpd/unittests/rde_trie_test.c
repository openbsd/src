/*	$OpenBSD: rde_trie_test.c,v 1.3 2018/09/10 20:51:59 benno Exp $ */

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
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>

#include "bgpd.h"
#include "rde.h"


static int
host_v4(const char *s, struct bgpd_addr *h, u_int8_t *len, int *orl)
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
host_v6(const char *s, struct bgpd_addr *h, u_int8_t *len, int *orl)
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
		*p = '/';
                return (1);
        }
	*p = '/';

        return (0);
}

static int
host_l(char *s, struct bgpd_addr *h, u_int8_t *len, int *orl)
{
	char *c, *t;

	*orl = 0;
	if ((c = strchr(s, '\t')) != NULL) {
		if (c[1] == '1') {
			*orl = 1;
		}
		*c = '\0';
	}

	if (host_v4(s, h, len, orl))
		return (1);
	if (host_v6(s, h, len, orl))
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
	u_int8_t plen, min, max, maskmax;
	int foo;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		int state = 0;
		while ((s = strsep(&line, " \t"))) {
			if (*s == '\0')
				break;
			switch (state) {
			case 0:
				if (!host_l(s, &prefix, &plen, &foo))
					errx(1, "could not parse prefix \"%s\"",
					    s);
				break;
			case 1:	
				if (prefix.aid == AID_INET6)
					maskmax = 128;
				else
					maskmax = 32;
				min = strtonum(s, 0, maskmax, &errstr);
				if (errstr != NULL)
					errx(1, "min is %s: %s", errstr, s);
				break;
			case 2:
				if (prefix.aid == AID_INET6)
					maskmax = 128;
				else
					maskmax = 32;
				max = strtonum(s, 0, maskmax, &errstr);
				if (errstr != NULL)
					errx(1, "max is %s: %s", errstr, s);
				break;
			default:
				errx(1, "could not parse \"%s\", confused", s);
			}
			state++;
		}

		if (trie_add(th, &prefix, plen, min, max) != 0)
			errx(1, "trie_add(%s, %u, %u, %u) failed",
			    print_prefix(&prefix), plen, min, max);

		free(line);
	}
}

static void
test_file(FILE *in, struct trie_head *th)
{
	char *line;
	struct bgpd_addr prefix;
	u_int8_t plen;
	int orlonger;

	while ((line = fparseln(in, NULL, NULL, NULL, FPARSELN_UNESCALL))) {
		if (!host_l(line, &prefix, &plen, &orlonger))
			errx(1, "could not parse prefix \"%s\"", line);
		printf("%s ", line);
		if (trie_match(th, &prefix, plen, orlonger))
			printf("MATCH %i\n", orlonger);
		else
			printf("miss %i\n", orlonger);
		free(line);
	}
}

static void
usage(void)
{
        extern char *__progname;
	fprintf(stderr, "usage: %s prefixfile testfile\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct trie_head th = { 0 };
	FILE *in, *tin;
	int ch;

	if (argc != 3)
		usage();

	in = fopen(argv[1], "r");
	if (in == NULL)
		err(1, "fopen(%s)", argv[0]);
	tin = fopen(argv[2], "r");
	if (tin == NULL)
		err(1, "fopen(%s)", argv[1]);

	parse_file(in, &th);
	/* trie_dump(&th); */
	if (trie_equal(&th, &th) == 0)
		errx(1, "trie_equal failure");
	test_file(tin, &th);

	trie_free(&th);
}

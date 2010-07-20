/*	$OpenBSD: parser.c,v 1.9 2010/07/20 05:26:06 claudio Exp $ */

/*
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ospf6d.h"

#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	ADDRESS,
	FLAG,
	PREFIX,
	IFNAME
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_fib[];
static const struct token t_show[];
static const struct token t_show_iface[];
static const struct token t_show_db[];
static const struct token t_show_area[];
static const struct token t_show_nbr[];
static const struct token t_show_rib[];
static const struct token t_show_fib[];
static const struct token t_log[];

static const struct token t_main[] = {
	{KEYWORD,	"reload",	RELOAD,		NULL},
	{KEYWORD,	"fib",		FIB,		t_fib},
	{KEYWORD,	"show",		SHOW,		t_show},
	{KEYWORD,	"log",		NONE,		t_log},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_fib[] = {
	{ KEYWORD,	"couple",	FIB_COUPLE,	NULL},
	{ KEYWORD,	"decouple",	FIB_DECOUPLE,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"interfaces",	SHOW_IFACE,	t_show_iface},
	{KEYWORD,	"database",	SHOW_DB,	t_show_db},
	{KEYWORD,	"neighbor",	SHOW_NBR,	t_show_nbr},
	{KEYWORD,	"rib",		SHOW_RIB,	t_show_rib},
	{KEYWORD,	"fib",		SHOW_FIB,	t_show_fib},
	{KEYWORD,	"summary",	SHOW_SUM,	NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_iface[] = {
	{NOTOKEN,	"",		NONE,			NULL},
	{KEYWORD,	"detail",	SHOW_IFACE_DTAIL,	NULL},
	{IFNAME,	"",		SHOW_IFACE_DTAIL,	NULL},
	{ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_db[] = {
	{NOTOKEN,	"",			NONE,		NULL},
	{KEYWORD,	"area",			SHOW_DBBYAREA,	t_show_area},
	{KEYWORD,	"asbr",			SHOW_DBASBR,	NULL},
	{KEYWORD,	"external",		SHOW_DBEXT,	NULL},
	{KEYWORD,	"link",			SHOW_DBLINK,	NULL},
	{KEYWORD,	"network",		SHOW_DBNET,	NULL},
	{KEYWORD,	"router",		SHOW_DBRTR,	NULL},
	{KEYWORD,	"intra",		SHOW_DBINTRA,	NULL},
	{KEYWORD,	"self-originated",	SHOW_DBSELF,	NULL},
	{KEYWORD,	"summary",		SHOW_DBSUM,	NULL},
	{ENDTOKEN,	"",			NONE,		NULL}
};

static const struct token t_show_area[] = {
	{ADDRESS,	"",		NONE,		NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_nbr[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"detail",	SHOW_NBR_DTAIL,	NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_rib[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"detail",	SHOW_RIB_DTAIL,	NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_fib[] = {
	{NOTOKEN,	"",		NONE,			NULL},
	{FLAG,		"connected",	F_CONNECTED,		t_show_fib},
	{FLAG,		"static",	F_STATIC,		t_show_fib},
	{FLAG,		"ospf",		F_OSPFD_INSERTED,	t_show_fib},
	{ADDRESS,	"",		NONE,			NULL},
	{ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_log[] = {
	{KEYWORD,	"verbose",	LOG_VERBOSE,		NULL},
	{KEYWORD,	"brief",	LOG_BRIEF,		NULL},
	{ENDTOKEN,	"",		NONE,			NULL}
};

static struct parse_result	res;

struct parse_result *
parse(int argc, char *argv[])
{
	const struct token	*table = t_main;
	const struct token	*match;

	bzero(&res, sizeof(res));

	while (argc >= 0) {
		if ((match = match_token(argv[0], table)) == NULL) {
			fprintf(stderr, "valid commands/args:\n");
			show_valid_args(table);
			return (NULL);
		}

		argc--;
		argv++;

		if (match->type == NOTOKEN || match->next == NULL)
			break;

		table = match->next;
	}

	if (argc > 0) {
		fprintf(stderr, "superfluous argument: %s\n", argv[0]);
		return (NULL);
	}

	return (&res);
}

const struct token *
match_token(const char *word, const struct token *table)
{
	u_int			 i, match;
	const struct token	*t = NULL;

	match = 0;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			if (word == NULL || strlen(word) == 0) {
				match++;
				t = &table[i];
			}
			break;
		case KEYWORD:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;
		case FLAG:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				res.flags |= t->value;
			}
			break;
		case ADDRESS:
			if (parse_addr(word, &res.addr)) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;
		case PREFIX:
			if (parse_prefix(word, &res.addr, &res.prefixlen)) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;
		case IFNAME:
			if (!match && word != NULL && strlen(word) > 0) {
				if (strlcpy(res.ifname, word,
				    sizeof(res.ifname)) >=
				    sizeof(res.ifname))
					err(1, "interface name too long");
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;

		case ENDTOKEN:
			break;
		}
	}

	if (match != 1) {
		if (word == NULL)
			fprintf(stderr, "missing argument:\n");
		else if (match > 1)
			fprintf(stderr, "ambiguous argument: %s\n", word);
		else if (match < 1)
			fprintf(stderr, "unknown argument: %s\n", word);
		return (NULL);
	}

	return (t);
}

void
show_valid_args(const struct token *table)
{
	int	i;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			fprintf(stderr, "  <cr>\n");
			break;
		case KEYWORD:
		case FLAG:
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case ADDRESS:
			fprintf(stderr, "  <address>\n");
			break;
		case PREFIX:
			fprintf(stderr, "  <address>[/<len>]\n");
			break;
		case IFNAME:
			fprintf(stderr, "  <interface>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

/* XXX shared with parse.y should be merged */
int
parse_addr(const char *word, struct in6_addr *addr)
{
	struct addrinfo	hints, *r;

	if (word == NULL)
		return (0);

	bzero(addr, sizeof(struct in6_addr));
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(word, "0", &hints, &r) == 0) {
		*addr = ((struct sockaddr_in6 *)r->ai_addr)->sin6_addr;
		/* XXX address scope !!! */
		/* ((struct sockaddr_in6 *)r->ai_addr)->sin6_scope_id */
		freeaddrinfo(r);
		return (1);
	}
	return (0);
}

/* XXX shared with parse.y should be merged */
int
parse_prefix(const char *word, struct in6_addr *addr, u_int8_t *prefixlen)
{
	char		*p, *ps;
	const char	*errstr;
	int		 mask;

	if (word == NULL)
		return (0);

	if ((p = strrchr(word, '/')) != NULL) {
		mask = strtonum(p + 1, 0, 128, &errstr);
		if (errstr)
			errx(1, "invalid netmask: %s", errstr);

		if ((ps = malloc(strlen(word) - strlen(p) + 1)) == NULL)
			err(1, "parse_prefix: malloc");
		strlcpy(ps, word, strlen(word) - strlen(p) + 1);

		if (parse_addr(ps, addr) == 0) {
			free(ps);
			return (0);
		}

		inet6applymask(addr, addr, mask);
		*prefixlen = mask;
		return (1);
	}
	*prefixlen = 128;
	return (parse_addr(word, addr));
}

/* XXX prototype defined in ospfd.h and shared with the kroute.c version */
u_int8_t
mask2prefixlen(struct sockaddr_in6 *sa_in6)
{
	u_int8_t	l = 0, *ap, *ep;

	/*
	 * sin6_len is the size of the sockaddr so substract the offset of
	 * the possibly truncated sin6_addr struct.
	 */
	ap = (u_int8_t *)&sa_in6->sin6_addr;
	ep = (u_int8_t *)sa_in6 + sa_in6->sin6_len;
	for (; ap < ep; ap++) {
		/* this "beauty" is adopted from sbin/route/show.c ... */
		switch (*ap) {
		case 0xff:
			l += 8;
			break;
		case 0xfe:
			l += 7;
			return (l);
		case 0xfc:
			l += 6;
			return (l);
		case 0xf8:
			l += 5;
			return (l);
		case 0xf0:
			l += 4;
			return (l);
		case 0xe0:
			l += 3;
			return (l);
		case 0xc0:
			l += 2;
			return (l);
		case 0x80:
			l += 1;
			return (l);
		case 0x00:
			return (l);
		default:
			errx(1, "non continguous inet6 netmask");
		}
	}

	return (l);
}

/* XXX local copy from kroute.c, should go to shared file */
struct in6_addr *
prefixlen2mask(u_int8_t prefixlen)
{
	static struct in6_addr	mask;
	int			i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	return (&mask);
}

/* XXX local copy from kroute.c, should go to shared file */
void
inet6applymask(struct in6_addr *dest, const struct in6_addr *src, int prefixlen)
{
	struct in6_addr	mask;
	int		i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	for (i = 0; i < 16; i++)
		dest->s6_addr[i] = src->s6_addr[i] & mask.s6_addr[i];
}

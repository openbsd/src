/*	$OpenBSD: parser.c,v 1.3 2004/02/26 16:19:58 claudio Exp $ */

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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	ADDRESS,
	FLAG,
	ASNUM,
	ASTYPE
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_show[];
static const struct token t_show_fib[];
static const struct token t_show_rib[];
static const struct token t_show_neighbor[];
static const struct token t_show_neighbor_modifiers[];
static const struct token t_fib[];
static const struct token t_neighbor[];
static const struct token t_neighbor_modifiers[];
static const struct token t_show_as[];
static const struct token t_show_ip[];

static const struct token t_main[] = {
	{ KEYWORD,	"reload",	RELOAD,		NULL},
	{ KEYWORD,	"show",		SHOW,		t_show},
	{ KEYWORD,	"fib",		FIB,		t_fib},
	{ KEYWORD,	"neighbor",	NEIGHBOR,	t_neighbor},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ KEYWORD,	"fib",		SHOW_FIB,	t_show_fib},
	{ KEYWORD,	"interfaces",	SHOW_INTERFACE,	NULL},
	{ KEYWORD,	"neighbor",	SHOW_NEIGHBOR,	t_show_neighbor},
	{ KEYWORD,	"nexthop",	SHOW_NEXTHOP,	NULL},
	{ KEYWORD,	"rib",		SHOW_RIB,	t_show_rib},
	{ KEYWORD,	"ip",		NONE,		t_show_ip},
	{ KEYWORD,	"summary",	SHOW_SUMMARY,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_fib[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ FLAG,		"connected",	F_CONNECTED,		t_show_fib},
	{ FLAG,		"static",	F_STATIC,		t_show_fib},
	{ FLAG,		"bgp",		F_BGPD_INSERTED,	t_show_fib},
	{ FLAG,		"nexthop",	F_NEXTHOP,		t_show_fib},
	{ ADDRESS,	"",		NONE,			NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_rib[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ ASTYPE,	"as",		AS_ALL,		t_show_as},
	{ ASTYPE,	"source-as",	AS_SOURCE,	t_show_as},
	{ ASTYPE,	"transit-as",	AS_TRANSIT,	t_show_as},
	{ ASTYPE,	"empty-as",	AS_EMPTY,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_neighbor[] = {
	{ NOTOKEN,	"",		NONE,	NULL},
	{ ADDRESS,	"",		NONE,	t_show_neighbor_modifiers},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_neighbor_modifiers[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ KEYWORD,	"timers",	SHOW_NEIGHBOR_TIMERS,	NULL},
	{ KEYWORD,	"messages",	SHOW_NEIGHBOR,		NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_fib[] = {
	{ KEYWORD,	"couple",	FIB_COUPLE,	NULL},
	{ KEYWORD,	"decouple",	FIB_DECOUPLE,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor[] = {
	{ ADDRESS,	"",		NONE,		t_neighbor_modifiers},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor_modifiers[] = {
	{ KEYWORD,	"up",		NEIGHBOR_UP,	NULL},
	{ KEYWORD,	"down",		NEIGHBOR_DOWN,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_as[] = {
	{ ASNUM,	"",		NONE,		NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_ip[] = {
	{ KEYWORD,	"bgp",		SHOW_RIB,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static struct parse_result	res;

const struct token	*match_token(const char *, const struct token []);
void			 show_valid_args(const struct token []);
int			 parse_addr(const char *, struct bgpd_addr *);
int			 parse_asnum(const char *, u_int16_t *);

struct parse_result *
parse(int argc, char *argv[])
{
	int			 curarg = 1;
	const struct token	*table = t_main;
	const struct token	*match;
	char			*word;

	bzero(&res, sizeof(res));
	if (argc == 1)
		return (&res);

	for (;;) {
		if (argc > curarg)
			word = argv[curarg];
		else
			word = NULL;

		if ((match = match_token(word, table)) == NULL) {
			fprintf(stderr, "valid commands/args:\n");
			show_valid_args(table);
			return (NULL);
		}

		curarg++;

		if (match->type == NOTOKEN)
			break;

		if (match->next == NULL)
			break;

		table = match->next;
	}

	if (curarg < argc) {
		fprintf(stderr, "superflous argument: %s\n", argv[curarg]);
		return (NULL);
	}

	return (&res);
}

const struct token *
match_token(const char *word, const struct token table[])
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
		case ASTYPE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				res.as.type = t->value;
			}
			break;
		case ASNUM:
			if (parse_asnum(word, &res.as.as)) {
				match++;
				t = &table[i];
			}
			break;
		case ENDTOKEN:
			break;
		}
	}

	if (match != 1) {
		if (match > 1)
			fprintf(stderr, "ambiguous argument: %s\n", word);
		if (match < 1)
			fprintf(stderr, "unknown argument: %s\n", word);
		return (NULL);
	}

	return (t);
}

void
show_valid_args(const struct token table[])
{
	int	i;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			fprintf(stderr, "  (nothing)\n");
			break;
		case KEYWORD:
		case FLAG:
		case ASTYPE:
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case ADDRESS:
			fprintf(stderr, "  <address>\n");
			break;
		case ASNUM:
			fprintf(stderr, "  <asnum>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

int
parse_addr(const char *word, struct bgpd_addr *addr)
{
	struct in_addr	ina;

	if (word == NULL)
		return (0);

	bzero(addr, sizeof(struct bgpd_addr));
	bzero(&ina, sizeof(ina));

	if (inet_pton(AF_INET, word, &ina)) {
		addr->af = AF_INET;
		addr->v4 = ina;
		return (1);
	}

	return (0);
}

int
parse_asnum(const char *word, u_int16_t *asnum)
{
	u_long	 ulval;
	char	*ep;

	if (word == NULL)
		return (0);

	errno = 0;
	ulval = strtoul(word, &ep, 0);
	if (word[0] == '\0' || *ep != '\0')
		return (0);
	if (errno == ERANGE && ulval == ULONG_MAX)
		return (0);
	if (ulval > USHRT_MAX)
		return (0);
	*asnum = (u_int16_t)ulval;
	return (1);
}


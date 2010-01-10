/*	$OpenBSD: parser.c,v 1.59 2010/01/10 00:16:23 claudio Exp $ */

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

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "irrfilter.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	ADDRESS,
	PEERADDRESS,
	FLAG,
	ASNUM,
	ASTYPE,
	PREFIX,
	PEERDESC,
	RIBNAME,
	COMMUNITY,
	LOCALPREF,
	MED,
	NEXTHOP,
	PFTABLE,
	PREPNBR,
	PREPSELF,
	WEIGHT,
	FAMILY,
	GETOPT
};

enum getopts {
	GETOPT_NONE,
	GETOPT_IRRFILTER
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_show[];
static const struct token t_show_summary[];
static const struct token t_show_fib[];
static const struct token t_show_rib[];
static const struct token t_show_rib_neigh[];
static const struct token t_show_rib_rib[];
static const struct token t_show_neighbor[];
static const struct token t_show_neighbor_modifiers[];
static const struct token t_fib[];
static const struct token t_neighbor[];
static const struct token t_neighbor_modifiers[];
static const struct token t_show_as[];
static const struct token t_show_prefix[];
static const struct token t_show_ip[];
static const struct token t_show_community[];
static const struct token t_network[];
static const struct token t_network_show[];
static const struct token t_prefix[];
static const struct token t_set[];
static const struct token t_community[];
static const struct token t_localpref[];
static const struct token t_med[];
static const struct token t_nexthop[];
static const struct token t_pftable[];
static const struct token t_prepnbr[];
static const struct token t_prepself[];
static const struct token t_weight[];
static const struct token t_irrfilter[];
static const struct token t_irrfilter_opts[];
static const struct token t_log[];

static const struct token t_main[] = {
	{ KEYWORD,	"reload",	RELOAD,		NULL},
	{ KEYWORD,	"show",		SHOW,		t_show},
	{ KEYWORD,	"fib",		FIB,		t_fib},
	{ KEYWORD,	"neighbor",	NEIGHBOR,	t_neighbor},
	{ KEYWORD,	"network",	NONE,		t_network},
	{ KEYWORD,	"irrfilter",	IRRFILTER,	t_irrfilter},
	{ KEYWORD,	"log",		NONE,		t_log},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ KEYWORD,	"fib",		SHOW_FIB,	t_show_fib},
	{ KEYWORD,	"interfaces",	SHOW_INTERFACE,	NULL},
	{ KEYWORD,	"neighbor",	SHOW_NEIGHBOR,	t_show_neighbor},
	{ KEYWORD,	"network",	NETWORK_SHOW,	t_network_show},
	{ KEYWORD,	"nexthop",	SHOW_NEXTHOP,	NULL},
	{ KEYWORD,	"rib",		SHOW_RIB,	t_show_rib},
	{ KEYWORD,	"ip",		NONE,		t_show_ip},
	{ KEYWORD,	"summary",	SHOW_SUMMARY,	t_show_summary},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_summary[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ KEYWORD,	"terse",	SHOW_SUMMARY_TERSE,	NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_fib[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ FLAG,		"connected",	F_CONNECTED,		t_show_fib},
	{ FLAG,		"static",	F_STATIC,		t_show_fib},
	{ FLAG,		"bgp",		F_BGPD_INSERTED,	t_show_fib},
	{ FLAG,		"nexthop",	F_NEXTHOP,		t_show_fib},
	{ FAMILY,	"",		NONE,			t_show_fib},
	{ ADDRESS,	"",		NONE,			NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_rib[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ ASTYPE,	"as",		AS_ALL,		t_show_as},
	{ ASTYPE,	"source-as",	AS_SOURCE,	t_show_as},
	{ ASTYPE,	"transit-as",	AS_TRANSIT,	t_show_as},
	{ ASTYPE,	"peer-as",	AS_PEER,	t_show_as},
	{ ASTYPE,	"empty-as",	AS_EMPTY,	t_show_rib},
	{ KEYWORD,	"community",	NONE,		t_show_community},
	{ FLAG,		"detail",	F_CTL_DETAIL,	t_show_rib},
	{ FLAG,		"in",		F_CTL_ADJ_IN,	t_show_rib},
	{ FLAG,		"out",		F_CTL_ADJ_OUT,	t_show_rib},
	{ KEYWORD,	"neighbor",	NONE,		t_show_rib_neigh},
	{ KEYWORD,	"table",	NONE,		t_show_rib_rib},
	{ KEYWORD,	"summary",	SHOW_SUMMARY,	t_show_summary},
	{ KEYWORD,	"memory",	SHOW_RIB_MEM,	NULL},
	{ FAMILY,	"",		NONE,		t_show_rib},
	{ PREFIX,	"",		NONE,		t_show_prefix},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_rib_neigh[] = {
	{ PEERADDRESS,	"",		NONE,	t_show_rib},
	{ PEERDESC,	"",		NONE,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_rib_rib[] = {
	{ RIBNAME,	"",		NONE,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_neighbor[] = {
	{ NOTOKEN,	"",		NONE,	NULL},
	{ PEERADDRESS,	"",		NONE,	t_show_neighbor_modifiers},
	{ PEERDESC,	"",		NONE,	t_show_neighbor_modifiers},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_neighbor_modifiers[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ KEYWORD,	"timers",	SHOW_NEIGHBOR_TIMERS,	NULL},
	{ KEYWORD,	"messages",	SHOW_NEIGHBOR,		NULL},
	{ KEYWORD,	"terse",	SHOW_NEIGHBOR_TERSE,	NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_fib[] = {
	{ KEYWORD,	"couple",	FIB_COUPLE,	NULL},
	{ KEYWORD,	"decouple",	FIB_DECOUPLE,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor[] = {
	{ PEERADDRESS,	"",		NONE,		t_neighbor_modifiers},
	{ PEERDESC,	"",		NONE,		t_neighbor_modifiers},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor_modifiers[] = {
	{ KEYWORD,	"up",		NEIGHBOR_UP,		NULL},
	{ KEYWORD,	"down",		NEIGHBOR_DOWN,		NULL},
	{ KEYWORD,	"clear",	NEIGHBOR_CLEAR,		NULL},
	{ KEYWORD,	"refresh",	NEIGHBOR_RREFRESH,	NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_as[] = {
	{ ASNUM,	"",		NONE,		t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_prefix[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ FLAG,		"all",		F_LONGER,	NULL},
	{ FLAG,		"longer-prefixes", F_LONGER,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_ip[] = {
	{ KEYWORD,	"bgp",		SHOW_RIB,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_community[] = {
	{ COMMUNITY,	"",		NONE,		t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_network[] = {
	{ KEYWORD,	"add",		NETWORK_ADD,	t_prefix},
	{ KEYWORD,	"delete",	NETWORK_REMOVE,	t_prefix},
	{ KEYWORD,	"flush",	NETWORK_FLUSH,	NULL},
	{ KEYWORD,	"show",		NETWORK_SHOW,	t_network_show},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_prefix[] = {
	{ PREFIX,	"",		NONE,		t_set},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_network_show[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ FAMILY,	"",		NONE,			NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_set[] = {
	{ NOTOKEN,	"",			NONE,	NULL},
	{ KEYWORD,	"community",		NONE,	t_community},
	{ KEYWORD,	"localpref",		NONE,	t_localpref},
	{ KEYWORD,	"med",			NONE,	t_med},
	{ KEYWORD,	"metric",		NONE,	t_med},
	{ KEYWORD,	"nexthop",		NONE,	t_nexthop},
	{ KEYWORD,	"pftable",		NONE,	t_pftable},
	{ KEYWORD,	"prepend-neighbor",	NONE,	t_prepnbr},
	{ KEYWORD,	"prepend-self",		NONE,	t_prepself},
	{ KEYWORD,	"weight",		NONE,	t_weight},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_community[] = {
	{ COMMUNITY,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_localpref[] = {
	{ LOCALPREF,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_med[] = {
	{ MED,		"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_nexthop[] = {
	{ NEXTHOP,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_pftable[] = {
	{ PFTABLE,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_prepnbr[] = {
	{ PREPNBR,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_prepself[] = {
	{ PREPSELF,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_weight[] = {
	{ WEIGHT,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_irrfilter[] = {
	{ GETOPT,	"",	GETOPT_IRRFILTER,	t_irrfilter},
	{ ASNUM,	"",	NONE,			t_irrfilter_opts},
	{ ENDTOKEN,	"",	NONE,			NULL}
};

static const struct token t_irrfilter_opts[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ FLAG,		"importonly",	F_IMPORTONLY,		t_irrfilter_opts},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_log[] = {
	{ KEYWORD,	"verbose",	LOG_VERBOSE,	NULL},
	{ KEYWORD,	"brief",	LOG_BRIEF,	NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static struct parse_result	res;

const struct token	*match_token(int *argc, char **argv[],
			    const struct token []);
void			 show_valid_args(const struct token []);
int			 parse_addr(const char *, struct bgpd_addr *);
int			 parse_prefix(const char *, struct bgpd_addr *,
			     u_int8_t *);
int			 parse_asnum(const char *, u_int32_t *);
int			 parse_number(const char *, struct parse_result *,
			     enum token_type);
int			 getcommunity(const char *);
int			 parse_community(const char *, struct parse_result *);
int			 parse_nexthop(const char *, struct parse_result *);
int			 bgpctl_getopt(int *, char **[], int);

struct parse_result *
parse(int argc, char *argv[])
{
	const struct token	*table = t_main;
	const struct token	*match;

	bzero(&res, sizeof(res));
	res.community.as = COMMUNITY_UNSET;
	res.community.type = COMMUNITY_UNSET;
	res.flags = (F_IPV4 | F_IPV6);
	TAILQ_INIT(&res.set);
	if ((res.irr_outdir = getcwd(NULL, 0)) == NULL) {
		fprintf(stderr, "getcwd failed: %s", strerror(errno));
		return (NULL);
	}

	while (argc >= 0) {
		if ((match = match_token(&argc, &argv, table)) == NULL) {
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
match_token(int *argc, char **argv[], const struct token table[])
{
	u_int			 i, match;
	const struct token	*t = NULL;
	struct filter_set	*fs;
	const char		*word = *argv[0];

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
		case FAMILY:
			if (word == NULL)
				break;
			if (!strcmp(word, "inet") || !strcmp(word, "IPv4")) {
				match++;
				t = &table[i];
				res.aid = AID_INET;
			}
			if (!strcmp(word, "inet6") || !strcmp(word, "IPv6")) {
				match++;
				t = &table[i];
				res.aid = AID_INET6;
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
		case PEERADDRESS:
			if (parse_addr(word, &res.peeraddr)) {
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
		case PEERDESC:
			if (!match && word != NULL && strlen(word) > 0) {
				if (strlcpy(res.peerdesc, word,
				    sizeof(res.peerdesc)) >=
				    sizeof(res.peerdesc))
					errx(1, "neighbor description too "
					    "long");
				match++;
				t = &table[i];
			}
			break;
		case RIBNAME:
			if (!match && word != NULL && strlen(word) > 0) {
				if (strlcpy(res.rib, word, sizeof(res.rib)) >=
				    sizeof(res.rib))
					errx(1, "rib name too long");
				match++;
				t = &table[i];
			}
			break;
		case COMMUNITY:
			if (word != NULL && strlen(word) > 0 &&
			    parse_community(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case LOCALPREF:
		case MED:
		case PREPNBR:
		case PREPSELF:
		case WEIGHT:
			if (word != NULL && strlen(word) > 0 &&
			    parse_number(word, &res, table[i].type)) {
				match++;
				t = &table[i];
			}
			break;
		case NEXTHOP:
			if (word != NULL && strlen(word) > 0 &&
			    parse_nexthop(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case PFTABLE:
			if (word != NULL && strlen(word) > 0) {
				if ((fs = calloc(1,
				    sizeof(struct filter_set))) == NULL)
					err(1, NULL);
				if (strlcpy(fs->action.pftable, word,
				    sizeof(fs->action.pftable)) >=
				    sizeof(fs->action.pftable))
					errx(1, "pftable name too long");
				TAILQ_INSERT_TAIL(&res.set, fs, entry);
				match++;
				t = &table[i];
			}
			break;
		case GETOPT:
			if (bgpctl_getopt(argc, argv, table[i].value)) {
				match++;
				t = &table[i];
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
show_valid_args(const struct token table[])
{
	int	i;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			fprintf(stderr, "  <cr>\n");
			break;
		case KEYWORD:
		case FLAG:
		case ASTYPE:
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case ADDRESS:
		case PEERADDRESS:
			fprintf(stderr, "  <address>\n");
			break;
		case PREFIX:
			fprintf(stderr, "  <address>[/<len>]\n");
			break;
		case ASNUM:
			fprintf(stderr, "  <asnum>\n");
			break;
		case PEERDESC:
			fprintf(stderr, "  <neighbor description>\n");
			break;
		case RIBNAME:
			fprintf(stderr, "  <rib name>\n");
			break;
		case COMMUNITY:
			fprintf(stderr, "  <community>\n");
			break;
		case LOCALPREF:
		case MED:
		case PREPNBR:
		case PREPSELF:
		case WEIGHT:
			fprintf(stderr, "  <number>\n");
			break;
		case NEXTHOP:
			fprintf(stderr, "  <address>\n");
			break;
		case PFTABLE:
			fprintf(stderr, "  <pftable>\n");
			break;
		case FAMILY:
			fprintf(stderr, "  [ inet | inet6 | IPv4 | IPv6 ]\n");
			break;
		case GETOPT:
			fprintf(stderr, "  <options>\n");
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
	struct addrinfo	hints, *r;

	if (word == NULL)
		return (0);

	bzero(addr, sizeof(struct bgpd_addr));
	bzero(&ina, sizeof(ina));

	if (inet_net_pton(AF_INET, word, &ina, sizeof(ina)) != -1) {
		addr->aid = AID_INET;
		addr->v4 = ina;
		return (1);
	}

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(word, "0", &hints, &r) == 0) {
		sa2addr(r->ai_addr, addr);
		freeaddrinfo(r);
		return (1);
	}

	return (0);
}

int
parse_prefix(const char *word, struct bgpd_addr *addr, u_int8_t *prefixlen)
{
	char		*p, *ps;
	const char	*errstr;
	int		 mask = -1;

	if (word == NULL)
		return (0);

	bzero(addr, sizeof(struct bgpd_addr));

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

		free(ps);
	} else
		if (parse_addr(word, addr) == 0)
			return (0);

	switch (addr->aid) {
	case AID_INET:
		if (mask == -1)
			mask = 32;
		if (mask > 32)
			errx(1, "invalid netmask: too large");
		addr->v4.s_addr = addr->v4.s_addr & htonl(prefixlen2mask(mask));
		break;
	case AID_INET6:
		if (mask == -1)
			mask = 128;
		inet6applymask(&addr->v6, &addr->v6, mask);
		break;
	default:
		return (0);
	}

	*prefixlen = mask;
	return (1);
}

int
parse_asnum(const char *word, u_int32_t *asnum)
{
	const char	*errstr;
	char		*dot;
	u_int32_t	 uval, uvalh = 0;

	if (word == NULL)
		return (0);

	if (strlen(word) < 1 || word[0] < '0' || word[0] > '9')
		return (0);

	if ((dot = strchr(word,'.')) != NULL) {
		*dot++ = '\0';
		uvalh = strtonum(word, 0, USHRT_MAX, &errstr);
		if (errstr)
			errx(1, "AS number is %s: %s", errstr, word);
		uval = strtonum(dot, 0, USHRT_MAX, &errstr);
		if (errstr)
			errx(1, "AS number is %s: %s", errstr, word);
	} else {
		uval = strtonum(word, 0, UINT_MAX, &errstr);
		if (errstr)
			errx(1, "AS number is %s: %s", errstr, word);
	}

	*asnum = uval | (uvalh << 16);
	return (1);
}

int
parse_number(const char *word, struct parse_result *r, enum token_type type)
{
	struct filter_set	*fs;
	const char		*errstr;
	u_int			 uval;

	if (word == NULL)
		return (0);

	uval = strtonum(word, 0, UINT_MAX, &errstr);
	if (errstr)
		errx(1, "number is %s: %s", errstr, word);

	/* number was parseable */
	if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
		err(1, NULL);
	switch (type) {
	case LOCALPREF:
		fs->type = ACTION_SET_LOCALPREF;
		fs->action.metric = uval;
		break;
	case MED:
		fs->type = ACTION_SET_MED;
		fs->action.metric = uval;
		break;
	case PREPNBR:
		if (uval > 128) {
			free(fs);
			return (0);
		}
		fs->type = ACTION_SET_PREPEND_PEER;
		fs->action.prepend = uval;
		break;
	case PREPSELF:
		if (uval > 128) {
			free(fs);
			return (0);
		}
		fs->type = ACTION_SET_PREPEND_SELF;
		fs->action.prepend = uval;
		break;
	case WEIGHT:
		fs->type = ACTION_SET_WEIGHT;
		fs->action.metric = uval;
		break;
	default:
		errx(1, "king bula sez bad things happen");
	}

	TAILQ_INSERT_TAIL(&r->set, fs, entry);
	return (1);
}

int
getcommunity(const char *s)
{
	const char	*errstr;
	u_int16_t	 uval;

	if (strcmp(s, "*") == 0)
		return (COMMUNITY_ANY);

	uval = strtonum(s, 0, USHRT_MAX, &errstr);
	if (errstr)
		errx(1, "Community is %s: %s", errstr, s);

	return (uval);
}

int
parse_community(const char *word, struct parse_result *r)
{
	struct filter_set	*fs;
	char			*p;
	int			 as, type;

	/* Well-known communities */
	if (strcasecmp(word, "NO_EXPORT") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_NO_EXPORT;
		goto done;
	} else if (strcasecmp(word, "NO_ADVERTISE") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_NO_ADVERTISE;
		goto done;
	} else if (strcasecmp(word, "NO_EXPORT_SUBCONFED") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_NO_EXPSUBCONFED;
		goto done;
	} else if (strcasecmp(word, "NO_PEER") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_NO_PEER;
		goto done;
	}

	if ((p = strchr(word, ':')) == NULL) {
		fprintf(stderr, "Bad community syntax\n");
		return (0);
	}
	*p++ = 0;

	as = getcommunity(word);
	type = getcommunity(p);

done:
	if (as == 0) {
		fprintf(stderr, "Invalid community\n");
		return (0);
	}
	if (as == COMMUNITY_WELLKNOWN)
		switch (type) {
		case COMMUNITY_NO_EXPORT:
		case COMMUNITY_NO_ADVERTISE:
		case COMMUNITY_NO_EXPSUBCONFED:
			/* valid */
			break;
		default:
			/* unknown */
			fprintf(stderr, "Unknown well-known community\n");
			return (0);
		}

	if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
		err(1, NULL);
	fs->type = ACTION_SET_COMMUNITY;
	fs->action.community.as = as;
	fs->action.community.type = type;

	r->community.as = as;
	r->community.type = type;

	TAILQ_INSERT_TAIL(&r->set, fs, entry);
	return (1);
}

int
parse_nexthop(const char *word, struct parse_result *r)
{
	struct filter_set	*fs;

	if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
		err(1, NULL);

	if (strcmp(word, "blackhole") == 0)
		fs->type = ACTION_SET_NEXTHOP_BLACKHOLE;
	else if (strcmp(word, "reject") == 0)
		fs->type = ACTION_SET_NEXTHOP_REJECT;
	else if (strcmp(word, "no-modify") == 0)
		fs->type = ACTION_SET_NEXTHOP_NOMODIFY;
	else if (parse_addr(word, &fs->action.nexthop)) {
		fs->type = ACTION_SET_NEXTHOP;
	} else {
		free(fs);
		return (0);
	}

	TAILQ_INSERT_TAIL(&r->set, fs, entry);
	return (1);
}

int
bgpctl_getopt(int *argc, char **argv[], int type)
{
	int	  ch;

	optind = optreset = 1;
	while ((ch = getopt((*argc) + 1, (*argv) - 1, "46o:")) != -1) {
		switch (ch) {
		case '4':
			res.flags = (res.flags | F_IPV4) & ~F_IPV6;
			break;
		case '6':
			res.flags = (res.flags | F_IPV6) & ~F_IPV4;
			break;
		case 'o':
			res.irr_outdir = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (optind > 1) {
		(*argc) -= (optind - 1);
		(*argv) += (optind - 1);

		/* need to move one backwards as calling code moves forward */
		(*argc)++;
		(*argv)--;
		return (1);
	} else
		return (0);
}

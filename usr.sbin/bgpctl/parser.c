/*	$OpenBSD: parser.c,v 1.88 2018/12/19 15:27:29 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2016 Job Snijders <job@instituut.net>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
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
	SHUTDOWN_COMMUNICATION,
	COMMUNITY,
	EXTCOMMUNITY,
	EXTCOM_SUBTYPE,
	LARGE_COMMUNITY,
	LOCALPREF,
	MED,
	NEXTHOP,
	PFTABLE,
	PREPNBR,
	PREPSELF,
	WEIGHT,
	FAMILY,
	GETOPT,
	RTABLE,
	FILENAME,
	BULK
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
static const struct token t_show_ovs[];
static const struct token t_show_mrt[];
static const struct token t_show_mrt_file[];
static const struct token t_show_rib_neigh[];
static const struct token t_show_mrt_neigh[];
static const struct token t_show_rib_rib[];
static const struct token t_show_neighbor[];
static const struct token t_show_neighbor_modifiers[];
static const struct token t_fib[];
static const struct token t_neighbor[];
static const struct token t_neighbor_modifiers[];
static const struct token t_show_rib_as[];
static const struct token t_show_mrt_as[];
static const struct token t_show_prefix[];
static const struct token t_show_ip[];
static const struct token t_show_community[];
static const struct token t_show_extcommunity[];
static const struct token t_show_ext_subtype[];
static const struct token t_show_largecommunity[];
static const struct token t_network[];
static const struct token t_network_show[];
static const struct token t_prefix[];
static const struct token t_set[];
static const struct token t_community[];
static const struct token t_extcommunity[];
static const struct token t_ext_subtype[];
static const struct token t_largecommunity[];
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
static const struct token t_fib_table[];
static const struct token t_show_fib_table[];

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
	{ KEYWORD,	"tables",	SHOW_FIB_TABLES, NULL},
	{ KEYWORD,	"ip",		NONE,		t_show_ip},
	{ KEYWORD,	"summary",	SHOW_SUMMARY,	t_show_summary},
	{ KEYWORD,	"mrt",		SHOW_MRT,	t_show_mrt},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_summary[] = {
	{ NOTOKEN,	"",		NONE,			NULL},
	{ KEYWORD,	"terse",	SHOW_SUMMARY_TERSE,	NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_fib[] = {
	{ NOTOKEN,	"",		NONE,		 NULL},
	{ FLAG,		"connected",	F_CONNECTED,	 t_show_fib},
	{ FLAG,		"static",	F_STATIC,	 t_show_fib},
	{ FLAG,		"bgp",		F_BGPD_INSERTED, t_show_fib},
	{ FLAG,		"nexthop",	F_NEXTHOP,	 t_show_fib},
	{ KEYWORD,	"table",	NONE,		 t_show_fib_table},
	{ FAMILY,	"",		NONE,		 t_show_fib},
	{ ADDRESS,	"",		NONE,		 NULL},
	{ ENDTOKEN,	"",		NONE,		 NULL}
};

static const struct token t_show_rib[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ ASTYPE,	"as",		AS_ALL,		t_show_rib_as},
	{ ASTYPE,	"source-as",	AS_SOURCE,	t_show_rib_as},
	{ ASTYPE,	"transit-as",	AS_TRANSIT,	t_show_rib_as},
	{ ASTYPE,	"peer-as",	AS_PEER,	t_show_rib_as},
	{ ASTYPE,	"empty-as",	AS_EMPTY,	t_show_rib},
	{ KEYWORD,	"community",	NONE,		t_show_community},
	{ KEYWORD,	"ext-community", NONE,		t_show_extcommunity},
	{ KEYWORD,	"large-community", NONE,	t_show_largecommunity},
	{ FLAG,		"best",		F_CTL_ACTIVE,	t_show_rib},
	{ FLAG,		"selected",	F_CTL_ACTIVE,	t_show_rib},
	{ FLAG,		"detail",	F_CTL_DETAIL,	t_show_rib},
	{ FLAG,		"error",	F_CTL_INVALID,	t_show_rib},
	{ FLAG,		"ssv"	,	F_CTL_SSV,	t_show_rib},
	{ FLAG,		"in",		F_CTL_ADJ_IN,	t_show_rib},
	{ FLAG,		"out",		F_CTL_ADJ_OUT,	t_show_rib},
	{ KEYWORD,	"neighbor",	NONE,		t_show_rib_neigh},
	{ KEYWORD,	"table",	NONE,		t_show_rib_rib},
	{ KEYWORD,	"summary",	SHOW_SUMMARY,	t_show_summary},
	{ KEYWORD,	"memory",	SHOW_RIB_MEM,	NULL},
	{ KEYWORD,	"ovs",		NONE,		t_show_ovs},
	{ FAMILY,	"",		NONE,		t_show_rib},
	{ PREFIX,	"",		NONE,		t_show_prefix},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_ovs[] = {
	{ FLAG,		"valid"	,	F_CTL_OVS_VALID,	t_show_rib},
	{ FLAG,		"invalid",	F_CTL_OVS_INVALID,	t_show_rib},
	{ FLAG,		"not-found",	F_CTL_OVS_NOTFOUND,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_mrt[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ ASTYPE,	"as",		AS_ALL,		t_show_mrt_as},
	{ ASTYPE,	"source-as",	AS_SOURCE,	t_show_mrt_as},
	{ ASTYPE,	"transit-as",	AS_TRANSIT,	t_show_mrt_as},
	{ ASTYPE,	"peer-as",	AS_PEER,	t_show_mrt_as},
	{ ASTYPE,	"empty-as",	AS_EMPTY,	t_show_mrt},
	{ FLAG,		"detail",	F_CTL_DETAIL,	t_show_mrt},
	{ FLAG,		"ssv"	,	F_CTL_SSV,	t_show_mrt},
	{ KEYWORD,	"neighbor",	NONE,		t_show_mrt_neigh},
	{ KEYWORD,	"file",		NONE,		t_show_mrt_file},
	{ FAMILY,	"",		NONE,		t_show_mrt},
	{ PREFIX,	"",		NONE,		t_show_prefix},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_mrt_file[] = {
	{ FILENAME,	"",		NONE,		t_show_mrt},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_rib_neigh[] = {
	{ PEERADDRESS,	"",		NONE,	t_show_rib},
	{ PEERDESC,	"",		NONE,	t_show_rib},
	{ ENDTOKEN,	"",		NONE,	NULL}
};

static const struct token t_show_mrt_neigh[] = {
	{ PEERADDRESS,	"",		NONE,	t_show_mrt},
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
	{ KEYWORD,	"table",	NONE,		t_fib_table},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor[] = {
	{ PEERADDRESS,	"",		NONE,		t_neighbor_modifiers},
	{ PEERDESC,	"",		NONE,		t_neighbor_modifiers},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_nei_mod_shutc[] = {
	{ NOTOKEN,	"",		NONE,		NULL},
	{ SHUTDOWN_COMMUNICATION, "",	NONE,		NULL},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_neighbor_modifiers[] = {
	{ KEYWORD,	"up",		NEIGHBOR_UP,		NULL},
	{ KEYWORD,	"down",		NEIGHBOR_DOWN,		t_nei_mod_shutc},
	{ KEYWORD,	"clear",	NEIGHBOR_CLEAR,		t_nei_mod_shutc},
	{ KEYWORD,	"refresh",	NEIGHBOR_RREFRESH,	NULL},
	{ KEYWORD,	"destroy",	NEIGHBOR_DESTROY,	NULL},
	{ ENDTOKEN,	"",		NONE,			NULL}
};

static const struct token t_show_rib_as[] = {
	{ ASNUM,	"",		NONE,		t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show_mrt_as[] = {
	{ ASNUM,	"",		NONE,		t_show_mrt},
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

static const struct token t_show_extcommunity[] = {
	{ EXTCOM_SUBTYPE,	"bdc",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"defgw",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"esi-lab",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"esi-rt",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"l2vid",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"mac-mob",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"odi",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ort",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ori",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ovs",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"rt",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"soo",		NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"srcas",	NONE,	t_show_ext_subtype},
	{ EXTCOM_SUBTYPE,	"vrfri",	NONE,	t_show_ext_subtype},
	{ ENDTOKEN,	"",	NONE,	NULL}
};

static const struct token t_show_ext_subtype[] = {
	{ EXTCOMMUNITY,	"",	NONE,	t_show_rib},
	{ ENDTOKEN,	"",	NONE,	NULL}
};

static const struct token t_show_largecommunity[] = {
	{ LARGE_COMMUNITY,	"",	NONE,		t_show_rib},
	{ ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_network[] = {
	{ KEYWORD,	"add",		NETWORK_ADD,	t_prefix},
	{ KEYWORD,	"delete",	NETWORK_REMOVE,	t_prefix},
	{ KEYWORD,	"flush",	NETWORK_FLUSH,	NULL},
	{ KEYWORD,	"show",		NETWORK_SHOW,	t_network_show},
	{ KEYWORD,	"mrt",		NETWORK_MRT,	t_show_mrt},
	{ KEYWORD,	"bulk",		NETWORK_BULK_ADD,	t_set},
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
	{ KEYWORD,	"ext-community",	NONE,	t_extcommunity},
	{ KEYWORD,	"large-community",	NONE,	t_largecommunity},
	{ KEYWORD,	"localpref",		NONE,	t_localpref},
	{ KEYWORD,	"med",			NONE,	t_med},
	{ KEYWORD,	"metric",		NONE,	t_med},
	{ KEYWORD,	"nexthop",		NONE,	t_nexthop},
	{ KEYWORD,	"pftable",		NONE,	t_pftable},
	{ KEYWORD,	"prepend-neighbor",	NONE,	t_prepnbr},
	{ KEYWORD,	"prepend-self",		NONE,	t_prepself},
	{ KEYWORD,	"weight",		NONE,	t_weight},
	{ KEYWORD,	"add",			NETWORK_BULK_ADD,	NULL},
	{ KEYWORD,	"delete",		NETWORK_BULK_REMOVE,	NULL},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_community[] = {
	{ COMMUNITY,	"",			NONE,	t_set},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_extcommunity[] = {
	{ EXTCOM_SUBTYPE,	"bdc",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"defgw",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"esi-lab",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"esi-rt",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"l2vid",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"mac-mob",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"odi",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ort",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ori",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"ovs",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"rt",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"soo",		NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"srcas",	NONE,	t_ext_subtype},
	{ EXTCOM_SUBTYPE,	"vrfri",	NONE,	t_ext_subtype},
	{ ENDTOKEN,	"",	NONE,	NULL}
};

static const struct token t_ext_subtype[] = {
	{ EXTCOMMUNITY,	"",	NONE,	t_set},
	{ ENDTOKEN,	"",	NONE,	NULL}
};

static const struct token t_largecommunity[] = {
	{ LARGE_COMMUNITY,	"",		NONE,	t_set},
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

static const struct token t_fib_table[] = {
	{ RTABLE,	"",			NONE,	t_fib},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static const struct token t_show_fib_table[] = {
	{ RTABLE,	"",			NONE,	t_show_fib},
	{ ENDTOKEN,	"",			NONE,	NULL}
};

static struct parse_result	res;

const struct token	*match_token(int *argc, char **argv[],
			    const struct token []);
void			 show_valid_args(const struct token []);
int			 parse_addr(const char *, struct bgpd_addr *);
int			 parse_asnum(const char *, size_t, u_int32_t *);
int			 parse_number(const char *, struct parse_result *,
			     enum token_type);
int			 parse_community(const char *, struct parse_result *);
int			 parsesubtype(const char *, u_int8_t *, u_int8_t *);
int			 parseextvalue(const char *, u_int32_t *);
u_int			 parseextcommunity(const char *, struct parse_result *);
int			 parse_largecommunity(const char *,
			     struct parse_result *);
int			 parse_nexthop(const char *, struct parse_result *);
int			 bgpctl_getopt(int *, char **[], int);

struct parse_result *
parse(int argc, char *argv[])
{
	const struct token	*table = t_main;
	const struct token	*match;

	bzero(&res, sizeof(res));
	res.rtableid = getrtable();
	TAILQ_INIT(&res.set);
	if ((res.irr_outdir = getcwd(NULL, 0)) == NULL) {
		fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
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
	size_t			wordlen = 0;

	match = 0;
	if (word != NULL)
		wordlen = strlen(word);
	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			if (word == NULL || wordlen == 0) {
				match++;
				t = &table[i];
			}
			break;
		case KEYWORD:
			if (word != NULL && strncmp(word, table[i].keyword,
			    wordlen) == 0) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;
		case FLAG:
			if (word != NULL && strncmp(word, table[i].keyword,
			    wordlen) == 0) {
				match++;
				t = &table[i];
				res.flags |= t->value;
			}
			break;
		case FAMILY:
			if (word == NULL)
				break;
			if (!strcmp(word, "inet") ||
			    !strcasecmp(word, "IPv4")) {
				match++;
				t = &table[i];
				res.aid = AID_INET;
			}
			if (!strcmp(word, "inet6") ||
			    !strcasecmp(word, "IPv6")) {
				match++;
				t = &table[i];
				res.aid = AID_INET6;
			}
			if (!strcasecmp(word, "VPNv4")) {
				match++;
				t = &table[i];
				res.aid = AID_VPN_IPv4;
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
			if (parse_prefix(word, wordlen, &res.addr, &res.prefixlen)) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
			}
			break;
		case ASTYPE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    wordlen) == 0) {
				match++;
				t = &table[i];
				res.as.type = t->value;
			}
			break;
		case ASNUM:
			if (parse_asnum(word, wordlen, &res.as.as_min)) {
				res.as.as_max = res.as.as_min;
				match++;
				t = &table[i];
			}
			break;
		case PEERDESC:
			if (!match && word != NULL && wordlen > 0) {
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
			if (!match && word != NULL && wordlen > 0) {
				if (strlcpy(res.rib, word, sizeof(res.rib)) >=
				    sizeof(res.rib))
					errx(1, "rib name too long");
				match++;
				t = &table[i];
			}
			break;
		case SHUTDOWN_COMMUNICATION:
			if (!match && word != NULL && wordlen > 0) {
				if (strlcpy(res.shutcomm, word,
				    sizeof(res.shutcomm)) >=
				    sizeof(res.shutcomm))
					errx(1, "shutdown reason too long");
				match++;
				t = &table[i];
			}
			break;
		case COMMUNITY:
			if (word != NULL && wordlen > 0 &&
			    parse_community(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case EXTCOM_SUBTYPE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    wordlen) == 0) {
				if (parsesubtype(word, &res.community.c.e.type,
				    &res.community.c.e.subtype) == 0)
					errx(1, "Bad ext-community unknown "
					    "type");
				match++;
				t = &table[i];
			}
			break;
		case EXTCOMMUNITY:
			if (word != NULL && wordlen > 0 &&
			    parseextcommunity(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case LARGE_COMMUNITY:
			if (word != NULL && wordlen > 0 &&
			    parse_largecommunity(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case LOCALPREF:
		case MED:
		case PREPNBR:
		case PREPSELF:
		case WEIGHT:
		case RTABLE:
			if (word != NULL && wordlen > 0 &&
			    parse_number(word, &res, table[i].type)) {
				match++;
				t = &table[i];
			}
			break;
		case NEXTHOP:
			if (word != NULL && wordlen > 0 &&
			    parse_nexthop(word, &res)) {
				match++;
				t = &table[i];
			}
			break;
		case PFTABLE:
			if (word != NULL && wordlen > 0) {
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
		case FILENAME:
			if (word != NULL && wordlen > 0) {
				if ((res.mrtfd = open(word, O_RDONLY)) == -1) {
					/*
					 * ignore error if path has no / and
					 * does not exist. In hope to print
					 * usage.
					 */
					if (errno == ENOENT &&
					    !strchr(word, '/'))
						break;
					err(1, "mrt open(%s)", word);
				}
				match++;
				t = &table[i];
			}
			break;
		case BULK:
			match++;
			t = &table[i];
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
		case EXTCOM_SUBTYPE:
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
		case SHUTDOWN_COMMUNICATION:
			fprintf(stderr, "  <shutdown reason>\n");
			break;
		case COMMUNITY:
			fprintf(stderr, "  <community>\n");
			break;
		case EXTCOMMUNITY:
			fprintf(stderr, "  <extended-community>\n");
			break;
		case LARGE_COMMUNITY:
			fprintf(stderr, "  <large-community>\n");
			break;
		case LOCALPREF:
		case MED:
		case PREPNBR:
		case PREPSELF:
		case WEIGHT:
			fprintf(stderr, "  <number>\n");
			break;
		case RTABLE:
			fprintf(stderr, "  <rtableid>\n");
			break;
		case NEXTHOP:
			fprintf(stderr, "  <address>\n");
			break;
		case PFTABLE:
			fprintf(stderr, "  <pftable>\n");
			break;
		case FAMILY:
			fprintf(stderr, "  [ inet | inet6 | IPv4 | IPv6 | VPNv4 ]\n");
			break;
		case GETOPT:
			fprintf(stderr, "  <options>\n");
			break;
		case FILENAME:
			fprintf(stderr, "  <filename>\n");
			break;
		case BULK:
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
parse_prefix(const char *word, size_t wordlen, struct bgpd_addr *addr, u_int8_t *prefixlen)
{
	char		*p, *ps;
	const char	*errstr;
	int		 mask = -1;

	if (word == NULL)
		return (0);

	bzero(addr, sizeof(struct bgpd_addr));

	if ((p = strrchr(word, '/')) != NULL) {
		size_t plen = strlen(p);
		mask = strtonum(p + 1, 0, 128, &errstr);
		if (errstr)
			errx(1, "netmask %s", errstr);

		if ((ps = malloc(wordlen - plen + 1)) == NULL)
			err(1, "parse_prefix: malloc");
		strlcpy(ps, word, wordlen - plen + 1);

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
parse_asnum(const char *word, size_t wordlen, u_int32_t *asnum)
{
	const char	*errstr;
	char		*dot;
	u_int32_t	 uval, uvalh = 0;

	if (word == NULL)
		return (0);

	if (wordlen < 1 || word[0] < '0' || word[0] > '9')
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
	if (type == RTABLE) {
		r->rtableid = uval;
		return (1);
	}

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

static u_int32_t
getcommunity(const char *s, int large, u_int8_t *flag)
{
	int64_t		 max = USHRT_MAX;
	const char	*errstr;
	u_int32_t	 uval;

	if (strcmp(s, "*") == 0) {
		*flag = COMMUNITY_ANY;
		return (0);
	}

	if (large)
		max = UINT_MAX;

	uval = strtonum(s, 0, max, &errstr);
	if (errstr)
		errx(1, "Community is %s: %s", errstr, s);

	*flag = 0;
	return (uval);
}

int
parse_community(const char *word, struct parse_result *r)
{
	struct filter_set	*fs;
	char			*p;
	u_int32_t		 as, type;
	u_int8_t		 asflag, tflag;

	/* Well-known communities */
	if (strcasecmp(word, "GRACEFUL_SHUTDOWN") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_GRACEFUL_SHUTDOWN;
		goto done;
	} else if (strcasecmp(word, "NO_EXPORT") == 0) {
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
	} else if (strcasecmp(word, "BLACKHOLE") == 0) {
		as = COMMUNITY_WELLKNOWN;
		type = COMMUNITY_BLACKHOLE;
		goto done;
	}

	if ((p = strchr(word, ':')) == NULL) {
		fprintf(stderr, "Bad community syntax\n");
		return (0);
	}
	*p++ = 0;

	as = getcommunity(word, 0, &asflag);
	type = getcommunity(p, 0, &tflag);

done:
	if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
		err(1, NULL);
	fs->type = ACTION_SET_COMMUNITY;
	fs->action.community.type = COMMUNITY_TYPE_BASIC;
	fs->action.community.c.b.data1 = as;
	fs->action.community.c.b.data2 = type;
	fs->action.community.dflag1 = asflag;
	fs->action.community.dflag2 = tflag;

	r->community = fs->action.community;

	TAILQ_INSERT_TAIL(&r->set, fs, entry);
	return (1);
}

int
parsesubtype(const char *name, u_int8_t *type, u_int8_t *subtype)
{
	const struct ext_comm_pairs *cp;
	int found = 0;

	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (strcmp(name, cp->subname) == 0) {
			if (found == 0) {
				*type = cp->type;
				*subtype = cp->subtype;
			}
			found++;
		}
	}
	if (found > 1)
		*type = -1;
	return (found);
}

int
parseextvalue(const char *s, u_int32_t *v)
{
	const char	*errstr;
	char		*p;
	struct in_addr	 ip;
	u_int32_t	 uvalh = 0, uval;

	if ((p = strchr(s, '.')) == NULL) {
		/* AS_PLAIN number (4 or 2 byte) */
		uval = strtonum(s, 0, UINT_MAX, &errstr);
		if (errstr) {
			fprintf(stderr, "Bad ext-community: %s is %s\n", s,
			    errstr);
			return (-1);
		}
		*v = uval;
		if (uval <= USHRT_MAX)
			return (EXT_COMMUNITY_TRANS_TWO_AS);
		else
			return (EXT_COMMUNITY_TRANS_FOUR_AS);
	} else if (strchr(p + 1, '.') == NULL) {
		/* AS_DOT number (4-byte) */
		*p++ = '\0';
		uvalh = strtonum(s, 0, USHRT_MAX, &errstr);
		if (errstr) {
			fprintf(stderr, "Bad ext-community: %s is %s\n", s,
			    errstr);
			return (-1);
		}
		uval = strtonum(p, 0, USHRT_MAX, &errstr);
		if (errstr) {
			fprintf(stderr, "Bad ext-community: %s is %s\n", p,
			    errstr);
			return (-1);
		}
		*v = uval | (uvalh << 16);
		return (EXT_COMMUNITY_TRANS_FOUR_AS);
	} else {
		/* more than one dot -> IP address */
		if (inet_aton(s, &ip) == 0) {
			fprintf(stderr, "Bad ext-community: %s not parseable\n",
			    s);
			return (-1);
		}
		*v = ntohl(ip.s_addr);
		return (EXT_COMMUNITY_TRANS_IPV4);
	}
	return (-1);
}

u_int
parseextcommunity(const char *word, struct parse_result *r)
{
	struct filter_set		*fs;
	const struct ext_comm_pairs	*cp;
	const char			*errstr;
	u_int64_t			 ullval;
	u_int32_t			 uval;
	char				*p, *ep;
	int				 type;

	type = r->community.c.e.type;

	switch (type) {
	case 0xff:
		if ((p = strchr(word, ':')) == NULL) {
			fprintf(stderr, "Bad ext-community: %s\n", word);
			return (0);
		}
		*p++ = '\0';
		if ((type = parseextvalue(word, &uval)) == -1)
			return (0);
		switch (type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
			ullval = strtonum(p, 0, UINT_MAX, &errstr);
			break;
		case EXT_COMMUNITY_TRANS_IPV4:
		case EXT_COMMUNITY_TRANS_FOUR_AS:
			ullval = strtonum(p, 0, USHRT_MAX, &errstr);
			break;
		default:
			fprintf(stderr, "parseextcommunity: unexpected "
			    "result\n");
			return (0);
		}
		if (errstr) {
			fprintf(stderr, "Bad ext-community: %s is %s\n", p,
			    errstr);
			return (0);
		}
		switch (type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
			r->community.c.e.data1 = uval;
			r->community.c.e.data2 = ullval;
			break;
		case EXT_COMMUNITY_TRANS_IPV4:
		case EXT_COMMUNITY_TRANS_FOUR_AS:
			r->community.c.e.data1 = uval;
			r->community.c.e.data2 = ullval;
			break;
		}
		break;
	case EXT_COMMUNITY_TRANS_OPAQUE:
	case EXT_COMMUNITY_TRANS_EVPN:
		errno = 0;
		ullval = strtoull(word, &ep, 0);
		if (word[0] == '\0' || *ep != '\0') {
			fprintf(stderr, "Bad ext-community: bad value\n");
			return (0);
		}
		if (errno == ERANGE && ullval > EXT_COMMUNITY_OPAQUE_MAX) {
			fprintf(stderr, "Bad ext-community: too big\n");
			return (0);
		}
		r->community.c.e.data2 = ullval;
		break;
	case EXT_COMMUNITY_NON_TRANS_OPAQUE:
		if (strcmp(word, "valid") == 0)
			r->community.c.e.data2 = EXT_COMMUNITY_OVS_VALID;
		else if (strcmp(word, "invalid") == 0)
			r->community.c.e.data2 = EXT_COMMUNITY_OVS_INVALID;
		else if (strcmp(word, "not-found") == 0)
			r->community.c.e.data2 = EXT_COMMUNITY_OVS_NOTFOUND;
		else {
			fprintf(stderr, "Bad ext-community value: %s\n", word);
			return (0);
		}
		break;
	}
	r->community.c.e.type = type;

	/* verify type/subtype combo */
	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (cp->type == r->community.c.e.type &&
		    cp->subtype == r->community.c.e.subtype) {
			r->community.type = COMMUNITY_TYPE_EXT;;
			if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
				err(1, NULL);

			fs->type = ACTION_SET_COMMUNITY;
			memcpy(&fs->action.community, &r->community,
			    sizeof(struct filter_community));

			TAILQ_INSERT_TAIL(&r->set, fs, entry);
			return (1);
		}
	}

	fprintf(stderr, "Bad ext-community: bad format for type\n");
	return (0);
}

int
parse_largecommunity(const char *word, struct parse_result *r)
{
	struct filter_set *fs;
	char		*p, *po = strdup(word);
	char		*array[3] = { NULL, NULL, NULL };
	char		*val;
	u_int32_t	 as, ld1, ld2;
	u_int8_t	 asflag, ld1flag, ld2flag;
	int		 i = 0;

	p = po;
	while ((p != NULL) && (i < 3)) {
		val = strsep(&p, ":");
		array[i++] = val;
	}

	if ((p != NULL) || !(array[0] && array[1] && array[2]))
		errx(1, "Invalid Large-Community syntax");

	as = getcommunity(array[0], 1, &asflag);
	ld1 = getcommunity(array[1], 1, &ld1flag);
	ld2 = getcommunity(array[2], 1, &ld2flag);

	free(po);

	if ((fs = calloc(1, sizeof(struct filter_set))) == NULL)
		err(1, NULL);
	fs->type = ACTION_SET_COMMUNITY;
	fs->action.community.type = COMMUNITY_TYPE_LARGE;
	fs->action.community.c.l.data1 = as;
	fs->action.community.c.l.data2 = ld1;
	fs->action.community.c.l.data3 = ld2;
	fs->action.community.dflag1 = asflag;
	fs->action.community.dflag2 = ld1flag;
	fs->action.community.dflag3 = ld2flag;

	r->community = fs->action.community;

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

/*	$OpenBSD: parser.c,v 1.15 2014/04/14 12:56:21 blambert Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <event.h>

#include "snmpd.h"
#include "snmp.h"
#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	TRAPOID,
	ELEMENTOBJECT,
	VALTYPE,
	IPADDRVAL,
	INT32VAL,
	UINT32VAL,
	INT64VAL,
	STRINGVAL,
	SNMPOID,
	SNMPHOST,
	SNMPCOMMUNITY,
	SNMPVERSION
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_show[];
static const struct token t_trap[];
static const struct token t_trapoid[];
static const struct token t_element[];
static const struct token t_oid[];
static const struct token t_type[];
static const struct token t_ipaddr[];
static const struct token t_int32[];
static const struct token t_uint32[];
static const struct token t_int64[];
static const struct token t_string[];
static const struct token t_snmp[];
static const struct token t_snmpclient[];
static const struct token t_snmphost[];
static const struct token t_snmpoid[];
static const struct token t_snmpcommunity[];
static const struct token t_snmpversion[];

static const struct token t_main[] = {
	{KEYWORD,	"monitor",	MONITOR,	NULL},
	{KEYWORD,	"show",		NONE,		t_show},
	{KEYWORD,	"snmp",		NONE,		t_snmp},
	{KEYWORD,	"trap",		NONE,		t_trap},
	{KEYWORD,	"walk",		WALK,		t_snmphost},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{KEYWORD,	"mib",		SHOW_MIB,	NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmp[] = {
	{KEYWORD,	"bulkwalk",	BULKWALK,	t_snmphost},
	{KEYWORD,	"get",		GET,		t_snmphost},
	{KEYWORD,	"walk",		WALK,		t_snmphost},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmphost[] = {
	{SNMPHOST,	"",		NONE,		t_snmpclient},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmpclient[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"oid",		NONE,		t_snmpoid},
	{KEYWORD,	"community",	NONE,		t_snmpcommunity},
	{KEYWORD,	"version",	NONE,		t_snmpversion},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmpoid[] = {
	{SNMPOID,	"",		NONE,		t_snmpclient},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmpcommunity[] = {
	{SNMPCOMMUNITY,	"",		NONE,		t_snmpclient},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_snmpversion[] = {
	{SNMPVERSION,	"",		NONE,		t_snmpclient},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_trap[] = {
	{KEYWORD,	"send",		TRAP,		t_trapoid},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_trapoid[] = {
	{TRAPOID,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_element[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"oid",		NONE,		t_oid},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_oid[] = {
	{ELEMENTOBJECT,	"",		NONE,		t_type},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_type[] = {
	{VALTYPE,	"ip",		SNMP_IPADDR,	t_ipaddr },
	{VALTYPE,	"counter",	SNMP_COUNTER32,	t_int32 },
	{VALTYPE,	"gauge",	SNMP_GAUGE32,	t_int32 },
	{VALTYPE,	"unsigned",	SNMP_GAUGE32,	t_uint32 },
	{VALTYPE,	"ticks",	SNMP_TIMETICKS,	t_int32 },
	{VALTYPE,	"opaque",	SNMP_OPAQUE,	t_int32 },
	{VALTYPE,	"nsap",		SNMP_NSAPADDR,	t_int32 },
	{VALTYPE,	"counter64",	SNMP_COUNTER64,	t_int64 },
	{VALTYPE,	"uint",		SNMP_UINTEGER32, t_uint32 },
	{VALTYPE,	"int",		SNMP_INTEGER32,	t_int32 },
	{VALTYPE,	"bitstring",	SNMP_BITSTRING,	t_string },
	{VALTYPE,	"string",	SNMP_OCTETSTRING, t_string },
	{VALTYPE,	"null",		SNMP_NULL,	t_element },
	{VALTYPE,	"oid",		SNMP_OBJECT,	t_string },
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_ipaddr[] = {
	{IPADDRVAL,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_int32[] = {
	{INT32VAL,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_uint32[] = {
	{UINT32VAL,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_int64[] = {
	{INT64VAL,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_string[] = {
	{STRINGVAL,	"",		NONE,		t_element},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static struct parse_result	 res;

const struct token		*match_token(char *, const struct token []);
void				 show_valid_args(const struct token []);

struct parse_result *
parse(int argc, char *argv[])
{
	const struct token	*table = t_main;
	const struct token	*match;

	bzero(&res, sizeof(res));
	res.version = -1;
	TAILQ_INIT(&res.oids);
	TAILQ_INIT(&res.varbinds);

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
match_token(char *word, const struct token table[])
{
	u_int			 i, match = 0;
	const struct token	*t = NULL;
	const char		*errs = NULL;
	int			 terminal = 0;
	struct parse_val	*val;
	struct parse_varbind	*vb = NULL;

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
		case SNMPHOST:
			if (!match && word != NULL && strlen(word) > 0 &&
			    res.host == NULL) {
				if ((res.host = strdup(word)) == NULL)
					err(1, "strdup");
				match++;
				t = &table[i];
			}
			break;
		case SNMPOID:
			if (!match && word != NULL && strlen(word) > 0) {
				if ((val = calloc(1, sizeof(*val))) == NULL ||
				    (val->val = strdup(word)) == NULL)
					err(1, "strdup");
				TAILQ_INSERT_TAIL(&res.oids, val, val_entry);
				match++;
				t = &table[i];
			}
			break;
		case SNMPCOMMUNITY:
			if (!match && word != NULL && strlen(word) > 0 &&
			    res.community == NULL) {
				if ((res.community = strdup(word)) == NULL)
					err(1, "strdup");
				match++;
				t = &table[i];
			}
			break;
		case SNMPVERSION:
			if (!match && word != NULL && strlen(word) > 0 &&
			    res.version == -1) {
				if (strcmp("1", word) == 0)
					res.version = SNMP_V1;
				else if (strcmp("2c", word) == 0)
					res.version = SNMP_V2;
				else
					break;
				match++;
				t = &table[i];
			}
			break;
		case VALTYPE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
				if (vb == NULL)
					errx(1, "inconsistent varbind list");
				vb->sm.snmp_type = t->value;
				if (t->value == SNMP_NULL)
					terminal = 1;
			}
			break;
		case TRAPOID:
			if (word == NULL || strlen(word) == 0)
				break;
			if ((res.trapoid = strdup(word)) == NULL)
				err(1, "malloc");
			match++;
			t = &table[i];
			break;
		case ELEMENTOBJECT:
			if (word == NULL || strlen(word) == 0)
				break;
			if ((vb = calloc(1, sizeof(*vb))) == NULL)
				errx(1, "calloc");
			if (strlcpy(vb->sm.snmp_oid, word,
			    sizeof(vb->sm.snmp_oid)) >= sizeof(vb->sm.snmp_oid))
				errx(1, "oid too long");

			TAILQ_INSERT_TAIL(&res.varbinds, vb, vb_entry);
			match++;
			t = &table[i];
			break;
		case IPADDRVAL:
			if (word == NULL || strlen(word) == 0)
				break;
			vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
			if (vb == NULL)
				errx(1, "inconsistent varbind list");
			if (inet_pton(AF_INET, word, &vb->u.in4) == -1) {
				/* XXX the SNMP_IPADDR type is IPv4-only? */
				if (inet_pton(AF_INET6, word, &vb->u.in6) == -1)
					errx(1, "invalid IP address");
				vb->sm.snmp_len = sizeof(vb->u.in6);
			} else {
				vb->sm.snmp_len = sizeof(vb->u.in4);
			}
			terminal = 1;
			break;
		case INT32VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
			if (vb == NULL)
				errx(1, "inconsistent varbind list");
			vb->u.d = strtonum(word, INT_MIN, INT_MAX, &errs);
			vb->sm.snmp_len = sizeof(vb->u.d);
			terminal = 1;
			break;
		case UINT32VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
			if (vb == NULL)
				errx(1, "inconsistent varbind list");
			vb->u.u = strtonum(word, 0, UINT_MAX, &errs);
			vb->sm.snmp_len = sizeof(vb->u.u);
			terminal = 1;
			break;
		case INT64VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
			if (vb == NULL)
				errx(1, "inconsistent varbind list");
			vb->u.l = strtonum(word, INT64_MIN, INT64_MAX, &errs);
			vb->sm.snmp_len = sizeof(vb->u.l);
			terminal = 1;
			break;
		case STRINGVAL:
			if (word == NULL || strlen(word) == 0)
				break;
			vb = TAILQ_LAST(&res.varbinds, parse_varbinds);
			if (vb == NULL)
				errx(1, "inconsistent varbind list");
			vb->u.str = word;
			vb->sm.snmp_len = strlen(word);
			terminal = 1;
			break;
		case ENDTOKEN:
			break;
		}
		if (terminal)
			break;
	}

	if (terminal) {
		t = &table[i];
	} else if (match != 1) {
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
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case VALTYPE:
			fprintf(stderr, "  %s <value>\n", table[i].keyword);
			break;
		case SNMPHOST:
			fprintf(stderr, "  <hostname>\n");
			break;
		case SNMPOID:
		case TRAPOID:
		case ELEMENTOBJECT:
			fprintf(stderr, "  <oid-string>\n");
			break;
		case IPADDRVAL:
			fprintf(stderr, "  <ip-address>\n");
			break;
		case INT32VAL:
			fprintf(stderr, "  <int32>\n");
			break;
		case UINT32VAL:
			fprintf(stderr, "  <uint32>\n");
			break;
		case INT64VAL:
			fprintf(stderr, "  <int64>\n");
			break;
		case STRINGVAL:
		case SNMPCOMMUNITY:
			fprintf(stderr, "  <string>\n");
			break;
		case SNMPVERSION:
			fprintf(stderr, "  [1|2c]\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

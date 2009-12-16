/*	$OpenBSD: parser.c,v 1.9 2009/12/16 22:17:53 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@vantronix.net>
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
	STRINGVAL
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

static const struct token t_main[] = {
	{KEYWORD,	"monitor",	MONITOR,	NULL},
	{KEYWORD,	"show",		NONE,		t_show},
	{KEYWORD,	"trap",		NONE,		t_trap},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{KEYWORD,	"mib",		SHOW_MIB,	NULL},
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
static struct imsgbuf		*ibuf;
static struct snmp_imsg		 sm;

const struct token		*match_token(char *, const struct token []);
void				 show_valid_args(const struct token []);

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
match_token(char *word, const struct token table[])
{
	u_int			 i, match = 0;
	const struct token	*t = NULL;
	const char		*errs = NULL;
	u_int32_t		 u;
	int32_t			 d;
	int64_t			 l;
	struct iovec		 iov[2];
	int			 iovcnt = 0;
	struct in_addr		 in4;
	struct in6_addr		 in6;

	bzero(&iov, sizeof(iov));

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
		case VALTYPE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				sm.snmp_type = t->value;
				if (t->value == SNMP_NULL)
					iovcnt = 1;
			}
			break;
		case TRAPOID:
			if (word == NULL || strlen(word) == 0)
				break;
			if (ibuf == NULL &&
			    (ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
				err(1, "malloc");
			res.ibuf = ibuf;
			imsg_init(ibuf, -1);

			/* Create a new trap */
			imsg_compose(ibuf, IMSG_SNMP_TRAP,
			    0, 0, -1, NULL, 0);

			/* First element must be the trap OID. */
			bzero(&sm, sizeof(sm));
			sm.snmp_type = SNMP_NULL;
			if (strlcpy(sm.snmp_oid, word,
			    sizeof(sm.snmp_oid)) >= sizeof(sm.snmp_oid))
				errx(1, "trap oid too long");
			if (imsg_compose(ibuf, IMSG_SNMP_ELEMENT, 0, 0, -1,
			    &sm, sizeof(sm)) == -1)
				errx(1, "imsg");

			match++;
			t = &table[i];
			break;
		case ELEMENTOBJECT:
			if (word == NULL || strlen(word) == 0)
				break;
			bzero(&sm, sizeof(sm));
			if (strlcpy(sm.snmp_oid, word,
			    sizeof(sm.snmp_oid)) >= sizeof(sm.snmp_oid))
				errx(1, "oid too long");
			match++;
			t = &table[i];
			break;
		case IPADDRVAL:
			if (word == NULL || strlen(word) == 0)
				break;
			if (inet_pton(AF_INET, word, &in4) == -1) {
				/* XXX the SNMP_IPADDR type is IPv4-only? */
				if (inet_pton(AF_INET6, word, &in6) == -1)
					errx(1, "invalid IP address");
				iov[1].iov_len = sizeof(in6);
				iov[1].iov_base = &in6;
			} else {
				iov[1].iov_len = sizeof(in4);
				iov[1].iov_base = &in4;
			}
			iovcnt = 2;
			break;
		case INT32VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			d = strtonum(word, INT_MIN, INT_MAX, &errs);
			iov[1].iov_len = sizeof(d);
			iov[1].iov_base = &d;
			iovcnt = 2;
			break;
		case UINT32VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			u = strtonum(word, 0, UINT_MAX, &errs);
			iov[1].iov_len = sizeof(u);
			iov[1].iov_base = &u;
			iovcnt = 2;
			break;
		case INT64VAL:
			if (word == NULL || strlen(word) == 0)
				break;
			l = strtonum(word, INT64_MIN, INT64_MAX, &errs);
			iov[1].iov_len = sizeof(l);
			iov[1].iov_base = &l;
			iovcnt = 2;
			break;
		case STRINGVAL:
			if (word == NULL || strlen(word) == 0)
				break;
			iov[1].iov_len = strlen(word);
			iov[1].iov_base = word;
			iovcnt = 2;
			break;
		case ENDTOKEN:
			break;
		}
		if (iovcnt)
			break;
	}

	if (iovcnt) {
		/* Write trap varbind element */
		sm.snmp_len = iov[1].iov_len;
		iov[0].iov_len = sizeof(sm);
		iov[0].iov_base = &sm;
		if (imsg_composev(ibuf, IMSG_SNMP_ELEMENT, 0, 0, -1,
		    iov, iovcnt) == -1)
			err(1, "imsg");

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
			fprintf(stderr, "  <string>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

/*	$OpenBSD: parser.c,v 1.12 2007/09/07 08:33:31 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>

#include <openssl/ssl.h>

#include "hoststated.h"

#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	HOSTID,
	TABLEID,
	SERVICEID,
	KEYWORD
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_show[];
static const struct token t_service[];
static const struct token t_table[];
static const struct token t_host[];
static const struct token t_service_id[];
static const struct token t_table_id[];
static const struct token t_host_id[];

static const struct token t_main[] = {
	{KEYWORD,	"monitor",	MONITOR,	NULL},
	{KEYWORD,	"show",		NULL,		t_show},
	{KEYWORD,	"reload",	RELOAD,		NULL},
	{KEYWORD,	"stop",		SHUTDOWN,	NULL},
	{KEYWORD,	"service",	NONE,		t_service},
	{KEYWORD,	"table",	NONE,		t_table},
	{KEYWORD,	"host",		NONE,		t_host},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_show[] = {
	{KEYWORD,	"summary",	SHOW_SUM,	NULL},
	{KEYWORD,	"hosts",	SHOW_HOSTS,	NULL},
	{KEYWORD,	"relays",	SHOW_RELAYS,	NULL},
	{KEYWORD,	"sessions",	SHOW_SESSIONS,	NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_service[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"disable",	SERV_DISABLE,	t_service_id},
	{KEYWORD,	"enable",	SERV_ENABLE,	t_service_id},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_table[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"disable",	TABLE_DISABLE,	t_table_id},
	{KEYWORD,	"enable",	TABLE_ENABLE,	t_table_id},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_host[] = {
	{NOTOKEN,	"",		NONE,		NULL},
	{KEYWORD,	"disable",	HOST_DISABLE,	t_host_id},
	{KEYWORD,	"enable",	HOST_ENABLE,	t_host_id},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_service_id[] = {
	{SERVICEID,	"",		NONE,		NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_table_id[] = {
	{TABLEID,	"",		NONE,		NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
};

static const struct token t_host_id[] = {
	{HOSTID,	"",		NONE,		NULL},
	{ENDTOKEN,	"",		NONE,		NULL}
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
match_token(const char *word, const struct token table[])
{
	u_int			 i, match;
	const struct token	*t = NULL;
	const char		*errstr;

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
		case HOSTID:
			if (word == NULL)
				break;
			res.id.id = strtonum(word, 0, UINT_MAX, &errstr);
			if (errstr) {
				strlcpy(res.id.name, word, sizeof(res.id.name));
				res.id.id = EMPTY_ID;
			}
			t = &table[i];
			match++;
			break;
		case TABLEID:
			if (word == NULL)
				break;
			res.id.id = strtonum(word, 0, UINT_MAX, &errstr);
			if (errstr) {
				strlcpy(res.id.name, word, sizeof(res.id.name));
				res.id.id = EMPTY_ID;
			}
			t = &table[i];
			match++;
			break;
		case SERVICEID:
			if (word == NULL)
				break;
			res.id.id = strtonum(word, 0, UINT_MAX, &errstr);
			if (errstr) {
				strlcpy(res.id.name, word, sizeof(res.id.name));
				res.id.id = EMPTY_ID;
			}
			t = &table[i];
			match++;
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
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case SERVICEID:
			fprintf(stderr, "  <serviceid>\n");
			break;
		case TABLEID:
			fprintf(stderr, "  <tableid>\n");
			break;
		case HOSTID:
			fprintf(stderr, "  <hostid>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

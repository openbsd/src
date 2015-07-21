/*	$OpenBSD: parser.c,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

enum token_type {
	NOTOKEN,
	KEYWORD,
	HOSTNAME,
	SECRET,
	USERNAME,
	PASSWORD,
	PORT,
	METHOD,
	NAS_PORT,
	ENDTOKEN
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static struct parse_result res;

static const struct token t_test[];
static const struct token t_secret[];
static const struct token t_username[];
static const struct token t_test_opts[];
static const struct token t_password[];
static const struct token t_port[];
static const struct token t_method[];
static const struct token t_nas_port[];

static const struct token t_main[] = {
	{ KEYWORD,	"test",		TEST,		t_test },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_test[] = {
	{ HOSTNAME,	"",		NONE,		t_secret },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_secret[] = {
	{ SECRET,	"",		NONE,		t_username },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_username[] = {
	{ USERNAME,	"",		NONE,		t_test_opts },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_test_opts[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ KEYWORD,	"password",	NONE,		t_password },
	{ KEYWORD,	"port",		NONE,		t_port },
	{ KEYWORD,	"method",	NONE,		t_method },
	{ KEYWORD,	"nas-port",	NONE,		t_nas_port },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_password[] = {
	{ PASSWORD,	"",		NONE,		t_test_opts },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_port[] = {
	{ PORT,		"",		NONE,		t_test_opts },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_method[] = {
	{ METHOD,	"",		NONE,		t_test_opts },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_nas_port[] = {
	{ NAS_PORT,	"",		NONE,		t_test_opts },
	{ ENDTOKEN,	"",		NONE,		NULL }
};


static const struct token	*match_token(char *, const struct token []);
static void			 show_valid_args(const struct token []);

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

static const struct token *
match_token(char *word, const struct token table[])
{
	u_int			 i, match = 0;
	const struct token	*t = NULL;
	long long		 num;
	const char		*errstr;

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
		case HOSTNAME:
			if (word == NULL)
				break;
			match++;
			res.hostname = word;
			t = &table[i];
			break;
		case SECRET:
			if (word == NULL)
				break;
			match++;
			res.secret = word;
			t = &table[i];
			break;
		case USERNAME:
			if (word == NULL)
				break;
			match++;
			res.username = word;
			t = &table[i];
			break;
		case PASSWORD:
			if (word == NULL)
				break;
			match++;
			res.password = word;
			t = &table[i];
			break;
		case PORT:
			if (word == NULL)
				break;
			num = strtonum(word, 1, UINT16_MAX, &errstr);
			if (errstr != NULL) {
				fprintf(stderr,
				    "invalid argument: %s is %s for \"port\"\n",
				    word, errstr);
				return (NULL);
			}
			match++;
			res.port = num;
			t = &table[i];
			break;
		case METHOD:
			if (word == NULL)
				break;
			if (strcasecmp(word, "pap") == 0)
				res.auth_method = PAP;
			else if (strcasecmp(word, "chap") == 0)
				res.auth_method = CHAP;
			else if (strcasecmp(word, "mschapv2") == 0)
				res.auth_method = MSCHAPV2;
			else {
				fprintf(stderr,
				    "invalid argument: %s for \"method\"\n",
				    word);
				return (NULL);
			}
			match++;
			t = &table[i];
			break;
		case NAS_PORT:
			if (word == NULL)
				break;
			num = strtonum(word, 0, 65535, &errstr);
			if (errstr != NULL) {
				fprintf(stderr,
				    "invalid argument: %s is %s for "
				    "\"nas-port\"\n", word, errstr);
				return (NULL);
			}
			match++;
			res.nas_port = num;
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

static void
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
		case HOSTNAME:
			fprintf(stderr, "  <hostname>\n");
			break;
		case SECRET:
			fprintf(stderr, "  <radius secret>\n");
			break;
		case USERNAME:
			fprintf(stderr, "  <username>\n");
			break;
		case PASSWORD:
			fprintf(stderr, "  <password>\n");
			break;
		case PORT:
			fprintf(stderr, "  <port number>\n");
			break;
		case METHOD:
			fprintf(stderr, "  <auth method (pap, chap, "
			    "mschapv2)>\n");
			break;
		case NAS_PORT:
			fprintf(stderr, "  <nas-port (0-65535)>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

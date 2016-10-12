/*	$OpenBSD: parser.c,v 1.3 2016/10/12 19:07:42 reyk Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <netdb.h>

#include "switchd.h"
#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	PATH,
	ADDRESS,
	URI
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_reset[];
static const struct token t_log[];
static const struct token t_load[];
static const struct token t_show[];
static const struct token t_connect[];
static const struct token t_disconnect[];
static const struct token t_forward_to[];
static const struct token t_uri[];

static const struct token t_main[] = {
	{ KEYWORD,	"connect",	CONNECT,	t_connect },
	{ KEYWORD,	"disconnect",	DISCONNECT,	t_disconnect },
	{ KEYWORD,	"load",		LOAD,		t_load },
	{ KEYWORD,	"log",		NONE,		t_log },
	{ KEYWORD,	"monitor",	MONITOR,	NULL },
	{ KEYWORD,	"reload",	RELOAD,		NULL },
	{ KEYWORD,	"reset",	NONE,		t_reset },
	{ KEYWORD,	"show",		NONE,		t_show },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_log[] = {
	{ KEYWORD,	"verbose",	LOG_VERBOSE,	NULL },
	{ KEYWORD,	"brief",	LOG_BRIEF,	NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_reset[] = {
	{ KEYWORD,	"all",		RESETALL,	NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_load[] = {
	{ PATH,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_show[] = {
	{ KEYWORD,	"summary",	SHOW_SUM,	NULL },
	{ KEYWORD,	"switches",	SHOW_SWITCHES,	NULL },
	{ KEYWORD,	"macs",		SHOW_MACS,	NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};
static const struct token t_connect[] = {
	{ ADDRESS,	"",		NONE,		t_forward_to },
	{ ENDTOKEN,	"",		NONE,		NULL }
};
static const struct token t_disconnect[] = {
	{ ADDRESS,	"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};
static const struct token t_forward_to[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ KEYWORD,	"forward-to",	NONE,		t_uri },
	{ ENDTOKEN,	"",		NONE,		NULL }
};
static const struct token  t_uri[] = {
	{ URI,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static struct parse_result	 res;

const struct token		*match_token(char *, const struct token []);
void				 show_valid_args(const struct token []);
int				 parse_addr(const char *,
				    struct sockaddr_storage *);

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

int
parse_addr(const char *word, struct sockaddr_storage *ss)
{
	struct addrinfo		 hints, *ai;
	struct sockaddr_un	*un;

	memset(ss, 0, sizeof(*ss));

	/* device */
	if (*word == '/') {
		un = (struct sockaddr_un *)ss;
		if (strlcpy(un->sun_path, word, sizeof(un->sun_path)) >=
		    sizeof(un->sun_path)) {
			warnx("invalid path");
			return (-1);
		}
		un->sun_family = AF_LOCAL;
		un->sun_len = sizeof(*un);
		return (0);
	}

	/* address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM; /* dummy */
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(word, "0", &hints, &ai) == 0) {
		if (ai->ai_addrlen > sizeof(*ss)) {
			warnx("invalid address length");
			return (-1);
		}
		memcpy(ss, ai->ai_addr, ai->ai_addrlen);
		ss->ss_len = ai->ai_addrlen;
		freeaddrinfo(ai);
		return (0);
	}

	/* FQDN */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM; /* dummy */
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	if (getaddrinfo(word, "0", &hints, &ai) == 0) {
		/* Pick first name only */
		if (ai->ai_addrlen > sizeof(*ss)) {
			warnx("invalid address length");
			return (-1);
		}
		memcpy(ss, ai->ai_addr, ai->ai_addrlen);
		ss->ss_len = ai->ai_addrlen;
		freeaddrinfo(ai);
		return (0);
	}

	return (-1);
}


const struct token *
match_token(char *word, const struct token table[])
{
	unsigned int		 i, match = 0;
	const struct token	*t = NULL;

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
		case PATH:
			if (!match && word != NULL && strlen(word) > 0) {
				res.path = strdup(word);
				match++;
				t = &table[i];
			}
			break;
		case ADDRESS:
			if (!match && word != NULL && strlen(word) > 0) {
				parse_addr(word, &res.addr);
				match++;
				t = &table[i];
			}
			break;
		case URI:
			if (!match && word != NULL && strlen(word) > 0) {
				res.uri = strdup(word);
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
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case PATH:
			fprintf(stderr, "  <path>\n");
			break;
		case ADDRESS:
			fprintf(stderr, "  <address>\n");
			break;
		case URI:
			fprintf(stderr, "  <uri>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

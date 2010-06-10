/*	$OpenBSD: parser.c,v 1.2 2010/06/10 14:08:37 reyk Exp $	*/

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <netdb.h>

#include "iked.h"
#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	FILENAME,
	CANAME,
	ADDRESS,
	FQDN
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
static const struct token t_ca[];
static const struct token t_ca_modifiers[];
static const struct token t_ca_cert[];
static const struct token t_ca_cert_modifiers[];
static const struct token t_show[];
static const struct token t_show_ca[];
static const struct token t_show_ca_modifiers[];

static const struct token t_main[] = {
	{ KEYWORD,	"active",	ACTIVE,		NULL },
	{ KEYWORD,	"passive",	PASSIVE,	NULL },
	{ KEYWORD,	"couple",	COUPLE,		NULL },
	{ KEYWORD,	"decouple",	DECOUPLE,	NULL },
	{ KEYWORD,	"load",		LOAD,		t_load },
	{ KEYWORD,	"log",		NONE,		t_log },
	{ KEYWORD,	"monitor",	MONITOR,	NULL },
	{ KEYWORD,	"reload",	RELOAD,		NULL },
	{ KEYWORD,	"reset",	NONE,		t_reset },
	{ KEYWORD,	"show",		NONE,		t_show },
	{ KEYWORD,	"ca",		CA,		t_ca },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_log[] = {
	{ KEYWORD,	"verbose",	LOG_VERBOSE, 	NULL },
	{ KEYWORD,	"brief",	LOG_BRIEF, 	NULL },
	{ ENDTOKEN, 	"",		NONE,		NULL }
};

static const struct token t_reset[] = {
	{ KEYWORD,	"all",		RESETALL,	NULL },
	{ KEYWORD,	"ca",		RESETCA,	NULL },
	{ KEYWORD,	"policy",	RESETPOLICY,	NULL },
	{ KEYWORD,	"sa",		RESETSA,	NULL },
	{ KEYWORD,	"user",		RESETUSER,	NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_load[] = {
	{ FILENAME,	"",		NONE,		NULL },
	{ ENDTOKEN, 	"",		NONE,		NULL }
};

static const struct token t_ca[] = {
	{ CANAME,	"",		NONE,		t_ca_modifiers },
	{ ENDTOKEN,	"",		NONE,		NULL },
};

static const struct token t_ca_modifiers[] = {
	{ KEYWORD,	"create",	CA_CREATE,	NULL },
	{ KEYWORD,	"delete",	CA_DELETE,	NULL },
	{ KEYWORD,	"install",	CA_INSTALL,	NULL },
	{ KEYWORD,	"certificate",	CA_CERTIFICATE,	t_ca_cert },
	{ ENDTOKEN, 	"",		NONE,		NULL }
};

static const struct token t_ca_cert[] = {
	{ ADDRESS,	"",		NONE,		t_ca_cert_modifiers },
	{ FQDN,		"",		NONE,		t_ca_cert_modifiers },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_ca_cert_modifiers[] = {
	{ KEYWORD,	"create",	CA_CERT_CREATE,		NULL },
	{ KEYWORD,	"delete",	CA_CERT_DELETE,		NULL },
	{ KEYWORD,	"install",	CA_CERT_INSTALL,	NULL },
	{ KEYWORD,	"export",	CA_CERT_EXPORT,		NULL },
	{ ENDTOKEN,	"",		NONE,			NULL }
};

static const struct token t_show[] = {
	{ KEYWORD,	"ca",		SHOW_CA,	t_show_ca },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_show_ca[] = {
	{ CANAME,	"",		NONE,		t_show_ca_modifiers },
	{ ENDTOKEN,	"",		NONE,		NULL },
};

static const struct token t_show_ca_modifiers[] = {
	{ KEYWORD,	"certificates",		SHOW_CA_CERTIFICATES,	NULL },
	{ ENDTOKEN,	"",			NONE,			NULL }
};

static struct parse_result	 res;

const struct token		*match_token(char *, const struct token []);
void				 show_valid_args(const struct token []);
int				 parse_addr(const char *);

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
parse_addr(const char *word)
{
	struct addrinfo hints, *r;

	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM; /* dummy */
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(word, "0", &hints, &r) == 0) {
		return (0);
	}

	return (1);
}


const struct token *
match_token(char *word, const struct token table[])
{
	u_int			 i, match = 0;
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
		case FILENAME:
			if (!match && word != NULL && strlen(word) > 0) {
				res.filename = strdup(word);
				match++;
				t = &table[i];
			}
			break;
		case CANAME:
			if (!match && word != NULL && strlen(word) > 0) {
				res.caname = strdup(word);
				match++;
				t = &table[i];
			}
			break;
		case ADDRESS:
		case FQDN:
			if (!match && word != NULL && strlen(word) > 0) {
				parse_addr(word);
				res.host = strdup(word);
				if (parse_addr(word) == 0)
					res.htype = HOST_IPADDR;
				else
					res.htype = HOST_FQDN;
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
		case FILENAME:
			fprintf(stderr, "  <filename>\n");
			break;
		case CANAME:
			fprintf(stderr, "  <caname>\n");
			break;
		case ADDRESS:
			fprintf(stderr, "  <ipaddr>\n");
			break;
		case FQDN:
			fprintf(stderr, "  <fqdn>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

/*	$OpenBSD: parser.c,v 1.2 2015/11/26 08:26:48 reyk Exp $	*/

/*
 * Copyright (c) 2010-2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/uio.h>

#include <machine/vmmvar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "vmd.h"
#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	DISK,
	NAME,
	NIFS,
	PATH,
	SIZE,
	VMID
};

struct token {
	enum token_type		 type;
	const char		*keyword;
	int			 value;
	const struct token	*next;
};

static const struct token t_main[];
static const struct token t_show[];
static const struct token t_create[];
static const struct token t_imgsize[];
static const struct token t_imgsize_val[];
static const struct token t_start[];
static const struct token t_start_name[];
static const struct token t_disk[];
static const struct token t_kernel[];
static const struct token t_memory[];
static const struct token t_ifs[];
static const struct token t_vm[];
static const struct token t_opt_id[];
static const struct token t_id[];
static const struct token t_opt_path[];

static const struct token t_main[] = {
	{ KEYWORD,	"create",	CMD_CREATE,	t_create },
	{ KEYWORD,	"load",		CMD_LOAD,	t_opt_path },
	{ KEYWORD,	"show",		NONE,		t_show },
	{ KEYWORD,	"start",	CMD_START,	t_start_name },
	{ KEYWORD,	"terminate",	CMD_TERMINATE,	t_id },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_create[] = {
	{ PATH,		"",		NONE,		t_imgsize },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_imgsize[] = {
	{ KEYWORD,	"size",		NONE,		t_imgsize_val },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_imgsize_val[] = {
	{ SIZE,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_start[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ KEYWORD,	"disk",		NONE,		t_disk },
	{ KEYWORD,	"interfaces",	NONE,		t_ifs },
	{ KEYWORD,	"kernel",	NONE,		t_kernel },
	{ KEYWORD,	"memory",	NONE,		t_memory },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_ifs[] = {
	{ NIFS,	"",		NONE,		t_start },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_disk[] = {
	{ DISK,		"",		NONE,		t_start },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_memory[] = {
	{ SIZE,		"",		NONE,		t_start },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_kernel[] = {
	{ PATH,		"",		NONE,		t_start },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_start_name[] = {
	{ NAME,		"",		NONE,		t_start },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_show[] = {
	{ KEYWORD,	"info",		CMD_INFO,	t_opt_id },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_opt_id[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ VMID,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_id[] = {
	{ VMID,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_opt_path[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ PATH,		"",		NONE,		NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
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
		case NAME:
			if (!match && word != NULL && strlen(word) > 0) {
				res.name = strdup(word);
				match++;
				t = &table[i];
			}
			break;
		case NIFS:
			if (match || word == NULL || *word == '\0')
				break;

			if (parse_ifs(&res, word, 0) == -1)
				return (NULL);

			match++;
			t = &table[i];
			break;
		case SIZE:
			if (match || word == NULL || *word == '\0')
				break;

			if (parse_size(&res, word, 0) == -1)
				return (NULL);

			match++;
			t = &table[i];
			break;
		case VMID:
			if (match || word == NULL || *word == '\0')
				break;

			if (parse_vmid(&res, word, 0) == -1)
				return (NULL);

			match++;
			t = &table[i];
			break;
		case DISK:
			if (match || word == NULL || *word == '\0')
				break;

			if (parse_disk(&res, word) == -1)
				return (NULL);

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
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case NAME:
			fprintf(stderr, "  <name>\n");
			break;
		case PATH:
		case DISK:
			fprintf(stderr, "  <path>\n");
			break;
		case NIFS:
			fprintf(stderr, "  <count>\n");
			break;
		case VMID:
			fprintf(stderr, "  <id>\n");
			break;
		case SIZE:
			fprintf(stderr, "  <size>(K|M|G)\n");
			break;
		case ENDTOKEN:
			break;
		}
	}
}

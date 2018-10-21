/*	$OpenBSD: parser.c,v 1.10 2018/10/21 21:10:24 akoshibe Exp $	*/

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
#include "ofp_map.h"
#include "parser.h"

enum token_type {
	NOTOKEN,
	ENDTOKEN,
	KEYWORD,
	PATH,
	ADDRESS,
	URI,
	TABLE,
	FLOWADD,
	FLOWDELETE,
	FLOWMODIFY,
	FLOWAPPLY,
	FLOWWRITE,
	FLOWMATCH,
	MATCHINPORT,
	ACTIONOUTPUT,
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
static const struct token t_switch[];
#if 0
static const struct token t_switchreq[];
#endif
static const struct token t_table[];
static const struct token t_dump[];
static const struct token t_flow[];
static const struct token t_flowmod[];
static const struct token t_flowmatch[];
static const struct token t_matchinport[];
static const struct token t_flowaction[];
static const struct token t_actionoutput[];
static const struct token t_connect[];
static const struct token t_disconnect[];
static const struct token t_forward_to[];
static const struct token t_uri[];

static const struct token t_main[] = {
	{ KEYWORD,	"connect",	CONNECT,	t_connect },
	{ KEYWORD,	"disconnect",	DISCONNECT,	t_disconnect },
	{ KEYWORD,	"dump",		NONE,		t_dump },
	{ KEYWORD,	"flow",		NONE,		t_flow },
	{ KEYWORD,	"load",		LOAD,		t_load },
	{ KEYWORD,	"log",		NONE,		t_log },
	{ KEYWORD,	"monitor",	MONITOR,	NULL },
	{ KEYWORD,	"reload",	RELOAD,		NULL },
	{ KEYWORD,	"reset",	NONE,		t_reset },
	{ KEYWORD,	"show",		NONE,		t_show },
	{ KEYWORD,	"switch",	NONE,		t_switch },
	{ KEYWORD,	"table",	NONE,		t_table },
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

static const struct token  t_table[] = {
	{ TABLE,	"",		NONE,		t_main },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token  t_switch[] = {
	{ URI,		"",		NONE,		t_main },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

#if 0
static const struct token  t_switchreq[] = {
	{ KEYWORD,	"dump",		NONE,		t_dump },
	{ KEYWORD,	"flow",		NONE,		t_flow },
	{ ENDTOKEN,	"",		NONE,		NULL }
};
#endif

static const struct token t_dump[] = {
	{ KEYWORD,	"desc",		DUMP_DESC,	NULL },
	{ KEYWORD,	"features",	DUMP_FEATURES,	NULL },
	{ KEYWORD,	"flows",	DUMP_FLOWS,	NULL },
	{ KEYWORD,	"tables",	DUMP_TABLES,	NULL },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_flow[] = {
	{ FLOWADD,	"add",		FLOW_ADD,	t_flowmod },
	{ FLOWDELETE,	"delete",	FLOW_DELETE,	t_flowmod },
	{ FLOWMODIFY,	"modify",	FLOW_MODIFY,	t_flowmod },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_flowmod[] = {
	{ NOTOKEN,	"",		NONE,		NULL },
	{ FLOWAPPLY,	"apply",	NONE,		t_flowaction },
	{ FLOWWRITE,	"write",	NONE,		t_flowaction },
	{ FLOWMATCH,	"match",	NONE,		t_flowmatch },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_flowmatch[] = {
	{ NOTOKEN,	"",		NONE,		t_flowmod },
	{ KEYWORD,	"inport",	NONE,		t_matchinport },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_matchinport[] = {
	{ MATCHINPORT,	"",		NONE,		t_flowmatch },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_flowaction[] = {
	{ NOTOKEN,	"",		NONE,		t_flowmod },
	{ KEYWORD,	"output",	NONE,		t_actionoutput },
	{ ENDTOKEN,	"",		NONE,		NULL }
};

static const struct token t_actionoutput[] = {
	{ ACTIONOUTPUT,	"",		NONE,		t_flowaction },
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

const struct token	*match_token(char *, const struct token [], int);
void			 show_valid_args(const struct token [], int);
int			 parse_addr(const char *,
			    struct sockaddr_storage *);

struct parse_result *
parse(int argc, char *argv[])
{
	const struct token	*table = t_main;
	const struct token	*match;

	bzero(&res, sizeof(res));

	res.table = OFP_TABLE_ID_ALL;

	while (argc >= 0) {
		if ((match = match_token(argv[0], table, 0)) == NULL) {
			fprintf(stderr, "valid commands/args:\n");
			show_valid_args(table, 0);
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
		memcpy(ss, ai->ai_addr, ai->ai_addrlen);
		ss->ss_len = ai->ai_addrlen;
		freeaddrinfo(ai);
		return (0);
	}

	return (-1);
}


const struct token *
match_token(char *word, const struct token table[], int level)
{
	unsigned int		 i, j, match = 0;
	int64_t			 val;
	struct constmap		*cm;
	const char		*errstr = NULL;
	const struct token	*t = NULL;
	size_t			 len;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			if (word == NULL || strlen(word) == 0) {
				match++;
				t = &table[i];
			}
			break;
		case KEYWORD:
		case FLOWADD:
		case FLOWDELETE:
		case FLOWMODIFY:
		case FLOWMATCH:
		case FLOWAPPLY:
		case FLOWWRITE:
			if (word != NULL && strncmp(word, table[i].keyword,
			    strlen(word)) == 0) {
				match++;
				t = &table[i];
				if (t->value)
					res.action = t->value;
				switch (table[i].type) {
				case FLOWADD:
				case FLOWDELETE:
				case FLOWMODIFY:
					if ((res.fbuf =
					    oflowmod_open(&res.fctx,
					    NULL, NULL, 0)) == NULL)
						goto flowerr;

					/* Update header */
					if (table[i].type == FLOWDELETE)
						res.fctx.ctx_fm->fm_command =
						    OFP_FLOWCMD_DELETE;
					else if (table[i].type == FLOWMODIFY)
						res.fctx.ctx_fm->fm_command =
						    OFP_FLOWCMD_MODIFY;
					break;
				case FLOWAPPLY:
					val = OFP_INSTRUCTION_T_APPLY_ACTIONS;
					if (oflowmod_instruction(&res.fctx,
					    val) == -1)
						goto flowerr;
					break;
				case FLOWWRITE:
					val = OFP_INSTRUCTION_T_WRITE_ACTIONS;
					if (oflowmod_instruction(&res.fctx,
					    val) == -1)
						goto flowerr;
					break;
				case FLOWMATCH:
					if (oflowmod_mopen(&res.fctx) == -1)
						goto flowerr;
					break;
				default:
					break;
				}
			}
			break;
		case MATCHINPORT:
		case ACTIONOUTPUT:
			if (!match && word != NULL && strlen(word) > 0) {
				match++;
				t = &table[i];

				val = -1;

				/* Is the port a keyword? */
				cm = ofp_port_map;
				for (j = 0; cm[j].cm_name != NULL; j++) {
					if (strcasecmp(cm[j].cm_name,
					    word) == 0) {
						val = cm[j].cm_type;
						break;
					}
				}

				/* Is the port a number? */
				if (val == -1) {
					val = strtonum(word, 1,
					    UINT32_MAX, &errstr);
					if (errstr != NULL)
						val = -1;
				}

				if (val == -1) {
					fprintf(stderr,
					    "could not parse port:"
					    " %s\n", word);
					return (NULL);
				}

				switch (table[i].type) {
				case MATCHINPORT:
					if (oxm_inport(res.fbuf, val) == -1)
						goto flowerr;
					break;
				case ACTIONOUTPUT:
					if (action_output(res.fbuf, val,
					    OFP_CONTROLLER_MAXLEN_MAX) == -1)
						goto flowerr;
					break;
				default:
					break;
				}
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
		case TABLE:
			if (word == NULL)
				break;
			res.table = strtonum(word, 0,
			    OFP_TABLE_ID_MAX, &errstr);
			if (errstr)
				res.table = OFP_TABLE_ID_ALL;
			t = &table[i];
			match++;
			break;
		case URI:
			if (!match && word != NULL && strlen(word) > 0) {
				len = 4;
				if (strncmp(word, "tcp:", len) == 0)
					res.uri.swa_type = SWITCH_CONN_TCP;
				else if (strncmp(word, "tls:", len) == 0)
					res.uri.swa_type = SWITCH_CONN_TLS;
				else if (strncmp(word, "/dev", len) == 0) {
					res.uri.swa_type = SWITCH_CONN_LOCAL;
					parse_addr(word, &res.uri.swa_addr);
					match++;
					t = &table[i];
					break;
				} else {
					/* set the default */
					res.uri.swa_type = SWITCH_CONN_TCP;
					len = 0;
				}
				if (parsehostport(word + len,
				    (struct sockaddr *)&res.uri.swa_addr,
				    sizeof(res.uri.swa_addr)) != 0) {
					fprintf(stderr,
					    "could not parse address: %s\n",
					    word);
					return (NULL);
				}
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
		else if (match < 1) {
			if (level == 0 &&
			    table[0].type == NOTOKEN && table[0].next)
				return (match_token(word, table[0].next, 1));
			else
				fprintf(stderr, "unknown argument: %s\n", word);
		}
		return (NULL);
	}

	return (t);

 flowerr:
	(void)oflowmod_err(&res.fctx, __func__, __LINE__);
	fprintf(stderr, "flow invalid\n");
	return (NULL);
}

void
show_valid_args(const struct token table[], int level)
{
	int	i;

	for (i = 0; table[i].type != ENDTOKEN; i++) {
		switch (table[i].type) {
		case NOTOKEN:
			if (level == 0)
				fprintf(stderr, "  <cr>\n");
			break;
		case KEYWORD:
		case FLOWADD:
		case FLOWDELETE:
		case FLOWMODIFY:
		case FLOWMATCH:
		case FLOWAPPLY:
		case FLOWWRITE:
			fprintf(stderr, "  %s\n", table[i].keyword);
			break;
		case MATCHINPORT:
		case ACTIONOUTPUT:
			fprintf(stderr, "  <port>\n");
			break;
		case PATH:
			fprintf(stderr, "  <path>\n");
			break;
		case ADDRESS:
			fprintf(stderr, "  <address>\n");
			break;
		case TABLE:
			fprintf(stderr, "  <table>\n");
			break;
		case URI:
			fprintf(stderr, "  <uri>\n");
			break;
		case ENDTOKEN:
			break;
		}
	}

	if (level == 0 && table[0].type == NOTOKEN && table[0].next)
		return (show_valid_args(table[0].next, 1));
}

/*	$OpenBSD: parse.y,v 1.3 2003/12/19 01:38:34 henning Exp $ */

/*
 * Copyright (c) 2002, 2003 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
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

%{
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bgpd.h"

enum parse_state {
	PSTATE_MAIN,
	PSTATE_GROUP,
	PSTATE_NEIGHBOR
};

static struct bgpd_config	*conf;
static struct mrt_config	*mrtconf;
static struct peer		*curpeer;
static struct peer		*curgroup;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
static int			 pdebug = 1;
static int			 pstate = PSTATE_MAIN;
char				*infile;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);

struct peer	*new_peer(void);
struct peer	*new_group(void);
int		 add_mrtconfig(enum mrtdump_type, char *, time_t);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int	 symset(const char *, const char *, int);
char	*symget(const char *);
int	 atoul(char *, u_long *);
int	 check_state_peeropts(void);
int	 check_state_main(void);

typedef struct {
	union {
		u_int32_t	 number;
		char		*string;
		struct in_addr	 addr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	SET AS BGPID HOLDTIME HOLDTIME_MIN
%token	GROUP NEIGHBOR
%token	REMOTEAS DESCR LOCALADDR MULTIHOP
%token	ERROR
%token	MRTDUMP
%token	<v.string>	STRING
%type	<v.number>	number
%type	<v.string>	string
%type	<v.addr>	address
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar neighbor '\n'
		| grammar group '\n'
		| grammar peeropts '\n'
		| grammar endsection '\n'
		| grammar varset '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				YYERROR;
			} else
				$$ = ulval;
		}
		;

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				fatal("string: asprintf", errno);
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string		{
			if (conf->opts & BGPD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable", errno);
		}
		;

conf_main	: AS number		{
			if (check_state_main())
				YYERROR;
			conf->as = $2;
		}
		| BGPID address		{
			if (check_state_main())
				YYERROR;
			conf->bgpid = $2.s_addr;
		}
		| HOLDTIME number	{
			if (check_state_main())
				YYERROR;
			if ($2 < MIN_HOLDTIME) {
				yyerror("holdtime must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			conf->holdtime = $2;
		}
		| HOLDTIME_MIN number	{
			if (check_state_main())
				YYERROR;
			if ($2 < MIN_HOLDTIME) {
				yyerror("holdtime_min must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			conf->min_holdtime = $2;
		}
		/*
		 *  XXX this is bad.
		 *  a) number should be optional
		 *  b) there are multiple dump types
		 */
		| MRTDUMP STRING STRING number	{
			if (check_state_main())
				YYERROR;
			if (strcmp($2, "table") == 0) {
				if (add_mrtconfig(MRT_TABLE_DUMP, $3, $4) == -1)
					YYERROR;
			} else {
				yyerror("unknown mrtdump type %s", $2);
				YYERROR;
			}
		}
		;

address		: STRING		{
			int	n;

			if ((n = inet_pton(AF_INET, $1, &$$)) == -1) {
				yyerror("inet_pton: %s", strerror(errno));
				YYERROR;
			}
			if (n == 0) {
				yyerror("could not parse address spec %s", $1);
				YYERROR;
			}
		}
		;

neighbor	: NEIGHBOR address '{'	{
			if (pstate >= PSTATE_NEIGHBOR) {
				yyerror("neighbor statment not allowed in this "
				    "context");
				YYERROR;
			}
			pstate = PSTATE_NEIGHBOR;
			curpeer = new_peer();
			curpeer->conf.remote_addr.sin_len =
			    sizeof(curpeer->conf.remote_addr);
			curpeer->conf.remote_addr.sin_family = AF_INET;
			curpeer->conf.remote_addr.sin_port = htons(BGP_PORT);
			curpeer->conf.remote_addr.sin_addr.s_addr = $2.s_addr;
		}
		;

group		: GROUP string '{'	{
			if (pstate >= PSTATE_GROUP) {
				yyerror("group statment not allowed in this "
				    "context");
				YYERROR;
			}
			pstate = PSTATE_GROUP;
			curgroup = curpeer = new_group();
			if (strlcpy(curgroup->conf.group, $2,
			    sizeof(curgroup->conf.group)) >
			    sizeof(curgroup->conf.group)) {
				yyerror("group name \"%s\" too long: max %u",
				    $2, sizeof(curgroup->conf.group) - 1);
				YYERROR;
			}
		}
		;

peeropts	: REMOTEAS number	{
			if (check_state_peeropts())
				YYERROR;
			curpeer->conf.remote_as = $2;
		}
		| DESCR string		{
			if (check_state_peeropts())
				YYERROR;
			if (strlcpy(curpeer->conf.descr, $2,
			    sizeof(curpeer->conf.descr)) >
			    sizeof(curpeer->conf.descr)) {
				yyerror("descr \"%s\" too long: max %u",
				    $2, sizeof(curpeer->conf.descr) - 1);
				YYERROR;
			}
			free($2);
		}
		| LOCALADDR address	{
			if (check_state_peeropts())
				YYERROR;
			curpeer->conf.local_addr.sin_len =
			    sizeof(curpeer->conf.local_addr);
			curpeer->conf.local_addr.sin_family = AF_INET;
			curpeer->conf.local_addr.sin_addr.s_addr = $2.s_addr;
		}
		| MULTIHOP number	{
			if (check_state_peeropts())
				YYERROR;
			if ($2 < 2 || $2 > 255) {
				yyerror("invalid multihop distance %d", $2);
				YYERROR;
			}
			curpeer->conf.distance = $2;
		}
		;

endsection	: '}'			{
			switch (pstate) {
			case PSTATE_GROUP:
				free(curgroup);
				curgroup = NULL;
				pstate = PSTATE_MAIN;
				break;
			case PSTATE_NEIGHBOR:
				curpeer->next = conf->peers;
				conf->peers = curpeer;
				curpeer = NULL;
				if (curgroup == NULL)
					pstate = PSTATE_MAIN;
				else
					pstate = PSTATE_GROUP;
				break;
			default:
				yyerror("\"}\" not allowed in this context");
				YYERROR;
			}
		}
		;
%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;
	char		*nfmt;

	errors = 1;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", infile, yylval.lineno, fmt) == -1)
		fatal("yyerror asprintf", 0);
	vlog(LOG_CRIT, nfmt, ap);
	va_end(ap);
	return (0);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "AS",			AS},
		{ "bgpid",		BGPID},
		{ "descr",		DESCR},
		{ "group",		GROUP},
		{ "holdtime",		HOLDTIME},
		{ "holdtime_min",	HOLDTIME_MIN},
		{ "local-address",	LOCALADDR},
		{ "mrtdump",		MRTDUMP},
		{ "multihop",		MULTIHOP},
		{ "neighbor",		NEIGHBOR},
		{ "remote-as",		REMOTEAS},
		{ "set",		SET},
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p) {
		if (pdebug > 1)
			fprintf(stderr, "%s: %d\n", s, p->k_val);
		return (p->k_val);
	} else {
		if (pdebug > 1)
			fprintf(stderr, "string: %s\n", s);
		return (STRING);
	}
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(FILE *f)
{
	int	c, next;

	if (parsebuf) {
		/* Read character from the parsebuffer instead of input. */
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

	if (pushback_index)
		return (pushback_buffer[--pushback_index]);

	while ((c = getc(f)) == '\\') {
		next = getc(f);
		if (next != '\n') {
			if (isspace(next))
				yyerror("whitespace after \\");
			ungetc(next, f);
			break;
		}
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(f);
		} while (c == '\t' || c == ' ');
		ungetc(c, f);
		c = ' ';
	}

	return (c);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
findeol(void)
{
	int	c;

	parsebuf = NULL;
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
		c = lgetc(fin);
		if (c == '\n') {
			lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
yylex(void)
{
	char	 buf[8096];
	char	*p, *val;
	int	 endc, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(fin)) == ' ')
		; /* nothing */

	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(fin)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);

			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			if (isalnum(c) || c == '_') {
				*p++ = (char)c;
				continue;
			}
			*p = '\0';
			lungetc(c);
			break;
		}
		val = symget(buf);
		if (val == NULL) {
			yyerror("macro '%s' not defined", buf);
			return (findeol());
		}
		parsebuf = val;
		parseindex = 0;
		goto top;
	}

	switch (c) {
	case '\'':
	case '"':
		endc = c;
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);
			if (c == endc) {
				*p = '\0';
				break;
			}
			if (c == '\n') {
				lineno++;
				continue;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			fatal("yylex: strdup", errno);
		return (STRING);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		token = lookup(buf);
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			fatal("yylex: strdup", errno);
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

int
parse_config(char *filename, struct bgpd_config *xconf,
    struct mrt_config *xmconf)
{
	struct sym	*sym;

	if ((conf = calloc(1, sizeof(struct bgpd_config))) == NULL)
		fatal(NULL, errno);
	if ((mrtconf = calloc(1, sizeof(struct mrt_config))) == NULL)
		fatal(NULL, errno);
	LIST_INIT(mrtconf);

	curpeer = NULL;
	curgroup = NULL;
	lineno = 1;
	errors = 0;

	if (strcmp(filename, "-") == 0) {
		fin = stdin;
		infile = "stdin";
	} else {
		if ((fin = fopen(filename, "r")) == NULL) {
			warn("%s", filename);
			return (1);
		}
		infile = filename;
	}

	yyparse();

	/* Free macros and check which have not been used. */
	TAILQ_FOREACH(sym, &symhead, entries) {
		if ((conf->opts & BGPD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		free(sym->nam);
		free(sym->val);
		TAILQ_REMOVE(&symhead, sym, entries);
	}

	errors += merge_config(xconf, conf);
	errors += mrt_mergeconfig(xmconf, mrtconf);

	return (errors ? -1 : 0);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entries))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}
	if ((sym = calloc(1, sizeof(*sym))) == NULL)
		return (-1);

	sym->nam = strdup(nam);
	if (sym->nam == NULL) {
		free(sym);
		return (-1);
	}
	sym->val = strdup(val);
	if (sym->val == NULL) {
		free(sym->nam);
		free(sym);
		return (-1);
	}
	sym->used = 0;
	sym->persist = persist;
	TAILQ_INSERT_TAIL(&symhead, sym, entries);
	return (0);
}

int
cmdline_symset(char *s)
{
	char	*sym, *val;
	int	 ret;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	if ((sym = malloc(strlen(s) - strlen(val) + 1)) == NULL)
		fatal("cmdline_symset: malloc", errno);

	strlcpy(sym, s, strlen(s) - strlen(val) + 1);

	ret = symset(sym, val + 1, 1);
	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entries)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

int
atoul(char *s, u_long *ulvalp)
{
	u_long	 ulval;
	char	*ep;

	errno = 0;
	ulval = strtoul(s, &ep, 0);
	if (s[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && ulval == ULONG_MAX)
		return (-1);
	*ulvalp = ulval;
	return (0);
}

struct peer *
new_peer(void)
{
	struct peer	*p;

	if ((p = calloc(1, sizeof(struct peer))) == NULL)
		fatal("new_peer", errno);

	if (curgroup != NULL) {
		memcpy(p, curgroup, sizeof(struct peer));
		if (strlcpy(p->conf.group, curgroup->conf.group,
		    sizeof(p->conf.group)) > sizeof(p->conf.group))
			fatal("new_peer strlcpy", 0);
		if (strlcpy(p->conf.descr, curgroup->conf.descr,
		    sizeof(p->conf.descr)) > sizeof(p->conf.descr))
			fatal("new_peer strlcpy", 0);
	}
	p->state = STATE_NONE;
	p->next = NULL;
	p->conf.distance = 1;
	p->conf.local_addr.sin_len = sizeof(p->conf.local_addr);
	p->conf.local_addr.sin_family = AF_INET;

	return (p);
}

struct peer *
new_group(void)
{
	struct peer	*p;

	if ((p = calloc(1, sizeof(struct peer))) == NULL)
		fatal("new_group", errno);

	return (p);
}

int
add_mrtconfig(enum mrtdump_type type, char *name, time_t timeout)
{
	struct mrtdump_config	*m, *n;

	LIST_FOREACH(m, mrtconf, list) {
		if (m->type == type) {
			yyerror("only one mrtdump per type allowed.");
			return (-1);
		}
	}

	if ((n = calloc(1, sizeof(struct mrtdump_config))) == NULL)
		fatal("add_mrtconfig", errno);

	n->type = MRT_TABLE_DUMP;
	n->fd = -1;
	if (strlcpy(n->name, name, sizeof(n->name)) > sizeof(n->name)) {
		yyerror("filename \"%s\" too long: max %u",
		    name, sizeof(n->name) - 1);
		return (-1);
	}
	n->ReopenTimerInterval = timeout;

	LIST_INSERT_HEAD(mrtconf, n, list);

	return (0);
}

int
check_state_peeropts(void)
{
	if (pstate != PSTATE_GROUP &&
	    pstate != PSTATE_NEIGHBOR) {
		yyerror("statement not allowed in this context");
		return (-1);
	} else
		return (0);
}

int
check_state_main(void)
{
	if (pstate != PSTATE_MAIN) {
		yyerror("This statement is only valid outside "
		    "group or neighbor definitions");
		return (-1);
	} else
		return (0);
}

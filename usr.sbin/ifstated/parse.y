/*	$OpenBSD: parse.y,v 1.7 2004/04/28 01:00:50 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Ryan McBride <mcbride@openbsd.org>
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <event.h>

#include "ifstated.h"

static struct ifsd_config	*conf;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
static int			 pdebug = 1;
char				*infile;
char				*start_state;

struct ifsd_action		*curaction;
struct ifsd_state		*curstate = NULL;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);


TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

void			 link_states(struct ifsd_action *);
int			 symset(const char *, const char *, int);
char			*symget(const char *);
int			 atoul(char *, u_long *);
void			 set_expression_depth(struct ifsd_expression *, int);
void			 init_state(struct ifsd_state *);
struct ifsd_ifstate	*new_ifstate(u_short, int);
struct ifsd_external	*new_external(char *, u_int32_t);

typedef struct {
	union {
		u_int32_t	 number;
		char		*string;
		struct in_addr	 addr;
		u_short		 interface;

		struct ifsd_expression	*expression;
		struct ifsd_ifstate	*ifstate;
		struct ifsd_external	*external;

	} v;
	int lineno;
} YYSTYPE;

%}

%token	STATE INITSTATE
%token	LINK UP DOWN UNKNOWN ADDED REMOVED
%token	IF RUN SETSTATE EVERY INIT LOGLEVEL
%left	AND OR
%left	UNARY
%token	ERROR
%token	<v.string>	STRING
%type	<v.number>	number
%type	<v.string>	string
%type	<v.interface>	interface
%type	<v.ifstate>	if_test
%type	<v.external>	ext_test
%type	<v.expression>	expr term
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar action '\n'
		| grammar state '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				free($1);
				YYERROR;
			} else
				$$ = ulval;
			free($1);
		}
		;

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1) {
				free($1);
				free($2);
				yyerror("string: asprintf");
				YYERROR;
			}
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string		{
			if (conf->opts & IFSD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1) {
				free($1);
				free($3);
				yyerror("cannot store variable");
				YYERROR;
			}
			free($1);
			free($3);
		}
		;

conf_main	: INITSTATE STRING		{
			start_state = $2;
		}
		| LOGLEVEL STRING			{
			if (!strcmp($2, "none"))
				conf->loglevel = IFSD_LOG_NONE;
			else if (!strcmp($2, "quiet"))
				conf->loglevel = IFSD_LOG_QUIET;
			else if (!strcmp($2, "normal"))
				conf->loglevel = IFSD_LOG_NORMAL;
			else if (!strcmp($2, "verbose"))
				conf->loglevel = IFSD_LOG_VERBOSE;
			else if (!strcmp($2, "debug"))
				conf->loglevel = IFSD_LOG_DEBUG;
			free($2);
		}
		;

interface	: STRING		{
			if (($$ = if_nametoindex($1)) == 0) {
				yyerror("unknown interface %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

action		: RUN STRING		{
			struct ifsd_action *action;

			if ((action = calloc(1, sizeof(*action))) == NULL)
				err(1, "action: calloc");
			action->type = IFSD_ACTION_COMMAND;
			action->act.command = $2;
			if (action->act.command == NULL)
				err(1, "action: strdup");
			TAILQ_INSERT_TAIL(&curaction->act.c.actions,
			    action, entries);
		}
		| SETSTATE STRING	{
			struct ifsd_action *action;

			if (curstate == NULL) {
				free($2);
				yyerror("setstate must be used inside 'if'");
				YYERROR;
			}
			if ((action = calloc(1, sizeof(*action))) == NULL)
				err(1, "action: calloc");
			action->type = IFSD_ACTION_CHANGESTATE;
			action->act.statename = $2;
			TAILQ_INSERT_TAIL(&curaction->act.c.actions,
			    action, entries);
		}
		| IF {
			struct ifsd_action *action;

			if ((action = calloc(1, sizeof(*action))) == NULL)
				err(1, "action: calloc");
			action->type = IFSD_ACTION_CONDITION;
			TAILQ_INIT(&action->act.c.actions);
			TAILQ_INSERT_TAIL(&curaction->act.c.actions,
			    action, entries);
			action->parent = curaction;
			curaction = action;
		} expr optnl '{' optnl action_l '}' {
			set_expression_depth(curaction->act.c.expression, 0);
			curaction = curaction->parent;
		}
		;

action_l	: action_l action nl
		| action nl
		;

init		: INIT {
			if (curstate != NULL)
				curaction = curstate->init;
			else
				curaction = conf->always.init;
		} optnl '{' optnl action_l '}' {
			if (curstate != NULL)
				curaction = curstate->always;
			else
				curaction = conf->always.always;
		}
		;

if_test		: interface LINK UP		{
			$$ = new_ifstate($1, IFSD_LINKUP);
		}
		| interface LINK DOWN		{
			$$ = new_ifstate($1, IFSD_LINKDOWN);
		}
		| interface LINK UNKNOWN	{
			$$ = new_ifstate($1, IFSD_LINKUNKNOWN);
		}
		;

ext_test	: STRING EVERY number {
			$$ = new_external($1, $3);
			free($1);
		}
		;

term		: if_test {
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				errx(1, "term: calloc");
			curaction->act.c.expression = $$;
			$$->type = IFSD_OPER_IFSTATE;
			$$->u.ifstate = $1;
			TAILQ_INSERT_TAIL(&$1->expressions, $$, entries);
		}
		| ext_test {
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				errx(1, "term: calloc");
			curaction->act.c.expression = $$;
			$$->type = IFSD_OPER_EXTERNAL;
			$$->u.external = $1;
			TAILQ_INSERT_TAIL(&$1->expressions, $$, entries);
		}
		| '(' expr ')'			{
			$$ = $2;
		}
		;

expr		: '!' expr %prec UNARY			{
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				errx(1, "expr: calloc");
			curaction->act.c.expression = $$;
			$$->type = IFSD_OPER_NOT;
			$2->parent = $$;
			$$->right = $2;
		}
		| expr AND expr			{
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				errx(1, "expr: calloc");
			curaction->act.c.expression = $$;
			$$->type = IFSD_OPER_AND;
			$1->parent = $$;
			$3->parent = $$;
			$$->left = $1;
			$$->right = $3;
		}
		| expr OR expr			{
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				errx(1, "expr: calloc");
			curaction->act.c.expression = $$;
			$$->type = IFSD_OPER_OR;
			$1->parent = $$;
			$3->parent = $$;
			$$->left = $1;
			$$->right = $3;
		}
		| term
		;

state		: STATE string {
			struct ifsd_state *state = NULL;

			TAILQ_FOREACH(state, &conf->states, entries)
				if (!strcmp(state->name, $2)) {
					yyerror("state %s already exists", $2);
					free($2);
					YYERROR;
				}
			if ((state = calloc(1, sizeof(*curstate))) == NULL)
				errx(1, "state: calloc");
			init_state(state);
			state->name = $2;
			curstate = state;
			curaction = state->always;
		} optnl '{' optnl stateopts_l '}' {
			TAILQ_INSERT_TAIL(&conf->states, curstate, entries);
			curstate = NULL;
			curaction = conf->always.always;
		}
		;

stateopts_l	: stateopts_l stateoptsl
		| stateoptsl
		;

stateoptsl	: init nl
		| action nl
		;

%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list	ap;

	errors = 1;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", infile, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
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
		{ "added",		ADDED},
		{ "and",		AND},
		{ "down",		DOWN},
		{ "every",		EVERY},
		{ "if",			IF},
		{ "init",		INIT},
		{ "init-state",		INITSTATE},
		{ "link",		LINK},
		{ "loglevel",		LOGLEVEL},
		{ "or",			OR},
		{ "removed",		REMOVED},
		{ "run",		RUN},
		{ "set-state",		SETSTATE},
		{ "state",		STATE},
		{ "unknown",		UNKNOWN},
		{ "up",			UP}
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
			errx(1, "yylex: strdup");
		return (STRING);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && \
	x != '!' && x != '=' && x != '#' && \
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
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "yylex: strdup");
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

struct ifsd_config *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;
	struct ifsd_state *state;

	if ((conf = calloc(1, sizeof(struct ifsd_config))) == NULL) {
		errx(1, "parse_config calloc");
		return (NULL);
	}

	if ((fin = fopen(filename, "r")) == NULL) {
		warn("%s", filename);
		free(conf);
		return (NULL);
	}
	infile = filename;

	TAILQ_INIT(&conf->states);

	init_state(&conf->always);
	curaction = conf->always.always;
	conf->loglevel = IFSD_LOG_NORMAL;
	conf->opts = opts;

	yyparse();

	fclose(fin);

	/* Link states */
	TAILQ_FOREACH(state, &conf->states, entries) {
		link_states(state->init);
		link_states(state->always);
	}

	if (start_state != NULL) {
		TAILQ_FOREACH(state, &conf->states, entries) {
			if (strcmp(start_state, state->name) == 0) {
				conf->curstate = state;
				break;
			}
		}
		if (conf->curstate == NULL)
			errx(1, "invalid start state %s", start_state);
	} else {
		conf->curstate = TAILQ_FIRST(&conf->states);
	}

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entries);
		if ((conf->opts & IFSD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}

	if (errors) {
		clear_config(conf);
		return (NULL);
	}

	return (conf);
}

void
link_states(struct ifsd_action *action)
{
	struct ifsd_action *subaction;

	switch (action->type) {
	default:
	case IFSD_ACTION_COMMAND:
		break;
	case IFSD_ACTION_CHANGESTATE: {
		struct ifsd_state *state;

		TAILQ_FOREACH(state, &conf->states, entries) {
			if (strcmp(action->act.statename,
			    state->name) == 0) {
				action->act.nextstate = state;
				break;
			}
		}
		break;
	}
	case IFSD_ACTION_CONDITION:
		TAILQ_FOREACH(subaction, &action->act.c.actions, entries)
			link_states(subaction);
		break;
	}
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
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		errx(1, "cmdline_symset: malloc");

	strlcpy(sym, s, len);

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

void
set_expression_depth(struct ifsd_expression *expression, int depth)
{
	expression->depth = depth;
	if (conf->maxdepth < depth)
		conf->maxdepth = depth;
	if (expression->left != NULL)
		set_expression_depth(expression->left, depth + 1);
	if (expression->right != NULL)
		set_expression_depth(expression->right, depth + 1);
}

void
init_state(struct ifsd_state *state)
{
	TAILQ_INIT(&state->interface_states);
	TAILQ_INIT(&state->external_tests);

	if ((state->init = calloc(1, sizeof(*state->init))) == NULL)
		err(1, "init_state: calloc");
	state->init->type = IFSD_ACTION_CONDITION;
	TAILQ_INIT(&state->init->act.c.actions);

	if ((state->always = calloc(1, sizeof(*state->always))) == NULL)
		err(1, "init_state: calloc");
	state->always->type = IFSD_ACTION_CONDITION;
	TAILQ_INIT(&state->always->act.c.actions);
}

struct ifsd_ifstate *
new_ifstate(u_short ifindex, int s)
{
	struct ifsd_ifstate *ifstate = NULL;
	struct ifsd_state *state;

	if (curstate != NULL)
		state = curstate;
	else
		state = &conf->always;

	TAILQ_FOREACH(ifstate, &state->interface_states, entries)
		if (ifstate->ifindex == ifindex && ifstate->ifstate == s)
			break;
	if (ifstate == NULL) {
		if ((ifstate = calloc(1, sizeof(*ifstate))) == NULL)
			errx(1, "new_ifstate: calloc");
		ifstate->ifindex = ifindex;
		ifstate->ifstate = s;
		TAILQ_INIT(&ifstate->expressions);
		TAILQ_INSERT_TAIL(&state->interface_states, ifstate, entries);
	}
	ifstate->prevstate = -1;
	ifstate->refcount++;
	return (ifstate);
}

struct ifsd_external *
new_external(char *command, u_int32_t frequency)
{
	struct ifsd_external *external = NULL;
	struct ifsd_state *state;

	if (curstate != NULL)
		state = curstate;
	else
		state = &conf->always;

	TAILQ_FOREACH(external, &state->external_tests, entries)
		if (strcmp(external->command, command) == 0 &&
		    external->frequency == frequency)
			break;
	if (external == NULL) {
		if ((external = calloc(1, sizeof(*external))) == NULL)
			errx(1, "new_external: calloc");
		if ((external->command = strdup(command)) == NULL)
			errx(1, "new_external: strdup");
		external->frequency = frequency;
		TAILQ_INIT(&external->expressions);
		TAILQ_INSERT_TAIL(&state->external_tests, external, entries);
	}
	external->prevstatus = -1;
	external->refcount++;
	return (external);
}

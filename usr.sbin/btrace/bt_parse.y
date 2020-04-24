/*	$OpenBSD: bt_parse.y,v 1.13 2020/04/24 15:10:41 mpi Exp $	*/

/*
 * Copyright (c) 2019 - 2020 Martin Pieuchot <mpi@openbsd.org>
 * Copyright (c) 2019 Tobias Heider <tobhe@openbsd.org>
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
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

/*
 * B tracing language parser.
 *
 * The dialect of the language understood by this parser aims to be
 * compatible with the one understood bpftrace(8), see:
 *
 * https://github.com/iovisor/bpftrace/blob/master/docs/reference_guide.md
 *
 */

%{
#include <sys/queue.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "bt_parser.h"

/* Name for the default map @[], hopefully nobody will use this one ;) */
#define UNNAMED_MAP	"___unnamed_map_doesnt_have_any_name"

/* Number of rules to evaluate. */
struct bt_ruleq		g_rules = TAILQ_HEAD_INITIALIZER(g_rules);

/* Number of probes except BEGIN/END. */
int		 	g_nprobes;

/* List of global variables, including maps. */
SLIST_HEAD(, bt_var)	 g_variables;

struct bt_rule	*br_new(struct bt_probe *, struct bt_filter *, struct bt_stmt *,
		     enum bt_rtype);
struct bt_filter *bf_new(enum bt_operand, enum bt_filtervar, int);
struct bt_probe	*bp_new(const char *, const char *, const char *, int32_t);
struct bt_arg	*ba_append(struct bt_arg *, struct bt_arg *);
struct bt_stmt	*bs_new(enum bt_action, struct bt_arg *, struct bt_var *);
struct bt_stmt	*bs_append(struct bt_stmt *, struct bt_stmt *);

struct bt_var	*bv_find(const char *);
struct bt_arg	*bv_get(const char *);
struct bt_stmt	*bv_set(const char *, struct bt_arg *);

struct bt_arg	*bm_get(const char *, struct bt_arg *);
struct bt_stmt	*bm_set(const char *, struct bt_arg *, struct bt_arg *);
struct bt_stmt	*bm_op(enum bt_action, struct bt_arg *, struct bt_arg *);

/*
 * Lexer
 */
const char	*pbuf;
size_t		 plen;
size_t		 pindex;
int		 perrors = 0;

typedef struct {
	union {
		long			 number;
		int			 i;
		const char		*string;
		struct bt_probe		*probe;
		struct bt_filter	*filter;
		struct bt_stmt		*stmt;
		struct bt_arg		*arg;
		enum bt_rtype		 rtype;
	} v;
	const char			*filename;
	int				 lineno;
	int				 colno;
} yystype;
#define YYSTYPE yystype

static void	 yyerror(const char *, ...);
static int	 yylex(void);
%}

%token	ERROR OP_EQ OP_NEQ BEGIN END
/* Builtins */
%token	BUILTIN PID TID
/* Functions and Map operators */
%token  F_DELETE FUNC0 FUNC1 FUNCN MOP0 MOP1
%token	<v.string>	STRING CSTRING
%token	<v.number>	NUMBER

%type	<v.string>	gvar
%type	<v.i>		filterval oper builtin
%type	<v.i>		BUILTIN F_DELETE FUNC0 FUNC1 FUNCN MOP0 MOP1
%type	<v.probe>	probe
%type	<v.filter>	predicate
%type	<v.stmt>	action stmt stmtlist
%type	<v.arg>		expr vargs map mexpr term
%type	<v.rtype>	beginend

%left	'+' '-'
%left	'/' '*'
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar rule
		| grammar error
		;

rule		: beginend action	 { br_new(NULL, NULL, $2, $1); }
		| probe predicate action { br_new($1, $2, $3, B_RT_PROBE); }
		;

beginend	: BEGIN				{ $$ = B_RT_BEGIN; }
		| END				{ $$ = B_RT_END; }
		;

probe		: STRING ':' STRING ':' STRING	{ $$ = bp_new($1, $3, $5, 0); }
		| STRING ':' HZ ':' NUMBER	{ $$ = bp_new($1, "hz", NULL, $5); }
		;


filterval	: PID				{ $$ = B_FV_PID; }
		| TID				{ $$ = B_FV_TID; }
		;

oper		: OP_EQ				{ $$ = B_OP_EQ; }
		| OP_NEQ			{ $$ = B_OP_NE; }
		;

predicate	: /* empty */			{ $$ = NULL; }
		| '/' filterval oper NUMBER '/' { $$ = bf_new($3, $2, $4); }
		| '/' NUMBER oper filterval '/' { $$ = bf_new($3, $4, $2); }
		;

builtin		: PID 				{ $$ = B_AT_BI_PID; }
		| TID 				{ $$ = B_AT_BI_TID; }
		| BUILTIN			{ $$ = $1; }
		;

mexpr		: MOP0 '(' ')'			{ $$ = ba_new(NULL, $1); }
		| MOP1 '(' expr ')'		{ $$ = ba_new($3, $1); }
		| expr				{ $$ = $1; }
		;

expr		: CSTRING			{ $$ = ba_new($1, B_AT_STR); }
		| term
		;

term		: '(' term ')'			{ $$ = $2; }
		| term '+' term			{ $$ = ba_op('+', $1, $3); }
		| term '-' term			{ $$ = ba_op('-', $1, $3); }
		| term '/' term			{ $$ = ba_op('/', $1, $3); }
		| term '*' term			{ $$ = ba_op('*', $1, $3); }
		| NUMBER			{ $$ = ba_new($1, B_AT_LONG); }
		| builtin			{ $$ = ba_new(NULL, $1); }
		| gvar				{ $$ = bv_get($1); }
		| map				{ $$ = $1; }


gvar		: '@' STRING			{ $$ = $2; }
		| '@'				{ $$ = UNNAMED_MAP; }

map		: gvar '[' vargs ']'		{ $$ = bm_get($1, $3); }
		;

vargs		: expr
		| vargs ',' expr		{ $$ = ba_append($1, $3); }
		;

NL		: /* empty */ | '\n'
		;

stmt		: ';' NL			{ $$ = NULL; }
		| gvar '=' expr			{ $$ = bv_set($1, $3); }
		| gvar '[' vargs ']' '=' mexpr	{ $$ = bm_set($1, $3, $6); }
		| FUNCN '(' vargs ')'		{ $$ = bs_new($1, $3, NULL); }
		| FUNC1 '(' expr ')'		{ $$ = bs_new($1, $3, NULL); }
		| FUNC0 '(' ')'			{ $$ = bs_new($1, NULL, NULL); }
		| F_DELETE '(' map ')'		{ $$ = bm_op($1, $3, NULL); }
		;

stmtlist	: stmt
		| stmtlist stmt			{ $$ = bs_append($1, $2); }
		;

action		: '{' stmtlist '}'		{ $$ = $2; }
		;

%%

/* Create a new rule, representing  "probe / filter / { action }" */
struct bt_rule *
br_new(struct bt_probe *probe, struct bt_filter *filter, struct bt_stmt *head,
    enum bt_rtype rtype)
{
	struct bt_rule *br;

	br = calloc(1, sizeof(struct bt_rule));
	if (br == NULL)
		err(1, "bt_rule: calloc");
	br->br_probe = probe;
	br->br_filter = filter;
	/* SLIST_INSERT_HEAD() nullify the next pointer. */
	SLIST_FIRST(&br->br_action) = head;
	br->br_type = rtype;

	if (rtype == B_RT_PROBE) {
		g_nprobes++;
		TAILQ_INSERT_TAIL(&g_rules, br, br_next);
	} else {
		TAILQ_INSERT_HEAD(&g_rules, br, br_next);
	}

	return br;
}

/* Create a new filter */
struct bt_filter *
bf_new(enum bt_operand op, enum bt_filtervar var, int val)
{
	struct bt_filter *df;

	if (val < 0 || val > INT_MAX)
		errx(1, "invalid pid '%d'", val);

	df = calloc(1, sizeof(struct bt_filter));
	if (df == NULL)
		err(1, "bt_filter: calloc");
	df->bf_op = op;
	df->bf_var = var;
	df->bf_val = val;

	return df;
}

/* Create a new probe */
struct bt_probe *
bp_new(const char *prov, const char *func, const char *name, int32_t rate)
{
	struct bt_probe *bp;

	if (rate < 0 || rate > INT32_MAX)
		errx(1, "only positive values permitted");

	bp = calloc(1, sizeof(struct bt_probe));
	if (bp == NULL)
		err(1, "bt_probe: calloc");
	bp->bp_prov = prov;
	bp->bp_func = func;
	bp->bp_name = name;
	bp->bp_rate = rate;

	return bp;
}

/* Create a new argument */
struct bt_arg *
ba_new0(void *val, enum bt_argtype type)
{
	struct bt_arg *ba;

	ba = calloc(1, sizeof(struct bt_arg));
	if (ba == NULL)
		err(1, "bt_arg: calloc");
	ba->ba_value = val;
	ba->ba_type = type;

	return ba;
}

/*
 * Link two arguments together, to build an argument list used in
 * function calls.
 */
struct bt_arg *
ba_append(struct bt_arg *da0, struct bt_arg *da1)
{
	struct bt_arg *ba = da0;

	assert(da1 != NULL);

	if (da0 == NULL)
		return da1;

	while (SLIST_NEXT(ba, ba_next) != NULL)
		ba = SLIST_NEXT(ba, ba_next);

	SLIST_INSERT_AFTER(ba, da1, ba_next);

	return da0;
}

/* Create an operator argument */
struct bt_arg *
ba_op(const char op, struct bt_arg *da0, struct bt_arg *da1)
{
	enum bt_argtype type;

	switch (op) {
	case '+':
		type = B_AT_OP_ADD;
		break;
	case '-':
		type = B_AT_OP_MINUS;
		break;
	case '*':
		type = B_AT_OP_MULT;
		break;
	case '/':
		type = B_AT_OP_DIVIDE;
		break;
	default:
		assert(0);
	}

	return ba_new(ba_append(da0, da1), type);
}

/* Create a new statement: function call or assignment. */
struct bt_stmt *
bs_new(enum bt_action act, struct bt_arg *head, struct bt_var *var)
{
	struct bt_stmt *bs;

	bs = calloc(1, sizeof(struct bt_stmt));
	if (bs == NULL)
		err(1, "bt_stmt: calloc");
	bs->bs_act = act;
	bs->bs_var = var;
	/* SLIST_INSERT_HEAD() nullify the next pointer. */
	SLIST_FIRST(&bs->bs_args) = head;

	return bs;
}

/* Link two statements together, to build an 'action'. */
struct bt_stmt *
bs_append(struct bt_stmt *ds0, struct bt_stmt *ds1)
{
	struct bt_stmt *bs = ds0;

	if (ds0 == NULL)
		return ds1;

	if (ds1 == NULL)
		return ds0;

	while (SLIST_NEXT(bs, bs_next) != NULL)
		bs = SLIST_NEXT(bs, bs_next);

	SLIST_INSERT_AFTER(bs, ds1, bs_next);

	return ds0;
}

const char *
bv_name(struct bt_var *bv)
{
	if (strncmp(bv->bv_name, UNNAMED_MAP, strlen(UNNAMED_MAP)) == 0)
		return "";
	return bv->bv_name;
}

/* Return the global variable corresponding to `vname'. */
struct bt_var *
bv_find(const char *vname)
{
	struct bt_var *bv;

	SLIST_FOREACH(bv, &g_variables, bv_next) {
		if (strcmp(vname, bv->bv_name) == 0)
			break;
	}

	return bv;
}

/* Find or allocate a global variable. */
struct bt_var *
bv_new(const char *vname)
{
	struct bt_var *bv;

	bv = calloc(1, sizeof(struct bt_var));
	if (bv == NULL)
		err(1, "bt_var: calloc");
	bv->bv_name = vname;
	SLIST_INSERT_HEAD(&g_variables, bv, bv_next);

	return bv;
}

/* Create a 'variable store' statement to assign a value to a variable. */
struct bt_stmt *
bv_set(const char *vname, struct bt_arg *vval)
{
	struct bt_var *bv;

	bv = bv_find(vname);
	if (bv == NULL)
		bv = bv_new(vname);
	return bs_new(B_AC_STORE, vval, bv);
}

/* Create an argument that points to a variable. */
struct bt_arg *
bv_get(const char *vname)
{
	struct bt_var *bv;

	bv = bv_find(vname);
	if (bv == NULL)
		yyerror("variable '%s' accessed before being set", vname);

	return ba_new(bv, B_AT_VAR);
}

struct bt_stmt *
bm_op(enum bt_action mact, struct bt_arg *ba, struct bt_arg *mval)
{
	return bs_new(mact, ba, (struct bt_var *)mval);
}

/* Create a 'map store' statement to assign a value to a map entry. */
struct bt_stmt *
bm_set(const char *mname, struct bt_arg *mkey, struct bt_arg *mval)
{
	struct bt_arg *ba;
	struct bt_var *bv;

	bv = bv_find(mname);
	if (bv == NULL)
		bv = bv_new(mname);
	ba = ba_new(bv, B_AT_MAP);
	ba->ba_key = mkey;
	return bs_new(B_AC_INSERT, ba, (struct bt_var *)mval);
}

/* Create an argument that points to a variable and attach a key to it. */
struct bt_arg *
bm_get(const char *mname, struct bt_arg *mkey)
{
	struct bt_arg *ba;

	ba = bv_get(mname);
	ba->ba_type = B_AT_MAP;
	ba->ba_key = mkey;
	return ba;
}

struct keyword {
	const char	*word;
	int		 token;
	int		 type;
};

int
kw_cmp(const void *str, const void *xkw)
{
	return (strcmp(str, ((const struct keyword *)xkw)->word));
}

struct keyword *
lookup(char *s)
{
	static const struct keyword kws[] = {
		{ "!=",		OP_NEQ,		0 },
		{ "==",		OP_EQ,		0 },
		{ "BEGIN",	BEGIN,		0 },
		{ "END",	END,		0 },
		{ "arg0",	BUILTIN,	B_AT_BI_ARG0 },
		{ "arg1",	BUILTIN,	B_AT_BI_ARG1 },
		{ "arg2",	BUILTIN,	B_AT_BI_ARG2 },
		{ "arg3",	BUILTIN,	B_AT_BI_ARG3 },
		{ "arg4",	BUILTIN,	B_AT_BI_ARG4 },
		{ "arg5",	BUILTIN,	B_AT_BI_ARG5 },
		{ "arg6",	BUILTIN,	B_AT_BI_ARG6 },
		{ "arg7",	BUILTIN,	B_AT_BI_ARG7 },
		{ "arg8",	BUILTIN,	B_AT_BI_ARG8 },
		{ "arg9",	BUILTIN,	B_AT_BI_ARG9 },
		{ "clear",	FUNC1,		B_AC_CLEAR },
		{ "comm",	BUILTIN,	B_AT_BI_COMM },
		{ "count",	MOP0, 		B_AT_MF_COUNT },
		{ "cpu",	BUILTIN,	B_AT_BI_CPU },
		{ "delete",	F_DELETE,	B_AC_DELETE },
		{ "exit",	FUNC0,		B_AC_EXIT },
		{ "hz",		HZ,		0 },
		{ "kstack",	BUILTIN,	B_AT_BI_KSTACK },
		{ "max",	MOP1,		B_AT_MF_MAX },
		{ "min",	MOP1,		B_AT_MF_MIN },
		{ "nsecs",	BUILTIN,	B_AT_BI_NSECS },
		{ "pid",	PID,		0 /*B_AT_BI_PID*/ },
		{ "print",	FUNCN,	B_AC_PRINT },
		{ "printf",	FUNCN,		B_AC_PRINTF },
		{ "retval",	BUILTIN,	B_AT_BI_RETVAL },
		{ "sum",	MOP1,		B_AT_MF_SUM },
		{ "tid",	TID,		0 /*B_AT_BI_TID*/ },
		{ "time",	FUNC1,		B_AC_TIME },
		{ "ustack",	BUILTIN,	B_AT_BI_USTACK },
		{ "zero",	FUNC1,		B_AC_ZERO },
	};

	return bsearch(s, kws, nitems(kws), sizeof(kws[0]), kw_cmp);
}

int
peek(void)
{
	if (pbuf != NULL) {
		if (pindex < plen)
			return pbuf[pindex];
	}
	return EOF;
}

int
lgetc(void)
{
	if (pbuf != NULL) {
		if (pindex < plen) {
			yylval.colno++;
			return pbuf[pindex++];
		}
	}
	return EOF;
}

void
lungetc(void)
{
	if (pbuf != NULL && pindex > 0) {
		yylval.colno--;
		pindex--;
	}
}

int
yylex(void)
{
	unsigned char	 buf[1024];
	unsigned char	*ebuf, *p, *str;
	int		 c;

	ebuf = buf + sizeof(buf);
	p = buf;

again:
	/* skip whitespaces */
	for (c = lgetc(); isspace(c); c = lgetc()) {
		if (c == '\n') {
			yylval.lineno++;
			yylval.colno = 0;
		}
	}

	/* skip single line comments and shell magic */
	if ((c == '/' && peek() == '/') ||
	    (yylval.lineno == 1 && yylval.colno == 1 && c == '#' &&
	     peek() == '!')) {
		for (c = lgetc(); c != EOF; c = lgetc()) {
			if (c == '\n') {
				yylval.lineno++;
				yylval.colno = 0;
				goto again;
			}
		}
	}

	/* skip multi line comments */
	if (c == '/' && peek() == '*') {
		int pc;

		for (pc = 0, c = lgetc(); c != EOF; c = lgetc()) {
			if (pc == '*' && c == '/')
				goto again;
			pc = c;
		}
	}

	switch (c) {
	case '=':
		if (peek() == '=')
			break;
	case ',':
	case '(':
	case ')':
	case '{':
	case '}':
	case ':':
	case ';':
	case '/':
		return c;
	case EOF:
		return 0;
	case '"':
		/* parse C-like string */
		while ((c = lgetc()) != EOF && c != '"') {
			if (c == '\\') {
				c = lgetc();
				switch (c) {
				case '\\':	c = '\\';	break;
				case '\'':	c = '\'';	break;
				case '"':	c = '"';	break;
				case 'a':	c = '\a';	break;
				case 'b':	c = '\b';	break;
				case 'e':	c = 033;	break;
				case 'f':	c = '\f';	break;
				case 'n':	c = '\n';	break;
				case 'r':	c = '\r';	break;
				case 't':	c = '\t';	break;
				case 'v':	c = '\v';	break;
				default:
					yyerror("'%c' unsuported escape", c);
					return ERROR;
				}
			}
			*p++ = c;
			if (p == ebuf) {
				yyerror("too long line");
				return ERROR;
			}
		}
		if (c == EOF) {
			yyerror("\"%s\" invalid EOF", buf);
			return ERROR;
		}
		*p++ = '\0';
		if ((str = strdup(buf)) == NULL)
			err(1, "%s", __func__);
		yylval.v.string = str;
		return CSTRING;
	default:
		break;
	}

#define allowed_to_end_number(x) \
    (isspace(x) || x == ')' || x == '/' || x == '{' || x == ';' || x == ']' || x == ',')

	/* parsing number */
	if (isdigit(c)) {
		do {
			*p++ = c;
			if (p == ebuf) {
				yyerror("too long line");
				return ERROR;
			}
		} while ((c = lgetc()) != EOF && isdigit(c));
		lungetc();
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LONG_MIN, LONG_MAX,
			    &errstr);
			if (errstr) {
				yyerror("invalid number '%s' (%s)", buf,
				    errstr);
				return ERROR;
			}
			return NUMBER;
		} else {
			while (p > buf + 1) {
				--p;
				lungetc();
			}
			c = *--p;
		}
	}

#define allowed_in_string(x) (isalnum(c) || c == '!' || c == '=' || c == '_')

	/* parsing next word */
	if (allowed_in_string(c)) {
		struct keyword *kwp;
		do {
			*p++ = c;
			if (p == ebuf) {
				yyerror("too long line");
				return ERROR;
			}
		} while ((c = lgetc()) != EOF && (allowed_in_string(c)));
		lungetc();
		*p = '\0';
		kwp = lookup(buf);
		if (kwp == NULL) {
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "%s", __func__);
			return STRING;
		}
		yylval.v.i = kwp->type;
		return kwp->token;
	}

	if (c == '\n') {
		yylval.lineno++;
		yylval.colno = 0;
	}
	if (c == EOF)
		return 0;
	return c;
}

void
pprint_syntax_error(void)
{
	char line[BUFSIZ];
	int c, indent = yylval.colno;
	size_t i;

	strlcpy(line, &pbuf[pindex - yylval.colno], sizeof(line));

	for (i = 0; line[i] != '\0' && (c = line[i]) != '\n'; i++) {
		if (c == '\t')
			indent += (8 - 1);
		fputc(c, stderr);
	}

	fprintf(stderr, "\n%*c\n", indent, '^');
}

void
yyerror(const char *fmt, ...)
{
	const char *prefix;
	va_list	va;

	prefix = (yylval.filename != NULL) ? yylval.filename : getprogname();

	fprintf(stderr, "%s:%d:%d: ", prefix, yylval.lineno, yylval.colno);
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, ":\n");

	pprint_syntax_error();

	perrors++;
}

int
btparse(const char *str, size_t len, const char *filename, int debug)
{
	if (debug > 0)
		yydebug = 1;
	pbuf = str;
	plen = len;
	pindex = 0;
	yylval.filename = filename;
	yylval.lineno = 1;

	yyparse();

	return perrors;
}

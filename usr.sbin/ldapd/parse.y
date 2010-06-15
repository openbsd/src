/*	$OpenBSD: parse.y,v 1.4 2010/06/15 19:30:26 martinh Exp $ */

/*
 * Copyright (c) 2009 Martin Hedenfalk <martin@bzero.se>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "ldapd.h"

static int
attr_oid_cmp(struct attr_type *a, struct attr_type *b)
{
	return strcasecmp(a->oid, b->oid);
}

static int
obj_oid_cmp(struct object *a, struct object *b)
{
	return strcasecmp(a->oid, b->oid);
}

static int
oidname_cmp(struct oidname *a, struct oidname *b)
{
	return strcasecmp(a->on_name, b->on_name);
}

RB_GENERATE(attr_type_tree, attr_type, link, attr_oid_cmp);
RB_GENERATE(object_tree, object, link, obj_oid_cmp);
RB_GENERATE(oidname_tree, oidname, link, oidname_cmp);

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;
struct file	*pushfile(const char *, int);
int		 popfile(void);
int		 check_file_secrecy(int, const char *);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

struct listener *host_unix(const char *path);
struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);
int		 host(const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);
int		 interface(const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entry;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};
int		 symset(const char *, const char *, int);
char		*symget(const char *);

struct ldapd_config	*conf;

static struct attr_list	*append_attr(struct attr_list *alist,
				struct attr_type *a);
static struct obj_list	*append_obj(struct obj_list *olist, struct object *obj);
int			 is_oidstr(const char *oidstr);
static struct name_list *append_name(struct name_list *nl, const char *name);
static struct aci	*mk_aci(int type, int rights, enum scope scope,
				char *target, char *subject);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct attr_type	*attr;
		struct attr_list	*attrlist;
		struct object		*obj;
		struct obj_list		*objlist;
		struct name_list	*namelist;
		struct {
			int		 type;
			void		*data;
			long long	 value;
		}			 data;
		struct aci		*aci;
	} v;
	int lineno;
} YYSTYPE;

static struct attr_type *cattr = NULL;
static struct object *cobj = NULL;
static struct namespace *cns = NULL;

%}

%token	ERROR LISTEN ON TLS LDAPS PORT NAMESPACE ROOTDN ROOTPW INDEX
%token	SUP SYNTAX EQUALITY ORDERING SUBSTR OBSOLETE NAME SINGLE_VALUE
%token	DESC USAGE ATTRIBUTETYPE NO_USER_MOD COLLECTIVE
%token	INCLUDE CERTIFICATE FSYNC CACHE_SIZE INDEX_CACHE_SIZE
%token	OBJECTCLASS MUST MAY SECURE RELAX STRICT SCHEMA
%token	ABSTRACT AUXILIARY STRUCTURAL USE COMPRESSION LEVEL
%token	USER_APPLICATIONS DIRECTORY_OPERATION
%token	DISTRIBUTED_OPERATION DSA_OPERATION
%token	DENY ALLOW READ WRITE BIND ACCESS TO ROOT
%token	ANY CHILDREN OF ATTRIBUTE IN SUBTREE BY SELF
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.string>	numericoid oidlen
%type	<v.string>	qdescr certname
%type	<v.number>	usage kind port ssl boolean
%type	<v.attrlist>	attrptrs attrlist
%type	<v.attr>	attrptr
%type	<v.objlist>	objptrs objlist
%type	<v.obj>		objptr
%type	<v.namelist>	qdescrs qdescrlist
%type	<v.number>	aci_type aci_access aci_rights aci_right aci_scope
%type	<v.string>	aci_target aci_subject
%type	<v.aci>		aci
%type	<v.number>	comp_level

%%

grammar		: /* empty */
		| grammar include
		| grammar attr_type
		| grammar object
		| grammar varset
		| grammar conf_main
		| grammar database
		| grammar aci			{
			SIMPLEQ_INSERT_TAIL(&conf->acl, $2, entry);
		}
		| grammar error			{ file->errors++; }
		;

ssl		: /* empty */			{ $$ = 0; }
		| TLS				{ $$ = F_STARTTLS; }
		| LDAPS				{ $$ = F_LDAPS; }
		| SECURE			{ $$ = F_SECURE; }
		;

certname	: /* empty */			{ $$ = NULL; }
		| CERTIFICATE STRING		{ $$ = $2; }
		;

port		: PORT STRING			{
			struct servent	*servent;

			servent = getservbyname($2, "tcp");
			if (servent == NULL) {
				yyerror("port %s is invalid", $2);
				free($2);
				YYERROR;
			}
			$$ = servent->s_port;
			free($2);
		}
		| PORT NUMBER			{
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %lld", $2);
				YYERROR;
			}
			$$ = htons($2);
		}
		| /* empty */			{
			$$ = 0;
		}
		;

conf_main	: LISTEN ON STRING port ssl certname	{
			char			*cert;

			if ($4 == 0) {
				if ($5 == F_LDAPS)
					$4 = htons(LDAPS_PORT);
				else
					$4 = htons(LDAP_PORT);
			}

			cert = ($6 != NULL) ? $6 : $3;

			if (($5 == F_STARTTLS || $5 == F_LDAPS) &&
			    ssl_load_certfile(conf, cert, F_SCERT) < 0) {
				yyerror("cannot load certificate: %s", cert);
				free($6);
				free($3);
				YYERROR;
			}

			if (! interface($3, cert, &conf->listeners,
				MAX_LISTEN, $4, $5)) {
				if (host($3, cert, &conf->listeners,
					MAX_LISTEN, $4, $5) <= 0) {
					yyerror("invalid virtual ip or interface: %s", $3);
					free($6);
					free($3);
					YYERROR;
				}
			}
			free($6);
			free($3);
		}
		;

database	: NAMESPACE STRING '{'		{
			cns = namespace_new($2);
			free($2);
			TAILQ_INSERT_TAIL(&conf->namespaces, cns, next);
		} ns_opts	'}'		{
			cns = NULL;
		}
		;

boolean		: STRING			{
			if (strcasecmp($1, "true") == 0 ||
			    strcasecmp($1, "yes") == 0)
				$$ = 1;
			else if (strcasecmp($1, "false") == 0 ||
			    strcasecmp($1, "off") == 0 ||
			    strcasecmp($1, "no") == 0)
				$$ = 0;
			else {
				yyerror("invalid boolean value '%s'", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		| ON				{ $$ = 1; }
		;

ns_opts		: /* empty */
		| ns_opts ROOTDN STRING		{
			cns->rootdn = $3;
			normalize_dn(cns->rootdn);
		}
		| ns_opts ROOTPW STRING		{ cns->rootpw = $3; }
		| ns_opts INDEX STRING		{
			struct attr_index	*ai;
			if ((ai = calloc(1, sizeof(*ai))) == NULL) {
				yyerror("calloc");
                                free($3);
				YYERROR;
			}
			ai->attr = $3;
			ai->type = INDEX_EQUAL;
			TAILQ_INSERT_TAIL(&cns->indices, ai, next);
		}
		| ns_opts CACHE_SIZE NUMBER	{
			cns->cache_size = $3;
		}
		| ns_opts INDEX_CACHE_SIZE NUMBER	{
			cns->index_cache_size = $3;
		}
		| ns_opts FSYNC boolean			{
			cns->sync = $3;
		}
		| ns_opts aci				{
			SIMPLEQ_INSERT_TAIL(&cns->acl, $2, entry);
		}
		| ns_opts RELAX SCHEMA			{
			cns->relax = 1;
		}
		| ns_opts STRICT SCHEMA			{
			cns->relax = 0;
		}
		| ns_opts USE COMPRESSION comp_level	{
			cns->compression_level = $4;
		}
		;

comp_level	: /* empty */			{ $$ = 6; }
		| LEVEL NUMBER			{ $$ = $2; }
		;

aci		: aci_type aci_access TO aci_scope aci_target aci_subject {
			if (($$ = mk_aci($1, $2, $4, $5, $6)) == NULL) {
				free($5);
				free($6);
				YYERROR;
			}
		}
		| aci_type aci_access {
			if (($$ = mk_aci($1, $2, LDAP_SCOPE_SUBTREE, NULL,
			    NULL)) == NULL) {
				YYERROR;
			}
		}
		;

aci_type	: DENY				{ $$ = ACI_DENY; }
		| ALLOW				{ $$ = ACI_ALLOW; }
		;

aci_access	: /* empty */			{ $$ = ACI_ALL; };
		| ACCESS			{ $$ = ACI_ALL; };
		| aci_rights ACCESS		{ $$ = $1; };
		;

aci_rights	: aci_right			{ $$ = $1; }
		| aci_rights ',' aci_right	{ $$ = $1 | $3; }
		;

aci_right	: READ				{ $$ = ACI_READ; };
		| WRITE				{ $$ = ACI_WRITE; };
		| BIND				{ $$ = ACI_BIND; };
		;


aci_scope	: /* empty */			{ $$ = LDAP_SCOPE_BASE; }
		| SUBTREE			{ $$ = LDAP_SCOPE_SUBTREE; }
		| CHILDREN OF			{ $$ = LDAP_SCOPE_ONELEVEL; }
		;

aci_target	: ANY				{ $$ = NULL; }
		| ROOT				{ $$ = strdup(""); }
		| STRING			{ $$ = $1; normalize_dn($$); }
		;

aci_subject	: /* empty */			{ $$ = NULL; }
		| BY ANY			{ $$ = NULL; }
		| BY STRING			{ $$ = $2; normalize_dn($$); }
		| BY SELF			{ $$ = strdup("@"); }
		;

attr_type	: ATTRIBUTETYPE '(' numericoid {
			struct attr_type	*p;

			if ((cattr = calloc(1, sizeof(*cattr))) == NULL) {
				yyerror("calloc");
                                free($3);
				YYERROR;
                        }
			cattr->oid = $3;
                        p = RB_INSERT(attr_type_tree, &conf->attr_types, cattr);
			if (p != NULL) {
				yyerror("attribute '%s' already defined", $3);
				free($3);
				free(cattr);
				cattr = NULL;
				YYERROR;
			}
		} attr_data ')' {
			cattr = NULL;
		}
		;

attr_data	: /* empty */
		| attr_data NAME qdescrs	{ cattr->names = $3; }
		| attr_data DESC STRING		{ cattr->desc = $3; }
		| attr_data OBSOLETE		{ cattr->obsolete = 1; }
		| attr_data SUP STRING		{
			cattr->sup = lookup_attribute($3);
			if (cattr->sup == NULL)
				yyerror("%s: no such attribute", $3);
			free($3);
			if (cattr->sup == NULL)
				YYERROR;
		}
		| attr_data EQUALITY STRING	{ cattr->equality = $3; }
		| attr_data ORDERING STRING	{ cattr->ordering = $3; }
		| attr_data SUBSTR STRING	{ cattr->substr = $3; }
		| attr_data SYNTAX oidlen	{ cattr->syntax = $3; }
		| attr_data SINGLE_VALUE	{ cattr->single = 1; }
		| attr_data COLLECTIVE		{ cattr->collective = 1; }
		| attr_data NO_USER_MOD		{ cattr->immutable = 1; }
		| attr_data USAGE usage		{ cattr->usage = $3; }
		;

usage		: USER_APPLICATIONS		{ $$ = USAGE_USER_APP; }
		| DIRECTORY_OPERATION		{ $$ = USAGE_DIR_OP; }
		| DISTRIBUTED_OPERATION		{ $$ = USAGE_DIST_OP; }
		| DSA_OPERATION			{ $$ = USAGE_DSA_OP; }
		;

object		: OBJECTCLASS '(' numericoid {
			struct object		*p;

			if ((cobj = calloc(1, sizeof(*cobj))) == NULL) {
				yyerror("calloc");
                                free($3);
				YYERROR;
                        }
			cobj->oid = $3;
                        p = RB_INSERT(object_tree, &conf->objects, cobj);
			if (p != NULL) {
				yyerror("object '%s' already defined", $3);
				free($3);
				free(cobj);
				cobj = NULL;
				YYERROR;
			}
		} obj_data ')' {
			cobj = NULL;
		}
		;

obj_data	: /* empty */
		| obj_data NAME qdescrs		{ cobj->names = $3; }
		| obj_data DESC STRING		{ cobj->desc = $3; }
		| obj_data OBSOLETE		{ cobj->obsolete = 1; }
		| obj_data SUP objlist		{ cobj->sup = $3; }
		| obj_data kind			{ cobj->kind = $2; }
		| obj_data MUST attrlist	{ cobj->must = $3; }
		| obj_data MAY attrlist		{ cobj->may = $3; }
		;

attrptr		: STRING			{
			$$ = lookup_attribute($1);
			if ($$ == NULL) {
				yyerror("undeclared attribute '%s'", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

attrptrs	: attrptr			{
			if (($$ = append_attr(NULL, $1)) == NULL)
				YYERROR;
		}
		| attrptrs '$' attrptr		{
			if (($$ = append_attr($1, $3)) == NULL)
				YYERROR;
		}
		;

attrlist	: attrptr			{
			if (($$ = append_attr(NULL, $1)) == NULL)
				YYERROR;
		}
		| '(' attrptrs ')'		{ $$ = $2; }
		;

objptr		: STRING			{
			$$ = lookup_object($1);
			if ($$ == NULL) {
				yyerror("undeclared object class '%s'", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

objptrs		: objptr			{
			if (($$ = append_obj(NULL, $1)) == NULL)
				YYERROR;
		}
		| objptrs '$' objptr		{
			if (($$ = append_obj($1, $3)) == NULL)
				YYERROR;
		}
		;

objlist		: objptr			{
			if (($$ = append_obj(NULL, $1)) == NULL)
				YYERROR;
		}
		| '(' objptrs ')'		{ $$ = $2; }
		;

kind		: ABSTRACT			{ $$ = KIND_ABSTRACT; }
		| STRUCTURAL			{ $$ = KIND_STRUCTURAL; }
		| AUXILIARY			{ $$ = KIND_AUXILIARY; }
		;

qdescr		: STRING			{
			struct oidname		*on, *old;

			if ((on = calloc(1, sizeof(*on))) == NULL) {
				yyerror("calloc");
				free($1);
				YYERROR;
			}
			on->on_name = $1;
			if (cattr) {
				on->on_attr_type = cattr;
				old = RB_INSERT(oidname_tree, &conf->attr_names,
				    on);
				if (old != NULL) {
					yyerror("attribute name '%s' already"
					    " defined for oid %s",
					    $1, old->on_attr_type->oid);
					free($1);
					free(on);
					YYERROR;
				}
			} else {
				on->on_object = cobj;
				old = RB_INSERT(oidname_tree,
				    &conf->object_names, on);
				if (old != NULL) {
					yyerror("object name '%s' already"
					    " defined for oid %s",
					    $1, old->on_object->oid);
					free($1);
					free(on);
					YYERROR;
				}
			}
		}
		;

qdescrs		: qdescr			{ $$ = append_name(NULL, $1); }
		| '(' qdescrlist ')'		{ $$ = $2; }
		;

qdescrlist	: /* empty */			{ $$ = NULL; }
		| qdescrlist qdescr		{ $$ = append_name($1, $2); }
		;

numericoid	: STRING			{
			if (!is_oidstr($1)) {
				yyerror("invalid OID: %s", $1);
				free($1);
				YYERROR;
			}
			$$ = $1;
		}
		;

oidlen		: STRING				{ $$ = $1; }
		| numericoid '{' NUMBER '}'	{
			$$ = $1;
		}

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2, 1)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
		}
		;

varset		: STRING '=' STRING		{
			/*if (conf->opts & BGPD_OPT_VERBOSE)*/
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
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

	file->errors++;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", file->name, yylval.lineno, fmt) == -1)
		fatalx("yyerror asprintf");
	vlog(LOG_CRIT, nfmt, ap);
	va_end(ap);
	free(nfmt);
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
		{ "ABSTRACT",		ABSTRACT },
		{ "AUXILIARY",		AUXILIARY },
		{ "COLLECTIVE",		COLLECTIVE },
		{ "DESC",		DESC },
		{ "EQUALITY",		EQUALITY },
		{ "MAY",		MAY },
		{ "MUST",		MUST },
		{ "NAME",		NAME },
		{ "NO-USER-MODIFICATION", NO_USER_MOD},
		{ "ORDERING",		ORDERING },
		{ "SINGLE-VALUE",	SINGLE_VALUE },
		{ "STRUCTURAL",		STRUCTURAL },
		{ "SUBSTR",		SUBSTR },
		{ "SUP",		SUP },
		{ "SYNTAX",		SYNTAX },
		{ "USAGE",		USAGE },
		{ "access",		ACCESS },
		{ "allow",		ALLOW },
		{ "any",		ANY },
		{ "attribute",		ATTRIBUTE },
		{ "attributetype",	ATTRIBUTETYPE },
		{ "bind",		BIND },
		{ "by",			BY },
		{ "cache-size",		CACHE_SIZE },
		{ "certificate",	CERTIFICATE },
		{ "children",		CHILDREN },
		{ "compression",	COMPRESSION },
		{ "dSAOperation",	DSA_OPERATION },
		{ "deny",		DENY },
		{ "directoryOperation",	DIRECTORY_OPERATION },
		{ "distributedOperation", DISTRIBUTED_OPERATION },
		{ "fsync",		FSYNC },
		{ "in",			IN },
		{ "include",		INCLUDE },
		{ "index",		INDEX },
		{ "index-cache-size",	INDEX_CACHE_SIZE },
		{ "ldaps",		LDAPS },
		{ "level",		LEVEL },
		{ "listen",		LISTEN },
		{ "namespace",		NAMESPACE },
		{ "objectclass",	OBJECTCLASS },
		{ "of",			OF },
		{ "on",			ON },
		{ "port",		PORT },
		{ "read",		READ },
		{ "relax",		RELAX },
		{ "root",		ROOT },
		{ "rootdn",		ROOTDN },
		{ "rootpw",		ROOTPW },
		{ "schema",		SCHEMA },
		{ "secure",		SECURE },
		{ "self",		SELF },
		{ "strict",		STRICT },
		{ "subtree",		SUBTREE },
		{ "tls",		TLS },
		{ "to",			TO },
		{ "use",		USE },
		{ "userApplications",	USER_APPLICATIONS },
		{ "write",		WRITE },

	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(int quotec)
{
	int		c, next;

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

	if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	while (c == EOF) {
		if (file == topfile || popfile() == EOF)
			return (EOF);
		c = getc(file->stream);
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

	/* skip to either EOF or the first real EOL */
	while (1) {
		if (pushback_index)
			c = pushback_buffer[--pushback_index];
		else
			c = lgetc(0);
		if (c == '\n') {
			file->lineno++;
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
	int	 quotec, next, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(0)) == EOF)
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
		if (*buf == '\0')
			return ('$');
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
		quotec = c;
		while (1) {
			if ((c = lgetc(quotec)) == EOF)
				return (0);
			if (c == '\n') {
				file->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n')
					continue;
				else
					lungetc(next);
			} else if (c == quotec) {
				*p = '\0';
				break;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			fatal("yylex: strdup");
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
			    LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
				    buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
			while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				fatal("yylex: strdup");
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = file->lineno;
		file->lineno++;
		goto top;
	}
	if (c == EOF)
		return (0);
	return (c);
}

int
check_file_secrecy(int fd, const char *fname)
{
	struct stat	st;

	if (fstat(fd, &st)) {
		log_warn("cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("%s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	log_debug("parsing config %s", name);

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
#if 0
	if (secret &&
	    check_file_secrecy(fileno(nfile->stream), nfile->name)) {
		fclose(nfile->stream);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
#endif
	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
popfile(void)
{
	struct file	*prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

int
parse_config(char *filename)
{
	struct sym		*sym, *next;
	int			 errors = 0;

	if ((conf = calloc(1, sizeof(struct ldapd_config))) == NULL)
		fatal(NULL);

	RB_INIT(&conf->attr_types);
	RB_INIT(&conf->objects);
	TAILQ_INIT(&conf->namespaces);
	TAILQ_INIT(&conf->listeners);
	if ((conf->sc_ssl = calloc(1, sizeof(*conf->sc_ssl))) == NULL)
		fatal(NULL);
	SPLAY_INIT(conf->sc_ssl);
	SIMPLEQ_INIT(&conf->acl);

	if ((file = pushfile(filename, 1)) == NULL) {
		free(conf);
		return (-1);
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if (/*(conf->opts & BGPD_OPT_VERBOSE2) &&*/ !sym->used)
			fprintf(stderr, "warning: macro \"%s\" not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	return (errors ? -1 : 0);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entry))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
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
	TAILQ_INSERT_TAIL(&symhead, sym, entry);
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
		fatal("cmdline_symset: malloc");

	strlcpy(sym, s, len);

	ret = symset(sym, val + 1, 1);
	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

struct attr_type *
lookup_attribute_by_name(const char *name)
{
	struct oidname		*on, find;

	find.on_name = name;
	on = RB_FIND(oidname_tree, &conf->attr_names, &find);

	if (on)
		return on->on_attr_type;
	return NULL;
}

struct attr_type *
lookup_attribute_by_oid(const char *oid)
{
	struct attr_type	 find;

	find.oid = oid;
	return RB_FIND(attr_type_tree, &conf->attr_types, &find);
}

struct attr_type *
lookup_attribute(const char *oid_or_name)
{
	if (is_oidstr(oid_or_name))
		return lookup_attribute_by_oid(oid_or_name);
	return lookup_attribute_by_name(oid_or_name);
}

struct object *
lookup_object_by_oid(const char *oid)
{
	struct object	 find;

	find.oid = oid;
	return RB_FIND(object_tree, &conf->objects, &find);
}

struct object *
lookup_object_by_name(const char *name)
{
	struct oidname		*on, find;

	find.on_name = name;
	on = RB_FIND(oidname_tree, &conf->object_names, &find);

	if (on)
		return on->on_object;
	return NULL;
}

struct object *
lookup_object(const char *oid_or_name)
{
	if (is_oidstr(oid_or_name))
		return lookup_object_by_oid(oid_or_name);
	return lookup_object_by_name(oid_or_name);
}

static struct attr_list *
append_attr(struct attr_list *alist, struct attr_type *a)
{
	struct attr_ptr		*aptr;

	if (alist == NULL) {
		if ((alist = calloc(1, sizeof(*alist))) == NULL) {
			yyerror("calloc");
			return NULL;
		}
		SLIST_INIT(alist);
	}

	if ((aptr = calloc(1, sizeof(*aptr))) == NULL) {
		yyerror("calloc");
		return NULL;
	}
	aptr->attr_type = a;
	SLIST_INSERT_HEAD(alist, aptr, next);

	return alist;
}

static struct obj_list *
append_obj(struct obj_list *olist, struct object *obj)
{
	struct obj_ptr		*optr;

	if (olist == NULL) {
		if ((olist = calloc(1, sizeof(*olist))) == NULL) {
			yyerror("calloc");
			return NULL;
		}
		SLIST_INIT(olist);
	}

	if ((optr = calloc(1, sizeof(*optr))) == NULL) {
		yyerror("calloc");
		return NULL;
	}
	optr->object = obj;
	SLIST_INSERT_HEAD(olist, optr, next);

	return olist;
}

int
is_oidstr(const char *oidstr)
{
	struct ber_oid		oid;
	return (ber_string2oid(oidstr, &oid) == 0);
}

static struct name_list *
append_name(struct name_list *nl, const char *name)
{
	struct name	*n;

	if (nl == NULL) {
		if ((nl = calloc(1, sizeof(*nl))) == NULL) {
			yyerror("calloc");
			return NULL;
		}
		SLIST_INIT(nl);
	}
	if ((n = calloc(1, sizeof(*n))) == NULL) {
		yyerror("calloc");
		return NULL;
	}
	n->name = name;
	SLIST_INSERT_HEAD(nl, n, next);

	return nl;
}

struct listener *
host_unix(const char *path)
{
	struct sockaddr_un	*saun;
	struct listener		*h;

	if (*path != '/')
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	saun = (struct sockaddr_un *)&h->ss;
	saun->sun_len = sizeof(struct sockaddr_un);
	saun->sun_family = AF_UNIX;
	if (strlcpy(saun->sun_path, path, sizeof(saun->sun_path)) >=
	    sizeof(saun->sun_path))
		fatal("socket path too long");
	h->flags = F_SECURE;

	return (h);
}

struct listener *
host_v4(const char *s, in_port_t port)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct listener		*h;

	bzero(&ina, sizeof(ina));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;
	sain->sin_port = port;

	return (h);
}

struct listener *
host_v6(const char *s, in_port_t port)
{
	struct in6_addr		 ina6;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	bzero(&ina6, sizeof(ina6));
	if (inet_pton(AF_INET6, s, &ina6) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	return (h);
}

int
host_dns(const char *s, const char *cert,
    struct listenerlist *al, int max, in_port_t port, u_int8_t flags)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("host_dns: could not parse \"%s\": %s", s,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res && cnt < max; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		h->port = port;
		h->flags = flags;
		h->ss.ss_family = res->ai_family;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));

		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
			sain->sin_port = port;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_port = port;
		}

		TAILQ_INSERT_HEAD(al, h, entry);
		cnt++;
	}
	if (cnt == max && res) {
		log_warnx("host_dns: %s resolves to more than %d hosts",
		    s, max);
	}
	freeaddrinfo(res0);
	return (cnt);
}

int
host(const char *s, const char *cert, struct listenerlist *al,
    int max, in_port_t port, u_int8_t flags)
{
	struct listener *h;

	/* Unix socket path? */
	h = host_unix(s);

	/* IPv4 address? */
	if (h == NULL)
		h = host_v4(s, port);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s, port);

	if (h != NULL) {
		h->port = port;
		h->flags |= flags;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, cert, al, max, port, flags));
}

int
interface(const char *s, const char *cert,
    struct listenerlist *al, int max, in_port_t port, u_int8_t flags)
{
	int			 ret = 0;
	struct ifaddrs		*ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (strcmp(s, p->ifa_name) != 0)
			continue;

		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			if ((h = calloc(1, sizeof(*h))) == NULL)
				fatal(NULL);
			sain = (struct sockaddr_in *)&h->ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_port = port;

			h->fd = -1;
			h->port = port;
			h->flags = flags;
			h->ssl = NULL;
			h->ssl_cert_name[0] = '\0';
			if (cert != NULL)
				(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));

			ret = 1;
			TAILQ_INSERT_HEAD(al, h, entry);

			break;

		case AF_INET6:
			if ((h = calloc(1, sizeof(*h))) == NULL)
				fatal(NULL);
			sin6 = (struct sockaddr_in6 *)&h->ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = port;

			h->fd = -1;
			h->port = port;
			h->flags = flags;
			h->ssl = NULL;
			h->ssl_cert_name[0] = '\0';
			if (cert != NULL)
				(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));

			ret = 1;
			TAILQ_INSERT_HEAD(al, h, entry);

			break;
		}
	}

	freeifaddrs(ifap);

	return ret;
}

static struct aci *
mk_aci(int type, int rights, enum scope scope, char *target, char *subject)
{
	struct aci	*aci;

	if ((aci = calloc(1, sizeof(*aci))) == NULL) {
		yyerror("calloc");
		return NULL;
	}
	aci->type = type;
	aci->rights = rights;
	aci->scope = scope;
	aci->target = target;
	aci->subject = subject;

	log_debug("%s %02X access to %s scope %d by %s",
	    aci->type == ACI_DENY ? "deny" : "allow",
	    aci->rights,
	    aci->target ?: "any",
	    aci->scope,
	    aci->subject ?: "any");

	return aci;
}

struct namespace *
namespace_new(const char *suffix)
{
	struct namespace		*ns;

	if ((ns = calloc(1, sizeof(*ns))) == NULL)
		return NULL;
	ns->suffix = strdup(suffix);
	ns->sync = 1;
	if (ns->suffix == NULL) {
		free(ns->suffix);
		free(ns);
		return NULL;
	}
	TAILQ_INIT(&ns->indices);
	TAILQ_INIT(&ns->request_queue);
	SIMPLEQ_INIT(&ns->acl);

	return ns;
}


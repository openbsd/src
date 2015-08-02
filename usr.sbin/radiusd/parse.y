/* Adapted from usr.sbin/ntpd/parse.y 1.50 */

/*
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
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "radiusd.h"
#include "radiusd_local.h"
#include "log.h"

static struct	 radiusd *conf;
static struct	 radiusd_authentication authen;
static struct	 radiusd_client client;

static struct	 radiusd_module *find_module (const char *);
static void	 free_str_l (void *);
static struct	 radiusd_module_ref *create_module_ref (const char *);
static void	 radiusd_authentication_init (struct radiusd_authentication *);
static void	 radiusd_client_init (struct radiusd_client *);

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;
struct file	*pushfile(const char *);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

typedef struct {
	union {
		int64_t				  number;
		char				 *string;
		struct radiusd_listen		  listen;
		int				  yesno;
		struct {
			char			**v;
			int			  c;
		} str_l;
		struct {
			int			 af;
			struct radiusd_addr	 addr;
			struct radiusd_addr	 mask;
		} prefix;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	INCLUDE LISTEN ON PORT CLIENT SECRET LOAD MODULE MSGAUTH_REQUIRED
%token	AUTHENTICATE AUTHENTICATE_BY DECORATE_BY SET
%token	ERROR YES NO
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		optport
%type	<v.listen>		listen_addr
%type	<v.str_l>		str_l
%type	<v.prefix>		prefix
%type	<v.yesno>		yesno
%type	<v.string>		strnum
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar listen '\n'
		| grammar client '\n'
		| grammar module '\n'
		| grammar authenticate '\n'
		| grammar error '\n'
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
			nfile->lineno--;
		}
		;
listen		: LISTEN ON listen_addr {
			struct radiusd_listen *n;

			if ((n = malloc(sizeof(struct radiusd_listen)))
			    == NULL) {
outofmemory:
				yyerror("Out of memory: %s", strerror(errno));
				YYERROR;
			}
			*n = $3;
			TAILQ_INSERT_TAIL(&conf->listen, n, next);
		}
listen_addr	: STRING optport {
			int		 gai_errno;
			struct addrinfo hints, *res;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = AI_PASSIVE;
			hints.ai_flags |= AI_NUMERICHOST | AI_NUMERICSERV;

			if ((gai_errno =
				    getaddrinfo($1, NULL, &hints, &res)) != 0 ||
			    res->ai_addrlen > sizeof($$.addr)) {
				yyerror("Could not parse the address: %s: %s",
				    $1, gai_strerror(gai_errno));
				free($1);
				YYERROR;
			}
			free($1);
			$$.stype = res->ai_socktype;
			$$.sproto = res->ai_protocol;
			memcpy(&$$.addr, res->ai_addr, res->ai_addrlen);
			$$.addr.ipv4.sin_port = ($2 == 0)?
			    htons(RADIUS_DEFAULT_PORT) : htons($2);
			freeaddrinfo(res);
		}
optport		: { $$ = 0; }
		| PORT NUMBER	{ $$ = $2; }
		;
client		: CLIENT prefix optnl clientopts_b {
			struct radiusd_client *client0;

			client0 = calloc(1, sizeof(struct radiusd_client));
			if (client0 == NULL)
				goto outofmemory;
			strlcpy(client0->secret, client.secret,
			    sizeof(client0->secret));
			client0->msgauth_required = client.msgauth_required;
			client0->af = $2.af;
			client0->addr = $2.addr;
			client0->mask = $2.mask;
			TAILQ_INSERT_TAIL(&conf->client, client0, next);
			radiusd_client_init(&client);
		}

clientopts_b	: '{' optnl_l clientopts_l optnl_l '}'
		| '{' optnl_l '}'	/* allow empty block */
		;

clientopts_l	: clientopts_l nl clientopts
		| clientopts
		;

clientopts	: SECRET STRING {
			if (strlcpy(client.secret, $2, sizeof(client.secret))
			    >= sizeof(client.secret)) {
				yyerror("secret is too long");
				YYERROR;
			}
		}
		| MSGAUTH_REQUIRED yesno {
			client.msgauth_required = $2;
		}
		;

prefix		: STRING '/' NUMBER {
			int		 gai_errno, q, r;
			struct addrinfo	 hints, *res;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;	/* dummy */
			hints.ai_flags |= AI_NUMERICHOST | AI_NUMERICSERV;

			if ((gai_errno = getaddrinfo($1, NULL, &hints, &res))
			    != 0) {
				free($1);
				yyerror("Could not parse the address: %s: %s",
				    $1, gai_strerror(gai_errno));
				YYERROR;
			}
			free($1);
			q = $3 >> 3;
			r = $3 & 7;
			switch (res->ai_family) {
			case AF_INET:
				if ($3 < 0 || 32 < $3) {
					yyerror("mask len %d is out of range",
					    $3);
					YYERROR;
				}
				$$.addr.addr.ipv4 = ((struct sockaddr_in *)
				    res->ai_addr)->sin_addr;
				$$.mask.addr.ipv4.s_addr = htonl((uint32_t)
				    ((0xffffffffffULL) << (32 - $3)));
				break;
			case AF_INET6:
				if ($3 < 0 || 128 < $3) {
					yyerror("mask len %d is out of range",
					    $3);
					YYERROR;
				}
				$$.addr.addr.ipv6 = ((struct sockaddr_in6 *)
				    res->ai_addr)->sin6_addr;
				memset(&$$.mask.addr.ipv6, 0,
				    sizeof($$.mask.addr.ipv6));
				if (q > 0)
					memset(&$$.mask.addr.ipv6, 0xff, q);
				if (r > 0)
					*((u_char *)&$$.mask.addr.ipv6 + q) =
					    (0xff00 >> r) & 0xff;
				break;
			}
			$$.af = res->ai_family;
			freeaddrinfo(res);
		}
		;
module		: MODULE LOAD STRING STRING {
			struct radiusd_module *module;
			if ((module = radiusd_module_load(conf, $4, $3))
			    == NULL) {
				free($3);
				free($4);
				YYERROR;
			}
			free($3);
			free($4);
			TAILQ_INSERT_TAIL(&conf->module, module, next);
		}
		| MODULE SET STRING STRING str_l {
			struct radiusd_module	*module;

			module = find_module($3);
			if (module == NULL) {
				yyerror("module `%s' is not found", $3);
				free($3);
				free($4);
				free_str_l(&$5);
				YYERROR;
			}
			if (radiusd_module_set(module, $4, $5.c, $5.v)) {
				yyerror("syntax error by module `%s'", $3);
				free($3);
				free($4);
				free_str_l(&$5);
				YYERROR;
			}
			free($3);
			free($4);
			free_str_l(&$5);
		}
		;
authenticate	: AUTHENTICATE str_l optnl authopts_b {
			struct radiusd_authentication *a;

			if ((a = calloc(1,
			    sizeof(struct radiusd_authentication))) == NULL) {
				free_str_l(&$2);
				goto outofmemory;
			}
			a->auth = authen.auth;
			a->deco = authen.deco;
			a->username = $2.v;

			TAILQ_INSERT_TAIL(&conf->authen, a, next);
			radiusd_authentication_init(&authen);
		}
		;

authopts_b	: '{' optnl_l authopts_l optnl_l '}'
		| '{' optnl_l '}'	/* empty options */
		;

authopts_l	: authopts_l nl authopts
		| authopts
		;

authopts	: AUTHENTICATE_BY STRING {
			struct radiusd_module_ref	*modref;

			modref = create_module_ref($2);
			free($2);
			if (modref == NULL)
				YYERROR;
			authen.auth = modref;
		}
		/* XXX decoration doesn't work for this moment.  */
		| DECORATE_BY str_l {
			int				 i;
			struct radiusd_module_ref	*modref;

			for (i = 0; i < $2.c; i++) {
				if ((modref = create_module_ref($2.v[i]))
				    == NULL) {
					free_str_l(&$2);
					YYERROR;
				}
				TAILQ_INSERT_TAIL(&authen.deco, modref, next);
			}
			free_str_l(&$2);
		}
		;
str_l		: str_l strnum {
			int	  i;
			char	**v;
			if ((v = calloc(sizeof(char **), $$.c + 2)) == NULL)
				goto outofmemory;
			for (i = 0; i < $$.c; i++)
				v[i] = $$.v[i];
			v[i++] = $2;
			v[i] = NULL;
			$$.c++;
			free($$.v);
			$$.v = v;
		}
		| strnum {
			if (($$.v = calloc(sizeof(char **), 2)) == NULL)
				goto outofmemory;
			$$.v[0] = $1;
			$$.v[1] = NULL;
			$$.c = 1;
		}
		;
strnum		: STRING	{ $$ = $1; }
		| NUMBER {
			/* Treat number as a string */
			asprintf(&($$), "%jd", (intmax_t)$1);
			if ($$ == NULL)
				goto outofmemory;
		}
		;
optnl		:
		| '\n'
		;
nl		: '\n' optnl		/* one new line or more */
		;
optnl_l		:
		| '\n' optnl_l
		;
yesno		: YES { $$ = true; }
		| NO  { $$ = false; }
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
	char		*msg;

	file->errors++;
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) == -1)
		fatalx("yyerror vasprintf");
	va_end(ap);
	logit(LOG_CRIT, "%s:%d: %s", file->name, yylval.lineno, msg);
	free(msg);
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
		{ "authenticate",		AUTHENTICATE},
		{ "authenticate-by",		AUTHENTICATE_BY},
		{ "client",			CLIENT},
		{ "decorate-by",		DECORATE_BY},
		{ "include",			INCLUDE},
		{ "listen",			LISTEN},
		{ "load",			LOAD},
		{ "module",			MODULE},
		{ "msgauth-required",		MSGAUTH_REQUIRED},
		{ "no",				NO},
		{ "on",				ON},
		{ "port",			PORT},
		{ "secret",			SECRET},
		{ "set",			SET},
		{ "yes",			YES},
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
	char	*p;
	int	 quotec, next, c;
	int	 token;

	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */

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
				else if (next == '\n') {
					file->lineno++;
					continue;
				} else
					lungetc(next);
			} else if (c == quotec) {
				*p = '\0';
				break;
			} else if (c == '\0') {
				yyerror("syntax error");
				return (findeol());
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
	}
	if (c == EOF)
		return (0);
	return (c);
}

struct file *
pushfile(const char *name)
{
	struct file	*nfile;

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
parse_config(const char *filename, struct radiusd *radiusd)
{
	int				 errors = 0;
	struct radiusd_listen		*l;
	struct radiusd_module_ref	*m, *mt;

	conf = radiusd;
	radiusd_conf_init(conf);
	radiusd_authentication_init(&authen);
	radiusd_client_init(&client);
	authen.auth = NULL;

	if ((file = pushfile(filename)) == NULL) {
		errors++;
		goto out;
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	if (TAILQ_EMPTY(&conf->listen)) {
		if ((l = malloc(sizeof(struct radiusd_listen))) == NULL) {
			log_warn("Out of memory");
			return (-1);
		}
		l->stype = SOCK_DGRAM;
		l->sproto = IPPROTO_UDP;
		l->addr.ipv4.sin_family = AF_INET;
		l->addr.ipv4.sin_len = sizeof(struct sockaddr_in);
		l->addr.ipv4.sin_addr.s_addr = htonl(0x7F000001L);
		l->addr.ipv4.sin_port = htons(RADIUS_DEFAULT_PORT);
		TAILQ_INSERT_TAIL(&conf->listen, l, next);
	}
	TAILQ_FOREACH(l, &conf->listen, next) {
		l->sock = -1;
	}
	if (authen.auth != NULL)
		free(authen.auth);
	TAILQ_FOREACH_SAFE(m, &authen.deco, next, mt) {
		TAILQ_REMOVE(&authen.deco, m, next);
		free(m);
	}
out:
	conf = NULL;
	return (errors ? -1 : 0);
}

static struct radiusd_module *
find_module(const char *name)
{
	struct radiusd_module	*module;

	TAILQ_FOREACH(module, &conf->module, next) {
		if (strcmp(name, module->name) == 0)
			return (module);
	}

	return (NULL);
}

static void
free_str_l(void *str_l0)
{
	int				  i;
	struct {
		char			**v;
		int			  c;
	}				 *str_l = str_l0;

	for (i = 0; i < str_l->c; i++)
		free(str_l->v[i]);
	free(str_l->v);
}

static struct radiusd_module_ref *
create_module_ref(const char *modulename)
{
	struct radiusd_module		*module;
	struct radiusd_module_ref	*modref;

	if ((module = find_module(modulename)) == NULL) {
		yyerror("module `%s' is not found", modulename);
		return (NULL);
	}
	if ((modref = calloc(1, sizeof(struct radiusd_module_ref))) == NULL) {
		yyerror("Out of memory: %s", strerror(errno));
		return (NULL);
	}
	modref->module = module;

	return (modref);
}

static void
radiusd_authentication_init(struct radiusd_authentication *auth)
{
	memset(auth, 0, sizeof(struct radiusd_authentication));
	TAILQ_INIT(&auth->deco);
}

static void
radiusd_client_init(struct radiusd_client *clnt)
{
	memset(clnt, 0, sizeof(struct radiusd_client));
	clnt->msgauth_required = true;
}

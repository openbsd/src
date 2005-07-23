/*	$OpenBSD: parse.y,v 1.10 2005/07/23 19:28:27 hshoexer Exp $	*/

/*
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "ipsecctl.h"

#define KEYSIZE_LIMIT	1024

static struct ipsecctl	*ipsec = NULL;
static FILE		*fin = NULL;
static int		 lineno = 1;
static int		 errors = 0;
static int		 debug = 0;

int			 yyerror(const char *, ...);
int			 yyparse(void);
int			 kw_cmp(const void *, const void *);
int			 lookup(char *);
int			 lgetc(FILE *);
int			 lungetc(int);
int			 findeol(void);
int			 yylex(void);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int		 used;
	int		 persist;
	char		*nam;
	char		*val;
};

int			 symset(const char *, const char *, int);
int			 cmdline_symset(char *);
char			*symget(const char *);
int			 atoul(char *, u_long *);
int			 atospi(char *, u_int32_t *);
u_int8_t		 x2i(unsigned char *);
struct ipsec_key	*parsekey(unsigned char *, size_t);
struct ipsec_addr	*host(const char *);
struct ipsec_addr	*copyhost(const struct ipsec_addr *);
struct ipsec_rule	*create_sa(struct ipsec_addr *, struct ipsec_addr *,
			 u_int32_t, struct ipsec_key *);
struct ipsec_rule	*create_flow(u_int8_t, struct ipsec_addr *, struct
			     ipsec_addr *, struct ipsec_addr *, u_int8_t,
			     char *, char *, u_int16_t);
struct ipsec_rule	*reverse_rule(struct ipsec_rule *);

typedef struct {
	union {
		u_int32_t	 number;
		u_int8_t	 dir;
		char		*string;
		int		 log;
		u_int8_t	 protocol;
		struct {
			struct ipsec_addr *src;
			struct ipsec_addr *dst;
		} hosts;
		struct ipsec_addr *peer;
		struct ipsec_addr *host;
		struct {
			char *srcid;
			char *dstid;
		} ids;
		char		*id;
		u_int16_t	 authtype;
		struct {
			u_int32_t	spiout;
			u_int32_t	spiin;
		} spis;
		struct ipsec_key *key;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FLOW FROM ESP AH IN PEER ON OUT TO SRCID DSTID RSA PSK TCPMD5 SPI KEY
%token	KEYFILE ERROR
%token	<v.string>		STRING
%type	<v.dir>			dir
%type	<v.protocol>		protocol
%type	<v.number>		number
%type	<v.hosts>		hosts
%type	<v.peer>		peer
%type	<v.host>		host
%type	<v.ids>			ids
%type	<v.id>			id
%type	<v.authtype>		authtype
%type	<v.spis>		spispec
%type	<v.key>			keyspec
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar flowrule '\n'
		| grammar tcpmd5rule '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING			{
			unsigned long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				free($1);
				YYERROR;
			}
			if (ulval > UINT_MAX) {
				yyerror("0x%lx out of range", ulval);
				free($1);
				YYERROR;
			}
			$$ = (u_int32_t)ulval;
			free($1);
		}

flowrule	: FLOW ipsecrule		{ }
		;

tcpmd5rule	: TCPMD5 hosts spispec keyspec	{
			struct ipsec_rule	*r;

			r = create_sa($2.src, $2.dst, $3.spiout, $4);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "tcpmd5rule: ipsecctl_add_rule");
		}
		;

ipsecrule	: protocol dir hosts peer ids authtype	{
			struct ipsec_rule	*r;

			r = create_flow($2, $3.src, $3.dst, $4, $1, $5.srcid,
			    $5.dstid, $6);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "esprule: ipsecctl_add_rule");

			/* Create and add reverse rule. */
			if ($2 == IPSEC_INOUT) {
				r = reverse_rule(r);	
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "esprule: ipsecctl_add_rule");
			}
		}
		;

protocol	: /* empty */			{ $$ = IPSEC_ESP; }
		| ESP				{ $$ = IPSEC_ESP; }
		| AH				{ $$ = IPSEC_AH; }
		;

dir		: /* empty */			{ $$ = IPSEC_INOUT; }
		| IN				{ $$ = IPSEC_IN; }
		| OUT				{ $$ = IPSEC_OUT; }
		;

hosts		: FROM host TO host		{
			$$.src = $2;
			$$.dst = $4;
		}
		;

peer		: /* empty */			{ $$ = NULL; }
		| PEER STRING			{
			if (($$ = host($2)) == NULL) {
				free($2);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free($2);
		}
		;

host		: STRING			{
			if (($$ = host($1)) == NULL) {
				free($1);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free($1);
		}
		| STRING '/' number		{
			char	*buf;

			if (asprintf(&buf, "%s/%u", $1, $3) == -1)
				err(1, "host: asprintf");
			free($1);
			if (($$ = host(buf)) == NULL)	{
				free(buf);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free(buf);
		}
		;

ids		: /* empty */			{
			$$.srcid = NULL;
			$$.dstid = NULL;
		}
		| SRCID id DSTID id		{
			$$.srcid = $2;
			$$.dstid = $4;
		}
		| SRCID id			{
			$$.srcid = $2;
			$$.dstid = NULL;
		}
		| DSTID id			{
			$$.srcid = NULL;
			$$.dstid = $2;
		}
		;

id		: STRING			{ $$ = $1; }
		;

authtype	: /* empty */			{ $$ = 0; }
		| RSA				{ $$ = AUTH_RSA; }
		| PSK				{ $$ = AUTH_PSK; }
		;

spispec		: SPI STRING			{
			u_int32_t	 spi;
			char		*p = strchr($2, ':');

			if (p != NULL) {
				*p++ = 0;

				if (atospi($2, &spi) == -1) {
					yyerror("%s is not a valid spi", $2);
					free($2);
					YYERROR;
				}
				$$.spiin = spi;
			}
			if (atospi($2, &spi) == -1) {
				yyerror("%s is not a valid spi", $2);
				free($2);
				YYERROR;
			}
			$$.spiout = spi;

			free($2);
		}
		;

keyspec		: /* empty */			{ $$ = NULL; }
		| KEY STRING			{
			unsigned char	 *hex;
			
			hex = $2;
			if (!strncmp(hex, "0x", 2))
				hex += 2;
			$$ = parsekey(hex, strlen(hex));

			free($2);
		}
		| KEYFILE STRING		{
			struct stat	 sb;
			int		 fd;
			unsigned char	*hex;

			if (stat($2, &sb) < 0)
				err(1, "stat");
			if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0))
				errx(1, "key too %s", sb.st_size ? "large" :
				    "small");
			if ((hex = calloc(sb.st_size, sizeof(unsigned char)))
			    == NULL)
				err(1, "calloc");
			if ((fd = open($2, O_RDONLY)) < 0)
				err(1, "open");
			if (read(fd, hex, sb.st_size) < sb.st_size)
				err(1, "read");
			close(fd);
			$$ = parsekey(hex, sb.st_size);

			free($2);
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
	extern char 	*infile;

	errors = 1;
	va_start(ap, fmt);
	fprintf(stderr, "%s: %d: ", infile, yyval.lineno);
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
		{ "ah",			AH},
		{ "dstid",		DSTID},
		{ "esp",		ESP},
		{ "flow",		FLOW},
		{ "from",		FROM},
		{ "in",			IN},
		{ "key",		KEY},
		{ "keyfile",		KEYFILE},
		{ "out",		OUT},
		{ "peer",		PEER},
		{ "psk",		PSK},
		{ "rsa",		RSA},
		{ "spi",		SPI},
		{ "srcid",		SRCID},
		{ "tcpmd5",		TCPMD5},
		{ "to",			TO},
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p) {
		if (debug > 1)
			fprintf(stderr, "%s: %d\n", s, p->k_val);
		return (p->k_val);
	} else {
		if (debug > 1)
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
			yyerror("macro \"%s\" not defined", buf);
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
			err(1, "yylex: strdup");
		return (STRING);
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

int
parse_rules(FILE *input, struct ipsecctl *ipsecx)
{
	struct sym	*sym, *next;

	ipsec = ipsecx;
	fin = input;
	lineno = 1;
	errors = 0;

	yyparse();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entries);
		free(sym->nam);
		free(sym->val);
		TAILQ_REMOVE(&symhead, sym, entries);
		free(sym);
	}

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
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		err(1, "cmdline_symset: malloc");

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

int
atospi(char *s, u_int32_t *spivalp)
{
	unsigned long	ulval;

	if (atoul(s, &ulval) == -1)
		return (-1);
	if (ulval >= SPI_RESERVED_MIN && ulval <= SPI_RESERVED_MAX)
		return (-1);
	*spivalp = ulval;
	return (0);
}

u_int8_t
x2i(unsigned char *s)
{
	char	ss[3];

	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	if (!isxdigit(s[0]) || !isxdigit(s[1])) {
		yyerror("keys need to be specified in hex digits");
		return -1;
	}
	return ((u_int8_t)strtoul(ss, NULL, 16));
}

struct ipsec_key *
parsekey(unsigned char *hexkey, size_t len)
{
	struct ipsec_key *key;
	int		  i;

	key = calloc(1, sizeof(struct ipsec_key));
	if (key == NULL)
		err(1, "calloc");

	key->len = len / 2;
	key->data = calloc(key->len, sizeof(u_int8_t));
	if (key->data == NULL)
		err(1, "calloc");

	for (i = 0; i < (int)key->len; i++)
		key->data[i] = x2i(hexkey + 2 * i);

	return (key);
}

struct ipsec_addr *
host(const char *s)
{
	struct ipsec_addr	*ipa;
	int			 i, bits = 32;

	/* XXX for now only AF_INET. */

	ipa = calloc(1, sizeof(struct ipsec_addr));
	if (ipa == NULL)
		err(1, "calloc");
	
	if (strrchr(s, '/') != NULL) {
		bits = inet_net_pton(AF_INET, s, &ipa->v4, sizeof(ipa->v4));
		if (bits == -1 || bits > 32) {
			free(ipa);
			return(NULL);
		}
	} else {
		if (inet_pton(AF_INET, s, &ipa->v4) != 1) {
			free(ipa);
			return NULL;
		}
	}

	bzero(&ipa->v4mask, sizeof(ipa->v4mask));
	if (bits == 32) {
		ipa->v4mask.mask32 = 0xffffffff;
		ipa->netaddress = 0;
	} else {
		for (i = 31; i > 31 - bits; i--)
			ipa->v4mask.mask32 |= (1 << i);
		ipa->v4mask.mask32 = htonl(ipa->v4mask.mask32);
		ipa->netaddress = 1;
	}

	ipa->af = AF_INET;

	return ipa;
}

struct ipsec_addr *
copyhost(const struct ipsec_addr *src)
{
	struct ipsec_addr *dst;

	dst = calloc(1, sizeof(struct ipsec_addr));
	if (dst == NULL)
		err(1, "calloc");
	
	memcpy(dst, src, sizeof(struct ipsec_addr));
	return dst;
}

struct ipsec_rule *
create_sa(struct ipsec_addr *src, struct ipsec_addr *dst, u_int32_t spi,
    struct ipsec_key *key)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "calloc");

	r->type = RULE_SA;

	r->src = src;
	r->dst = dst;
	r->spi = spi;
	r->key = key;

	return r;
}

struct ipsec_rule *
create_flow(u_int8_t dir, struct ipsec_addr *src, struct ipsec_addr *dst,
    struct ipsec_addr *peer, u_int8_t proto, char *srcid, char *dstid,
    u_int16_t authtype)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "calloc");
	
	r->type = RULE_FLOW;

	if (dir == IPSEC_INOUT)
		r->direction = IPSEC_OUT;
	else
		r->direction = dir;

	if (r->direction == IPSEC_IN)
		r->flowtype = TYPE_USE;
	else
		r->flowtype = TYPE_REQUIRE;

	r->src = src;
	r->dst = dst;

	if (peer == NULL) {
		/* Set peer to remote host.  Must be a host address. */
		if (r->direction == IPSEC_IN) {
			if (r->src->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->src);
		} else {
			if (r->dst->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->dst);
		}
	} else
		r->peer = peer;

	r->proto = proto;
	r->auth.srcid = srcid;
	r->auth.dstid = dstid;
	r->auth.idtype = ID_FQDN;	/* XXX For now only FQDN. */
#ifdef notyet
	r->auth.type = authtype;
#endif

	return r;

errout:
	free(r);
	if (srcid)
		free(srcid);
	if (dstid)
		free(dstid);
	free(src);
	free(dst);

	return NULL;
}

struct ipsec_rule *
reverse_rule(struct ipsec_rule *rule)
{
	struct ipsec_rule *reverse;

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "calloc");

	reverse->type = RULE_FLOW;
	
	if (rule->direction == (u_int8_t)IPSEC_OUT) {
		reverse->direction = (u_int8_t)IPSEC_IN;
		reverse->flowtype = TYPE_USE;
	} else {
		reverse->direction = (u_int8_t)IPSEC_OUT;
		reverse->flowtype = TYPE_REQUIRE;
	}
	
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	reverse->peer = copyhost(rule->peer);
	reverse->proto = (u_int8_t)rule->proto;

	if (rule->auth.dstid && (reverse->auth.dstid =
	    strdup(rule->auth.dstid)) == NULL)
		err(1, "strdup");
	if (rule->auth.srcid && (reverse->auth.srcid =
	    strdup(rule->auth.srcid)) == NULL)
		err(1, "strdup");
	reverse->auth.idtype = rule->auth.idtype;
	reverse->auth.type = rule->auth.type;

	return reverse;
}

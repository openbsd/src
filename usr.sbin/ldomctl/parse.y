/*	$OpenBSD: parse.y,v 1.6 2014/11/20 05:51:20 jsg Exp $	*/

/*
 * Copyright (c) 2012 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldomctl.h"
#include "util.h"

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
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

struct ldom_config		*conf;

struct opts {
	uint64_t	mac_addr;
	uint64_t	mtu;
} opts;
void		opts_default(void);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct opts		 opts;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	DOMAIN
%token	VCPU MEMORY VDISK VNET
%token	MAC_ADDR MTU
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		memory
%type	<v.opts>		vnet_opts vnet_opts_l vnet_opt
%type	<v.opts>		mac_addr
%type	<v.opts>		mtu
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar domain '\n'
		| grammar error '\n'		{ file->errors++; }
		;

domain		: DOMAIN STRING optnl '{' optnl	{
			domain = xzalloc(sizeof(struct domain));
			domain->name = $2;
			SIMPLEQ_INIT(&domain->vdisk_list);
			SIMPLEQ_INIT(&domain->vnet_list);
		}
		    domainopts_l '}' {
			/* domain names need to be unique. */
			struct domain *odomain;
			SIMPLEQ_FOREACH(odomain, &conf->domain_list, entry)
				if (strcmp(odomain->name, $2) == 0) {
					yyerror("duplicate domain name: %s", $2);
					YYERROR;
				}
			SIMPLEQ_INSERT_TAIL(&conf->domain_list, domain, entry);
			domain = NULL;
		}
		;

domainopts_l	: domainopts_l domainoptsl
		| domainoptsl
		;

domainoptsl	: domainopts nl
		;

domainopts	: VCPU NUMBER {
			domain->vcpu = $2;
		}
		| MEMORY memory {
			domain->memory = $2;
		}
		| VDISK STRING {
			struct vdisk *vdisk = xmalloc(sizeof(struct vdisk));
			vdisk->path = $2;
			SIMPLEQ_INSERT_TAIL(&domain->vdisk_list, vdisk, entry);
		}
		| VNET vnet_opts {
			struct vnet *vnet = xmalloc(sizeof(struct vnet));
			vnet->mac_addr = $2.mac_addr;
			vnet->mtu = $2.mtu;
			SIMPLEQ_INSERT_TAIL(&domain->vnet_list, vnet, entry);
		}
		;

vnet_opts	:	{ opts_default(); }
		  vnet_opts_l
			{ $$ = opts; }
		|	{ opts_default(); $$ = opts; }
		;
vnet_opts_l	: vnet_opts_l vnet_opt
		| vnet_opt
		;
vnet_opt	: mac_addr
		| mtu
		;

mac_addr	: MAC_ADDR '=' STRING {
			struct ether_addr *ea;

			if ((ea = ether_aton($3)) == NULL) {
				yyerror("invalid address: %s", $3);
				YYERROR;
			}

			opts.mac_addr =
			    (uint64_t)ea->ether_addr_octet[0] << 40 |
			    (uint64_t)ea->ether_addr_octet[1] << 32 |
			    ea->ether_addr_octet[2] << 24 |
			    ea->ether_addr_octet[3] << 16 |
			    ea->ether_addr_octet[4] << 8 |
			    ea->ether_addr_octet[5];
		}
		;

mtu		: MTU '=' NUMBER {
			opts.mtu = $3;
		}
		;

memory		: NUMBER {
			$$ = $1;
		}
		| STRING {
			uint64_t size;
			char *cp;

			size = strtoll($1, &cp, 10);
			if (cp != NULL) {
				if (strcmp(cp, "K") == 0)
					size *= 1024;
				else if (strcmp(cp, "M") == 0)
					size *= 1024 * 1024;
				else if (strcmp(cp, "G") == 0)
					size *= 1024 * 1024 * 1024;
				else {
                                        yyerror("unknown unit %s", cp);
                                        YYERROR;
				}
			}
			$$ = size;
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

%%

void
opts_default(void)
{
	opts.mac_addr = -1;
	opts.mtu = 1500;
}

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d ", file->name, yylval.lineno);
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
		{ "domain",		DOMAIN},
		{ "mac-addr",		MAC_ADDR},
		{ "memory",		MEMORY},
		{ "mtu",		MTU},
		{ "vcpu",		VCPU},
		{ "vdisk",		VDISK},
		{ "vnet",		VNET}
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

u_char	*parsebuf;
int	 parseindex;
u_char	 pushback_buffer[MAXPUSHBACK];
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
	u_char	 buf[8096];
	u_char	*p;
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
			*p++ = c;
		}
		yylval.v.string = xstrdup(buf);
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
			yylval.v.string = xstrdup(buf);
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

	nfile = xzalloc(sizeof(struct file));
	nfile->name = xstrdup(name);
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		warn("%s", nfile->name);
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
parse_config(const char *filename, struct ldom_config *xconf)
{
	int		 errors = 0;

	conf = xconf;

	if ((file = pushfile(filename)) == NULL) {
		return (-1);
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	return (errors ? -1 : 0);
}

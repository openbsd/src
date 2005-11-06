/*	$OpenBSD: parse.y,v 1.33 2005/11/06 22:51:51 hshoexer Exp $	*/

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

const struct ipsec_xf authxfs[] = {
	{"unknown",		AUTHXF_UNKNOWN,		0,	0},
	{"none",		AUTHXF_NONE,		0,	0},
	{"hmac-md5",		AUTHXF_HMAC_MD5,	16,	0},
	{"hmac-ripemd160",	AUTHXF_HMAC_RIPEMD160,	20,	0},
	{"hmac-sha1",		AUTHXF_HMAC_SHA1,	20,	0},
	{"hmac-sha2-256",	AUTHXF_HMAC_SHA2_256,	32,	0},
	{"hmac-sha2-384",	AUTHXF_HMAC_SHA2_384,	48,	0},
	{"hmac-sha2-512",	AUTHXF_HMAC_SHA2_512,	64,	0},
	{"md5",			AUTHXF_MD5,		16,	0},
	{"sha1",		AUTHXF_SHA1,		20,	0},
	{NULL,			0,			0,	0},
};

const struct ipsec_xf encxfs[] = {
	{"unknown",		ENCXF_UNKNOWN,		0,	0},
	{"none",		ENCXF_NONE,		0,	0},
	{"3des-cbc",		ENCXF_3DES_CBC,		24,	24},
	{"des-cbc",		ENCXF_DES_CBC,		8,	8},
	{"aes",			ENCXF_AES,		16,	32},
	{"aesctr",		ENCXF_AESCTR,		16+4,	32+4},
	{"blowfish",		ENCXF_BLOWFISH,		5,	56},
	{"cast128",		ENCXF_CAST128,		5,	16},
	{"null",		ENCXF_NULL,		0,	0},
	{"skipjack",		ENCXF_SKIPJACK,		10,	10},
	{NULL,			0,			0,	0},
};

const struct ipsec_xf compxfs[] = {
	{"unknown",		COMPXF_UNKNOWN,		0,	0},
	{"deflate",		COMPXF_DEFLATE,		0,	0},
	{"lzs",			COMPXF_LZS,		0,	0},
	{NULL,			0,			0,	0},
};

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
struct ipsec_key	*parsekeyfile(char *);
struct ipsec_addr_wrap	*host(const char *);
struct ipsec_addr_wrap	*host_v4(const char *, int);
void			 set_ipmask(struct ipsec_addr_wrap *, u_int8_t);
struct ipsec_addr_wrap	*copyhost(const struct ipsec_addr_wrap *);
const struct ipsec_xf	*parse_xf(const char *, const struct ipsec_xf *);
struct ipsec_transforms *transforms(const char *, const char *, const char *);
struct ipsec_transforms *copytransforms(const struct ipsec_transforms *);
int			 validate_sa(u_int32_t, u_int8_t,
			     struct ipsec_transforms *, struct ipsec_key *,
			     struct ipsec_key *);
struct ipsec_rule	*create_sa(u_int8_t, struct ipsec_addr_wrap *,
			     struct ipsec_addr_wrap *, u_int32_t,
			     struct ipsec_transforms *, struct ipsec_key *,
			     struct ipsec_key *);
struct ipsec_rule	*reverse_sa(struct ipsec_rule *, u_int32_t,
			     struct ipsec_key *, struct ipsec_key *);
struct ipsec_rule	*create_flow(u_int8_t, struct ipsec_addr_wrap *, struct
			     ipsec_addr_wrap *, struct ipsec_addr_wrap *,
			     u_int8_t, char *, char *, u_int16_t);
struct ipsec_rule	*reverse_rule(struct ipsec_rule *);
struct ipsec_rule	*create_ike(struct ipsec_addr_wrap *, struct
			     ipsec_addr_wrap *, struct ipsec_addr_wrap *,
			     struct ipsec_transforms *, struct
			     ipsec_transforms *, u_int8_t, u_int8_t, char *,
			     char *);

typedef struct {
	union {
		u_int32_t	 number;
		u_int8_t	 ikemode;
		u_int8_t	 dir;
		char		*string;
		u_int8_t	 protocol;
		struct {
			struct ipsec_addr_wrap *src;
			struct ipsec_addr_wrap *dst;
		} hosts;
		struct ipsec_addr_wrap *peer;
		struct ipsec_addr_wrap *host;
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
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} authkeys;
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} enckeys;
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} keys;
		struct ipsec_transforms *transforms;
		struct ipsec_transforms *mmxfs;
		struct ipsec_transforms *qmxfs;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FLOW FROM ESP AH IN PEER ON OUT TO SRCID DSTID RSA PSK TCPMD5 SPI
%token	AUTHKEY ENCKEY FILENAME AUTHXF ENCXF ERROR IKE MAIN QUICK PASSIVE
%token	ACTIVE ANY IPCOMP COMPXF
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
%type	<v.authkeys>		authkeyspec
%type	<v.enckeys>		enckeyspec
%type	<v.keys>		keyspec
%type	<v.transforms>		transforms
%type	<v.mmxfs>		mmxfs
%type	<v.qmxfs>		qmxfs
%type	<v.ikemode>		ikemode
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar ikerule '\n'
		| grammar flowrule '\n'
		| grammar sarule '\n'
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

tcpmd5rule	: TCPMD5 hosts spispec authkeyspec	{
			struct ipsec_rule	*r;

			r = create_sa(IPSEC_TCPMD5, $2.src, $2.dst, $3.spiout,
			    NULL, $4.keyout, NULL);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "tcpmd5rule: ipsecctl_add_rule");

			/* Create and add reverse SA rule. */
			if ($3.spiin != 0 || $4.keyin != NULL) {
				r = reverse_sa(r, $3.spiin, $4.keyin, NULL);
				if (r == NULL)
					YYERROR;
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "tcpmd5rule: ipsecctl_add_rule");
			}
		}
		;

sarule		: protocol hosts spispec transforms authkeyspec enckeyspec {
			struct ipsec_rule	*r;

			r = create_sa($1, $2.src, $2.dst, $3.spiout, $4,
			    $5.keyout, $6.keyout);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "sarule: ipsecctl_add_rule");

			/* Create and add reverse SA rule. */
			if ($3.spiin != 0 || $5.keyin || $6.keyin) {
				r = reverse_sa(r, $3.spiin, $5.keyin,
				    $6.keyin);
				if (r == NULL)
					YYERROR;
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "sarule: ipsecctl_add_rule");
			}
		}
		;

flowrule	: FLOW protocol dir hosts peer ids authtype	{
			struct ipsec_rule	*r;

			r = create_flow($3, $4.src, $4.dst, $5, $2, $6.srcid,
			    $6.dstid, $7);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "flowrule: ipsecctl_add_rule");

			/* Create and add reverse flow rule. */
			if ($3 == IPSEC_INOUT) {
				r = reverse_rule(r);	
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "flowrule: ipsecctl_add_rule");
			}
		}
		;

ikerule		: IKE ikemode protocol hosts peer mmxfs qmxfs ids {
			struct ipsec_rule	*r;

			r = create_ike($4.src, $4.dst, $5, $6, $7, $3, $2,
			    $8.srcid, $8.dstid);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "ikerule: ipsecctl_add_rule");
		};

protocol	: /* empty */			{ $$ = IPSEC_ESP; }
		| ESP				{ $$ = IPSEC_ESP; }
		| AH				{ $$ = IPSEC_AH; }
		| IPCOMP			{ $$ = IPSEC_IPCOMP; }
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
		| ANY				{
			struct ipsec_addr_wrap	*ipa;

			ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
			if (ipa == NULL)
				err(1, "host: calloc");

			ipa->af = AF_INET;
			ipa->netaddress = 1;
			if ((ipa->name = strdup("0.0.0.0/0")) == NULL)
				err(1, "host: strdup");
			$$ = ipa;
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

				if (atospi(p, &spi) == -1) {
					yyerror("%s is not a valid spi", p);
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

transforms	: /* empty */			{
			struct ipsec_transforms *xfs;

			/* We create just an empty transform */
			if ((xfs = calloc(1, sizeof(struct ipsec_transforms)))
			    == NULL)
				err(1, "transforms: calloc");
			$$ = xfs;
		}
		| AUTHXF STRING ENCXF STRING	{
			if (($$ = transforms($2, $4, NULL)) == NULL) {
				free($2);
				free($4);
				yyerror("could not parse transforms");
				YYERROR;
			}
			free($2);
			free($4);
		}
		| AUTHXF STRING			{
			if (($$ = transforms($2, NULL, NULL)) == NULL) {
				free($2);
				yyerror("could not parse transforms");
				YYERROR;
			}
			free($2);
		}
		| ENCXF STRING			{
			if (($$ = transforms(NULL, $2, NULL)) == NULL) {
				free($2);
				yyerror("could not parse transforms");
				YYERROR;
			}
			free($2);
		}
		| COMPXF STRING			{
			if (($$ = transforms(NULL, NULL, $2)) == NULL) {
				free($2);
				yyerror("could not parse transforms");
				YYERROR;
			}
			free($2);
		}
		;

mmxfs		: /* empty */			{
			struct ipsec_transforms *xfs;

			/* We create just an empty transform */
			if ((xfs = calloc(1, sizeof(struct ipsec_transforms)))
			    == NULL)
				err(1, "mmxfs: calloc");
			$$ = xfs;
		}
		| MAIN transforms		{ $$ = $2; }
		; 

qmxfs		: /* empty */			{
			struct ipsec_transforms *xfs;

			/* We create just an empty transform */
			if ((xfs = calloc(1, sizeof(struct ipsec_transforms)))
			    == NULL)
				err(1, "qmxfs: calloc");
			$$ = xfs;
		}
		| QUICK transforms		{ $$ = $2; }
		;

authkeyspec	: /* empty */			{
			$$.keyout = NULL;
			$$.keyin = NULL;
		}
		| AUTHKEY keyspec		{
			$$.keyout = $2.keyout;
			$$.keyin = $2.keyin;
		}
		;

enckeyspec	: /* empty */			{
			$$.keyout = NULL;
			$$.keyin = NULL;
		}
		| ENCKEY keyspec		{
			$$.keyout = $2.keyout;
			$$.keyin = $2.keyin;
		}
		;

keyspec		: STRING			{
			unsigned char	*hex;
			unsigned char	*p = strchr($1, ':');
			
			if (p != NULL ) {
				*p++ = 0;

				if (!strncmp(p, "0x", 2))
					p += 2;
				$$.keyin = parsekey(p, strlen(p));
			}

			hex = $1;
			if (!strncmp(hex, "0x", 2))
				hex += 2;
			$$.keyout = parsekey(hex, strlen(hex));

			free($1);
		}
		| FILENAME STRING		{
			unsigned char	*p = strchr($2, ':');

			if (p != NULL) {
				*p++ = 0;
				$$.keyin = parsekeyfile(p);
			}
			$$.keyout = parsekeyfile($2);
			free($2);
		}
		;
ikemode		: /* empty */			{ $$ = IKE_ACTIVE; }
		| PASSIVE			{ $$ = IKE_PASSIVE; }
		| ACTIVE			{ $$ = IKE_ACTIVE; }
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
		{ "active",		ACTIVE},
		{ "ah",			AH},
		{ "any",		ANY},
		{ "auth",		AUTHXF},
		{ "authkey",		AUTHKEY},
		{ "comp",		COMPXF},
		{ "dstid",		DSTID},
		{ "enc",		ENCXF},
		{ "enckey",		ENCKEY},
		{ "esp",		ESP},
		{ "file",		FILENAME},
		{ "flow",		FLOW},
		{ "from",		FROM},
		{ "ike",		IKE},
		{ "in",			IN},
		{ "ipcomp",		IPCOMP},
		{ "main",		MAIN},
		{ "out",		OUT},
		{ "passive",		PASSIVE},
		{ "peer",		PEER},
		{ "psk",		PSK},
		{ "quick",		QUICK},
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
	if (ulval >= SPI_RESERVED_MIN && ulval <= SPI_RESERVED_MAX) {
		yyerror("illegal SPI value");
		return (-1);
	}
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
		return (-1);
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
		err(1, "parsekey: calloc");

	key->len = len / 2;
	key->data = calloc(key->len, sizeof(u_int8_t));
	if (key->data == NULL)
		err(1, "parsekey: calloc");

	for (i = 0; i < (int)key->len; i++)
		key->data[i] = x2i(hexkey + 2 * i);

	return (key);
}

struct ipsec_key *
parsekeyfile(char *filename)
{
	struct stat	 sb;
	int		 fd;
	unsigned char	*hex;

	if (stat(filename, &sb) < 0)
		err(1, "parsekeyfile: stat %s", filename);
	if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0))
		errx(1, "parsekeyfile: key too %s", sb.st_size ? "large" :
		    "small");
	if ((hex = calloc(sb.st_size, sizeof(unsigned char)))
	    == NULL)
		err(1, "parsekeyfile: calloc");
	if ((fd = open(filename, O_RDONLY)) < 0)
		err(1, "parsekeyfile: open");
	if (read(fd, hex, sb.st_size) < sb.st_size)
		err(1, "parsekeyfile: read");
	close(fd);
	return (parsekey(hex, sb.st_size));
}

struct ipsec_addr_wrap *
host(const char *s)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	int			 mask, v4mask, cont = 1;
	char			*p, *q, *ps;

	if ((p = strrchr(s, '/')) != NULL) {
		mask = strtol(p + 1, &q, 0);
		if (!q || *q || mask > 32 || q == (p + 1))
			errx(1, "host: invalid netmask '%s'", p);
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "host: calloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
		v4mask = mask;
	} else {
		if ((ps = strdup(s)) == NULL)
			err(1, "host: strdup");
		v4mask = 32;
		mask = -1;
	}

#if notyet
	/* Does interface with this name exist? */
	if (cont && (ipa = host_if(ps, mask)) != NULL)
		cont = 0;
#endif

	/* IPv4 address? */
	if (cont && (ipa = host_v4(s, mask)) != NULL)
		cont = 0;

#if notyet
	/* IPv6 address? */
	if (cont && (ipa = host_dsn(ps, v4mask, 0)) != NULL)
		cont = 0;
#endif
	free(ps);

	if (ipa == NULL || cont == 1) {
		fprintf(stderr, "no IP address found for %s\n", s);
		return (NULL);
	}
	return (ipa);
}

struct ipsec_addr_wrap *
host_v4(const char *s, int mask)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	struct in_addr		 ina;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (NULL);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (NULL);
	}

	ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (ipa == NULL)
		err(1, "host_v4: calloc");

	ipa->address.v4 = ina;
	ipa->name = strdup(s);
	if (ipa->name == NULL)
		err(1, "host_v4: strdup");
	ipa->af = AF_INET;

	set_ipmask(ipa, bits);
	if (bits != (ipa->af == AF_INET ? 32 : 128))
		ipa->netaddress = 1;

	return (ipa);
}

void
set_ipmask(struct ipsec_addr_wrap *address, u_int8_t b)
{
	struct ipsec_addr 	*ipa;
	int			 i, j = 0;

	ipa = &address->mask;
	bzero(ipa, sizeof(struct ipsec_addr));

	while (b >= 32) {
		ipa->addr32[j++] = 0xffffffff;
		b -= 32;
	}
	for (i = 31; i > 31 - b; --i)
		ipa->addr32[j] |= (1 << i);
	if (b)
		ipa->addr32[j] = htonl(ipa->addr32[j]);
}

struct ipsec_addr_wrap *
copyhost(const struct ipsec_addr_wrap *src)
{
	struct ipsec_addr_wrap *dst;

	dst = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (dst == NULL)
		err(1, "copyhost: calloc");

	memcpy(dst, src, sizeof(struct ipsec_addr_wrap));

	if ((dst->name = strdup(src->name)) == NULL)
		err(1, "copyhost: strdup");
	
	return dst;
}

const struct ipsec_xf *
parse_xf(const char *name, const struct ipsec_xf xfs[])
{
	int		i;

	for (i = 0; xfs[i].name != NULL; i++) {
		if (strncmp(name, xfs[i].name, strlen(name)))
			continue;
		return &xfs[i];
	}
	return (NULL);
}

struct ipsec_transforms *
transforms(const char *authname, const char *encname, const char *compname)
{
	struct ipsec_transforms *xfs;

	xfs = calloc(1, sizeof(struct ipsec_transforms));
	if (xfs == NULL)
		err(1, "transforms: calloc");

	if (authname) {
		xfs->authxf = parse_xf(authname, authxfs);
		if (xfs->authxf == NULL)
			yyerror("%s not a valid transform", authname);
	}
	if (encname) {
		xfs->encxf = parse_xf(encname, encxfs);
		if (xfs->encxf == NULL)
			yyerror("%s not a valid transform", encname);
	}
	if (compname) {
		xfs->compxf = parse_xf(compname, compxfs);
		if (xfs->compxf == NULL)
			yyerror("%s not a valid transform", compname);
	}

	return (xfs);
}

struct ipsec_transforms *
copytransforms(const struct ipsec_transforms *xfs)
{
	struct ipsec_transforms *newxfs;

	if (xfs == NULL)
		return (NULL);

	newxfs = calloc(1, sizeof(struct ipsec_transforms));
	if (newxfs == NULL)
		err(1, "copytransforms: calloc");

	memcpy(newxfs, xfs, sizeof(struct ipsec_transforms));
	return (newxfs);
}

int
validate_sa(u_int32_t spi, u_int8_t protocol, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey)
{
	/* Sanity checks */
	if (spi == 0) {
		yyerror("no SPI specified");
		return (0);
	}
	if (protocol == IPSEC_AH) {
		if (!xfs) {
			yyerror("no transforms specified");
			return (0);
		}
		if (!xfs->authxf)
			xfs->authxf = &authxfs[AUTHXF_HMAC_SHA2_256];
		if (xfs->encxf) {
			yyerror("ah does not provide encryption");
			return (0);
		}
	}
	if (protocol == IPSEC_ESP) {
		if (!xfs) {
			yyerror("no transforms specified");
			return (0);
		}
		if (!xfs->authxf)
			xfs->authxf = &authxfs[AUTHXF_HMAC_SHA2_256];
		if (!xfs->encxf)
			xfs->encxf = &encxfs[ENCXF_AESCTR];
	}
	if (protocol == IPSEC_IPCOMP) {
		if (!xfs) {
			yyerror("no transform specified");
			return (0);
		}
		if (!xfs->compxf)
			xfs->compxf = &compxfs[COMPXF_DEFLATE];
	}
	if (protocol == IPSEC_TCPMD5 && authkey == NULL) {
		yyerror("authentication key needed for tcpmd5");
		return (0);
	}
	if (xfs && xfs->authxf) {
		if (!authkey) {
			yyerror("no authentication key specified");
			return (0);
		}
		if (authkey->len != xfs->authxf->keymin) {
			yyerror("wrong authentication key length, needs to be "
			    "%d bits", xfs->authxf->keymin * 8);
			return (0);
		}
	}
	if (xfs && xfs->encxf) {
		if (!enckey && xfs->encxf != &encxfs[ENCXF_NULL]) {
			yyerror("no encryption key specified");
			return (0);
		}
		if (enckey) {
		if (enckey->len < xfs->encxf->keymin) {
			yyerror("encryption key too short, minimum %d bits",
			    xfs->encxf->keymin * 8);
			return (0);
		}
		if (xfs->encxf->keymax < enckey->len) {
			yyerror("encryption key too long, maximum %d bits",
			    xfs->encxf->keymax * 8);
			return (0);
		}
		}
	}

	return 1;
}

struct ipsec_rule *
create_sa(u_int8_t protocol, struct ipsec_addr_wrap *src, struct
    ipsec_addr_wrap *dst, u_int32_t spi, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey)
{
	struct ipsec_rule *r;

	if (validate_sa(spi, protocol, xfs, authkey, enckey) == 0)
		return (NULL);

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_sa: calloc");

	r->type |= RULE_SA;
	r->proto = protocol;
	r->src = src;
	r->dst = dst;
	r->spi = spi;
	r->xfs = xfs;
	r->authkey = authkey;
	r->enckey = enckey;

	return r;
}

struct ipsec_rule *
reverse_sa(struct ipsec_rule *rule, u_int32_t spi, struct ipsec_key *authkey,
    struct ipsec_key *enckey)
{
	struct ipsec_rule *reverse;

	if (validate_sa(spi, rule->proto, rule->xfs, authkey, enckey) == 0)
		return (NULL);

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "reverse_sa: calloc");

	reverse->type |= RULE_SA;
	reverse->proto = rule->proto;
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	reverse->spi = spi;
	reverse->xfs = copytransforms(rule->xfs);
	reverse->authkey = authkey;
	reverse->enckey = enckey;

	return (reverse);
}

struct ipsec_rule *
create_flow(u_int8_t dir, struct ipsec_addr_wrap *src, struct ipsec_addr_wrap
    *dst, struct ipsec_addr_wrap *peer, u_int8_t proto, char *srcid, char
    *dstid, u_int16_t authtype)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_flow: calloc");
	
	r->type |= RULE_FLOW;

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
	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_flow: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->idtype = ID_FQDN;	/* XXX For now only FQDN. */
#ifdef notyet
	r->auth->type = authtype;
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
		err(1, "reverse_rule: calloc");

	reverse->type |= RULE_FLOW;
	
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

	reverse->auth = calloc(1, sizeof(struct ipsec_auth));
	if (reverse->auth == NULL)
		err(1, "reverse_rule: calloc");
	if (rule->auth->dstid && (reverse->auth->dstid =
	    strdup(rule->auth->dstid)) == NULL)
		err(1, "reverse_rule: strdup");
	if (rule->auth->srcid && (reverse->auth->srcid =
	    strdup(rule->auth->srcid)) == NULL)
		err(1, "reverse_rule: strdup");
	reverse->auth->idtype = rule->auth->idtype;
	reverse->auth->type = rule->auth->type;

	return reverse;
}

struct ipsec_rule *
create_ike(struct ipsec_addr_wrap *src, struct ipsec_addr_wrap *dst, struct
    ipsec_addr_wrap * peer, struct ipsec_transforms *mmxfs, struct
    ipsec_transforms *qmxfs, u_int8_t proto, u_int8_t mode, char *srcid, char
    *dstid)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_ike: calloc");

	r->type = RULE_IKE;

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
	r->ikemode = mode;
	r->mmxfs = mmxfs;
	r->qmxfs = qmxfs;
	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_ike: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->idtype = ID_FQDN;	/* XXX For now only FQDN. */

	return (r);

errout:
	free(r);
	if (srcid)
		free(srcid);
	if (dstid)
		free(dstid);
	free(src);
	free(dst);

	return (NULL);
}

/*	$OpenBSD: parse.y,v 1.29 2015/07/21 04:52:29 renato Exp $ */

/*
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if_types.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "ldp.h"
#include "ldpd.h"
#include "lde.h"
#include "ldpe.h"
#include "log.h"

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
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

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

void		 clear_config(struct ldpd_conf *xconf);
u_int32_t	 get_rtr_id(void);
int		 host(const char *, struct in_addr *, struct in_addr *);

static struct ldpd_conf	*conf;
static int			 errors = 0;

struct iface		*iface = NULL;
struct tnbr		*tnbr = NULL;
struct nbr_params	*nbrp = NULL;
struct l2vpn		*l2vpn = NULL;
struct l2vpn_pw		*pw = NULL;

struct config_defaults {
	u_int16_t	lhello_holdtime;
	u_int16_t	lhello_interval;
	u_int16_t	thello_holdtime;
	u_int16_t	thello_interval;
	u_int8_t	pwflags;
};

struct config_defaults	 globaldefs;
struct config_defaults	 ifacedefs;
struct config_defaults	 tnbrdefs;
struct config_defaults	 pwdefs;
struct config_defaults	*defs;

struct iface		*conf_get_if(struct kif *);
struct tnbr		*conf_get_tnbr(struct in_addr);
struct nbr_params	*conf_get_nbrp(struct in_addr);
struct l2vpn		*conf_get_l2vpn(char *);
struct l2vpn_if		*conf_get_l2vpn_if(struct l2vpn *, struct kif *);
struct l2vpn_pw		*conf_get_l2vpn_pw(struct l2vpn *, struct kif *);

typedef struct {
	union {
		int64_t		 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	INTERFACE TNEIGHBOR ROUTERID FIBUPDATE
%token	LHELLOHOLDTIME LHELLOINTERVAL
%token	THELLOHOLDTIME THELLOINTERVAL
%token	THELLOACCEPT
%token	KEEPALIVE
%token	NEIGHBOR PASSWORD
%token	L2VPN TYPE VPLS PWTYPE MTU BRIDGE
%token	ETHERNET ETHERNETTAGGED STATUSTLV CONTROLWORD
%token	PSEUDOWIRE NEIGHBOR PWID
%token	EXTTAG
%token	YES NO
%token	ERROR
%token	<v.string>	STRING
%token	<v.number>	NUMBER
%type	<v.number>	yesno
%type	<v.number>	l2vpn_type
%type	<v.number>	pw_type
%type	<v.string>	string

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar interface '\n'
		| grammar tneighbor '\n'
		| grammar neighbor '\n'
		| grammar l2vpn '\n'
		| grammar error '\n'		{ file->errors++; }
		;

string		: string STRING	{
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

yesno		: YES	{ $$ = 1; }
		| NO	{ $$ = 0; }
		;

l2vpn_type	: VPLS	{ $$ = L2VPN_TYPE_VPLS; }
		;

pw_type		: ETHERNET		{ $$ = PW_TYPE_ETHERNET; }
		| ETHERNETTAGGED	{ $$ = PW_TYPE_ETHERNET_TAGGED; }
		;

varset		: STRING '=' string {
			if (conf->opts & LDPD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

conf_main	: ROUTERID STRING {
			if (!inet_aton($2, &conf->rtr_id)) {
				yyerror("error parsing router-id");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| FIBUPDATE yesno {
			if ($2 == 0)
				conf->flags |= LDPD_FLAG_NO_FIB_UPDATE;
			else
				conf->flags &= ~LDPD_FLAG_NO_FIB_UPDATE;
		}
		| THELLOACCEPT yesno {
			if ($2 == 0)
				conf->flags &= ~LDPD_FLAG_TH_ACCEPT;
			else
				conf->flags |= LDPD_FLAG_TH_ACCEPT;
		}
		| KEEPALIVE NUMBER {
			if ($2 < MIN_KEEPALIVE ||
			    $2 > MAX_KEEPALIVE) {
				yyerror("keepalive out of range (%d-%d)",
				    MIN_KEEPALIVE, MAX_KEEPALIVE);
				YYERROR;
			}
			conf->keepalive = $2;
		}
		| iface_defaults
		| tnbr_defaults
		;

iface_defaults	: LHELLOHOLDTIME NUMBER {
			if ($2 < MIN_HOLDTIME ||
			    $2 > MAX_HOLDTIME) {
				yyerror("hello holdtime out of range (%d-%d)",
				    MIN_HOLDTIME, MAX_HOLDTIME);
				YYERROR;
			}
			defs->lhello_holdtime = $2;
		}
		| LHELLOINTERVAL NUMBER {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			defs->lhello_interval = $2;
		}
		;

tnbr_defaults	: THELLOHOLDTIME NUMBER {
			if ($2 < MIN_HOLDTIME ||
			    $2 > MAX_HOLDTIME) {
				yyerror("hello holdtime out of range (%d-%d)",
				    MIN_HOLDTIME, MAX_HOLDTIME);
				YYERROR;
			}
			conf->thello_holdtime = $2;
			defs->thello_holdtime = $2;
		}
		| THELLOINTERVAL NUMBER {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			conf->thello_interval = $2;
			defs->thello_interval = $2;
		}
		;

nbr_opts	: PASSWORD STRING {
			if (strlcpy(nbrp->auth.md5key, $2,
			    sizeof(nbrp->auth.md5key)) >=
			    sizeof(nbrp->auth.md5key)) {
				yyerror("tcp md5sig password too long: max %zu",
				    sizeof(nbrp->auth.md5key) - 1);
				free($2);
				YYERROR;
			}
			nbrp->auth.md5key_len = strlen($2);
			nbrp->auth.method = AUTH_MD5SIG;
			free($2);
		}
		;

pw_defaults	: STATUSTLV yesno {
			if ($2 == 1) {
				defs->pwflags |= F_PW_STATUSTLV_CONF;
				defs->pwflags |= F_PW_STATUSTLV;
			} else {
				defs->pwflags &= ~F_PW_STATUSTLV_CONF;
				defs->pwflags &= ~F_PW_STATUSTLV;
			}
		}
		| CONTROLWORD yesno {
			if ($2 == 1) {
				defs->pwflags |= F_PW_CONTROLWORD_CONF;
				defs->pwflags |= F_PW_CONTROLWORD;
			} else {
				defs->pwflags &= ~F_PW_CONTROLWORD_CONF;
				defs->pwflags &= ~F_PW_CONTROLWORD;
			}
		}
		;

pwopts		: PWID NUMBER {
			if ($2 < MIN_PWID_ID ||
			    $2 > MAX_PWID_ID) {
				yyerror("pw-id out of range (%d-%d)",
				    MIN_PWID_ID, MAX_PWID_ID);
				YYERROR;
			}

			pw->pwid = $2;
		}
		| NEIGHBOR STRING {
			struct in_addr	 addr;
			struct tnbr	*t;

			if (inet_aton($2, &addr) == 0) {
				yyerror(
				    "error parsing neighbor address");
				free($2);
				YYERROR;
			}
			free($2);

			pw->addr.s_addr = addr.s_addr;

			t = tnbr_find(conf, addr);
			if (t == NULL) {
				t = tnbr_new(conf, addr);
				LIST_INSERT_HEAD(&conf->tnbr_list, t, entry);
			}

			t->pw_count++;
		}
		| pw_defaults
		;

pseudowire	: PSEUDOWIRE STRING {
			struct kif	*kif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			if (kif->media_type != IFT_MPLSTUNNEL) {
				yyerror("unsupported interface type on "
				    "interface %s", kif->ifname);
				YYERROR;
			}

			pw = conf_get_l2vpn_pw(l2vpn, kif);
			if (pw == NULL)
				YYERROR;
			LIST_INSERT_HEAD(&l2vpn->pw_list, pw, entry);

			memcpy(&pwdefs, defs, sizeof(pwdefs));
			defs = &pwdefs;
		} pw_block {
			struct l2vpn	*l;
			struct l2vpn_pw *p;

			LIST_FOREACH(l, &conf->l2vpn_list, entry)
				LIST_FOREACH(p, &l->pw_list, entry)
					if (pw != p &&
					    pw->pwid == p->pwid &&
					    pw->addr.s_addr == p->addr.s_addr) {
						yyerror("pseudowire already "
						    "configured");
						YYERROR;
					}

			pw->flags = defs->pwflags;
			defs = &globaldefs;
			pw = NULL;
		}
		;

pw_block	: '{' optnl pwopts_l '}'
		| '{' optnl '}'
		| /* nothing */
		;

pwopts_l	: pwopts_l pwopts nl
		| pwopts optnl
		;

l2vpnopts	: PWTYPE pw_type {
			l2vpn->pw_type = $2;
		}
		| MTU NUMBER {
			if ($2 < MIN_L2VPN_MTU ||
			    $2 > MAX_L2VPN_MTU) {
				yyerror("l2vpn mtu out of range (%d-%d)",
				    MIN_L2VPN_MTU, MAX_L2VPN_MTU);
				YYERROR;
			}
			l2vpn->mtu = $2;
		}
		| pw_defaults
		| BRIDGE STRING {
			struct l2vpn	 *l;
			struct kif	 *kif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			if (l2vpn->br_ifindex != 0) {
				yyerror("bridge interface cannot be "
				    "redefined on l2vpn %s", l2vpn->name);
				YYERROR;
			}

			if (kif->media_type != IFT_BRIDGE) {
				yyerror("unsupported interface type on "
				    "interface %s", kif->ifname);
				YYERROR;
			}

			LIST_FOREACH(l, &conf->l2vpn_list, entry) {
				if (l->br_ifindex == kif->ifindex) {
					yyerror("bridge %s is already being "
					    "used by l2vpn %s", kif->ifname,
					    l->name);
					YYERROR;
				}
			}

			l2vpn->br_ifindex = kif->ifindex;
			strlcpy(l2vpn->br_ifname, kif->ifname,
			    sizeof(l2vpn->br_ifname));
		}
		| INTERFACE STRING {
			struct kif	*kif;
			struct l2vpn_if	*lif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			if (kif->media_type == IFT_BRIDGE
			    || kif->media_type == IFT_LOOP
			    || kif->media_type == IFT_CARP) {
				yyerror("unsupported interface type on "
				    "interface %s", kif->ifname);
				YYERROR;
			}

			lif = conf_get_l2vpn_if(l2vpn, kif);
			if (lif == NULL)
				YYERROR;
			LIST_INSERT_HEAD(&l2vpn->if_list, lif, entry);
		}
		| pseudowire
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

interface	: INTERFACE STRING	{
			struct kif	*kif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			iface = conf_get_if(kif);
			if (iface == NULL)
				YYERROR;
			if (iface->media_type == IFT_LOOP ||
			    iface->media_type == IFT_CARP ||
			    iface->media_type == IFT_MPLSTUNNEL) {
				yyerror("unsupported interface type on "
				    "interface %s", iface->name);
				YYERROR;
			}
			LIST_INSERT_HEAD(&conf->iface_list, iface, entry);

			memcpy(&ifacedefs, defs, sizeof(ifacedefs));
			defs = &ifacedefs;
		} interface_block {
			iface->hello_holdtime = defs->lhello_holdtime;
			iface->hello_interval = defs->lhello_interval;
			iface = NULL;

			defs = &globaldefs;
		}
		;

interface_block	: '{' optnl interfaceopts_l '}'
		| '{' optnl '}'
		| /* nothing */
		;

interfaceopts_l	: interfaceopts_l iface_defaults nl
		| iface_defaults optnl
		;

tneighbor	: TNEIGHBOR STRING	{
			struct in_addr	 addr;

			if (inet_aton($2, &addr) == 0) {
				yyerror(
				    "error parsing neighbor address");
				free($2);
				YYERROR;
			}
			free($2);

			tnbr = conf_get_tnbr(addr);
			if (tnbr == NULL)
				YYERROR;

			tnbr->flags |= F_TNBR_CONFIGURED;
			LIST_INSERT_HEAD(&conf->tnbr_list, tnbr, entry);

			memcpy(&tnbrdefs, defs, sizeof(tnbrdefs));
			defs = &tnbrdefs;
		} tneighbor_block {
			tnbr->hello_holdtime = defs->thello_holdtime;
			tnbr->hello_interval = defs->thello_interval;
			tnbr = NULL;

			defs = &globaldefs;
		}
		;

tneighbor_block	: '{' optnl tneighboropts_l '}'
		| '{' optnl '}'
		| /* nothing */
		;

tneighboropts_l	: tneighboropts_l tnbr_defaults nl
		| tnbr_defaults optnl
		;

neighbor	: NEIGHBOR STRING	{
			struct in_addr	 addr;

			if (inet_aton($2, &addr) == 0) {
				yyerror(
				    "error parsing neighbor address");
				free($2);
				YYERROR;
			}
			free($2);

			nbrp = conf_get_nbrp(addr);
			if (nbrp == NULL)
				YYERROR;
			LIST_INSERT_HEAD(&conf->nbrp_list, nbrp, entry);
		} neighbor_block {
			nbrp = NULL;
		}
		;

neighbor_block	: '{' optnl neighboropts_l '}'
		| '{' optnl '}'
		| /* nothing */
		;

neighboropts_l	: neighboropts_l nbr_opts nl
		| nbr_opts optnl
		;

l2vpn		: L2VPN STRING TYPE l2vpn_type {
			l2vpn = conf_get_l2vpn($2);
			if (l2vpn == NULL)
				YYERROR;
			l2vpn->type = $4;
			LIST_INSERT_HEAD(&conf->l2vpn_list, l2vpn, entry);
		} l2vpn_block {
			l2vpn = NULL;
		}
		;

l2vpn_block	: '{' optnl l2vpnopts_l '}'
		| '{' optnl '}'
		| /* nothing */
		;

l2vpnopts_l	: l2vpnopts_l l2vpnopts nl
		| l2vpnopts optnl
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
		{"bridge",			BRIDGE},
		{"control-word",		CONTROLWORD},
		{"ethernet",			ETHERNET},
		{"ethernet-tagged",		ETHERNETTAGGED},
		{"fib-update",			FIBUPDATE},
		{"interface",			INTERFACE},
		{"keepalive",			KEEPALIVE},
		{"l2vpn",			L2VPN},
		{"link-hello-holdtime",		LHELLOHOLDTIME},
		{"link-hello-interval",		LHELLOINTERVAL},
		{"mtu",				MTU},
		{"neighbor",			NEIGHBOR},
		{"no",				NO},
		{"password",			PASSWORD},
		{"pseudowire",			PSEUDOWIRE},
		{"pw-id",			PWID},
		{"pw-type",			PWTYPE},
		{"router-id",			ROUTERID},
		{"status-tlv",			STATUSTLV},
		{"targeted-hello-accept",	THELLOACCEPT},
		{"targeted-hello-holdtime",	THELLOHOLDTIME},
		{"targeted-hello-interval",	THELLOINTERVAL},
		{"targeted-neighbor",		TNEIGHBOR},
		{"type",			TYPE},
		{"vpls",			VPLS},
		{"yes",				YES}
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
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
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
	u_char	*p, *val;
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
				*p++ = c;
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
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(1, "yylex: strdup");
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
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "yylex: strdup");
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
	if (st.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO)) {
		log_warnx("%s: group writable or world read/writable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("strdup");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	} else if (secret &&
	    check_file_secrecy(fileno(nfile->stream), nfile->name)) {
		fclose(nfile->stream);
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

struct ldpd_conf *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct ldpd_conf))) == NULL)
		fatal("parse_config");
	conf->opts = opts;
	conf->keepalive = DEFAULT_KEEPALIVE;

	bzero(&globaldefs, sizeof(globaldefs));
	defs = &globaldefs;
	defs->lhello_holdtime = LINK_DFLT_HOLDTIME;
	defs->lhello_interval = DEFAULT_HELLO_INTERVAL;
	defs->thello_holdtime = TARGETED_DFLT_HOLDTIME;
	defs->thello_interval = DEFAULT_HELLO_INTERVAL;
	conf->thello_holdtime = TARGETED_DFLT_HOLDTIME;
	conf->thello_interval = DEFAULT_HELLO_INTERVAL;
	defs->pwflags |= F_PW_STATUSTLV_CONF;
	defs->pwflags |= F_PW_STATUSTLV;
	defs->pwflags |= F_PW_CONTROLWORD_CONF;
	defs->pwflags |= F_PW_CONTROLWORD;

	if ((file = pushfile(filename, !(conf->opts & LDPD_OPT_NOACTION))) == NULL) {
		free(conf);
		return (NULL);
	}
	topfile = file;

	LIST_INIT(&conf->iface_list);
	LIST_INIT(&conf->addr_list);
	LIST_INIT(&conf->tnbr_list);
	LIST_INIT(&conf->nbrp_list);
	LIST_INIT(&conf->l2vpn_list);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->opts & LDPD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	/* free global config defaults */
	if (errors) {
		clear_config(conf);
		return (NULL);
	}

	if (conf->rtr_id.s_addr == 0)
		conf->rtr_id.s_addr = get_rtr_id();

	return (conf);
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

	TAILQ_FOREACH(sym, &symhead, entry)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

struct iface *
conf_get_if(struct kif *kif)
{
	struct iface	*i;

	LIST_FOREACH(i, &conf->iface_list, entry) {
		if (i->ifindex == kif->ifindex) {
			yyerror("interface %s already configured",
			    kif->ifname);
			return (NULL);
		}
	}

	i = if_new(kif);

	return (i);
}

struct tnbr *
conf_get_tnbr(struct in_addr addr)
{
	struct tnbr	*t;

	t = tnbr_find(conf, addr);
	if (t && (t->flags & F_TNBR_CONFIGURED)) {
		yyerror("targeted neighbor %s already configured",
		    inet_ntoa(addr));
		return (NULL);
	}

	t = tnbr_new(conf, addr);

	return (t);
}

struct nbr_params *
conf_get_nbrp(struct in_addr addr)
{
	struct nbr_params	*n;

	LIST_FOREACH(n, &conf->nbrp_list, entry) {
		if (n->addr.s_addr == addr.s_addr) {
			yyerror("neighbor %s already configured",
			    inet_ntoa(addr));
			return (NULL);
		}
	}

	n = nbr_params_new(addr);

	return (n);
}

struct l2vpn *
conf_get_l2vpn(char *name)
{
	struct l2vpn	 *l;

	if (l2vpn_find(conf, name)) {
		yyerror("l2vpn %s already configured", name);
		return (NULL);
	}

	l = l2vpn_new(name);

	return (l);
}

struct l2vpn_if *
conf_get_l2vpn_if(struct l2vpn *l, struct kif *kif)
{
	struct iface	*i;
	struct l2vpn	*ltmp;

	LIST_FOREACH(i, &conf->iface_list, entry) {
		if (i->ifindex == kif->ifindex) {
			yyerror("interface %s already configured",
			    kif->ifname);
			return (NULL);
		}
	}

	LIST_FOREACH(ltmp, &conf->l2vpn_list, entry)
		if (l2vpn_if_find(ltmp, kif->ifindex)) {
			yyerror("interface %s is already being "
			    "used by l2vpn %s", kif->ifname, ltmp->name);
			return (NULL);
		}

	return (l2vpn_if_new(l, kif));
}

struct l2vpn_pw *
conf_get_l2vpn_pw(struct l2vpn *l, struct kif *kif)
{
	struct l2vpn	*ltmp;

	LIST_FOREACH(ltmp, &conf->l2vpn_list, entry) {
		if (l2vpn_pw_find(ltmp, kif->ifindex)) {
			yyerror("pseudowire %s is already being "
			    "used by l2vpn %s", kif->ifname, ltmp->name);
			return (NULL);
		}
	}

	return (l2vpn_pw_new(l, kif));
}

void
clear_config(struct ldpd_conf *xconf)
{
	struct iface		*i;
	struct tnbr		*t;
	struct nbr_params	*n;
	struct l2vpn		*l;
	struct l2vpn_if		*f;
	struct l2vpn_pw		*p;

	while ((i = LIST_FIRST(&xconf->iface_list)) != NULL) {
		LIST_REMOVE(i, entry);
		if_del(i);
	}

	while ((t = LIST_FIRST(&xconf->tnbr_list)) != NULL) {
		LIST_REMOVE(t, entry);
		tnbr_del(t);
	}

	while ((n = LIST_FIRST(&xconf->nbrp_list)) != NULL) {
		LIST_REMOVE(n, entry);
		free(n);
	}

	while ((l = LIST_FIRST(&xconf->l2vpn_list)) != NULL) {
		while ((f = LIST_FIRST(&l->if_list)) != NULL) {
			LIST_REMOVE(f, entry);
			free(f);
		}
		while ((p = LIST_FIRST(&l->pw_list)) != NULL) {
			LIST_REMOVE(p, entry);
			free(p);
		}
		LIST_REMOVE(l, entry);
		free(l);
	}

	free(xconf);
}

u_int32_t
get_rtr_id(void)
{
	struct ifaddrs		*ifap, *ifa;
	u_int32_t		 ip = 0, cur, localnet;

	localnet = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strncmp(ifa->ifa_name, "carp", 4) == 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		cur = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if ((cur & localnet) == localnet)	/* skip 127/8 */
			continue;
		if (cur > ip || ip == 0)
			ip = cur;
	}
	freeifaddrs(ifap);

	if (ip == 0)
		fatal("router-id is 0.0.0.0");

	return (ip);
}

int
host(const char *s, struct in_addr *addr, struct in_addr *mask)
{
	struct in_addr		 ina;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (0);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (0);
	}

	addr->s_addr = ina.s_addr;
	mask->s_addr = prefixlen2mask(bits);

	return (1);
}

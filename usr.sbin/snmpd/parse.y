/*	$OpenBSD: parse.y,v 1.62 2020/10/30 07:43:48 martijn Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>

#include "snmpd.h"
#include "mib.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	size_t			 ungetpos;
	size_t			 ungetsize;
	u_char			*ungetbuf;
	int			 eof_reached;
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
int		 igetc(void);
int		 lgetc(int);
void		 lungetc(int);
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

struct snmpd			*conf = NULL;
static int			 errors = 0;
static struct usmuser		*user = NULL;
static char			*snmpd_port = SNMPD_PORT;

int		 host(const char *, const char *, int,
		    struct sockaddr_storage *, int);
int		 listen_add(struct sockaddr_storage *, int);

typedef struct {
	union {
		int64_t		 number;
		char		*string;
		struct host	*host;
		struct timeval	 tv;
		struct ber_oid	*oid;
		struct {
			int		 type;
			void		*data;
			long long	 value;
		}		 data;
		enum usmauth	 auth;
		enum usmpriv	 enc;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	INCLUDE
%token  LISTEN ON
%token	SYSTEM CONTACT DESCR LOCATION NAME OBJECTID SERVICES RTFILTER
%token	READONLY READWRITE OCTETSTRING INTEGER COMMUNITY TRAP RECEIVER
%token	SECLEVEL NONE AUTH ENC USER AUTHKEY ENCKEY ERROR DISABLED
%token	HANDLE DEFAULT SRCADDR TCP UDP PFADDRFILTER PORT
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.string>	hostcmn
%type	<v.string>	srcaddr port
%type	<v.number>	optwrite yesno seclevel
%type	<v.data>	objtype cmd
%type	<v.oid>		oid hostoid trapoid
%type	<v.auth>	auth
%type	<v.enc>		enc

%%

grammar		: /* empty */
		| grammar include '\n'
		| grammar '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar system '\n'
		| grammar mib '\n'
		| grammar error '\n'		{ file->errors++; }
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2, 0)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
		}
		;

varset		: STRING '=' STRING	{
			char *s = $1;
			while (*s++) {
				if (isspace((unsigned char)*s)) {
					yyerror("macro name cannot contain "
					    "whitespace");
					free($1);
					free($3);
					YYERROR;
				}
			}
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

yesno		:  STRING			{
			if (!strcmp($1, "yes"))
				$$ = 1;
			else if (!strcmp($1, "no"))
				$$ = 0;
			else {
				yyerror("syntax error, "
				    "either yes or no expected");
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

main		: LISTEN ON listenproto
		| READONLY COMMUNITY STRING	{
			if (strlcpy(conf->sc_rdcommunity, $3,
			    sizeof(conf->sc_rdcommunity)) >=
			    sizeof(conf->sc_rdcommunity)) {
				yyerror("r/o community name too long");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| READWRITE COMMUNITY STRING	{
			if (strlcpy(conf->sc_rwcommunity, $3,
			    sizeof(conf->sc_rwcommunity)) >=
			    sizeof(conf->sc_rwcommunity)) {
				yyerror("r/w community name too long");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| READWRITE DISABLED {
			conf->sc_readonly = 1;
 		}
		| TRAP COMMUNITY STRING		{
			if (strlcpy(conf->sc_trcommunity, $3,
			    sizeof(conf->sc_trcommunity)) >=
			    sizeof(conf->sc_trcommunity)) {
				yyerror("trap community name too long");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| TRAP RECEIVER	host
		| TRAP HANDLE hostcmn trapoid cmd {
			struct trapcmd *cmd = $5.data;

			cmd->cmd_oid = $4;

			if (trapcmd_add(cmd) != 0) {
				free($4);
				free(cmd);
				yyerror("duplicate oid");
				YYERROR;
			}
			conf->sc_traphandler = 1;
		}
		| RTFILTER yesno		{
			if ($2 == 1)
				conf->sc_rtfilter = ROUTE_FILTER(RTM_NEWADDR) |
				    ROUTE_FILTER(RTM_DELADDR) |
				    ROUTE_FILTER(RTM_IFINFO) |
				    ROUTE_FILTER(RTM_IFANNOUNCE);
			else
				conf->sc_rtfilter = 0;
		}
		| PFADDRFILTER yesno		{
			conf->sc_pfaddrfilter = $2;
		}
		| SECLEVEL seclevel {
			conf->sc_min_seclevel = $2;
		}
		| USER STRING			{
			const char *errstr;
			user = usm_newuser($2, &errstr);
			if (user == NULL) {
				yyerror("%s", errstr);
				free($2);
				YYERROR;
			}
		} userspecs {
			const char *errstr;
			if (usm_checkuser(user, &errstr) < 0) {
				yyerror("%s", errstr);
				YYERROR;
			}
			user = NULL;
		}
		;

listenproto	: UDP listen_udp
		| TCP listen_tcp
		| listen_udp

listen_udp	: STRING port			{
			struct sockaddr_storage ss[16];
			int nhosts, i;

			nhosts = host($1, $2, SOCK_DGRAM, ss, nitems(ss));
			if (nhosts < 1) {
				yyerror("invalid address: %s", $1);
				free($1);
				if ($2 != snmpd_port)
					free($2);
				YYERROR;
			}
			if (nhosts > (int)nitems(ss))
				log_warn("%s:%s resolves to more than %zu hosts",
				    $1, $2, nitems(ss));

			free($1);
			if ($2 != snmpd_port)
				free($2);
			for (i = 0; i < nhosts; i++) {
				if (listen_add(&(ss[i]), SOCK_DGRAM) == -1) {
					yyerror("calloc");
					YYERROR;
				}
			}
		}

listen_tcp	: STRING port			{
			struct sockaddr_storage ss[16];
			int nhosts, i;

			nhosts = host($1, $2, SOCK_STREAM, ss, nitems(ss));
			if (nhosts < 1) {
				yyerror("invalid address: %s", $1);
				free($1);
				if ($2 != snmpd_port)
					free($2);
				YYERROR;
			}
			if (nhosts > (int)nitems(ss))
				log_warn("%s:%s resolves to more than %zu hosts",
				    $1, $2, nitems(ss));

			free($1);
			if ($2 != snmpd_port)
				free($2);
			for (i = 0; i < nhosts; i++) {
				if (listen_add(&(ss[i]), SOCK_STREAM) == -1) {
					yyerror("calloc");
					YYERROR;
				}
			}
		}

port		: /* empty */			{
			$$ = snmpd_port;
		}
		| PORT STRING			{
			$$ = $2;
		}
		| PORT NUMBER			{
			char *number;

			if ($2 > UINT16_MAX) {
				yyerror("port number too large");
				YYERROR;
			}
			if ($2 < 1) {
				yyerror("port number too small");
				YYERROR;
			}
			if (asprintf(&number, "%"PRId64, $2) == -1) {
				yyerror("malloc");
				YYERROR;
			}
			$$ = number;
		}
		;

system		: SYSTEM sysmib
		;

sysmib		: CONTACT STRING		{
			struct ber_oid	 o = OID(MIB_sysContact);
			mps_set(&o, $2, strlen($2));
		}
		| DESCR STRING			{
			struct ber_oid	 o = OID(MIB_sysDescr);
			mps_set(&o, $2, strlen($2));
		}
		| LOCATION STRING		{
			struct ber_oid	 o = OID(MIB_sysLocation);
			mps_set(&o, $2, strlen($2));
		}
		| NAME STRING			{
			struct ber_oid	 o = OID(MIB_sysName);
			mps_set(&o, $2, strlen($2));
		}
		| OBJECTID oid			{
			struct ber_oid	 o = OID(MIB_sysOID);
			mps_set(&o, $2, sizeof(struct ber_oid));
		}
		| SERVICES NUMBER		{
			struct ber_oid	 o = OID(MIB_sysServices);
			mps_set(&o, NULL, $2);
		}
		;

mib		: OBJECTID oid NAME STRING optwrite objtype	{
			struct oid	*oid;
			if ((oid = (struct oid *)
			    calloc(1, sizeof(*oid))) == NULL) {
				yyerror("calloc");
				free($2);
				free($6.data);
				YYERROR;
			}

			smi_oidlen($2);
			bcopy($2, &oid->o_id, sizeof(struct ber_oid));
			free($2);
			oid->o_name = $4;
			oid->o_data = $6.data;
			oid->o_val = $6.value;
			switch ($6.type) {
			case 1:
				oid->o_get = mps_getint;
				oid->o_set = mps_setint;
				break;
			case 2:
				oid->o_get = mps_getstr;
				oid->o_set = mps_setstr;
				break;
			}
			oid->o_flags = OID_RD|OID_DYNAMIC;
			if ($5)
				oid->o_flags |= OID_WR;

			if (smi_insert(oid) == -1) {
				yyerror("duplicate oid");
				free(oid->o_name);
				free(oid->o_data);
				YYERROR;
			}
		}
		;

objtype		: INTEGER NUMBER			{
			$$.type = 1;
			$$.data = NULL;
			$$.value = $2;
		}
		| OCTETSTRING STRING			{
			$$.type = 2;
			$$.data = $2;
			$$.value = strlen($2);
		}
		;

optwrite	: READONLY				{ $$ = 0; }
		| READWRITE				{ $$ = 1; }
		;

oid		: STRING				{
			struct ber_oid	*sysoid;
			if ((sysoid =
			    calloc(1, sizeof(*sysoid))) == NULL) {
				yyerror("calloc");
				free($1);
				YYERROR;
			}
			if (ober_string2oid($1, sysoid) == -1) {
				yyerror("invalid OID: %s", $1);
				free(sysoid);
				free($1);
				YYERROR;
			}
			free($1);
			$$ = sysoid;
		}
		;

trapoid		: oid					{ $$ = $1; }
		| DEFAULT				{
			struct ber_oid	*sysoid;
			if ((sysoid =
			    calloc(1, sizeof(*sysoid))) == NULL) {
				yyerror("calloc");
				YYERROR;
			}
			ober_string2oid("1.3", sysoid);
			$$ = sysoid;
		}
		;

hostoid		: /* empty */				{ $$ = NULL; }
		| OBJECTID oid				{ $$ = $2; }
		;

hostcmn		: /* empty */				{ $$ = NULL; }
		| COMMUNITY STRING			{ $$ = $2; }
		;

srcaddr		: /* empty */				{ $$ = NULL; }
		| SRCADDR STRING			{ $$ = $2; }
		;

hostdef		: STRING hostoid hostcmn srcaddr	{
			struct sockaddr_storage ss;
			struct trap_address *tr;

			if ((tr = calloc(1, sizeof(*tr))) == NULL) {
				yyerror("calloc");
				YYERROR;
			}

			if (host($1, SNMPD_TRAPPORT, SOCK_DGRAM, &ss, 1) <= 0) {
				yyerror("invalid host: %s", $1);
				free($1);
				free($2);
				free($3);
				free($4);
				free(tr);
				YYERROR;
			}
			free($1);
			memcpy(&(tr->ss), &ss, sizeof(ss));
			if ($4 != NULL) {
				if (host($1, "0", SOCK_DGRAM, &ss, 1) <= 0) {
					yyerror("invalid host: %s", $1);
					free($2);
					free($3);
					free($4);
					free(tr);
					YYERROR;
				}
				free($4);
				memcpy(&(tr->ss_local), &ss, sizeof(ss));
			}
			tr->sa_oid = $2;
			tr->sa_community = $3;
			TAILQ_INSERT_TAIL(&(conf->sc_trapreceivers), tr, entry);
		}
		;

hostlist	: /* empty */
		| hostlist comma hostdef
		;

host		: hostdef
		| '{' hostlist '}'
		;

comma		: /* empty */
		| ','
		;

seclevel	: NONE		{ $$ = 0; }
		| AUTH		{ $$ = SNMP_MSGFLAG_AUTH; }
		| ENC		{ $$ = SNMP_MSGFLAG_AUTH | SNMP_MSGFLAG_PRIV; }
		;

userspecs	: /* empty */
		| userspecs userspec
		;

userspec	: AUTHKEY STRING		{
			user->uu_authkey = $2;
		}
		| AUTH auth			{
			user->uu_auth = $2;
		}
		| ENCKEY STRING			{
			user->uu_privkey = $2;
		}
		| ENC enc			{
			user->uu_priv = $2;
		}
		;

auth		: STRING			{
			if (strcasecmp($1, "hmac-md5") == 0 ||
			    strcasecmp($1, "hmac-md5-96") == 0)
				$$ = AUTH_MD5;
			else if (strcasecmp($1, "hmac-sha1") == 0 ||
			     strcasecmp($1, "hmac-sha1-96") == 0)
				$$ = AUTH_SHA1;
			else if (strcasecmp($1, "hmac-sha224") == 0 ||
			    strcasecmp($1, "usmHMAC128SHA224AuthProtocol") == 0)
				$$ = AUTH_SHA224;
			else if (strcasecmp($1, "hmac-sha256") == 0 ||
			    strcasecmp($1, "usmHMAC192SHA256AuthProtocol") == 0)
				$$ = AUTH_SHA256;
			else if (strcasecmp($1, "hmac-sha384") == 0 ||
			    strcasecmp($1, "usmHMAC256SHA384AuthProtocol") == 0)
				$$ = AUTH_SHA384;
			else if (strcasecmp($1, "hmac-sha512") == 0 ||
			    strcasecmp($1, "usmHMAC384SHA512AuthProtocol") == 0)
				$$ = AUTH_SHA512;
			else {
				yyerror("syntax error, bad auth hmac");
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

enc		: STRING			{
			if (strcasecmp($1, "des") == 0 ||
			    strcasecmp($1, "cbc-des") == 0)
				$$ = PRIV_DES;
			else if (strcasecmp($1, "aes") == 0 ||
			    strcasecmp($1, "cfb128-aes-128") == 0)
				$$ = PRIV_AES;
			else {
				yyerror("syntax error, bad encryption cipher");
				free($1);
				YYERROR;
			}
			free($1);

		}
		;

cmd		: STRING		{
			struct trapcmd	*cmd;
			size_t		 span, limit;
			char		*pos, **args, **args2;
			int		 nargs = 32;		/* XXX */

			if ((cmd = calloc(1, sizeof(*cmd))) == NULL ||
			    (args = calloc(nargs, sizeof(char *))) == NULL) {
				free(cmd);
				free($1);
				YYERROR;
			}

			pos = $1;
			limit = strlen($1);

			while (pos < $1 + limit &&
			    (span = strcspn(pos, " \t")) != 0) {
				pos[span] = '\0';
				args[cmd->cmd_argc] = strdup(pos);
				if (args[cmd->cmd_argc] == NULL) {
					trapcmd_free(cmd);
					free(args);
					free($1);
					YYERROR;
				}
				cmd->cmd_argc++;
				if (cmd->cmd_argc >= nargs - 1) {
					nargs *= 2;
					args2 = calloc(nargs, sizeof(char *));
					if (args2 == NULL) {
						trapcmd_free(cmd);
						free(args);
						free($1);
						YYERROR;
					}
					args = args2;
				}
				pos += span + 1;
			}
			free($1);
			cmd->cmd_argv = args;
			$$.data = cmd;
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
		{ "auth",			AUTH },
		{ "authkey",			AUTHKEY },
		{ "community",			COMMUNITY },
		{ "contact",			CONTACT },
		{ "default",			DEFAULT },
		{ "description",		DESCR },
		{ "disabled",			DISABLED},
		{ "enc",			ENC },
		{ "enckey",			ENCKEY },
		{ "filter-pf-addresses",	PFADDRFILTER },
		{ "filter-routes",		RTFILTER },
		{ "handle",			HANDLE },
		{ "include",			INCLUDE },
		{ "integer",			INTEGER },
		{ "listen",			LISTEN },
		{ "location",			LOCATION },
		{ "name",			NAME },
		{ "none",			NONE },
		{ "oid",			OBJECTID },
		{ "on",				ON },
		{ "port",			PORT },
		{ "read-only",			READONLY },
		{ "read-write",			READWRITE },
		{ "receiver",			RECEIVER },
		{ "seclevel",			SECLEVEL },
		{ "services",			SERVICES },
		{ "source-address",		SRCADDR },
		{ "string",			OCTETSTRING },
		{ "system",			SYSTEM },
		{ "tcp",			TCP },
		{ "trap",			TRAP },
		{ "udp",			UDP },
		{ "user",			USER }
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define START_EXPAND	1
#define DONE_EXPAND	2

static int	expanding;

int
igetc(void)
{
	int	c;

	while (1) {
		if (file->ungetpos > 0)
			c = file->ungetbuf[--file->ungetpos];
		else c = getc(file->stream);

		if (c == START_EXPAND)
			expanding = 1;
		else if (c == DONE_EXPAND)
			expanding = 0;
		else
			break;
	}
	return (c);
}

int
lgetc(int quotec)
{
	int		c, next;

	if (quotec) {
		if ((c = igetc()) == EOF) {
			yyerror("reached end of file while parsing quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = igetc()) == '\\') {
		next = igetc();
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(file->stream);
		} while (c == '\t' || c == ' ');
		ungetc(c, file->stream);
		c = ' ';
	}

	if (c == EOF) {
		/*
		 * Fake EOL when hit EOF for the first time. This gets line
		 * count right if last line in included file is syntactically
		 * invalid and has no newline.
		 */
		if (file->eof_reached == 0) {
			file->eof_reached = 1;
			return ('\n');
		}
		while (c == EOF) {
			if (file == topfile || popfile() == EOF)
				return (EOF);
			c = igetc();
		}
	}
	return (c);
}

void
lungetc(int c)
{
	if (c == EOF)
		return;

	if (file->ungetpos >= file->ungetsize) {
		void *p = reallocarray(file->ungetbuf, file->ungetsize, 2);
		if (p == NULL)
			err(1, "%s", __func__);
		file->ungetbuf = p;
		file->ungetsize *= 2;
	}
	file->ungetbuf[file->ungetpos++] = c;
}

int
findeol(void)
{
	int	c;

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
	if (c == '$' && !expanding) {
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
		p = val + strlen(val) - 1;
		lungetc(DONE_EXPAND);
		while (p >= val) {
			lungetc(*p);
			p--;
		}
		lungetc(START_EXPAND);
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
				if (next == quotec || next == ' ' ||
				    next == '\t')
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
			err(1, "%s", __func__);
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((size_t)(p-buf) >= sizeof(buf)) {
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
			if ((size_t)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "%s", __func__);
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
		log_warn("%s", __func__);
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("%s", __func__);
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s: %s", __func__, nfile->name);
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
	nfile->lineno = TAILQ_EMPTY(&files) ? 1 : 0;
	nfile->ungetsize = 16;
	nfile->ungetbuf = malloc(nfile->ungetsize);
	if (nfile->ungetbuf == NULL) {
		log_warn("%s", __func__);
		fclose(nfile->stream);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
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
	free(file->ungetbuf);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

struct snmpd *
parse_config(const char *filename, u_int flags)
{
	struct sockaddr_storage ss;
	struct sym	*sym, *next;
	struct address	*h;
	int found;

	if ((conf = calloc(1, sizeof(*conf))) == NULL) {
		log_warn("%s", __func__);
		return (NULL);
	}

	conf->sc_flags = flags;
	conf->sc_confpath = filename;
	TAILQ_INIT(&conf->sc_addresses);
	strlcpy(conf->sc_rdcommunity, "public", SNMPD_MAXCOMMUNITYLEN);
	strlcpy(conf->sc_rwcommunity, "private", SNMPD_MAXCOMMUNITYLEN);
	strlcpy(conf->sc_trcommunity, "public", SNMPD_MAXCOMMUNITYLEN);
	TAILQ_INIT(&conf->sc_trapreceivers);

	if ((file = pushfile(filename, 0)) == NULL) {
		free(conf);
		return (NULL);
	}
	topfile = file;
	setservent(1);

	yyparse();
	errors = file->errors;
	popfile();

	endservent();

	/* Setup default listen addresses */
	if (TAILQ_EMPTY(&conf->sc_addresses)) {
		if (host("0.0.0.0", SNMPD_PORT, SOCK_DGRAM, &ss, 1) != 1)
			fatal("Unexpected resolving of 0.0.0.0");
		if (listen_add(&ss, SOCK_DGRAM) == -1)
			fatal("calloc");
		if (host("::", SNMPD_PORT, SOCK_DGRAM, &ss, 1) != 1)
			fatal("Unexpected resolving of ::");
		if (listen_add(&ss, SOCK_DGRAM) == -1)
			fatal("calloc");
	}
	if (conf->sc_traphandler) {
		found = 0;
		TAILQ_FOREACH(h, &conf->sc_addresses, entry) {
			if (h->type == SOCK_DGRAM)
				found = 1;
		}
		if (!found) {
			log_warnx("trap handler needs at least one "
			    "udp listen address");
			free(conf);
			return (NULL);
		}
	}

	/* Free macros and check which have not been used. */
	TAILQ_FOREACH_SAFE(sym, &symhead, entry, next) {
		if ((conf->sc_flags & SNMPD_F_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (errors) {
		free(conf);
		return (NULL);
	}

	return (conf);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0)
			break;
	}

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

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);
	sym = strndup(s, val - s);
	if (sym == NULL)
		errx(1, "%s: strndup", __func__);
	ret = symset(sym, val + 1, 1);
	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	}
	return (NULL);
}

int
host(const char *s, const char *port, int type, struct sockaddr_storage *ss,
    int max)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, i;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = type;
	error = getaddrinfo(s, port, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return 0;
	if (error) {
		log_warnx("Could not parse \"%s\": %s", s, gai_strerror(error));
		return -1;
	}

	for (i = 0, res = res0; res; res = res->ai_next, i++) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if (i >= max)
			continue;

		bcopy(res->ai_addr, &(ss[i]), res->ai_addrlen);
	}
	freeaddrinfo(res0);

	return i;
}

int
listen_add(struct sockaddr_storage *ss, int type)
{
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct address *h;

	if ((h = calloc(1, sizeof(*h))) == NULL)
		return -1;
	bcopy(ss, &(h->ss), sizeof(*ss));
	if (ss->ss_family == AF_INET) {
		sin = (struct sockaddr_in *)ss;
		h->port = ntohs(sin->sin_port);
	} else {
		sin6 = (struct sockaddr_in6*)ss;
		h->port = ntohs(sin6->sin6_port);
	}
	h->type = type;
	TAILQ_INSERT_TAIL(&(conf->sc_addresses), h, entry);

	return 0;
}

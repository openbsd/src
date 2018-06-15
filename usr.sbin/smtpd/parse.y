/*	$OpenBSD: parse.y,v 1.212 2018/06/15 08:57:17 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "ssl.h"
#include "log.h"

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
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 igetc(void);
int		 lgetc(int);
void		 lungetc(int);
int		 findeol(void);
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));

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

struct smtpd		*conf = NULL;
static int		 errors = 0;

struct filter_conf	*filter = NULL;
struct table		*table = NULL;
struct mta_limits	*limits;
static struct pki	*pki;
static struct ca	*sca;

struct dispatcher	*dispatcher;
struct rule		*rule;


enum listen_options {
	LO_FAMILY	= 0x000001,
	LO_PORT		= 0x000002,
	LO_SSL		= 0x000004,
	LO_FILTER      	= 0x000008,
	LO_PKI      	= 0x000010,
	LO_AUTH      	= 0x000020,
	LO_TAG      	= 0x000040,
	LO_HOSTNAME   	= 0x000080,
	LO_HOSTNAMES   	= 0x000100,
	LO_MASKSOURCE  	= 0x000200,
	LO_NODSN	= 0x000400,
	LO_SENDERS	= 0x000800,
	LO_RECEIVEDAUTH = 0x001000,
	LO_MASQUERADE	= 0x002000,
	LO_CA		= 0x010000
};

static struct listen_opts {
	char	       *ifx;
	int		family;
	in_port_t	port;
	uint16_t	ssl;
	char	       *filtername;
	char	       *pki;
	char	       *ca;
	uint16_t       	auth;
	struct table   *authtable;
	char	       *tag;
	char	       *hostname;
	struct table   *hostnametable;
	struct table   *sendertable;
	uint16_t	flags;

	uint32_t       	options;
} listen_opts;

static void	create_sock_listener(struct listen_opts *);
static void	create_if_listener(struct listen_opts *);
static void	config_listener(struct listener *, struct listen_opts *);
static int	host_v4(struct listen_opts *);
static int	host_v6(struct listen_opts *);
static int	host_dns(struct listen_opts *);
static int	interface(struct listen_opts *);

void		 set_local(const char *);
void		 set_localaddrs(struct table *);
int		 delaytonum(char *);
int		 is_if_in_group(const char *, const char *);

static int config_lo_filter(struct listen_opts *, char *);
static int config_lo_mask_source(struct listen_opts *);

typedef struct {
	union {
		int64_t		 number;
		struct table	*table;
		char		*string;
		struct host	*host;
		struct mailaddr	*maddr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	ACTION ALIAS ANY ARROW AUTH AUTH_OPTIONAL
%token	BACKUP BOUNCE
%token	CA CERT CIPHERS COMPRESSION
%token	DHE DOMAIN
%token	ENCRYPTION ERROR EXPAND_ONLY
%token	FILTER FOR FORWARD_ONLY FROM
%token	HELO HELO_SRC HOST HOSTNAME HOSTNAMES
%token	INCLUDE INET4 INET6
%token	JUNK
%token	KEY
%token	LIMIT LISTEN LMTP LOCAL
%token	MAIL_FROM MAILDIR MASK_SRC MASQUERADE MATCH MAX_MESSAGE_SIZE MAX_DEFERRED MBOX MDA MTA MX
%token	NODSN NOVERIFY
%token	ON
%token	PKI PORT
%token	QUEUE
%token	RCPT_TO RECIPIENT RECEIVEDAUTH RELAY REJECT
%token	SCHEDULER SENDER SENDERS SMTP SMTPS SOCKET SRC SUB_ADDR_DELIM
%token	TABLE TAG TAGGED TLS TLS_REQUIRE TO TTL
%token	USER USERBASE
%token	VERIFY VIRTUAL
%token	WARN_INTERVAL WRAPPER

%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.table>	table
%type	<v.number>	size negation
%type	<v.table>	tables tablenew tableref
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar bounce '\n'
		| grammar ca '\n'
		| grammar mda '\n'
		| grammar mta '\n'
		| grammar pki '\n'
		| grammar queue '\n'
		| grammar scheduler '\n'
		| grammar smtp '\n'
		| grammar listen '\n'
		| grammar table '\n'
		| grammar dispatcher '\n'
		| grammar match '\n'
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

varset		: STRING '=' STRING		{
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

comma		: ','
		| nl
		| /* empty */
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl
		;

negation	: '!'		{ $$ = 1; }
		| /* empty */	{ $$ = 0; }
		;

assign		: '=' | ARROW;


keyval		: STRING assign STRING		{
			table->t_type = T_HASH;
			table_add(table, $1, $3);
			free($1);
			free($3);
		}
		;

keyval_list	: keyval
		| keyval comma keyval_list
		;

stringel	: STRING			{
			table->t_type = T_LIST;
			table_add(table, $1, NULL);
			free($1);
		}
		;

string_list	: stringel
		| stringel comma string_list
		;

tableval_list	: string_list			{ }
		| keyval_list			{ }
		;

bounce:
BOUNCE WARN_INTERVAL {
	memset(conf->sc_bounce_warn, 0, sizeof conf->sc_bounce_warn);
} bouncedelays
;


ca:
CA STRING {
	char buf[HOST_NAME_MAX+1];

	/* if not catchall, check that it is a valid domain */
	if (strcmp($2, "*") != 0) {
		if (!res_hnok($2)) {
			yyerror("not a valid domain name: %s", $2);
			free($2);
			YYERROR;
		}
	}
	xlowercase(buf, $2, sizeof(buf));
	free($2);
	sca = dict_get(conf->sc_ca_dict, buf);
	if (sca == NULL) {
		sca = xcalloc(1, sizeof *sca);
		(void)strlcpy(sca->ca_name, buf, sizeof(sca->ca_name));
		dict_set(conf->sc_ca_dict, sca->ca_name, sca);
	}
} ca_params
;


ca_params_opt:
CERT STRING {
	sca->ca_cert_file = $2;
}
;

ca_params:
ca_params_opt
;


mda:
MDA LIMIT limits_mda
| MDA WRAPPER STRING STRING {
	if (dict_get(conf->sc_mda_wrappers, $3)) {
		yyerror("mda wrapper already declared with that name: %s", $3);
		YYERROR;
	}
	dict_set(conf->sc_mda_wrappers, $3, $4);
}
;


mta:
MTA MAX_DEFERRED NUMBER  {
	conf->sc_mta_max_deferred = $3;
}
| MTA LIMIT FOR DOMAIN STRING {
	struct mta_limits	*d;

	limits = dict_get(conf->sc_limits_dict, $5);
	if (limits == NULL) {
		limits = xcalloc(1, sizeof(*limits));
		dict_xset(conf->sc_limits_dict, $5, limits);
		d = dict_xget(conf->sc_limits_dict, "default");
		memmove(limits, d, sizeof(*limits));
	}
	free($5);
} limits_mta
| MTA LIMIT {
	limits = dict_get(conf->sc_limits_dict, "default");
} limits_mta
;


pki:
PKI STRING {
	char buf[HOST_NAME_MAX+1];

	/* if not catchall, check that it is a valid domain */
	if (strcmp($2, "*") != 0) {
		if (!res_hnok($2)) {
			yyerror("not a valid domain name: %s", $2);
			free($2);
			YYERROR;
		}
	}
	xlowercase(buf, $2, sizeof(buf));
	free($2);
	pki = dict_get(conf->sc_pki_dict, buf);
	if (pki == NULL) {
		pki = xcalloc(1, sizeof *pki);
		(void)strlcpy(pki->pki_name, buf, sizeof(pki->pki_name));
		dict_set(conf->sc_pki_dict, pki->pki_name, pki);
	}
} pki_params
;
 
pki_params_opt:
CERT STRING {
	pki->pki_cert_file = $2;
}
| KEY STRING {
	pki->pki_key_file = $2;
}
| DHE STRING {
	if (strcasecmp($2, "none") == 0)
		pki->pki_dhe = 0;
	else if (strcasecmp($2, "auto") == 0)
		pki->pki_dhe = 1;
	else if (strcasecmp($2, "legacy") == 0)
		pki->pki_dhe = 2;
	else {
		yyerror("invalid DHE keyword: %s", $2);
		free($2);
		YYERROR;
	}
	free($2);
}
;


pki_params:
pki_params_opt pki_params
| /* empty */
;


queue:
QUEUE COMPRESSION {
	conf->sc_queue_flags |= QUEUE_COMPRESSION;
}
| QUEUE ENCRYPTION {
	conf->sc_queue_flags |= QUEUE_ENCRYPTION;
}
| QUEUE ENCRYPTION STRING {
	if (strcasecmp($3, "stdin") == 0 || strcasecmp($3, "-") == 0) {
		conf->sc_queue_key = "stdin";
		free($3);
	}
	else
		conf->sc_queue_key = $3;
	conf->sc_queue_flags |= QUEUE_ENCRYPTION;
}
| QUEUE TTL STRING {
	conf->sc_ttl = delaytonum($3);
	if (conf->sc_ttl == -1) {
		yyerror("invalid ttl delay: %s", $3);
		free($3);
		YYERROR;
	}
	free($3);
}
;


scheduler:
SCHEDULER LIMIT limits_scheduler
;


smtp:
SMTP LIMIT limits_smtp
| SMTP CIPHERS STRING {
	conf->sc_tls_ciphers = $3;
}
| SMTP MAX_MESSAGE_SIZE size {
	conf->sc_maxsize = $3;
}
| SMTP SUB_ADDR_DELIM STRING {
	if (strlen($3) != 1) {
		yyerror("subaddressing-delimiter must be one character");
		free($3);
		YYERROR;
	}
	if (isspace((int)*$3) ||  !isprint((int)*$3) || *$3== '@') {
		yyerror("sub-addr-delim uses invalid character");
		free($3);
		YYERROR;
	}
	conf->sc_subaddressing_delim = $3;
}
;


dispatcher_local_option:
USER STRING {
	if (dispatcher->u.local.requires_root) {
		yyerror("user may not be specified for this dispatcher");
		YYERROR;
	}

	if (dispatcher->u.local.forward_only) {
		yyerror("user may not be specified for forward-only");
		YYERROR;
	}

	if (dispatcher->u.local.expand_only) {
		yyerror("user may not be specified for expand-only");
		YYERROR;
	}

	if (dispatcher->u.local.user) {
		yyerror("user already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.local.user = $2;
}
| ALIAS tables {
	struct table   *t = $2;

	if (dispatcher->u.local.table_alias) {
		yyerror("alias mapping already specified for this dispatcher");
		YYERROR;
	}

	if (dispatcher->u.local.table_virtual) {
		yyerror("virtual mapping already specified for this dispatcher");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_HASH, K_ALIAS)) {
		yyerror("table \"%s\" may not be used for alias lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.local.table_alias = strdup(t->t_name);
}
| VIRTUAL tables {
	struct table   *t = $2;

	if (dispatcher->u.local.table_virtual) {
		yyerror("virtual mapping already specified for this dispatcher");
		YYERROR;
	}

	if (dispatcher->u.local.table_alias) {
		yyerror("alias mapping already specified for this dispatcher");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_HASH, K_ALIAS)) {
		yyerror("table \"%s\" may not be used for virtual lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.local.table_virtual = strdup(t->t_name);
}
| USERBASE tables {
	struct table   *t = $2;

	if (dispatcher->u.local.table_userbase) {
		yyerror("userbase mapping already specified for this dispatcher");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_HASH, K_USERINFO)) {
		yyerror("table \"%s\" may not be used for userbase lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.local.table_userbase = strdup(t->t_name);
}
| WRAPPER STRING {
	if (! dict_get(conf->sc_mda_wrappers, $2)) {
		yyerror("no mda wrapper with that name: %s", $2);
		YYERROR;
	}
	dispatcher->u.local.mda_wrapper = $2;
}
;

dispatcher_local_options:
dispatcher_local_option dispatcher_local_options
| /* empty */
;

dispatcher_local:
MBOX {
	dispatcher->u.local.requires_root = 1;
	dispatcher->u.local.user = xstrdup("root");
	asprintf(&dispatcher->u.local.command, "/usr/libexec/mail.local -f %%{mbox.from} %%{user.username}");
} dispatcher_local_options
| MAILDIR {
	asprintf(&dispatcher->u.local.command, "/usr/libexec/mail.maildir");
} dispatcher_local_options
| MAILDIR JUNK {
	asprintf(&dispatcher->u.local.command, "/usr/libexec/mail.maildir -j");
} dispatcher_local_options
| MAILDIR STRING {
	if (strncmp($2, "~/", 2) == 0)
		asprintf(&dispatcher->u.local.command,
		    "/usr/libexec/mail.maildir \"%%{user.directory}/%s\"", $2+2);
	else
		asprintf(&dispatcher->u.local.command,
		    "/usr/libexec/mail.maildir \"%s\"", $2);
} dispatcher_local_options
| MAILDIR STRING JUNK {
	if (strncmp($2, "~/", 2) == 0)
		asprintf(&dispatcher->u.local.command,
		    "/usr/libexec/mail.maildir -j \"%%{user.directory}/%s\"", $2+2);
	else
		asprintf(&dispatcher->u.local.command,
		    "/usr/libexec/mail.maildir -j \"%s\"", $2);
} dispatcher_local_options
| LMTP STRING {
	asprintf(&dispatcher->u.local.command,
	    "/usr/libexec/mail.lmtp -f \"%%{sender}\" -d %s %%{user.username}", $2);
} dispatcher_local_options
| LMTP STRING RCPT_TO {
	asprintf(&dispatcher->u.local.command,
	    "/usr/libexec/mail.lmtp -f \"%%{sender}\" -d %s %%{dest}", $2);
} dispatcher_local_options
| MDA STRING {
	asprintf(&dispatcher->u.local.command,
	    "/usr/libexec/mail.mda \"%s\"", $2);
} dispatcher_local_options
| FORWARD_ONLY {
	dispatcher->u.local.forward_only = 1;
} dispatcher_local_options
| EXPAND_ONLY {
	dispatcher->u.local.expand_only = 1;
} dispatcher_local_options

;

dispatcher_remote_option:
HELO STRING {
	if (dispatcher->u.remote.helo) {
		yyerror("helo already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.remote.helo = $2;
}
| HELO_SRC tables {
	struct table   *t = $2;

	if (dispatcher->u.remote.helo_source) {
		yyerror("helo-source mapping already specified for this dispatcher");
		YYERROR;
	}
	if (!table_check_use(t, T_DYNAMIC|T_HASH, K_ADDRNAME)) {
		yyerror("table \"%s\" may not be used for helo-source lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.remote.helo_source = strdup(t->t_name);
}
| PKI STRING {
	if (dispatcher->u.remote.pki) {
		yyerror("pki already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.remote.pki = $2;
}
| CA STRING {
	if (dispatcher->u.remote.ca) {
		yyerror("ca already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.remote.ca = $2;
}
| SRC tables {
	struct table   *t = $2;

	if (dispatcher->u.remote.source) {
		yyerror("source mapping already specified for this dispatcher");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_SOURCE)) {
		yyerror("table \"%s\" may not be used for source lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.remote.source = strdup(t->t_name);
}
| MAIL_FROM STRING {
	if (dispatcher->u.remote.mail_from) {
		yyerror("mail-from already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.remote.mail_from = $2;
}
| BACKUP MX STRING {
	if (dispatcher->u.remote.backup) {
		yyerror("backup already specified for this dispatcher");
		YYERROR;
	}
	if (dispatcher->u.remote.smarthost) {
		yyerror("backup and host are mutually exclusive");
		YYERROR;
	}

	dispatcher->u.remote.backup = 1;
	dispatcher->u.remote.backupmx = $3;
}
| BACKUP {
	if (dispatcher->u.remote.backup) {
		yyerror("backup already specified for this dispatcher");
		YYERROR;
	}
	if (dispatcher->u.remote.smarthost) {
		yyerror("backup and host are mutually exclusive");
		YYERROR;
	}

	dispatcher->u.remote.backup = 1;
}
| HOST tables {
	struct table   *t = $2;

	if (dispatcher->u.remote.smarthost) {
		yyerror("host mapping already specified for this dispatcher");
		YYERROR;
	}
	if (dispatcher->u.remote.backup) {
		yyerror("backup and host are mutually exclusive");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_RELAYHOST)) {
		yyerror("table \"%s\" may not be used for host lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.remote.smarthost = strdup(t->t_name);
}
| TLS NOVERIFY {
	if (dispatcher->u.remote.smarthost == NULL) {
		yyerror("tls no-verify may not be specified without host on a dispatcher");
		YYERROR;
	}

	if (dispatcher->u.remote.tls_noverify == 1) {
		yyerror("tls no-verify already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->u.remote.tls_noverify = 1;
}
| AUTH tables {
	struct table   *t = $2;

	if (dispatcher->u.remote.smarthost == NULL) {
		yyerror("auth may not be specified without host on a dispatcher");
		YYERROR;
	}

	if (dispatcher->u.remote.auth) {
		yyerror("auth mapping already specified for this dispatcher");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_HASH, K_CREDENTIALS)) {
		yyerror("table \"%s\" may not be used for auth lookups",
		    t->t_name);
		YYERROR;
	}

	dispatcher->u.remote.auth = strdup(t->t_name);
}
;

dispatcher_remote_options:
dispatcher_remote_option dispatcher_remote_options
| /* empty */
;

dispatcher_remote :
RELAY dispatcher_remote_options
;

dispatcher_type:
dispatcher_local {
	dispatcher->type = DISPATCHER_LOCAL;
}
| dispatcher_remote {
	dispatcher->type = DISPATCHER_REMOTE;
}
;

dispatcher_option:
TTL STRING {
	if (dispatcher->ttl) {
		yyerror("ttl already specified for this dispatcher");
		YYERROR;
	}

	dispatcher->ttl = delaytonum($2);
	if (dispatcher->ttl == -1) {
		yyerror("ttl delay \"%s\" is invalid", $2);
		free($2);
		YYERROR;
	}
	free($2);
}
;

dispatcher_options:
dispatcher_option dispatcher_options
| /* empty */
;

dispatcher:
ACTION STRING {
	if (dict_get(conf->sc_dispatchers, $2)) {
		yyerror("dispatcher already declared with that name: %s", $2);
		YYERROR;
	}
	dispatcher = xcalloc(1, sizeof *dispatcher);
} dispatcher_type dispatcher_options {
	if (dispatcher->type == DISPATCHER_LOCAL)
		if (dispatcher->u.local.table_userbase == NULL)
			dispatcher->u.local.table_userbase = "<getpwnam>";
	dict_set(conf->sc_dispatchers, $2, dispatcher);
	dispatcher = NULL;
}
;

match_option:
negation TAG tables {
	struct table   *t = $3;

	if (rule->flag_tag) {
		yyerror("tag already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_STRING)) {
		yyerror("table \"%s\" may not be used for tag lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_tag = $1 ? -1 : 1;
	rule->table_tag = strdup(t->t_name);
}
| negation HELO tables {
	struct table   *t = $3;

	if (rule->flag_smtp_helo) {
		yyerror("mail-helo already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_DOMAIN)) {
		yyerror("table \"%s\" may not be used for helo lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_smtp_helo = $1 ? -1 : 1;
	rule->table_smtp_helo = strdup(t->t_name);
}
| negation TLS {
	if (rule->flag_smtp_starttls) {
		yyerror("tls already specified for this rule");
		YYERROR;
	}
	rule->flag_smtp_starttls = $1 ? -1 : 1;
}
| negation AUTH {
	if (rule->flag_smtp_auth) {
		yyerror("auth already specified for this rule");
		YYERROR;
	}
	rule->flag_smtp_auth = $1 ? -1 : 1;
}
| negation AUTH tables {
	struct table   *t = $3;

	if (rule->flag_smtp_auth) {
		yyerror("auth already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_CREDENTIALS)) {
		yyerror("table \"%s\" may not be used for auth lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_smtp_auth = $1 ? -1 : 1;
	rule->table_smtp_auth = strdup(t->t_name);
}
| negation MAIL_FROM tables {
	struct table   *t = $3;

	if (rule->flag_smtp_mail_from) {
		yyerror("mail-from already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_MAILADDR)) {
		yyerror("table \"%s\" may not be used for mail-from lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_smtp_mail_from = $1 ? -1 : 1;
	rule->table_smtp_mail_from = strdup(t->t_name);
}
| negation RCPT_TO tables {
	struct table   *t = $3;

	if (rule->flag_smtp_rcpt_to) {
		yyerror("rcpt-to already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_MAILADDR)) {
		yyerror("table \"%s\" may not be used for rcpt-to lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_smtp_rcpt_to = $1 ? -1 : 1;
	rule->table_smtp_rcpt_to = strdup(t->t_name);
}

| negation FROM SOCKET {
	if (rule->flag_from) {
		yyerror("from already specified for this rule");
		YYERROR;
	}
	rule->flag_from = $1 ? -1 : 1;
	rule->flag_from_socket = 1;
}
| negation FROM LOCAL {
	struct table	*t = table_find("<localhost>", NULL);

	if (rule->flag_from) {
		yyerror("from already specified for this rule");
		YYERROR;
	}
	rule->flag_from = $1 ? -1 : 1;
	rule->table_from = strdup(t->t_name);
}
| negation FROM ANY {
	struct table	*t = table_find("<anyhost>", NULL);

	if (rule->flag_from) {
		yyerror("from already specified for this rule");
		YYERROR;
	}
	rule->flag_from = $1 ? -1 : 1;
	rule->table_from = strdup(t->t_name);
}
| negation FROM SRC tables {
	struct table   *t = $4;

	if (rule->flag_from) {
		yyerror("from already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_NETADDR)) {
		yyerror("table \"%s\" may not be used for from lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_from = $1 ? -1 : 1;
	rule->table_from = strdup(t->t_name);
}

| negation FOR LOCAL {
	struct table   *t = table_find("<localnames>", NULL);

	if (rule->flag_for) {
		yyerror("for already specified for this rule");
		YYERROR;
	}
	rule->flag_for = $1 ? -1 : 1;
	rule->table_for = strdup(t->t_name);
}
| negation FOR ANY {
	struct table   *t = table_find("<anydestination>", NULL);

	if (rule->flag_for) {
		yyerror("for already specified for this rule");
		YYERROR;
	}
	rule->flag_for = $1 ? -1 : 1;
	rule->table_for = strdup(t->t_name);
}
| negation FOR DOMAIN tables {
	struct table   *t = $4;

	if (rule->flag_for) {
		yyerror("for already specified for this rule");
		YYERROR;
	}

	if (!table_check_use(t, T_DYNAMIC|T_LIST, K_DOMAIN)) {
		yyerror("table \"%s\" may not be used for 'for' lookups",
		    t->t_name);
		YYERROR;
	}

	rule->flag_for = $1 ? -1 : 1;
	rule->table_for = strdup(t->t_name);
}
;

match_options:
match_option match_options
| /* empty */
;

match_dispatcher:
STRING {
	if (dict_get(conf->sc_dispatchers, $1) == NULL) {
		yyerror("no such dispatcher: %s", $1);
		YYERROR;
	}
	rule->dispatcher = $1;
}
;

action:
REJECT {
	rule->reject = 1;
}
| ACTION match_dispatcher
;

match:
MATCH {
	rule = xcalloc(1, sizeof *rule);
} match_options action {
	if (!rule->flag_from) {
		rule->table_from = strdup("<localhost>");
		rule->flag_from = 1;
	}
	if (!rule->flag_for) {
		rule->table_for = strdup("<localnames>");
		rule->flag_for = 1;
	}
	TAILQ_INSERT_TAIL(conf->sc_rules, rule, r_entry);
	rule = NULL;
}
;

size		: NUMBER		{
			if ($1 < 0) {
				yyerror("invalid size: %" PRId64, $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
			long long result;

			if (scan_scaled($1, &result) == -1 || result < 0) {
				yyerror("invalid size: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			$$ = result;
		}
		;

bouncedelay	: STRING {
			time_t	d;
			int	i;

			d = delaytonum($1);
			if (d < 0) {
				yyerror("invalid bounce delay: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			for (i = 0; i < MAX_BOUNCE_WARN; i++) {
				if (conf->sc_bounce_warn[i] != 0)
					continue;
				conf->sc_bounce_warn[i] = d;
				break;
			}
		}
		;

bouncedelays	: bouncedelays ',' bouncedelay
		| bouncedelay
		;

opt_limit_mda	: STRING NUMBER {
			if (!strcmp($1, "max-session")) {
				conf->sc_mda_max_session = $2;
			}
			else if (!strcmp($1, "max-session-per-user")) {
				conf->sc_mda_max_user_session = $2;
			}
			else if (!strcmp($1, "task-lowat")) {
				conf->sc_mda_task_lowat = $2;
			}
			else if (!strcmp($1, "task-hiwat")) {
				conf->sc_mda_task_hiwat = $2;
			}
			else if (!strcmp($1, "task-release")) {
				conf->sc_mda_task_release = $2;
			}
			else {
				yyerror("invalid scheduler limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_smtp	: opt_limit_smtp limits_smtp
		| /* empty */
		;

opt_limit_smtp : STRING NUMBER {
			if (!strcmp($1, "max-rcpt")) {
				conf->sc_session_max_rcpt = $2;
			}
			else if (!strcmp($1, "max-mails")) {
				conf->sc_session_max_mails = $2;
			}
			else {
				yyerror("invalid session limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_mda	: opt_limit_mda limits_mda
		| /* empty */
		;

opt_limit_mta	: INET4 {
			limits->family = AF_INET;
		}
		| INET6 {
			limits->family = AF_INET6;
		}
		| STRING NUMBER {
			if (!limit_mta_set(limits, $1, $2)) {
				yyerror("invalid mta limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_mta	: opt_limit_mta limits_mta
		| /* empty */
		;

opt_limit_scheduler : STRING NUMBER {
			if (!strcmp($1, "max-inflight")) {
				conf->sc_scheduler_max_inflight = $2;
			}
			else if (!strcmp($1, "max-evp-batch-size")) {
				conf->sc_scheduler_max_evp_batch_size = $2;
			}
			else if (!strcmp($1, "max-msg-batch-size")) {
				conf->sc_scheduler_max_msg_batch_size = $2;
			}
			else if (!strcmp($1, "max-schedule")) {
				conf->sc_scheduler_max_schedule = $2;
			}
			else {
				yyerror("invalid scheduler limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_scheduler: opt_limit_scheduler limits_scheduler
		| /* empty */
		;


opt_sock_listen : FILTER STRING {
			if (config_lo_filter(&listen_opts, $2)) {
				YYERROR;
			}
		}
		| MASK_SRC {
			if (config_lo_mask_source(&listen_opts)) {
				YYERROR;
			}
		}
		;

opt_if_listen : INET4 {
			if (listen_opts.options & LO_FAMILY) {
				yyerror("address family already specified");
				YYERROR;
			}
			listen_opts.options |= LO_FAMILY;
			listen_opts.family = AF_INET;
		}
		| INET6			{
			if (listen_opts.options & LO_FAMILY) {
				yyerror("address family already specified");
				YYERROR;
			}
			listen_opts.options |= LO_FAMILY;
			listen_opts.family = AF_INET6;
		}
		| PORT STRING			{
			struct servent	*servent;

			if (listen_opts.options & LO_PORT) {
				yyerror("port already specified");
				YYERROR;
			}
			listen_opts.options |= LO_PORT;

			servent = getservbyname($2, "tcp");
			if (servent == NULL) {
				yyerror("invalid port: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			listen_opts.port = ntohs(servent->s_port);
		}
		| PORT NUMBER			{
			if (listen_opts.options & LO_PORT) {
				yyerror("port already specified");
				YYERROR;
			}
			listen_opts.options |= LO_PORT;

			if ($2 <= 0 || $2 > (int)USHRT_MAX) {
				yyerror("invalid port: %" PRId64, $2);
				YYERROR;
			}
			listen_opts.port = $2;
		}
		| FILTER STRING			{
			if (config_lo_filter(&listen_opts, $2)) {
				YYERROR;
			}
		}
		| SMTPS				{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_SMTPS;
		}
		| SMTPS VERIFY 			{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_SMTPS|F_TLS_VERIFY;
		}
		| TLS				{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS;
		}
		| TLS_REQUIRE			{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS|F_STARTTLS_REQUIRE;
		}
		| TLS_REQUIRE VERIFY   		{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS|F_STARTTLS_REQUIRE|F_TLS_VERIFY;
		}
		| PKI STRING			{
			if (listen_opts.options & LO_PKI) {
				yyerror("pki already specified");
				YYERROR;
			}
			listen_opts.options |= LO_PKI;
			listen_opts.pki = $2;
		}
		| CA STRING			{
			if (listen_opts.options & LO_CA) {
				yyerror("ca already specified");
				YYERROR;
			}
			listen_opts.options |= LO_CA;
			listen_opts.ca = $2;
		}
		| AUTH				{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.auth = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL			{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.auth = F_AUTH;
		}
		| AUTH tables  			{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.authtable = $2;
			listen_opts.auth = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL tables 		{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.authtable = $2;
			listen_opts.auth = F_AUTH;
		}
		| TAG STRING			{
			if (listen_opts.options & LO_TAG) {
				yyerror("tag already specified");
				YYERROR;
			}
			listen_opts.options |= LO_TAG;

			if (strlen($2) >= SMTPD_TAG_SIZE) {
       				yyerror("tag name too long");
				free($2);
				YYERROR;
			}
			listen_opts.tag = $2;
		}
		| HOSTNAME STRING	{
			if (listen_opts.options & LO_HOSTNAME) {
				yyerror("hostname already specified");
				YYERROR;
			}
			listen_opts.options |= LO_HOSTNAME;

			listen_opts.hostname = $2;
		}
		| HOSTNAMES tables	{
			struct table	*t = $2;

			if (listen_opts.options & LO_HOSTNAMES) {
				yyerror("hostnames already specified");
				YYERROR;
			}
			listen_opts.options |= LO_HOSTNAMES;

			if (!table_check_use(t, T_DYNAMIC|T_HASH, K_ADDRNAME)) {
				yyerror("invalid use of table \"%s\" as "
				    "HOSTNAMES parameter", t->t_name);
				YYERROR;
			}
			listen_opts.hostnametable = t;
		}
		| MASK_SRC	{
			if (config_lo_mask_source(&listen_opts)) {
				YYERROR;
			}
		}
		| RECEIVEDAUTH	{
			if (listen_opts.options & LO_RECEIVEDAUTH) {
				yyerror("received-auth already specified");
				YYERROR;
			}
			listen_opts.options |= LO_RECEIVEDAUTH;
			listen_opts.flags |= F_RECEIVEDAUTH;
		}
		| NODSN	{
			if (listen_opts.options & LO_NODSN) {
				yyerror("no-dsn already specified");
				YYERROR;
			}
			listen_opts.options |= LO_NODSN;
			listen_opts.flags &= ~F_EXT_DSN;
		}
		| SENDERS tables	{
			struct table	*t = $2;

			if (listen_opts.options & LO_SENDERS) {
				yyerror("senders already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SENDERS;

			if (!table_check_use(t, T_DYNAMIC|T_HASH, K_MAILADDRMAP)) {
				yyerror("invalid use of table \"%s\" as "
				    "SENDERS parameter", t->t_name);
				YYERROR;
			}
			listen_opts.sendertable = t;
		}
		| SENDERS tables MASQUERADE	{
			struct table	*t = $2;

			if (listen_opts.options & LO_SENDERS) {
				yyerror("senders already specified");
				YYERROR;
			}
			listen_opts.options |= LO_SENDERS|LO_MASQUERADE;

			if (!table_check_use(t, T_DYNAMIC|T_HASH, K_MAILADDRMAP)) {
				yyerror("invalid use of table \"%s\" as "
				    "SENDERS parameter", t->t_name);
				YYERROR;
			}
			listen_opts.sendertable = t;
		}
		;

listener_type	: socket_listener
		| if_listener
		;

socket_listener	: SOCKET sock_listen {
			if (conf->sc_sock_listener) {
				yyerror("socket listener already configured");
				YYERROR;
			}
			create_sock_listener(&listen_opts);
		}
		;

if_listener	: STRING if_listen {
			listen_opts.ifx = $1;
			create_if_listener(&listen_opts);
		}
		;

sock_listen	: opt_sock_listen sock_listen
		| /* empty */
		;

if_listen	: opt_if_listen if_listen
		| /* empty */
		;


listen		: LISTEN {
			memset(&listen_opts, 0, sizeof listen_opts);
			listen_opts.family = AF_UNSPEC;
			listen_opts.flags |= F_EXT_DSN;
		} ON listener_type
		;

table		: TABLE STRING STRING	{
			char *p, *backend, *config;

			p = $3;
			if (*p == '/') {
				backend = "static";
				config = $3;
			}
			else {
				backend = $3;
				config = NULL;
				for (p = $3; *p && *p != ':'; p++)
					;
				if (*p == ':') {
					*p = '\0';
					backend = $3;
					config  = p+1;
				}
			}
			if (config != NULL && *config != '/') {
				yyerror("invalid backend parameter for table: %s",
				    $2);
				free($2);
				free($3);
				YYERROR;
			}
			table = table_create(backend, $2, NULL, config);
			if (!table_config(table)) {
				yyerror("invalid configuration file %s for table %s",
				    config, table->t_name);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| TABLE STRING {
			table = table_create("static", $2, NULL, NULL);
			free($2);
		} '{' tableval_list '}' {
			table = NULL;
		}
		;

tablenew	: STRING			{
			struct table	*t;

			t = table_create("static", NULL, NULL, NULL);
			t->t_type = T_LIST;
			table_add(t, $1, NULL);
			free($1);
			$$ = t;
		}
		| '{'				{
			table = table_create("static", NULL, NULL, NULL);
		} tableval_list '}'		{
			$$ = table;
		}
		;

tableref       	: '<' STRING '>'       		{
			struct table	*t;

			if ((t = table_find($2, NULL)) == NULL) {
				yyerror("no such table: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			$$ = t;
		}
		;

tables		: tablenew			{ $$ = $1; }
		| tableref			{ $$ = $1; }
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
		{ "action",		ACTION },
		{ "alias",		ALIAS },
		{ "any",		ANY },
		{ "auth",		AUTH },
		{ "auth-optional",     	AUTH_OPTIONAL },
		{ "backup",		BACKUP },
		{ "bounce",		BOUNCE },
		{ "ca",			CA },
		{ "cert",		CERT },
		{ "ciphers",		CIPHERS },
		{ "compression",	COMPRESSION },
		{ "dhe",		DHE },
		{ "domain",		DOMAIN },
		{ "encryption",		ENCRYPTION },
		{ "expand-only",      	EXPAND_ONLY },
		{ "filter",		FILTER },
		{ "for",		FOR },
		{ "forward-only",      	FORWARD_ONLY },
		{ "from",		FROM },
		{ "helo",		HELO },
		{ "helo-src",       	HELO_SRC },
		{ "host",		HOST },
		{ "hostname",		HOSTNAME },
		{ "hostnames",		HOSTNAMES },
		{ "include",		INCLUDE },
		{ "inet4",		INET4 },
		{ "inet6",		INET6 },
		{ "junk",		JUNK },
		{ "key",		KEY },
		{ "limit",		LIMIT },
		{ "listen",		LISTEN },
		{ "lmtp",		LMTP },
		{ "local",		LOCAL },
		{ "mail-from",		MAIL_FROM },
		{ "maildir",		MAILDIR },
		{ "mask-src",		MASK_SRC },
		{ "masquerade",		MASQUERADE },
		{ "match",		MATCH },
		{ "max-deferred",  	MAX_DEFERRED },
		{ "max-message-size",  	MAX_MESSAGE_SIZE },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "mta",		MTA },
		{ "mx",			MX },
		{ "no-dsn",		NODSN },
		{ "no-verify",		NOVERIFY },
		{ "on",			ON },
		{ "pki",		PKI },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "rcpt-to",		RCPT_TO },
		{ "received-auth",     	RECEIVEDAUTH },
		{ "recipient",		RECIPIENT },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "scheduler",		SCHEDULER },
		{ "senders",   		SENDERS },
		{ "smtp",		SMTP },
		{ "smtps",		SMTPS },
		{ "socket",		SOCKET },
		{ "src",		SRC },
		{ "sub-addr-delim",	SUB_ADDR_DELIM },
		{ "table",		TABLE },
		{ "tag",		TAG },
		{ "tagged",		TAGGED },
		{ "tls",		TLS },
		{ "tls-require",       	TLS_REQUIRE },
		{ "to",			TO },
		{ "ttl",		TTL },
		{ "user",		USER },
		{ "userbase",		USERBASE },
		{ "verify",		VERIFY },
		{ "virtual",		VIRTUAL },
		{ "warn-interval",	WARN_INTERVAL },
		{ "wrapper",		WRAPPER },
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
		else
			c = getc(file->stream);

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
			yyerror("reached end of file while parsing "
			    "quoted string");
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
			err(1, "lungetc");
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
	unsigned char	 buf[8096];
	unsigned char	*p, *val;
	int		 quotec, next, c;
	int		 token;

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

	if (c == '=') {
		if ((c = lgetc(0)) != EOF && c == '>')
			return (ARROW);
		lungetc(c);
		c = '=';
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
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
		log_warn("warn: cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("warn: %s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO)) {
		log_warnx("warn: %s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("warn: malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("warn: malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("warn: %s", nfile->name);
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
		log_warn("warn: malloc");
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

int
parse_config(struct smtpd *x_conf, const char *filename, int opts)
{
	struct sym     *sym, *next;
	struct table   *t;
	char		hostname[HOST_NAME_MAX+1];
	char		hostname_copy[HOST_NAME_MAX+1];

	if (getmailname(hostname, sizeof hostname) == -1)
		return (-1);

	conf = x_conf;
	memset(conf, 0, sizeof(*conf));

	(void)strlcpy(conf->sc_hostname, hostname, sizeof(conf->sc_hostname));

	conf->sc_maxsize = DEFAULT_MAX_BODY_SIZE;
	conf->sc_subaddressing_delim = SUBADDRESSING_DELIMITER;

	conf->sc_tables_dict = calloc(1, sizeof(*conf->sc_tables_dict));
	conf->sc_rules = calloc(1, sizeof(*conf->sc_rules));
	conf->sc_dispatchers = calloc(1, sizeof(*conf->sc_dispatchers));
	conf->sc_listeners = calloc(1, sizeof(*conf->sc_listeners));
	conf->sc_ca_dict = calloc(1, sizeof(*conf->sc_ca_dict));
	conf->sc_pki_dict = calloc(1, sizeof(*conf->sc_pki_dict));
	conf->sc_ssl_dict = calloc(1, sizeof(*conf->sc_ssl_dict));
	conf->sc_limits_dict = calloc(1, sizeof(*conf->sc_limits_dict));
	conf->sc_mda_wrappers = calloc(1, sizeof(*conf->sc_mda_wrappers));

	/* Report mails delayed for more than 4 hours */
	conf->sc_bounce_warn[0] = 3600 * 4;

	if (conf->sc_tables_dict == NULL	||
	    conf->sc_rules == NULL		||
	    conf->sc_dispatchers == NULL	||
	    conf->sc_listeners == NULL		||
	    conf->sc_ca_dict == NULL		||
	    conf->sc_pki_dict == NULL		||
	    conf->sc_ssl_dict == NULL		||
	    conf->sc_limits_dict == NULL        ||
	    conf->sc_mda_wrappers == NULL) {
		log_warn("warn: cannot allocate memory");
		free(conf->sc_tables_dict);
		free(conf->sc_rules);
		free(conf->sc_dispatchers);
		free(conf->sc_listeners);
		free(conf->sc_ca_dict);
		free(conf->sc_pki_dict);
		free(conf->sc_ssl_dict);
		free(conf->sc_limits_dict);
		free(conf->sc_mda_wrappers);
		return (-1);
	}

	errors = 0;

	table = NULL;

	dict_init(conf->sc_dispatchers);
	dict_init(conf->sc_mda_wrappers);
	dict_init(conf->sc_ca_dict);
	dict_init(conf->sc_pki_dict);
	dict_init(conf->sc_ssl_dict);
	dict_init(conf->sc_tables_dict);

	dict_init(conf->sc_limits_dict);
	limits = xcalloc(1, sizeof(*limits));
	limit_mta_set_defaults(limits);
	dict_xset(conf->sc_limits_dict, "default", limits);

	TAILQ_INIT(conf->sc_listeners);
	TAILQ_INIT(conf->sc_rules);

	conf->sc_ttl = SMTPD_QUEUE_EXPIRY;
	conf->sc_opts = opts;

	conf->sc_mta_max_deferred = 100;
	conf->sc_scheduler_max_inflight = 5000;
	conf->sc_scheduler_max_schedule = 10;
	conf->sc_scheduler_max_evp_batch_size = 256;
	conf->sc_scheduler_max_msg_batch_size = 1024;
	
	conf->sc_session_max_rcpt = 1000;
	conf->sc_session_max_mails = 100;

	conf->sc_mda_max_session = 50;
	conf->sc_mda_max_user_session = 7;
	conf->sc_mda_task_hiwat = 50;
	conf->sc_mda_task_lowat = 30;
	conf->sc_mda_task_release = 10;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(PURGE_EVERYTHING);
		return (-1);
	}
	topfile = file;

	/*
	 * declare special "localhost", "anyhost" and "localnames" tables
	 */
	set_local(hostname);

	t = table_create("static", "<anydestination>", NULL, NULL);
	t->t_type = T_LIST;
	table_add(t, "*", NULL);

	/* can't truncate here */
	(void)strlcpy(hostname_copy, hostname, sizeof hostname_copy);

	hostname_copy[strcspn(hostname_copy, ".")] = '\0';
	if (strcmp(hostname, hostname_copy) != 0)
		table_add(t, hostname_copy, NULL);

	table_create("getpwnam", "<getpwnam>", NULL, NULL);

	/* bounce dispatcher */
	dispatcher = xcalloc(1, sizeof *dispatcher);
	dispatcher->type = DISPATCHER_BOUNCE;
	conf->sc_dispatcher_bounce = dispatcher;
	dispatcher = NULL;

	/*
	 * parse configuration
	 */
	setservent(1);
	yyparse();
	errors = file->errors;
	popfile();
	endservent();

	/* If the socket listener was not configured, create a default one. */
	if (!conf->sc_sock_listener) {
		memset(&listen_opts, 0, sizeof listen_opts);
		create_sock_listener(&listen_opts);
	}

	/* Free macros and check which have not been used. */
	TAILQ_FOREACH_SAFE(sym, &symhead, entry, next) {
		if ((conf->sc_opts & SMTPD_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(conf->sc_rules)) {
		log_warnx("warn: no rules, nothing to do");
		errors++;
	}

	if (errors) {
		purge_config(PURGE_EVERYTHING);
		return (-1);
	}

	return (0);
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
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		errx(1, "cmdline_symset: malloc");

	(void)strlcpy(sym, s, len);

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

static void
create_sock_listener(struct listen_opts *lo)
{
	struct listener *l = xcalloc(1, sizeof(*l));
	lo->tag = "local";
	lo->hostname = conf->sc_hostname;
	l->ss.ss_family = AF_LOCAL;
	l->ss.ss_len = sizeof(struct sockaddr *);
	l->local = 1;
	conf->sc_sock_listener = l;
	config_listener(l, lo);
}

static void
create_if_listener(struct listen_opts *lo)
{
	uint16_t	flags;

	if (lo->port != 0 && lo->ssl == F_SSL)
		errx(1, "invalid listen option: tls/smtps on same port");

	if (lo->auth != 0 && !lo->ssl)
		errx(1, "invalid listen option: auth requires tls/smtps");

	if (lo->pki && !lo->ssl)
		errx(1, "invalid listen option: pki requires tls/smtps");

	flags = lo->flags;

	if (lo->port) {
		lo->flags = lo->ssl|lo->auth|flags;
		lo->port = htons(lo->port);
	}
	else {
		if (lo->ssl & F_SMTPS) {
			lo->port = htons(465);
			lo->flags = F_SMTPS|lo->auth|flags;
		}

		if (!lo->ssl || (lo->ssl & F_STARTTLS)) {
			lo->port = htons(25);
			lo->flags = lo->auth|flags;
			if (lo->ssl & F_STARTTLS)
				lo->flags |= F_STARTTLS;
		}
	}

	if (interface(lo))
		return;
	if (host_v4(lo))
		return;
	if (host_v6(lo))
		return;
	if (host_dns(lo))
		return;

	errx(1, "invalid virtual ip or interface: %s", lo->ifx);
}

static void
config_listener(struct listener *h,  struct listen_opts *lo)
{
	h->fd = -1;
	h->port = lo->port;
	h->flags = lo->flags;

	if (lo->hostname == NULL)
		lo->hostname = conf->sc_hostname;

	if (lo->filtername)
		(void)strlcpy(h->filter, lo->filtername, sizeof(h->filter));

	h->pki_name[0] = '\0';

	if (lo->authtable != NULL)
		(void)strlcpy(h->authtable, lo->authtable->t_name, sizeof(h->authtable));
	if (lo->pki != NULL) {
		if (!lowercase(h->pki_name, lo->pki, sizeof(h->pki_name))) {
			log_warnx("pki name too long: %s", lo->pki);
			fatalx(NULL);
		}
		if (dict_get(conf->sc_pki_dict, h->pki_name) == NULL) {
			log_warnx("pki name not found: %s", lo->pki);
			fatalx(NULL);
		}
	}

	if (lo->ca != NULL) {
		if (!lowercase(h->ca_name, lo->ca, sizeof(h->ca_name))) {
			log_warnx("ca name too long: %s", lo->ca);
			fatalx(NULL);
		}
		if (dict_get(conf->sc_ca_dict, h->ca_name) == NULL) {
			log_warnx("ca name not found: %s", lo->ca);
			fatalx(NULL);
		}
	}
	if (lo->tag != NULL)
		(void)strlcpy(h->tag, lo->tag, sizeof(h->tag));

	(void)strlcpy(h->hostname, lo->hostname, sizeof(h->hostname));
	if (lo->hostnametable)
		(void)strlcpy(h->hostnametable, lo->hostnametable->t_name, sizeof(h->hostnametable));
	if (lo->sendertable) {
		(void)strlcpy(h->sendertable, lo->sendertable->t_name, sizeof(h->sendertable));
		if (lo->options & LO_MASQUERADE)
			h->flags |= F_MASQUERADE;
	}

	if (lo->ssl & F_TLS_VERIFY)
		h->flags |= F_TLS_VERIFY;

	if (lo->ssl & F_STARTTLS_REQUIRE)
		h->flags |= F_STARTTLS_REQUIRE;
	
	if (h != conf->sc_sock_listener)
		TAILQ_INSERT_TAIL(conf->sc_listeners, h, entry);
}

static int
host_v4(struct listen_opts *lo)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct listener		*h;

	if (lo->family != AF_UNSPEC && lo->family != AF_INET)
		return (0);

	memset(&ina, 0, sizeof(ina));
	if (inet_pton(AF_INET, lo->ifx, &ina) != 1)
		return (0);

	h = xcalloc(1, sizeof(*h));
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;
	sain->sin_port = lo->port;

	if (sain->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
		h->local = 1;
	config_listener(h,  lo);

	return (1);
}

static int
host_v6(struct listen_opts *lo)
{
	struct in6_addr		 ina6;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	if (lo->family != AF_UNSPEC && lo->family != AF_INET6)
		return (0);

	memset(&ina6, 0, sizeof(ina6));
	if (inet_pton(AF_INET6, lo->ifx, &ina6) != 1)
		return (0);

	h = xcalloc(1, sizeof(*h));
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = lo->port;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
		h->local = 1;
	config_listener(h,  lo);

	return (1);
}

static int
host_dns(struct listen_opts *lo)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = lo->family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;
	error = getaddrinfo(lo->ifx, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("warn: host_dns: could not parse \"%s\": %s", lo->ifx,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		h = xcalloc(1, sizeof(*h));

		h->ss.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
			sain->sin_port = lo->port;
			if (sain->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
				h->local = 1;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_port = lo->port;
			if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				h->local = 1;
		}

		config_listener(h, lo);

		cnt++;
	}

	freeaddrinfo(res0);
	return (cnt);
}

static int
interface(struct listen_opts *lo)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;
	int			ret = 0;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		if (strcmp(p->ifa_name, lo->ifx) != 0 &&
		    !is_if_in_group(p->ifa_name, lo->ifx))
			continue;
		if (lo->family != AF_UNSPEC && lo->family != p->ifa_addr->sa_family)
			continue;

		h = xcalloc(1, sizeof(*h));

		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&h->ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_port = lo->port;
			if (sain->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
				h->local = 1;
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&h->ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = lo->port;
			if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				h->local = 1;
			break;

		default:
			free(h);
			continue;
		}

		config_listener(h, lo);
		ret = 1;
	}

	freeifaddrs(ifap);

	return ret;
}

void
set_local(const char *hostname)
{
	struct table	*t;

	t = table_create("static", "<localnames>", NULL, NULL);
	t->t_type = T_LIST;
	table_add(t, "localhost", NULL);
	table_add(t, hostname, NULL);

	set_localaddrs(t);
}

void
set_localaddrs(struct table *localnames)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_storage ss;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct table		*t;
	char buf[NI_MAXHOST + 5];

	t = table_create("static", "<anyhost>", NULL, NULL);
	table_add(t, "local", NULL);
	table_add(t, "0.0.0.0/0", NULL);
	table_add(t, "::/0", NULL);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	t = table_create("static", "<localhost>", NULL, NULL);
	table_add(t, "local", NULL);

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			table_add(t, ss_to_text(&ss), NULL);
			table_add(localnames, ss_to_text(&ss), NULL);
			(void)snprintf(buf, sizeof buf, "[%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			table_add(t, ss_to_text(&ss), NULL);
			table_add(localnames, ss_to_text(&ss), NULL);
			(void)snprintf(buf, sizeof buf, "[%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
			(void)snprintf(buf, sizeof buf, "[ipv6:%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
			break;
		}
	}

	freeifaddrs(ifap);
}

int
delaytonum(char *str)
{
	unsigned int     factor;
	size_t           len;
	const char      *errstr = NULL;
	int              delay;

	/* we need at least 1 digit and 1 unit */
	len = strlen(str);
	if (len < 2)
		goto bad;

	switch(str[len - 1]) {

	case 's':
		factor = 1;
		break;

	case 'm':
		factor = 60;
		break;

	case 'h':
		factor = 60 * 60;
		break;

	case 'd':
		factor = 24 * 60 * 60;
		break;

	default:
		goto bad;
	}

	str[len - 1] = '\0';
	delay = strtonum(str, 1, INT_MAX / factor, &errstr);
	if (errstr)
		goto bad;

	return (delay * factor);

bad:
	return (-1);
}

int
is_if_in_group(const char *ifname, const char *groupname)
{
        unsigned int		 len;
        struct ifgroupreq        ifgr;
        struct ifg_req          *ifg;
	int			 s;
	int			 ret = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

        memset(&ifgr, 0, sizeof(ifgr));
        if (strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ) >= IFNAMSIZ)
		errx(1, "interface name too large");

        if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
                if (errno == EINVAL || errno == ENOTTY)
			goto end;
		err(1, "SIOCGIFGROUP");
        }

        len = ifgr.ifgr_len;
        ifgr.ifgr_groups = xcalloc(len/sizeof(struct ifg_req),
		sizeof(struct ifg_req));
        if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1)
                err(1, "SIOCGIFGROUP");

        ifg = ifgr.ifgr_groups;
        for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
                len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, groupname) == 0) {
			ret = 1;
			break;
		}
        }
        free(ifgr.ifgr_groups);

end:
	close(s);
	return ret;
}

static int
config_lo_filter(struct listen_opts *lo, char *filter_name) {
	if (lo->options & LO_FILTER) {
		yyerror("filter already specified");
		return -1;
	}
	lo->options |= LO_FILTER;
	lo->filtername = filter_name;

	return 0;
}

static int
config_lo_mask_source(struct listen_opts *lo) {
	if (lo->options & LO_MASKSOURCE) {
		yyerror("mask-source already specified");
		return -1;
	}
	lo->options |= LO_MASKSOURCE;
	lo->flags |= F_MASK_SOURCE;

	return 0;
}


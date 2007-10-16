/*	$OpenBSD: parse.y,v 1.129 2007/10/16 06:06:49 deraadt Exp $	*/

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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>

#include "ipsecctl.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file;
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
int		 cmdline_symset(char *);

#define KEYSIZE_LIMIT	1024

static struct ipsecctl	*ipsec = NULL;
static int		 debug = 0;

const struct ipsec_xf authxfs[] = {
	{ "unknown",		AUTHXF_UNKNOWN,		0,	0 },
	{ "none",		AUTHXF_NONE,		0,	0 },
	{ "hmac-md5",		AUTHXF_HMAC_MD5,	16,	0 },
	{ "hmac-ripemd160",	AUTHXF_HMAC_RIPEMD160,	20,	0 },
	{ "hmac-sha1",		AUTHXF_HMAC_SHA1,	20,	0 },
	{ "hmac-sha2-256",	AUTHXF_HMAC_SHA2_256,	32,	0 },
	{ "hmac-sha2-384",	AUTHXF_HMAC_SHA2_384,	48,	0 },
	{ "hmac-sha2-512",	AUTHXF_HMAC_SHA2_512,	64,	0 },
	{ NULL,			0,			0,	0 },
};

const struct ipsec_xf encxfs[] = {
	{ "unknown",		ENCXF_UNKNOWN,		0,	0 },
	{ "none",		ENCXF_NONE,		0,	0 },
	{ "3des-cbc",		ENCXF_3DES_CBC,		24,	24 },
	{ "des-cbc",		ENCXF_DES_CBC,		8,	8 },
	{ "aes",		ENCXF_AES,		16,	32 },
	{ "aesctr",		ENCXF_AESCTR,		16+4,	32+4 },
	{ "blowfish",		ENCXF_BLOWFISH,		5,	56 },
	{ "cast128",		ENCXF_CAST128,		5,	16 },
	{ "null",		ENCXF_NULL,		0,	0 },
	{ "skipjack",		ENCXF_SKIPJACK,		10,	10 },
	{ NULL,			0,			0,	0 },
};

const struct ipsec_xf compxfs[] = {
	{ "unknown",		COMPXF_UNKNOWN,		0,	0 },
	{ "deflate",		COMPXF_DEFLATE,		0,	0 },
	{ "lzs",		COMPXF_LZS,		0,	0 },
	{ NULL,			0,			0,	0 },
};

const struct ipsec_xf groupxfs[] = {
	{ "unknown",		GROUPXF_UNKNOWN,	0,	0 },
	{ "none",		GROUPXF_NONE,		0,	0 },
	{ "modp768",		GROUPXF_768,		768,	0 },
	{ "grp1",		GROUPXF_768,		768,	0 },
	{ "modp1024",		GROUPXF_1024,		1024,	0 },
	{ "grp2",		GROUPXF_1024,		1024,	0 },
	{ "modp1536",		GROUPXF_1536,		1536,	0 },
	{ "grp5",		GROUPXF_1536,		1536,	0 },
	{ "modp2048",		GROUPXF_2048,		2048,	0 },
	{ "grp14",		GROUPXF_2048,		2048,	0 },
	{ "modp3072",		GROUPXF_3072,		3072,	0 },
	{ "grp15",		GROUPXF_3072,		3072,	0 },
	{ "modp4096",		GROUPXF_4096,		4096,	0 },
	{ "grp16",		GROUPXF_4096,		4096,	0 },
	{ "modp6144",		GROUPXF_6144,		6144,	0 },
	{ "grp17",		GROUPXF_6144,		6144,	0 },
	{ "modp8192",		GROUPXF_8192,		8192,	0 },
	{ "grp18",		GROUPXF_8192,		8192,	0 },
	{ NULL,			0,			0,	0 },
};

int			 atoul(char *, u_long *);
int			 atospi(char *, u_int32_t *);
u_int8_t		 x2i(unsigned char *);
struct ipsec_key	*parsekey(unsigned char *, size_t);
struct ipsec_key	*parsekeyfile(char *);
struct ipsec_addr_wrap	*host(const char *);
struct ipsec_addr_wrap	*host_v6(const char *, int);
struct ipsec_addr_wrap	*host_v4(const char *, int);
struct ipsec_addr_wrap	*host_dns(const char *, int);
struct ipsec_addr_wrap	*host_if(const char *, int);
void			 ifa_load(void);
int			 ifa_exists(const char *);
struct ipsec_addr_wrap	*ifa_lookup(const char *ifa_name);
struct ipsec_addr_wrap	*ifa_grouplookup(const char *);
void			 set_ipmask(struct ipsec_addr_wrap *, u_int8_t);
const struct ipsec_xf	*parse_xf(const char *, const struct ipsec_xf *);
struct ipsec_life	*parse_life(int);
struct ipsec_transforms *copytransforms(const struct ipsec_transforms *);
struct ipsec_life	*copylife(const struct ipsec_life *);
struct ipsec_auth	*copyipsecauth(const struct ipsec_auth *);
struct ike_auth		*copyikeauth(const struct ike_auth *);
struct ipsec_key	*copykey(struct ipsec_key *);
struct ipsec_addr_wrap	*copyhost(const struct ipsec_addr_wrap *);
char			*copytag(const char *);
struct ipsec_rule	*copyrule(struct ipsec_rule *);
int			 validate_af(struct ipsec_addr_wrap *,
			    struct ipsec_addr_wrap *);
int			 validate_sa(u_int32_t, u_int8_t,
			     struct ipsec_transforms *, struct ipsec_key *,
			     struct ipsec_key *, u_int8_t);
struct ipsec_rule	*create_sa(u_int8_t, u_int8_t, struct ipsec_hosts *,
			     u_int32_t, struct ipsec_transforms *,
			     struct ipsec_key *, struct ipsec_key *);
struct ipsec_rule	*reverse_sa(struct ipsec_rule *, u_int32_t,
			     struct ipsec_key *, struct ipsec_key *);
struct ipsec_rule	*create_sagroup(struct ipsec_addr_wrap *, u_int8_t,
			     u_int32_t, struct ipsec_addr_wrap *, u_int8_t,
			     u_int32_t);
struct ipsec_rule	*create_flow(u_int8_t, u_int8_t, struct ipsec_hosts *,
			     struct ipsec_hosts *, u_int8_t, char *, char *,
			     u_int8_t);
void			 expand_any(struct ipsec_addr_wrap *);
int			 expand_rule(struct ipsec_rule *, u_int8_t, u_int32_t,
			     struct ipsec_key *, struct ipsec_key *, int);
struct ipsec_rule	*reverse_rule(struct ipsec_rule *);
struct ipsec_rule	*create_ike(u_int8_t, struct ipsec_hosts *,
			     struct ipsec_hosts *, struct ike_mode *,
			     struct ike_mode *, u_int8_t, u_int8_t, u_int8_t,
			     char *, char *, struct ike_auth *, char *);
int			 add_sagroup(struct ipsec_rule *);
int			 get_id_type(char *);

struct ipsec_transforms *ipsec_transforms;

typedef struct {
	union {
		int64_t	 	 number;
		u_int8_t	 ikemode;
		u_int8_t	 dir;
		u_int8_t	 satype;	/* encapsulating prococol */
		u_int8_t	 proto;		/* encapsulated protocol */
		u_int8_t	 tmode;
		char		*string;
		u_int16_t	 port;
		struct ipsec_hosts hosts;
		struct ipsec_hosts peers;
		struct ipsec_addr_wrap *singlehost;
		struct ipsec_addr_wrap *host;
		struct {
			char *srcid;
			char *dstid;
		} ids;
		char		*id;
		u_int8_t	 type;
		struct ike_auth	 ikeauth;
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
		struct ipsec_life	*life;
		struct ike_mode		*mode;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FLOW FROM ESP AH IN PEER ON OUT TO SRCID DSTID RSA PSK TCPMD5 SPI
%token	AUTHKEY ENCKEY FILENAME AUTHXF ENCXF ERROR IKE MAIN QUICK AGGRESSIVE
%token	PASSIVE ACTIVE ANY IPIP IPCOMP COMPXF TUNNEL TRANSPORT DYNAMIC LIFE
%token	TYPE DENY BYPASS LOCAL PROTO USE ACQUIRE REQUIRE DONTACQ GROUP PORT TAG
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.string>		string
%type	<v.dir>			dir
%type	<v.satype>		satype
%type	<v.proto>		proto
%type	<v.number>		protoval
%type	<v.tmode>		tmode
%type	<v.hosts>		hosts
%type	<v.port>		port
%type	<v.number>		portval
%type	<v.peers>		peers
%type	<v.singlehost>		singlehost
%type	<v.host>		host host_list
%type	<v.ids>			ids
%type	<v.id>			id
%type	<v.spis>		spispec
%type	<v.authkeys>		authkeyspec
%type	<v.enckeys>		enckeyspec
%type	<v.keys>		keyspec
%type	<v.transforms>		transforms
%type	<v.ikemode>		ikemode
%type	<v.ikeauth>		ikeauth
%type	<v.type>		type
%type	<v.life>		life
%type	<v.mode>		phase1mode phase2mode
%type	<v.string>		tag
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar ikerule '\n'
		| grammar flowrule '\n'
		| grammar sarule '\n'
		| grammar tcpmd5rule '\n'
		| grammar varset '\n'
		| grammar error '\n'		{ file->errors++; }
		;

comma		: ','
		| /* empty */
		;

tcpmd5rule	: TCPMD5 hosts spispec authkeyspec	{
			struct ipsec_rule	*r;

			r = create_sa(IPSEC_TCPMD5, IPSEC_TRANSPORT, &$2,
			    $3.spiout, NULL, $4.keyout, NULL);
			if (r == NULL)
				YYERROR;

			if (expand_rule(r, 0, $3.spiin, $4.keyin, NULL, 0))
				errx(1, "tcpmd5rule: expand_rule");
		}
		;

sarule		: satype tmode hosts spispec transforms authkeyspec
		    enckeyspec {
			struct ipsec_rule	*r;

			r = create_sa($1, $2, &$3, $4.spiout, $5, $6.keyout,
			    $7.keyout);
			if (r == NULL)
				YYERROR;

			if (expand_rule(r, 0, $4.spiin, $6.keyin, $7.keyin, 1))
				errx(1, "sarule: expand_rule");
		}
		;

flowrule	: FLOW satype dir proto hosts peers ids type {
			struct ipsec_rule	*r;

			r = create_flow($3, $4, &$5, &$6, $2, $7.srcid,
			    $7.dstid, $8);
			if (r == NULL)
				YYERROR;

			if (expand_rule(r, $3, 0, NULL, NULL, 0))
				errx(1, "flowrule: expand_rule");
		}
		;

ikerule		: IKE ikemode satype tmode proto hosts peers
		    phase1mode phase2mode ids ikeauth tag {
			struct ipsec_rule	*r;

			r = create_ike($5, &$6, &$7, $8, $9, $3, $4, $2,
			    $10.srcid, $10.dstid, &$11, $12);
			if (r == NULL)
				YYERROR;

			if (expand_rule(r, 0, 0, NULL, NULL, 0))
				errx(1, "ikerule: expand_rule");
		}
		;

satype		: /* empty */			{ $$ = IPSEC_ESP; }
		| ESP				{ $$ = IPSEC_ESP; }
		| AH				{ $$ = IPSEC_AH; }
		| IPCOMP			{ $$ = IPSEC_IPCOMP; }
		| IPIP				{ $$ = IPSEC_IPIP; }
		;

proto		: /* empty */			{ $$ = 0; }
		| PROTO protoval		{ $$ = $2; }
		| PROTO ESP 			{ $$ = IPPROTO_ESP; }
		| PROTO AH			{ $$ = IPPROTO_AH; }
		;

protoval	: STRING			{
			struct protoent *p;

			p = getprotobyname($1);
			if (p == NULL) {
				yyerror("unknown protocol: %s", $1);
				YYERROR;
			}
			$$ = p->p_proto;
			free($1);
		}
		| NUMBER			{
			if ($1 > 255 || $1 < 0) {
				yyerror("protocol outside range");
				YYERROR;
			}
		}
		;

tmode		: /* empty */			{ $$ = IPSEC_TUNNEL; }
		| TUNNEL			{ $$ = IPSEC_TUNNEL; }
		| TRANSPORT			{ $$ = IPSEC_TRANSPORT; }
		;

dir		: /* empty */			{ $$ = IPSEC_INOUT; }
		| IN				{ $$ = IPSEC_IN; }
		| OUT				{ $$ = IPSEC_OUT; }
		;

hosts		: FROM host port TO host port		{
			$$.src = $2;
			$$.sport = $3;
			$$.dst = $5;
			$$.dport = $6;
		}
		| TO host port FROM host port		{
			$$.src = $5;
			$$.sport = $6;
			$$.dst = $2;
			$$.dport = $3;
		}
		;

port		: /* empty */				{ $$ = 0; }
		| PORT portval				{ $$ = $2; }
		;

portval		: STRING				{
			struct servent *s;

			if ((s = getservbyname($1, "tcp")) != NULL ||
			    (s = getservbyname($1, "udp")) != NULL) {
				$$ = s->s_port;
			} else {
				yyerror("unknown port: %s", $1);
				YYERROR;
			}
		}
		| NUMBER				{
			if ($1 > USHRT_MAX || $1 < 0) {
				yyerror("port outside range");
				YYERROR;
			}
			$$ = htons($1);
		}
		;

peers		: /* empty */				{
			$$.dst = NULL;
			$$.src = NULL;
		}
		| PEER singlehost LOCAL singlehost	{
			$$.dst = $2;
			$$.src = $4;
		}
		| LOCAL singlehost PEER singlehost	{
			$$.dst = $4;
			$$.src = $2;
		}
		| PEER singlehost			{
			$$.dst = $2;
			$$.src = NULL;
		}
		| LOCAL singlehost			{
			$$.dst = NULL;
			$$.src = $2;
		}
		;

singlehost	: /* empty */			{ $$ = NULL; }
		| STRING			{
			if (($$ = host($1)) == NULL) {
				free($1);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free($1);
		}
		;

host_list	: host				{ $$ = $1; }
		| host_list comma host		{
			if ($3 == NULL)
				$$ = $1;
			else if ($1 == NULL)
				$$ = $3;
			else {
				$1->tail->next = $3;
				$1->tail = $3->tail;
				$$ = $1;
			}
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
		| STRING '/' NUMBER		{
			char	*buf;

			if (asprintf(&buf, "%s/%lld", $1, $3) == -1)
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
			ipa->af = AF_UNSPEC;
			ipa->netaddress = 1;
			ipa->tail = ipa;
			$$ = ipa;
		}
		| '{' host_list '}'		{ $$ = $2; }
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

type		: /* empty */			{
			$$ = TYPE_REQUIRE;
		}
		| TYPE USE			{
			$$ = TYPE_USE;
		}
		| TYPE ACQUIRE			{
			$$ = TYPE_ACQUIRE;
		}
		| TYPE REQUIRE			{
			$$ = TYPE_REQUIRE;
		}
		| TYPE DENY			{
			$$ = TYPE_DENY;
		}
		| TYPE BYPASS			{
			$$ = TYPE_BYPASS;
		}
		| TYPE DONTACQ			{
			$$ = TYPE_DONTACQ;
		}
		;

id		: STRING			{ $$ = $1; }
		;

spispec		: SPI STRING			{
			u_int32_t	 spi;
			char		*p = strchr($2, ':');

			if (p != NULL) {
				*p++ = 0;

				if (atospi(p, &spi) == -1) {
					free($2);
					YYERROR;
				}
				$$.spiin = spi;
			} else
				$$.spiin = 0;

			if (atospi($2, &spi) == -1) {
				free($2);
				YYERROR;
			}
			$$.spiout = spi;


			free($2);
		}
		| SPI NUMBER			{
			if ($2 > UINT_MAX || $2 < 0) {
				yyerror("%lld not a valid spi", $2);
				YYERROR;
			}
			if ($2 >= SPI_RESERVED_MIN && $2 <= SPI_RESERVED_MAX) {
				yyerror("%lld within reserved spi range", $2);
				YYERROR;
			}

			$$.spiin = 0;
			$$.spiout = $2;
		}
		;

transforms	:					{
			if ((ipsec_transforms = calloc(1,
			    sizeof(struct ipsec_transforms))) == NULL)
				err(1, "transforms: calloc");
		}
		    transforms_l
			{ $$ = ipsec_transforms; }
		| /* empty */				{
			if (($$ = calloc(1,
			    sizeof(struct ipsec_transforms))) == NULL)
				err(1, "transforms: calloc");
		}
		;

transforms_l	: transforms_l transform
		| transform
		;

transform	: AUTHXF STRING			{
			if (ipsec_transforms->authxf)
				yyerror("auth already set");
			else {
				ipsec_transforms->authxf = parse_xf($2,
				    authxfs);
				if (!ipsec_transforms->authxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		| ENCXF STRING			{
			if (ipsec_transforms->encxf)
				yyerror("enc already set");
			else {
				ipsec_transforms->encxf = parse_xf($2, encxfs);
				if (!ipsec_transforms->encxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		| COMPXF STRING			{
			if (ipsec_transforms->compxf)
				yyerror("comp already set");
			else {
				ipsec_transforms->compxf = parse_xf($2,
				    compxfs);
				if (!ipsec_transforms->compxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		| GROUP STRING			{
			if (ipsec_transforms->groupxf)
				yyerror("group already set");
			else {
				ipsec_transforms->groupxf = parse_xf($2,
				    groupxfs);
				if (!ipsec_transforms->groupxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		;

phase1mode	: /* empty */	{
			struct ike_mode		*p1;

			/* We create just an empty main mode */
			if ((p1 = calloc(1, sizeof(struct ike_mode))) == NULL)
				err(1, "phase1mode: calloc");
			p1->ike_exch = IKE_MM;
			$$ = p1;
		}
		| MAIN transforms life		{
			struct ike_mode	*p1;

			if ((p1 = calloc(1, sizeof(struct ike_mode))) == NULL)
				err(1, "phase1mode: calloc");
			p1->xfs = $2;
			p1->life = $3;
			p1->ike_exch = IKE_MM;
			$$ = p1;
		}
		| AGGRESSIVE transforms life		{
			struct ike_mode	*p1;

			if ((p1 = calloc(1, sizeof(struct ike_mode))) == NULL)
				err(1, "phase1mode: calloc");
			p1->xfs = $2;
			p1->life = $3;
			p1->ike_exch = IKE_AM;
			$$ = p1;
		}
		;

phase2mode	: /* empty */	{
			struct ike_mode		*p2;

			/* We create just an empty quick mode */
			if ((p2 = calloc(1, sizeof(struct ike_mode))) == NULL)
				err(1, "phase1mode: calloc");
			p2->ike_exch = IKE_QM;
			$$ = p2;
		}
		| QUICK transforms life		{
			struct ike_mode	*p2;

			if ((p2 = calloc(1, sizeof(struct ike_mode))) == NULL)
				err(1, "phase1mode: calloc");
			p2->xfs = $2;
			p2->life = $3;
			p2->ike_exch = IKE_QM;
			$$ = p2;
		}
		;

life		: /* empty */			{
			struct ipsec_life *life;

			/* We create just an empty transform */
			if ((life = calloc(1, sizeof(struct ipsec_life)))
			    == NULL)
				err(1, "life: calloc");
			life->lifetime = -1;
			life->lifevolume = -1;
			$$ = life;
		}
		| LIFE NUMBER			{
			if ($2 > INT_MAX || $2 < 0) {
				yyerror("%lld not a valid lifetime", $2);
				YYERROR;
			}
			$$ = parse_life($2);
		}
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
			} else
				$$.keyin = NULL;

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
		| DYNAMIC			{ $$ = IKE_DYNAMIC; }
		| ACTIVE			{ $$ = IKE_ACTIVE; }
		;

ikeauth		: /* empty */			{
			$$.type = IKE_AUTH_RSA;
			$$.string = NULL;
		}
		| RSA				{
			$$.type = IKE_AUTH_RSA;
			$$.string = NULL;
		}
		| PSK STRING			{
			$$.type = IKE_AUTH_PSK;
			if (($$.string = strdup($2)) == NULL)
				err(1, "ikeauth: strdup");
		}
		;

tag		: /* empty */
		{
			$$ = NULL;
		}
		| TAG STRING
		{
			$$ = $2;
		}
		;

string		: string STRING
		{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				err(1, "string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string
		{
			if (ipsec->opts & IPSECCTL_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				err(1, "cannot store variable");
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

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s: %d: ", file->name, yylval.lineno);
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
		{ "acquire",		ACQUIRE },
		{ "active",		ACTIVE },
		{ "aggressive",		AGGRESSIVE },
		{ "ah",			AH },
		{ "any",		ANY },
		{ "auth",		AUTHXF },
		{ "authkey",		AUTHKEY },
		{ "bypass",		BYPASS },
		{ "comp",		COMPXF },
		{ "deny",		DENY },
		{ "dontacq",		DONTACQ },
		{ "dstid",		DSTID },
		{ "dynamic",		DYNAMIC },
		{ "enc",		ENCXF },
		{ "enckey",		ENCKEY },
		{ "esp",		ESP },
		{ "file",		FILENAME },
		{ "flow",		FLOW },
		{ "from",		FROM },
		{ "group",		GROUP },
		{ "ike",		IKE },
		{ "in",			IN },
		{ "ipcomp",		IPCOMP },
		{ "ipip",		IPIP },
		{ "life",		LIFE },
		{ "local",		LOCAL },
		{ "main",		MAIN },
		{ "out",		OUT },
		{ "passive",		PASSIVE },
		{ "peer",		PEER },
		{ "port",		PORT },
		{ "proto",		PROTO },
		{ "psk",		PSK },
		{ "quick",		QUICK },
		{ "require",		REQUIRE },
		{ "rsa",		RSA },
		{ "spi",		SPI },
		{ "srcid",		SRCID },
		{ "tag",		TAG },
		{ "tcpmd5",		TCPMD5 },
		{ "to",			TO },
		{ "transport",		TRANSPORT },
		{ "tunnel",		TUNNEL },
		{ "type",		TYPE },
		{ "use",		USE }
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
			yyerror("reached end of file while parsing quoted string");
			if (popfile() == EOF)
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
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(file->stream);
		} while (c == '\t' || c == ' ');
		ungetc(c, file->stream);
		c = ' ';
	}

	while (c == EOF) {
		if (popfile() == EOF)
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
	char	 buf[8096];
	char	*p, *val;
	int	 quotec, next, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(0)) == ' ')
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
			err(1, "yylex: strdup");
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}')

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
		warn("cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		warnx("%s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL ||
	    (nfile->name = strdup(name)) == NULL)
		return (NULL);
	if (TAILQ_FIRST(&files) == NULL && strcmp(nfile->name, "-") == 0) {
		nfile->stream = stdin;
		free(nfile->name);
		if ((nfile->name = strdup("stdin")) == NULL) {
			free(nfile);
			return (NULL);
		}
	} else if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
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

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL) {
		prev->errors += file->errors;
		TAILQ_REMOVE(&files, file, entry);
		fclose(file->stream);
		free(file->name);
		free(file);
		file = prev;
		return (0);
	}
	return (EOF);
}

int
parse_rules(const char *filename, struct ipsecctl *ipsecx)
{
	struct sym	*sym;
	int		 errors = 0;

	ipsec = ipsecx;

	if ((file = pushfile(filename, 1)) == NULL) {
		warn("cannot open the main config file!");
		return (-1);
	}

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	while ((sym = TAILQ_FIRST(&symhead))) {
		if ((ipsec->opts & IPSECCTL_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		free(sym->nam);
		free(sym->val);
		TAILQ_REMOVE(&symhead, sym, entry);
		free(sym);
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

	TAILQ_FOREACH(sym, &symhead, entry)
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
	if (ulval > UINT_MAX) {
		yyerror("%lld not a valid spi", ulval);
		return (-1);
	}
	if (ulval >= SPI_RESERVED_MIN && ulval <= SPI_RESERVED_MAX) {
		yyerror("%lld within reserved spi range", ulval);
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

	if ((fd = open(filename, O_RDONLY)) < 0)
		err(1, "open %s", filename);
	if (fstat(fd, &sb) < 0)
		err(1, "parsekeyfile: stat %s", filename);
	if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0))
		errx(1, "%s: key too %s", filename, sb.st_size ? "large" :
		    "small");
	if ((hex = calloc(sb.st_size, sizeof(unsigned char))) == NULL)
		err(1, "parsekeyfile: calloc");
	if (read(fd, hex, sb.st_size) < sb.st_size)
		err(1, "parsekeyfile: read");
	close(fd);
	return (parsekey(hex, sb.st_size));
}

int
get_id_type(char *string)
{
	if (string && strchr(string, '@'))
		return (ID_UFQDN);
	return (ID_FQDN);
}

struct ipsec_addr_wrap *
host(const char *s)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	int			 mask, cont = 1;
	char			*p, *q, *ps;

	if ((p = strrchr(s, '/')) != NULL) {
		errno = 0;
		mask = strtol(p + 1, &q, 0);
		if (errno == ERANGE || !q || *q || mask > 128 || q == (p + 1))
			errx(1, "host: invalid netmask '%s'", p);
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "host: calloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
	} else {
		if ((ps = strdup(s)) == NULL)
			err(1, "host: strdup");
		mask = -1;
	}

	/* Does interface with this name exist? */
	if (cont && (ipa = host_if(ps, mask)) != NULL)
		cont = 0;

	/* IPv4 address? */
	if (cont && (ipa = host_v4(s, mask == -1 ? 32 : mask)) != NULL)
		cont = 0;

	/* IPv6 address? */
	if (cont && (ipa = host_v6(ps, mask == -1 ? 128 : mask)) != NULL)
		cont = 0;

	/* dns lookup */
	if (cont && mask == -1 && (ipa = host_dns(s, mask)) != NULL)
		cont = 0;
	free(ps);

	if (ipa == NULL || cont == 1) {
		fprintf(stderr, "no IP address found for %s\n", s);
		return (NULL);
	}
	return (ipa);
}

struct ipsec_addr_wrap *
host_v6(const char *s, int prefixlen)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	struct addrinfo		 hints, *res;
	char			 hbuf[NI_MAXHOST];

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, NULL, &hints, &res))
		return (NULL);
	if (res->ai_next)
		err(1, "host_v6: numeric hostname expanded to multiple item");

	ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (ipa == NULL)
		err(1, "host_v6: calloc");
	ipa->af = res->ai_family;
	memcpy(&ipa->address.v6,
	    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
	    sizeof(struct in6_addr));
	if (prefixlen > 128)
		prefixlen = 128;
	ipa->next = NULL;
	ipa->tail = ipa;

	set_ipmask(ipa, prefixlen);
	if (getnameinfo(res->ai_addr, res->ai_addrlen,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST)) {
		errx(1, "could not get a numeric hostname");
	}

	if (prefixlen != 128) {
		ipa->netaddress = 1;
		asprintf(&ipa->name, "%s/%d", hbuf, prefixlen);
	} else
		ipa->name = strdup(hbuf);
	if (ipa->name == NULL)
		err(1, "host_v6: strdup");

	freeaddrinfo(res);

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
	ipa->next = NULL;
	ipa->tail = ipa;

	set_ipmask(ipa, bits);
	if (bits != (ipa->af == AF_INET ? 32 : 128))
		ipa->netaddress = 1;

	return (ipa);
}

struct ipsec_addr_wrap *
host_dns(const char *s, int mask)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	struct addrinfo		 hints, *res0, *res;
	int			 error;
	char			 hbuf[NI_MAXHOST];

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error)
		return (NULL);

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET && res->ai_family != AF_INET6)
			continue;

		ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (ipa == NULL)
			err(1, "host_dns: calloc");
		switch (res->ai_family) {
		case AF_INET:
			memcpy(&ipa->address.v4,
			    &((struct sockaddr_in *)res->ai_addr)->sin_addr,
			    sizeof(struct in_addr));
			break;
		case AF_INET6:
			/* XXX we do not support scoped IPv6 address yet */
			if (((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id) {
				free(ipa);
				continue;
			}
			memcpy(&ipa->address.v6,
			    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
			    sizeof(struct in6_addr));
			break;
		}
		error = getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
		if (error)
			err(1, "host_dns: getnameinfo");
		ipa->name = strdup(hbuf);
		if (ipa->name == NULL)
			err(1, "host_dns: strdup");
		ipa->af = res->ai_family;
		ipa->next = NULL;
		ipa->tail = ipa;

		/*
		 * XXX for now, no netmask support for IPv6.
		 * but since there's no way to specify address family, once you
		 * have IPv6 address on a host, you cannot use dns/netmask
		 * syntax.
		 */
		if (ipa->af == AF_INET)
			set_ipmask(ipa, mask == -1 ? 32 : mask);
		else
			if (mask != -1)
				err(1, "host_dns: cannot apply netmask "
				    "on non-IPv4 address");
		break;
	}
	freeaddrinfo(res0);

	return (ipa);
}

struct ipsec_addr_wrap *
host_if(const char *s, int mask)
{
	struct ipsec_addr_wrap *ipa = NULL;

	if (ifa_exists(s))
		ipa = ifa_lookup(s);

	return (ipa);
}

/* interface lookup routintes */

struct ipsec_addr_wrap	*iftab;

void
ifa_load(void)
{
	struct ifaddrs		*ifap, *ifa;
	struct ipsec_addr_wrap	*n = NULL, *h = NULL;

	if (getifaddrs(&ifap) < 0)
		err(1, "ifa_load: getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_addr->sa_family == AF_INET ||
		    ifa->ifa_addr->sa_family == AF_INET6 ||
		    ifa->ifa_addr->sa_family == AF_LINK))
			continue;
		n = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (n == NULL)
			err(1, "ifa_load: calloc");
		n->af = ifa->ifa_addr->sa_family;
		if ((n->name = strdup(ifa->ifa_name)) == NULL)
			err(1, "ifa_load: strdup");
		if (n->af == AF_INET) {
			n->af = AF_INET;
			memcpy(&n->address.v4, &((struct sockaddr_in *)
			    ifa->ifa_addr)->sin_addr,
			    sizeof(struct in_addr));
			memcpy(&n->mask.v4, &((struct sockaddr_in *)
			    ifa->ifa_netmask)->sin_addr,
			    sizeof(struct in_addr));
		} else if (n->af == AF_INET6) {
			n->af = AF_INET6;
			memcpy(&n->address.v6, &((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_addr,
			    sizeof(struct in6_addr));
			memcpy(&n->mask.v6, &((struct sockaddr_in6 *)
			    ifa->ifa_netmask)->sin6_addr,
			    sizeof(struct in6_addr));
		}
		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}

	iftab = h;
	freeifaddrs(ifap);
}

int
ifa_exists(const char *ifa_name)
{
	struct ipsec_addr_wrap	*n;
	struct ifgroupreq	 ifgr;
	int			 s;

	if (iftab == NULL)
		ifa_load();

	/* check wether this is a group */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(1, "ifa_exists: socket");
	bzero(&ifgr, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, ifa_name, sizeof(ifgr.ifgr_name));
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == 0) {
		close(s);
		return (1);
	}
	close(s);

	for (n = iftab; n; n = n->next) {
		if (n->af == AF_LINK && !strncmp(n->name, ifa_name,
		    IFNAMSIZ))
			return (1);
	}

	return (0);
}

struct ipsec_addr_wrap *
ifa_grouplookup(const char *ifa_name)
{
	struct ifg_req		*ifg;
	struct ifgroupreq	 ifgr;
	int			 s;
	size_t			 len;
	struct ipsec_addr_wrap	*n, *h = NULL, *hn;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");
	bzero(&ifgr, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, ifa_name, sizeof(ifgr.ifgr_name));
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1) {
		close(s);
		return (NULL);
	}

	len = ifgr.ifgr_len;
	if ((ifgr.ifgr_groups = calloc(1, len)) == NULL)
		err(1, "calloc");
	if (ioctl(s, SIOCGIFGMEMB, (caddr_t)&ifgr) == -1)
		err(1, "ioctl");

	for (ifg = ifgr.ifgr_groups; ifg && len >= sizeof(struct ifg_req);
	    ifg++) {
		len -= sizeof(struct ifg_req);
		if ((n = ifa_lookup(ifg->ifgrq_member)) == NULL)
			continue;
		if (h == NULL)
			h = n;
		else {
			for (hn = h; hn->next != NULL; hn = hn->next)
				;	/* nothing */
			hn->next = n;
			n->tail = hn;
		}
	}
	free(ifgr.ifgr_groups);
	close(s);

	return (h);
}

struct ipsec_addr_wrap *
ifa_lookup(const char *ifa_name)
{
	struct ipsec_addr_wrap	*p = NULL, *h = NULL, *n = NULL;

	if (iftab == NULL)
		ifa_load();

	if ((n = ifa_grouplookup(ifa_name)) != NULL)
		return (n);

	for (p = iftab; p; p = p->next) {
		if (p->af != AF_INET && p->af != AF_INET6)
			continue;
		if (strncmp(p->name, ifa_name, IFNAMSIZ))
			continue;
		n = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (n == NULL)
			err(1, "ifa_lookup: calloc");
		memcpy(n, p, sizeof(struct ipsec_addr_wrap));
		if ((n->name = strdup(p->name)) == NULL)
			err(1, "ifa_lookup: strdup");
		switch (n->af) {
		case AF_INET:
			set_ipmask(n, 32);
			break;
		case AF_INET6:
			/* route/show.c and bgpd/util.c give KAME credit */
			if (IN6_IS_ADDR_LINKLOCAL(&n->address.v6)) {
				u_int16_t tmp16;
				/* for now we can not handle link local,
				 * therefore bail for now
				 */
				free(n);
				continue;

				memcpy(&tmp16, &n->address.v6.s6_addr[2],
				    sizeof(tmp16));
				/* use this when we support link-local
				 * n->??.scopeid = ntohs(tmp16);
				 */
				n->address.v6.s6_addr[2] = 0;
				n->address.v6.s6_addr[3] = 0;
			}
			set_ipmask(n, 128);
			break;
		}

		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}

	return (h);
}

void
set_ipmask(struct ipsec_addr_wrap *address, u_int8_t b)
{
	struct ipsec_addr	*ipa;
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

struct ipsec_life *
parse_life(int value)
{
	struct ipsec_life	*life;

	life = calloc(1, sizeof(struct ipsec_life));
	if (life == NULL)
		err(1, "calloc");

	life->lifetime = value;

	return (life);
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

struct ipsec_life *
copylife(const struct ipsec_life *life)
{
	struct ipsec_life *newlife;

	if (life == NULL)
		return (NULL);

	newlife = calloc(1, sizeof(struct ipsec_life));
	if (newlife == NULL)
		err(1, "copylife: calloc");

	memcpy(newlife, life, sizeof(struct ipsec_life));
	return (newlife);
}

struct ipsec_auth *
copyipsecauth(const struct ipsec_auth *auth)
{
	struct ipsec_auth	*newauth;

	if (auth == NULL)
		return (NULL);

	if ((newauth = calloc(1, sizeof(struct ipsec_auth))) == NULL)
		err(1, "calloc");
	if (auth->srcid &&
	    asprintf(&newauth->srcid, "%s", auth->srcid) == -1)
		err(1, "asprintf");
	if (auth->dstid &&
	    asprintf(&newauth->dstid, "%s", auth->dstid) == -1)
		err(1, "asprintf");

	newauth->srcid_type = auth->srcid_type;
	newauth->dstid_type = auth->dstid_type;
	newauth->type = auth->type;

	return (newauth);
}

struct ike_auth *
copyikeauth(const struct ike_auth *auth)
{
	struct ike_auth	*newauth;

	if (auth == NULL)
		return (NULL);

	if ((newauth = calloc(1, sizeof(struct ike_auth))) == NULL)
		err(1, "calloc");
	if (auth->string &&
	    asprintf(&newauth->string, "%s", auth->string) == -1)
		err(1, "asprintf");

	newauth->type = auth->type;

	return (newauth);
}

struct ipsec_key *
copykey(struct ipsec_key *key)
{
	struct ipsec_key	*newkey;

	if (key == NULL)
		return (NULL);

	if ((newkey = calloc(1, sizeof(struct ipsec_key))) == NULL)
		err(1, "calloc");
	if ((newkey->data = calloc(key->len, sizeof(u_int8_t))) == NULL)
		err(1, "calloc");
	memcpy(newkey->data, key->data, key->len);
	newkey->len = key->len;

	return (newkey);
}

struct ipsec_addr_wrap *
copyhost(const struct ipsec_addr_wrap *src)
{
	struct ipsec_addr_wrap *dst;

	if (src == NULL)
		return (NULL);

	dst = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (dst == NULL)
		err(1, "copyhost: calloc");

	memcpy(dst, src, sizeof(struct ipsec_addr_wrap));

	if (src->name != NULL && (dst->name = strdup(src->name)) == NULL)
		err(1, "copyhost: strdup");

	return dst;
}

char *
copytag(const char *src)
{
	char *tag;

	if (src == NULL)
		return (NULL);
	if ((tag = strdup(src)) == NULL)
		err(1, "copytag: strdup");

	return (tag);
}

struct ipsec_rule *
copyrule(struct ipsec_rule *rule)
{
	struct ipsec_rule	*r;

	if ((r = calloc(1, sizeof(struct ipsec_rule))) == NULL)
		err(1, "calloc");

	r->src = copyhost(rule->src);
	r->dst = copyhost(rule->dst);
	r->local = copyhost(rule->local);
	r->peer = copyhost(rule->peer);
	r->auth = copyipsecauth(rule->auth);
	r->ikeauth = copyikeauth(rule->ikeauth);
	r->xfs = copytransforms(rule->xfs);
	r->p1xfs = copytransforms(rule->p1xfs);
	r->p2xfs = copytransforms(rule->p2xfs);
	r->p1life = copylife(rule->p1life);
	r->p2life = copylife(rule->p2life);
	r->authkey = copykey(rule->authkey);
	r->enckey = copykey(rule->enckey);
	r->tag = copytag(rule->tag);

	r->p1ie = rule->p1ie;
	r->p2ie = rule->p2ie;
	r->type = rule->type;
	r->satype = rule->satype;
	r->proto = rule->proto;
	r->tmode = rule->tmode;
	r->direction = rule->direction;
	r->flowtype = rule->flowtype;
	r->sport = rule->sport;
	r->dport = rule->dport;
	r->ikemode = rule->ikemode;
	r->spi = rule->spi;
	r->nr = rule->nr;

	return (r);
}

int
validate_af(struct ipsec_addr_wrap *src, struct ipsec_addr_wrap *dst)
{
	struct ipsec_addr_wrap *ta;
	u_int8_t src_v4 = 0;
	u_int8_t dst_v4 = 0;
	u_int8_t src_v6 = 0;
	u_int8_t dst_v6 = 0;

	for (ta = src; ta; ta = ta->next) {
		if (ta->af == AF_INET)
			src_v4 = 1;
		if (ta->af == AF_INET6)
			src_v6 = 1;
		if (ta->af == AF_UNSPEC)
			return 0;
		if (src_v4 && src_v6)
			break;
	}
	for (ta = dst; ta; ta = ta->next) {
		if (ta->af == AF_INET)
			dst_v4 = 1;
		if (ta->af == AF_INET6)
			dst_v6 = 1;
		if (ta->af == AF_UNSPEC)
			return 0;
		if (dst_v4 && dst_v6)
			break;
	}
	if (src_v4 != dst_v4 && src_v6 != dst_v6)
		return (1);

	return (0);
}


int
validate_sa(u_int32_t spi, u_int8_t satype, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey, u_int8_t tmode)
{
	/* Sanity checks */
	if (spi == 0) {
		yyerror("no SPI specified");
		return (0);
	}
	if (satype == IPSEC_AH) {
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
		if (xfs->compxf) {
			yyerror("ah does not provide compression");
			return (0);
		}
	}
	if (satype == IPSEC_ESP) {
		if (!xfs) {
			yyerror("no transforms specified");
			return (0);
		}
		if (xfs->compxf) {
			yyerror("esp does not provide compression");
			return (0);
		}
		if (!xfs->authxf)
			xfs->authxf = &authxfs[AUTHXF_HMAC_SHA2_256];
		if (!xfs->encxf)
			xfs->encxf = &encxfs[ENCXF_AES];
	}
	if (satype == IPSEC_IPCOMP) {
		if (!xfs) {
			yyerror("no transform specified");
			return (0);
		}
		if (xfs->authxf || xfs->encxf) {
			yyerror("no encryption or authentication with ipcomp");
			return (0);
		}
		if (!xfs->compxf)
			xfs->compxf = &compxfs[COMPXF_DEFLATE];
	}
	if (satype == IPSEC_IPIP) {
		if (!xfs) {
			yyerror("no transform specified");
			return (0);
		}
		if (xfs->authxf || xfs->encxf || xfs->compxf) {
			yyerror("no encryption, authentication or compression"
			    " with ipip");
			return (0);
		}
	}
	if (satype == IPSEC_TCPMD5 && authkey == NULL && tmode !=
	    IPSEC_TRANSPORT) {
		yyerror("authentication key needed for tcpmd5");
		return (0);
	}
	if (xfs && xfs->authxf) {
		if (!authkey && xfs->authxf != &authxfs[AUTHXF_NONE]) {
			yyerror("no authentication key specified");
			return (0);
		}
		if (authkey && authkey->len != xfs->authxf->keymin) {
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
				yyerror("encryption key too short (%d bits), "
				    "minimum %d bits", enckey->len * 8,
				    xfs->encxf->keymin * 8);
				return (0);
			}
			if (xfs->encxf->keymax < enckey->len) {
				yyerror("encryption key too long (%d bits), "
				    "maximum %d bits", enckey->len * 8,
				    xfs->encxf->keymax * 8);
				return (0);
			}
		}
	}

	return 1;
}

int
add_sagroup(struct ipsec_rule *r)
{
	struct ipsec_rule	*rp, *last, *group;
	int			 found = 0;

	TAILQ_FOREACH(rp, &ipsec->group_queue, group_entry) {
		if ((strcmp(rp->src->name, r->src->name) == 0) &&
		    (strcmp(rp->dst->name, r->dst->name) == 0)) {
			found = 1;
			break;
		}
	}
	if (found) {
		last = TAILQ_LAST(&rp->dst_group_queue, dst_group_queue);
		TAILQ_INSERT_TAIL(&rp->dst_group_queue, r, dst_group_entry);

		group = create_sagroup(last->dst, last->satype, last->spi,
		    r->dst, r->satype, r->spi);
		if (group == NULL)
			return (1);
		group->nr = ipsec->rule_nr++;
		if (ipsecctl_add_rule(ipsec, group))
			return (1);
	} else {
		TAILQ_INSERT_TAIL(&ipsec->group_queue, r, group_entry);
		TAILQ_INIT(&r->dst_group_queue);
		TAILQ_INSERT_TAIL(&r->dst_group_queue, r, dst_group_entry);
	}

	return (0);
}

struct ipsec_rule *
create_sa(u_int8_t satype, u_int8_t tmode, struct ipsec_hosts *hosts,
    u_int32_t spi, struct ipsec_transforms *xfs, struct ipsec_key *authkey,
    struct ipsec_key *enckey)
{
	struct ipsec_rule *r;

	if (validate_sa(spi, satype, xfs, authkey, enckey, tmode) == 0)
		return (NULL);

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_sa: calloc");

	r->type |= RULE_SA;
	r->satype = satype;
	r->tmode = tmode;
	r->src = hosts->src;
	r->dst = hosts->dst;
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

	if (validate_sa(spi, rule->satype, rule->xfs, authkey, enckey,
	    rule->tmode) == 0)
		return (NULL);

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "reverse_sa: calloc");

	reverse->type |= RULE_SA;
	reverse->satype = rule->satype;
	reverse->tmode = rule->tmode;
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	reverse->spi = spi;
	reverse->xfs = copytransforms(rule->xfs);
	reverse->authkey = authkey;
	reverse->enckey = enckey;

	return (reverse);
}

struct ipsec_rule *
create_sagroup(struct ipsec_addr_wrap *dst, u_int8_t proto, u_int32_t spi,
    struct ipsec_addr_wrap *dst2, u_int8_t proto2, u_int32_t spi2)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_sagroup: calloc");

	r->type |= RULE_GROUP;

	r->dst = copyhost(dst);
	r->dst2 = copyhost(dst2);
	r->proto = proto;
	r->proto2 = proto2;
	r->spi = spi;
	r->spi2 = spi2;
	r->satype = proto;

	return (r);
}

struct ipsec_rule *
create_flow(u_int8_t dir, u_int8_t proto, struct ipsec_hosts *hosts,
    struct ipsec_hosts *peers,
    u_int8_t satype, char *srcid, char *dstid, u_int8_t type)
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

	r->satype = satype;
	r->proto = proto;
	r->src = hosts->src;
	r->sport = hosts->sport;
	r->dst = hosts->dst;
	r->dport = hosts->dport;
	if ((hosts->sport != 0 || hosts->dport != 0) &&
	    (proto != IPPROTO_TCP && proto != IPPROTO_UDP)) {
		yyerror("no protocol supplied with source/destination ports");
		goto errout;
	}

	if (type == TYPE_DENY || type == TYPE_BYPASS) {
		r->flowtype = type;
		return (r);
	}

	r->flowtype = type;
	r->local = peers->src;
	if (peers->dst == NULL) {
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
		r->peer = peers->dst;

	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_flow: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->srcid_type = get_id_type(srcid);
	r->auth->dstid_type = get_id_type(dstid);
	return r;

errout:
	free(r);
	if (srcid)
		free(srcid);
	if (dstid)
		free(dstid);
	free(hosts->src);
	hosts->src = NULL;
	free(hosts->dst);
	hosts->dst = NULL;

	return NULL;
}

void
expand_any(struct ipsec_addr_wrap *ipa_in)
{
	struct ipsec_addr_wrap *oldnext, *ipa;

	for (ipa = ipa_in; ipa; ipa = ipa->next) {
		if (ipa->af != AF_UNSPEC)
			continue;
		oldnext = ipa->next;

		ipa->af = AF_INET;
		ipa->netaddress = 1;
		if ((ipa->name = strdup("0.0.0.0/0")) == NULL)
			err(1, "expand_any: strdup");

		ipa->next = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (ipa->next == NULL)
			err(1, "expand_any: calloc");
		ipa->next->af = AF_INET6;
		ipa->next->netaddress = 1;
		if ((ipa->next->name = strdup("::/0")) == NULL)
			err(1, "expand_any: strdup");

		ipa->next->next = oldnext;
	}
}

int
expand_rule(struct ipsec_rule *rule, u_int8_t direction, u_int32_t spi,
    struct ipsec_key *authkey, struct ipsec_key *enckey, int group)
{
	struct ipsec_rule	*r, *revr;
	struct ipsec_addr_wrap	*src, *dst;
	int added = 0;

	if (validate_af(rule->src, rule->dst)) {
		yyerror("source/destination address families do not match");
		goto errout;
	}
	expand_any(rule->src);
	expand_any(rule->dst);
	expand_any(rule->peer);
	for (src = rule->src; src; src = src->next) {
		for (dst = rule->dst; dst; dst = dst->next) {
			if (src->af != dst->af)
				continue;
			r = copyrule(rule);

			r->src = copyhost(src);
			r->dst = copyhost(dst);

			r->nr = ipsec->rule_nr++;
			if (ipsecctl_add_rule(ipsec, r))
				return (1);
			if (group && add_sagroup(r))
				return (1);

			if (direction == IPSEC_INOUT) {
				/* Create and add reverse flow rule. */
				revr = reverse_rule(r);
				if (revr == NULL)
					return (1);

				revr->nr = ipsec->rule_nr++;
				if (ipsecctl_add_rule(ipsec, revr))
					return (1);
				if (group && add_sagroup(revr))
					return (1);
			} else if (spi != 0 || authkey || enckey) {
				/* Create and add reverse sa rule. */
				revr = reverse_sa(r, spi, authkey, enckey);
				if (revr == NULL)
					return (1);

				revr->nr = ipsec->rule_nr++;
				if (ipsecctl_add_rule(ipsec, revr))
					return (1);
				if (group && add_sagroup(revr))
					return (1);
			}
			added++;
		}
	}
	if (!added)
		yyerror("rule expands to no valid combination");
 errout:
	ipsecctl_free_rule(rule);
	return (0);
}

struct ipsec_rule *
reverse_rule(struct ipsec_rule *rule)
{
	struct ipsec_rule *reverse;

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "reverse_rule: calloc");

	reverse->type |= RULE_FLOW;

	/* Reverse direction */
	if (rule->direction == (u_int8_t)IPSEC_OUT)
		reverse->direction = (u_int8_t)IPSEC_IN;
	else
		reverse->direction = (u_int8_t)IPSEC_OUT;

	reverse->flowtype = rule->flowtype;
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	reverse->sport = rule->dport;
	reverse->dport = rule->sport;
	if (rule->local)
		reverse->local = copyhost(rule->local);
	if (rule->peer)
		reverse->peer = copyhost(rule->peer);
	reverse->satype = rule->satype;
	reverse->proto = rule->proto;

	if (rule->auth) {
		reverse->auth = calloc(1, sizeof(struct ipsec_auth));
		if (reverse->auth == NULL)
			err(1, "reverse_rule: calloc");
		if (rule->auth->dstid && (reverse->auth->dstid =
		    strdup(rule->auth->dstid)) == NULL)
			err(1, "reverse_rule: strdup");
		if (rule->auth->srcid && (reverse->auth->srcid =
		    strdup(rule->auth->srcid)) == NULL)
			err(1, "reverse_rule: strdup");
		reverse->auth->srcid_type = rule->auth->srcid_type;
		reverse->auth->dstid_type = rule->auth->dstid_type;
		reverse->auth->type = rule->auth->type;
	}

	return reverse;
}

struct ipsec_rule *
create_ike(u_int8_t proto, struct ipsec_hosts *hosts, struct ipsec_hosts *peers,
    struct ike_mode *phase1mode, struct ike_mode *phase2mode, u_int8_t satype,
    u_int8_t tmode, u_int8_t mode, char *srcid, char *dstid,
    struct ike_auth *authtype, char *tag)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_ike: calloc");

	r->type = RULE_IKE;

	r->proto = proto;
	r->src = hosts->src;
	r->sport = hosts->sport;
	r->dst = hosts->dst;
	r->dport = hosts->dport;
	if ((hosts->sport != 0 || hosts->dport != 0) &&
	    (proto != IPPROTO_TCP && proto != IPPROTO_UDP)) {
		yyerror("no protocol supplied with source/destination ports");
		free(r);
		free(hosts->src);
		hosts->src = NULL;
		free(hosts->dst);
		hosts->dst = NULL;
		if (phase1mode) {
			free(phase1mode->xfs);
			phase1mode->xfs = NULL;
			free(phase1mode->life);
			phase1mode->life = NULL;
		}
		if (phase2mode) {
			free(phase2mode->xfs);
			phase2mode->xfs = NULL;
			free(phase2mode->life);
			phase2mode->life = NULL;
		}
		if (srcid)
			free(srcid);
		if (dstid)
			free(dstid);
		return NULL;
	}

	if (peers->dst == NULL) {
		/* Set peer to remote host.  Must be a host address. */
		if (r->direction == IPSEC_IN) {
			if (r->src->netaddress)
				r->peer = NULL;
			else
				r->peer = copyhost(r->src);
		} else {
			if (r->dst->netaddress)
				r->peer = NULL;
			else
				r->peer = copyhost(r->dst);
		}
	} else
		r->peer = peers->dst;

	if (peers->src)
		r->local = peers->src;

	r->satype = satype;
	r->tmode = tmode;
	r->ikemode = mode;
	if (phase1mode) {
		r->p1xfs = phase1mode->xfs;
		r->p1life = phase1mode->life;
		r->p1ie = phase1mode->ike_exch;
	} else {
		r->p1ie = IKE_MM;
	}
	if (phase2mode) {
		r->p2xfs = phase2mode->xfs;
		r->p2life = phase2mode->life;
		r->p2ie = phase2mode->ike_exch;
	} else {
		r->p2ie = IKE_QM;
	}

	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_ike: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->srcid_type = get_id_type(srcid);
	r->auth->dstid_type = get_id_type(dstid);
	r->ikeauth = calloc(1, sizeof(struct ike_auth));
	if (r->ikeauth == NULL)
		err(1, "create_ike: calloc");
	r->ikeauth->type = authtype->type;
	r->ikeauth->string = authtype->string;
	r->tag = tag;

	return (r);
}

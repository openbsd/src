/*	$OpenBSD: asr.c,v 1.9 2011/07/13 15:08:24 eric Exp $	*/
/*
 * Copyright (c) 2010,2011 Eric Faurot <eric@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr.h"
#include "dnsutil.h"

#define unused __attribute__ ((unused))

#define DEFAULT_CONFFILE	"/etc/resolv.conf"
#define DEFAULT_HOSTFILE	"/etc/hosts"
#define DEFAULT_CONF		"lookup bind file\nnameserver 127.0.0.1\n"
#define DEFAULT_LOOKUP		"lookup bind file"

#define ASR_MAXNS	5
#define ASR_MAXDB	3
#define ASR_MAXDOM	10

enum asr_query_type {
	ASR_QUERY_DNS,
	ASR_QUERY_HOST,
	ASR_QUERY_ADDRINFO,
	ASR_QUERY_CNAME,
};

enum asr_db_type {
	ASR_DB_FILE,
	ASR_DB_DNS,
	ASR_DB_YP,
};
struct asr_db {
	int		 ad_type;
	char		*ad_path;
	int		 ad_timeout;
	int		 ad_retries;
	int		 ad_count;
	struct sockaddr *ad_sa[ASR_MAXNS];
};

struct asr_ctx {
	int		 ac_refcount;
	int		 ac_ndots;
	int		 ac_forcetcp;
	char		*ac_domain;
	int		 ac_domcount;
	char		*ac_dom[ASR_MAXDOM];
	int		 ac_dbcount;
	struct asr_db	 ac_db[ASR_MAXDB];
	int		 ac_family[3];
};

struct asr {
	char		*a_path;
	time_t		 a_mtime;
	struct asr_ctx	*a_ctx;
};

struct asr_query {

	struct asr_ctx		*aq_ctx;
	int			 aq_type;
	int			 aq_flags;
	int			 aq_state;

	int			 aq_timeout;
	int			 aq_fd;

	int			 aq_dom_idx;
	int			 aq_family_idx;
	int			 aq_db_idx;
	int			 aq_ns_idx;
	int			 aq_ns_cycles;
	/* for dns */
	char 			*aq_fqdn;   /* the fqdn being looked for */
	struct query		 aq_query;
	uint16_t		 aq_reqid;
	char			*aq_buf;
	size_t			 aq_buflen;
	size_t			 aq_bufsize;
	size_t			 aq_bufoffset; /* for TCP */
	uint16_t		 aq_datalen; /* for TCP */
	struct packed		 aq_packed;
	int			 aq_nanswer;

	/* for host */
	char			*aq_host;
	int			 aq_family;
	int			 aq_count;
	FILE			*aq_file;

	/* for cname */
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	}	aq_sa;

	/* for addrinfo */
	char			*aq_hostname;
	char			*aq_servname;
	struct addrinfo		 aq_hints;
	struct asr_query	*aq_subq;
	struct addrinfo		*aq_aifirst;
	struct addrinfo		*aq_ailast;
};

#define AQ_FAMILY(p) ((p)->aq_ctx->ac_family[(p)->aq_family_idx])
#define AQ_DB(p) (&((p)->aq_ctx->ac_db[(p)->aq_db_idx]))
#define AQ_NS_SA(p) (AQ_DB(p)->ad_sa[(p)->aq_ns_idx])
#define AQ_BUF_LEFT(p) ((p)->aq_bufsize -  (p)->aq_buflen)
#define AQ_BUF_DATA(p) ((p)->aq_buf +  (p)->aq_bufoffset)
#define AQ_BUF_LEN(p) ((p)->aq_buflen - (p)->aq_bufoffset)
#define AQ_BUF_WPOS(p) ((p)->aq_buf + (p)->aq_buflen)
 
enum asr_state {
	ASR_STATE_INIT,
	ASR_STATE_NEXT_DOMAIN,
	ASR_STATE_SEARCH_DOMAIN,
	ASR_STATE_NEXT_DB,
	ASR_STATE_QUERY_DB,
	ASR_STATE_NEXT_FAMILY,
	ASR_STATE_LOOKUP_FAMILY,
	ASR_STATE_NEXT_NS,
	ASR_STATE_QUERY_NS,
	ASR_STATE_READ_RR,
	ASR_STATE_QUERY_FILE,
	ASR_STATE_READ_FILE,
	ASR_STATE_UDP_SEND,
	ASR_STATE_UDP_RECV,
	ASR_STATE_TCP_WRITE,
	ASR_STATE_TCP_READ,
	ASR_STATE_PACKET,
	ASR_STATE_SUBQUERY,
	ASR_STATE_HALT,
};

/* misc utility functions */

int   asr_ndots(const char *);
int   asr_is_fqdn(const char *);
int   asr_cmp_fqdn_name(const char*, char*);
char *asr_make_fqdn(const char *, const char *);
int   asr_parse_nameserver(struct sockaddr *, const char *);

/* query functions */
int asr_run_dns(struct asr_query *, struct asr_result *);
int asr_run_host(struct asr_query *, struct asr_result *);
int asr_run_addrinfo(struct asr_query *, struct asr_result *);
int asr_run_cname(struct asr_query *, struct asr_result *);

/* a few helpers */
const char * asr_error(int);

void asr_check_reload(struct asr *);
void asr_query_free(struct asr_query *);
int  asr_iter_family(struct asr_query *, int);
int  asr_ensure_buf(struct asr_query *, size_t);
int  asr_setup_packet(struct asr_query *);
int  asr_validate_packet(struct asr_query *);
int  asr_udp_send(struct asr_query *);
int  asr_udp_recv(struct asr_query *);
int  asr_tcp_write(struct asr_query *);
int  asr_tcp_read(struct asr_query *);
int  asr_parse_hosts_cb(char **, int, void*, void*);
int  asr_parse_namedb_line(FILE *, char **, int);
int  asr_get_port(const char *, const char *, int);
int  asr_add_sockaddr(struct asr_query *, struct sockaddr *);
int  asr_add_sockaddr2(struct asr_query *, struct sockaddr *, int, int);
int  asr_db_add_nameserver(struct asr_db *, const char *);
void asr_db_done(struct asr_db *);

struct asr_ctx *asr_ctx_create(void);
int		asr_ctx_unref(struct asr_ctx *);
int		asr_ctx_add_searchdomain(struct asr_ctx *, const char *);
int		asr_ctx_from_file(struct asr_ctx *, const char *);
int		asr_ctx_from_string(struct asr_ctx *, const char *);
int		asr_ctx_parse_cb(const char *,
		 		 int (*)(char**, int, void*, void*),
				 void *, void *);
struct asr_query *asr_ctx_query(struct asr_ctx *, int);
struct asr_query *asr_ctx_query_host(struct asr_ctx *, const char *, int);

#ifdef ASR_DEBUG

void asr_dump(struct asr *);
void asr_dump_query(struct asr_query *);

struct kv { int code; const char *name; };

static const char*	kvlookup(struct kv *, int);

int asr_debug = 0;

void
asr_dump(struct asr *a)
{
	char		 buf[256];
	int		 i, j;
	struct asr_db	*ad;
	struct asr_ctx	*ac;

	ac = a->a_ctx;

	printf("--------- ASR CONFIG ---------------\n");
	printf("DOMAIN \"%s\"\n", ac->ac_domain);
	printf("SEARCH\n");
	for(i = 0; i < ac->ac_domcount; i++)
		printf("   \"%s\"\n", ac->ac_dom[i]);
	printf("OPTIONS\n");
	printf(" forcetcp: %i\n", ac->ac_forcetcp);
	printf(" ndots: %i\n", ac->ac_ndots);
	printf(" family: ");
	for(i = 0; ac->ac_family[i] != -1; i++)
		printf(" %s", (ac->ac_family[i] == AF_INET) ? "inet" : "inet6");
	printf("\n");
	printf("DB\n");
	for(ad = ac->ac_db, i = 0; i < ac->ac_dbcount; i++, ad++) {
		switch (ad->ad_type) {
		case ASR_DB_FILE:
			printf("   FILE \"%s\"\n", ad->ad_path);
			break;
		case ASR_DB_DNS:
			printf("   DNS timeout %ims, retries %i\n",
				ad->ad_timeout,
				ad->ad_retries);
			for(j = 0; j < ad->ad_count; j++)
				printf("     NS %s\n",
				    print_addr(ad->ad_sa[j], buf,
					sizeof buf));
			break;
		case ASR_DB_YP:
			printf("   YP\n");
			break;
		default:
			printf(" - ???? %i\n", ad->ad_type);
		}
	}
	printf("------------------------------------\n");
}

static const char *
kvlookup(struct kv *kv, int code)
{
	while (kv->name) {
		if (kv->code == code)
			return (kv->name);
		kv++;
	}
	return "???";
}

struct kv kv_query_type[] = {
	{ ASR_QUERY_DNS,		"ASR_QUERY_DNS"			},
	{ ASR_QUERY_HOST,		"ASR_QUERY_HOST"		},
	{ ASR_QUERY_ADDRINFO,		"ASR_QUERY_ADDRINFO"		},
	{ ASR_QUERY_CNAME,		"ASR_QUERY_CNAME"		},
	{ 0, NULL }
};

struct kv kv_db_type[] = {
	{ ASR_DB_FILE,			"ASR_DB_FILE"			},
	{ ASR_DB_DNS,			"ASR_DB_DNS"			},
	{ ASR_DB_YP,			"ASR_DB_YP"			},
	{ 0, NULL }
};

struct kv kv_state[] = {
	{ ASR_STATE_INIT,		"ASR_STATE_INIT"		},
	{ ASR_STATE_NEXT_DOMAIN,	"ASR_STATE_NEXT_DOMAIN"		},
	{ ASR_STATE_SEARCH_DOMAIN,	"ASR_STATE_SEARCH_DOMAIN"	},
	{ ASR_STATE_NEXT_DB,		"ASR_STATE_NEXT_DB"		},
	{ ASR_STATE_QUERY_DB,		"ASR_STATE_QUERY_DB"		},
	{ ASR_STATE_NEXT_FAMILY,	"ASR_STATE_NEXT_FAMILY"		},
	{ ASR_STATE_LOOKUP_FAMILY,	"ASR_STATE_LOOKUP_FAMILY"	},
	{ ASR_STATE_NEXT_NS,		"ASR_STATE_NEXT_NS"		},
	{ ASR_STATE_QUERY_NS,		"ASR_STATE_QUERY_NS"		},
	{ ASR_STATE_READ_RR,		"ASR_STATE_READ_RR"		},
	{ ASR_STATE_QUERY_FILE,		"ASR_STATE_QUERY_FILE"		},
	{ ASR_STATE_READ_FILE,		"ASR_STATE_READ_FILE"		},
	{ ASR_STATE_UDP_SEND,		"ASR_STATE_UDP_SEND"		},
	{ ASR_STATE_UDP_RECV,		"ASR_STATE_UDP_RECV"		},
	{ ASR_STATE_TCP_WRITE,		"ASR_STATE_TCP_WRITE"		},
	{ ASR_STATE_TCP_READ,		"ASR_STATE_TCP_READ"		},
	{ ASR_STATE_PACKET,		"ASR_STATE_PACKET"		},
	{ ASR_STATE_SUBQUERY,		"ASR_STATE_SUBQUERY"		},
	{ ASR_STATE_HALT,		"ASR_STATE_HALT"		},
	{ 0, NULL }
};

struct kv kv_transition[] = {
	{ ASR_COND,			"ASR_COND"			},
	{ ASR_YIELD,			"ASR_YIELD"			},
	{ ASR_DONE,			"ASR_DONE"			},
        { 0, NULL }
};

void
asr_dump_query(struct asr_query *aq)
{
	printf("%-25s fqdn=%s dom %-2i fam %-2i famidx %-2i db %-2i ns %-2i ns_cycles %-2i fd %-2i %ims",
		kvlookup(kv_state, aq->aq_state),
		aq->aq_fqdn,
		aq->aq_dom_idx,
		aq->aq_family,
		aq->aq_family_idx,
		aq->aq_db_idx,
		aq->aq_ns_idx,
		aq->aq_ns_cycles,
		aq->aq_fd,
		aq->aq_timeout);
	printf("\n");
}

#endif /* ASR_DEBUG */

struct asr *
asr_resolver(const char *conf)
{
	int		 r;
	struct asr	*asr;

#ifdef ASR_DEBUG
	if (asr_debug == 0)
		if(getenv("ASR_DEBUG")) {
		printf("asr: %zu\n", sizeof(struct asr));
		printf("asr_ctx: %zu\n", sizeof(struct asr_ctx));
		printf("asr_db: %zu\n", sizeof(struct asr_db));
		printf("asr_query: %zu\n", sizeof(struct asr_query));
		printf("asr_result: %zu\n", sizeof(struct asr_result));
		asr_debug = 1;
	}
#endif
	if ((asr = calloc(1, sizeof(*asr))) == NULL)
		return (NULL);

	if ((asr->a_ctx = asr_ctx_create()) == NULL) {
		free(asr);
		return (NULL);
	}

	if (conf == NULL)
		conf = DEFAULT_CONFFILE;

	if (conf[0] == '!') {
		r = asr_ctx_from_string(asr->a_ctx, conf + 1);
	} else {
		r = 0;
		asr->a_path = strdup(conf);
		asr_check_reload(asr);
		if (asr->a_ctx == NULL)
			r = asr_ctx_from_string(asr->a_ctx, DEFAULT_CONF);
	}

	if (r == -1) {
		asr_ctx_unref(asr->a_ctx);
		free(asr);
		return (NULL);
	}

#ifdef ASR_DEBUG
	if (asr_debug)
		asr_dump(asr);
#endif

	return (asr);
}

void
asr_abort(struct asr_query *aq)
{
	asr_query_free(aq);
}

int
asr_run(struct asr_query *aq, struct asr_result *ar)
{
	int	r;

#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("-> QUERY %p(%p) %s\n",
			aq, aq->aq_ctx,
			kvlookup(kv_query_type, aq->aq_type));
	}
#endif

	switch(aq->aq_type) {
	case ASR_QUERY_DNS:
		r = asr_run_dns(aq, ar);
		break;
	case ASR_QUERY_HOST:
		r = asr_run_host(aq, ar);
		break;
	case ASR_QUERY_ADDRINFO:
		r = asr_run_addrinfo(aq, ar);
		break;
	case ASR_QUERY_CNAME:
		r = asr_run_cname(aq, ar);
		break;
	default:
		ar->ar_err = EOPNOTSUPP;
		ar->ar_errstr = "unknown query type";
		r = ASR_DONE;
	}
#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("<- ");
		asr_dump_query(aq);
		printf("   = %s\n", kvlookup(kv_transition, r));
	}
#endif
	if (r == ASR_DONE)
		asr_query_free(aq);

	return (r);
}

int
asr_run_sync(struct asr_query *aq, struct asr_result *ar)
{
	struct pollfd		 fds[1];
	int			 r;

	while((r = asr_run(aq, ar)) == ASR_COND) {
		fds[0].fd = ar->ar_fd;
		fds[0].events = (ar->ar_cond == ASR_READ) ? POLLIN : POLLOUT;
	again:
		r = poll(fds, 1, ar->ar_timeout);
		if (r == -1 && errno == EINTR)
			goto again;
		if (r == -1) /* impossible? */
			err(1, "poll");
	}

	return (r);
}

void
asr_check_reload(struct asr *asr)
{
        struct stat	 st;
	struct asr_ctx	*ac;

	if (asr->a_path == NULL)
		return;

	if (stat(asr->a_path, &st) == -1)
		return;

	if (asr->a_mtime == st.st_mtime)
		return;

	if ((ac = asr_ctx_create()) == NULL)
		return;

	asr->a_mtime = st.st_mtime;

	if (asr_ctx_from_file(ac, asr->a_path) == -1) {
		asr_ctx_unref(ac);
		return;
	}

	if (asr->a_ctx)
		asr_ctx_unref(asr->a_ctx);
	asr->a_ctx = ac;	
}

struct asr_ctx *
asr_ctx_create(void)
{
	struct asr_ctx	*ac;

	if ((ac = calloc(1, sizeof(*ac))) == NULL)
		return (NULL);

	ac->ac_refcount = 1;
	ac->ac_ndots = 1;
	ac->ac_family[0] = AF_INET;
	ac->ac_family[1] = AF_INET6;
	ac->ac_family[2] = -1;

	return (ac);
}

int
asr_ctx_unref(struct asr_ctx *ac)
{
	int	i;

	ac->ac_refcount--;

	if (ac->ac_refcount == 0) {
		if (ac->ac_domain)
			free(ac->ac_domain);

		for(i = 0; i < ac->ac_dbcount; i++)
			asr_db_done(&ac->ac_db[i]);

		for(i = 0; i < ac->ac_domcount; i++)
			free(ac->ac_dom[i]);

		free(ac);
		return (0);
	}

	return (ac->ac_refcount);
}

int
asr_ctx_add_searchdomain(struct asr_ctx *ac, const char *domain)
{
	if (ac->ac_domcount == ASR_MAXDOM)
		return (-1);

	if ((ac->ac_dom[ac->ac_domcount] = asr_make_fqdn(domain, NULL)) == NULL)
		return (0);

	ac->ac_domcount += 1;

	return (1);
}

static int
pass0(char **tok, int n, void *a0, void *a1)
{
	struct asr_ctx	*ac = (struct asr_ctx*)a0;
	struct asr_db	*ad;
	int		*nscount = (int*)a1;
	int		 i, j, d;
	const char	*e;

	/* search for lookup, domain, family, options, and count nameservers */

	if (!strcmp(tok[0], "nameserver")) {
		*nscount += 1;

	} else if (!strcmp(tok[0], "domain")) {
		if (n != 2)
			return (0);
		if (ac->ac_domain)
			return (0);
		ac->ac_domain = strdup(tok[1]);
	} else if (!strcmp(tok[0], "lookup")) {
		/* ignore the line if we already set lookup */
		if (ac->ac_dbcount != 0)
			return (0);
		if (n - 1 > ASR_MAXDB)
			return (0);
		/* ensure that each lookup is only given once */
		for(i = 1; i < n; i++)
			for(j = i + 1; j < n; j++)
				if (!strcmp(tok[i], tok[j]))
					return (0);
		for(i = 1, ad = ac->ac_db; i < n;
		    i++, ac->ac_dbcount++, ad++) {

			if (!strcmp(tok[i], "yp")) {
				ad->ad_type = ASR_DB_YP;

			} else if (!strcmp(tok[i], "bind")) {
				ad->ad_type = ASR_DB_DNS;
				ad->ad_count = 0;
				ad->ad_timeout = 1000;
				ad->ad_retries = 3;

			} else if (!strcmp(tok[i], "file")) {
				ad->ad_type = ASR_DB_FILE;
				ad->ad_path = strdup(DEFAULT_HOSTFILE);
			} else {
				/* ignore the line */
				ac->ac_dbcount = 0;
				return (0);
			}
		}
	} else if (!strcmp(tok[0], "search")) {
		/* resolv.conf says the last line wins */
		for(i = 0; i < ac->ac_domcount; i++)
			free(ac->ac_dom[i]);
		ac->ac_domcount = 0;
		for(i = 1; i < n; i++)
			asr_ctx_add_searchdomain(ac, tok[i]);
	} else if (!strcmp(tok[0], "family")) {
		if (n == 1 || n > 3)
			return (0);
		for (i = 1; i < n; i++)
			if (strcmp(tok[i], "inet4") && strcmp(tok[i], "inet6"))
				return (0);
		for (i = 1; i < n; i++)
			ac->ac_family[i - 1] = strcmp(tok[i], "inet4") ? \
			    AF_INET6 : AF_INET;
		ac->ac_family[i - 1] = -1;
	} else if (!strcmp(tok[0], "option")) {
		for(i = 1; i < n; i++) {
			if (!strcmp(tok[i], "tcp"))
				ac->ac_forcetcp = 1;
			else if ((!strncmp(tok[i], "ndots:", 6))) {
				e = NULL;
				d = strtonum(tok[i] + 6, 1, 16, &e);
				if (e == NULL)
					ac->ac_ndots = d;
			}
		}
	}

	return (0);
}

static int
pass1(char **tok, int n, void *a0, unused void *a1)
{
	struct asr_db *ad = (struct asr_db*) a0;

	/* fill the DNS db with the specified nameservers */

	if (!strcmp(tok[0], "nameserver")) {
		if (n != 2)
			return (0);
		asr_db_add_nameserver(ad, tok[1]);
	}
	return (0);
}

int
asr_ctx_from_string(struct asr_ctx *ac, const char *str)
{
	char		 buf[512], *ch;
	struct asr_db	*ad;
	int		 i;
	int		 nscount = 0;

	asr_ctx_parse_cb(str, pass0, ac, &nscount);

	if (ac->ac_dbcount == 0) {
		/* no lookup directive */
		asr_ctx_parse_cb(DEFAULT_LOOKUP, pass0, ac, &nscount);
	}

	ad = NULL;
	for(i = 0; i < ac->ac_dbcount; i++)
		if (ac->ac_db[i].ad_type == ASR_DB_DNS) {
			ad = &ac->ac_db[i];
			break;
		}

	if (nscount && ad)
		asr_ctx_parse_cb(str, pass1, ad, NULL);

	if (ac->ac_domain == NULL)
		if (gethostname(buf, sizeof buf) == 0) {
			ch = strchr(buf, '.');
			if (ch)
				ac->ac_domain = strdup(ch + 1);
			else /* assume root. see resolv.conf(5) */
				ac->ac_domain = strdup("");
		}

	if (ac->ac_domcount == 0)
		for(ch = ac->ac_domain; ch; ) {
			asr_ctx_add_searchdomain(ac, ch);
			ch = strchr(ch, '.');
			if (ch && asr_ndots(++ch) == 0)
				break;
		}

	return (0);
}

int
asr_ctx_from_file(struct asr_ctx *ac, const char *path)
{
	FILE	*cf;
	char	 buf[1024];
	ssize_t	 r;

	cf = fopen(path, "r");
	if (cf == NULL)
		return (-1);

	/* XXX make sure we read the whole file */
	r = fread(buf, 1, sizeof buf - 1, cf);
	fclose(cf);
	if (r == -1)
		return (-1);
	buf[r] = '\0';
 
	return asr_ctx_from_string(ac, buf);
}

int
asr_ctx_parse_cb(const char	 *str,
		 int		(*cb)(char**, int, void*, void*),
		 void		 *arg0,
		 void		 *arg1)
{
	size_t		 len;
	const char	*line;
	char		 buf[1024];
	char		*tok[10], **tp, *cp;
	int		 ntok;

	line = str;
	while (*line) {
		len = strcspn(line, "\n\0");
		if (len < sizeof buf) {
			memmove(buf, line, len);
			buf[len] = '\0';
		} else
			buf[0] = '\0';
		line += len;
		if (*line == '\n')
			line++;
		buf[strcspn(buf, ";#")] = '\0';
		for(cp = buf, tp = tok, ntok = 0;
		    tp < &tok[10] && (*tp = strsep(&cp, " \t")) != NULL;)
			if (**tp != '\0') {
				tp++;
				ntok++;
			}
		*tp = NULL;

		if (tok[0] == NULL)
			continue;

		if (cb(tok, ntok, arg0, arg1))
			break;
	}

	return (0);
}

struct asr_query *
asr_ctx_query(struct asr_ctx *ac, int type)
{
	struct asr_query	*aq;

	if ((aq = calloc(1, sizeof(*aq))) == NULL)
		return (NULL);

	ac->ac_refcount += 1;

	aq->aq_ctx = ac;
	aq->aq_fd = -1;
	aq->aq_type = type;
	aq->aq_state = ASR_STATE_INIT;

	return (aq);
}

void
asr_done(struct asr *asr)
{
	asr_ctx_unref(asr->a_ctx);
	if (asr->a_path)
		free(asr->a_path);
	free(asr);
}

int
asr_parse_hosts_cb(char **tok, int n, void *a0, void *a1)
{
	struct asr_query	*aq = (struct asr_query*) a0;
	struct asr_result	*ar = (struct asr_result*) a1;
	int			 i;

	for (i = 1; i < n; i++) {
		if (strcmp(tok[i], aq->aq_host))
			continue;
		if (sockaddr_from_str(&ar->ar_sa.sa, aq->aq_family, tok[0]) == -1)
			continue;
		ar->ar_cname = strdup(tok[1]);
		return (1);
	}

	return (0);
}

/*
 * utility functions
 */

int
asr_parse_nameserver(struct sockaddr *sa, const char *s)
{
	const char	*estr;
	char		 buf[256];
	char		*port = NULL;
	in_port_t	 portno = 53;

	if (*s == '[') {
		strlcpy(buf, s + 1, sizeof buf);
		s = buf;
		port = strchr(buf, ']');
		if (port == NULL)
			return (-1);
		*port++ = '\0';
		if (*port != ':')
			return (-1);
		port++;
	}
	
	if (port) {
		portno = strtonum(port, 1, USHRT_MAX, &estr);
		if (estr)
			return (-1);
	}

	if (sockaddr_from_str(sa, PF_UNSPEC, s) == -1)
		return (-1);

	sockaddr_set_port(sa, portno);

	return (0);
}

int
asr_db_add_nameserver(struct asr_db *ad, const char *nameserver)
{
	struct sockaddr_storage	ss;

	if (ad->ad_type != ASR_DB_DNS)
		return (-1);

	if (ad->ad_count == ASR_MAXNS)
		return (-1);

	if (asr_parse_nameserver((struct sockaddr*)&ss, nameserver))
		return (-1);

	if ((ad->ad_sa[ad->ad_count] = calloc(1, ss.ss_len)) == NULL)
		return (0);

	memmove(ad->ad_sa[ad->ad_count], &ss, ss.ss_len);
	ad->ad_count += 1;

	return (1);
}

void
asr_db_done(struct asr_db *ad)
{
	int	i;

	switch(ad->ad_type) {
	case ASR_DB_DNS:
		for(i = 0; i < ad->ad_count; i++)
			free(ad->ad_sa[i]);
		break;

	case ASR_DB_YP:
		break;

	case ASR_DB_FILE:
		free(ad->ad_path);
		break;
	default:
		errx(1, "asr_db_done: unknown db type");
	}
}

int
asr_parse_namedb_line(FILE *file, char **tokens, int ntoken)
{
	size_t	  len;
	char	 *buf, *cp, **tp;
	int	  ntok;

  again:
	if ((buf = fgetln(file, &len)) == NULL)
		return (-1);

	if (buf[len - 1] == '\n')
		len--;

	buf[len] = '\0';
	buf[strcspn(buf, "#")] = '\0';
	for(cp = buf, tp = tokens, ntok = 0;
	    ntok < ntoken && (*tp = strsep(&cp, " \t")) != NULL;)
		if (**tp != '\0') {
			tp++;
			ntok++;
		}
	*tp = NULL;
	if (tokens[0] == NULL)
		goto again;

	return (ntok);
}

const char *
asr_error(int v)
{
	switch(v) {
	case ASR_OK:
		return "no error";
	case EASR_MEMORY:
		return "out of memory";
	case EASR_TIMEDOUT:
		return "all nameservers timed out";
	case EASR_NAMESERVER:
		return "no nameserver specified";
	case EASR_FAMILY:
		return "invalid address family";
	case EASR_NOTFOUND:
		return "not found";
	case EASR_NAME:
		return "invalid domain name";
	default:
		return "unknown error code";
	}
}

int
asr_cmp_fqdn_name(const char *fqdn, char *name)
{
	int i;

	/* compare a fqdn with a name that may not end with a dot */

	for (i = 0; fqdn[i] && name[i]; i++)
		if (fqdn[i] != name[i])
			return (-1);

	if (fqdn[i] == name[i])
		return (0);

	if (fqdn[i] == 0 || fqdn[i] != '.' || fqdn[i+1] != 0)
		return (-1);

	return (0);
}

int
asr_ndots(const char *s)
{
	int n;

	for(n = 0; *s; s++)
		if (*s == '.')
			n += 1;

	return (n);
}

int
asr_is_fqdn(const char *name)
{
	size_t	len;

	len = strlen(name);
	return (len > 0 && name[len -1] == '.');
}

char *
asr_make_fqdn(const char *name, const char *domain)
{
	char	*fqdn;
	size_t	 len;

	if (domain == NULL)
		domain = ".";
#ifdef ASR_DEBUG
	else
		if (!asr_is_fqdn(domain))
			errx(1, "domain is not FQDN: %s", domain);
#endif

	len = strlen(name);
	if (len == 0) {
		fqdn = strdup(domain);
	} else if (name[len - 1] !=  '.') {
		if (domain[0] == '.')
			domain += 1;
		len += strlen(domain) + 2;
		fqdn = malloc(len);
		if (fqdn == NULL)
			return (NULL);
		strlcpy(fqdn, name, len);
		strlcat(fqdn, ".", len);
		strlcat(fqdn, domain, len);
	} else {
		fqdn = strdup(name);
	}

	return (fqdn);
}

void
asr_query_free(struct asr_query *aq)
{
	if (aq->aq_aifirst)
		freeaddrinfo(aq->aq_aifirst);
	if (aq->aq_subq)
		asr_abort(aq->aq_subq);
	if (aq->aq_host)
		free(aq->aq_host);
	if (aq->aq_fqdn)
		free(aq->aq_fqdn);
	if (aq->aq_buf)
		free(aq->aq_buf);
	if (aq->aq_hostname)
		free(aq->aq_hostname);
	if (aq->aq_servname)
		free(aq->aq_servname);
	if (aq->aq_fd != -1)
		close(aq->aq_fd);
	asr_ctx_unref(aq->aq_ctx);
	free(aq);
}

/*
 * for asr_query_dns
 */

struct asr_query *
asr_query_dns(struct asr *asr,
	      uint16_t	 type,
	      uint16_t	 class,
	      const char *name,
	      int	 flags)
{
	struct asr_query	*aq;

	asr_check_reload(asr);

	if ((aq = asr_ctx_query(asr->a_ctx, ASR_QUERY_DNS)) == NULL)
		return (NULL);

	aq->aq_flags = flags;
	aq->aq_query.q_type = type;
	aq->aq_query.q_class = class;
	aq->aq_fqdn = asr_make_fqdn(name, NULL);
	if (aq->aq_fqdn == NULL)
		goto abort;

	return (aq);
    abort:
	asr_query_free(aq);
	return (NULL);
}

int
asr_setup_packet(struct asr_query *aq)
{
	struct packed		 p;
	struct header		 h;

	if (dname_from_fqdn(aq->aq_fqdn,
		aq->aq_query.q_dname,
		sizeof(aq->aq_query.q_dname)) == -1) {
		return (-1);
	}

        aq->aq_reqid = res_randomid();

	memset(&h, 0, sizeof h);
	h.id = aq->aq_reqid;
	if (!(aq->aq_flags & ASR_NOREC))
		h.flags |= RD_MASK;
	h.qdcount = 1;

	if (aq->aq_buf == NULL) {
		aq->aq_bufsize = PACKET_MAXLEN;
		if ((aq->aq_buf = malloc(aq->aq_bufsize)) == NULL)
			return (-2);
	}
	aq->aq_bufoffset = 0;

	packed_init(&p, aq->aq_buf, aq->aq_bufsize);
	pack_header(&p, &h);
	pack_query(&p, aq->aq_query.q_type, aq->aq_query.q_class,
		aq->aq_query.q_dname);
	aq->aq_buflen = p.offset;

	return (0);
}

int
asr_ensure_buf(struct asr_query *aq, size_t n)
{
	char	*t;

	if (aq->aq_buf == NULL) {
		aq->aq_buf = malloc(n);
		if (aq->aq_buf == NULL)
			return (-1);
		aq->aq_bufsize = n;
		return (0);
	}

	if (aq->aq_bufsize > n)
		return (0);

	t = realloc(aq->aq_buf, n);
	if (t == NULL)
		return (-1);
	aq->aq_buf = t;
	aq->aq_bufsize = n;

	return (0);
}

int
asr_validate_packet(struct asr_query *aq)
{
	struct packed	 p;
	struct header	 h;
	struct query	 q;
	struct rr	 rr;
	int		 r;

	packed_init(&p, aq->aq_buf, aq->aq_buflen);

	unpack_header(&p, &h);
	if (p.err)
		return (-1);
	if (h.id != aq->aq_reqid)
		return (-1);
	if (h.qdcount != 1)
		return (-1);
	if ((h.flags & Z_MASK) != 0)
		return (-1);	/* should be zero, we could allow this */
	if (h.flags & TC_MASK)
		return (-2);
	if (OPCODE(h.flags) != OP_QUERY)
		return (-1);	/* actually, it depends on the request */
	if ((h.flags & QR_MASK) == 0)
		return (-1);	/* not a response */

	unpack_query(&p, &q);
	if (p.err)
		return (-1);
	if (q.q_type != aq->aq_query.q_type ||
	    q.q_class != aq->aq_query.q_class ||
	    strcasecmp(q.q_dname, aq->aq_query.q_dname))
		return (-1);

	/* validate the rest of the packet */
	for(r = h.ancount + h.nscount + h.arcount; r; r--)
		unpack_rr(&p, &rr);

	if (p.err || (p.offset != aq->aq_buflen))
		return (-1);

	return (0);
}

int
asr_udp_send(struct asr_query *aq)
{
	ssize_t		 n;
	int		 errno_save;

	aq->aq_fd = sockaddr_connect(AQ_NS_SA(aq), SOCK_DGRAM);
	if (aq->aq_fd == -1)
		return (-1);

	aq->aq_timeout = AQ_DB(aq)->ad_timeout;

	n = send(aq->aq_fd, aq->aq_buf, aq->aq_buflen, 0);
	if (n == -1) {
		errno_save = errno;
		close(aq->aq_fd);
		aq->aq_fd = -1;
		if (errno_save == EAGAIN)
			return (-2); /* timeout */
		return (-1);
	}

	return (0);
}

int
asr_udp_recv(struct asr_query *aq)
{
	ssize_t		 n;
	int		 errno_save;

	n = recv(aq->aq_fd, aq->aq_buf, aq->aq_bufsize, 0);
	errno_save = errno;
	close(aq->aq_fd);
	aq->aq_fd = -1;
	if (n == -1) {
		if (errno_save == EAGAIN)
			return (-2); /* timeout */
		return (-1);
	}

	aq->aq_buflen = n;

	switch (asr_validate_packet(aq)) {
	case -2:
		return (1); /* truncated */
	case -1:
		return (-1);
	default:
		break;
	}

	return (0);
}

int
asr_tcp_write(struct asr_query *aq)
{
	struct iovec	iov[2];
	uint16_t	len;
	ssize_t		n;
	int		i, ret, se;
	socklen_t	sl;

	if (aq->aq_fd == -1) { /* connect */
		aq->aq_fd = sockaddr_connect(AQ_NS_SA(aq), SOCK_STREAM);
		if (aq->aq_fd == -1)
			return (-1);
		aq->aq_timeout = AQ_DB(aq)->ad_timeout;
		return (1);
	}

	ret = -1;
	i = 0;
	if (aq->aq_datalen == 0) {
		/* check connection first */
		sl = sizeof(se);
		if (getsockopt(aq->aq_fd, SOL_SOCKET, SO_ERROR, &se, &sl) == -1) {
			warn("getsockopt");
			goto close;
		}
		if (se)
			goto close;

		/* need to send datalen first */
		len = htons(aq->aq_buflen);
		iov[i].iov_base = &len;
		iov[i].iov_len = sizeof(len);
		i++;
	}

	iov[i].iov_base = AQ_BUF_DATA(aq);
	iov[i].iov_len = AQ_BUF_LEN(aq);
	i++;

	n = writev(aq->aq_fd, iov, i);
	if (n == -1) {
		if (errno == EAGAIN)
			ret = -2;
		else
			warn("writev");
		goto close;
	}

	if (aq->aq_datalen == 0 && n < 2) {
		/* we want to write the data len */
		warnx("short write");
		goto close;
	}
	
	if (aq->aq_datalen == 0) {
		aq->aq_datalen = len;
		n -= 2;
	}

	aq->aq_bufoffset += n;
	if (aq->aq_bufoffset == aq->aq_buflen) {
		aq->aq_datalen = 0;
		return (0); /* all sent */
	}

	aq->aq_timeout = AQ_DB(aq)->ad_timeout;
	return (1);

close:
	close(aq->aq_fd);
	aq->aq_fd = -1;
	return (ret);
}

int
asr_tcp_read(struct asr_query *aq)
{
	uint16_t	len;
	ssize_t		n;
	int		ret;

	ret = -1;

	if (aq->aq_datalen == 0) {
		n = read(aq->aq_fd, &len, sizeof(len));
		if (n == -1) {
			if (errno == EAGAIN) /* timeout */
				ret = -2;
			else
				warn("read");
			goto close;
		}
		if (n < 2) {
			warnx("short read");
			goto close;
		}
		aq->aq_datalen = ntohs(len);
		aq->aq_bufoffset = 0;
		aq->aq_buflen = 0;

		if (asr_ensure_buf(aq, aq->aq_datalen) == -1) {
			ret = -3;
			goto close;
		}

		return (1); /* need more data */
	}

	n = read(aq->aq_fd, AQ_BUF_WPOS(aq), AQ_BUF_LEFT(aq));
	if (n == -1) {
		if (errno == EAGAIN) /* timeout */
			ret = -2;
		else
			warn("read");
		goto close;
	}
	if (n == 0) {
		warnx("closed");
		goto close;
	}
	aq->aq_buflen += n;

	if (aq->aq_buflen != aq->aq_datalen)
		return (1); /* need more data */

	if (asr_validate_packet(aq) != 0)
		goto close;

	ret = 0;

close:
	close(aq->aq_fd);
	aq->aq_fd = -1;
	return (ret);
}

int
asr_run_dns(struct asr_query *aq, struct asr_result *ar)
{
	for(;;) { /* block not indented on purpose */
#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("   ");
		asr_dump_query(aq);
	}
#endif
	switch(aq->aq_state) {

	case ASR_STATE_INIT:
		aq->aq_ns_cycles = -1;
		aq->aq_db_idx = 0;
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_NEXT_DB:
		aq->aq_db_idx += 1;
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_QUERY_DB:
		if (aq->aq_db_idx >= aq->aq_ctx->ac_dbcount) {
			if (aq->aq_ns_cycles == -1)
				ar->ar_err = EASR_NAMESERVER;
			else
				ar->ar_err = EASR_TIMEDOUT;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}

		if (AQ_DB(aq)->ad_type != ASR_DB_DNS) {
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_ns_cycles = 0;
		aq->aq_ns_idx = 0;
		aq->aq_state = ASR_STATE_QUERY_NS;
		break;

	case ASR_STATE_NEXT_NS:
		aq->aq_ns_idx += 1;
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_ns_idx = 0;
			aq->aq_ns_cycles++;
		}
		if (aq->aq_ns_cycles >= AQ_DB(aq)->ad_retries) {
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_state = ASR_STATE_QUERY_NS;
		break;

	case ASR_STATE_QUERY_NS:
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		}
		switch (asr_setup_packet(aq)) {
		case -2:
			ar->ar_err = EASR_MEMORY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		case -1:
			ar->ar_err = EASR_NAME;
			aq->aq_state = ASR_STATE_HALT;
			break;
		default:
			break;
		}
		if (aq->aq_ctx->ac_forcetcp)
			aq->aq_state = ASR_STATE_TCP_WRITE;
		else
			aq->aq_state = ASR_STATE_UDP_SEND;
		break;

	case ASR_STATE_UDP_SEND:
		if (asr_udp_send(aq) == 0) {
			aq->aq_state = ASR_STATE_UDP_RECV;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		aq->aq_state = ASR_STATE_NEXT_NS;
		break;

	case ASR_STATE_UDP_RECV:
		switch (asr_udp_recv(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0: /* done */
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1: /* truncated */
			aq->aq_state = ASR_STATE_TCP_WRITE;
			break;
		}
		break;

	case ASR_STATE_TCP_WRITE:
		switch (asr_tcp_write(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_TCP_READ;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		case 1:
			ar->ar_cond = ASR_WRITE;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_TCP_READ:
		switch (asr_tcp_read(aq)) {
		case -3:
			aq->aq_state = ASR_STATE_HALT;
			ar->ar_err = EASR_MEMORY;
			break;
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1:
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_PACKET:
		memmove(&ar->ar_sa.sa, AQ_NS_SA(aq), AQ_NS_SA(aq)->sa_len);
		ar->ar_datalen = aq->aq_buflen;
		ar->ar_data = aq->aq_buf;
		aq->aq_buf = NULL;
		ar->ar_err = ASR_OK;
		aq->aq_state = ASR_STATE_HALT;
		break;

	case ASR_STATE_HALT:
		ar->ar_errstr = asr_error(ar->ar_err);
		if (ar->ar_err)
			ar->ar_data = NULL;
		return (ASR_DONE);

	default:
		errx(1, "asr_run_dns: unknown state");
	}}
}

/*
 * for asr_query_host
 */

struct asr_query *
asr_query_host(struct asr *asr, const char *host, int family)
{
	asr_check_reload(asr);

	return asr_ctx_query_host(asr->a_ctx, host, family);
}

struct asr_query *
asr_ctx_query_host(struct asr_ctx *ac, const char *host, int family)
{
	struct asr_query	*aq;

	if ((aq = asr_ctx_query(ac, ASR_QUERY_HOST)) == NULL)
		return (NULL);

	aq->aq_family = family;
	aq->aq_host = strdup(host);
	if (aq->aq_host)
		return (aq);

	asr_query_free(aq);
	return (NULL);
}

int
asr_run_host(struct asr_query *aq, struct asr_result *ar)
{
	struct header	 h;
	struct query	 q;
	struct rr	 rr;
	char		*tok[10];
	int		 ntok = 10, i, n, family;

	for(;;) { /* block not indented on purpose */
#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("   ");
		asr_dump_query(aq);
	}
#endif
	switch(aq->aq_state) {

	case ASR_STATE_INIT:
		if (aq->aq_family != AF_INET &&
		    aq->aq_family != AF_INET6 &&
		    aq->aq_family != AF_UNSPEC) {
			ar->ar_err = EASR_FAMILY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		aq->aq_count = 0;
		aq->aq_dom_idx = 0;
		/* check if we need to try it as an absolute name first */
		if (asr_ndots(aq->aq_host) >= aq->aq_ctx->ac_ndots)
			aq->aq_dom_idx = -1;
		aq->aq_state = ASR_STATE_SEARCH_DOMAIN;
		break;

	case ASR_STATE_NEXT_DOMAIN:
		/* no domain search for fully qualified names */
		if (asr_is_fqdn(aq->aq_host)) {
			ar->ar_err = EASR_NOTFOUND;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		aq->aq_dom_idx += 1;
		aq->aq_state = ASR_STATE_SEARCH_DOMAIN;
		break;

	case ASR_STATE_SEARCH_DOMAIN:
		if (aq->aq_dom_idx >= aq->aq_ctx->ac_domcount) {
			ar->ar_err = EASR_NOTFOUND;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		if (aq->aq_fqdn)
			free(aq->aq_fqdn);

		if (aq->aq_dom_idx == -1) /* try as absolute first */
			aq->aq_fqdn = asr_make_fqdn(aq->aq_host, NULL);
		else
			aq->aq_fqdn = asr_make_fqdn(aq->aq_host,
			    aq->aq_ctx->ac_dom[aq->aq_dom_idx]);

		if (aq->aq_fqdn == NULL) {
			ar->ar_err = EASR_MEMORY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		aq->aq_db_idx = 0;
		aq->aq_family_idx = 0;
		aq->aq_state = ASR_STATE_LOOKUP_FAMILY;
		break;

	case ASR_STATE_NEXT_FAMILY:
		aq->aq_family_idx += 1;
		if ((aq->aq_family != AF_UNSPEC) || (AQ_FAMILY(aq) == -1)) {
			/* The family was specified, or we have 
			 *  tried all families with this DB
			 */
			if (aq->aq_count) {
				ar->ar_count = aq->aq_count;
				ar->ar_err = ASR_OK;
				aq->aq_state = ASR_STATE_HALT;
			} else
				aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_state = ASR_STATE_LOOKUP_FAMILY;
		break;

	case ASR_STATE_LOOKUP_FAMILY:
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_NEXT_DB:
		aq->aq_db_idx += 1;
		aq->aq_family_idx = 0;
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_QUERY_DB:
		if (aq->aq_db_idx >= aq->aq_ctx->ac_dbcount) {
			aq->aq_state = ASR_STATE_NEXT_DOMAIN;
			break;
		}

		switch(AQ_DB(aq)->ad_type) {
		case ASR_DB_DNS:
			family = aq->aq_family;
			if (family == AF_UNSPEC)
				family = AQ_FAMILY(aq);
			if (family == AF_INET)
				aq->aq_query.q_type = T_A;
			else if (family == AF_INET6)
				aq->aq_query.q_type = T_AAAA;
			else
				errx(1, "bad family: %i", family);
			aq->aq_query.q_class = C_IN;
			aq->aq_flags = 0;
			aq->aq_ns_cycles = 0;
			aq->aq_ns_idx = 0;
			aq->aq_state = ASR_STATE_QUERY_NS;
			break;
		case ASR_DB_FILE:
			aq->aq_state = ASR_STATE_QUERY_FILE;
			break;
		default:
			aq->aq_state = ASR_STATE_NEXT_DB;
		}
		break;

	case ASR_STATE_NEXT_NS:
		aq->aq_ns_idx += 1;
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_ns_idx = 0;
			aq->aq_ns_cycles++;
		}
		if (aq->aq_ns_cycles >= AQ_DB(aq)->ad_retries) {
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_state = ASR_STATE_QUERY_NS;
		break;

	case ASR_STATE_QUERY_NS:
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		}
		switch (asr_setup_packet(aq)) {
		case -2:
			ar->ar_err = EASR_MEMORY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		case -1:
			ar->ar_err = EASR_NAME;
			aq->aq_state = ASR_STATE_HALT;
			break;
		default:
			break;
		}
		if (aq->aq_ctx->ac_forcetcp)
			aq->aq_state = ASR_STATE_TCP_WRITE;
		else
			aq->aq_state = ASR_STATE_UDP_SEND;
		break;

	case ASR_STATE_UDP_SEND:
		if (asr_udp_send(aq) == 0) {
			aq->aq_state = ASR_STATE_UDP_RECV;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		aq->aq_state = ASR_STATE_NEXT_NS;
		break;

	case ASR_STATE_UDP_RECV:
		switch (asr_udp_recv(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0: /* done */
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1: /* truncated */
			aq->aq_state = ASR_STATE_TCP_WRITE;
			break;
		}
		break;

	case ASR_STATE_TCP_WRITE:
		switch (asr_tcp_write(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_TCP_READ;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		case 1:
			ar->ar_cond = ASR_WRITE;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_TCP_READ:
		switch (asr_tcp_read(aq)) {
		case -3:
			aq->aq_state = ASR_STATE_HALT;
			ar->ar_err = EASR_MEMORY;
			break;
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1:
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_PACKET:
		packed_init(&aq->aq_packed, aq->aq_buf, aq->aq_buflen);
		unpack_header(&aq->aq_packed, &h);
		aq->aq_nanswer = h.ancount;
		for(; h.qdcount; h.qdcount--)
			unpack_query(&aq->aq_packed, &q);
		aq->aq_state = ASR_STATE_READ_RR;
		break;

	case ASR_STATE_READ_RR:
		if (aq->aq_nanswer == 0) {
			free(aq->aq_buf);
			aq->aq_buf = NULL;
			/* done with this NS, try with next family */
			aq->aq_state = ASR_STATE_NEXT_FAMILY;
			break;
		}
		aq->aq_nanswer -= 1;
		unpack_rr(&aq->aq_packed, &rr);
		if (rr.rr_type == aq->aq_query.q_type &&
		    rr.rr_class == aq->aq_query.q_class) {
			aq->aq_count += 1;
			ar->ar_count = aq->aq_count;
			sockaddr_from_rr(&ar->ar_sa.sa, &rr);
			ar->ar_cname = NULL; /* XXX */
			return (ASR_YIELD);
		}
		break;

	case ASR_STATE_QUERY_FILE:
		aq->aq_file = fopen(AQ_DB(aq)->ad_path, "r");
		if (aq->aq_file == NULL)
			aq->aq_state = ASR_STATE_NEXT_DB;
		else
			aq->aq_state = ASR_STATE_READ_FILE;
		break;

	case ASR_STATE_READ_FILE:
		n = asr_parse_namedb_line(aq->aq_file, tok, ntok);
		if (n == -1) {
			fclose(aq->aq_file);
			aq->aq_file = NULL;
			/* XXX as an optimization, the file could be parsed only once */
			aq->aq_state = ASR_STATE_NEXT_FAMILY;
			break;
		}

		for (i = 1; i < n; i++) {
			/* for the first round, try the host as-is  */
			/* XXX not nice */
			if (aq->aq_dom_idx <= 0 && !strcmp(aq->aq_host, tok[i])) {
			} else if (asr_cmp_fqdn_name(aq->aq_fqdn, tok[i]) == -1)
				continue;
			family = aq->aq_family;
			if (family == AF_UNSPEC)
				family = AQ_FAMILY(aq);
			if (sockaddr_from_str(&ar->ar_sa.sa, family, tok[0]) == -1)
				continue;

			aq->aq_count += 1;
			ar->ar_count = aq->aq_count;
			ar->ar_cname = strdup(tok[1]);
			return (ASR_YIELD);
		}
		break;

	case ASR_STATE_HALT:
		ar->ar_count = aq->aq_count;
		ar->ar_errstr = asr_error(ar->ar_err);
		return (ASR_DONE);

	default:
		errx(1, "asr_run_host: unknown state");
	}}
}



/*
 * for asr_query_addrinfo
 */

struct asr_query *
asr_query_addrinfo(struct asr		 *asr,
		   const char		 *hostname,
		   const char		 *servname,
		   const struct addrinfo *hints)
{
	struct asr_query	*aq;

	asr_check_reload(asr);

	if ((aq = asr_ctx_query(asr->a_ctx, ASR_QUERY_ADDRINFO)) == NULL)
		return (NULL);

	if (hostname && (aq->aq_hostname = strdup(hostname)) == NULL)
		goto abort;
	if (servname && (aq->aq_servname = strdup(servname)) == NULL)
		goto abort;
	if (hints)
		memmove(&aq->aq_hints, hints, sizeof *hints);
	else {
		memset(&aq->aq_hints, 0, sizeof aq->aq_hints);
		aq->aq_hints.ai_family = PF_UNSPEC;
	}

	return (aq);
    abort:
	asr_query_free(aq);
	return (NULL);
}

int
asr_get_port(const char *servname, const char *proto, int numonly)
{
	struct servent		se;
	struct servent_data	sed;
	int			port, r;
	const char*		e;

	if (servname == NULL)
		return (0);

	e = NULL;
	port = strtonum(servname, 0, USHRT_MAX, &e);
	if (e == NULL)
		return (port);
	if (errno == ERANGE)
		return (-3); /* invalid */
	if (numonly)
		return (-3);

	memset(&sed, 0, sizeof(sed));
	r = getservbyname_r(servname, proto, &se, &sed);
	port = ntohs(se.s_port);
	endservent_r(&sed);

	if (r == -1)
		return (-2); /* not found */

	return (port);
}

int
asr_add_sockaddr2(struct asr_query *aq,
		  struct sockaddr *sa,
		  int socktype,
		  int protocol)
{
	struct addrinfo		*ai;
	const char		*proto;
	int			 port;

	switch (protocol) {
	case IPPROTO_TCP:
		proto = "tcp";
		break;
	case IPPROTO_UDP:
		proto = "udp";
		break;
	default:
		proto = NULL;
	}

	port = -1;
	if (proto) {
		port = asr_get_port(aq->aq_servname, proto,
				    aq->aq_hints.ai_flags & AI_NUMERICSERV);
		if (port < 0)
			return (port);
	}

	ai = calloc(1, sizeof *ai + sa->sa_len);
	if (ai == NULL)
		return (-1); /* no mem */
	ai->ai_family = sa->sa_family;
	ai->ai_socktype = socktype;
	ai->ai_protocol = protocol;
	ai->ai_addrlen = sa->sa_len;
	ai->ai_addr = (void*)(ai + 1);
	memmove(ai->ai_addr, sa, sa->sa_len);

	if (port != -1)
		sockaddr_set_port((struct sockaddr*)ai->ai_addr, port);

	if (aq->aq_aifirst == NULL)
		aq->aq_aifirst = ai;
	if (aq->aq_ailast)
		aq->aq_ailast->ai_next = ai;
	aq->aq_ailast = ai;

	aq->aq_count += 1;

	return (0);
}

struct match {
	int family;
	int socktype;
	int protocol;
};

static const struct match matches[] = {
	{ PF_INET,	SOCK_DGRAM,	IPPROTO_UDP	},
	{ PF_INET,	SOCK_STREAM,	IPPROTO_TCP	},
	{ PF_INET,	SOCK_RAW,	0		},
	{ PF_INET6,	SOCK_DGRAM,	IPPROTO_UDP	},
	{ PF_INET6,	SOCK_STREAM,	IPPROTO_TCP	},
	{ PF_INET6,	SOCK_RAW,	0		},
	{ -1, 		0, 		0, 		},
};

#define MATCH_FAMILY(a, b) ((a) == matches[(b)].family || (a) == PF_UNSPEC)
#define MATCH_PROTO(a, b) ((a) == matches[(b)].protocol || (a) == 0)
/* do not match SOCK_RAW unless explicitely specified */
#define MATCH_SOCKTYPE(a, b) ((a) == matches[(b)].socktype || ((a) == 0 && \
				matches[(b)].socktype != SOCK_RAW))

int
asr_add_sockaddr(struct asr_query *aq, struct sockaddr *sa)
{
	int i, e;

	for(i = 0; matches[i].family != -1; i++) {
		if (matches[i].family != sa->sa_family ||
		    !MATCH_SOCKTYPE(aq->aq_hints.ai_socktype, i) ||
		    !MATCH_PROTO(aq->aq_hints.ai_protocol, i))
			continue;
		e = asr_add_sockaddr2(aq, sa, matches[i].socktype, matches[i].protocol);
		switch(e) {
		case -3:
			return (EAI_NONAME);
		case -2:
			/* Only report bad service if the protocol was specified */
			if (aq->aq_hints.ai_protocol == 0)
				break;
			return (EAI_SERVICE);
		case -1:
			return (EAI_MEMORY);
		}
	}

	return (0);
}

int
asr_iter_family(struct asr_query *aq, int first)
{
	if (first) {
		aq->aq_family_idx = 0;
		if (aq->aq_hints.ai_family != PF_UNSPEC)
			return aq->aq_hints.ai_family;
		return AQ_FAMILY(aq);
	}

	if (aq->aq_hints.ai_family != PF_UNSPEC)
		return (-1);

	aq->aq_family_idx++;

	return AQ_FAMILY(aq);
}

int
asr_run_addrinfo(struct asr_query *aq, struct asr_result *ar)
{
	const char	  *str;
	struct addrinfo	  *ai;
	int		   i, family, r;
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	} sa;

	for(;;) { /* block not indented on purpose */
#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("   ");
		asr_dump_query(aq);
	}
#endif
	switch(aq->aq_state) {

	case ASR_STATE_INIT:
		aq->aq_count = 0;
		aq->aq_state = ASR_STATE_HALT;
		ar->ar_err = 0;

		if (aq->aq_hostname == NULL &&
		    aq->aq_servname == NULL) {
			ar->ar_err = EAI_BADHINTS;
			break;
		}

		ai = &aq->aq_hints;

		if (ai->ai_addrlen ||
		    ai->ai_canonname ||
		    ai->ai_addr ||
		    ai->ai_next) {
			ar->ar_err = EAI_BADHINTS;
			break;
		}

		if (ai->ai_flags & ~AI_MASK) {
			ar->ar_err = EAI_BADHINTS;
			break;
		}

		if (ai->ai_family != PF_UNSPEC &&
		    ai->ai_family != PF_INET &&
		    ai->ai_family != PF_INET6) {
			ar->ar_err = EAI_FAMILY;
			break;
		}

		if (ai->ai_socktype &&
		    ai->ai_socktype != SOCK_DGRAM  &&
		    ai->ai_socktype != SOCK_STREAM &&
		    ai->ai_socktype != SOCK_RAW) {
			ar->ar_err = EAI_SOCKTYPE;
			break;
		}

		if (ai->ai_protocol &&
		    ai->ai_protocol != IPPROTO_UDP  &&
		    ai->ai_protocol != IPPROTO_TCP) {
			ar->ar_err = EAI_PROTOCOL;
			break;
		}

		if (ai->ai_socktype == SOCK_RAW &&
		    aq->aq_servname != NULL) {
			ar->ar_err = EAI_SERVICE;
			break;
		}

		/* make sure there is at least a valid combination */
		for (i = 0; matches[i].family != -1; i++)
			if (MATCH_FAMILY(ai->ai_family, i) &&
			    MATCH_SOCKTYPE(ai->ai_socktype, i) &&
			    MATCH_PROTO(ai->ai_protocol, i))
				break;
		if (matches[i].family == -1) {
			ar->ar_err = EAI_BADHINTS;
			break;
		}

		if (aq->aq_hostname == NULL) {
			for(family = asr_iter_family(aq, 1);
			    family != -1;
			    family = asr_iter_family(aq, 0)) {
				if (family == PF_INET)
					str = (ai->ai_flags & AI_PASSIVE) ? \
						"0.0.0.0" : "127.0.0.1";
				else /* PF_INET6 */
					str = (ai->ai_flags & AI_PASSIVE) ? \
						"::" : "::1";
				 /* can't fail */
				sockaddr_from_str(&sa.sa, family, str);
				if ((r = asr_add_sockaddr(aq, &sa.sa))) {
					ar->ar_err = r;
					aq->aq_state = ASR_STATE_HALT;
					break;
				}
			}
			if (ar->ar_err == 0 && aq->aq_count == 0)
				ar->ar_err = EAI_NODATA;
			break;
		}

		/* try numeric addresses */
		for(family = asr_iter_family(aq, 1);
		    family != -1;
		    family = asr_iter_family(aq, 0)) {

			if (sockaddr_from_str(&sa.sa, family,
					      aq->aq_hostname) == -1)
				continue;

			if ((r = asr_add_sockaddr(aq, &sa.sa))) {
				ar->ar_err = r;
				aq->aq_state = ASR_STATE_HALT;
				break;
			}

			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		if (ar->ar_err || aq->aq_count)
			break;

		if (ai->ai_flags & AI_NUMERICHOST) {
			ar->ar_err = EAI_FAIL;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}

		/* subquery for hostname */
		if ((aq->aq_subq = asr_ctx_query_host(aq->aq_ctx,
						      aq->aq_hostname,
						      ai->ai_family)) == NULL) {
			ar->ar_err = EAI_MEMORY;
			aq->aq_state = ASR_STATE_HALT;
		}

		aq->aq_state = ASR_STATE_SUBQUERY;
		break;

	case ASR_STATE_SUBQUERY:
		switch ((r = asr_run(aq->aq_subq, ar))) {
		case ASR_COND:
			return (r);
		case ASR_YIELD:
			if ((r = asr_add_sockaddr(aq, &ar->ar_sa.sa))) {
				ar->ar_err = r;
				aq->aq_state = ASR_STATE_HALT;
			}
			free(ar->ar_cname);
			break;
		case ASR_DONE:
			aq->aq_subq = NULL;
			if (ar->ar_count == 0)
				ar->ar_err = EAI_NODATA;
			else if (aq->aq_count == 0)
				ar->ar_err = EAI_NONAME;
			else
				ar->ar_err = 0;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		break;

	case ASR_STATE_HALT:
		if (ar->ar_err == 0) {
			ar->ar_errstr = NULL;
			ar->ar_count = aq->aq_count;
			ar->ar_ai = aq->aq_aifirst;
			aq->aq_aifirst = NULL;
		} else {
			ar->ar_ai = NULL;
			ar->ar_errstr = gai_strerror(ar->ar_err);
		}
		return (ASR_DONE);

	default:
		errx(1, "asr_run_addrinfo: unknown state");
	}}
}


struct asr_query *
asr_query_cname(struct asr		*asr,
		const struct sockaddr	*sa,
		socklen_t		 sl)
{
	struct asr_query	*aq;

	asr_check_reload(asr);

	if ((aq = asr_ctx_query(asr->a_ctx, ASR_QUERY_CNAME)) == NULL)
		return (NULL);

	memmove(&aq->aq_sa.sa, sa, sl);
	aq->aq_sa.sa.sa_len = sl;

	return (aq);
}

int
asr_run_cname(struct asr_query *aq, struct asr_result *ar)
{
	struct header	 h;
	struct query	 q;
	struct rr	 rr;
	char		*tok[10], buf[DOMAIN_MAXLEN];
	int		 ntok = 10, n;

	for(;;) { /* block not indented on purpose */
#ifdef ASR_DEBUG
	if (asr_debug) {
		printf("   ");
		asr_dump_query(aq);
	}
#endif
	switch(aq->aq_state) {

	case ASR_STATE_INIT:
		if (aq->aq_sa.sa.sa_family != AF_INET &&
		    aq->aq_sa.sa.sa_family != AF_INET6) {
			ar->ar_err = EASR_FAMILY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}
		aq->aq_db_idx = 0;
		aq->aq_count = 0;
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_NEXT_DB:
		/* stop here if we already have at least one answer */
		if (aq->aq_count) {
			ar->ar_err = 0;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}

		aq->aq_db_idx += 1;
		aq->aq_state = ASR_STATE_QUERY_DB;
		break;

	case ASR_STATE_QUERY_DB:
		if (aq->aq_db_idx >= aq->aq_ctx->ac_dbcount) {
			ar->ar_err = EASR_NOTFOUND;
			aq->aq_state = ASR_STATE_HALT;
			break;
		}

		switch(AQ_DB(aq)->ad_type) {
		case ASR_DB_DNS:
			if (aq->aq_fqdn == NULL) {
				sockaddr_as_fqdn(&aq->aq_sa.sa, buf, sizeof(buf));
				if ((aq->aq_fqdn = strdup(buf)) == NULL) {
					ar->ar_err = EASR_MEMORY;
					aq->aq_state = ASR_STATE_HALT;
					break;
				}
			}
			aq->aq_query.q_type = T_PTR;
			aq->aq_query.q_class = C_IN;
			aq->aq_flags = 0;
			aq->aq_ns_cycles = 0;
			aq->aq_ns_idx = 0;
			aq->aq_state = ASR_STATE_QUERY_NS;
			break;
		case ASR_DB_FILE:
			aq->aq_state = ASR_STATE_QUERY_FILE;
			break;
		default:
			aq->aq_state = ASR_STATE_NEXT_DB;
		}
		break;

	case ASR_STATE_NEXT_NS:
		aq->aq_ns_idx += 1;
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_ns_idx = 0;
			aq->aq_ns_cycles++;
		}
		if (aq->aq_ns_cycles >= AQ_DB(aq)->ad_retries) {
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_state = ASR_STATE_QUERY_NS;
		break;

	case ASR_STATE_QUERY_NS:
		if (aq->aq_ns_idx >= AQ_DB(aq)->ad_count) {
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		}
		switch (asr_setup_packet(aq)) {
		case -2:
			ar->ar_err = EASR_MEMORY;
			aq->aq_state = ASR_STATE_HALT;
			break;
		case -1:
			ar->ar_err = EASR_NAME; /* XXX impossible */
			aq->aq_state = ASR_STATE_HALT;
			break;
		default:
			break;
		}
		if (aq->aq_ctx->ac_forcetcp)
			aq->aq_state = ASR_STATE_TCP_WRITE;
		else
			aq->aq_state = ASR_STATE_UDP_SEND;
		break;

	case ASR_STATE_UDP_SEND:
		if (asr_udp_send(aq) == 0) {
			aq->aq_state = ASR_STATE_UDP_RECV;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		aq->aq_state = ASR_STATE_NEXT_NS;
		break;

	case ASR_STATE_UDP_RECV:
		switch (asr_udp_recv(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0: /* done */
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1: /* truncated */
			aq->aq_state = ASR_STATE_TCP_WRITE;
			break;
		}
		break;

	case ASR_STATE_TCP_WRITE:
		switch (asr_tcp_write(aq)) {
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_TCP_READ;
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		case 1:
			ar->ar_cond = ASR_WRITE;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_TCP_READ:
		switch (asr_tcp_read(aq)) {
		case -3:
			aq->aq_state = ASR_STATE_HALT;
			ar->ar_err = EASR_MEMORY;
			break;
		case -2: /* timeout */
		case -1: /* fail */
			aq->aq_state = ASR_STATE_NEXT_NS;
			break;
		case 0:
			aq->aq_state = ASR_STATE_PACKET;
			break;
		case 1:
			ar->ar_cond = ASR_READ;
			ar->ar_fd = aq->aq_fd;
			ar->ar_timeout = aq->aq_timeout;
			return (ASR_COND);
		}
		break;

	case ASR_STATE_PACKET:
		packed_init(&aq->aq_packed, aq->aq_buf, aq->aq_buflen);
		unpack_header(&aq->aq_packed, &h);
		aq->aq_nanswer = h.ancount;
		for(; h.qdcount; h.qdcount--)
			unpack_query(&aq->aq_packed, &q);
		aq->aq_state = ASR_STATE_READ_RR;
		break;

	case ASR_STATE_READ_RR:
		if (aq->aq_nanswer == 0) {
			free(aq->aq_buf);
			aq->aq_buf = NULL;
			/* done with this NS, try with next family */
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		aq->aq_nanswer -= 1;
		unpack_rr(&aq->aq_packed, &rr);
		if (rr.rr_type == aq->aq_query.q_type &&
		    rr.rr_class == aq->aq_query.q_class) {
			aq->aq_count += 1;
			ar->ar_count = aq->aq_count;
			print_dname(rr.rr.ptr.ptrname, buf, sizeof(buf));
			ar->ar_cname = strdup(buf);
			ar->ar_cname[strlen(buf) - 1] = 0;
			return (ASR_YIELD);
		}
		break;

	case ASR_STATE_QUERY_FILE:
		aq->aq_file = fopen(AQ_DB(aq)->ad_path, "r");
		if (aq->aq_file == NULL)
			aq->aq_state = ASR_STATE_NEXT_DB;
		else
			aq->aq_state = ASR_STATE_READ_FILE;
		break;

	case ASR_STATE_READ_FILE:
		n = asr_parse_namedb_line(aq->aq_file, tok, ntok);
		if (n == -1) {
			fclose(aq->aq_file);
			aq->aq_file = NULL;
			/* XXX as an optimization, the file could be parsed only once */
			aq->aq_state = ASR_STATE_NEXT_DB;
			break;
		}
		if (sockaddr_from_str(&ar->ar_sa.sa, aq->aq_sa.sa.sa_family, tok[0]) == -1)
			break;
		if (ar->ar_sa.sa.sa_len != aq->aq_sa.sa.sa_len ||
		    memcmp(&ar->ar_sa.sa, &aq->aq_sa.sa, aq->aq_sa.sa.sa_len))
			break;

		aq->aq_count += 1;
		ar->ar_count = aq->aq_count;
		ar->ar_cname = strdup(tok[1]);
		return (ASR_YIELD);

	case ASR_STATE_HALT:
		ar->ar_count = aq->aq_count;
		ar->ar_errstr = asr_error(ar->ar_err);
		return (ASR_DONE);

	default:
		errx(1, "asr_run_cname: unknown state %i", aq->aq_state);
	}}
}

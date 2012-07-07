/*	$OpenBSD: asr_debug.c,v 1.4 2012/07/07 20:41:52 eric Exp $	*/
/*
 * Copyright (c) 2010-2012 Eric Faurot <eric@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <inttypes.h>
#include <resolv.h>
#include <string.h>
#include <stdarg.h>

#include "asr.h"
#include "asr_private.h"

static void asr_vdebug(const char *, va_list);

static char *print_dname(const char *, char *, size_t);
static char *print_host(const struct sockaddr *, char *, size_t);

static const char *typetostr(uint16_t);
static const char *classtostr(uint16_t);
static const char *rcodetostr(uint16_t);

static const char *inet6_ntoa(struct in6_addr);


#define OPCODE_SHIFT	11
#define Z_SHIFT		 4

struct keyval {
	const char	*key;
	uint16_t	 value;
};

static struct keyval kv_class[] = {
	{ "IN",	C_IN },
	{ "CHAOS", C_CHAOS },
	{ "HS", C_HS },
	{ "ANY", C_ANY },
	{ NULL, 	0 },
};

static struct keyval kv_type[] = {
	{ "A",		T_A	},
	{ "NS",		T_NS	},
	{ "MD",		T_MD	},
	{ "MF",		T_MF	},
	{ "CNAME",	T_CNAME	},
	{ "SOA",	T_SOA	},
	{ "MB",		T_MB	},
	{ "MG",		T_MG	},
	{ "MR",		T_MR	},
	{ "NULL",	T_NULL	},
	{ "WKS",	T_WKS	},
	{ "PTR",	T_PTR	},
	{ "HINFO",	T_HINFO	},
	{ "MINFO",	T_MINFO	},
	{ "MX",		T_MX	},
	{ "TXT",	T_TXT	},

	{ "AAAA",	T_AAAA	},

	{ "AXFR",	T_AXFR	},
	{ "MAILB",	T_MAILB	},
	{ "MAILA",	T_MAILA	},
	{ "ANY",	T_ANY	},
	{ NULL, 	0 },
};

static struct keyval kv_rcode[] = {
	{ "NOERROR",	NOERROR	},
	{ "FORMERR",	FORMERR },
	{ "SERVFAIL",	SERVFAIL },
	{ "NXDOMAIN",	NXDOMAIN },
	{ "NOTIMP",	NOTIMP },
	{ "REFUSED",	REFUSED },
	{ NULL, 	0 },
};

static const char *
typetostr(uint16_t v)
{
	static char	 buf[16];
	size_t		 i;

	for(i = 0; kv_type[i].key; i++)
		if (kv_type[i].value == v)
			return (kv_type[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

static const char *
classtostr(uint16_t v)
{
	static char	 buf[16];
	size_t		 i;

	for(i = 0; kv_class[i].key; i++)
		if (kv_class[i].value == v)
			return (kv_class[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

static const char *
rcodetostr(uint16_t v)
{
	static char      buf[16];
	size_t           i;

	for(i = 0; kv_rcode[i].key; i++)
		if (kv_rcode[i].value == v)
			return (kv_rcode[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

static const char *
inet6_ntoa(struct in6_addr a)
{
	static char buf[256];
	struct sockaddr_in6	si;

	si.sin6_len = sizeof(si);
	si.sin6_family = PF_INET6;
	si.sin6_addr = a;

	return print_host((struct sockaddr*)&si, buf, sizeof buf);
}

static char*
print_rr(struct rr *rr, char *buf, size_t max)
{
	char	*res;
	char	 tmp[256];
	char	 tmp2[256];
	int	 r;

	res = buf;

	r = snprintf(buf, max, "%s %u %s %s ",
	    print_dname(rr->rr_dname, tmp, sizeof tmp),
	    rr->rr_ttl,
	    classtostr(rr->rr_class),
	    typetostr(rr->rr_type));
	if (r == -1) {
		buf[0] = '\0';
		return buf;
	}

	if ((size_t)r >= max)
		return buf;

	max -= r;
	buf += r;

	switch(rr->rr_type) {
	case T_CNAME:
		print_dname(rr->rr.cname.cname, buf, max);
		break;
	case T_MX:
		snprintf(buf, max, "%"PRIu32" %s",
		    rr->rr.mx.preference,
		    print_dname(rr->rr.mx.exchange, tmp, sizeof tmp));
		break;
	case T_NS:
		print_dname(rr->rr.ns.nsname, buf, max);
		break;
	case T_PTR:
		print_dname(rr->rr.ptr.ptrname, buf, max);
		break;
	case T_SOA:
		snprintf(buf, max, "%s %s %" PRIu32 " %" PRIu32 " %" PRIu32
		    " %" PRIu32 " %" PRIu32,
		    print_dname(rr->rr.soa.rname, tmp, sizeof tmp),
		    print_dname(rr->rr.soa.mname, tmp2, sizeof tmp2),
		    rr->rr.soa.serial,
		    rr->rr.soa.refresh,
		    rr->rr.soa.retry,
		    rr->rr.soa.expire,
		    rr->rr.soa.minimum);
		break;
	case T_A:
		if (rr->rr_class != C_IN)
			goto other;
		snprintf(buf, max, "%s", inet_ntoa(rr->rr.in_a.addr));
		break;
	case T_AAAA:
		if (rr->rr_class != C_IN)
			goto other;
		snprintf(buf, max, "%s", inet6_ntoa(rr->rr.in_aaaa.addr6));
		break;
	default:
	other:
		snprintf(buf, max, "(rdlen=%"PRIu16 ")", rr->rr.other.rdlen);
		break;
	}

	return (res);
}

static char*
print_query(struct query *q, char *buf, size_t max)
{
	char b[256];

	snprintf(buf, max, "%s	%s %s",
	    print_dname(q->q_dname, b, sizeof b),
	    classtostr(q->q_class), typetostr(q->q_type));

	return (buf);
}

static char*
print_dname(const char *_dname, char *buf, size_t max)
{
	return asr_strdname(_dname, buf, max);
}

static char*
print_header(struct header *h, char *buf, size_t max, int noid)
{
	snprintf(buf, max,
	"id:0x%04x %s op:%i %s %s %s %s z:%i r:%s qd:%i an:%i ns:%i ar:%i",
	    noid ? 0 : ((int)h->id),
	    (h->flags & QR_MASK) ? "QR":"  ",
	    (int)(OPCODE(h->flags) >> OPCODE_SHIFT),
	    (h->flags & AA_MASK) ? "AA":"  ",
	    (h->flags & TC_MASK) ? "TC":"  ",
	    (h->flags & RD_MASK) ? "RD":"  ",
	    (h->flags & RA_MASK) ? "RA":"  ",  
	    ((h->flags & Z_MASK) >> Z_SHIFT),
	    rcodetostr(RCODE(h->flags)),
	    h->qdcount, h->ancount, h->nscount, h->arcount);

	return buf;
}

static char *
print_host(const struct sockaddr *sa, char *buf, size_t len)
{
	switch (sa->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, &((const struct sockaddr_in*)sa)->sin_addr,
		    buf, len);
		break;
	case AF_INET6:
		inet_ntop(AF_INET6,
		    &((const struct sockaddr_in6*)sa)->sin6_addr, buf, len);
		break;
	default:
		buf[0] = '\0';
	}
	return (buf);
}

char *
asr_print_addr(const struct sockaddr *sa, char *buf, size_t len)
{
	char	h[256];

	print_host(sa, h, sizeof h);

	switch (sa->sa_family) {
	case AF_INET:
		snprintf(buf, len, "%s:%i", h,
		    ntohs(((const struct sockaddr_in*)(sa))->sin_port));
		break;
	case AF_INET6:
		snprintf(buf, len, "[%s]:%i", h,
		    ntohs(((const struct sockaddr_in6*)(sa))->sin6_port));
		break;
	default:
		snprintf(buf, len, "?");
		break;
	}

	return (buf);
}

struct kv { int code; const char *name; };

static const char*	kvlookup(struct kv *, int);

int	 asr_debug = 0;

void
asr_dump(struct asr *a)
{
	char		 buf[256];
	int		 i;
	struct asr_ctx	*ac;
	unsigned int	 options;

	ac = a->a_ctx;

	asr_printf("--------- ASR CONFIG ---------------\n");
	if (a->a_path)
		asr_printf("CONF FILE \"%s\"\n", a->a_path);
	else
		asr_printf("STATIC CONF\n");
	asr_printf("DOMAIN \"%s\"\n", ac->ac_domain);
	asr_printf("SEARCH\n");
	for(i = 0; i < ac->ac_domcount; i++)
		asr_printf("   \"%s\"\n", ac->ac_dom[i]);
	asr_printf("OPTIONS\n");
	asr_printf(" options:");
	options = ac->ac_options;
	if (options & RES_INIT) {
		asr_printf(" INIT"); options &= ~RES_INIT;
	}
	if (options & RES_DEBUG) {
		asr_printf(" DEBUG"); options &= ~RES_DEBUG;
	}
	if (options & RES_USEVC) {
		asr_printf(" USEVC"); options &= ~RES_USEVC;
	}
	if (options & RES_IGNTC) {
		asr_printf(" IGNTC"); options &= ~RES_IGNTC;
	}
	if (options & RES_RECURSE) {
		asr_printf(" RECURSE"); options &= ~RES_RECURSE;
	}
	if (options & RES_DEFNAMES) {
		asr_printf(" DEFNAMES"); options &= ~RES_DEFNAMES;
	}
	if (options & RES_STAYOPEN) {
		asr_printf(" STAYOPEN"); options &= ~RES_STAYOPEN;
	}
	if (options & RES_DNSRCH) {
		asr_printf(" DNSRCH"); options &= ~RES_DNSRCH;
	}
	if (options & RES_NOALIASES) {
		asr_printf(" NOALIASES"); options &= ~RES_NOALIASES;
	}
	if (options & RES_USE_EDNS0) {
		asr_printf(" USE_EDNS0"); options &= ~RES_USE_EDNS0;
	}
	if (options & RES_USE_DNSSEC) {
		asr_printf(" USE_DNSSEC"); options &= ~RES_USE_DNSSEC;
	}
	if (options)
		asr_printf("0x%08x\n", options);
	asr_printf("\n", ac->ac_options);

	asr_printf(" ndots: %i\n", ac->ac_ndots);
	asr_printf(" family:");
	for(i = 0; ac->ac_family[i] != -1; i++)
		asr_printf(" %s", (ac->ac_family[i] == AF_INET) ?
		    "inet" : "inet6");
	asr_printf("\n");
	asr_printf("NAMESERVERS timeout=%i retry=%i\n",
		   ac->ac_nstimeout,
		   ac->ac_nsretries);
	for(i = 0; i < ac->ac_nscount; i++)
		asr_printf("	%s\n", asr_print_addr(ac->ac_ns[i], buf,
		    sizeof buf));
	asr_printf("HOSTFILE %s\n", ac->ac_hostfile);
	asr_printf("LOOKUP");
	for(i = 0; i < ac->ac_dbcount; i++) {
		switch (ac->ac_db[i]) {
		case ASR_DB_FILE:
			asr_printf(" file");
			break;
		case ASR_DB_DNS:
			asr_printf(" dns");
			break;
		case ASR_DB_YP:
			asr_printf(" yp");
			break;
		default:
			asr_printf(" ?%i", ac->ac_db[i]);
		}
	}
	asr_printf("\n------------------------------------\n");
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
	{ ASR_SEND,		"ASR_SEND"		},
	{ ASR_SEARCH,		"ASR_SEARCH"		},
	{ ASR_GETRRSETBYNAME,	"ASR_GETRRSETBYNAME"	},
	{ ASR_GETHOSTBYNAME,	"ASR_GETHOSTBYNAME"	},
	{ ASR_GETHOSTBYADDR,	"ASR_GETHOSTBYADDR"	},
	{ ASR_GETNETBYNAME,	"ASR_GETNETBYNAME"	},
	{ ASR_GETNETBYADDR,	"ASR_GETNETBYADDR"	},
	{ ASR_GETADDRINFO,	"ASR_GETADDRINFO"	},
	{ ASR_GETNAMEINFO,	"ASR_GETNAMEINFO"	},
	{ ASR_HOSTADDR,		"ASR_HOSTADDR"		},
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
	{ ASR_STATE_SEARCH_DOMAIN,	"ASR_STATE_SEARCH_DOMAIN"	},
	{ ASR_STATE_LOOKUP_DOMAIN,	"ASR_STATE_LOOKUP_DOMAIN"	},
	{ ASR_STATE_NEXT_DOMAIN,	"ASR_STATE_NEXT_DOMAIN"		},
	{ ASR_STATE_NEXT_DB,		"ASR_STATE_NEXT_DB"		},
	{ ASR_STATE_SAME_DB,		"ASR_STATE_SAME_DB"		},
	{ ASR_STATE_NEXT_FAMILY,	"ASR_STATE_NEXT_FAMILY"		},
	{ ASR_STATE_LOOKUP_FAMILY,	"ASR_STATE_LOOKUP_FAMILY"	},
	{ ASR_STATE_NEXT_NS,		"ASR_STATE_NEXT_NS"		},
	{ ASR_STATE_READ_RR,		"ASR_STATE_READ_RR"		},
	{ ASR_STATE_READ_FILE,		"ASR_STATE_READ_FILE"		},
	{ ASR_STATE_UDP_SEND,		"ASR_STATE_UDP_SEND"		},
	{ ASR_STATE_UDP_RECV,		"ASR_STATE_UDP_RECV"		},
	{ ASR_STATE_TCP_WRITE,		"ASR_STATE_TCP_WRITE"		},
	{ ASR_STATE_TCP_READ,		"ASR_STATE_TCP_READ"		},
	{ ASR_STATE_PACKET,		"ASR_STATE_PACKET"		},
	{ ASR_STATE_SUBQUERY,		"ASR_STATE_SUBQUERY"		},
	{ ASR_STATE_NOT_FOUND,		"ASR_STATE_NOT_FOUND",		},
	{ ASR_STATE_HALT,		"ASR_STATE_HALT"		},
	{ 0, NULL }
};

struct kv kv_transition[] = {
	{ ASYNC_COND,			"ASYNC_COND"			},
	{ ASYNC_YIELD,			"ASYNC_YIELD"			},
	{ ASYNC_DONE,			"ASYNC_DONE"			},
        { 0, NULL }
};

const char *
asr_querystr(int type)
{
	return kvlookup(kv_query_type, type);
}

const char *
asr_transitionstr(int type)
{
	return kvlookup(kv_transition, type);
}

void
asr_dump_async(struct async *as)
{
	asr_printf("%s fd=%i timeout=%i"
		"   dom_idx=%i db_idx=%i ns_idx=%i ns_cycles=%i\n",
		kvlookup(kv_state, as->as_state),
		as->as_fd,
		as->as_timeout,

		as->as_dom_idx,
		as->as_db_idx,
		as->as_ns_idx,
		as->as_ns_cycles);
}

void
asr_dump_packet(FILE *f, const void *data, size_t len, int noid)
{
	char		buf[1024];
	struct packed	p;
	struct header	h;
	struct query	q;
	struct rr	rr;
	int		i, an, ns, ar, n;

	if (f == NULL)
		return;

	packed_init(&p, (char *)data, len);

	if (unpack_header(&p, &h) == -1) {
		fprintf(f, ";; BAD PACKET: %s\n", p.err);
		return;
	}

	fprintf(f, ";; HEADER %s\n", print_header(&h, buf, sizeof buf, noid));

	if (h.qdcount)
		fprintf(f, ";; QUERY SECTION:\n");
	for (i = 0; i < h.qdcount; i++) {
		if (unpack_query(&p, &q) == -1)
			goto error;
		fprintf(f, "%s\n", print_query(&q, buf, sizeof buf));
	}

	an = 0;
	ns = an + h.ancount;
	ar = ns + h.nscount;
	n = ar + h.arcount;

	for (i = 0; i < n; i++) {
		if (i == an)
			fprintf(f, "\n;; ANSWER SECTION:\n");
		if (i == ns)
			fprintf(f, "\n;; AUTHORITY SECTION:\n");
		if (i == ar)
			fprintf(f, "\n;; ADDITIONAL SECTION:\n");

		if (unpack_rr(&p, &rr) == -1)
			goto error;
		fprintf(f, "%s\n", print_rr(&rr, buf, sizeof buf));
	}

	if (p.offset != len)
		fprintf(f, ";; REMAINING GARBAGE %zu\n", len - p.offset);

    error:
	if (p.err)
		fprintf(f, ";; ERROR AT OFFSET %zu/%zu: %s\n", p.offset, p.len,
		    p.err);

	return;
}

static void
asr_vdebug(const char *fmt, va_list ap)
{
	if (asr_debug)
		vfprintf(stderr, fmt, ap);
}

void
asr_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	asr_vdebug(fmt, ap);
	va_end(ap);
}

void
async_set_state(struct async *as, int state)
{
	asr_printf("asr: [%s@%p] %s -> %s\n",
		kvlookup(kv_query_type, as->as_type),
		as,
		kvlookup(kv_state, as->as_state),
		kvlookup(kv_state, state));
	as->as_state = state;
}

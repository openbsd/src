/*
 * Copyright (c) 2009,2010	Eric Faurot	<eric@faurot.net>
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
#include <inttypes.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "dnsutil.h"

struct keyval {
	const char	*key;
	uint16_t	 value;
};

struct keyval kv_class[] = {
	{ "IN",	C_IN },
	{ "CS", C_CS },
	{ "CH", C_CH },
	{ "HS", C_HS },
};

struct keyval kv_type[] = {
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

	{ "AXFR",	T_AXFR	},
	{ "MAILB",	T_MAILB	},
	{ "MAILA",	T_MAILA	},
	{ "ALL",	T_ALL	},

	{ "AAAA",	T_AAAA	},
};

struct keyval kv_rcode[] = {
	{ "NOERROR",	NOERR	},
	{ "ERR_FORMAT",	ERR_FORMAT },
	{ "ERR_SERVER",	ERR_SERVER },
	{ "ERR_NAME",	ERR_NAME },
	{ "ERR_NOFUNC",	ERR_NOFUNC },
	{ "ERR_REFUSED",ERR_REFUSED },
};

const char *
typetostr(uint16_t v)
{
	static char	 buf[16];
	size_t		 i;

	for(i = 0; i < sizeof(kv_type)/sizeof(kv_type[0]); i++)
		if (kv_type[i].value == v)
			return (kv_type[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

const char *
classtostr(uint16_t v)
{
	static char	 buf[16];
	size_t		 i;

	for(i = 0; i < sizeof(kv_class)/sizeof(kv_class[0]); i++)
		if (kv_class[i].value == v)
			return (kv_class[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

const char *
rcodetostr(uint16_t v)
{
	static char      buf[16];
	size_t           i;

	for(i = 0; i < sizeof(kv_rcode)/sizeof(kv_rcode[0]); i++)
		if (kv_rcode[i].value == v)
			return (kv_rcode[i].key);

	snprintf(buf, sizeof buf, "%"PRIu16"?", v);

	return (buf);
}

uint16_t
strtotype(const char *name)
{
	size_t	i;

	for(i = 0; i < sizeof(kv_type)/sizeof(kv_type[0]); i++)
		if (!strcmp(kv_type[i].key, name))
			return (kv_type[i].value);

	return (0);
}

uint16_t
strtoclass(const char *name)
{
	size_t	i;

	for(i = 0; i < sizeof(kv_class)/sizeof(kv_class[0]); i++)
		if (!strcmp(kv_class[i].key, name))
			return (kv_class[i].value);

	return (0);
}

const char *
inet6_ntoa(struct in6_addr a)
{
	static char buf[256];
	struct sockaddr_in6	si;

	si.sin6_len = sizeof(si);
	si.sin6_family = PF_INET6;
	si.sin6_addr = a;

	return print_host((struct sockaddr*)&si, buf, sizeof buf);
}

const char*
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
			snprintf(buf, max,
			    "%s %s %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
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
			snprintf(buf, max, inet6_ntoa(rr->rr.in_aaaa.addr6));
			break;
		default:
		other:
			snprintf(buf, max, "(rdlen=%"PRIu16 ")", rr->rr.other.rdlen);
			break;
	}

	return (res);
}

const char*
print_rrdynamic(struct rr_dynamic *rd, char *buf, size_t max)
{
	char	*res;
	char	 tmp[256];
	char	 tmp2[256];
	int	 r;

	res = buf;

	r = snprintf(buf, max, "%s	%u	%s	%s	",
	    print_dname(rd->rd_dname, tmp, sizeof tmp),
	    rd->rd_ttl,
	    classtostr(rd->rd_class),
	    typetostr(rd->rd_type));
	if (r == -1) {
		buf[0] = '\0';
		return buf;
	}

	if ((size_t)r >= max)
		return buf;

	max -= r;
	buf += r;

	switch(rd->rd_type) {
		case T_CNAME:
			print_dname(rd->rd.cname.cname, buf, max);
			break;
		case T_MX:
			snprintf(buf, max, "%"PRIu32" %s",
			    rd->rd.mx.preference,
			    print_dname(rd->rd.mx.exchange, tmp, sizeof tmp));
			break;
		case T_NS:
			print_dname(rd->rd.ns.nsname, buf, max);
			break;
		case T_PTR:
			print_dname(rd->rd.ptr.ptrname, buf, max);
			break;
		case T_SOA:
			snprintf(buf, max,
			    "%s %s %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
			    print_dname(rd->rd.soa.rname, tmp, sizeof tmp),
			    print_dname(rd->rd.soa.mname, tmp2, sizeof tmp2),
			    rd->rd.soa.serial,
			    rd->rd.soa.refresh,
			    rd->rd.soa.retry,
			    rd->rd.soa.expire,
			    rd->rd.soa.minimum);
			break;
		case T_A:
			if (rd->rd_class != C_IN)
				goto other;
			snprintf(buf, max, "%s", inet_ntoa(rd->rd.in_a.addr));
			break;
		case T_AAAA:
			if (rd->rd_class != C_IN)
				goto other;
			snprintf(buf, max, inet6_ntoa(rd->rd.in_aaaa.addr6));
			break;
		default:
		other:
			snprintf(buf, max, "(rdlen=%"PRIu16 ")", rd->rd.other.rdlen);
			break;
	}

	return (res);
}

const char*
print_query(struct query *q, char *buf, size_t max)
{
	char b[256];

	snprintf(buf, max, "%s	%s %s",
	    print_dname(q->q_dname, b, sizeof b),
	    classtostr(q->q_class), typetostr(q->q_type));

	return (buf);
}

const char*
print_dname(const char *_dname, char *buf, size_t max)
{
	const unsigned char *dname = _dname;
	char	*res;
	size_t	 left, n, count;

	if (_dname[0] == 0) {
		strlcpy(buf, ".", max);
		return buf;
	}

	res = buf;
	left = max - 1;
	for (n = 0; dname[0] && left; n += dname[0]) {
		count = (dname[0] < (left - 1)) ? dname[0] : (left - 1);
		memmove(buf, dname + 1, count);
		dname += dname[0] + 1;
		left -= count;
		buf += count;
		if (left) {
			left -= 1;
			*buf++ = '.';
		}
	}
	buf[0] = 0;

	return (res);
}

const char*
print_header(struct header *h, char *buf, size_t max)
{
	snprintf(buf, max,
	"id:0x%04x %s op:%i %s %s %s %s z:%i r:%s qd:%i an:%i ns:%i ar:%i",
	    (int)h->id,
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

const char *
print_host(struct sockaddr *sa, char *buf, size_t len)
{
	int	e;

	if ((e = getnameinfo(sa, sa->sa_len,
	    buf, len, NULL, 0, NI_NUMERICHOST)) != 0) {
		buf[0] = '\0';
		return (NULL);
	}
	return (buf);
}

const char *
print_addr(struct sockaddr *sa, char *buf, size_t len)
{
	char	h[256];

	print_host(sa, h, sizeof h);

	switch (sa->sa_family) {
		case AF_INET:
			snprintf(buf, len, "%s:%i", h,
			    ntohs(((struct sockaddr_in*)(sa))->sin_port));
			break;
		case AF_INET6:
			snprintf(buf, len, "[%s]:%i", h,
			    ntohs(((struct sockaddr_in6*)(sa))->sin6_port));
			break;
		default:
			snprintf(buf, len, "?");
			break;
	}

	return (buf);
}

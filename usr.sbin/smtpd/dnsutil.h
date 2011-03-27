/*	$OpenBSD: dnsutil.h,v 1.3 2011/03/27 17:39:17 eric Exp $	*/
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

#include "dnsdefs.h"

struct packed {
	char		*data;
	size_t		 len;
	size_t		 offset;
	const char	*err;
};

struct header {
	uint16_t	id;
	uint16_t	flags;
	uint16_t	qdcount;
	uint16_t	ancount;
	uint16_t	nscount;
	uint16_t	arcount;
};

struct query {
	char			 q_dname[DOMAIN_MAXLEN];
	uint16_t		 q_type;
	uint16_t		 q_class;
};

struct rr {
	char		 rr_dname[DOMAIN_MAXLEN];
	uint16_t	 rr_type;
	uint16_t	 rr_class;
	uint32_t	 rr_ttl;
	union {
		struct {
			char	cname[DOMAIN_MAXLEN];
		} cname;
		struct {
			uint16_t	preference;
			char		exchange[DOMAIN_MAXLEN];
		} mx;
		struct {
			char	nsname[DOMAIN_MAXLEN];
		} ns;
		struct {
			char	ptrname[DOMAIN_MAXLEN];
		} ptr;
		struct {
			char		mname[DOMAIN_MAXLEN];
			char		rname[DOMAIN_MAXLEN];
			uint32_t	serial;
			uint32_t	refresh;
			uint32_t	retry;
			uint32_t	expire;
			uint32_t	minimum;
		} soa;
		struct {
			struct in_addr	addr;
		} in_a;
		struct {
			struct in6_addr	addr6;
		} in_aaaa;
		struct {
			uint16_t	 rdlen;
			const void	*rdata;
		} other;
	} rr;
};

struct rr_dynamic {
	const char		*rd_dname;
	uint16_t		 rd_type;
	uint16_t		 rd_class;
	uint32_t		 rd_ttl;
	union rr_subtype {
		struct rr_cname {
			char		*cname;
		} cname;
		struct rr_mx {
			uint16_t	 preference;
			char		*exchange;
		} mx;
		struct rr_ns {
			char		*nsname;
		} ns;
		struct rr_ptr {
			char		*ptrname;
		} ptr;
		struct rr_soa {
			char		*mname;
			char		*rname;
			uint32_t	 serial;
			uint32_t	 refresh;
			uint32_t	 retry;
			uint32_t	 expire;
			uint32_t	 minimum;
		} soa;
		struct rr_in_a {
			struct in_addr	 addr;
		} in_a;
		struct rr_in_aaaa {
			struct in6_addr	 addr6;
		} in_aaaa;
		struct rr_other {
			uint16_t	 rdlen;
			void		*rdata;
		} other;
	} rd;
};



/* pack.c */
void	packed_init(struct packed*, char*, size_t);

int	unpack_data(struct packed*, void*, size_t);
int	unpack_u16(struct packed*, uint16_t*);
int	unpack_u32(struct packed*, uint32_t*);
int	unpack_inaddr(struct packed*, struct in_addr*);
int	unpack_in6addr(struct packed*, struct in6_addr*);
int	unpack_dname(struct packed*, char*, size_t);
int	unpack_header(struct packed*, struct header*);
int	unpack_query(struct packed*, struct query*);
int	unpack_rr(struct packed*, struct rr*);

int	pack_data(struct packed*, const void*, size_t);
int	pack_u16(struct packed*, uint16_t);
int	pack_u32(struct packed*, uint32_t);
int	pack_inaddr(struct packed*, struct in_addr);
int	pack_in6addr(struct packed*, struct in6_addr);
int	pack_header(struct packed*, const struct header*);
int	pack_dname(struct packed*, const char*);
int	pack_query(struct packed*, uint16_t, uint16_t, const char*);
int	pack_rrdynamic(struct packed*, const struct rr_dynamic *rr);

/* sockaddr.c */
int	sockaddr_from_rr(struct sockaddr *, struct rr *);
int	sockaddr_from_str(struct sockaddr *, int, const char *);
ssize_t	sockaddr_as_fqdn(const struct sockaddr *, char *, size_t);
void	sockaddr_set_port(struct sockaddr *, int);
int	sockaddr_connect(const struct sockaddr *, int);
int	sockaddr_listen(const struct sockaddr *, int);

/* print.c */
const char *print_host(struct sockaddr*, char*, size_t);
const char *print_addr(struct sockaddr*, char*, size_t);
const char *print_dname(const char*, char*, size_t);
const char *print_header(struct header*, char*, size_t);
const char *print_query(struct query*, char*, size_t);
const char *print_rr(struct rr*, char*, size_t);
const char *print_rrdynamic(struct rr_dynamic*, char*, size_t);

const char *typetostr(uint16_t);
const char *classtostr(uint16_t);
const char *rcodetostr(uint16_t);

uint16_t    strtotype(const char*);
uint16_t    strtoclass(const char*);
const char *inet6_ntoa(struct in6_addr);

/* dname.c */
size_t	    dname_len(const char *);
size_t	    dname_depth(const char *);
ssize_t	    dname_from_fqdn(const char*, char*, size_t);
ssize_t	    dname_from_sockaddr(const struct sockaddr *, char*, size_t);
int	    dname_is_in(const char*, const char*);
int	    dname_is_wildcard(const char *);
int	    dname_is_reverse(const char *);
int	    dname_check_label(const char*, size_t);
const char* dname_up(const char*, unsigned int);

/* res_random.c */
unsigned int res_randomid(void);

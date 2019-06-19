/*	$OpenBSD: irrfilter.h,v 1.10 2018/09/17 13:35:36 claudio Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/tree.h>
#include <netinet/in.h>

#define	F_IMPORTONLY	0x01	/* skip export: items */
#define	F_IPV4		0x02	/* use IPv4 items */
#define	F_IPV6		0x04	/* use IPv6 items */

int	irrflags;
int	irrverbose;

enum pdir {
	PDIR_NONE,
	IMPORT,
	EXPORT
};

struct policy_item {
	TAILQ_ENTRY(policy_item)	 entry;
	char				*peer_addr;
	char				*action;
	char				*filter;
	enum pdir			 dir;
	u_int32_t			 peer_as;
};

TAILQ_HEAD(policy_head, policy_item);

struct router {
	TAILQ_ENTRY(router)		 entry;
	char				*address;
	struct policy_head		 policy_h;
};

TAILQ_HEAD(router_head, router)	router_head;

/* keep qtype and qtype_objs in whois.c in sync! */
enum qtype {
	QTYPE_NONE,
	QTYPE_OWNAS,
	QTYPE_ASSET,
	QTYPE_ROUTE,
	QTYPE_ROUTE6
};

struct irr_as_set {
	RB_ENTRY(irr_as_set)	  entry;
	char			 *name;
	char			**members;		/* direct members */
	char			**as_set;		/* members as-set */
	char			**as;			/* members aut-num */
	u_int			  n_members;
	u_int			  n_as_set;
	u_int			  n_as;
};

struct irr_prefix {
	union {
		struct in_addr	in;
		struct in6_addr	in6;
	} addr;
	sa_family_t	af;
	u_int8_t	len;
	u_int8_t	maxlen;
};

struct prefix_set {
	RB_ENTRY(prefix_set)	  entry;
	char			 *as;
	struct irr_prefix	**prefix;
	u_int			  prefixcnt;
};

/* eat trailing and leading whitespace */
#define ISWS(x)	(x == ' ' || x == '\t')
#define	EATWS(s)					\
	do {						\
		char	*ps;				\
		while (ISWS(*s))			\
			s++;				\
		ps = s + strlen(s) - 1;			\
		while (ps && ps >= s && ISWS(*ps))	\
			*ps-- = '\0';			\
	} while (0);

__dead void		 irr_main(u_int32_t, int, char *);
int			 whois(const char *, enum qtype);
int			 parse_response(FILE *, enum qtype);
int			 write_filters(char *);
struct irr_as_set	*asset_expand(char *);
int			 asset_addmember(char *);
struct prefix_set	*prefixset_get(char *);
int			 prefixset_addmember(char *);

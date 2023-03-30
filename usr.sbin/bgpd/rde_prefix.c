/*	$OpenBSD: rde_prefix.c,v 1.48 2023/03/30 13:25:23 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>

#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "rde.h"
#include "log.h"

/*
 * Prefix Table functions:
 * pt_add:    create new prefix and link it into the prefix table
 * pt_remove: Checks if there is no bgp prefix linked to the prefix,
 *            unlinks from the prefix table and frees the pt_entry.
 * pt_get:    get a prefix/prefixlen entry. While pt_lookup searches for the
 *            best matching prefix pt_get only finds the prefix/prefixlen
 *            entry. The speed of pt_get is important for the bgp updates.
 * pt_getaddr: convert the address into a struct bgpd_addr.
 * pt_lookup: lookup a IP in the prefix table. Mainly for "show ip bgp".
 * pt_empty:  returns true if there is no bgp prefix linked to the pt_entry.
 * pt_init:   initialize prefix table.
 * pt_alloc: allocate a AF specific pt_entry. Internal function.
 * pt_free:   free a pt_entry. Internal function.
 */

/* internal prototypes */
static struct pt_entry	*pt_alloc(struct pt_entry *, int len);
static void		 pt_free(struct pt_entry *);

struct pt_entry4 {
	RB_ENTRY(pt_entry)		pt_e;
	uint8_t				aid;
	uint8_t				prefixlen;
	uint16_t			len;
	uint32_t			refcnt;
	struct in_addr			prefix4;
};

struct pt_entry6 {
	RB_ENTRY(pt_entry)		pt_e;
	uint8_t				aid;
	uint8_t				prefixlen;
	uint16_t			len;
	uint32_t			refcnt;
	struct in6_addr			prefix6;
};

struct pt_entry_vpn4 {
	RB_ENTRY(pt_entry)		pt_e;
	uint8_t				aid;
	uint8_t				prefixlen;
	uint16_t			len;
	uint32_t			refcnt;
	uint64_t			rd;
	struct in_addr			prefix4;
	uint8_t				labelstack[21];
	uint8_t				labellen;
	uint8_t				pad1;
	uint8_t				pad2;
};

struct pt_entry_vpn6 {
	RB_ENTRY(pt_entry)		pt_e;
	uint8_t				aid;
	uint8_t				prefixlen;
	uint16_t			len;
	uint32_t			refcnt;
	uint64_t			rd;
	struct in6_addr			prefix6;
	uint8_t				labelstack[21];
	uint8_t				labellen;
	uint8_t				pad1;
	uint8_t				pad2;
};


RB_HEAD(pt_tree, pt_entry);
RB_PROTOTYPE(pt_tree, pt_entry, pt_e, pt_prefix_cmp);
RB_GENERATE(pt_tree, pt_entry, pt_e, pt_prefix_cmp);

struct pt_tree	pttable;

void
pt_init(void)
{
	RB_INIT(&pttable);
}

void
pt_shutdown(void)
{
	if (!RB_EMPTY(&pttable))
		log_debug("pt_shutdown: tree is not empty.");
}

void
pt_getaddr(struct pt_entry *pte, struct bgpd_addr *addr)
{
	memset(addr, 0, sizeof(struct bgpd_addr));
	addr->aid = pte->aid;
	switch (addr->aid) {
	case AID_INET:
		addr->v4 = ((struct pt_entry4 *)pte)->prefix4;
		break;
	case AID_INET6:
		addr->v6 = ((struct pt_entry6 *)pte)->prefix6;
		/* XXX scope_id ??? */
		break;
	case AID_VPN_IPv4:
		addr->v4 = ((struct pt_entry_vpn4 *)pte)->prefix4;
		addr->rd = ((struct pt_entry_vpn4 *)pte)->rd;
		addr->labellen = ((struct pt_entry_vpn4 *)pte)->labellen;
		memcpy(addr->labelstack,
		    ((struct pt_entry_vpn4 *)pte)->labelstack,
		    addr->labellen);
		break;
	case AID_VPN_IPv6:
		addr->v6 = ((struct pt_entry_vpn6 *)pte)->prefix6;
		addr->rd = ((struct pt_entry_vpn6 *)pte)->rd;
		addr->labellen = ((struct pt_entry_vpn6 *)pte)->labellen;
		memcpy(addr->labelstack,
		    ((struct pt_entry_vpn6 *)pte)->labelstack,
		    addr->labellen);
		break;
	default:
		fatalx("pt_getaddr: unknown af");
	}
}

struct pt_entry *
pt_fill(struct bgpd_addr *prefix, int prefixlen)
{
	static struct pt_entry4		pte4;
	static struct pt_entry6		pte6;
	static struct pt_entry_vpn4	pte_vpn4;
	static struct pt_entry_vpn6	pte_vpn6;

	switch (prefix->aid) {
	case AID_INET:
		memset(&pte4, 0, sizeof(pte4));
		pte4.len = sizeof(pte4);
		pte4.refcnt = UINT32_MAX;
		pte4.aid = prefix->aid;
		if (prefixlen > 32)
			fatalx("pt_fill: bad IPv4 prefixlen");
		inet4applymask(&pte4.prefix4, &prefix->v4, prefixlen);
		pte4.prefixlen = prefixlen;
		return ((struct pt_entry *)&pte4);
	case AID_INET6:
		memset(&pte6, 0, sizeof(pte6));
		pte6.len = sizeof(pte6);
		pte6.refcnt = UINT32_MAX;
		pte6.aid = prefix->aid;
		if (prefixlen > 128)
			fatalx("pt_fill: bad IPv6 prefixlen");
		inet6applymask(&pte6.prefix6, &prefix->v6, prefixlen);
		pte6.prefixlen = prefixlen;
		return ((struct pt_entry *)&pte6);
	case AID_VPN_IPv4:
		memset(&pte_vpn4, 0, sizeof(pte_vpn4));
		pte_vpn4.len = sizeof(pte_vpn4);
		pte_vpn4.refcnt = UINT32_MAX;
		pte_vpn4.aid = prefix->aid;
		if (prefixlen > 32)
			fatalx("pt_fill: bad IPv4 prefixlen");
		inet4applymask(&pte_vpn4.prefix4, &prefix->v4, prefixlen);
		pte_vpn4.prefixlen = prefixlen;
		pte_vpn4.rd = prefix->rd;
		pte_vpn4.labellen = prefix->labellen;
		memcpy(pte_vpn4.labelstack, prefix->labelstack,
		    prefix->labellen);
		return ((struct pt_entry *)&pte_vpn4);
	case AID_VPN_IPv6:
		memset(&pte_vpn6, 0, sizeof(pte_vpn6));
		pte_vpn6.len = sizeof(pte_vpn6);
		pte_vpn6.refcnt = UINT32_MAX;
		pte_vpn6.aid = prefix->aid;
		if (prefixlen > 128)
			fatalx("pt_get: bad IPv6 prefixlen");
		inet6applymask(&pte_vpn6.prefix6, &prefix->v6, prefixlen);
		pte_vpn6.prefixlen = prefixlen;
		pte_vpn6.rd = prefix->rd;
		pte_vpn6.labellen = prefix->labellen;
		memcpy(pte_vpn6.labelstack, prefix->labelstack,
		    prefix->labellen);
		return ((struct pt_entry *)&pte_vpn6);
	default:
		fatalx("pt_fill: unknown af");
	}
}

struct pt_entry *
pt_get(struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_entry	*pte;

	pte = pt_fill(prefix, prefixlen);
	return RB_FIND(pt_tree, &pttable, pte);
}

struct pt_entry *
pt_add(struct bgpd_addr *prefix, int prefixlen)
{
	struct pt_entry		*p = NULL;

	p = pt_fill(prefix, prefixlen);
	p = pt_alloc(p, p->len);

	if (RB_INSERT(pt_tree, &pttable, p) != NULL)
		fatalx("pt_add: insert failed");

	return (p);
}

void
pt_remove(struct pt_entry *pte)
{
	if (pte->refcnt != 0)
		fatalx("pt_remove: entry still holds references");

	if (RB_REMOVE(pt_tree, &pttable, pte) == NULL)
		log_warnx("pt_remove: remove failed.");
	pt_free(pte);
}

struct pt_entry *
pt_lookup(struct bgpd_addr *addr)
{
	struct pt_entry	*p;
	int		 i;

	switch (addr->aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		i = 32;
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		i = 128;
		break;
	default:
		fatalx("pt_lookup: unknown af");
	}
	for (; i >= 0; i--) {
		p = pt_get(addr, i);
		if (p != NULL)
			return (p);
	}
	return (NULL);
}

int
pt_prefix_cmp(const struct pt_entry *a, const struct pt_entry *b)
{
	const struct pt_entry4		*a4, *b4;
	const struct pt_entry6		*a6, *b6;
	const struct pt_entry_vpn4	*va4, *vb4;
	const struct pt_entry_vpn6	*va6, *vb6;
	int				 i;

	if (a->aid > b->aid)
		return (1);
	if (a->aid < b->aid)
		return (-1);

	switch (a->aid) {
	case AID_INET:
		a4 = (const struct pt_entry4 *)a;
		b4 = (const struct pt_entry4 *)b;
		if (ntohl(a4->prefix4.s_addr) > ntohl(b4->prefix4.s_addr))
			return (1);
		if (ntohl(a4->prefix4.s_addr) < ntohl(b4->prefix4.s_addr))
			return (-1);
		if (a4->prefixlen > b4->prefixlen)
			return (1);
		if (a4->prefixlen < b4->prefixlen)
			return (-1);
		return (0);
	case AID_INET6:
		a6 = (const struct pt_entry6 *)a;
		b6 = (const struct pt_entry6 *)b;

		i = memcmp(&a6->prefix6, &b6->prefix6, sizeof(struct in6_addr));
		if (i > 0)
			return (1);
		if (i < 0)
			return (-1);
		if (a6->prefixlen < b6->prefixlen)
			return (-1);
		if (a6->prefixlen > b6->prefixlen)
			return (1);
		return (0);
	case AID_VPN_IPv4:
		va4 = (const struct pt_entry_vpn4 *)a;
		vb4 = (const struct pt_entry_vpn4 *)b;
		if (be64toh(va4->rd) > be64toh(vb4->rd))
			return (1);
		if (be64toh(va4->rd) < be64toh(vb4->rd))
			return (-1);
		if (ntohl(va4->prefix4.s_addr) > ntohl(vb4->prefix4.s_addr))
			return (1);
		if (ntohl(va4->prefix4.s_addr) < ntohl(vb4->prefix4.s_addr))
			return (-1);
		if (va4->prefixlen > vb4->prefixlen)
			return (1);
		if (va4->prefixlen < vb4->prefixlen)
			return (-1);
		return (0);
	case AID_VPN_IPv6:
		va6 = (const struct pt_entry_vpn6 *)a;
		vb6 = (const struct pt_entry_vpn6 *)b;
		if (be64toh(va6->rd) > be64toh(vb6->rd))
			return (1);
		if (be64toh(va6->rd) < be64toh(vb6->rd))
			return (-1);
		i = memcmp(&va6->prefix6, &vb6->prefix6,
		    sizeof(struct in6_addr));
		if (i > 0)
			return (1);
		if (i < 0)
			return (-1);
		if (va6->prefixlen > vb6->prefixlen)
			return (1);
		if (va6->prefixlen < vb6->prefixlen)
			return (-1);
		return (0);
	default:
		fatalx("pt_prefix_cmp: unknown af");
	}
	return (-1);
}

/*
 * Returns a pt_entry cloned from the one passed in.
 * Function may not return on failure.
 */
static struct pt_entry *
pt_alloc(struct pt_entry *op, int len)
{
	struct pt_entry		*p;

	p = malloc(len);
	if (p == NULL)
		fatal("pt_alloc");
	rdemem.pt_cnt[op->aid]++;
	rdemem.pt_size[op->aid] += len;
	memcpy(p, op, len);
	p->refcnt = 0;

	return (p);
}

static void
pt_free(struct pt_entry *pte)
{
	rdemem.pt_cnt[pte->aid]--;
	rdemem.pt_size[pte->aid] -= pte->len;
	free(pte);
}

/* dump a prefix into specified buffer */
int
pt_write(u_char *buf, int len, struct pt_entry *pte, int withdraw)
{
	struct pt_entry_vpn4	*pvpn4 = (struct pt_entry_vpn4 *)pte;
	struct pt_entry_vpn6	*pvpn6 = (struct pt_entry_vpn6 *)pte;
	int			 totlen, psize;
	uint8_t			 plen;

	switch (pte->aid) {
	case AID_INET:
	case AID_INET6:
		plen = pte->prefixlen;
		totlen = PREFIX_SIZE(plen);

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		memcpy(buf, pte->data, totlen - 1);
		return (totlen);
	case AID_VPN_IPv4:
		plen = pvpn4->prefixlen;
		totlen = PREFIX_SIZE(plen) + sizeof(pvpn4->rd);
		psize = PREFIX_SIZE(plen) - 1;
		plen += sizeof(pvpn4->rd) * 8;
		if (withdraw) {
			/* withdraw have one compat label as placeholder */
			totlen += 3;
			plen += 3 * 8;
		} else {
			totlen += pvpn4->labellen;
			plen += pvpn4->labellen * 8;
		}

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		if (withdraw) {
			/* magic compatibility label as per rfc8277 */
			*buf++ = 0x80;
			*buf++ = 0x0;
			*buf++ = 0x0;
		} else {
			memcpy(buf, &pvpn4->labelstack, pvpn4->labellen);
			buf += pvpn4->labellen;
		}
		memcpy(buf, &pvpn4->rd, sizeof(pvpn4->rd));
		buf += sizeof(pvpn4->rd);
		memcpy(buf, &pvpn4->prefix4, psize);
		return (totlen);
	case AID_VPN_IPv6:
		plen = pvpn6->prefixlen;
		totlen = PREFIX_SIZE(plen) + sizeof(pvpn6->rd);
		psize = PREFIX_SIZE(plen) - 1;
		plen += sizeof(pvpn6->rd) * 8;
		if (withdraw) {
			/* withdraw have one compat label as placeholder */
			totlen += 3;
			plen += 3 * 8;
		} else {
			totlen += pvpn6->labellen;
			plen += pvpn6->labellen * 8;
		}

		if (totlen > len)
			return (-1);
		*buf++ = plen;
		if (withdraw) {
			/* magic compatibility label as per rfc8277 */
			*buf++ = 0x80;
			*buf++ = 0x0;
			*buf++ = 0x0;
		} else {
			memcpy(buf, &pvpn6->labelstack, pvpn6->labellen);
			buf += pvpn6->labellen;
		}
		memcpy(buf, &pvpn6->rd, sizeof(pvpn6->rd));
		buf += sizeof(pvpn6->rd);
		memcpy(buf, &pvpn6->prefix6, psize);
		return (totlen);
	default:
		return (-1);
	}
}

/* dump a prefix into specified ibuf, allocating space for it if needed */
int
pt_writebuf(struct ibuf *buf, struct pt_entry *pte)
{
	struct pt_entry_vpn4	*pvpn4 = (struct pt_entry_vpn4 *)pte;
	struct pt_entry_vpn6	*pvpn6 = (struct pt_entry_vpn6 *)pte;
	int	 		 totlen;
	void			*bptr;

	switch (pte->aid) {
	case AID_INET:
	case AID_INET6:
		totlen = PREFIX_SIZE(pte->prefixlen);
		break;
	case AID_VPN_IPv4:
		totlen = PREFIX_SIZE(pte->prefixlen) + sizeof(pvpn4->rd) +
		    pvpn4->labellen;
		break;
	case AID_VPN_IPv6:
		totlen = PREFIX_SIZE(pte->prefixlen) + sizeof(pvpn6->rd) +
		    pvpn6->labellen;
		break;
	default:
		return (-1);
	}

	if ((bptr = ibuf_reserve(buf, totlen)) == NULL)
		return (-1);
	if (pt_write(bptr, totlen, pte, 0) == -1)
		return (-1);
	return (0);
}

/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: a6.h,v 1.11 2001/01/09 21:52:14 bwelling Exp $ */

#ifndef DNS_A6_H
#define DNS_A6_H 1

#include <isc/lang.h>
#include <isc/stdtime.h>
#include <isc/bitstring.h>
#include <isc/net.h>

#include <dns/types.h>

typedef isc_result_t (*dns_findfunc_t)(void *arg, dns_name_t *name,
				       dns_rdatatype_t type,
				       isc_stdtime_t now,
				       dns_rdataset_t *rdataset,
				       dns_rdataset_t *sigrdataset);

typedef void (*dns_rrsetfunc_t)(void *arg, dns_name_t *name,
				dns_rdataset_t *rdataset,
				dns_rdataset_t *sigrdataset);

typedef void (*dns_in6addrfunc_t)(dns_a6context_t *a6ctx);

typedef void (*dns_a6missingfunc_t)(dns_a6context_t *a6ctx, dns_name_t *name);

struct dns_a6context {
	unsigned int			magic;
	/* Public. */
	dns_findfunc_t			find;
	dns_rrsetfunc_t			rrset;
	dns_in6addrfunc_t		address;
	dns_a6missingfunc_t		missing;
	void *				arg;
	unsigned int			chains;
	unsigned int			depth;
	isc_stdtime_t			now;
	isc_stdtime_t			expiration;
	unsigned int			prefixlen;
	struct in6_addr			in6addr;
	isc_bitstring_t			bitstring;
};

ISC_LANG_BEGINDECLS

void
dns_a6_init(dns_a6context_t *a6ctx, dns_findfunc_t find, dns_rrsetfunc_t rrset,
	    dns_in6addrfunc_t address, dns_a6missingfunc_t missing, void *arg);

void
dns_a6_reset(dns_a6context_t *a6ctx);

void
dns_a6_invalidate(dns_a6context_t *a6ctx);

void
dns_a6_copy(dns_a6context_t *source, dns_a6context_t *target);

isc_result_t
dns_a6_foreach(dns_a6context_t *a6ctx, dns_rdataset_t *rdataset,
	       isc_stdtime_t now);

ISC_LANG_ENDDECLS

#endif /* DNS_A6_H */

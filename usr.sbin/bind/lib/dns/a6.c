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

/* $ISC: a6.c,v 1.20 2001/06/04 19:32:54 tale Exp $ */

#include <config.h>

#include <isc/util.h>

#include <dns/a6.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>

#define A6CONTEXT_MAGIC		ISC_MAGIC('A', '6', 'X', 'X')
#define VALID_A6CONTEXT(ac)	ISC_MAGIC_VALID(ac, A6CONTEXT_MAGIC)

#define MAX_CHAINS	8
#define MAX_DEPTH	16

static inline void
maybe_disassociate(dns_rdataset_t *rdataset) {
	if (dns_rdataset_isassociated(rdataset))
		dns_rdataset_disassociate(rdataset);
}

static isc_result_t
foreach(dns_a6context_t *a6ctx, dns_rdataset_t *parent, unsigned int depth,
	unsigned int oprefixlen)
{
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_region_t r;
	dns_name_t name;
	dns_rdataset_t child;
	dns_rdataset_t childsig;
	isc_result_t result;
	isc_uint8_t prefixlen, octets;
	isc_bitstring_t bitstring;
	isc_stdtime_t expiration;

	expiration = a6ctx->now + parent->ttl;
	if (expiration < a6ctx->expiration || a6ctx->expiration == 0)
		a6ctx->expiration = expiration;

	depth++;
	result = dns_rdataset_first(parent);
	while (result == ISC_R_SUCCESS) {
		dns_rdataset_current(parent, &rdata);
		dns_rdata_toregion(&rdata, &r);
		prefixlen = r.base[0];
		if (prefixlen > oprefixlen) {
			/*
			 * Trying to go to a longer prefix is illegal.
			 */
			goto next_a6;
		}
		if (prefixlen < 128) {
			isc_bitstring_init(&bitstring, &r.base[1],
					   128 - prefixlen, 128 - prefixlen,
					   ISC_TRUE);
			isc_bitstring_copy(&bitstring, 128 - oprefixlen,
					   &a6ctx->bitstring, 128 - oprefixlen,
					   oprefixlen - prefixlen);
		}
		octets = 16 - prefixlen / 8;
		if (prefixlen != 0) {
			if (depth < MAX_DEPTH) {
				isc_region_consume(&r, octets + 1);
				dns_name_init(&name, NULL);
				dns_name_fromregion(&name, &r);
				dns_rdataset_init(&child);
				dns_rdataset_init(&childsig);
				result = (a6ctx->find)(a6ctx->arg, &name,
						       dns_rdatatype_a6,
						       a6ctx->now,
						       &child, &childsig);
				if (result == ISC_R_SUCCESS) {
					/*
					 * We've found a new A6 rrset.
					 */
					if (a6ctx->rrset != NULL)
						(a6ctx->rrset)(a6ctx->arg,
							       &name,
							       &child,
							       &childsig);
					/*
					 * Keep following the chain.
					 */
					result = foreach(a6ctx, &child, depth,
							 prefixlen);
					dns_rdataset_disassociate(&child);
					maybe_disassociate(&childsig);
					if (result != ISC_R_SUCCESS)
						break;
				} else if (result == ISC_R_NOTFOUND &&
					   a6ctx->missing != NULL) {
					/*
					 * We can't follow this chain, because
					 * we don't know the next link.
					 *
					 * We update the 'depth' and
					 * 'prefixlen' values so that the
					 * missing function can make a copy
					 * of the a6context and resume
					 * processing after it has found the
					 * missing a6 context.
					 */
					a6ctx->depth = depth;
					a6ctx->prefixlen = prefixlen;
					(a6ctx->missing)(a6ctx, &name);
				} else {
					/*
					 * Either something went wrong, or
					 * we got a negative cache response.
					 * In either case, we can't follow
					 * this chain further, and we don't
					 * want to call the 'missing'
					 * function.
					 *
					 * Note that we currently require that
					 * the target of an A6 record is
					 * a canonical domain name.  If the
					 * find routine returns DNS_R_CNAME or
					 * DNS_R_DNAME, we do NOT follow the
					 * chain.
					 *
					 * We do want to clean up...
					 */
					maybe_disassociate(&child);
					maybe_disassociate(&childsig);
				}
			}
		} else {
			/*
			 * We have a complete chain.
			 */
			if (a6ctx->address != NULL)
				(a6ctx->address)(a6ctx);
		}
	next_a6:
		dns_rdata_reset(&rdata);
		result = dns_rdataset_next(parent);
		if (result == ISC_R_SUCCESS) {
			a6ctx->chains++;
			if (a6ctx->chains > MAX_CHAINS)
				return (ISC_R_QUOTA);
		}
	}
	if (result != ISC_R_NOMORE)
		return (result);
	return (ISC_R_SUCCESS);
}

void
dns_a6_init(dns_a6context_t *a6ctx, dns_findfunc_t find, dns_rrsetfunc_t rrset,
	    dns_in6addrfunc_t address, dns_a6missingfunc_t missing, void *arg)
{
	REQUIRE(a6ctx != NULL);
	REQUIRE(find != NULL);

	a6ctx->magic = A6CONTEXT_MAGIC;
	a6ctx->find = find;
	a6ctx->rrset = rrset;
	a6ctx->missing = missing;
	a6ctx->address = address;
	a6ctx->arg = arg;
	a6ctx->chains = 1;
	a6ctx->depth = 0;
	a6ctx->now = 0;
	a6ctx->expiration = 0;
	a6ctx->prefixlen = 128;
	isc_bitstring_init(&a6ctx->bitstring,
			   (unsigned char *)a6ctx->in6addr.s6_addr,
			   128, 128, ISC_TRUE);
}

void
dns_a6_reset(dns_a6context_t *a6ctx) {
	REQUIRE(VALID_A6CONTEXT(a6ctx));

	a6ctx->chains = 1;
	a6ctx->depth = 0;
	a6ctx->expiration = 0;
	a6ctx->prefixlen = 128;
}

void
dns_a6_invalidate(dns_a6context_t *a6ctx) {
	REQUIRE(VALID_A6CONTEXT(a6ctx));

	a6ctx->magic = 0;
}

void
dns_a6_copy(dns_a6context_t *source, dns_a6context_t *target) {
	REQUIRE(VALID_A6CONTEXT(source));
	REQUIRE(VALID_A6CONTEXT(target));

	*target = *source;
	isc_bitstring_init(&target->bitstring,
			   (unsigned char *)target->in6addr.s6_addr,
			   128, 128, ISC_TRUE);
}

isc_result_t
dns_a6_foreach(dns_a6context_t *a6ctx, dns_rdataset_t *rdataset,
	       isc_stdtime_t now)
{
	isc_result_t result;

	REQUIRE(VALID_A6CONTEXT(a6ctx));
	REQUIRE(rdataset->type == dns_rdatatype_a6);

	if (now == 0)
		isc_stdtime_get(&now);
	a6ctx->now = now;

	result = foreach(a6ctx, rdataset, a6ctx->depth, a6ctx->prefixlen);
	if (result == ISC_R_QUOTA)
		result = ISC_R_SUCCESS;

	return (result);
}

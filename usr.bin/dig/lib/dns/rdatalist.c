/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rdatalist.c,v 1.6 2022/06/26 09:43:39 florian Exp $ */

/*! \file */

#include <stddef.h>

#include <isc/util.h>

#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>

#include "rdatalist_p.h"

static dns_rdatasetmethods_t methods = {
	isc__rdatalist_disassociate,
	isc__rdatalist_first,
	isc__rdatalist_next,
	isc__rdatalist_current,
	isc__rdatalist_clone,
	isc__rdatalist_count,
};

void
dns_rdatalist_init(dns_rdatalist_t *rdatalist) {

	REQUIRE(rdatalist != NULL);

	/*
	 * Initialize rdatalist.
	 */

	rdatalist->rdclass = 0;
	rdatalist->type = 0;
	rdatalist->covers = 0;
	rdatalist->ttl = 0;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LINK_INIT(rdatalist, link);
}

isc_result_t
dns_rdatalist_tordataset(dns_rdatalist_t *rdatalist,
			 dns_rdataset_t *rdataset)
{
	/*
	 * Make 'rdataset' refer to the rdata in 'rdatalist'.
	 */

	REQUIRE(rdatalist != NULL);
	REQUIRE(! dns_rdataset_isassociated(rdataset));

	rdataset->methods = &methods;
	rdataset->rdclass = rdatalist->rdclass;
	rdataset->type = rdatalist->type;
	rdataset->covers = rdatalist->covers;
	rdataset->ttl = rdatalist->ttl;
	rdataset->private1 = rdatalist;
	rdataset->private2 = NULL;

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_rdatalist_fromrdataset(dns_rdataset_t *rdataset,
			   dns_rdatalist_t **rdatalist)
{
	REQUIRE(rdatalist != NULL && rdataset != NULL);
	*rdatalist = rdataset->private1;

	return (ISC_R_SUCCESS);
}

void
isc__rdatalist_disassociate(dns_rdataset_t *rdataset) {
	UNUSED(rdataset);
}

isc_result_t
isc__rdatalist_first(dns_rdataset_t *rdataset) {
	dns_rdatalist_t *rdatalist;

	rdatalist = rdataset->private1;
	rdataset->private2 = ISC_LIST_HEAD(rdatalist->rdata);

	if (rdataset->private2 == NULL)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc__rdatalist_next(dns_rdataset_t *rdataset) {
	dns_rdata_t *rdata;

	REQUIRE(rdataset != NULL);

	rdata = rdataset->private2;
	if (rdata == NULL)
		return (ISC_R_NOMORE);

	rdataset->private2 = ISC_LIST_NEXT(rdata, link);

	if (rdataset->private2 == NULL)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

void
isc__rdatalist_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {
	dns_rdata_t *list_rdata;

	REQUIRE(rdataset != NULL);

	list_rdata = rdataset->private2;
	INSIST(list_rdata != NULL);

	dns_rdata_clone(list_rdata, rdata);
}

void
isc__rdatalist_clone(dns_rdataset_t *source, dns_rdataset_t *target) {

	REQUIRE(source != NULL);
	REQUIRE(target != NULL);

	*target = *source;

	/*
	 * Reset iterator state.
	 */
	target->private2 = NULL;
}

unsigned int
isc__rdatalist_count(dns_rdataset_t *rdataset) {
	dns_rdatalist_t *rdatalist;
	dns_rdata_t *rdata;
	unsigned int count;

	REQUIRE(rdataset != NULL);

	rdatalist = rdataset->private1;

	count = 0;
	for (rdata = ISC_LIST_HEAD(rdatalist->rdata);
	     rdata != NULL;
	     rdata = ISC_LIST_NEXT(rdata, link))
		count++;

	return (count);
}

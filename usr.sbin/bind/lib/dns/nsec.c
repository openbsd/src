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

/* $Id: nsec.c,v 1.8 2020/01/28 17:17:05 florian Exp $ */

/*! \file */



#include <isc/log.h>
#include <string.h>
#include <isc/util.h>


#include <dns/nsec.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>

#include "rdatastruct.h"
#include <dns/result.h>

#include <dst/dst.h>

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)

void
dns_nsec_setbit(unsigned char *array, unsigned int type, unsigned int bit) {
	unsigned int shift, mask;

	shift = 7 - (type % 8);
	mask = 1 << shift;

	if (bit != 0)
		array[type / 8] |= mask;
	else
		array[type / 8] &= (~mask & 0xFF);
}

isc_boolean_t
dns_nsec_isset(const unsigned char *array, unsigned int type) {
	unsigned int byte, shift, mask;

	byte = array[type / 8];
	shift = 7 - (type % 8);
	mask = 1 << shift;

	return (ISC_TF(byte & mask));
}

unsigned int
dns_nsec_compressbitmap(unsigned char *map, const unsigned char *raw,
			unsigned int max_type)
{
	unsigned char *start = map;
	unsigned int window;
	int octet;

	if (raw == NULL)
		return (0);

	for (window = 0; window < 256; window++) {
		if (window * 256 > max_type)
			break;
		for (octet = 31; octet >= 0; octet--)
			if (*(raw + octet) != 0)
				break;
		if (octet < 0) {
			raw += 32;
			continue;
		}
		*map++ = window;
		*map++ = octet + 1;
		/*
		 * Note: potential overlapping move.
		 */
		memmove(map, raw, octet + 1);
		map += octet + 1;
		raw += 32;
	}
	return (unsigned int)(map - start);
}

isc_boolean_t
dns_nsec_typepresent(dns_rdata_t *nsec, dns_rdatatype_t type) {
	dns_rdata_nsec_t nsecstruct;
	isc_result_t result;
	isc_boolean_t present;
	unsigned int i, len, window;

	REQUIRE(nsec != NULL);
	REQUIRE(nsec->type == dns_rdatatype_nsec);

	/* This should never fail */
	result = dns_rdata_tostruct(nsec, &nsecstruct);
	INSIST(result == ISC_R_SUCCESS);

	present = ISC_FALSE;
	for (i = 0; i < nsecstruct.len; i += len) {
		INSIST(i + 2 <= nsecstruct.len);
		window = nsecstruct.typebits[i];
		len = nsecstruct.typebits[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= nsecstruct.len);
		if (window * 256 > type)
			break;
		if ((window + 1) * 256 <= type)
			continue;
		if (type < (window * 256) + len * 8)
			present = ISC_TF(dns_nsec_isset(&nsecstruct.typebits[i],
							type % 256));
		break;
	}
	dns_rdata_freestruct(&nsecstruct);
	return (present);
}

/*%
 * Return ISC_R_SUCCESS if we can determine that the name doesn't exist
 * or we can determine whether there is data or not at the name.
 * If the name does not exist return the wildcard name.
 *
 * Return ISC_R_IGNORE when the NSEC is not the appropriate one.
 */
isc_result_t
dns_nsec_noexistnodata(dns_rdatatype_t type, dns_name_t *name,
		       dns_name_t *nsecname, dns_rdataset_t *nsecset,
		       isc_boolean_t *exists, isc_boolean_t *data,
		       dns_name_t *wild, dns_nseclog_t logit, void *arg)
{
	int order;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	dns_namereln_t relation;
	unsigned int olabels, nlabels, labels;
	dns_rdata_nsec_t nsec;
	isc_boolean_t atparent;
	isc_boolean_t ns;
	isc_boolean_t soa;

	REQUIRE(exists != NULL);
	REQUIRE(data != NULL);
	REQUIRE(nsecset != NULL &&
		nsecset->type == dns_rdatatype_nsec);

	result = dns_rdataset_first(nsecset);
	if (result != ISC_R_SUCCESS) {
		(*logit)(arg, ISC_LOG_DEBUG(3), "failure processing NSEC set");
		return (result);
	}
	dns_rdataset_current(nsecset, &rdata);

	(*logit)(arg, ISC_LOG_DEBUG(3), "looking for relevant NSEC");
	relation = dns_name_fullcompare(name, nsecname, &order, &olabels);

	if (order < 0) {
		/*
		 * The name is not within the NSEC range.
		 */
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "NSEC does not cover name, before NSEC");
		return (ISC_R_IGNORE);
	}

	if (order == 0) {
		/*
		 * The names are the same.   If we are validating "."
		 * then atparent should not be set as there is no parent.
		 */
		atparent = (olabels != 1) && dns_rdatatype_atparent(type);
		ns = dns_nsec_typepresent(&rdata, dns_rdatatype_ns);
		soa = dns_nsec_typepresent(&rdata, dns_rdatatype_soa);
		if (ns && !soa) {
			if (!atparent) {
				/*
				 * This NSEC record is from somewhere higher in
				 * the DNS, and at the parent of a delegation.
				 * It can not be legitimately used here.
				 */
				(*logit)(arg, ISC_LOG_DEBUG(3),
					      "ignoring parent nsec");
				return (ISC_R_IGNORE);
			}
		} else if (atparent && ns && soa) {
			/*
			 * This NSEC record is from the child.
			 * It can not be legitimately used here.
			 */
			(*logit)(arg, ISC_LOG_DEBUG(3),
				      "ignoring child nsec");
			return (ISC_R_IGNORE);
		}
		if (type == dns_rdatatype_cname || type == dns_rdatatype_nxt ||
		    type == dns_rdatatype_nsec || type == dns_rdatatype_key ||
		    !dns_nsec_typepresent(&rdata, dns_rdatatype_cname)) {
			*exists = ISC_TRUE;
			*data = dns_nsec_typepresent(&rdata, type);
			(*logit)(arg, ISC_LOG_DEBUG(3),
				      "nsec proves name exists (owner) data=%d",
				      *data);
			return (ISC_R_SUCCESS);
		}
		(*logit)(arg, ISC_LOG_DEBUG(3), "NSEC proves CNAME exists");
		return (ISC_R_IGNORE);
	}

	if (relation == dns_namereln_subdomain &&
	    dns_nsec_typepresent(&rdata, dns_rdatatype_ns) &&
	    !dns_nsec_typepresent(&rdata, dns_rdatatype_soa))
	{
		/*
		 * This NSEC record is from somewhere higher in
		 * the DNS, and at the parent of a delegation.
		 * It can not be legitimately used here.
		 */
		(*logit)(arg, ISC_LOG_DEBUG(3), "ignoring parent nsec");
		return (ISC_R_IGNORE);
	}

	result = dns_rdata_tostruct(&rdata, &nsec);
	if (result != ISC_R_SUCCESS)
		return (result);
	relation = dns_name_fullcompare(&nsec.next, name, &order, &nlabels);
	if (order == 0) {
		dns_rdata_freestruct(&nsec);
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "ignoring nsec matches next name");
		return (ISC_R_IGNORE);
	}

	if (order < 0 && !dns_name_issubdomain(nsecname, &nsec.next)) {
		/*
		 * The name is not within the NSEC range.
		 */
		dns_rdata_freestruct(&nsec);
		(*logit)(arg, ISC_LOG_DEBUG(3),
			    "ignoring nsec because name is past end of range");
		return (ISC_R_IGNORE);
	}

	if (order > 0 && relation == dns_namereln_subdomain) {
		(*logit)(arg, ISC_LOG_DEBUG(3),
			      "nsec proves name exist (empty)");
		dns_rdata_freestruct(&nsec);
		*exists = ISC_TRUE;
		*data = ISC_FALSE;
		return (ISC_R_SUCCESS);
	}
	if (wild != NULL) {
		dns_name_t common;
		dns_name_init(&common, NULL);
		if (olabels > nlabels) {
			labels = dns_name_countlabels(nsecname);
			dns_name_getlabelsequence(nsecname, labels - olabels,
						  olabels, &common);
		} else {
			labels = dns_name_countlabels(&nsec.next);
			dns_name_getlabelsequence(&nsec.next, labels - nlabels,
						  nlabels, &common);
		}
		result = dns_name_concatenate(dns_wildcardname, &common,
					      wild, NULL);
		if (result != ISC_R_SUCCESS) {
			dns_rdata_freestruct(&nsec);
			(*logit)(arg, ISC_LOG_DEBUG(3),
				    "failure generating wildcard name");
			return (result);
		}
	}
	dns_rdata_freestruct(&nsec);
	(*logit)(arg, ISC_LOG_DEBUG(3), "nsec range ok");
	*exists = ISC_FALSE;
	return (ISC_R_SUCCESS);
}

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

/* $Id: nsec3.c,v 1.8 2020/01/28 17:17:05 florian Exp $ */



#include <isc/base32.h>
#include <isc/buffer.h>
#include <isc/hex.h>
#include <isc/iterated_hash.h>
#include <isc/log.h>
#include <string.h>
#include <isc/util.h>
#include <isc/safe.h>

#include <dst/dst.h>



#include <dns/compress.h>


#include <dns/fixedname.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>

#include "rdatastruct.h"
#include <dns/result.h>

#define CHECK(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)

#define OPTOUT(x) (((x) & DNS_NSEC3FLAG_OPTOUT) != 0)
#define CREATE(x) (((x) & DNS_NSEC3FLAG_CREATE) != 0)
#define INITIAL(x) (((x) & DNS_NSEC3FLAG_INITIAL) != 0)
#define REMOVE(x) (((x) & DNS_NSEC3FLAG_REMOVE) != 0)

isc_boolean_t
dns_nsec3_typepresent(dns_rdata_t *rdata, dns_rdatatype_t type) {
	dns_rdata_nsec3_t nsec3;
	isc_result_t result;
	isc_boolean_t present;
	unsigned int i, len, window;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_nsec3);

	/* This should never fail */
	result = dns_rdata_tostruct(rdata, &nsec3);
	INSIST(result == ISC_R_SUCCESS);

	present = ISC_FALSE;
	for (i = 0; i < nsec3.len; i += len) {
		INSIST(i + 2 <= nsec3.len);
		window = nsec3.typebits[i];
		len = nsec3.typebits[i + 1];
		INSIST(len > 0 && len <= 32);
		i += 2;
		INSIST(i + len <= nsec3.len);
		if (window * 256 > type)
			break;
		if ((window + 1) * 256 <= type)
			continue;
		if (type < (window * 256) + len * 8)
			present = ISC_TF(dns_nsec_isset(&nsec3.typebits[i],
							type % 256));
		break;
	}
	dns_rdata_freestruct(&nsec3);
	return (present);
}

isc_result_t
dns_nsec3_hashname(dns_fixedname_t *result,
		   unsigned char rethash[NSEC3_MAX_HASH_LENGTH],
		   size_t *hash_length, dns_name_t *name, dns_name_t *origin,
		   dns_hash_t hashalg, unsigned int iterations,
		   const unsigned char *salt, size_t saltlength)
{
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	unsigned char nametext[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *downcased;
	isc_buffer_t namebuffer;
	isc_region_t region;
	size_t len;

	if (rethash == NULL)
		rethash = hash;

	memset(rethash, 0, NSEC3_MAX_HASH_LENGTH);

	dns_fixedname_init(&fixed);
	downcased = dns_fixedname_name(&fixed);
	dns_name_downcase(name, downcased, NULL);

	/* hash the node name */
	len = isc_iterated_hash(rethash, hashalg, iterations,
				salt, (int)saltlength,
				downcased->ndata, downcased->length);
	if (len == 0U)
		return (DNS_R_BADALG);

	if (hash_length != NULL)
		*hash_length = len;

	/* convert the hash to base32hex non-padded */
	region.base = rethash;
	region.length = (unsigned int)len;
	isc_buffer_init(&namebuffer, nametext, sizeof nametext);
	isc_base32hexnp_totext(&region, 1, "", &namebuffer);

	/* convert the hex to a domain name */
	dns_fixedname_init(result);
	return (dns_name_fromtext(dns_fixedname_name(result), &namebuffer,
				  origin, 0, NULL));
}

unsigned int
dns_nsec3_hashlength(dns_hash_t hash) {

	switch (hash) {
	case dns_hash_sha1:
		return(ISC_SHA1_DIGESTLENGTH);
	}
	return (0);
}

isc_boolean_t
dns_nsec3_supportedhash(dns_hash_t hash) {
	switch (hash) {
	case dns_hash_sha1:
		return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

isc_boolean_t
dns_nsec3param_fromprivate(dns_rdata_t *src, dns_rdata_t *target,
			   unsigned char *buf, size_t buflen)
{
	dns_decompress_t dctx;
	isc_result_t result;
	isc_buffer_t buf1;
	isc_buffer_t buf2;

	/*
	 * Algorithm 0 (reserved by RFC 4034) is used to identify
	 * NSEC3PARAM records from DNSKEY pointers.
	 */
	if (src->length < 1 || src->data[0] != 0)
		return (ISC_FALSE);

	isc_buffer_init(&buf1, src->data + 1, src->length - 1);
	isc_buffer_add(&buf1, src->length - 1);
	isc_buffer_setactive(&buf1, src->length - 1);
	isc_buffer_init(&buf2, buf, (unsigned int)buflen);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_NONE);
	result = dns_rdata_fromwire(target, src->rdclass,
				    dns_rdatatype_nsec3param,
				    &buf1, &dctx, 0, &buf2);
	dns_decompress_invalidate(&dctx);

	return (ISC_TF(result == ISC_R_SUCCESS));
}

void
dns_nsec3param_toprivate(dns_rdata_t *src, dns_rdata_t *target,
			 dns_rdatatype_t privatetype,
			 unsigned char *buf, size_t buflen)
{
	REQUIRE(buflen >= src->length + 1);

	REQUIRE(DNS_RDATA_INITIALIZED(target));

	memmove(buf + 1, src->data, src->length);
	buf[0] = 0;
	target->data = buf;
	target->length = src->length + 1;
	target->type = privatetype;
	target->rdclass = src->rdclass;
	target->flags = 0;
	ISC_LINK_INIT(target, link);
}


isc_result_t
dns_nsec3param_salttotext(dns_rdata_nsec3param_t *nsec3param, char *dst,
			  size_t dstlen)
{
	isc_result_t result;
	isc_region_t r;
	isc_buffer_t b;

	REQUIRE(nsec3param != NULL);
	REQUIRE(dst != NULL);

	if (nsec3param->salt_length == 0) {
		if (dstlen < 2U) {
			return (ISC_R_NOSPACE);
		}
		strlcpy(dst, "-", dstlen);
		return (ISC_R_SUCCESS);
	}

	r.base = nsec3param->salt;
	r.length = nsec3param->salt_length;
	isc_buffer_init(&b, dst, (unsigned int)dstlen);

	result = isc_hex_totext(&r, 2, "", &b);
	if (result != ISC_R_SUCCESS) {
		return (result);
	}

	if (isc_buffer_availablelength(&b) < 1) {
		return (ISC_R_NOSPACE);
	}
	isc_buffer_putuint8(&b, 0);

	return (ISC_R_SUCCESS);
}



isc_result_t
dns_nsec3_noexistnodata(dns_rdatatype_t type, dns_name_t* name,
			dns_name_t *nsec3name, dns_rdataset_t *nsec3set,
			dns_name_t *zonename, isc_boolean_t *exists,
			isc_boolean_t *data, isc_boolean_t *optout,
			isc_boolean_t *unknown, isc_boolean_t *setclosest,
			isc_boolean_t *setnearest, dns_name_t *closest,
			dns_name_t *nearest, dns_nseclog_t logit, void *arg)
{
	char namebuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fzone;
	dns_fixedname_t qfixed;
	dns_label_t hashlabel;
	dns_name_t *qname;
	dns_name_t *zone;
	dns_rdata_nsec3_t nsec3;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	int order;
	int scope;
	isc_boolean_t atparent;
	isc_boolean_t first;
	isc_boolean_t ns;
	isc_boolean_t soa;
	isc_buffer_t buffer;
	isc_result_t answer = ISC_R_IGNORE;
	isc_result_t result;
	unsigned char hash[NSEC3_MAX_HASH_LENGTH];
	unsigned char owner[NSEC3_MAX_HASH_LENGTH];
	unsigned int length;
	unsigned int qlabels;
	unsigned int zlabels;

	REQUIRE((exists == NULL && data == NULL) ||
		(exists != NULL && data != NULL));
	REQUIRE(nsec3set != NULL && nsec3set->type == dns_rdatatype_nsec3);
	REQUIRE((setclosest == NULL && closest == NULL) ||
		(setclosest != NULL && closest != NULL));
	REQUIRE((setnearest == NULL && nearest == NULL) ||
		(setnearest != NULL && nearest != NULL));

	result = dns_rdataset_first(nsec3set);
	if (result != ISC_R_SUCCESS) {
		(*logit)(arg, ISC_LOG_DEBUG(3), "failure processing NSEC3 set");
		return (result);
	}

	dns_rdataset_current(nsec3set, &rdata);

	result = dns_rdata_tostruct(&rdata, &nsec3);
	if (result != ISC_R_SUCCESS)
		return (result);

	(*logit)(arg, ISC_LOG_DEBUG(3), "looking for relevant NSEC3");

	dns_fixedname_init(&fzone);
	zone = dns_fixedname_name(&fzone);
	zlabels = dns_name_countlabels(nsec3name);

	/*
	 * NSEC3 records must have two or more labels to be valid.
	 */
	if (zlabels < 2)
		return (ISC_R_IGNORE);

	/*
	 * Strip off the NSEC3 hash to get the zone.
	 */
	zlabels--;
	dns_name_split(nsec3name, zlabels, NULL, zone);

	/*
	 * If not below the zone name we can ignore this record.
	 */
	if (!dns_name_issubdomain(name, zone))
		return (ISC_R_IGNORE);

	/*
	 * Is this zone the same or deeper than the current zone?
	 */
	if (dns_name_countlabels(zonename) == 0 ||
	    dns_name_issubdomain(zone, zonename))
		dns_name_copy(zone, zonename, NULL);

	if (!dns_name_equal(zone, zonename))
		return (ISC_R_IGNORE);

	/*
	 * Are we only looking for the most enclosing zone?
	 */
	if (exists == NULL || data == NULL)
		return (ISC_R_SUCCESS);

	/*
	 * Only set unknown once we are sure that this NSEC3 is from
	 * the deepest covering zone.
	 */
	if (!dns_nsec3_supportedhash(nsec3.hash)) {
		if (unknown != NULL)
			*unknown = ISC_TRUE;
		return (ISC_R_IGNORE);
	}

	/*
	 * Recover the hash from the first label.
	 */
	dns_name_getlabel(nsec3name, 0, &hashlabel);
	isc_region_consume(&hashlabel, 1);
	isc_buffer_init(&buffer, owner, sizeof(owner));
	result = isc_base32hex_decoderegion(&hashlabel, &buffer);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * The hash lengths should match.  If not ignore the record.
	 */
	if (isc_buffer_usedlength(&buffer) != nsec3.next_length)
		return (ISC_R_IGNORE);

	/*
	 * Work out what this NSEC3 covers.
	 * Inside (<0) or outside (>=0).
	 */
	scope = isc_safe_memcompare(owner, nsec3.next, nsec3.next_length);

	/*
	 * Prepare to compute all the hashes.
	 */
	dns_fixedname_init(&qfixed);
	qname = dns_fixedname_name(&qfixed);
	dns_name_downcase(name, qname, NULL);
	qlabels = dns_name_countlabels(qname);
	first = ISC_TRUE;

	while (qlabels >= zlabels) {
		length = isc_iterated_hash(hash, nsec3.hash, nsec3.iterations,
					   nsec3.salt, nsec3.salt_length,
					   qname->ndata, qname->length);
		/*
		 * The computed hash length should match.
		 */
		if (length != nsec3.next_length) {
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "ignoring NSEC bad length %u vs %u",
				 length, nsec3.next_length);
			return (ISC_R_IGNORE);
		}

		order = isc_safe_memcompare(hash, owner, length);
		if (first && order == 0) {
			/*
			 * The hashes are the same.
			 */
			atparent = dns_rdatatype_atparent(type);
			ns = dns_nsec3_typepresent(&rdata, dns_rdatatype_ns);
			soa = dns_nsec3_typepresent(&rdata, dns_rdatatype_soa);
			if (ns && !soa) {
				if (!atparent) {
					/*
					 * This NSEC3 record is from somewhere
					 * higher in the DNS, and at the
					 * parent of a delegation. It can not
					 * be legitimately used here.
					 */
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "ignoring parent NSEC3");
					return (ISC_R_IGNORE);
				}
			} else if (atparent && ns && soa) {
				/*
				 * This NSEC3 record is from the child.
				 * It can not be legitimately used here.
				 */
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "ignoring child NSEC3");
				return (ISC_R_IGNORE);
			}
			if (type == dns_rdatatype_cname ||
			    type == dns_rdatatype_nxt ||
			    type == dns_rdatatype_nsec ||
			    type == dns_rdatatype_key ||
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_cname)) {
				*exists = ISC_TRUE;
				*data = dns_nsec3_typepresent(&rdata, type);
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "NSEC3 proves name exists (owner) "
					 "data=%d", *data);
				return (ISC_R_SUCCESS);
			}
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "NSEC3 proves CNAME exists");
			return (ISC_R_IGNORE);
		}

		if (order == 0 &&
		    dns_nsec3_typepresent(&rdata, dns_rdatatype_ns) &&
		    !dns_nsec3_typepresent(&rdata, dns_rdatatype_soa))
		{
			/*
			 * This NSEC3 record is from somewhere higher in
			 * the DNS, and at the parent of a delegation.
			 * It can not be legitimately used here.
			 */
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "ignoring parent NSEC3");
			return (ISC_R_IGNORE);
		}

		/*
		 * Potential closest encloser.
		 */
		if (order == 0) {
			if (closest != NULL &&
			    (dns_name_countlabels(closest) == 0 ||
			     dns_name_issubdomain(qname, closest)) &&
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_ds) &&
			    !dns_nsec3_typepresent(&rdata, dns_rdatatype_dname) &&
			    (dns_nsec3_typepresent(&rdata, dns_rdatatype_soa) ||
			     !dns_nsec3_typepresent(&rdata, dns_rdatatype_ns)))
			{

				dns_name_format(qname, namebuf,
						sizeof(namebuf));
				(*logit)(arg, ISC_LOG_DEBUG(3),
					 "NSEC3 indicates potential closest "
					 "encloser: '%s'", namebuf);
				dns_name_copy(qname, closest, NULL);
				*setclosest = ISC_TRUE;
			}
			dns_name_format(qname, namebuf, sizeof(namebuf));
			(*logit)(arg, ISC_LOG_DEBUG(3),
				 "NSEC3 at super-domain %s", namebuf);
			return (answer);
		}

		/*
		 * Find if the name does not exist.
		 *
		 * We continue as we need to find the name closest to the
		 * closest encloser that doesn't exist.
		 *
		 * We also need to continue to ensure that we are not
		 * proving the non-existence of a record in a sub-zone.
		 * If that would be the case we will return ISC_R_IGNORE
		 * above.
		 */
		if ((scope < 0 && order > 0 &&
		     memcmp(hash, nsec3.next, length) < 0) ||
		    (scope >= 0 && (order > 0 ||
				    memcmp(hash, nsec3.next, length) < 0)))
		{
			dns_name_format(qname, namebuf, sizeof(namebuf));
			(*logit)(arg, ISC_LOG_DEBUG(3), "NSEC3 proves "
				 "name does not exist: '%s'", namebuf);
			if (nearest != NULL &&
			    (dns_name_countlabels(nearest) == 0 ||
			     dns_name_issubdomain(nearest, qname))) {
				dns_name_copy(qname, nearest, NULL);
				*setnearest = ISC_TRUE;
			}

			*exists = ISC_FALSE;
			*data = ISC_FALSE;
			if (optout != NULL) {
				if ((nsec3.flags & DNS_NSEC3FLAG_OPTOUT) != 0)
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "NSEC3 indicates optout");
				else
					(*logit)(arg, ISC_LOG_DEBUG(3),
						 "NSEC3 indicates secure range");
				*optout =
				    ISC_TF(nsec3.flags & DNS_NSEC3FLAG_OPTOUT);
			}
			answer = ISC_R_SUCCESS;
		}

		qlabels--;
		if (qlabels > 0)
			dns_name_split(qname, qlabels, NULL, qname);
		first = ISC_FALSE;
	}
	return (answer);
}

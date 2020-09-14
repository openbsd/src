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

/*! \file */

#include <stdint.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/util.h>

#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/compress.h>

void
dns_rdataset_init(dns_rdataset_t *rdataset) {

	/*
	 * Make 'rdataset' a valid, disassociated rdataset.
	 */

	REQUIRE(rdataset != NULL);

	rdataset->methods = NULL;
	ISC_LINK_INIT(rdataset, link);
	rdataset->rdclass = 0;
	rdataset->type = 0;
	rdataset->ttl = 0;
	rdataset->trust = 0;
	rdataset->covers = 0;
	rdataset->attributes = 0;
	rdataset->count = UINT32_MAX;
	rdataset->private1 = NULL;
	rdataset->private2 = NULL;
	rdataset->private3 = NULL;
	rdataset->privateuint4 = 0;
	rdataset->private5 = NULL;
	rdataset->private6 = NULL;
	rdataset->private7 = NULL;
	rdataset->resign = 0;
}

void
dns_rdataset_disassociate(dns_rdataset_t *rdataset) {

	/*
	 * Disassociate 'rdataset' from its rdata, allowing it to be reused.
	 */

	REQUIRE(rdataset->methods != NULL);

	(rdataset->methods->disassociate)(rdataset);
	rdataset->methods = NULL;
	ISC_LINK_INIT(rdataset, link);
	rdataset->rdclass = 0;
	rdataset->type = 0;
	rdataset->ttl = 0;
	rdataset->trust = 0;
	rdataset->covers = 0;
	rdataset->attributes = 0;
	rdataset->count = UINT32_MAX;
	rdataset->private1 = NULL;
	rdataset->private2 = NULL;
	rdataset->private3 = NULL;
	rdataset->privateuint4 = 0;
	rdataset->private5 = NULL;
	rdataset->private6 = NULL;
}

int
dns_rdataset_isassociated(dns_rdataset_t *rdataset) {
	/*
	 * Is 'rdataset' associated?
	 */

	if (rdataset->methods != NULL)
		return (1);

	return (0);
}

static void
question_disassociate(dns_rdataset_t *rdataset) {
	UNUSED(rdataset);
}

static isc_result_t
question_cursor(dns_rdataset_t *rdataset) {
	UNUSED(rdataset);

	return (ISC_R_NOMORE);
}

static void
question_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {
	/*
	 * This routine should never be called.
	 */
	UNUSED(rdataset);
	UNUSED(rdata);

	REQUIRE(0);
}

static void
question_clone(dns_rdataset_t *source, dns_rdataset_t *target) {
	*target = *source;
}

static unsigned int
question_count(dns_rdataset_t *rdataset) {
	/*
	 * This routine should never be called.
	 */
	UNUSED(rdataset);
	REQUIRE(0);

	return (0);
}

static dns_rdatasetmethods_t question_methods = {
	question_disassociate,
	question_cursor,
	question_cursor,
	question_current,
	question_clone,
	question_count,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

void
dns_rdataset_makequestion(dns_rdataset_t *rdataset, dns_rdataclass_t rdclass,
			  dns_rdatatype_t type)
{

	/*
	 * Make 'rdataset' a valid, associated, question rdataset, with a
	 * question class of 'rdclass' and type 'type'.
	 */

	REQUIRE(rdataset->methods == NULL);

	rdataset->methods = &question_methods;
	rdataset->rdclass = rdclass;
	rdataset->type = type;
	rdataset->attributes |= DNS_RDATASETATTR_QUESTION;
}

void
dns_rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target) {

	/*
	 * Make 'target' refer to the same rdataset as 'source'.
	 */

	REQUIRE(source->methods != NULL);
	REQUIRE(target->methods == NULL);

	(source->methods->clone)(source, target);
}

isc_result_t
dns_rdataset_first(dns_rdataset_t *rdataset) {

	/*
	 * Move the rdata cursor to the first rdata in the rdataset (if any).
	 */

	REQUIRE(rdataset->methods != NULL);

	return ((rdataset->methods->first)(rdataset));
}

isc_result_t
dns_rdataset_next(dns_rdataset_t *rdataset) {

	/*
	 * Move the rdata cursor to the next rdata in the rdataset (if any).
	 */

	REQUIRE(rdataset->methods != NULL);

	return ((rdataset->methods->next)(rdataset));
}

void
dns_rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {

	/*
	 * Make 'rdata' refer to the current rdata.
	 */

	REQUIRE(rdataset->methods != NULL);

	(rdataset->methods->current)(rdataset, rdata);
}

#define MAX_SHUFFLE	32
#define WANT_FIXED(r)	(((r)->attributes & DNS_RDATASETATTR_FIXEDORDER) != 0)
#define WANT_RANDOM(r)	(((r)->attributes & DNS_RDATASETATTR_RANDOMIZE) != 0)

struct towire_sort {
	int key;
	dns_rdata_t *rdata;
};

static int
towire_compare(const void *av, const void *bv) {
	const struct towire_sort *a = (const struct towire_sort *) av;
	const struct towire_sort *b = (const struct towire_sort *) bv;
	return (a->key - b->key);
}

static isc_result_t
towiresorted(dns_rdataset_t *rdataset, const dns_name_t *owner_name,
	     dns_compress_t *cctx, isc_buffer_t *target,
	     dns_rdatasetorderfunc_t order, const void *order_arg,
	     int partial, unsigned int *countp)
{
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_region_t r;
	isc_result_t result;
	unsigned int i, count = 0, added, choice;
	isc_buffer_t savedbuffer, rdlen, rrbuffer;
	unsigned int headlen;
	int question = 0;
	int shuffle = 0;
	dns_rdata_t *in = NULL, in_fixed[MAX_SHUFFLE];
	struct towire_sort *out = NULL, out_fixed[MAX_SHUFFLE];

	/*
	 * Convert 'rdataset' to wire format, compressing names as specified
	 * in cctx, and storing the result in 'target'.
	 */

	REQUIRE(rdataset->methods != NULL);
	REQUIRE(countp != NULL);
	REQUIRE((order == NULL) == (order_arg == NULL));
	REQUIRE(cctx != NULL);

	if ((rdataset->attributes & DNS_RDATASETATTR_QUESTION) != 0) {
		question = 1;
		count = 1;
		result = dns_rdataset_first(rdataset);
		INSIST(result == ISC_R_NOMORE);
	} else {
		count = (rdataset->methods->count)(rdataset);
		result = dns_rdataset_first(rdataset);
		if (result == ISC_R_NOMORE)
			return (ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	/*
	 * Do we want to shuffle this answer?
	 */
	if (!question && count > 1 &&
	    (!WANT_FIXED(rdataset) || order != NULL) &&
	    rdataset->type != dns_rdatatype_rrsig)
		shuffle = 1;

	if (shuffle && count > MAX_SHUFFLE) {
		in = reallocarray(NULL, count, sizeof(*in));
		out = reallocarray(NULL, count, sizeof(*out));
		if (in == NULL || out == NULL)
			shuffle = 0;
	} else {
		in = in_fixed;
		out = out_fixed;
	}

	if (shuffle) {
		/*
		 * First we get handles to all of the rdata.
		 */
		i = 0;
		do {
			INSIST(i < count);
			dns_rdata_init(&in[i]);
			dns_rdataset_current(rdataset, &in[i]);
			i++;
			result = dns_rdataset_next(rdataset);
		} while (result == ISC_R_SUCCESS);
		if (result != ISC_R_NOMORE)
			goto cleanup;
		INSIST(i == count);

		/*
		 * Now we shuffle.
		 */
		if (WANT_FIXED(rdataset)) {
			/*
			 * 'Fixed' order.
			 */
			INSIST(order != NULL);
			for (i = 0; i < count; i++) {
				out[i].key = (*order)(&in[i], order_arg);
				out[i].rdata = &in[i];
			}
		} else if (WANT_RANDOM(rdataset)) {
			/*
			 * 'Random' order.
			 */
			for (i = 0; i < count; i++) {
				choice = i + arc4random_uniform(count - i);
				rdata = in[i];
				in[i] = in[choice];
				in[choice] = rdata;
				if (order != NULL)
					out[i].key = (*order)(&in[i],
							      order_arg);
				else
					out[i].key = 0; /* Unused */
				out[i].rdata = &in[i];
			}
		} else {
			/*
			 * "Cyclic" order.
			 */
			uint32_t val;
			unsigned int j;

			val = rdataset->count;
			if (val == UINT32_MAX)
				val = arc4random();
			j = val % count;
			for (i = 0; i < count; i++) {
				if (order != NULL)
					out[i].key = (*order)(&in[j],
							      order_arg);
				else
					out[i].key = 0; /* Unused */
				out[i].rdata = &in[j];
				j++;
				if (j == count)
					j = 0; /* Wrap around. */
			}
		}

		/*
		 * Sorted order.
		 */
		if (order != NULL)
			qsort(out, count, sizeof(out[0]), towire_compare);
	}

	savedbuffer = *target;
	i = 0;
	added = 0;

	do {
		/*
		 * Copy out the name, type, class, ttl.
		 */

		rrbuffer = *target;
		dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);
		result = dns_name_towire(owner_name, cctx, target);
		if (result != ISC_R_SUCCESS)
			goto rollback;
		headlen = sizeof(dns_rdataclass_t) + sizeof(dns_rdatatype_t);
		if (!question)
			headlen += sizeof(dns_ttl_t)
				+ 2;  /* XXX 2 for rdata len */
		isc_buffer_availableregion(target, &r);
		if (r.length < headlen) {
			result = ISC_R_NOSPACE;
			goto rollback;
		}
		isc_buffer_putuint16(target, rdataset->type);
		isc_buffer_putuint16(target, rdataset->rdclass);
		if (!question) {
			isc_buffer_putuint32(target, rdataset->ttl);

			/*
			 * Save space for rdlen.
			 */
			rdlen = *target;
			isc_buffer_add(target, 2);

			/*
			 * Copy out the rdata
			 */
			if (shuffle)
				rdata = *(out[i].rdata);
			else {
				dns_rdata_reset(&rdata);
				dns_rdataset_current(rdataset, &rdata);
			}
			result = dns_rdata_towire(&rdata, cctx, target);
			if (result != ISC_R_SUCCESS)
				goto rollback;
			INSIST((target->used >= rdlen.used + 2) &&
			       (target->used - rdlen.used - 2 < 65536));
			isc_buffer_putuint16(&rdlen,
					     (uint16_t)(target->used -
							    rdlen.used - 2));
			added++;
		}

		if (shuffle) {
			i++;
			if (i == count)
				result = ISC_R_NOMORE;
			else
				result = ISC_R_SUCCESS;
		} else {
			result = dns_rdataset_next(rdataset);
		}
	} while (result == ISC_R_SUCCESS);

	if (result != ISC_R_NOMORE)
		goto rollback;

	*countp += count;

	result = ISC_R_SUCCESS;
	goto cleanup;

 rollback:
	if (partial && result == ISC_R_NOSPACE) {
		INSIST(rrbuffer.used < 65536);
		dns_compress_rollback(cctx, (uint16_t)rrbuffer.used);
		*countp += added;
		*target = rrbuffer;
		goto cleanup;
	}
	INSIST(savedbuffer.used < 65536);
	dns_compress_rollback(cctx, (uint16_t)savedbuffer.used);
	*countp = 0;
	*target = savedbuffer;

 cleanup:
	if (out != NULL && out != out_fixed)
		free(out);
	if (in != NULL && in != in_fixed)
		free(in);
	return (result);
}

isc_result_t
dns_rdataset_towiresorted(dns_rdataset_t *rdataset,
			  const dns_name_t *owner_name,
			  dns_compress_t *cctx,
			  isc_buffer_t *target,
			  dns_rdatasetorderfunc_t order,
			  const void *order_arg,
			  unsigned int *countp)
{
	return (towiresorted(rdataset, owner_name, cctx, target,
			     order, order_arg, 0, countp));
}

isc_result_t
dns_rdataset_towire(dns_rdataset_t *rdataset,
		    dns_name_t *owner_name,
		    dns_compress_t *cctx,
		    isc_buffer_t *target,
		    unsigned int *countp)
{
	return (towiresorted(rdataset, owner_name, cctx, target,
			     NULL, NULL, 0, countp));
}

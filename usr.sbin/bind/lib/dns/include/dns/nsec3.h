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

/* $Id: nsec3.h,v 1.5 2020/01/28 17:17:05 florian Exp $ */

#ifndef DNS_NSEC3_H
#define DNS_NSEC3_H 1

#include <isc/lang.h>
#include <isc/iterated_hash.h>



#include <dns/name.h>
#include "rdatastruct.h"
#include <dns/types.h>

#define DNS_NSEC3_SALTSIZE 255

/*
 * hash = 1, flags =1, iterations = 2, salt length = 1, salt = 255 (max)
 * hash length = 1, hash = 255 (max), bitmap = 8192 + 512 (max)
 */
#define DNS_NSEC3_BUFFERSIZE (6 + 255 + 255 + 8192 + 512)
/*
 * hash = 1, flags = 1, iterations = 2, salt length = 1, salt = 255 (max)
 */
#define DNS_NSEC3PARAM_BUFFERSIZE (5 + 255)

/*
 * Test "unknown" algorithm.  Is mapped to dns_hash_sha1.
 */
#define DNS_NSEC3_UNKNOWNALG ((dns_hash_t)245U)

ISC_LANG_BEGINDECLS

isc_boolean_t
dns_nsec3_typepresent(dns_rdata_t *nsec, dns_rdatatype_t type);
/*%<
 * Determine if a type is marked as present in an NSEC3 record.
 *
 * Requires:
 *	'nsec' points to a valid rdataset of type NSEC3
 */

isc_result_t
dns_nsec3_hashname(dns_fixedname_t *result,
		   unsigned char rethash[NSEC3_MAX_HASH_LENGTH],
		   size_t *hash_length, dns_name_t *name, dns_name_t *origin,
		   dns_hash_t hashalg, unsigned int iterations,
		   const unsigned char *salt, size_t saltlength);
/*%<
 * Make a hashed domain name from an unhashed one. If rethash is not NULL
 * the raw hash is stored there.
 */

unsigned int
dns_nsec3_hashlength(dns_hash_t hash);
/*%<
 * Return the length of the hash produced by the specified algorithm
 * or zero when unknown.
 */

isc_boolean_t
dns_nsec3_supportedhash(dns_hash_t hash);
/*%<
 * Return whether we support this hash algorithm or not.
 */


isc_result_t
dns_nsec3_active(dns_db_t *db, dns_dbversion_t *version,
		 isc_boolean_t complete, isc_boolean_t *answer);

isc_result_t
dns_nsec3_activex(dns_db_t *db, dns_dbversion_t *version,
		  isc_boolean_t complete, dns_rdatatype_t private,
		  isc_boolean_t *answer);
/*%<
 * Check if there are any complete/to be built NSEC3 chains.
 * If 'complete' is ISC_TRUE only complete chains will be recognized.
 *
 * dns_nsec3_activex() is similar to dns_nsec3_active() but 'private'
 * specifies the type of the private rdataset to be checked in addition to
 * the nsec3param rdataset at the zone apex.
 *
 * Requires:
 *	'db' to be valid.
 *	'version' to be valid or NULL.
 *	'answer' to be non NULL.
 */

isc_result_t
dns_nsec3_maxiterations(dns_db_t *db, dns_dbversion_t *version,
			unsigned int *iterationsp);
/*%<
 * Find the maximum permissible number of iterations allowed based on
 * the key strength.
 *
 * Requires:
 *	'db' to be valid.
 *	'version' to be valid or NULL.
 *	'mctx' to be valid.
 *	'iterationsp' to be non NULL.
 */

isc_boolean_t
dns_nsec3param_fromprivate(dns_rdata_t *src, dns_rdata_t *target,
			   unsigned char *buf, size_t buflen);
/*%<
 * Convert a private rdata to a nsec3param rdata.
 *
 * Return ISC_TRUE if 'src' could be successfully converted.
 *
 * 'buf' should be at least DNS_NSEC3PARAM_BUFFERSIZE in size.
 */

void
dns_nsec3param_toprivate(dns_rdata_t *src, dns_rdata_t *target,
			 dns_rdatatype_t privatetype,
			 unsigned char *buf, size_t buflen);
/*%<
 * Convert a nsec3param rdata to a private rdata.
 *
 * 'buf' should be at least src->length + 1 in size.
 */

isc_result_t
dns_nsec3param_salttotext(dns_rdata_nsec3param_t *nsec3param, char *dst,
			  size_t dstlen);
/*%<
 * Convert the salt of given NSEC3PARAM RDATA into hex-encoded, NULL-terminated
 * text stored at "dst".
 *
 * Requires:
 *
 *\li 	"dst" to have enough space (as indicated by "dstlen") to hold the
 * 	resulting text and its NULL-terminating byte.
 */

isc_result_t
dns_nsec3_noexistnodata(dns_rdatatype_t type, dns_name_t* name,
			dns_name_t *nsec3name, dns_rdataset_t *nsec3set,
			dns_name_t *zonename, isc_boolean_t *exists,
			isc_boolean_t *data, isc_boolean_t *optout,
			isc_boolean_t *unknown, isc_boolean_t *setclosest,
			isc_boolean_t *setnearest, dns_name_t *closest,
			dns_name_t *nearest, dns_nseclog_t logit, void *arg);

ISC_LANG_ENDDECLS

#endif /* DNS_NSEC3_H */

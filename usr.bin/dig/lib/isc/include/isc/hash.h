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

/* $Id: hash.h,v 1.5 2023/04/19 12:58:15 jsg Exp $ */

#ifndef ISC_HASH_H
#define ISC_HASH_H 1

#include <inttypes.h>

#include <isc/types.h>

/*****
 ***** Module Info
 *****/

/*! \file isc/hash.h
 *
 * \brief The hash API
 *	provides an unpredictable hash value for variable length data.
 *	A hash object contains a random vector (which is hidden from clients
 *	of this API) to make the actual hash value unpredictable.
 *
 *	The algorithm used in the API guarantees the probability of hash
 *	collision; in the current implementation, as long as the values stored
 *	in the random vector are unpredictable, the probability of hash
 *	collision between arbitrary two different values is at most 1/2^16.
 *
 *	Although the API is generic about the hash keys, it mainly expects
 *	DNS names (and sometimes IPv4/v6 addresses) as inputs.  It has an
 *	upper limit of the input length, and may run slow to calculate the
 *	hash values for large inputs.
 *
 *	This API is designed to be general so that it can provide multiple
 *	different hash contexts that have different random vectors.  However,
 *	it should be typical to have a single context for an entire system.
 *	To support such cases, the API also provides a single-context mode.
 *
 * \li MP:
 *	The hash object is almost read-only.  Once the internal random vector
 *	is initialized, no write operation will occur, and there will be no
 *	need to lock the object to calculate actual hash values.
 *
 * \li Reliability:
 *	In some cases this module uses low-level data copy to initialize the
 *	random vector.  Errors in this part are likely to crash the server or
 *	corrupt memory.
 *
 * \li Resources:
 *	A buffer, used as a random vector for calculating hash values.
 *
 * \li Security:
 *	This module intends to provide unpredictable hash values in
 *	adversarial environments in order to avoid denial of service attacks
 *	to hash buckets.
 *	Its unpredictability relies on the quality of entropy to build the
 *	random vector.
 *
 * \li Standards:
 *	None.
 */

/***
 *** Functions
 ***/

/*!<
 * \brief Set the contents of the random vector used in hashing.
 *
 * WARNING: This function is meant to be used only in testing code. It
 * must not be used anywhere in normally running code.
 *
 * The hash context must have been created beforehand, otherwise this
 * function is a nop.
 *
 * 'vec' is not documented here on purpose. You should know what you are
 * doing before using this function.
 */

uint32_t
isc_hash_function_reverse(const void *data, size_t length,
			  int case_sensitive,
			  const uint32_t *previous_hashp);
/*!<
 * \brief Calculate a hash over data.
 *
 * This hash function is useful for hashtables. The hash function is
 * opaque and not important to the caller. The returned hash values are
 * non-deterministic and will have different mapping every time a
 * process using this library is run, but will have uniform
 * distribution.
 *
 * isc_hash_function_reverse() calculates the hash from the
 * end to the start over the input data. The difference in order is
 * useful in incremental hashing; for example, a previously hashed
 * value for 'com' can be used as input when hashing 'example.com'.
 *
 * 'data' is the data to be hashed.
 *
 * 'length' is the size of the data to be hashed.
 *
 * 'case_sensitive' specifies whether the hash key should be treated as
 * case_sensitive values.  It should typically be 0 if the hash key
 * is a DNS name.
 *
 * 'previous_hashp' is a pointer to a previous hash value returned by
 * this function. It can be used to perform incremental hashing. NULL
 * must be passed during first calls.
 */

#endif /* ISC_HASH_H */

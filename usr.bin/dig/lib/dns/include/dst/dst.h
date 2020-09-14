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

/* $Id: dst.h,v 1.11 2020/09/14 08:40:43 florian Exp $ */

#ifndef DST_DST_H
#define DST_DST_H 1

/*! \file dst/dst.h */

#include <dns/types.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/secalg.h>
#include <dns/ds.h>

/***
 *** Types
 ***/

/*%
 * The dst_key structure is opaque.  Applications should use the accessor
 * functions provided to retrieve key attributes.  If an application needs
 * to set attributes, new accessor functions will be written.
 */

typedef struct dst_key		dst_key_t;
typedef struct dst_context 	dst_context_t;

/* DST algorithm codes */
#define DST_ALG_UNKNOWN		0
#define DST_ALG_RSAMD5		1
#define DST_ALG_RSA		DST_ALG_RSAMD5	/*%< backwards compatibility */
#define DST_ALG_HMACSHA1	161	/* XXXMPA */
#define DST_ALG_HMACSHA224	162	/* XXXMPA */
#define DST_ALG_HMACSHA256	163	/* XXXMPA */
#define DST_ALG_HMACSHA384	164	/* XXXMPA */
#define DST_ALG_HMACSHA512	165	/* XXXMPA */
#define DST_MAX_ALGS		255

/*% A buffer of this size is large enough to hold any key */
#define DST_KEY_MAXSIZE		1280

/*%
 * A buffer of this size is large enough to hold the textual representation
 * of any key
 */
#define DST_KEY_MAXTEXTSIZE	2048

/*% 'Type' for dst_read_key() */
#define DST_TYPE_KEY		0x1000000	/* KEY key */
#define DST_TYPE_PRIVATE	0x2000000
#define DST_TYPE_PUBLIC		0x4000000

/* Key timing metadata definitions */
#define DST_TIME_CREATED	0
#define DST_TIME_PUBLISH	1
#define DST_TIME_ACTIVATE	2
#define DST_TIME_REVOKE 	3
#define DST_TIME_INACTIVE	4
#define DST_TIME_DELETE 	5
#define DST_TIME_DSPUBLISH 	6
#define DST_MAX_TIMES		6

/* Numeric metadata definitions */
#define DST_NUM_PREDECESSOR	0
#define DST_NUM_SUCCESSOR	1
#define DST_NUM_MAXTTL		2
#define DST_NUM_ROLLPERIOD	3
#define DST_MAX_NUMERIC		3

/*
 * Current format version number of the private key parser.
 *
 * When parsing a key file with the same major number but a higher minor
 * number, the key parser will ignore any fields it does not recognize.
 * Thus, DST_MINOR_VERSION should be incremented whenever new
 * fields are added to the private key file (such as new metadata).
 *
 * When rewriting these keys, those fields will be dropped, and the
 * format version set back to the current one..
 *
 * When a key is seen with a higher major number, the key parser will
 * reject it as invalid.  Thus, DST_MAJOR_VERSION should be incremented
 * and DST_MINOR_VERSION set to zero whenever there is a format change
 * which is not backward compatible to previous versions of the dst_key
 * parser, such as change in the syntax of an existing field, the removal
 * of a currently mandatory field, or a new field added which would
 * alter the functioning of the key if it were absent.
 */
#define DST_MAJOR_VERSION	1
#define DST_MINOR_VERSION	3

/***
 *** Functions
 ***/

isc_result_t
dst_lib_init(void);
/*%<
 * Initializes the DST subsystem.
 *
 * Requires:
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	ISC_R_NOMEMORY
 * \li	DST_R_NOENGINE
 *
 * Ensures:
 * \li	DST is properly initialized.
 */

void
dst_lib_destroy(void);
/*%<
 * Releases all resources allocated by DST.
 */

int
dst_algorithm_supported(unsigned int alg);
/*%<
 * Checks that a given algorithm is supported by DST.
 *
 * Returns:
 * \li	1
 * \li	0
 */

isc_result_t
dst_context_create3(dst_key_t *key,
		    isc_logcategory_t *category, int useforsigning,
		    dst_context_t **dctxp);

/*%<
 * Creates a context to be used for a sign or verify operation.
 *
 * Requires:
 * \li	"key" is a valid key.
 * \li	dctxp != NULL && *dctxp == NULL
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	ISC_R_NOMEMORY
 *
 * Ensures:
 * \li	*dctxp will contain a usable context.
 */

void
dst_context_destroy(dst_context_t **dctxp);
/*%<
 * Destroys all memory associated with a context.
 *
 * Requires:
 * \li	*dctxp != NULL && *dctxp == NULL
 *
 * Ensures:
 * \li	*dctxp == NULL
 */

isc_result_t
dst_context_adddata(dst_context_t *dctx, const isc_region_t *data);
/*%<
 * Incrementally adds data to the context to be used in a sign or verify
 * operation.
 *
 * Requires:
 * \li	"dctx" is a valid context
 * \li	"data" is a valid region
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	DST_R_SIGNFAILURE
 * \li	all other errors indicate failure
 */

isc_result_t
dst_context_sign(dst_context_t *dctx, isc_buffer_t *sig);
/*%<
 * Computes a signature using the data and key stored in the context.
 *
 * Requires:
 * \li	"dctx" is a valid context.
 * \li	"sig" is a valid buffer.
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	DST_R_VERIFYFAILURE
 * \li	all other errors indicate failure
 *
 * Ensures:
 * \li	"sig" will contain the signature
 */

isc_result_t
dst_context_verify(dst_context_t *dctx, isc_region_t *sig);

isc_result_t
dst_key_todns(const dst_key_t *key, isc_buffer_t *target);
/*%<
 * Converts a DST key into a DNS KEY record.
 *
 * Requires:
 * \li	"key" is a valid key.
 * \li	"target" is a valid buffer.  There must be at least 4 bytes unused.
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	any other result indicates failure
 *
 * Ensures:
 * \li	If successful, the used pointer in 'target' is advanced by at least 4.
 */

isc_result_t
dst_key_frombuffer(unsigned int alg, unsigned int flags, unsigned int protocol,
		   isc_buffer_t *source, dst_key_t **keyp);
/*%<
 * Converts a buffer containing DNS KEY RDATA into a DST key.
 *
 * Requires:
 *\li	"name" is a valid absolute dns name.
 *\li	"alg" is a supported key algorithm.
 *\li	"source" is a valid buffer.
 *\li	"keyp" is not NULL and "*keyp" is NULL.
 *
 * Returns:
 *\li 	ISC_R_SUCCESS
 * \li	any other result indicates failure
 *
 * Ensures:
 *\li	If successful, *keyp will contain a valid key, and the consumed
 *	pointer in source will be advanced.
 */

void
dst_key_attach(dst_key_t *source, dst_key_t **target);
/*
 * Attach to a existing key increasing the reference count.
 *
 * Requires:
 *\li 'source' to be a valid key.
 *\li 'target' to be non-NULL and '*target' to be NULL.
 */

void
dst_key_free(dst_key_t **keyp);
/*%<
 * Decrement the key's reference counter and, when it reaches zero,
 * release all memory associated with the key.
 *
 * Requires:
 *\li	"keyp" is not NULL and "*keyp" is a valid key.
 *\li	reference counter greater than zero.
 *
 * Ensures:
 *\li	All memory associated with "*keyp" will be freed.
 *\li	*keyp == NULL
 */

/*%<
 * Accessor functions to obtain key fields.
 *
 * Require:
 *\li	"key" is a valid key.
 */
unsigned int
dst_key_size(const dst_key_t *key);

unsigned int
dst_key_alg(const dst_key_t *key);

isc_result_t
dst_key_sigsize(const dst_key_t *key, unsigned int *n);
/*%<
 * Computes the size of a signature generated by the given key.
 *
 * Requires:
 *\li	"key" is a valid key.
 *\li	"n" is not NULL
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	DST_R_UNSUPPORTEDALG
 *
 * Ensures:
 *\li	"n" stores the size of a generated signature
 */

uint16_t
dst_region_computeid(const isc_region_t *source, unsigned int alg);
/*%<
 * Computes the (revoked) key id of the key stored in the provided
 * region with the given algorithm.
 *
 * Requires:
 *\li	"source" contains a valid, non-NULL region.
 *
 * Returns:
 *\li 	the key id
 */

uint16_t
dst_key_getbits(const dst_key_t *key);
/*%<
 * Get the number of digest bits required (0 == MAX).
 *
 * Requires:
 *	"key" is a valid key.
 */

void
dst_key_setbits(dst_key_t *key, uint16_t bits);
/*%<
 * Set the number of digest bits required (0 == MAX).
 *
 * Requires:
 *	"key" is a valid key.
 */

#endif /* DST_DST_H */

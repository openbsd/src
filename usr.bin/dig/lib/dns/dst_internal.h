/*
 * Portions Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * Portions Copyright (C) Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dst_internal.h,v 1.11 2020/02/23 08:54:01 florian Exp $ */

#ifndef DST_DST_INTERNAL_H
#define DST_DST_INTERNAL_H 1

#include <isc/buffer.h>
#include <isc/region.h>
#include <isc/types.h>
#include <isc/refcount.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <isc/hmacsha.h>

#include <dns/time.h>
#include <dst/dst.h>

#include <openssl/err.h>
#include <openssl/objects.h>

/***
 *** Types
 ***/

typedef struct dst_func dst_func_t;

typedef struct dst_hmacsha1_key   dst_hmacsha1_key_t;
typedef struct dst_hmacsha224_key dst_hmacsha224_key_t;
typedef struct dst_hmacsha256_key dst_hmacsha256_key_t;
typedef struct dst_hmacsha384_key dst_hmacsha384_key_t;
typedef struct dst_hmacsha512_key dst_hmacsha512_key_t;

/*%
 * Indicate whether a DST context will be used for signing
 * or for verification
 */
typedef enum { DO_SIGN, DO_VERIFY } dst_use_t;

/*% DST Key Structure */
struct dst_key {
	isc_refcount_t	refs;
	unsigned int	key_size;	/*%< size of the key in bits */
	unsigned int	key_proto;	/*%< protocols this key is used for */
	unsigned int	key_alg;	/*%< algorithm of the key */
	uint32_t	key_flags;	/*%< flags of the public key */
	uint16_t	key_bits;	/*%< hmac digest bits */
	union {
		dst_hmacsha1_key_t *hmacsha1;
		dst_hmacsha224_key_t *hmacsha224;
		dst_hmacsha256_key_t *hmacsha256;
		dst_hmacsha384_key_t *hmacsha384;
		dst_hmacsha512_key_t *hmacsha512;

	} keydata;			/*%< pointer to key in crypto pkg fmt */

	dst_func_t *    func;	       /*%< crypto package specific functions */
};

struct dst_context {
	dst_use_t use;
	dst_key_t *key;
	isc_logcategory_t *category;
	union {
		isc_hmacsha1_t *hmacsha1ctx;
		isc_hmacsha224_t *hmacsha224ctx;
		isc_hmacsha256_t *hmacsha256ctx;
		isc_hmacsha384_t *hmacsha384ctx;
		isc_hmacsha512_t *hmacsha512ctx;
	} ctxdata;
};

struct dst_func {
	/*
	 * Context functions
	 */
	isc_result_t (*createctx)(dst_key_t *key, dst_context_t *dctx);
	void (*destroyctx)(dst_context_t *dctx);
	isc_result_t (*adddata)(dst_context_t *dctx, const isc_region_t *data);

	/*
	 * Key operations
	 */
	isc_result_t (*sign)(dst_context_t *dctx, isc_buffer_t *sig);
	isc_result_t (*verify)(dst_context_t *dctx, const isc_region_t *sig);
	void (*destroy)(dst_key_t *key);

	/* conversion functions */
	isc_result_t (*todns)(const dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*fromdns)(dst_key_t *key, isc_buffer_t *data);
};

/*%
 * Initializers
 */
isc_result_t dst__openssl_init(void);

isc_result_t dst__hmacsha1_init(struct dst_func **funcp);
isc_result_t dst__hmacsha224_init(struct dst_func **funcp);
isc_result_t dst__hmacsha256_init(struct dst_func **funcp);
isc_result_t dst__hmacsha384_init(struct dst_func **funcp);
isc_result_t dst__hmacsha512_init(struct dst_func **funcp);
isc_result_t dst__opensslrsa_init(struct dst_func **funcp,
				  unsigned char algorithm);
isc_result_t dst__opensslecdsa_init(struct dst_func **funcp);

/*%
 * Destructors
 */
void dst__openssl_destroy(void);

/*%
 * Memory allocators using the DST memory pool.
 */
void * dst__mem_alloc(size_t size);
void   dst__mem_free(void *ptr);
void * dst__mem_realloc(void *ptr, size_t size);

#endif /* DST_DST_INTERNAL_H */
/*! \file */

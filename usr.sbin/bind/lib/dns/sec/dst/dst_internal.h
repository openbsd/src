/*
 * Portions Copyright (C) 2000, 2001  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM AND
 * NETWORK ASSOCIATES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE CONSORTIUM OR NETWORK
 * ASSOCIATES BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: dst_internal.h,v 1.38 2001/08/28 03:58:25 marka Exp $ */

#ifndef DST_DST_INTERNAL_H
#define DST_DST_INTERNAL_H 1

#include <isc/lang.h>
#include <isc/buffer.h>
#include <isc/int.h>
#include <isc/magic.h>
#include <isc/region.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

#define KEY_MAGIC       ISC_MAGIC('D','S','T','K')
#define CTX_MAGIC       ISC_MAGIC('D','S','T','C')

#define VALID_KEY(x) ISC_MAGIC_VALID(x, KEY_MAGIC)
#define VALID_CTX(x) ISC_MAGIC_VALID(x, CTX_MAGIC)

/***
 *** Types
 ***/

typedef struct dst_func dst_func_t;

struct dst_key {
	unsigned int	magic;
	dns_name_t *	key_name;	/* name of the key */
	unsigned int	key_size;	/* size of the key in bits */
	unsigned int	key_proto;	/* protocols this key is used for */
	unsigned int	key_alg;	/* algorithm of the key */
	isc_uint32_t	key_flags;	/* flags of the public key */
	isc_uint16_t	key_id;		/* identifier of the key */
	dns_rdataclass_t key_class;	/* class of the key record */
	isc_mem_t	*mctx;		/* memory context */
	void *		opaque;		/* pointer to key in crypto pkg fmt */
	dst_func_t *	func;		/* crypto package specific functions */
};

struct dst_context {
	unsigned int magic;
	dst_key_t *key;
	isc_mem_t *mctx;
	void *opaque;
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
	isc_result_t (*computesecret)(const dst_key_t *pub,
				      const dst_key_t *priv,
				      isc_buffer_t *secret);
	isc_boolean_t (*compare)(const dst_key_t *key1, const dst_key_t *key2);
	isc_boolean_t (*paramcompare)(const dst_key_t *key1,
				      const dst_key_t *key2);
	isc_result_t (*generate)(dst_key_t *key, int parms);
	isc_boolean_t (*isprivate)(const dst_key_t *key);
	isc_boolean_t (*issymmetric)(void);
	void (*destroy)(dst_key_t *key);

	/* conversion functions */
	isc_result_t (*todns)(const dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*fromdns)(dst_key_t *key, isc_buffer_t *data);
	isc_result_t (*tofile)(const dst_key_t *key, const char *directory);
	isc_result_t (*fromfile)(dst_key_t *key, const char *filename);
};

/*
 * Initializers
 */
isc_result_t dst__openssl_init(void);

isc_result_t dst__hmacmd5_init(struct dst_func **funcp);
isc_result_t dst__opensslrsa_init(struct dst_func **funcp);
isc_result_t dst__openssldsa_init(struct dst_func **funcp);
isc_result_t dst__openssldh_init(struct dst_func **funcp);
isc_result_t dst__gssapi_init(struct dst_func **funcp);

/*
 * Destructors
 */
void dst__openssl_destroy(void);

void dst__hmacmd5_destroy(void);
void dst__opensslrsa_destroy(void);
void dst__openssldsa_destroy(void);
void dst__openssldh_destroy(void);
void dst__gssapi_destroy(void);

/*
 * Memory allocators using the DST memory pool.
 */
void * dst__mem_alloc(size_t size);
void   dst__mem_free(void *ptr);
void * dst__mem_realloc(void *ptr, size_t size);

/*
 * Entropy retriever using the DST entropy pool.
 */
isc_result_t dst__entropy_getdata(void *buf, unsigned int len,
				  isc_boolean_t pseudo);

/*
 * Generic helper functions.
 */
isc_result_t
dst__file_addsuffix(char *filename, unsigned int len,
		    const char *ofilename, const char *suffix);

ISC_LANG_ENDDECLS

#endif /* DST_DST_INTERNAL_H */

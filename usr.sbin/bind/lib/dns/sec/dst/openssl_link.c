/*
 * Portions Copyright (C) 1999-2001  Internet Software Consortium.
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

/*
 * Principal Author: Brian Wellington
 * $ISC: openssl_link.c,v 1.46 2001/07/31 03:45:04 marka Exp $
 */
#ifdef OPENSSL

#include <config.h>

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/mutexblock.h>
#include <isc/string.h>
#include <isc/thread.h>
#include <isc/util.h>

#include "dst_internal.h"

#include <openssl/rand.h>
#include <openssl/crypto.h>

#ifdef CRYPTO_LOCK_ENGINE
#include <openssl/engine.h>
#endif

static RAND_METHOD *rm = NULL;
static isc_mutex_t *locks = NULL;
static int nlocks;

#ifdef CRYPTO_LOCK_ENGINE
static ENGINE *e;
#endif


static int
entropy_get(unsigned char *buf, int num) {
	isc_result_t result;
	if (num < 0)
		return (-1);
	result = dst__entropy_getdata(buf, (unsigned int) num, ISC_FALSE);
	return (result == ISC_R_SUCCESS ? num : -1);
}

static int
entropy_getpseudo(unsigned char *buf, int num) {
	isc_result_t result;
	if (num < 0)
		return (-1);
	result = dst__entropy_getdata(buf, (unsigned int) num, ISC_TRUE);
	return (result == ISC_R_SUCCESS ? num : -1);
}

static void
entropy_add(const void *buf, int num, double entropy) {
	/*
	 * Do nothing.  The only call to this provides no useful data anyway.
	 */
	UNUSED(buf);
	UNUSED(num);
	UNUSED(entropy);
}

static void
lock_callback(int mode, int type, const char *file, int line) {
	UNUSED(file);
	UNUSED(line);
	if ((mode & CRYPTO_LOCK) != 0)
		LOCK(&locks[type]);
	else
		UNLOCK(&locks[type]);
}

static unsigned long
id_callback(void) {
	return ((unsigned long)isc_thread_self());
}

isc_result_t
dst__openssl_init() {
	isc_result_t result;

	CRYPTO_set_mem_functions(dst__mem_alloc, dst__mem_realloc,
				 dst__mem_free);
	nlocks = CRYPTO_num_locks();
	locks = dst__mem_alloc(sizeof(isc_mutex_t) * nlocks);
	if (locks == NULL)
		return (ISC_R_NOMEMORY);
	result = isc_mutexblock_init(locks, nlocks);
	if (result != ISC_R_SUCCESS)
		goto cleanup_mutexalloc;
	CRYPTO_set_locking_callback(lock_callback);
	CRYPTO_set_id_callback(id_callback);
	rm = dst__mem_alloc(sizeof(RAND_METHOD));
	if (rm == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_mutexinit;
	}
	rm->seed = NULL;
	rm->bytes = entropy_get;
	rm->cleanup = NULL;
	rm->add = entropy_add;
	rm->pseudorand = entropy_getpseudo;
	rm->status = NULL;
#ifdef CRYPTO_LOCK_ENGINE
	e = ENGINE_new();
	if (e == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_rm;
	}
	ENGINE_set_RAND(e, rm);
	RAND_set_rand_method(e);
#else
	RAND_set_rand_method(rm);
#endif
	return (ISC_R_SUCCESS);

#ifdef CRYPTO_LOCK_ENGINE
 cleanup_rm:
	dst__mem_free(rm);
#endif
 cleanup_mutexinit:
	RUNTIME_CHECK(isc_mutexblock_destroy(locks, nlocks) == ISC_R_SUCCESS);
 cleanup_mutexalloc:
	dst__mem_free(locks);
	return (result);
}

void
dst__openssl_destroy() {
	if (locks != NULL) {
		RUNTIME_CHECK(isc_mutexblock_destroy(locks, nlocks) ==
			      ISC_R_SUCCESS);
		dst__mem_free(locks);
	}
	if (rm != NULL)
		dst__mem_free(rm);
}

#endif /* OPENSSL */

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

/*
 * Principal Author: Brian Wellington
 */

#include <config.h>
#include <stdlib.h>

#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/mutexblock.h>
#include <isc/string.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <dns/log.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static isc_mutex_t *locks = NULL;
static int nlocks;
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
static void
lock_callback(int mode, int type, const char *file, int line) {
	UNUSED(file);
	UNUSED(line);
	if ((mode & CRYPTO_LOCK) != 0)
		LOCK(&locks[type]);
	else
		UNLOCK(&locks[type]);
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10000000L || defined(LIBRESSL_VERSION_NUMBER)
static unsigned long
id_callback(void) {
	return ((unsigned long)isc_thread_self());
}
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

#define FLARG
#define FILELINE
#if ISC_MEM_TRACKLINES
#define FLARG_PASS      , __FILE__, __LINE__
#else
#define FLARG_PASS
#endif

#else

#define FLARG           , const char *file, int line
#define FILELINE	, __FILE__, __LINE__
#if ISC_MEM_TRACKLINES
#define FLARG_PASS      , file, line
#else
#define FLARG_PASS
#endif

#endif

static void *
mem_alloc(size_t size FLARG) {
	INSIST(dst__memory_pool != NULL);
	return (isc__mem_allocate(dst__memory_pool, size FLARG_PASS));
}

static void
mem_free(void *ptr FLARG) {
	INSIST(dst__memory_pool != NULL);
	if (ptr != NULL)
		isc__mem_free(dst__memory_pool, ptr FLARG_PASS);
}

static void *
mem_realloc(void *ptr, size_t size FLARG) {
	INSIST(dst__memory_pool != NULL);
	return (isc__mem_reallocate(dst__memory_pool, ptr, size FLARG_PASS));
}

#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
static void
_set_thread_id(CRYPTO_THREADID *id)
{
	CRYPTO_THREADID_set_numeric(id, (unsigned long)isc_thread_self());
}
#endif

isc_result_t
dst__openssl_init(const char *engine) {
	isc_result_t result;

	UNUSED(engine);

	CRYPTO_set_mem_functions(mem_alloc, mem_realloc, mem_free);
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	nlocks = CRYPTO_num_locks();
	locks = mem_alloc(sizeof(isc_mutex_t) * nlocks FILELINE);
	if (locks == NULL)
		return (ISC_R_NOMEMORY);
	result = isc_mutexblock_init(locks, nlocks);
	if (result != ISC_R_SUCCESS)
		goto cleanup_mutexalloc;
	CRYPTO_set_locking_callback(lock_callback);
# if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
	CRYPTO_THREADID_set_callback(_set_thread_id);
# else
	CRYPTO_set_id_callback(id_callback);
# endif

	ERR_load_crypto_strings();
#endif

	return (ISC_R_SUCCESS);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
 cleanup_mutexalloc:
	mem_free(locks FILELINE);
	locks = NULL;
#endif
	return (result);
}

void
dst__openssl_destroy(void) {
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
	OPENSSL_cleanup();
#else
	/*
	 * Sequence taken from apps_shutdown() in <apps/apps.h>.
	 */
#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)
	CONF_modules_free();
#endif
	OBJ_cleanup();
	EVP_cleanup();
#if (OPENSSL_VERSION_NUMBER >= 0x00907000L)
	CRYPTO_cleanup_all_ex_data();
#endif
	ERR_clear_error();
#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_remove_thread_state(NULL);
#elif OPENSSL_VERSION_NUMBER < 0x10000000L || defined(LIBRESSL_VERSION_NUMBER)
	ERR_remove_state(0);
#endif
	ERR_free_strings();

	if (locks != NULL) {
		CRYPTO_set_locking_callback(NULL);
		DESTROYMUTEXBLOCK(locks, nlocks);
		mem_free(locks FILELINE);
		locks = NULL;
	}
#endif
}

static isc_result_t
toresult(isc_result_t fallback) {
	isc_result_t result = fallback;
	unsigned long err = ERR_get_error();
#if defined(HAVE_OPENSSL_ECDSA) && \
    defined(ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED)
	int lib = ERR_GET_LIB(err);
#endif
	int reason = ERR_GET_REASON(err);

	switch (reason) {
	/*
	 * ERR_* errors are globally unique; others
	 * are unique per sublibrary
	 */
	case ERR_R_MALLOC_FAILURE:
		result = ISC_R_NOMEMORY;
		break;
	default:
#if defined(HAVE_OPENSSL_ECDSA) && \
    defined(ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED)
		if (lib == ERR_R_ECDSA_LIB &&
		    reason == ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED) {
			result = ISC_R_NOENTROPY;
			break;
		}
#endif
		break;
	}

	return (result);
}

isc_result_t
dst__openssl_toresult(isc_result_t fallback) {
	isc_result_t result;

	result = toresult(fallback);

	ERR_clear_error();
	return (result);
}

isc_result_t
dst__openssl_toresult2(const char *funcname, isc_result_t fallback) {
	return (dst__openssl_toresult3(DNS_LOGCATEGORY_GENERAL,
				       funcname, fallback));
}

isc_result_t
dst__openssl_toresult3(isc_logcategory_t *category,
		       const char *funcname, isc_result_t fallback) {
	isc_result_t result;
	unsigned long err;
	const char *file, *data;
	int line, flags;
	char buf[256];

	result = toresult(fallback);

	isc_log_write(dns_lctx, category,
		      DNS_LOGMODULE_CRYPTO, ISC_LOG_WARNING,
		      "%s failed (%s)", funcname,
		      isc_result_totext(result));

	if (result == ISC_R_NOMEMORY)
		goto done;

	for (;;) {
		err = ERR_get_error_line_data(&file, &line, &data, &flags);
		if (err == 0U)
			goto done;
		ERR_error_string_n(err, buf, sizeof(buf));
		isc_log_write(dns_lctx, category,
			      DNS_LOGMODULE_CRYPTO, ISC_LOG_INFO,
			      "%s:%s:%d:%s", buf, file, line,
			      (flags & ERR_TXT_STRING) ? data : "");
	}

    done:
	ERR_clear_error();
	return (result);
}

/*! \file */

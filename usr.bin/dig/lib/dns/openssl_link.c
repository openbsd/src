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

#include <isc/util.h>

#include <dns/log.h>

#include "dst_internal.h"
#include "dst_openssl.h"

isc_result_t
dst__openssl_init(void) {
	ERR_load_crypto_strings();
	return (ISC_R_SUCCESS);
}

void
dst__openssl_destroy(void) {
	/*
	 * Sequence taken from apps_shutdown() in <apps/apps.h>.
	 */
	CONF_modules_free();
	OBJ_cleanup();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_clear_error();
	ERR_remove_state(0);
	ERR_free_strings();

}

static isc_result_t
toresult(isc_result_t fallback) {
	isc_result_t result = fallback;
	unsigned long err = ERR_get_error();
	int lib = ERR_GET_LIB(err);
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
		if (lib == ERR_R_ECDSA_LIB &&
		    reason == ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED) {
			result = ISC_R_NOENTROPY;
			break;
		}
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

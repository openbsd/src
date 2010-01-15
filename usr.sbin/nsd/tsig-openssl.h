/*
 * tsig-openssl.h -- Interface to OpenSSL for TSIG support.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef _TSIG_OPENSSL_H_
#define _TSIG_OPENSSL_H_

#if defined(TSIG) && defined(HAVE_SSL)

#include "region-allocator.h"

#include <openssl/hmac.h>
#include <openssl/sha.h>

/*
 * Initialize OpenSSL support for TSIG.
 */
int tsig_openssl_init(region_type *region);

void tsig_openssl_finalize();

#endif /* defined(TSIG) && defined(HAVE_SSL) */

#endif /* _TSIG_H_ */

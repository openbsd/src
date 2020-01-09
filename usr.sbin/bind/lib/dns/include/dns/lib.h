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

/* $Id: lib.h,v 1.4 2020/01/09 14:18:29 florian Exp $ */

#ifndef DNS_LIB_H
#define DNS_LIB_H 1

/*! \file dns/lib.h */

#include <isc/types.h>
#include <isc/lang.h>

ISC_LANG_BEGINDECLS

/*%
 * Tuning: external query load in packets per seconds.
 */
extern unsigned int dns_pps;
extern isc_msgcat_t *dns_msgcat;

void
dns_lib_initmsgcat(void);
/*%<
 * Initialize the DNS library's message catalog, dns_msgcat, if it
 * has not already been initialized.
 */

isc_result_t
dns_lib_init(void);
/*%<
 * A set of initialization procedure used in the DNS library.  This function
 * is provided for an application that is not aware of the underlying ISC or
 * DNS libraries much.
 */

void
dns_lib_shutdown(void);
/*%<
 * Free temporary resources allocated in dns_lib_init().
 */

ISC_LANG_ENDDECLS

#endif /* DNS_LIB_H */

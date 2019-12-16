/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef PK11_RESULT_H
#define PK11_RESULT_H 1

/*! \file pk11/result.h */

#include <isc/lang.h>
#include <isc/resultclass.h>

/*
 * Nothing in this file truly depends on <isc/result.h>, but the
 * PK11 result codes are considered to be publicly derived from
 * the ISC result codes, so including this file buys you the ISC_R_
 * namespace too.
 */
#include <isc/result.h>		/* Contractual promise. */

#define PK11_R_INITFAILED		(ISC_RESULTCLASS_PK11 + 0)
#define PK11_R_NOPROVIDER		(ISC_RESULTCLASS_PK11 + 1)
#define PK11_R_NORANDOMSERVICE		(ISC_RESULTCLASS_PK11 + 2)
#define PK11_R_NODIGESTSERVICE		(ISC_RESULTCLASS_PK11 + 3)
#define PK11_R_NOAESSERVICE		(ISC_RESULTCLASS_PK11 + 4)

#define PK11_R_NRESULTS			5	/* Number of results */

ISC_LANG_BEGINDECLS

LIBISC_EXTERNAL_DATA extern isc_msgcat_t *pk11_msgcat;

void
pk11_initmsgcat(void);

const char *
pk11_result_totext(isc_result_t);

void
pk11_result_register(void);

ISC_LANG_ENDDECLS

#endif /* PK11_RESULT_H */

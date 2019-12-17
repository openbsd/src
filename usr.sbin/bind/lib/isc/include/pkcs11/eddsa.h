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

#ifndef _EDDSA_H_
#define _EDDSA_H_ 1

#ifndef CKK_EDDSA
#ifdef PK11_SOFTHSMV2_FLAVOR
#define CKK_EDDSA               0x00008003UL
#endif
#endif

#ifndef CKM_EDDSA_KEY_PAIR_GEN
#ifdef PK11_SOFTHSMV2_FLAVOR
#define CKM_EDDSA_KEY_PAIR_GEN         0x00009040UL
#endif
#endif

#ifndef CKM_EDDSA
#ifdef PK11_SOFTHSMV2_FLAVOR
#define CKM_EDDSA                      0x00009041UL
#endif
#endif

#endif /* _EDDSA_H_ */

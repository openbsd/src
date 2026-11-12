/*	$OpenBSD: rpki-der.h,v 1.3 2026/11/12 13:34:44 tb Exp $ */

/*
 * Copyright (c) 2026 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef RPKI_DER_H
#define RPKI_DER_H

#include <openssl/asn1.h>

#include "bytestring.h"

/* RFC 5280, 4.1.2.5.1 and 4.1.2.5.2 as well as RFC 5652, 11.3. */
#define UTCTIME_LEN		15	/* 0x17 0x0d   YYMMDDHHMMSSZ */
#define GENERALIZEDTIME_LEN	17	/* 0x18 0x0f YYYYMMDDHHMMSSZ */

int cbs_oid_value_matches_obj(CBS *, const ASN1_OBJECT *);
int cbs_oid_value_matches_nid(CBS *, int);
int cbs_is_sha256_algorithmIdentifier_value(CBS *);
int cbs_is_rfc7935_signatureAlgorithm_value(CBS *);

int cbs_get_econtent_version(CBS *, uint64_t *);
int cbs_get_asid(CBS *, uint32_t *);

#endif /* RPKI_DER_H */

/*	$OpenBSD: usm.h,v 1.3 2019/09/18 09:54:36 martijn Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
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

#include "snmp.h"

enum usm_key_level {
	USM_KEY_UNSET = 0,
	USM_KEY_PASSWORD,
	USM_KEY_MASTER,
	USM_KEY_LOCALIZED
};

struct snmp_sec *usm_init(const char *, size_t);
int usm_setauth(struct snmp_sec *, const EVP_MD *, const char *, size_t,
    enum usm_key_level);
int usm_setpriv(struct snmp_sec *, const EVP_CIPHER *, const char *, size_t,
    enum usm_key_level);
int usm_setengineid(struct snmp_sec *, char *, size_t);
int usm_setbootstime(struct snmp_sec *, uint32_t, uint32_t);

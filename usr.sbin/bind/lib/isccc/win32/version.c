/*
 * Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001  Internet Software Consortium.
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

/* $Id: version.c,v 1.3 2019/12/16 16:31:36 deraadt Exp $ */

#include <versions.h>

#include <isccc/version.h>

LIBISCCC_EXTERNAL_DATA const char isccc_version[] = VERSION;

LIBISCCC_EXTERNAL_DATA const unsigned int isccc_libinterface = LIBINTERFACE;
LIBISCCC_EXTERNAL_DATA const unsigned int isccc_librevision = LIBREVISION;
LIBISCCC_EXTERNAL_DATA const unsigned int isccc_libage = LIBAGE;

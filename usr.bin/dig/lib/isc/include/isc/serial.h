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

/* $Id: serial.h,v 1.4 2020/02/17 18:58:39 jung Exp $ */

#ifndef ISC_SERIAL_H
#define ISC_SERIAL_H 1

#include <inttypes.h>

#include <isc/boolean.h>

/*! \file isc/serial.h
 *	\brief Implement 32 bit serial space arithmetic comparison functions.
 *	Note: Undefined results are returned as ISC_FALSE.
 */

/***
 ***	Functions
 ***/

isc_boolean_t
isc_serial_gt(uint32_t a, uint32_t b);
/*%<
 *	Return true if 'a' > 'b' otherwise false.
 */

isc_boolean_t
isc_serial_ge(uint32_t a, uint32_t b);
/*%<
 *	Return true if 'a' >= 'b' otherwise false.
 */

#endif /* ISC_SERIAL_H */

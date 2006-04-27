//
// bitfield.h - extract and set bit fields
//
// Written by Eryk Vershen
//
// Bitfields are not particularly transportable between big and little
// endian machines.  Big endian machines lay out bitfields starting
// from the most significant bit of the (one, two or four byte) number,
// whereas little endian machines lay out bitfields starting from the
// least signifcant bit.
//
// These routines were written to support some bitfields in a disk
// data structure (partition map) whose original definition was on
// a big-endian machine.
//
// They only work on 32-bit values because I didn't need 16-bit support.
// The bits in the long word are numbered from 0 (least significant) to
// 31 (most significant).
//

/*
 * Copyright 1996,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __bitfield__
#define __bitfield__


//
// Defines
//


//
// Types
//


//
// Global Constants
//


//
// Global Variables
//


//
// Forward declarations
//
unsigned long bitfield_set(unsigned long *bf, int base, int length, unsigned long value);
unsigned long bitfield_get(unsigned long bf, int base, int length);

#endif /* __bitfield__ */

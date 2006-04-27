//
// bitfield.c - extract and set bit fields
//
// Written by Eryk Vershen
//
// See comments in bitfield.h
//

/*
 * Copyright 1996, 1997 by Apple Computer, Inc.
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

#include "bitfield.h"


//
// Defines
//


//
// Types
//


//
// Global Constants
//
const unsigned long masks[] = {
    0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};


//
// Global Variables
//


//
// Forward declarations
//


//
// Routines
//
unsigned long
bitfield_set(unsigned long *bf, int base, int length, unsigned long value)
{
    unsigned long t;
    unsigned long m;
    int s;

    // compute shift & mask, coerce value to correct number of bits,
    // zap the old bits and stuff the new value
    // return the masked value in case someone wants it.
    s = (base + 1) - length;
    m = masks[length];
    t = value & m;
    *bf = (*bf & ~(m << s)) | (t << s);
    return t;
}


unsigned long
bitfield_get(unsigned long bf, int base, int length)
{
    unsigned long m;
    int s;

    // compute shift & mask
    // return the correct number of bits (shifted to low end)
    s = (base + 1) - length;
    m = masks[length];
    return ((bf >> s) & m);
}

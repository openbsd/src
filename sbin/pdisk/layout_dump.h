/*
 * layout_dump.h -
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
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

#ifndef __layout_dump__
#define __layout_dump__


/*
 * Defines
 */


/*
 * Types
 */
enum {
    kEnd,
    kHex,
    kDec,
    kUns,
    kBit
};

typedef struct {
    short   byte_offset;
    short   bit_offset;
    short   bit_length;
    short   format;
    char    *name;
} layout;


/*
 * Global Constants
 */


/*
 * Global Variables
 */


/*
 * Forward declarations
 */


/*
 * Routines
 */
void dump_using_layout(void *buffer, layout *desc);
void DumpRawBuffer(unsigned char *bufferPtr, int length);

#endif /* __layout_dump__ */

/*
 * layout_dump.c -
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1996,1997 by Apple Computer, Inc.
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

// for printf()
#include <stdio.h>
// for strlen()
#include <string.h>
#include "layout_dump.h"


/*
 * Defines
 */


/*
 * Types
 */


/*
 * Global Constants
 */
unsigned char bitmasks[] = {
    0x01, 0x03, 0x07, 0x0F,
    0x1F, 0x3F, 0x7F, 0xFF
};


/*
 * Global Variables
 */


/*
 * Forward declarations
 */


/*
 * Routines
 */
void
dump_using_layout(void *buffer, layout *desc)
{
    layout *entry;
    int byte_length;
    long    value;
    int max_name;
    int i;
    
    max_name = 0;
    for (entry = desc; entry->format != kEnd; entry++) {
	value = strlen(entry->name);
	if (value > max_name) {
	    max_name = value;
	}
    }
    
    
    for (entry = desc; entry->format != kEnd; entry++) {

	if (entry->format != kBit) {
	    printf("%*s: ", max_name, entry->name);
	
	    byte_length = entry->bit_length / 8;
	    
	    if (entry->bit_offset != 0 || (entry->bit_length % 8) != 0) {
		printf("entry %d, can't handle bitfields yet.\n", entry - desc);
		continue;
	    }
	    
	    value = 0;
	    for (i = entry->byte_offset; byte_length > 0;i++) {
		value = value << 8;
		value |= ((unsigned char *)buffer)[i];
		byte_length--;
	    }
	} else {
	    if (entry->bit_offset < 0 || entry->bit_offset > 8) {
		printf("entry %d, bad bit offset (%d).\n", entry - desc, entry->bit_offset);
		continue;
	    } else if (entry->bit_length <= 0 
		    || entry->bit_length > (entry->bit_offset + 1)) {
		printf("entry %d, bad bit length (%d,%d).\n", entry - desc,
			entry->bit_offset, entry->bit_length);
		continue;
	    }
	    value = (((unsigned char *)buffer)[entry->byte_offset]
		    & bitmasks[entry->bit_offset])
		    >> ((entry->bit_offset + 1) - entry->bit_length);
	}
	
	switch (entry->format) {
	case kHex:
	    printf("0x%x\n", value);
	    break;
	case kDec:
	    byte_length = entry->bit_length / 8;
	    switch (byte_length) {
	    case 4: printf("%d\n", (signed long)value); break;
	    case 2: printf("%d\n", (signed short)value); break;
	    case 1: printf("%d\n", (signed char)value); break;
	    }
	    break;
	case kUns:
	    byte_length = entry->bit_length / 8;
	    switch (byte_length) {
	    case 4: printf("%u\n", (unsigned long)value); break;
	    case 2: printf("%u\n", (unsigned short)value); break;
	    case 1: printf("%u\n", (unsigned char)value); break;
	    }
	    break;
	case kBit:
	    if (value) {
		printf("%*s  %s\n", max_name, "", entry->name);
	    }
	    break;
	default:
	    printf("entry %d, unknown format (%d).\n", entry - desc, entry->format);
	    break;
	}
    }
}


void
DumpRawBuffer(unsigned char *bufferPtr, int length )
{
	int                     i;
	int                     lineStart;
	int                     lineLength;
	short                   c;

#define kLineSize   16
	for (lineStart = 0; lineStart < length; lineStart += lineLength) {
	    lineLength = kLineSize;
	    if (lineStart + lineLength > length)
		lineLength = length - lineStart;
	    printf("%03x %3d:", lineStart, lineStart);
	    for (i = 0; i < lineLength; i++)
		printf(" %02x", bufferPtr[lineStart + i] & 0xFF);
	    for (; i < kLineSize; i++)
		printf("   ");
	    printf("  ");
	    for (i = 0; i < lineLength; i++) {
		c = bufferPtr[lineStart + i] & 0xFF;
		if (c > ' ' && c < '~')
		    printf("%c", c);
		else {
		    printf(".");
		}
	    }
	    printf("\n");
	}
}

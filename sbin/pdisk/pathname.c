/*
 * pathname.c -
 *
 * Written by Eryk Vershen
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


// for strncmp()
#include <string.h>

#include "pathname.h"
#include "file_media.h"

/*
 * Defines
 */


/*
 * Types
 */


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

MEDIA
open_pathname_as_media(char *path, int oflag)
{
    MEDIA	m = 0;

    m = open_file_as_media(path, oflag);
    return m;
}


MEDIA_ITERATOR
first_media_kind(long *state)
{
    *state = 0;
    return next_media_kind(state);
}


MEDIA_ITERATOR
next_media_kind(long *state)
{
    MEDIA_ITERATOR result;
    long ix;

    result = 0;
    ix = *state;

    switch (ix) {
    case 0:
	result = create_file_iterator();
	ix = 1;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 1:
	ix = 2;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 2:
	ix = 3;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 3:
    default:
	break;
    }

    *state = ix;
    return result;
}



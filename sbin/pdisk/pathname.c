/*
 * pathname.c -
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

/*
 * Note that open_pathname_as_media() and get_mklinux_name() have almost
 * identical structures.  If one changes the other must also!
 */
MEDIA
open_pathname_as_media(char *path, int oflag)
{
    MEDIA	m = 0;
#if !defined(__linux__) && !defined(__unix__)
    long	id;
    long	bus;
	
    if (strncmp("/dev/", path, 5) == 0) {
	if (strncmp("/dev/scsi", path, 9) == 0) {
	    if (path[9] >= '0' && path[9] <= '7' && path[10] == 0) {
		id = path[9] - '0';
		m = open_old_scsi_as_media(id);
	    } else if (path[9] >= '0' && path[9] <= '7' && path[10] == '.'
		    && path[11] >= '0' && path[11] <= '7' && path[12] == 0) {
		id = path[11] - '0';
		bus = path[9] - '0';
		m = open_scsi_as_media(bus, id);
	    }
	} else if (strncmp("/dev/ata", path, 8) == 0
		|| strncmp("/dev/ide", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '7' && path[9] == 0) {
		bus = path[8] - '0';
		m = open_ata_as_media(bus, 0);
	    } else if (path[8] >= '0' && path[8] <= '7' && path[9] == '.'
		    && path[10] >= '0' && path[10] <= '1' && path[11] == 0) {
		id = path[10] - '0';
		bus = path[8] - '0';
		m = open_ata_as_media(bus, id);
	    }
	} else if (strncmp("/dev/sd", path, 7) == 0) {
	    if (path[7] >= 'a' && path[7] <= 'z' && path[8] == 0) {
		id = path[7] - 'a';
		m = open_mklinux_scsi_as_media(id, 0);
	    }
	} else if (strncmp("/dev/scd", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '9' && path[9] == 0) {
		id = path[8] - '0';
		m = open_mklinux_scsi_as_media(id, 1);
	    }
	} else if (strncmp("/dev/hd", path, 7) == 0) {
	    if (path[7] >= 'a' && path[7] <= 'z' && path[8] == 0) {
		id = path[7] - 'a';
		m = open_mklinux_ata_as_media(id);
	    }
	}
    } else
#endif

    {
	m = open_file_as_media(path, oflag);
    }
    return m;
}


char *
get_mklinux_name(char *path)
{
    char	*result = 0;
#if !defined(__linux__) && !defined(__unix__)
    long	id;
    long	bus;

    if (strncmp("/dev/", path, 5) == 0) {
	if (strncmp("/dev/scsi", path, 9) == 0) {
	    if (path[9] >= '0' && path[9] <= '7' && path[10] == 0) {
		/* old scsi */
		id = path[9] - '0';
		result = mklinux_old_scsi_name(id);
	    } else if (path[9] >= '0' && path[9] <= '7' && path[10] == '.'
		    && path[11] >= '0' && path[11] <= '7' && path[12] == 0) {
		/* new scsi */
		id = path[11] - '0';
		bus = path[9] - '0';
		result = mklinux_scsi_name(bus, id);
	    }
	} else if (strncmp("/dev/ata", path, 8) == 0
		|| strncmp("/dev/ide", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '7' && path[9] == 0) {
		/* ata/ide - master device */
		bus = path[8] - '0';
		result = mklinux_ata_name(bus, 0);
	    } else if (path[8] >= '0' && path[8] <= '7' && path[9] == '.'
		    && path[10] >= '0' && path[10] <= '1' && path[11] == 0) {
		/* ata/ide */
		id = path[10] - '0';
		bus = path[8] - '0';
		result = mklinux_ata_name(bus, id);
	    }
	}
    }
#endif

    return result;
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
    long index;

    result = 0;
    index = *state;

    switch (index) {
    case 0:
#if defined(__linux__) || defined(__unix__)
	result = create_file_iterator();
#endif
	index = 1;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 1:
#if !defined(__linux__) && !defined(__unix__)
	result = create_ata_iterator();
#endif
	index = 2;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 2:
#if !defined(__linux__) && !defined(__unix__)
	result = create_scsi_iterator();
#endif
	index = 3;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 3:
    default:
	break;
    }

    *state = index;
    return result;
}



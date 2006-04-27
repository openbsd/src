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

/*
 * Note that open_pathname_as_media() and get_linux_name() have almost
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
		// scsi[0-7]
		id = path[9] - '0';
		m = open_old_scsi_as_media(id);
	    } else if (path[9] >= '0' && path[9] <= '7' && path[10] == '.'
		    && path[11] >= '0' && path[11] <= '7' && path[12] == 0) {
		// scsi[0-7].[0-7]
		id = path[11] - '0';
		bus = path[9] - '0';
		m = open_scsi_as_media(bus, id);
	    }
	} else if (strncmp("/dev/ata", path, 8) == 0
		|| strncmp("/dev/ide", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '7' && path[9] == 0) {
		// ata[0-7], ide[0-7]
		bus = path[8] - '0';
		m = open_ata_as_media(bus, 0);
	    } else if (path[8] >= '0' && path[8] <= '7' && path[9] == '.'
		    && path[10] >= '0' && path[10] <= '1' && path[11] == 0) {
		// ata[0-7].[0-1], ide[0-7].[0-1]
		id = path[10] - '0';
		bus = path[8] - '0';
		m = open_ata_as_media(bus, id);
	    }
	} else if (strncmp("/dev/sd", path, 7) == 0) {
	    if (path[7] >= 'a' && path[7] <= 'z' && path[8] == 0) {
		// sd[a-z]
		id = path[7] - 'a';
		m = open_linux_scsi_as_media(id, 0);
	    } else if (path[7] >= 'a' && path[7] <= 'z' && path[8] == '.'
		    && path[9] >= 'a' && path[9] <= 'z' && path[10] == 0) {
		// sd[a-z][a-z]
		bus = path[7] - 'a';
		id = path[9] - 'a';
		id += bus * 26;
		m = open_linux_scsi_as_media(id, 0);
	    }
	} else if (strncmp("/dev/scd", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '9' && path[9] == 0) {
		// scd[0-9]
		id = path[8] - '0';
		m = open_linux_scsi_as_media(id, 1);
	    }
	} else if (strncmp("/dev/hd", path, 7) == 0) {
	    if (path[7] >= 'a' && path[7] <= 'z' && path[8] == 0) {
		// hd[a-z]
		id = path[7] - 'a';
		m = open_linux_ata_as_media(id);
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
get_linux_name(char *path)
{
    char	*result = 0;
#if !defined(__linux__) && !defined(__unix__)
    long	id;
    long	bus;

    if (strncmp("/dev/", path, 5) == 0) {
	if (strncmp("/dev/scsi", path, 9) == 0) {
	    if (path[9] >= '0' && path[9] <= '7' && path[10] == 0) {
		/* old scsi */
		// scsi[0-7]
		id = path[9] - '0';
		result = linux_old_scsi_name(id);
	    } else if (path[9] >= '0' && path[9] <= '7' && path[10] == '.'
		    && path[11] >= '0' && path[11] <= '7' && path[12] == 0) {
		/* new scsi */
		// scsi[0-7].[0-7]
		id = path[11] - '0';
		bus = path[9] - '0';
		result = linux_scsi_name(bus, id);
	    }
	} else if (strncmp("/dev/ata", path, 8) == 0
		|| strncmp("/dev/ide", path, 8) == 0) {
	    if (path[8] >= '0' && path[8] <= '7' && path[9] == 0) {
		/* ata/ide - master device */
		// ata[0-7], ide[0-7]
		bus = path[8] - '0';
		result = linux_ata_name(bus, 0);
	    } else if (path[8] >= '0' && path[8] <= '7' && path[9] == '.'
		    && path[10] >= '0' && path[10] <= '1' && path[11] == 0) {
		/* ata/ide */
		// ata[0-7].[0-1], ide[0-7].[0-1]
		id = path[10] - '0';
		bus = path[8] - '0';
		result = linux_ata_name(bus, id);
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
    long ix;

    result = 0;
    ix = *state;

    switch (ix) {
    case 0:
#if defined(__linux__) || defined(__unix__)
	result = create_file_iterator();
#endif
	ix = 1;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 1:
#if !defined(__linux__) && !defined(__unix__)
	result = create_ata_iterator();
#endif
	ix = 2;
	if (result != 0) {
		break;
	}
	/* fall through to next interface */

    case 2:
#if !defined(__linux__) && !defined(__unix__)
	result = create_scsi_iterator();
#endif
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



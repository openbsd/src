/*	$OpenBSD: version.c,v 1.2 1997/08/22 07:16:32 downsj Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 */

#include <sys/types.h>
#include <stdio.h>

#include "top.h"
#include "patchlevel.h"

char *version_string()

{
    static char version[16];

    snprintf(version, sizeof(version), "%d.%d", VERSION, PATCHLEVEL);
#ifdef BETA
    strcat(version, BETA);
#endif
    return(version);
}

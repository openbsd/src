/*	$OpenBSD: hfs_misc.h,v 1.2 2016/01/11 07:54:07 jasper Exp $	*/

//
// hfs_misc.h - hfs routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 2000 by Eryk Vershen
 */

#ifndef __hfs_misc__
#define __hfs_misc__

#include "partition_map.h"

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
char *get_HFS_name(partition_map *entry, int *kind);

#endif /* __hfs_misc__ */

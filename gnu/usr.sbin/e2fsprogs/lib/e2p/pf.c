/*
 * pf.c			- Print file attributes on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#include <stdio.h>

#include "e2p.h"

static const unsigned long flags_array[] = {
	EXT2_SECRM_FL,
	EXT2_UNRM_FL,
	EXT2_COMPR_FL,
	EXT2_SYNC_FL,
#ifdef	EXT2_IMMUTABLE_FL
	EXT2_IMMUTABLE_FL,
#endif
#ifdef	EXT2_APPEND_FL
	EXT2_APPEND_FL,
#endif
#ifdef	EXT2_NODUMP_FL
	EXT2_NODUMP_FL,
#endif
#ifdef	EXT2_NOATIME_FL
	EXT2_NOATIME_FL,
#endif
	0};

static const char * short_flags[] = {
	"s",
	"u",
	"c",
	"S",
#ifdef	EXT2_IMMUTABLE_FL
	"i",
#endif
#ifdef	EXT2_APPEND_FL
	"a",
#endif
#ifdef	EXT2_NODUMP_FL
	"d",
#endif
#ifdef	EXT2_NOATIME_FL
	"A",
#endif
	NULL};

static const char * long_flags[] = {
	"Secure_Deletion, ",
	"Undelete, ",
	"Compressed_File, ",
	"Synchronous_Updates, ",
#ifdef	EXT2_IMMUTABLE_FL
	"Immutable, ",
#endif
#ifdef	EXT2_NODUMP_FL
	"Append_Only, ",
#endif
#ifdef	EXT2_NODUMP_FL
	"No_Dump, ",
#endif
#ifdef	EXT2_NOATIME_FL
	"No_Atime, ",
#endif
	NULL};

void print_flags (FILE * f, unsigned long flags, int long_format)
{
	int i;
	const char ** flags_names;

	if (long_format)
		flags_names = long_flags;
	else
		flags_names = short_flags;

	for (i = 0; flags_array[i] != 0; i++)
	{
		if (flags & flags_array[i])
			fprintf (f, flags_names[i]);
		else
			fprintf (f, "-");
	}
}

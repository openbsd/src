/*
 * chattr.c		- Change file attributes on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 * 93/11/13	- Replace stat() calls by lstat() to avoid loops
 * 94/02/27	- Integrated in Ted's distribution
 */

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <linux/ext2_fs.h>

#include "et/com_err.h"
#include "e2p/e2p.h"

#include "../version.h"

const char * program_name = "chattr";

int add = 0;
int rem = 0;
int set = 0;
int set_version = 0;

unsigned long version;

int recursive = 0;
int verbose = 0;

unsigned long af;
unsigned long rf;
unsigned long sf;

static void volatile fatal_error (const char * fmt_string, int errcode)
{
	fprintf (stderr, fmt_string, program_name);
	exit (errcode);
}

#define usage() fatal_error ("usage: %s [-RV] [-+=AacdisSu] [-v version] files...\n", \
			     1)

static int decode_arg (int * i, int argc, char ** argv)
{
	char * p;
	char * tmp;

	switch (argv[*i][0])
	{
	case '-':
		for (p = &argv[*i][1]; *p; p++)
			switch (*p)
			{
			case 'R':
				recursive = 1;
				break;
			case 'S':
				rf |= EXT2_SYNC_FL;
				rem = 1;
				break;
			case 'V':
				verbose = 1;
				break;
#ifdef	EXT2_APPEND_FL
			case 'a':
				rf |= EXT2_APPEND_FL;
				rem = 1;
				break;
#endif
#ifdef EXT2_NOATIME_FL
			case 'A':
				rf |= EXT2_NOATIME_FL;
				rem = 1;
				break;
#endif
			case 'c':
				rf |= EXT2_COMPR_FL;
				rem = 1;
				break;
#ifdef	EXT2_NODUMP_FL
			case 'd':
				rf |= EXT2_NODUMP_FL;
				rem = 1;
				break;
#endif
#ifdef	EXT2_IMMUTABLE_FL
			case 'i':
				rf |= EXT2_IMMUTABLE_FL;
				rem = 1;
				break;
#endif
			case 's':
				rf |= EXT2_SECRM_FL;
				rem = 1;
				break;
			case 'u':
				rf |= EXT2_UNRM_FL;
				rem = 1;
				break;
			case 'v':
				(*i)++;
				if (*i >= argc)
					usage ();
				version = strtol (argv[*i], &tmp, 0);
				if (*tmp)
				{
					com_err (program_name, 0,
						 "bad version - %s\n", argv[*i]);
					usage ();
				}
				set_version = 1;
				break;
			default:
				fprintf (stderr, "%s: Unrecognized argument: %c\n",
					 program_name, *p);
				usage ();
			}
		break;
	case '+':
		add = 1;
		for (p = &argv[*i][1]; *p; p++)
			switch (*p)
			{
			case 'S':
				af |= EXT2_SYNC_FL;
				break;
#ifdef	EXT2_APPEND_FL
			case 'a':
				af |= EXT2_APPEND_FL;
				break;
#endif
#ifdef EXT2_NOATIME_FL
			case 'A':
				af |= EXT2_NOATIME_FL;
				break;
#endif
			case 'c':
				af |= EXT2_COMPR_FL;
				break;
#ifdef	EXT2_NODUMP_FL
			case 'd':
				af |= EXT2_NODUMP_FL;
				break;
#endif
#ifdef	EXT2_IMMUTABLE_FL
			case 'i':
				af |= EXT2_IMMUTABLE_FL;
				break;
#endif
			case 's':
				af |= EXT2_SECRM_FL;
				break;
			case 'u':
				af |= EXT2_UNRM_FL;
				break;
			default:
				usage ();
			}
		break;
	case '=':
		set = 1;
		for (p = &argv[*i][1]; *p; p++)
			switch (*p)
			{
			case 'S':
				sf |= EXT2_SYNC_FL;
				break;
#ifdef	EXT2_APPEND_FL
			case 'a':
				sf |= EXT2_APPEND_FL;
				break;
#endif
#ifdef EXT2_NOATIME_FL
			case 'A':
				sf |= EXT2_NOATIME_FL;
				break;
#endif
			case 'c':
				sf |= EXT2_COMPR_FL;
				break;
#ifdef	EXT2_NODUMP_FL
			case 'd':
				sf |= EXT2_NODUMP_FL;
				break;
#endif
#ifdef	EXT2_IMMUTABLE_FL
			case 'i':
				sf |= EXT2_IMMUTABLE_FL;
				break;
#endif
			case 's':
				sf |= EXT2_SECRM_FL;
				break;
			case 'u':
				sf |= EXT2_UNRM_FL;
				break;
			default:
				usage ();
			}
		break;
	default:
		return EOF;
		break;
	}
	return 1;
}

static int chattr_dir_proc (const char *, struct dirent *, void *);

static void change_attributes (const char * name)
{
	unsigned long flags;
	struct stat st;

	if (lstat (name, &st) == -1)
	{
		com_err (program_name, errno, "while stating %s", name);
		return;
	}
	if (set)
	{
		if (verbose)
		{
			printf ("Flags of %s set as ", name);
			print_flags (stdout, sf, 0);
			printf ("\n");
		}
		if (fsetflags (name, sf) == -1)
			perror (name);
	}
	else
	{
		if (fgetflags (name, &flags) == -1)
			com_err (program_name, errno,
			         "while reading flags on %s", name);
		else
		{
			if (rem)
				flags &= ~rf;
			if (add)
				flags |= af;
			if (verbose)
			{
				printf ("Flags of %s set as ", name);
				print_flags (stdout, flags, 0);
				printf ("\n");
			}
			if (fsetflags (name, flags) == -1)
				com_err (program_name, errno,
				         "while setting flags on %s", name);
		}
	}
	if (set_version)
	{
		if (verbose)
			printf ("Version of %s set as %lu\n", name, version);
		if (fsetversion (name, version) == -1)
			com_err (program_name, errno,
			         "while setting version on %s", name);
	}
	if (S_ISDIR(st.st_mode) && recursive)
		iterate_on_dir (name, chattr_dir_proc, (void *) NULL);
}

static int chattr_dir_proc (const char * dir_name, struct dirent * de, void * private)
{
	if (strcmp (de->d_name, ".") && strcmp (de->d_name, ".."))
	{
	        char *path;

		path = malloc(strlen (dir_name) + 1 + strlen (de->d_name) + 1);
		if (!path)
			fatal_error("Couldn't allocate path variable "
				    "in chattr_dir_proc", 1);
		sprintf (path, "%s/%s", dir_name, de->d_name);
		change_attributes (path);
		free(path);
	}
	return 0;
}

void main (int argc, char ** argv)
{
	int i, j;
	int end_arg = 0;

	fprintf (stderr, "chattr %s, %s for EXT2 FS %s, %s\n",
		 E2FSPROGS_VERSION, E2FSPROGS_DATE,
		 EXT2FS_VERSION, EXT2FS_DATE);
	if (argc && *argv)
		program_name = *argv;
	i = 1;
	while (i < argc && !end_arg)
	{
		if (decode_arg (&i, argc, argv) == EOF)
			end_arg = 1;
		else
			i++;
	}
	if (i >= argc)
		usage ();
	if (set && (add || rem))
	{
		fprintf (stderr, "= is incompatible with - and +\n");
		exit (1);
	}
	if (!(add || rem || set || set_version))
	{
		fprintf (stderr, "Must use '-v', =, - or +\n");
		exit (1);
	}
	for (j = i; j < argc; j++)
		change_attributes (argv[j]);
}

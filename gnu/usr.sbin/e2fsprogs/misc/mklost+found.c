/*
 * mklost+found.c	- Creates a directory lost+found on a mounted second
 *			  extended file system
 *
 * Copyright (C) 1992, 1993  Remy Card <card@masi.ibp.fr>
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */

/*
 * History:
 * 93/04/22	- Creation
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <linux/ext2_fs.h>

#include "../version.h"

#define LPF "lost+found"

void main (int argc, char ** argv)
{
	char name [EXT2_NAME_LEN];
	char path [sizeof (LPF) + 1 + 256];
	struct stat st;
	int i, j;
	int d;

	fprintf (stderr, "mklost+found %s, %s for EXT2 FS %s, %s\n",
		 E2FSPROGS_VERSION, E2FSPROGS_DATE,
		 EXT2FS_VERSION, EXT2FS_DATE);
	if (argc != 1) {
		fprintf (stderr, "Usage: mklost+found\n");
		exit(1);
	}
	if (mkdir (LPF, 0755) == -1) {
		perror ("mkdir");
		exit(1);
	}
	
	i = 0;
	memset (name, 'x', 252);
	do {
		sprintf (name + 252, "%02d", i);
		strcpy (path, LPF);
		strcat (path, "/");
		strcat (path, name);
		if ((d = creat (path, 0644)) == -1) {
			perror ("creat");
			exit (1);
		}
		i++;
		close (d);
		if (stat (LPF, &st) == -1) {
			perror ("stat");
			exit (1);
		}
	} while (st.st_size <= (EXT2_NDIR_BLOCKS - 1) * st.st_blksize);
	for (j = 0; j < i; j++) {
		sprintf (name + 252, "%02d", j);
		strcpy (path, LPF);
		strcat (path, "/");
		strcat (path, name);
		if (unlink (path) == -1) {
			perror ("unlink");
			exit (1);
		}
	}
	exit (0);
}

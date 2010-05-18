/*	$OpenBSD: disk.c,v 1.30 2010/05/18 04:41:14 dlg Exp $	*/

/*
 * Copyright (c) 1997, 2001 Tobias Weingartner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/param.h>
#include "disk.h"
#include "misc.h"

DISK_metrics *DISK_getlabelmetrics(char *name);

int
DISK_open(char *disk, int mode)
{
	int fd;
	struct stat st;

	fd = opendev(disk, mode, OPENDEV_PART, NULL);
	if (fd == -1)
		err(1, "%s", disk);
	if (fstat(fd, &st) == -1)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		err(1, "%s is not a character device or a regular file", disk);
	return (fd);
}

/* Routine to go after the disklabel for geometry
 * information.  This should work everywhere, but
 * in the land of PC, things are not always what
 * they seem.
 */
DISK_metrics *
DISK_getlabelmetrics(char *name)
{
	DISK_metrics *lm = NULL;
	struct disklabel dl;
	int fd;

	/* Get label metrics */
	if ((fd = DISK_open(name, O_RDONLY)) != -1) {
		lm = malloc(sizeof(DISK_metrics));
		if (lm == NULL)
			err(1, NULL);

		if (ioctl(fd, DIOCGPDINFO, &dl) == -1 &&
		    ioctl(fd, DIOCGDINFO, &dl) == -1) {
			warn("DIOCGDINFO");
			free(lm);
			lm = NULL;
		} else {
			lm->cylinders = dl.d_ncylinders;
			lm->heads = dl.d_ntracks;
			lm->sectors = dl.d_nsectors;
			lm->size = dl.d_secperunit;
			unit_types[SECTORS].conversion = dl.d_secsize;
		}
		close(fd);
	}

	return (lm);
}

/* This is ugly, and convoluted.  All the magic
 * for disk geo/size happens here.  Basically,
 * the real size is the one we will use in the
 * rest of the program, the label size is what we
 * got from the disklabel.  If the disklabel fails,
 * we assume we are working with a normal file,
 * and should request the user to specify the
 * geometry he/she wishes to use.
 */
int
DISK_getmetrics(disk_t *disk, DISK_metrics *user)
{

	disk->label = DISK_getlabelmetrics(disk->name);

	/* If user supplied, use that */
	if (user) {
		disk->real = user;
		return (0);
	}

	/* If we have a label, use that */
	if (disk->label) {
		disk->real = disk->label;
		return (0);
	}

	/* Can not get geometry, punt */
	disk->real = NULL;
	return (1);
}

/*
 * Print the disk geometry information. Take an optional modifier
 * to indicate the units that should be used for display.
 */
int
DISK_printmetrics(disk_t *disk, char *units)
{
	const int secsize = unit_types[SECTORS].conversion;
	double size;
	int i;

	i = unit_lookup(units);
	size = ((double)disk->real->size * secsize) / unit_types[i].conversion;
	printf("Disk: %s\t", disk->name);
	if (disk->real) {
		printf("geometry: %d/%d/%d [%.0f ",
		    disk->real->cylinders, disk->real->heads,
		    disk->real->sectors, size);
		if (i == SECTORS && secsize != DEV_BSIZE)
			printf("%d-byte ", secsize);
		printf("%s]\n", unit_types[i].lname);
	} else
		printf("geometry: <none>\n");

	return (0);
}


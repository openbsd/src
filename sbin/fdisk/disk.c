/*	$OpenBSD: disk.c,v 1.10 2000/01/08 04:51:16 deraadt Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#ifdef CPU_BIOS
#include <machine/biosvar.h>
#endif
#include "disk.h"

int
DISK_open(disk, mode)
	char *disk;
	int mode;
{
	int fd;
	struct stat st;

	fd = opendev(disk, mode, OPENDEV_PART, NULL);
	if (fd < 0)
		err(1, "%s", disk);
	if (fstat(fd, &st) < 0)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		err(1, "%s is not a character device or a regular file", disk);
	return (fd);
}

int
DISK_close(fd)
	int fd;
{

	return (close(fd));
}

/* Routine to go after the disklabel for geometry
 * information.  This should work everywhere, but
 * in the land of PC, things are not always what
 * they seem.
 */
DISK_metrics *
DISK_getlabelmetrics(name)
	char *name;
{
	DISK_metrics *lm = NULL;
	struct disklabel dl;
	int fd;

	/* Get label metrics */
	if ((fd = DISK_open(name, O_RDONLY)) >= 0) {
		lm = malloc(sizeof(DISK_metrics));

		if (ioctl(fd, DIOCGDINFO, &dl) < 0) {
			warn("DIOCGDINFO");
			free(lm);
			lm = NULL;
		} else {
			lm->cylinders = dl.d_ncylinders;
			lm->heads = dl.d_ntracks;
			lm->sectors = dl.d_nsectors;
			lm->size = dl.d_secperunit;
		}
		DISK_close(fd);
	}

	return (lm);
}

#ifdef CPU_BIOS
/*
 * Routine to go after sysctl info for BIOS
 * geometry.  This should only really work on PC
 * type machines.  There is still a problem with
 * correlating the BIOS drive to the BSD drive.
 */
DISK_metrics *
DISK_getbiosmetrics(name)
	char *name;
{
	bios_diskinfo_t di;
	DISK_metrics *bm;
	struct stat st;
	int mib[4], size, fd;
	dev_t devno;

	if ((fd = DISK_open(name, O_RDONLY)) < 0)
		return (NULL);
	fstat(fd, &st);
	DISK_close(fd);

	/* Get BIOS metrics */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_CHR2BLK;
	mib[2] = st.st_rdev;
	size = sizeof(devno);
	if (sysctl(mib, 3, &devno, &size, NULL, 0) < 0) {
		warn("sysctl(machdep.chr2blk)");
		return (NULL);
	}
	devno = MAKEBOOTDEV(major(devno), 0, 0, DISKUNIT(devno), RAW_PART);

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_BIOS;
	mib[2] = BIOS_DISKINFO;
	mib[3] = devno;
	size = sizeof(di);
	if (sysctl(mib, 4, &di, &size, NULL, 0) < 0) {
		warn("sysctl");
		return (NULL);
	}

	bm = malloc(sizeof(di));
	bm->cylinders = di.bios_cylinders;
	bm->heads = di.bios_heads;
	bm->sectors = di.bios_sectors;
	bm->size = di.bios_cylinders * di.bios_heads * di.bios_sectors;
	return (bm);
}
#else
/*
 * We are not a PC, so we do not have BIOS metrics to contend
 * with.  Return NULL to indicate so.
 */
DISK_metrics *
DISK_getbiosmetrics(name)
	char *name;
{
	return (NULL);
}
#endif

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
DISK_getmetrics(disk, user)
	disk_t *disk;
	DISK_metrics *user;
{

	disk->label = DISK_getlabelmetrics(disk->name);
	disk->bios = DISK_getbiosmetrics(disk->name);

	/* If user supplied, use that */
	if (user) {
		disk->real = user;
		return (0);
	}

	/* If we have a label, use that */
	if (!disk->real && disk->label)
		disk->real = disk->label;

	/* Can not get geometry, punt */
	if (disk->real == NULL)
		return (1);

	/* If we have a bios, use that (if label looks bogus)
	 *
	 * XXX - This needs to be fixed!!!!
	 * Currently machdep.bios.biosdev is USELESS
	 * It needs to be, at least, a BSD device.
	 * Or we need a mapping from biosdev -> BSD universe.
	 */
	if (disk->bios)
		if (disk->real->cylinders > 262144 || disk->real->heads > 256 ||
		    disk->real->sectors > 63)
			disk->real = disk->bios;

	return (0);
}

int
DISK_printmetrics(disk)
	disk_t *disk;
{

	printf("Disk: %s\t", disk->name);
	if (disk->real)
		printf("geometry: %d/%d/%d [%d sectors]\n", disk->real->cylinders,
		    disk->real->heads, disk->real->sectors, disk->real->size);
	else
		printf("geometry: <none>\n");

	return (0);
}


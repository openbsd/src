/*	$OpenBSD: disk.c,v 1.2 1997/09/29 23:33:34 mickey Exp $	*/

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
#include <sys/disklabel.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#ifdef CPU_BIOSDEV
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
	if(fd < 0) err(1, "%s", disk);
	if(fstat(fd, &st) < 0) err(1, "%s", disk);

	if(!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		err(1, "%s is not a character device or a regular file", disk);

	return(fd);
}

int
DISK_close(fd)
	int fd;
{

	return(close(fd));
}

/* Routine to go after the disklabel for geometry
 * information.  This should work everywhere, but
 * in the land of PC, things are not always what
 * they seem.
 */
DISK_metrics *
DISK_getrealmetrics(name)
	char *name;
{
	DISK_metrics *lm = NULL;
	struct disklabel dl;
	int fd;

	/* Get real metrics */
	if((fd = DISK_open(name, O_RDONLY)) >= 0){
		lm = malloc(sizeof(DISK_metrics));

		if(ioctl(fd, DIOCGDINFO, &dl) < 0){
			warn("DIOCGDINFO");
			free(lm);
			lm = NULL;
		}else{
			lm->cylinders = dl.d_ncylinders;
			lm->heads = dl.d_ntracks;
			lm->sectors = dl.d_nsectors;
			lm->size = dl.d_secperunit;
		}
		DISK_close(fd);
	}

	return(lm);
}


#ifdef CPU_BIOSDEV
/* Routine to go after sysctl info for BIOS
 * geometry.  This should only really work on PC
 * type machines.  There is still a problem with
 * correlating the BIOS drive to the BSD drive.
 *
 * XXX - Somebody fix this!
 */
DISK_metrics *
DISK_getbiosmetrics(name)
	char *name;
{
	DISK_metrics *bm = NULL;
	int biosdev, biosgeo;
	int mib[3], size;

	/* Get BIOS metrics */
	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_BIOSDEV;
	size = sizeof(int);

	if(sysctl(mib, 2, &biosdev, &size, NULL, 0) < 0){
		warn("sysctl");
		return(NULL);
	}

	mib[1] = CPU_BIOSGEOMETRY;
	size = sizeof(int);

	if(sysctl(mib, 2, &biosgeo, &size, NULL, 0) < 0){
		warn("sysctl");
		return(NULL);
	}

	bm = malloc(sizeof(DISK_metrics));
	bm->cylinders = BIOSNTRACKS(biosgeo);
	bm->heads = BIOSNHEADS(biosgeo);
	bm->sectors = BIOSNSECTS(biosgeo);
	bm->size = BIOSNTRACKS(biosgeo) * BIOSNHEADS(biosgeo)
		* BIOSNSECTS(biosgeo);

	return(bm);
}
#endif /* CPU_BIOSDEV */


/* This is ugly, and convoluted.  All the magic
 * for disk geo/size happens here.  Basically,
 * the bios size is the one we will use in the
 * rest of the program, the real size is what we
 * got from the disklabel.  If the disklabel fails,
 * we assume we are working with a normal file,
 * and should request the user to specify the
 * geometry he/she wishes to use.
 */
int
DISK_getmetrics(disk)
	disk_t *disk;
{

	disk->real = DISK_getrealmetrics(disk->name);
#ifdef CPU_BIOSDEV
	disk->bios = DISK_getbiosmetrics(disk->name);
#else
	disk->bios = disk->real;			/* We aint no stinkin PC */
#endif

	/* Can not get geometry, punt */
	if(disk->bios == NULL || disk->real == NULL)
		return(1);

	return(0);
}

int
DISK_printmetrics(disk)
	disk_t *disk;
{

	if(disk->real)
		printf("Disk GEO: %d/%d/%d [%d sectors]\n", disk->real->cylinders,
			disk->real->heads, disk->real->sectors, disk->real->size);
	else
		printf("Disk GEO: <none>\n");

	if(disk->bios)
		printf("Bios GEO: %d/%d/%d [%d sectors]\n", disk->bios->cylinders,
			disk->bios->heads, disk->bios->sectors, disk->bios->size);
	else
		printf("Bios GEO: <none>\n");

	return(0);
}


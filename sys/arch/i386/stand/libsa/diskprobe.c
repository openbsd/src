/*	$OpenBSD: diskprobe.c,v 1.5 1997/10/24 01:38:51 weingart Exp $	*/

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
 *	This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <stand/boot/bootarg.h>
#include <machine/biosvar.h>
#include <lib/libz/zlib.h>
#include "biosdev.h"
#include "libsa.h"


/* Local Prototypes */
static void disksum __P((bios_diskinfo_t*));

/* These get passed to kernel */
bios_diskinfo_t bios_diskinfo[16];

/* Probe for all BIOS disks */
void
diskprobe()
{
	u_int drive, i = 0, rv;
	struct disklabel label;
	u_int unit, type;

	printf("Probing disks:");

	/* Floppies */
	for(drive = 0; drive < 4; drive++) {
		rv = bios_getinfo(drive, &bios_diskinfo[i]);

		if(rv) break;

		printf(" fd%u", drive);

		/* Fill out best we can - (fd?) */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(2, 0, 0, drive, 0);
		i++;
	}

	/* Hard disks */
	for(drive = 0x80; drive < 0x88; drive++) {
		rv = bios_getinfo(drive, &bios_diskinfo[i]);

		if(rv) break;

		unit = drive - 0x80;
		printf(" hd%u%s", unit, (bios_diskinfo[i].bios_edd > 0?"+":""));

		/* Try to find the label, to figure out device type */
		if((bios_getdisklabel(drive, &label)) ) {
			printf("*");
			type = 0;	/* XXX let it be IDE */
		} else {
			/* Best guess */
			if (label.d_type == DTYPE_SCSI)
				type = 4;
			else
				type = 0;
		}

		/* Fill out best we can */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(type, 0, 0, unit, 0);
		disksum(&bios_diskinfo[i]);
		i++;
	}

	/* End of list */
	bios_diskinfo[i].bios_number = -1;
	addbootarg(BOOTARG_DISKINFO,
		   (i + 1) * sizeof(bios_diskinfo[0]), bios_diskinfo);

	printf("\n");
}

/* Find info on given BIOS disk */
bios_diskinfo_t *
bios_dklookup(dev)
	register int dev;
{
	register int i;

	for(i = 0; bios_diskinfo[i].bios_number != -1; i++)
		if(bios_diskinfo[i].bios_number == dev)
			return(&bios_diskinfo[i]);

	return(NULL);
}

/* Find given sum in diskinfo array */
static bios_diskinfo_t *
find_sum(sum)
	u_int32_t sum;
{
	int i;

	for(i = 0; bios_diskinfo[i].bios_number != -1; i++)
		if(bios_diskinfo[i].checksum == sum)
			return(&bios_diskinfo[i]);

	return(NULL);
}

/* Checksum given drive until different
 *
 * Use the adler32() function from libz,
 * as it is quick, small, and available.
 */
static void
disksum(bdi)
	bios_diskinfo_t *bdi;
{
	u_int32_t sum;
	int len, st;
	int hpc, spt, dev;
	char *buf;

	buf = alloca(DEV_BSIZE);
	dev = bdi->bios_number;
	hpc = bdi->bios_heads;
	spt = bdi->bios_sectors;

	/* Adler32 checksum */
	sum = adler32(0, NULL, 0);
	for(len = 0; len < 32; len++){
		int cyl, head, sect;
		bios_diskinfo_t *bd;

		btochs(len, cyl, head, sect, hpc, spt);

		st = biosd_io(F_READ, dev, cyl, head, sect, 1, buf);
		if(st) break;

		sum = adler32(sum, buf, DEV_BSIZE);

		/* Do a minimum of 8 sectors */
		if((len >= 8) && ((bd = find_sum(sum)) == NULL))
			break;
	}

	if(st) {
		bdi->checksum = 0;
		bdi->checklen = 0;
	} else {
		bdi->checksum = sum;
		bdi->checklen = len;
	}
}


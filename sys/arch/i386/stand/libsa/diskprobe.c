/*	$OpenBSD: diskprobe.c,v 1.7 1997/10/25 02:07:10 weingart Exp $	*/

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

#define MAX_CKSUMLEN 32		/* Max amount of diskblocks used in cksum */

/* Local Prototypes */
static int disksum __P((int));

/* These get passed to kernel */
bios_diskinfo_t bios_diskinfo[16];
u_int32_t bios_cksumlen;

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

		if(rv) {
#ifdef BIOS_DEBUG
			printf(" <!fd%u>", drive);
#endif
			break;
		}

		printf(" fd%u", drive);

		/* Fill out best we can - (fd?) */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(2, 0, 0, drive, 0);
		i++;
	}

#ifdef BIOS_DEBUG
	printf("/");
#endif

	/* Hard disks */
	for(drive = 0x80; drive < 0x88; drive++) {
		rv = bios_getinfo(drive, &bios_diskinfo[i]);

		if(rv) {
#ifdef BIOS_DEBUG
			printf(" <!hd%u>", drive);
#endif
			break;
		}

		unit = drive - 0x80;
		printf(" hd%u%s", unit, (bios_diskinfo[i].bios_edd > 0?"+":""));

		/* Try to find the label, to figure out device type */
		if((bios_getdisklabel(drive, &label)) ) {
			printf("*");
			type = 0;	/* XXX let it be IDE */
		} else {
			/* Best guess */
			switch (label.d_type) {
			case DTYPE_SCSI:
				type = 4;
				bios_diskinfo[i].flags |= BDI_GOODLABEL;
				break;

			case DTYPE_ESDI:
			case DTYPE_ST506:
				type = 0;
				bios_diskinfo[i].flags |= BDI_GOODLABEL;
				break;

			default:
				bios_diskinfo[i].flags |= BDI_BADLABEL;
				type = 0;	/* XXX Suggest IDE */
			}
		}

		/* Fill out best we can */
		bios_diskinfo[i].bsd_dev = MAKEBOOTDEV(type, 0, 0, unit, 0);
		i++;
	}

	/* End of list */
	bios_diskinfo[i].bios_number = -1;
	addbootarg(BOOTARG_DISKINFO,
		   (i + 1) * sizeof(bios_diskinfo[0]), bios_diskinfo);

	/* Checksumming of hard disks */
	for (i = 0; disksum(i) && i < MAX_CKSUMLEN; i++)
		;
	bios_cksumlen = i + 1;
	addbootarg(BOOTARG_CKSUMLEN, sizeof(u_int32_t), &bios_cksumlen);

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

/*
 * Checksum one more block on all harddrives
 *
 * Use the adler32() function from libz,
 * as it is quick, small, and available.
 */
static int
disksum(blk)
	int blk;
{
	bios_diskinfo_t *bdi, *bd;
	int st, reprobe = 0;
	int hpc, spt, dev;
	char *buf;
	int cyl, head, sect;

	buf = alloca(DEV_BSIZE);
	for (bdi = bios_diskinfo; bdi->bios_number != -1; bdi++) {
		/* Skip this disk if it is not a HD or has had an I/O error */
		if (!(bdi->bios_number & 0x80) || bdi->flags & BDI_INVALID)
			continue;

		dev = bdi->bios_number;
		hpc = bdi->bios_heads;
		spt = bdi->bios_sectors;

		/* Adler32 checksum */
		btochs(blk, cyl, head, sect, hpc, spt);
		st = biosd_io(F_READ, dev, cyl, head, sect, 1, buf);
		if (st) {
			bdi->flags |= BDI_INVALID;
			continue;
		}
		bdi->checksum = adler32(bdi->checksum, buf, DEV_BSIZE);

		for (bd = bios_diskinfo; bd != bdi; bd++)
			if (bdi->checksum == bd->checksum)
				reprobe = 1;
	}

	return (reprobe);
}

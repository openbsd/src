/*	$OpenBSD: diskprobe.c,v 1.15 1998/04/18 07:39:50 deraadt Exp $	*/

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

/* We want the disk type names from disklabel.h */
#define DKTYPENAMES

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <stand/boot/bootarg.h>
#include <machine/biosvar.h>
#include <lib/libz/zlib.h>
#include "disk.h"
#include "biosdev.h"
#include "libsa.h"

#define MAX_CKSUMLEN MAXBSIZE / DEV_BSIZE	/* Max # of blks to cksum */

/* Local Prototypes */
static int disksum __P((int));

/* List of disk devices we found/probed */
struct disklist_lh disklist;

/* Pointer to boot device */
struct diskinfo *bootdev_dip;

extern int debug;

/* Probe for all BIOS floppies */
static void
floppyprobe()
{
	struct diskinfo *dip;
	int i;

	/* Floppies */
	for(i = 0; i < 4; i++) {
		dip = alloc(sizeof(struct diskinfo));
		bzero(dip, sizeof(*dip));

		if(bios_getdiskinfo(i, &dip->bios_info)) {
#ifdef BIOS_DEBUG
			if (debug)
				printf(" <!fd%u>", i);
#endif
			free(dip, 0);
			break;
		}

		printf(" fd%u", i);

		/* Fill out best we can - (fd?) */
		dip->bios_info.bsd_dev = MAKEBOOTDEV(2, 0, 0, i, RAW_PART);
		if((bios_getdisklabel(&dip->bios_info, &dip->disklabel)) != 0) 
			dip->bios_info.flags |= BDI_BADLABEL;
		else
			dip->bios_info.flags |= BDI_GOODLABEL;

		/* Add to queue of disks */
		TAILQ_INSERT_TAIL(&disklist, dip, list);
	}
}


/* Probe for all BIOS floppies */
static void
hardprobe()
{
	struct diskinfo *dip;
	int i;
	u_int bsdunit, type;
	u_int scsi = 0, ide = 0;

	/* Hard disks */
	for(i = 0x80; i < 0x88; i++) {
		dip = alloc(sizeof(struct diskinfo));

		if(bios_getdiskinfo(i, &dip->bios_info)) {
#ifdef BIOS_DEBUG
			if (debug)
				printf(" <!hd%u>", i&0x7f);
#endif
			free(dip, 0);
			break;
		}

		printf(" hd%u%s", i&0x7f, (dip->bios_info.bios_edd > 0?"+":""));

		/* Try to find the label, to figure out device type */
		if((bios_getdisklabel(&dip->bios_info, &dip->disklabel)) ) {
			printf("*");
			bsdunit = ide++;
			type = 0;	/* XXX let it be IDE */
		} else {
			/* Best guess */
			switch (dip->disklabel.d_type) {
			case DTYPE_SCSI:
				type = 4;
				bsdunit = scsi++;
				dip->bios_info.flags |= BDI_GOODLABEL;
				break;

			case DTYPE_ESDI:
			case DTYPE_ST506:
				type = 0;
				bsdunit = ide++;
				dip->bios_info.flags |= BDI_GOODLABEL;
				break;

			default:
				dip->bios_info.flags |= BDI_BADLABEL;
				type = 0;	/* XXX Suggest IDE */
				bsdunit = ide++;
			}
		}

		dip->bios_info.checksum = 0; /* just in case */
		/* Fill out best we can */
		dip->bios_info.bsd_dev = MAKEBOOTDEV(type, 0, 0, bsdunit, RAW_PART);

		/* Add to queue of disks */
		TAILQ_INSERT_TAIL(&disklist, dip, list);
	}
}


/* Probe for all BIOS supported disks */
u_int32_t bios_cksumlen;
void
diskprobe()
{
	struct diskinfo *dip;
	int i;

	/* These get passed to kernel */
	bios_diskinfo_t *bios_diskinfo;

	/* Init stuff */
	printf("disk:");
	TAILQ_INIT(&disklist);

	/* Do probes */
	floppyprobe();
#ifdef BIOS_DEBUG
	if (debug)
		printf(";");
#endif
	hardprobe();

	/* Checksumming of hard disks */
	for (i = 0; disksum(i++) && i < MAX_CKSUMLEN; )
		;
	bios_cksumlen = i;

	/* Get space for passing bios_diskinfo stuff to kernel */
	for(i = 0, dip = TAILQ_FIRST(&disklist); dip; dip = TAILQ_NEXT(dip, list))
		i++;
	bios_diskinfo = alloc(++i * sizeof(bios_diskinfo_t));

	/* Copy out the bios_diskinfo stuff */
	for(i = 0, dip = TAILQ_FIRST(&disklist); dip; dip = TAILQ_NEXT(dip, list))
		bios_diskinfo[i++] = dip->bios_info;

	bios_diskinfo[i++].bios_number = -1;
	/* Register for kernel use */
	addbootarg(BOOTARG_CKSUMLEN, sizeof(u_int32_t), &bios_cksumlen);
	addbootarg(BOOTARG_DISKINFO, i * sizeof(bios_diskinfo_t), bios_diskinfo);

	printf("\n");
}


/* Find info on given BIOS disk */
struct diskinfo *
dklookup(dev)
	int dev;
{
	struct diskinfo *dip;

	for(dip = TAILQ_FIRST(&disklist); dip; dip = TAILQ_NEXT(dip, list))
		if(dip->bios_info.bios_number == dev)
			return(dip);

	return(NULL);
}

void
dump_diskinfo()
{
	struct diskinfo *dip;

	(void)fstypenames, (void)fstypesnames;

	printf("Disk\tBIOS#\tType\tCyls\tHeads\tSecs\tFlags\tChecksum\n");
	for(dip = TAILQ_FIRST(&disklist); dip; dip = TAILQ_NEXT(dip, list)){
		bios_diskinfo_t *bdi = &dip->bios_info;
		int d = bdi->bios_number;

		printf("%cd%d\t0x%x\t%s\t%d\t%d\t%d\t0x%x\t0x%x\n",
		    (d & 0x80)?'h':'f', d & 0x7F, d,
			(bdi->flags & BDI_BADLABEL)?"*none*":
				dktypenames[B_TYPE(dip->disklabel.d_type)],
		    bdi->bios_cylinders, bdi->bios_heads, bdi->bios_sectors,
		    bdi->flags, bdi->checksum);
	}
}

/* Find BIOS protion on given BIOS disk
 * XXX - Use dklookup() instead.
 */
bios_diskinfo_t *
bios_dklookup(dev)
	register int dev;
{
	struct diskinfo *dip;

	dip = dklookup(dev);
	if(dip)
		return(&dip->bios_info);

	return(NULL);
}

/*
 * Checksum one more block on all harddrives
 *
 * Use the adler32() function from libz,
 * as it is quick, small, and available.
 */
int
disksum(blk)
	int blk;
{
	struct diskinfo *dip, *dip2;
	int st, reprobe = 0;
	int hpc, spt, dev;
	char *buf;
	int cyl, head, sect;

	buf = alloca(DEV_BSIZE);
	for(dip = TAILQ_FIRST(&disklist); dip; dip = TAILQ_NEXT(dip, list)){
		bios_diskinfo_t *bdi = &dip->bios_info;

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

		for(dip2 = TAILQ_FIRST(&disklist); dip2 != dip;
				dip2 = TAILQ_NEXT(dip2, list)){
			bios_diskinfo_t *bd = &dip2->bios_info;
			if ((bd->bios_number & 0x80) &&
			    !(bd->flags & BDI_INVALID) &&
			    bdi->checksum == bd->checksum)
				reprobe = 1;
		}
	}

	return (reprobe);
}


/*	$NetBSD: disk.c,v 1.13 1995/03/12 12:09:18 mycroft Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include "boot.h"
#ifdef DO_BAD144
#include <sys/dkbad.h>
#endif DO_BAD144
#include <sys/disklabel.h>

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS		512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)

#ifdef DOSREAD
char *devs[] = {"wd", "hd", "fd", "wt", "sd", "dos", 0};
#else
char *devs[] = {"wd", "hd", "fd", "wt", "sd", 0};
#endif

#ifdef DO_BAD144
struct dkbad dkb;
int do_bad144;
int bsize;
#endif DO_BAD144

char *iodest;
struct fs *fs;
struct inode inode;
int spt, spc;
int dosdev, unit, part, maj, boff, poff, bnum, cnt;

extern struct disklabel disklabel;
extern char iobuf[];

devopen()
{
	struct dos_partition *dptr;
	struct disklabel *lp;
	int i, sector, di;
	
	di = get_diskinfo(dosdev);
	spt = SPT(di);

	/* Hack for 2.88MB floppy drives. */
	if (!(dosdev & 0x80) && (spt == 36))
		spt = 18;

	spc = spt * HEADS(di);
	if (dosdev == 2) {
		boff = 0;
		part = (spt == 15 ? 3 : 1);
	} else {
		Bread(0, iobuf);
		dptr = (struct dos_partition *)&iobuf[DOSPARTOFF];
		sector = LABELSECTOR;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_typ == DOSPTYP_OPENBSD) {
				sector = dptr->dp_start + LABELSECTOR;
				break;
			}
		if (i >= NDOSPART)
			for (i = 0; i < NDOSPART; i++, dptr++)
				if (dptr->dp_typ == DOSPTYP_386BSD) {
					sector = dptr->dp_start + LABELSECTOR;
					break;
				}
		lp = &disklabel;
		Bread(sector++, lp);
		if (lp->d_magic != DISKMAGIC) {
			printf("bad disklabel");
			return 1;
		}
		if (maj == 4 || maj == 0 || maj == 1) {
			if (lp->d_type == DTYPE_SCSI)
				maj = 4; /* use scsi as boot dev */
			else
				maj = 0; /* must be ESDI/IDE */
		}
		boff = lp->d_partitions[part].p_offset;
#ifdef DO_BAD144
		bsize = lp->d_partitions[part].p_size;
		do_bad144 = 0;
		if (lp->d_flags & D_BADSECT) {
			/* this disk uses bad144 */
			int i;
			int dkbbnum;
			struct dkbad *dkbptr;

			/* find the first readable bad144 sector */
			dkbbnum = lp->d_secperunit - lp->d_nsectors;
			if (lp->d_secsize > DEV_BSIZE)
				dkbbnum *= lp->d_secsize / DEV_BSIZE;
			do_bad144 = 0;
			for (i = 5; i; i--) {
			/* XXX: what if the "DOS sector" < 512 bytes ??? */
				Bread(dkbbnum, iobuf);
				dkbptr = (struct dkbad *)&iobuf[0];
/* XXX why is this not in <sys/dkbad.h> ??? */
#define DKBAD_MAGIC 0x4321
				if (dkbptr->bt_mbz == 0 &&
				    dkbptr->bt_flag == DKBAD_MAGIC) {
					dkb = *dkbptr;	/* structure copy */
					do_bad144 = 1;
					break;
				}
				dkbbnum += 2;
			}
			if (!do_bad144)
				printf("Bad badsect table\n");
			else
				printf("Using bad144 bad sector at %d\n",
				    dkbbnum);
		}
#endif DO_BAD144
	}
	return 0;
}

devread()
{
	int offset, sector = bnum;
	for (offset = 0; offset < cnt; offset += BPS)
		Bread(badsect(sector++), iodest + offset);
}

#define I_ADDR		((void *) 0)	/* XXX where all reads go */

/* Read ahead buffer large enough for one track on a 1440K floppy.  For
 * reading from floppies, the bootstrap has to be loaded on a 64K boundary
 * to ensure that this buffer doesn't cross a 64K DMA boundary.
 */
#define RA_SECTORS	18
static char ra_buf[RA_SECTORS * BPS];
static int ra_dev;
static int ra_end;
static int ra_first;
static int ra_sectors;
static int ra_skip;

Bread(sector, addr)
	int sector;
	void *addr;
{
	extern int ourseg;
	int dmalimit = ((((ourseg<<4)+(int)ra_buf)+65536) & ~65535)
		- ((ourseg<<4)+ (int)ra_buf);
	if (dmalimit<RA_SECTORS*BPS) {
		if (dmalimit*2<RA_SECTORS*BPS) {
			ra_sectors = (RA_SECTORS*BPS-dmalimit)/BPS;
			ra_skip = RA_SECTORS - ra_sectors;
		} else {
			ra_sectors = dmalimit/BPS;
			ra_skip = 0;
		}
	} else {
		ra_sectors = RA_SECTORS;
		ra_skip=0;
	}

	if (dosdev != ra_dev || sector < ra_first || sector >= ra_end) {
		int cyl, head, sec, nsec;

		cyl = sector / spc;
		head = (sector % spc) / spt;
		sec = sector % spt;
		nsec = spt - sec;
		if (nsec > ra_sectors)
			nsec = ra_sectors;
		twiddle();
		while (biosread(dosdev, cyl, head, sec, nsec,
				ra_buf+ra_skip*BPS)) {
			printf("Error: C:%d H:%d S:%d\n", cyl, head, sec);
			nsec = 1;
			twiddle();
		}
		ra_dev = dosdev;
		ra_first = sector;
		ra_end = sector + nsec;
	}
	bcopy(ra_buf + (sector - ra_first+ra_skip) * BPS, addr, BPS);
}

badsect(sector)
	int sector;
{
	int i;
#ifdef DO_BAD144
	if (do_bad144) {
		u_short cyl, head, sec;
		int newsec;
		struct disklabel *dl = &disklabel;

		/* XXX */
		/* from wd.c */
		/* bt_cyl = cylinder number in sorted order */
		/* bt_trksec is actually (head << 8) + sec */

		/* only remap sectors in the partition */
		if (sector < boff || sector >= boff + bsize)
			goto no_remap;

		cyl = sector / dl->d_secpercyl;
		head = (sector % dl->d_secpercyl) / dl->d_nsectors;
		sec = sector % dl->d_nsectors;
		sec += head << 8;

		/* now, look in the table for a possible bad sector */
		for (i = 0; i < 126; i++) {
			if (dkb.bt_bad[i].bt_cyl == cyl &&
			    dkb.bt_bad[i].bt_trksec == sec) {
				/* found same sector */
				goto remap;
			} else if (dkb.bt_bad[i].bt_cyl > cyl) {
				goto no_remap;
			}
		}
		goto no_remap;
	remap:
		/* otherwise find replacement sector */
		newsec = dl->d_secperunit - dl->d_nsectors - i -1;
		return newsec;
	}
#endif DO_BAD144
no_remap:
	return sector;
}

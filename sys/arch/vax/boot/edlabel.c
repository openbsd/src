/*	$NetBSD: edlabel.c,v 1.2 1996/08/02 11:22:11 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#define DKTYPENAMES

#include "sys/param.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"
#include "ufs/ffs/fs.h"

struct	disklabel dlabel;
char	bootblock[8192];

void 
showlabel()
{
	struct	disklabel *lp;
	struct	partition *pp;
	int	i, j;

	lp = &dlabel;

	printf("\n\ndisk type %d (%s),  %s: %s%s%s\n", lp->d_type, lp->d_type
	    <DKMAXTYPES?dktypenames[lp->d_type]:dktypenames[0], lp->d_typename,
	    lp->d_flags & D_REMOVABLE?" removable":"", lp->d_flags & D_ECC?
	    " ecc":"", lp->d_flags & D_BADSECT?" badsect":"");

	printf("interleave %d, rpm %d, trackskew %d, cylinderskew %d\n",
	    lp->d_interleave, lp->d_rpm, lp->d_trackskew, lp->d_cylskew);
	printf("headswitch %d, track-to-track %d, drivedata: %d %d %d %d %d\n",
	    lp->d_headswitch, lp->d_trkseek, lp->d_drivedata[0],
	    lp->d_drivedata[1], lp->d_drivedata[2], lp->d_drivedata[3],
	    lp->d_drivedata[4]);

	printf("\nbytes/sector: %d\n", lp->d_secsize);
	printf("sectors/track: %d\n", lp->d_nsectors);
	printf("tracks/cylinder: %d\n", lp->d_ntracks);
	printf("sectors/cylinder: %d\n", lp->d_secpercyl);
	printf("cylinders: %d\n", lp->d_ncylinders);

	printf("\n%d partitions:\n", lp->d_npartitions);
	printf("     size   offset\n");
	pp = lp->d_partitions;
	for (i = 0; i < lp->d_npartitions; i++) {
		printf("%c:   %d,    %d\n", 97 + i, lp->d_partitions[i].p_size,
		    lp->d_partitions[i].p_offset);
	}
	printf("\n");
}

setdefaultlabel()
{
	printf("Sorry, not implemented yet. Later...\n\n");
}

#define GETNUM(out, num) printf(out, num);gets(store); \
	if (*store) num = atoi(store);
#define GETNUM2(out, num1, num) printf(out, num1, num);gets(store); \
	if (*store) num = atoi(store);
#define	GETSTR(out, str) printf(out, str);gets(store); \
	if (*store) bcopy(store, str, strlen(store));
#define	FLAGS(out, flag) printf(out, lp->d_flags & flag?'y':'n');gets(store); \
	if (*store == 'y' || *store == 'Y') lp->d_flags |= flag; \
	else lp->d_flags &= ~flag;

editlabel()
{
	struct	disklabel *lp;
	char	store[256];
	int	i;

	lp = &dlabel;
	printf("\nFirst set disk type. Valid types are:\n");
	for (i = 0; i < DKMAXTYPES; i++)
		printf("%d  %s\n", i, dktypenames[i]);

	GETNUM("\nNumeric disk type? [%d] ", lp->d_type);
	GETSTR("Disk name? [%s] ", lp->d_typename);
	FLAGS("badsectoring? [%c] ", D_BADSECT);
	FLAGS("ecc? [%c] ", D_ECC);
	FLAGS("removable? [%c] ", D_REMOVABLE);

	GETNUM("Interleave? [%d] ", lp->d_interleave);
	GETNUM("rpm? [%d] ", lp->d_rpm);
	GETNUM("trackskew? [%d] ", lp->d_trackskew);
	GETNUM("cylinderskew? [%d] ", lp->d_cylskew);
	GETNUM("headswitch? [%d] ", lp->d_headswitch);
	GETNUM("track-to-track? [%d] ", lp->d_trkseek);
	GETNUM("drivedata 0? [%d] ", lp->d_drivedata[0]);
	GETNUM("drivedata 1? [%d] ", lp->d_drivedata[1]);
	GETNUM("drivedata 2? [%d] ", lp->d_drivedata[2]);
	GETNUM("drivedata 3? [%d] ", lp->d_drivedata[3]);
	GETNUM("drivedata 4? [%d] ", lp->d_drivedata[4]);
	lp->d_secsize = 512;
	GETNUM("\nbytes/sector? [%d] ", lp->d_secsize);
	GETNUM("sectors/track? [%d] ", lp->d_nsectors);
	GETNUM("tracks/cylinder? [%d] ", lp->d_ntracks);
	lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	GETNUM("sectors/cylinder? [%d] ", lp->d_secpercyl);
	GETNUM("cylinders? [%d] ", lp->d_ncylinders);
	lp->d_npartitions = MAXPARTITIONS;
	for (i = 0; i < 8; i++) {
		GETNUM2("%c partition: offset? [%d] ", 97 + i,
		    lp->d_partitions[i].p_offset);
		GETNUM("             size? [%d] ", lp->d_partitions[i].p_size);
	}
}

int bootdev;

void 
Xmain()
{
	register bdev  asm("r10");

	struct	open_file file;
	char	diskname[64], *msg, *filename, indata[64];
	int	i, rsize;

	bootdev = bdev;
	printf("With this program you can modify everything in the on-disk\n");
	printf("disklabel. To do something useful you must know the exact\n");
	printf("geometry of your disk, and have ideas about how you want\n");
	printf("your partitions to be placed on disk. Some hints:\n");
	printf("The a partition should be at least ~20000 blocks, the\n");
	printf("b (swap) is depending on your use of the machine but it\n");
	printf("should almost never be less than ~32000 blocks.\n\n");
	printf("Disk geometry for most DEC disks can be found in the disktab");
	printf("\nfile, and disknames is listed in the installation notes.\n");
	printf("\nRemember that disk names is given as disk(adapt, ctrl, ");
	printf("disk, part)\nwhen using the installation tools.\n\n");

	autoconf();
igen:
	printf("Label which disk? ");
	gets(diskname);
	if (*diskname == 0) goto igen;
	if (devopen(&file, diskname, &filename)) {
		printf("cannot open %s\n", diskname);
		goto igen;
	}
	if ((*file.f_dev->dv_strategy)(file.f_devdata, F_READ,
	    (daddr_t)0, 8192, bootblock, &rsize)) {
		printf("cannot read label block\n");
		goto igen;
	}
	if (msg = (char *) getdisklabel(LABELOFFSET + bootblock, &dlabel))
		printf("%s: %s\n", diskname, msg);

	do {
		printf("(E)dit, (S)how, (D)efaults, (W)rite, (Q)uit) : ");
		gets(indata);
		switch (*indata) {
		case ('e'):
		case ('E'):
			editlabel();
			break;
		case ('s'):
		case ('S'):
			showlabel();
			break;
		case ('d'):
		case ('D'):
			setdefaultlabel();
			break;
		case ('w'):
		case ('W'):
			dlabel.d_magic = DISKMAGIC;
			dlabel.d_magic2 = DISKMAGIC;
			dlabel.d_bbsize = BBSIZE;
			dlabel.d_sbsize = SBSIZE;
			dlabel.d_checksum = 0;
			dlabel.d_checksum = dkcksum(&dlabel);
			bcopy(&dlabel, LABELOFFSET + bootblock,
			    sizeof(struct disklabel));
			if ((*file.f_dev->dv_strategy)(file.f_devdata, F_WRITE,
			    (daddr_t)0, 8192, bootblock, &rsize)) {
				printf("cannot write label sectors.\n");
				break;
			}
			printf("\nThis program does not (yet) write");
			printf(" bootblocks, only disklabel.\n");
			printf("Remember to write the bootblocks from the ");
			printf("miniroot later with the\ncommand ");
			printf("\"disklabel -B <diskname>\".\n\n");
			break;
		case ('q'):
		case ('Q'):
		default:
			break;
		}
	} while (*indata != 'q' && *indata != 'Q');
	goto igen;
}

/*	$OpenBSD: mbr.c,v 1.7 1998/09/14 03:54:35 rahnds Exp $	*/

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
#include <unistd.h>
#include <memory.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <machine/param.h>
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "part.h"

MBR_init(disk_t *disk, mbr_t *mbr)
{
	/* Fix up given mbr for this disk */
	mbr->part[0].flag = 0;
	mbr->part[1].flag = 0;
	mbr->part[2].flag = 0;

	mbr->part[3].flag = DOSACTIVE;
	mbr->signature = DOSMBR_SIGNATURE;

	/* Use whole disk, save for first head, on first cyl. */
	mbr->part[3].id = DOSPTYP_OPENBSD;
	mbr->part[3].scyl = 0;
	mbr->part[3].shead = 1;
	mbr->part[3].ssect = 1;

	/* Go right to the end */
	mbr->part[3].ecyl = disk->real->cylinders - 1;
	mbr->part[3].ehead = disk->real->heads - 1;
	mbr->part[3].esect = disk->real->sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3]);

#if defined(__powerpc__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	mbr->part[0].flag = DOSACTIVE;	/* Boot from dos part */
	mbr->part[3].flag = 0;
	mbr->part[3].ns += mbr->part[3].bs;
	mbr->part[3].bs = mbr->part[0].bs + mbr->part[0].ns;
	mbr->part[3].ns -= mbr->part[3].bs;
	PRT_fix_CHS(disk, &mbr->part[3]);
	if ((mbr->part[3].shead != 1) || (mbr->part[3].ssect != 1)) {
		/* align the parition on a cylinder boundary */
		mbr->part[3].shead = 0;
		mbr->part[3].ssect = 1;
		mbr->part[3].scyl += 1;
	}
	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3]);
#endif
}

void
MBR_parse(disk, mbr_buf, offset, reloff, mbr)
	disk_t *disk;
	char *mbr_buf;
	off_t offset;
	off_t reloff;
	mbr_t *mbr;
{
	int i;

	memcpy(mbr->code, mbr_buf, MBR_CODE_SIZE);
	mbr->offset = offset;
	mbr->reloffset = reloff;
	mbr->nt_serial = getlong(&mbr_buf[MBR_NTSER_OFF]);
	mbr->spare = getshort(&mbr_buf[MBR_SPARE_OFF]);
	mbr->signature = getshort(&mbr_buf[MBR_SIG_OFF]);

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(disk, &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i], 
		    offset, reloff,
		    &mbr->part[i]);
}

void
MBR_make(mbr, mbr_buf)
	mbr_t *mbr;
	char *mbr_buf;
{
	int i;

	memcpy(mbr_buf, mbr->code, MBR_CODE_SIZE);
	putlong(&mbr_buf[MBR_NTSER_OFF], mbr->nt_serial);
	putshort(&mbr_buf[MBR_SPARE_OFF], mbr->spare);
	putshort(&mbr_buf[MBR_SIG_OFF], mbr->signature);

	for (i = 0; i < NDOSPART; i++)
		PRT_make(&mbr->part[i], mbr->offset, mbr->reloffset, 
		    &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i]);
}

void
MBR_print(mbr)
	mbr_t *mbr;
{
	int i;

	/* Header */
	printf("Signatures: 0x%X,0x%X\n",
	    (int)mbr->signature, (int)mbr->nt_serial);
	PRT_print(0, NULL);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i]);
}

int
MBR_read(fd, where, buf)
	int fd;
	off_t where;
	char *buf;
{
	off_t off;
	int len;

	where *= DEV_BSIZE;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (off);
	len = read(fd, buf, DEV_BSIZE);
	if (len != DEV_BSIZE)
		return (len);
	return (0);
}

int
MBR_write(fd, where, buf)
	int fd;
	off_t where;
	char *buf;
{
	off_t off;
	int len;

	where *= DEV_BSIZE;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (off);
	len = write(fd, buf, DEV_BSIZE);
	if (len != DEV_BSIZE)
		return (len);
	return (0);
}

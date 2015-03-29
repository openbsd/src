/*	$OpenBSD: mbr.c,v 1.50 2015/03/29 21:16:39 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <err.h>
#include <errno.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include "disk.h"
#include "part.h"
#include "misc.h"
#include "mbr.h"

struct mbr initial_mbr;

void
MBR_init_GPT(struct mbr *mbr)
{
	/* Initialize a protective MBR for GPT. */
	bzero(&mbr->part, sizeof(mbr->part));

	/* Use whole disk, starting after MBR. */
	mbr->part[0].id = DOSPTYP_EFI;
	mbr->part[0].bs = 1;
	mbr->part[0].ns = disk.size - 1;

	/* Fix up start/length fields. */
	PRT_fix_CHS(&mbr->part[0]);
}

void
MBR_init(struct mbr *mbr)
{
	extern int g_flag;
	u_int64_t adj;
	daddr_t i;

	if (g_flag) {
		MBR_init_GPT(mbr);
		return;
	}

	/* Fix up given mbr for this disk */
	mbr->part[0].flag = 0;
	mbr->part[1].flag = 0;
	mbr->part[2].flag = 0;

	mbr->part[3].flag = DOSACTIVE;
	mbr->signature = DOSMBR_SIGNATURE;

	/* Use whole disk. Reserve first track, or first cyl, if possible. */
	mbr->part[3].id = DOSPTYP_OPENBSD;
	if (disk.heads > 1)
		mbr->part[3].shead = 1;
	else
		mbr->part[3].shead = 0;
	if (disk.heads < 2 && disk.cylinders > 1)
		mbr->part[3].scyl = 1;
	else
		mbr->part[3].scyl = 0;
	mbr->part[3].ssect = 1;

	/* Go right to the end */
	mbr->part[3].ecyl = disk.cylinders - 1;
	mbr->part[3].ehead = disk.heads - 1;
	mbr->part[3].esect = disk.sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(&mbr->part[3], 3);

#if defined(__powerpc__) || defined(__mips__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	mbr->part[0].flag = DOSACTIVE;	/* Boot from dos part */
	mbr->part[3].flag = 0;
	mbr->part[3].ns += mbr->part[3].bs;
	mbr->part[3].bs = mbr->part[0].bs + mbr->part[0].ns;
	mbr->part[3].ns -= mbr->part[3].bs;
	PRT_fix_CHS(&mbr->part[3]);
	if ((mbr->part[3].shead != 1) || (mbr->part[3].ssect != 1)) {
		/* align the partition on a cylinder boundary */
		mbr->part[3].shead = 0;
		mbr->part[3].ssect = 1;
		mbr->part[3].scyl += 1;
	}
	/* Fix up start/length fields */
	PRT_fix_BN(&mbr->part[3], 3);
#endif

	/* Start OpenBSD MBR partition on a power of 2 block number. */
	i = 1;
	while (i < DL_SECTOBLK(&dl, mbr->part[3].bs))
		i *= 2;
	adj = DL_BLKTOSEC(&dl, i) - mbr->part[3].bs;
	mbr->part[3].bs += adj;
	mbr->part[3].ns -= adj; 
	PRT_fix_CHS(&mbr->part[3]);
}

void
MBR_parse(struct dos_mbr *dos_mbr, off_t offset, off_t reloff, struct mbr *mbr)
{
	struct dos_partition dos_parts[NDOSPART];
	int i;

	memcpy(mbr->code, dos_mbr->dmbr_boot, sizeof(mbr->code));
	mbr->offset = offset;
	mbr->reloffset = reloff;
	mbr->signature = letoh16(dos_mbr->dmbr_sign);

	memcpy(dos_parts, dos_mbr->dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(&dos_parts[i], offset, reloff, &mbr->part[i]);
}

void
MBR_make(struct mbr *mbr, struct dos_mbr *dos_mbr)
{
	struct dos_partition dos_partition;
	int i;

	memcpy(dos_mbr->dmbr_boot, mbr->code, sizeof(dos_mbr->dmbr_boot));
	dos_mbr->dmbr_sign = htole16(DOSMBR_SIGNATURE);

	for (i = 0; i < NDOSPART; i++) {
		PRT_make(&mbr->part[i], mbr->offset, mbr->reloffset,
		    &dos_partition);
		memcpy(&dos_mbr->dmbr_parts[i], &dos_partition,
		    sizeof(dos_mbr->dmbr_parts[i]));
	}
}

void
MBR_print(struct mbr *mbr, char *units)
{
	int i;

	/* Header */
	printf("Signature: 0x%X\n", (int)mbr->signature);
	PRT_print(0, NULL, units);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i], units);
}

int
MBR_read(int fd, off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = readsector(fd, where);
	if (secbuf == NULL)
		return (-1);

	memcpy(dos_mbr, secbuf, sizeof(*dos_mbr));
	free(secbuf);

	return (0);
}

int
MBR_write(int fd, off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = readsector(fd, where);
	if (secbuf == NULL)
		return (-1);

	/*
	 * Place the new MBR at the start of the sector and
	 * write the sector back to "disk".
	 */
	memcpy(secbuf, dos_mbr, sizeof(*dos_mbr));
	writesector(fd, secbuf, where);
	ioctl(fd, DIOCRLDINFO, 0);

	free(secbuf);

	return (0);
}

/*
 * Parse the MBR partition table into 'mbr', leaving the rest of 'mbr'
 * untouched.
 */
void
MBR_pcopy(struct mbr *mbr)
{
	struct dos_partition dos_parts[NDOSPART];
	struct dos_mbr dos_mbr;
	int i, fd, error;

	fd = DISK_open(disk.name, O_RDONLY);
	error = MBR_read(fd, 0, &dos_mbr);
	close(fd);

	if (error == -1)
		return;

	memcpy(dos_parts, dos_mbr.dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(&dos_parts[i], 0, 0, &mbr->part[i]);
}

/*
 * If *dos_mbr has a 0xee or 0xef partition, nothing needs to happen. If no
 * such partition is present but the first or last sector on the disk has a
 * GPT, zero the GPT to ensure the MBR takes priority and fewer BIOSes get
 * confused.
 */
void
MBR_zapgpt(int fd, struct dos_mbr *dos_mbr, uint64_t lastsec)
{
	struct dos_partition dos_parts[NDOSPART];
	char *secbuf;
	uint64_t sig;
	int i;

	memcpy(dos_parts, dos_mbr->dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		if ((dos_parts[i].dp_typ == DOSPTYP_EFI) ||
		    (dos_parts[i].dp_typ == DOSPTYP_EFISYS))
			return;

	secbuf = readsector(fd, GPTSECTOR);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		writesector(fd, secbuf, GPTSECTOR);
	}
	free(secbuf);

	secbuf = readsector(fd, lastsec);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		writesector(fd, secbuf, lastsec);
	}
	free(secbuf);
}

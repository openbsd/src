/*	$OpenBSD: mbr.c,v 1.62 2015/11/18 17:09:26 krw Exp $	*/

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
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "part.h"
#include "misc.h"
#include "mbr.h"
#include "gpt.h"

struct mbr initial_mbr;

int
MBR_protective_mbr(struct mbr *mbr)
{
	u_int64_t dsize;
	int efi, found, i;
	u_int32_t psize;

	found = efi = 0;
	for (i = 0; i < NDOSPART; i++) {
		if (mbr->part[i].id == DOSPTYP_UNUSED)
			continue;
		found++;
		if (mbr->part[i].id != DOSPTYP_EFI)
			continue;
		dsize = DL_GETDSIZE(&dl);
		psize = mbr->part[i].ns;
		if (psize == (dsize - 1) || psize == UINT32_MAX) {
			if (mbr->part[i].bs == 1)
				efi++;
		}
	}
	if (found == 1 && efi == 1)
		return (0);

	return (1);
}

void
MBR_init_GPT(struct mbr *mbr)
{
	u_int64_t sz;

	sz = DL_GETDSIZE(&dl);

	memset(&mbr->part, 0, sizeof(mbr->part));

	/* Use whole disk, starting after MBR. */
	mbr->part[0].id = DOSPTYP_EFI;
	mbr->part[0].bs = 1;
	if (sz > UINT32_MAX)
		mbr->part[0].ns = UINT32_MAX;
	else
		mbr->part[0].ns = sz - 1;

	/* Fix up start/length fields. */
	PRT_fix_CHS(&mbr->part[0]);
}

void
MBR_init(struct mbr *mbr)
{
	extern u_int32_t b_arg;
	u_int64_t adj;
	daddr_t i;

	memset(&mbr->part, 0, sizeof(mbr->part));
	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));

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
#if defined(__i386__) || defined(__amd64__)
	if (b_arg > 0) {
		/* Add an EFI system partition on i386/amd64. */
		mbr->part[0].id = DOSPTYP_EFISYS;
		mbr->part[0].bs = 64;
		mbr->part[0].ns = b_arg;
		PRT_fix_CHS(&mbr->part[0]);
		mbr->part[3].ns += mbr->part[3].bs;
		mbr->part[3].bs = mbr->part[0].bs + mbr->part[0].ns;
		mbr->part[3].ns -= mbr->part[3].bs;
		PRT_fix_CHS(&mbr->part[3]);
	}
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

	DISK_printgeometry(NULL);

	/* Header */
	printf("Offset: %lld\t", (long long)mbr->offset);
	printf("Signature: 0x%X\n", (int)mbr->signature);
	PRT_print(0, NULL, units);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i], units);
}

int
MBR_read(off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = DISK_readsector(where);
	if (secbuf == NULL)
		return (-1);

	memcpy(dos_mbr, secbuf, sizeof(*dos_mbr));
	free(secbuf);

	return (0);
}

int
MBR_write(off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = DISK_readsector(where);
	if (secbuf == NULL)
		return (-1);

	/*
	 * Place the new MBR at the start of the sector and
	 * write the sector back to "disk".
	 */
	memcpy(secbuf, dos_mbr, sizeof(*dos_mbr));
	DISK_writesector(secbuf, where);

	/* Refresh in-kernel disklabel from the updated disk information. */
	ioctl(disk.fd, DIOCRLDINFO, 0);

	free(secbuf);

	return (0);
}

/*
 * If *dos_mbr has a 0xee or 0xef partition, nothing needs to happen. If no
 * such partition is present but the first or last sector on the disk has a
 * GPT, zero the GPT to ensure the MBR takes priority and fewer BIOSes get
 * confused.
 */
void
MBR_zapgpt(struct dos_mbr *dos_mbr, uint64_t lastsec)
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

	secbuf = DISK_readsector(GPTSECTOR);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		DISK_writesector(secbuf, GPTSECTOR);
	}
	free(secbuf);

	secbuf = DISK_readsector(lastsec);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		DISK_writesector(secbuf, lastsec);
	}
	free(secbuf);
}

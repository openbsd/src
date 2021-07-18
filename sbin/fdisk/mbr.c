/*	$OpenBSD: mbr.c,v 1.89 2021/07/18 21:40:13 krw Exp $	*/

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
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "part.h"
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "gpt.h"

struct mbr		initial_mbr;

static int		gpt_chk_mbr(struct dos_partition *, uint64_t);

int
MBR_protective_mbr(struct mbr *mbr)
{
	struct dos_partition	dp[NDOSPART], dos_partition;
	int			i;

	if (mbr->mbr_lba_self != 0)
		return -1;

	for (i = 0; i < NDOSPART; i++) {
		PRT_make(&mbr->mbr_prt[i], mbr->mbr_lba_self, mbr->mbr_lba_firstembr,
		    &dos_partition);
		memcpy(&dp[i], &dos_partition, sizeof(dp[i]));
	}

	return gpt_chk_mbr(dp, DL_GETDSIZE(&dl));
}

void
MBR_init_GPT(struct mbr *mbr)
{
	memset(&mbr->mbr_prt, 0, sizeof(mbr->mbr_prt));

	/* Use whole disk, starting after MBR.
	 *
	 * Always set the partition size to UINT32_MAX (as MS does). EFI
	 * firmware has been encountered that lies in unpredictable ways
	 * about the size of the disk, thus making it impossible to boot
	 * such devices.
	 */
	mbr->mbr_prt[0].prt_id = DOSPTYP_EFI;
	mbr->mbr_prt[0].prt_bs = 1;
	mbr->mbr_prt[0].prt_ns = UINT32_MAX;

	/* Fix up start/length fields. */
	PRT_fix_CHS(&mbr->mbr_prt[0]);
}

void
MBR_init(struct mbr *mbr)
{
	uint64_t		adj;
	daddr_t			daddr;

	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));

	/*
	 * XXX Do *NOT* zap all MBR parts! Some archs still read initmbr
	 * from disk!! Just mark them inactive until -b goodness spreads
	 * further.
	 */
	mbr->mbr_prt[0].prt_flag = 0;
	mbr->mbr_prt[1].prt_flag = 0;
	mbr->mbr_prt[2].prt_flag = 0;

	mbr->mbr_prt[3].prt_flag = DOSACTIVE;
	mbr->mbr_signature = DOSMBR_SIGNATURE;

	/* Use whole disk. Reserve first track, or first cyl, if possible. */
	mbr->mbr_prt[3].prt_id = DOSPTYP_OPENBSD;
	if (disk.dk_heads > 1)
		mbr->mbr_prt[3].prt_shead = 1;
	else
		mbr->mbr_prt[3].prt_shead = 0;
	if (disk.dk_heads < 2 && disk.dk_cylinders > 1)
		mbr->mbr_prt[3].prt_scyl = 1;
	else
		mbr->mbr_prt[3].prt_scyl = 0;
	mbr->mbr_prt[3].prt_ssect = 1;

	/* Go right to the end */
	mbr->mbr_prt[3].prt_ecyl = disk.dk_cylinders - 1;
	mbr->mbr_prt[3].prt_ehead = disk.dk_heads - 1;
	mbr->mbr_prt[3].prt_esect = disk.dk_sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(&mbr->mbr_prt[3], 3);

#if defined(__powerpc__) || defined(__mips__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	mbr->mbr_prt[0].prt_flag = DOSACTIVE;	/* Boot from dos part */
	mbr->mbr_prt[3].prt_flag = 0;
	mbr->mbr_prt[3].prt_ns += mbr->mbr_prt[3].prt_bs;
	mbr->mbr_prt[3].prt_bs = mbr->mbr_prt[0].prt_bs + mbr->mbr_prt[0].prt_ns;
	mbr->mbr_prt[3].prt_ns -= mbr->mbr_prt[3].prt_bs;
	PRT_fix_CHS(&mbr->mbr_prt[3]);
	if ((mbr->mbr_prt[3].prt_shead != 1) || (mbr->mbr_prt[3].prt_ssect != 1)) {
		/* align the partition on a cylinder boundary */
		mbr->mbr_prt[3].prt_shead = 0;
		mbr->mbr_prt[3].prt_ssect = 1;
		mbr->mbr_prt[3].prt_scyl += 1;
	}
	/* Fix up start/length fields */
	PRT_fix_BN(&mbr->mbr_prt[3], 3);
#else
	mbr->mbr_prt[0] = disk.dk_bootprt;
	PRT_fix_CHS(&mbr->mbr_prt[0]);
	mbr->mbr_prt[3].prt_ns += mbr->mbr_prt[3].prt_bs;
	mbr->mbr_prt[3].prt_bs = mbr->mbr_prt[0].prt_bs + mbr->mbr_prt[0].prt_ns;
	mbr->mbr_prt[3].prt_ns -= mbr->mbr_prt[3].prt_bs;
	PRT_fix_CHS(&mbr->mbr_prt[3]);
#endif

	/* Start OpenBSD MBR partition on a power of 2 block number. */
	daddr = 1;
	while (daddr < DL_SECTOBLK(&dl, mbr->mbr_prt[3].prt_bs))
		daddr *= 2;
	adj = DL_BLKTOSEC(&dl, daddr) - mbr->mbr_prt[3].prt_bs;
	mbr->mbr_prt[3].prt_bs += adj;
	mbr->mbr_prt[3].prt_ns -= adj;
	PRT_fix_CHS(&mbr->mbr_prt[3]);
}

void
MBR_parse(const struct dos_mbr *dos_mbr, const uint64_t lba_self,
    const uint64_t lba_firstembr, struct mbr *mbr)
{
	struct dos_partition	dos_parts[NDOSPART];
	int			i;

	memcpy(mbr->mbr_code, dos_mbr->dmbr_boot, sizeof(mbr->mbr_code));
	mbr->mbr_lba_self = lba_self;
	mbr->mbr_lba_firstembr = lba_firstembr;
	mbr->mbr_signature = letoh16(dos_mbr->dmbr_sign);

	memcpy(dos_parts, dos_mbr->dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(&dos_parts[i], lba_self, lba_firstembr,
		    &mbr->mbr_prt[i]);
}

void
MBR_make(const struct mbr *mbr, struct dos_mbr *dos_mbr)
{
	struct dos_partition	dos_partition;
	int			i;

	memcpy(dos_mbr->dmbr_boot, mbr->mbr_code, sizeof(dos_mbr->dmbr_boot));
	dos_mbr->dmbr_sign = htole16(DOSMBR_SIGNATURE);

	for (i = 0; i < NDOSPART; i++) {
		PRT_make(&mbr->mbr_prt[i], mbr->mbr_lba_self, mbr->mbr_lba_firstembr,
		    &dos_partition);
		memcpy(&dos_mbr->dmbr_parts[i], &dos_partition,
		    sizeof(dos_mbr->dmbr_parts[i]));
	}
}

void
MBR_print(const struct mbr *mbr, const char *units)
{
	int			i;

	DISK_printgeometry(NULL);

	printf("Offset: %lld\t", (long long)mbr->mbr_lba_self);
	printf("Signature: 0x%X\n", (int)mbr->mbr_signature);
	PRT_print(0, NULL, units);

	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->mbr_prt[i], units);
}

int
MBR_read(const uint64_t lba_self, const uint64_t lba_firstembr, struct mbr *mbr)
{
	struct dos_mbr		 dos_mbr;
	char			*secbuf;

	secbuf = DISK_readsector(lba_self);
	if (secbuf == NULL)
		return -1;

	memcpy(&dos_mbr, secbuf, sizeof(dos_mbr));
	free(secbuf);

	MBR_parse(&dos_mbr, lba_self, lba_firstembr, mbr);

	return 0;
}

int
MBR_write(const uint64_t sector, const struct dos_mbr *dos_mbr)
{
	char			*secbuf;

	secbuf = DISK_readsector(sector);
	if (secbuf == NULL)
		return -1;

	memcpy(secbuf, dos_mbr, sizeof(*dos_mbr));
	DISK_writesector(secbuf, sector);

	/* Refresh in-kernel disklabel from the updated disk information. */
	ioctl(disk.dk_fd, DIOCRLDINFO, 0);

	free(secbuf);

	return 0;
}

/*
 * Return the index into dp[] of the EFI GPT (0xEE) partition, or -1 if no such
 * partition exists.
 *
 * Taken from kern/subr_disk.c.
 *
 */
int
gpt_chk_mbr(struct dos_partition *dp, u_int64_t dsize)
{
	struct dos_partition	*dp2;
	int			 efi, eficnt, found, i;
	uint32_t		 psize;

	found = efi = eficnt = 0;
	for (dp2 = dp, i = 0; i < NDOSPART; i++, dp2++) {
		if (dp2->dp_typ == DOSPTYP_UNUSED)
			continue;
		found++;
		if (dp2->dp_typ != DOSPTYP_EFI)
			continue;
		if (letoh32(dp2->dp_start) != GPTSECTOR)
			continue;
		psize = letoh32(dp2->dp_size);
		if (psize <= (dsize - GPTSECTOR) || psize == UINT32_MAX) {
			efi = i;
			eficnt++;
		}
	}
	if (found == 1 && eficnt == 1)
		return efi;

	return -1;
}

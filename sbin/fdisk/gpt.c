/*	$OpenBSD: gpt.c,v 1.1 2015/10/05 01:39:08 krw Exp $	*/
/*
 * Copyright (c) 2015 Markus Muller <mmu@grummel.net>
 * Copyright (c) 2015 Kenneth R Westerback <krw@openbsd.org>
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

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>
#include <errno.h>

#include "disk.h"
#include "misc.h"
#include "part.h"
#include "gpt.h"

struct gpt_header gh;
struct gpt_partition gp[NGPTPARTITIONS];

int
GPT_init(void)
{
	extern u_int32_t b_arg;
	const int secsize = unit_types[SECTORS].conversion;
	struct uuid guid;
	int entries, needed;
	uint32_t status;
	const uint8_t gpt_uuid_efi_system[] = GPT_UUID_EFI_SYSTEM;
	const uint8_t gpt_uuid_openbsd[] = GPT_UUID_OPENBSD;

	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));

	entries = sizeof(gp) / GPTMINPARTSIZE;
	needed = sizeof(gp) / secsize + 2;
	/* Start on 64 sector boundary */
	if (needed % 64)
		needed += (64 - (needed % 64));

	gh.gh_sig = htole64(GPTSIGNATURE);
	gh.gh_rev = htole32(GPTREVISION);
	gh.gh_size = htole32(GPTMINHDRSIZE);
	gh.gh_csum = 0;
	gh.gh_rsvd = 0;
	gh.gh_lba_self = htole64(1);
	gh.gh_lba_alt = htole64(DL_GETDSIZE(&dl) - 1);
	gh.gh_lba_start = htole64(needed);
	gh.gh_lba_end = htole64(DL_GETDSIZE(&dl) - needed);
	gh.gh_part_lba = htole64(2);
	gh.gh_part_num = htole32(NGPTPARTITIONS);
	gh.gh_part_size = htole32(GPTMINPARTSIZE);

	uuid_create(&guid, &status);
	if (status != uuid_s_ok)
		return (1);
	uuid_enc_le(&gh.gh_guid, &guid);

#if defined(__i386__) || defined(__amd64__)
	if (b_arg > 0) {
		/* Add an EFI system partition on i386/amd64. */
		uuid_dec_be(gpt_uuid_efi_system, &guid);
		uuid_enc_le(&gp[1].gp_type, &guid);
		uuid_create(&guid, &status);
		if (status != uuid_s_ok)
			return (1);
		uuid_enc_le(&gp[1].gp_guid, &guid);
		gp[1].gp_lba_start = gh.gh_lba_start;
		gp[1].gp_lba_end = htole64(letoh64(gh.gh_lba_start)+b_arg - 1);
	}
#endif
	uuid_dec_be(gpt_uuid_openbsd, &guid);
	uuid_enc_le(&gp[3].gp_type, &guid);
	uuid_create(&guid, &status);
	if (status != uuid_s_ok)
		return (1);
	uuid_enc_le(&gp[3].gp_guid, &guid);
	gp[3].gp_lba_start = gh.gh_lba_start;
#if defined(__i386__) || defined(__amd64__)
	if (b_arg > 0) {
		gp[3].gp_lba_start = htole64(letoh64(gp[3].gp_lba_start) +
		    b_arg);
		if (letoh64(gp[3].gp_lba_start) % 64)
			gp[3].gp_lba_start =
			    htole64(letoh64(gp[3].gp_lba_start) +
			    (64 - letoh64(gp[3].gp_lba_start) % 64));
	}
#endif
	gp[3].gp_lba_end = gh.gh_lba_end;
	gh.gh_part_csum = crc32((unsigned char *)&gp, sizeof(gp));
	gh.gh_csum = crc32((unsigned char *)&gh, sizeof(gh));

	return 0;
}

int
GPT_write(int fd)
{
	char *secbuf;
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;
	u_int64_t altgh, altgp;

	ioctl(fd, DIOCRLDINFO, 0);

	/* Assume we always write full-size partition table. XXX */
	altgh = DL_GETDSIZE(&dl) - 1;
	altgp = DL_GETDSIZE(&dl) - 1 - (sizeof(gp) / secsize);

	/*
	 * Place the new GPT header at the start of sectors 1 and
	 * DL_GETDSIZE(lp)-1 and write the sectors back.
	 */
	gh.gh_lba_self = htole64(1);
	gh.gh_lba_alt = htole64(altgh);
	gh.gh_part_lba = htole64(2);
	gh.gh_part_csum = crc32((unsigned char *)&gp, sizeof(gp));
	gh.gh_csum = 0;
	gh.gh_csum = crc32((unsigned char *)&gh, letoh32(gh.gh_size));

	secbuf = DISK_readsector(fd, 1);
	if (secbuf == NULL)
		return (-1);

	memcpy(secbuf, &gh, sizeof(gh));
	DISK_writesector(fd, secbuf, 1);
	free(secbuf);

	gh.gh_lba_self = htole64(altgh);
	gh.gh_lba_alt = htole64(1);
	gh.gh_part_lba = htole64(altgp);
	gh.gh_csum = 0;
	gh.gh_csum = crc32((unsigned char *)&gh, letoh32(gh.gh_size));

	secbuf = DISK_readsector(fd, altgh);
	if (secbuf == NULL)
		return (-1);

	memcpy(secbuf, &gh, sizeof(gh));
	DISK_writesector(fd, secbuf, altgh);
	free(secbuf);

	/*
	 * Write partition table after primary header
	 * (i.e. at sector 1) and before alt header
	 * (i.e. ending in sector before alt header.
	 * XXX ALWAYS NGPTPARTITIONS!
	 * XXX ASSUME gp is multiple of sector size!
	 */
	off = lseek(fd, secsize * 2, SEEK_SET);
	if (off == secsize * 2)
		len = write(fd, &gp, sizeof(gp));
	else
		len = -1;
	if (len == -1 || len != sizeof(gp)) {
		errno = EIO;
		return (-1);
	}

	off = lseek(fd, secsize * altgp, SEEK_SET);
	if (off == secsize * altgp)
		len = write(fd, &gp, sizeof(gp));
	else
		len = -1;

	if (len == -1 || len != sizeof(gp)) {
		errno = EIO;
		return (-1);
	}

	return (0);
}

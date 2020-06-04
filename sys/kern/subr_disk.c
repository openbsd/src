/*	$OpenBSD: subr_disk.c,v 1.237 2020/05/29 04:42:25 deraadt Exp $	*/
/*	$NetBSD: subr_disk.c,v 1.17 1996/03/16 23:17:08 christos Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/disk.h>
#include <sys/reboot.h>
#include <sys/dkio.h>
#include <sys/vnode.h>
#include <sys/task.h>
#include <sys/stdint.h>

#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <dev/cons.h>

#include <lib/libz/zlib.h>

#include "softraid.h"

#ifdef DEBUG
#define DPRINTF(x...)	printf(x)
#else
#define DPRINTF(x...)
#endif

/*
 * A global list of all disks attached to the system.  May grow or
 * shrink over time.
 */
struct	disklist_head disklist;	/* TAILQ_HEAD */
int	disk_count;		/* number of drives in global disklist */
int	disk_change;		/* set if a disk has been attached/detached
				 * since last we looked at this variable. This
				 * is reset by hw_sysctl()
				 */

#define DUID_SIZE 8

u_char	bootduid[DUID_SIZE];	/* DUID of boot disk. */
u_char	rootduid[DUID_SIZE];	/* DUID of root disk. */

/* softraid callback, do not use! */
void (*softraid_disk_attach)(struct disk *, int);

void sr_map_root(void);

struct disk_attach_task {
	struct task task;
	struct disk *dk;
};

void disk_attach_callback(void *);

int spoofgptlabel(struct buf *, void (*)(struct buf *), struct disklabel *);

int gpt_chk_mbr(struct dos_partition *, u_int64_t);
int gpt_chk_hdr(struct gpt_header *, struct disklabel *);
int gpt_chk_parts(struct gpt_header *, struct gpt_partition *);
int gpt_get_fstype(struct uuid *);

int duid_equal(u_char *, u_char *);

/*
 * Compute checksum for disk label.
 */
u_int
dkcksum(struct disklabel *lp)
{
	u_int16_t *start, *end;
	u_int16_t sum = 0;

	start = (u_int16_t *)lp;
	end = (u_int16_t *)&lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

int
initdisklabel(struct disklabel *lp)
{
	int i;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (DL_GETDSIZE(lp) == 0)
		DL_SETDSIZE(lp, MAXDISKSIZE);
	if (lp->d_secpercyl == 0)
		return (ERANGE);
	lp->d_npartitions = MAXPARTITIONS;
	for (i = 0; i < RAW_PART; i++) {
		DL_SETPSIZE(&lp->d_partitions[i], 0);
		DL_SETPOFFSET(&lp->d_partitions[i], 0);
	}
	if (DL_GETPSIZE(&lp->d_partitions[RAW_PART]) == 0)
		DL_SETPSIZE(&lp->d_partitions[RAW_PART], DL_GETDSIZE(lp));
	DL_SETPOFFSET(&lp->d_partitions[RAW_PART], 0);
	DL_SETBSTART(lp, 0);
	DL_SETBEND(lp, DL_GETDSIZE(lp));
	lp->d_version = 1;
	lp->d_bbsize = 8192;
	lp->d_sbsize = 64*1024;			/* XXX ? */
	return (0);
}

/*
 * Check an incoming block to make sure it is a disklabel, convert it to
 * a newer version if needed, etc etc.
 */
int
checkdisklabel(void *rlp, struct disklabel *lp, u_int64_t boundstart,
    u_int64_t boundend)
{
	struct disklabel *dlp = rlp;
	struct __partitionv0 *v0pp;
	struct partition *pp;
	u_int64_t disksize;
	int error = 0;
	int i;

	if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC)
		error = ENOENT;	/* no disk label */
	else if (dlp->d_npartitions > MAXPARTITIONS)
		error = E2BIG;	/* too many partitions */
	else if (dlp->d_secpercyl == 0)
		error = EINVAL;	/* invalid label */
	else if (dlp->d_secsize == 0)
		error = ENOSPC;	/* disk too small */
	else if (dkcksum(dlp) != 0)
		error = EINVAL;	/* incorrect checksum */

	if (error) {
		u_int16_t *start, *end, sum = 0;

		/* If it is byte-swapped, attempt to convert it */
		if (swap32(dlp->d_magic) != DISKMAGIC ||
		    swap32(dlp->d_magic2) != DISKMAGIC ||
		    swap16(dlp->d_npartitions) > MAXPARTITIONS)
			return (error);

		/*
		 * Need a byte-swap aware dkcksum variant
		 * inlined, because dkcksum uses a sub-field
		 */
		start = (u_int16_t *)dlp;
		end = (u_int16_t *)&dlp->d_partitions[
		    swap16(dlp->d_npartitions)];
		while (start < end)
			sum ^= *start++;
		if (sum != 0)
			return (error);

		dlp->d_magic = swap32(dlp->d_magic);
		dlp->d_type = swap16(dlp->d_type);

		/* d_typename and d_packname are strings */

		dlp->d_secsize = swap32(dlp->d_secsize);
		dlp->d_nsectors = swap32(dlp->d_nsectors);
		dlp->d_ntracks = swap32(dlp->d_ntracks);
		dlp->d_ncylinders = swap32(dlp->d_ncylinders);
		dlp->d_secpercyl = swap32(dlp->d_secpercyl);
		dlp->d_secperunit = swap32(dlp->d_secperunit);

		/* d_uid is a string */

		dlp->d_acylinders = swap32(dlp->d_acylinders);

		dlp->d_flags = swap32(dlp->d_flags);

		for (i = 0; i < NDDATA; i++)
			dlp->d_drivedata[i] = swap32(dlp->d_drivedata[i]);

		dlp->d_secperunith = swap16(dlp->d_secperunith);
		dlp->d_version = swap16(dlp->d_version);

		for (i = 0; i < NSPARE; i++)
			dlp->d_spare[i] = swap32(dlp->d_spare[i]);

		dlp->d_magic2 = swap32(dlp->d_magic2);

		dlp->d_npartitions = swap16(dlp->d_npartitions);
		dlp->d_bbsize = swap32(dlp->d_bbsize);
		dlp->d_sbsize = swap32(dlp->d_sbsize);

		for (i = 0; i < MAXPARTITIONS; i++) {
			pp = &dlp->d_partitions[i];
			pp->p_size = swap32(pp->p_size);
			pp->p_offset = swap32(pp->p_offset);
			if (dlp->d_version == 0) {
				v0pp = (struct __partitionv0 *)pp;
				v0pp->p_fsize = swap32(v0pp->p_fsize);
			} else {
				pp->p_offseth = swap16(pp->p_offseth);
				pp->p_sizeh = swap16(pp->p_sizeh);
			}
			pp->p_cpg = swap16(pp->p_cpg);
		}

		dlp->d_checksum = 0;
		dlp->d_checksum = dkcksum(dlp);
		error = 0;
	}

	/* XXX should verify lots of other fields and whine a lot */

	/* Initial passed in lp contains the real disk size. */
	disksize = DL_GETDSIZE(lp);

	if (lp != dlp)
		*lp = *dlp;

	if (lp->d_version == 0) {
		lp->d_version = 1;
		lp->d_secperunith = 0;

		v0pp = (struct __partitionv0 *)lp->d_partitions;
		pp = lp->d_partitions;
		for (i = 0; i < lp->d_npartitions; i++, pp++, v0pp++) {
			pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(v0pp->
			    p_fsize, v0pp->p_frag);
			pp->p_offseth = 0;
			pp->p_sizeh = 0;
		}
	}

#ifdef DEBUG
	if (DL_GETDSIZE(lp) != disksize)
		printf("on-disk disklabel has incorrect disksize (%llu)\n",
		    DL_GETDSIZE(lp));
	if (DL_GETPSIZE(&lp->d_partitions[RAW_PART]) != disksize)
		printf("on-disk disklabel RAW_PART has incorrect size (%llu)\n",
		    DL_GETPSIZE(&lp->d_partitions[RAW_PART]));
	if (DL_GETPOFFSET(&lp->d_partitions[RAW_PART]) != 0)
		printf("on-disk disklabel RAW_PART offset != 0 (%llu)\n",
		    DL_GETPOFFSET(&lp->d_partitions[RAW_PART]));
#endif
	DL_SETDSIZE(lp, disksize);
	DL_SETPSIZE(&lp->d_partitions[RAW_PART], disksize);
	DL_SETPOFFSET(&lp->d_partitions[RAW_PART], 0);
	DL_SETBSTART(lp, boundstart);
	DL_SETBEND(lp, boundend < DL_GETDSIZE(lp) ? boundend : DL_GETDSIZE(lp));

	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	return (0);
}

/*
 * Read a disk sector.
 */
int
readdisksector(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp, u_int64_t sector)
{
	bp->b_blkno = DL_SECTOBLK(lp, sector);
	bp->b_bcount = lp->d_secsize;
	bp->b_error = 0;
	CLR(bp->b_flags, B_READ | B_WRITE | B_DONE | B_ERROR);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);

	(*strat)(bp);

	return (biowait(bp));
}

/*
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Return buffer
 * for use in signalling errors if requested.
 *
 * We would like to check if each MBR has a valid BOOT_MAGIC, but
 * we cannot because it doesn't always exist. So.. we assume the
 * MBR is valid.
 */
int
readdoslabel(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp, daddr_t *partoffp, int spoofonly)
{
	struct disklabel *gptlp;
	u_int64_t dospartoff = 0, dospartend = DL_GETBEND(lp);
	int i, ourpart = -1, wander = 1, n = 0, loop = 0, offset;
	struct dos_partition dp[NDOSPART], *dp2;
	u_int64_t sector = DOSBBSECTOR;
	u_int32_t extoff = 0;
	int error;

	if (lp->d_secpercyl == 0)
		return (EINVAL);	/* invalid label */
	if (lp->d_secsize == 0)
		return (ENOSPC);	/* disk too small */

	/* do DOS partitions in the process of getting disklabel? */

	/*
	 * Read dos partition table, follow extended partitions.
	 * Map the partitions to disklabel entries i-p
	 */
	while (wander && loop < DOS_MAXEBR) {
		loop++;
		wander = 0;
		if (sector < extoff)
			sector = extoff;

		/* read MBR/EBR */
		error = readdisksector(bp, strat, lp, sector);
		if (error) {
/*wrong*/		if (partoffp)
/*wrong*/			*partoffp = -1;
			return (error);
		}

		bcopy(bp->b_data + DOSPARTOFF, dp, sizeof(dp));

		if (n == 0 && sector == DOSBBSECTOR) {
			u_int16_t mbrtest;

			/* Check the end of sector marker. */
			mbrtest = ((bp->b_data[510] << 8) & 0xff00) |
			    (bp->b_data[511] & 0xff);
			if (mbrtest != 0x55aa)
				goto notmbr;

			if (gpt_chk_mbr(dp, DL_GETDSIZE(lp)) != 0)
				goto notgpt;

			gptlp = malloc(sizeof(struct disklabel), M_DEVBUF,
			    M_NOWAIT);
			if (gptlp == NULL)
				return (ENOMEM);
			*gptlp = *lp;
			error = spoofgptlabel(bp, strat, gptlp);
			if (error == 0) {
				dospartoff = DL_GETBSTART(gptlp);
				dospartend = DL_GETBEND(gptlp);
				if (partoffp == 0)
					*lp = *gptlp;
				free(gptlp, M_DEVBUF,
				    sizeof(struct disklabel));
				if (partoffp && dospartoff == 0)
					return (ENXIO);
				goto notfat;
			} else {
				free(gptlp, M_DEVBUF,
				    sizeof(struct disklabel));
				goto notmbr;
			}
		}

notgpt:
		if (ourpart == -1) {
			/* Search for our MBR partition */
			for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
			    i++, dp2++)
				if (letoh32(dp2->dp_size) &&
				    dp2->dp_typ == DOSPTYP_OPENBSD)
					ourpart = i;
			if (ourpart == -1)
				goto donot;
			/*
			 * This is our MBR partition. need sector
			 * address for SCSI/IDE, cylinder for
			 * ESDI/ST506/RLL
			 */
			dp2 = &dp[ourpart];
			dospartoff = letoh32(dp2->dp_start) + sector;
			dospartend = dospartoff + letoh32(dp2->dp_size);

			/*
			 * Record the OpenBSD partition's placement (in
			 * 512-byte blocks!) for the caller. No need to
			 * finish spoofing.
			 */
			if (partoffp) {
				*partoffp = DL_SECTOBLK(lp, dospartoff);
				return (0);
			}

			if (lp->d_ntracks == 0)
				lp->d_ntracks = dp2->dp_ehd + 1;
			if (lp->d_nsectors == 0)
				lp->d_nsectors = DPSECT(dp2->dp_esect);
			if (lp->d_secpercyl == 0)
				lp->d_secpercyl = lp->d_ntracks *
				    lp->d_nsectors;
		}
donot:
		/*
		 * In case the disklabel read below fails, we want to
		 * provide a fake label in i-p.
		 */
		for (dp2=dp, i=0; i < NDOSPART; i++, dp2++) {
			struct partition *pp;
			u_int8_t fstype;

			if (dp2->dp_typ == DOSPTYP_OPENBSD ||
			    dp2->dp_typ == DOSPTYP_EFI)
				continue;
			if (letoh32(dp2->dp_size) > DL_GETDSIZE(lp))
				continue;
			if (letoh32(dp2->dp_start) > DL_GETDSIZE(lp))
				continue;
			if (letoh32(dp2->dp_size) == 0)
				continue;

			switch (dp2->dp_typ) {
			case DOSPTYP_UNUSED:
				fstype = FS_UNUSED;
				break;

			case DOSPTYP_LINUX:
				fstype = FS_EXT2FS;
				break;

			case DOSPTYP_NTFS:
				fstype = FS_NTFS;
				break;

			case DOSPTYP_EFISYS:
			case DOSPTYP_FAT12:
			case DOSPTYP_FAT16S:
			case DOSPTYP_FAT16B:
			case DOSPTYP_FAT16L:
			case DOSPTYP_FAT32:
			case DOSPTYP_FAT32L:
				fstype = FS_MSDOS;
				break;
			case DOSPTYP_EXTEND:
			case DOSPTYP_EXTENDL:
				sector = letoh32(dp2->dp_start) + extoff;
				if (!extoff) {
					extoff = letoh32(dp2->dp_start);
					sector = 0;
				}
				wander = 1;
				continue;
				break;
			default:
				fstype = FS_OTHER;
				break;
			}

			/*
			 * Don't set fstype/offset/size when just looking for
			 * the offset of the OpenBSD partition. It would
			 * invalidate the disklabel checksum!
			 *
			 * Don't try to spoof more than 8 partitions, i.e.
			 * 'i' -'p'.
			 */
			if (partoffp || n >= 8)
				continue;

			pp = &lp->d_partitions[8+n];
			n++;
			pp->p_fstype = fstype;
			if (letoh32(dp2->dp_start))
				DL_SETPOFFSET(pp,
				    letoh32(dp2->dp_start) + sector);
			DL_SETPSIZE(pp, letoh32(dp2->dp_size));
		}
	}

notmbr:
	if (n == 0 && sector == DOSBBSECTOR && ourpart == -1) {
		u_int16_t fattest;

		/* Check for a valid initial jmp instruction. */
		switch ((u_int8_t)bp->b_data[0]) {
		case 0xeb:
			/*
			 * Two-byte jmp instruction. The 2nd byte is the number
			 * of bytes to jmp and the 3rd byte must be a NOP.
			 */
			if ((u_int8_t)bp->b_data[2] != 0x90)
				goto notfat;
			break;
		case 0xe9:
			/*
			 * Three-byte jmp instruction. The next two bytes are a
			 * little-endian 16 bit value.
			 */
			break;
		default:
			goto notfat;
			break;
		}

		/* Check for a valid bytes per sector value. */
		fattest = ((bp->b_data[12] << 8) & 0xff00) |
		    (bp->b_data[11] & 0xff);
		if (fattest < 512 || fattest > 4096 || (fattest % 512 != 0))
			goto notfat;

		if (partoffp)
			return (ENXIO);	/* No place for disklabel on FAT! */

		DL_SETPSIZE(&lp->d_partitions['i' - 'a'],
		    DL_GETPSIZE(&lp->d_partitions[RAW_PART]));
		DL_SETPOFFSET(&lp->d_partitions['i' - 'a'], 0);
		lp->d_partitions['i' - 'a'].p_fstype = FS_MSDOS;

		spoofonly = 1;	/* No disklabel to read from disk. */
	}

notfat:
	/* record the OpenBSD partition's placement for the caller */
	if (partoffp)
		*partoffp = DL_SECTOBLK(lp, dospartoff);
	else {
		DL_SETBSTART(lp, dospartoff);
		DL_SETBEND(lp, (dospartend < DL_GETDSIZE(lp)) ? dospartend :
		    DL_GETDSIZE(lp));
	}

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (0);

	error = readdisksector(bp, strat, lp, dospartoff +
	    DL_BLKTOSEC(lp, DOS_LABELSECTOR));
	if (error)
		return (bp->b_error);

	offset = DL_BLKOFFSET(lp, DOS_LABELSECTOR);
	error = checkdisklabel(bp->b_data + offset, lp,
	    DL_GETBSTART((struct disklabel*)(bp->b_data+offset)),
	    DL_GETBEND((struct disklabel *)(bp->b_data+offset)));

	return (error);
}

/*
 * Returns 0 if the MBR with the provided partition array is a GPT protective
 * MBR, and returns 1 otherwise. A GPT protective MBR would have one and only
 * one MBR partition, an EFI partition that either covers the whole disk or as
 * much of it as is possible with a 32bit size field.
 *
 * NOTE: MS always uses a size of UINT32_MAX for the EFI partition!**
 */
int
gpt_chk_mbr(struct dos_partition *dp, u_int64_t dsize)
{
	struct dos_partition *dp2;
	int efi, found, i;
	u_int32_t psize;

	found = efi = 0;
	for (dp2=dp, i=0; i < NDOSPART; i++, dp2++) {
		if (dp2->dp_typ == DOSPTYP_UNUSED)
			continue;
		found++;
		if (dp2->dp_typ != DOSPTYP_EFI)
			continue;
		psize = letoh32(dp2->dp_size);
		if (psize == (dsize - 1) ||
		    psize == UINT32_MAX) {
			if (letoh32(dp2->dp_start) == 1)
				efi++;
		}
	}
	if (found == 1 && efi == 1)
		return (0);

	return (1);
}

int
gpt_chk_hdr(struct gpt_header *gh, struct disklabel *lp)
{
	uint64_t ghpartlba;
	uint64_t ghlbaend, ghlbastart;
	uint32_t orig_gh_csum;
	uint32_t ghsize, ghpartsize, ghpartspersec;

	if (letoh64(gh->gh_sig) != GPTSIGNATURE)
		return (EINVAL);

	if (letoh32(gh->gh_rev) != GPTREVISION)
		return (EINVAL);

	ghsize = letoh32(gh->gh_size);
	ghpartsize = letoh32(gh->gh_part_size);
	ghpartspersec = lp->d_secsize / ghpartsize;
	ghpartlba = letoh64(gh->gh_part_lba);
	ghlbaend = letoh64(gh->gh_lba_end);
	ghlbastart = letoh64(gh->gh_lba_start);

	if (ghsize < GPTMINHDRSIZE || ghsize > sizeof(struct gpt_header))
		return (EINVAL);

	orig_gh_csum = gh->gh_csum;
	gh->gh_csum = 0;
	gh->gh_csum = crc32(0, (unsigned char *)gh, ghsize);

	if (orig_gh_csum != gh->gh_csum)
		return (EINVAL);

	if (ghlbastart >= DL_GETDSIZE(lp) ||
	    ghlbaend >= DL_GETDSIZE(lp) ||
	    ghpartlba >= DL_GETDSIZE(lp))
		return (EINVAL);

	/*
	* Size per partition entry shall be 128*(2**n) with n >= 0.
	* We don't support partition entries larger than block size.
	*/
	if (ghpartsize % GPTMINPARTSIZE || ghpartsize > lp->d_secsize
	    || ghpartspersec == 0) {
		DPRINTF("invalid partition size\n");
		return (EINVAL);
	}

	/* XXX: we don't support multiples of GPTMINPARTSIZE yet */
	if (ghpartsize != GPTMINPARTSIZE) {
		DPRINTF("partition sizes larger than %d bytes are not "
		    "supported", GPTMINPARTSIZE);
		return (EINVAL);
	}

	if (letoh64(gh->gh_lba_alt) >= DL_GETDSIZE(lp)) {
		DPRINTF("alternate header's position is bogus\n");
		return (EINVAL);
	}

	return 0;
}

int
gpt_chk_parts(struct gpt_header *gh, struct gpt_partition *gp)
{
	u_int32_t checksum;
	checksum = crc32(0, (unsigned char *)gp,
	    letoh32(gh->gh_part_num) * letoh32(gh->gh_part_size));

	if (checksum != gh->gh_part_csum)
		return (EINVAL);

	return 0;
}

int
gpt_get_fstype(struct uuid *uuid_part)
{
	static int init = 0;
	static struct uuid uuid_openbsd, uuid_msdos, uuid_chromefs,
	    uuid_linux, uuid_hfs, uuid_unused, uuid_efi_system;
	static const uint8_t gpt_uuid_openbsd[] = GPT_UUID_OPENBSD;
	static const uint8_t gpt_uuid_msdos[] = GPT_UUID_MSDOS;
	static const uint8_t gpt_uuid_chromerootfs[] = GPT_UUID_CHROMEROOTFS;
	static const uint8_t gpt_uuid_linux[] = GPT_UUID_LINUX;
	static const uint8_t gpt_uuid_hfs[] = GPT_UUID_APPLE_HFS;
	static const uint8_t gpt_uuid_unused[] = GPT_UUID_UNUSED;
	static const uint8_t gpt_uuid_efi_system[] = GPT_UUID_EFI_SYSTEM;

	if (init == 0) {
		uuid_dec_be(gpt_uuid_openbsd, &uuid_openbsd);
		uuid_dec_be(gpt_uuid_msdos, &uuid_msdos);
		uuid_dec_be(gpt_uuid_chromerootfs, &uuid_chromefs);
		uuid_dec_be(gpt_uuid_linux, &uuid_linux);
		uuid_dec_be(gpt_uuid_hfs, &uuid_hfs);
		uuid_dec_be(gpt_uuid_unused, &uuid_unused);
		uuid_dec_be(gpt_uuid_efi_system, &uuid_efi_system);
		init = 1;
	}

	if (!memcmp(uuid_part, &uuid_unused, sizeof(struct uuid)))
		return FS_UNUSED;
	else if (!memcmp(uuid_part, &uuid_openbsd, sizeof(struct uuid)))
		return FS_BSDFFS;
	else if (!memcmp(uuid_part, &uuid_msdos, sizeof(struct uuid)))
		return FS_MSDOS;
	else if (!memcmp(uuid_part, &uuid_chromefs, sizeof(struct uuid)))
		return FS_EXT2FS;
	else if (!memcmp(uuid_part, &uuid_linux, sizeof(struct uuid)))
		return FS_EXT2FS;
	else if (!memcmp(uuid_part, &uuid_hfs, sizeof(struct uuid)))
		return FS_HFS;
	else if (!memcmp(uuid_part, &uuid_efi_system, sizeof(struct uuid)))
		return FS_MSDOS;
	else
		return FS_OTHER;
}

/*
 * Spoof a disklabel based on the GPT information on the disk.
 */
int
spoofgptlabel(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp)
{
	static const u_int8_t gpt_uuid_openbsd[] = GPT_UUID_OPENBSD;
	struct gpt_header gh;
	struct uuid uuid_part, uuid_openbsd;
	struct gpt_partition *gp, *gp_tmp;
	struct partition *pp;
	size_t gpsz;
	u_int64_t ghlbaend, ghlbastart, gptpartoff, gptpartend, sector;
	u_int64_t start, end;
	int i, altheader = 0, error, n;
	uint32_t ghpartnum;

	uuid_dec_be(gpt_uuid_openbsd, &uuid_openbsd);

	for (sector = GPTSECTOR; ; sector = DL_GETDSIZE(lp)-1, altheader = 1) {
		uint64_t ghpartlba;
		uint32_t ghpartsize;
		uint32_t ghpartspersec;

		error = readdisksector(bp, strat, lp, sector);
		if (error) {
			DPRINTF("error reading from disk\n");
			return (error);
		}

		bcopy(bp->b_data, &gh, sizeof(gh));

		if (gpt_chk_hdr(&gh, lp)) {
			if (altheader) {
				DPRINTF("alternate header also broken\n");
				return (EINVAL);
			}
			continue;
		}

		ghpartsize = letoh32(gh.gh_part_size);
		ghpartspersec = lp->d_secsize / ghpartsize;
		ghpartnum = letoh32(gh.gh_part_num);
		ghpartlba = letoh64(gh.gh_part_lba);
		ghlbaend = letoh64(gh.gh_lba_end);
		ghlbastart = letoh64(gh.gh_lba_start);

		/* read GPT partition entry array */
		gp = mallocarray(ghpartnum, sizeof(struct gpt_partition),
		    M_DEVBUF, M_NOWAIT|M_ZERO);
		if (gp == NULL)
			return (ENOMEM);
		gpsz = ghpartnum * sizeof(struct gpt_partition);

		/*
		* XXX:	Fails if # of partition entries is not a multiple of
		*	ghpartspersec.
		*/
		sector = ghpartlba;
		for (i = 0; i < ghpartnum / ghpartspersec; i++, sector++) {
			error = readdisksector(bp, strat, lp, sector);
			if (error) {
				free(gp, M_DEVBUF, gpsz);
				return (error);
			}

			bcopy(bp->b_data, gp + i * ghpartspersec,
			    ghpartspersec * sizeof(struct gpt_partition));
		}

		if (gpt_chk_parts(&gh, gp)) {
			free(gp, M_DEVBUF, gpsz);
			if (altheader) {
				DPRINTF("alternate partition entries are also "
				    "broken\n");
				return (EINVAL);
			}
			continue;
		}
		break;
	}

	/* Find OpenBSD partition and spoof others along the way. */
	n = 0;
	gptpartoff = 0;
	gptpartend = DL_GETBEND(lp);
	for (gp_tmp = gp, i = 0; i < ghpartnum; gp_tmp++, i++) {
		start = letoh64(gp_tmp->gp_lba_start);
		end = letoh64(gp_tmp->gp_lba_end);
		if (start > end || start < ghlbastart || end > ghlbaend)
			continue; /* entry invalid */

		uuid_dec_le(&gp_tmp->gp_type, &uuid_part);
		if (!memcmp(&uuid_part, &uuid_openbsd, sizeof(struct uuid))) {
			if (gptpartoff == 0) {
				gptpartoff = start;
				gptpartend = end + 1;
			}
			continue; /* Do *NOT* spoof OpenBSD partitions! */
		}

		 /*
		 * Don't try to spoof more than 8 partitions, i.e.
		 * 'i' -'p'.
		 */
		if (n >= 8)
			continue;

		pp = &lp->d_partitions[8+n];
		n++;
		pp->p_fstype = gpt_get_fstype(&uuid_part);
		DL_SETPOFFSET(pp, start);
		DL_SETPSIZE(pp, end - start + 1);
	}

	free(gp, M_DEVBUF, gpsz);

	DL_SETBSTART(lp, gptpartoff);
	DL_SETBEND(lp, (gptpartend < DL_GETDSIZE(lp)) ? gptpartend :
	    DL_GETDSIZE(lp));

	return (0);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(struct disklabel *olp, struct disklabel *nlp, u_int openmask)
{
	struct partition *opp, *npp;
	struct disk *dk;
	int i;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0 ||
	    (nlp->d_secsize % DEV_BSIZE) != 0)
		return (EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	for (i = 0; i < MAXPARTITIONS; i++) {
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if ((openmask & (1 << i)) &&
		    (DL_GETPOFFSET(npp) != DL_GETPOFFSET(opp) ||
		    DL_GETPSIZE(npp) < DL_GETPSIZE(opp)))
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fragblock = opp->p_fragblock;
			npp->p_cpg = opp->p_cpg;
		}
	}

	/* Generate a UID if the disklabel does not already have one. */
	if (duid_iszero(nlp->d_uid)) {
		do {
			arc4random_buf(nlp->d_uid, sizeof(nlp->d_uid));
			TAILQ_FOREACH(dk, &disklist, dk_link)
				if (dk->dk_label &&
				    duid_equal(dk->dk_label->d_uid, nlp->d_uid))
					break;
		} while (dk != NULL || duid_iszero(nlp->d_uid));
	}

	/* Preserve the disk size and RAW_PART values. */
	DL_SETDSIZE(nlp, DL_GETDSIZE(olp));
	npp = &nlp->d_partitions[RAW_PART];
	DL_SETPOFFSET(npp, 0);
	DL_SETPSIZE(npp, DL_GETDSIZE(nlp));

	nlp->d_checksum = 0;
	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;

	disk_change = 1;

	return (0);
}

/*
 * Determine the size of the transfer, and make sure it is within the
 * boundaries of the partition. Adjust transfer if needed, and signal errors or
 * early completion.
 */
int
bounds_check_with_label(struct buf *bp, struct disklabel *lp)
{
	struct partition *p = &lp->d_partitions[DISKPART(bp->b_dev)];
	daddr_t partblocks, sz;

	/* Avoid division by zero, negative offsets, and negative sizes. */
	if (lp->d_secpercyl == 0 || bp->b_blkno < 0 || bp->b_bcount < 0)
		goto bad;

	/* Ensure transfer is a whole number of aligned sectors. */
	if ((bp->b_blkno % DL_BLKSPERSEC(lp)) != 0 ||
	    (bp->b_bcount % lp->d_secsize) != 0)
		goto bad;

	/* Ensure transfer starts within partition boundary. */
	partblocks = DL_SECTOBLK(lp, DL_GETPSIZE(p));
	if (bp->b_blkno > partblocks)
		goto bad;

	/* If exactly at end of partition or null transfer, return EOF. */
	if (bp->b_blkno == partblocks || bp->b_bcount == 0)
		goto done;

	/* Truncate request if it extends past the end of the partition. */
	sz = bp->b_bcount >> DEV_BSHIFT;
	if (sz > partblocks - bp->b_blkno) {
		sz = partblocks - bp->b_blkno;
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	return (0);

 bad:
	bp->b_error = EINVAL;
	bp->b_flags |= B_ERROR;
 done:
	bp->b_resid = bp->b_bcount;
	return (-1);
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf
 * if pri is LOG_PRINTF, otherwise it uses log at the specified priority.
 * The message should be completed (with at least a newline) with printf
 * or addlog, respectively.  There is no trailing space.
 */
void
diskerr(struct buf *bp, char *dname, char *what, int pri, int blkdone,
    struct disklabel *lp)
{
	int unit = DISKUNIT(bp->b_dev), part = DISKPART(bp->b_dev);
	int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2)));
	char partname = 'a' + part;
	daddr_t sn;

	if (pri != LOG_PRINTF) {
		log(pri, "%s", "");
		pr = addlog;
	} else
		pr = printf;
	(*pr)("%s%d%c: %s %sing fsbn ", dname, unit, partname, what,
	    bp->b_flags & B_READ ? "read" : "writ");
	sn = bp->b_blkno;
	if (bp->b_bcount <= DEV_BSIZE)
		(*pr)("%lld", (long long)sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			(*pr)("%lld of ", (long long)sn);
		}
		(*pr)("%lld-%lld", (long long)bp->b_blkno,
		    (long long)(bp->b_blkno + (bp->b_bcount - 1) / DEV_BSIZE));
	}
	if (lp && (blkdone >= 0 || bp->b_bcount <= lp->d_secsize)) {
		sn += DL_SECTOBLK(lp, DL_GETPOFFSET(&lp->d_partitions[part]));
		(*pr)(" (%s%d bn %lld; cn %lld", dname, unit, (long long)sn,
		    (long long)(sn / DL_SECTOBLK(lp, lp->d_secpercyl)));
		sn %= DL_SECTOBLK(lp, lp->d_secpercyl);
		(*pr)(" tn %lld sn %lld)",
		    (long long)(sn / DL_SECTOBLK(lp, lp->d_nsectors)),
		    (long long)(sn % DL_SECTOBLK(lp, lp->d_nsectors)));
	}
}

/*
 * Initialize the disklist.  Called by main() before autoconfiguration.
 */
void
disk_init(void)
{

	TAILQ_INIT(&disklist);
	disk_count = disk_change = 0;
}

int
disk_construct(struct disk *diskp)
{
	rw_init_flags(&diskp->dk_lock, "dklk", RWL_IS_VNODE);
	mtx_init(&diskp->dk_mtx, IPL_BIO);

	diskp->dk_flags |= DKF_CONSTRUCTED;

	return (0);
}

/*
 * Attach a disk.
 */
void
disk_attach(struct device *dv, struct disk *diskp)
{
	int majdev;

	if (!ISSET(diskp->dk_flags, DKF_CONSTRUCTED))
		disk_construct(diskp);

	/*
	 * Allocate and initialize the disklabel structures.  Note that
	 * it's not safe to sleep here, since we're probably going to be
	 * called during autoconfiguration.
	 */
	diskp->dk_label = malloc(sizeof(struct disklabel), M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (diskp->dk_label == NULL)
		panic("disk_attach: can't allocate storage for disklabel");

	/*
	 * Set the attached timestamp.
	 */
	microuptime(&diskp->dk_attachtime);

	/*
	 * Link into the disklist.
	 */
	TAILQ_INSERT_TAIL(&disklist, diskp, dk_link);
	++disk_count;
	disk_change = 1;

	/*
	 * Store device structure and number for later use.
	 */
	diskp->dk_device = dv;
	diskp->dk_devno = NODEV;
	if (dv != NULL) {
		majdev = findblkmajor(dv);
		if (majdev >= 0)
			diskp->dk_devno =
			    MAKEDISKDEV(majdev, dv->dv_unit, RAW_PART);

		if (diskp->dk_devno != NODEV) {
			struct disk_attach_task *dat;

			dat = malloc(sizeof(*dat), M_TEMP, M_WAITOK);

			/* XXX: Assumes dk is part of the device softc. */
			device_ref(dv);
			dat->dk = diskp;

			task_set(&dat->task, disk_attach_callback, dat);
			task_add(systq, &dat->task);
		}
	}

	if (softraid_disk_attach)
		softraid_disk_attach(diskp, 1);
}

void
disk_attach_callback(void *xdat)
{
	struct disk_attach_task *dat = xdat;
	struct disk *dk = dat->dk;
	struct disklabel dl;
	char errbuf[100];

	free(dat, M_TEMP, sizeof(*dat));

	if (dk->dk_flags & (DKF_OPENED | DKF_NOLABELREAD))
		goto done;

	/* Read disklabel. */
	if (disk_readlabel(&dl, dk->dk_devno, errbuf, sizeof(errbuf)) == NULL) {
		enqueue_randomness(dl.d_checksum);
		dk->dk_flags |= DKF_LABELVALID;
	}

done:
	dk->dk_flags |= DKF_OPENED;
	device_unref(dk->dk_device);
	wakeup(dk);
}

/*
 * Detach a disk.
 */
void
disk_detach(struct disk *diskp)
{

	if (softraid_disk_attach)
		softraid_disk_attach(diskp, -1);

	/*
	 * Free the space used by the disklabel structures.
	 */
	free(diskp->dk_label, M_DEVBUF, sizeof(*diskp->dk_label));

	/*
	 * Remove from the disklist.
	 */
	TAILQ_REMOVE(&disklist, diskp, dk_link);
	disk_change = 1;
	if (--disk_count < 0)
		panic("disk_detach: disk_count < 0");
}

int
disk_openpart(struct disk *dk, int part, int fmt, int haslabel)
{
	KASSERT(part >= 0 && part < MAXPARTITIONS);

	/* Unless opening the raw partition, check that the partition exists. */
	if (part != RAW_PART && (!haslabel ||
	    part >= dk->dk_label->d_npartitions ||
	    dk->dk_label->d_partitions[part].p_fstype == FS_UNUSED))
		return (ENXIO);

	/* Ensure the partition doesn't get changed under our feet. */
	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		dk->dk_bopenmask |= (1 << part);
		break;
	}
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;

	return (0);
}

void
disk_closepart(struct disk *dk, int part, int fmt)
{
	KASSERT(part >= 0 && part < MAXPARTITIONS);

	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		dk->dk_bopenmask &= ~(1 << part);
		break;
	}
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;
}

void
disk_gone(int (*open)(dev_t, int, int, struct proc *), int unit)
{
	int bmaj, cmaj, mn;

	/* Locate the lowest minor number to be detached. */
	mn = DISKMINOR(unit, 0);

	for (bmaj = 0; bmaj < nblkdev; bmaj++)
		if (bdevsw[bmaj].d_open == open)
			vdevgone(bmaj, mn, mn + MAXPARTITIONS - 1, VBLK);
	for (cmaj = 0; cmaj < nchrdev; cmaj++)
		if (cdevsw[cmaj].d_open == open)
			vdevgone(cmaj, mn, mn + MAXPARTITIONS - 1, VCHR);
}

/*
 * Increment a disk's busy counter.  If the counter is going from
 * 0 to 1, set the timestamp.
 */
void
disk_busy(struct disk *diskp)
{

	/*
	 * XXX We'd like to use something as accurate as microtime(),
	 * but that doesn't depend on the system TOD clock.
	 */
	mtx_enter(&diskp->dk_mtx);
	if (diskp->dk_busy++ == 0)
		microuptime(&diskp->dk_timestamp);
	mtx_leave(&diskp->dk_mtx);
}

/*
 * Decrement a disk's busy counter, increment the byte count, total busy
 * time, and reset the timestamp.
 */
void
disk_unbusy(struct disk *diskp, long bcount, daddr_t blkno, int read)
{
	struct timeval dv_time, diff_time;

	mtx_enter(&diskp->dk_mtx);

	if (diskp->dk_busy-- == 0)
		printf("disk_unbusy: %s: dk_busy < 0\n", diskp->dk_name);

	microuptime(&dv_time);

	timersub(&dv_time, &diskp->dk_timestamp, &diff_time);
	timeradd(&diskp->dk_time, &diff_time, &diskp->dk_time);

	diskp->dk_timestamp = dv_time;
	if (bcount > 0) {
		if (read) {
			diskp->dk_rbytes += bcount;
			diskp->dk_rxfer++;
		} else {
			diskp->dk_wbytes += bcount;
			diskp->dk_wxfer++;
		}
	} else
		diskp->dk_seek++;

	mtx_leave(&diskp->dk_mtx);

	enqueue_randomness(bcount ^ diff_time.tv_usec ^
	    (blkno >> 32) ^ (blkno & 0xffffffff));
}

int
disk_lock(struct disk *dk)
{
	return (rw_enter(&dk->dk_lock, RW_WRITE|RW_INTR));
}

void
disk_lock_nointr(struct disk *dk)
{
	rw_enter_write(&dk->dk_lock);
}

void
disk_unlock(struct disk *dk)
{
	rw_exit_write(&dk->dk_lock);
}

int
dk_mountroot(void)
{
	char errbuf[100];
	int part = DISKPART(rootdev);
	int (*mountrootfn)(void);
	struct disklabel dl;
	char *error;

	error = disk_readlabel(&dl, rootdev, errbuf, sizeof(errbuf));
	if (error)
		panic("%s", error);

	if (DL_GETPSIZE(&dl.d_partitions[part]) == 0)
		panic("root filesystem has size 0");
	switch (dl.d_partitions[part].p_fstype) {
#ifdef EXT2FS
	case FS_EXT2FS:
		{
		extern int ext2fs_mountroot(void);
		mountrootfn = ext2fs_mountroot;
		}
		break;
#endif
#ifdef FFS
	case FS_BSDFFS:
		{
		extern int ffs_mountroot(void);
		mountrootfn = ffs_mountroot;
		}
		break;
#endif
#ifdef CD9660
	case FS_ISO9660:
		{
		extern int cd9660_mountroot(void);
		mountrootfn = cd9660_mountroot;
		}
		break;
#endif
	default:
#ifdef FFS
		{
		extern int ffs_mountroot(void);

		printf("filesystem type %d not known.. assuming ffs\n",
		    dl.d_partitions[part].p_fstype);
		mountrootfn = ffs_mountroot;
		}
#else
		panic("disk 0x%x filesystem type %d not known",
		    rootdev, dl.d_partitions[part].p_fstype);
#endif
	}
	return (*mountrootfn)();
}

struct device *
getdisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of: exit");
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-p]", dv->dv_xname);
#if defined(NFSCLIENT)
			if (dv->dv_class == DV_IFNET)
				printf(" %s", dv->dv_xname);
#endif
		}
		printf("\n");
	}
	return (dv);
}

struct device *
parsedisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;
	int majdev, part = defpart;
	char c;

	if (len == 0)
		return (NULL);
	c = str[len-1];
	if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
		part = c - 'a';
		len -= 1;
	}

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK &&
		    strncmp(str, dv->dv_xname, len) == 0 &&
		    dv->dv_xname[len] == '\0') {
			majdev = findblkmajor(dv);
			if (majdev < 0)
				return NULL;
			*devp = MAKEDISKDEV(majdev, dv->dv_unit, part);
			break;
		}
#if defined(NFSCLIENT)
		if (dv->dv_class == DV_IFNET &&
		    strncmp(str, dv->dv_xname, len) == 0 &&
		    dv->dv_xname[len] == '\0') {
			*devp = NODEV;
			break;
		}
#endif
	}

	return (dv);
}

void
setroot(struct device *bootdv, int part, int exitflags)
{
	int majdev, unit, len, s, slept = 0;
	struct swdevt *swp;
	struct device *rootdv, *dv;
	dev_t nrootdev, nswapdev = NODEV, temp = NODEV;
	struct ifnet *ifp = NULL;
	struct disk *dk;
	char buf[128];
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

	/* Ensure that all disk attach callbacks have completed. */
	do {
		TAILQ_FOREACH(dk, &disklist, dk_link) {
			if (dk->dk_devno != NODEV &&
			    (dk->dk_flags & DKF_OPENED) == 0) {
				tsleep_nsec(dk, 0, "dkopen", SEC_TO_NSEC(1));
				slept++;
				break;
			}
		}
	} while (dk != NULL && slept < 5);

	if (slept == 5) {
		printf("disklabels not read:");
		TAILQ_FOREACH(dk, &disklist, dk_link)
			if (dk->dk_devno != NODEV &&
			    (dk->dk_flags & DKF_OPENED) == 0)
				printf(" %s", dk->dk_name);
		printf("\n");
	}

	if (duid_iszero(bootduid)) {
		/* Locate DUID for boot disk since it was not provided. */
		TAILQ_FOREACH(dk, &disklist, dk_link)
			if (dk->dk_device == bootdv)
				break;
		if (dk && (dk->dk_flags & DKF_LABELVALID))
			bcopy(dk->dk_label->d_uid, bootduid, sizeof(bootduid));
	} else if (bootdv == NULL) {
		/* Locate boot disk based on the provided DUID. */
		TAILQ_FOREACH(dk, &disklist, dk_link)
			if (duid_equal(dk->dk_label->d_uid, bootduid))
				break;
		if (dk && (dk->dk_flags & DKF_LABELVALID))
			bootdv = dk->dk_device;
	}
	bcopy(bootduid, rootduid, sizeof(rootduid));

#if NSOFTRAID > 0
	sr_map_root();
#endif

	/*
	 * If `swap generic' and we couldn't determine boot device,
	 * ask the user.
	 */
	dk = NULL;
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;
	if (boothowto & RB_ASKNAME) {
		while (1) {
			printf("root device");
			if (bootdv != NULL) {
				printf(" (default %s", bootdv->dv_xname);
				if (bootdv->dv_class == DV_DISK)
					printf("%c", 'a' + part);
				printf(")");
			}
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));
			cnpollc(0);
			splx(s);
			if (strcmp(buf, "exit") == 0)
				reboot(exitflags);
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, bootdv->dv_xname, sizeof buf);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, part, &nrootdev);
				if (dv != NULL) {
					rootdv = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			dv = getdisk(buf, len, part, &nrootdev);
			if (dv != NULL) {
				rootdv = dv;
				break;
			}
		}

		if (rootdv->dv_class == DV_IFNET)
			goto gotswap;

		/* try to build swap device out of new root device */
		while (1) {
			printf("swap device");
			if (rootdv != NULL)
				printf(" (default %s%s)", rootdv->dv_xname,
				    rootdv->dv_class == DV_DISK ? "b" : "");
			printf(": ");
			s = splhigh();
			cnpollc(1);
			len = getsn(buf, sizeof(buf));
			cnpollc(0);
			splx(s);
			if (strcmp(buf, "exit") == 0)
				reboot(exitflags);
			if (len == 0 && rootdv != NULL) {
				switch (rootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
					if (nswapdev == nrootdev)
						continue;
					break;
				default:
					break;
				}
				break;
			}
			dv = getdisk(buf, len, 1, &nswapdev);
			if (dv) {
				if (dv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				if (nswapdev == nrootdev)
					continue;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;
#if defined(NFSCLIENT)
	} else if (mountroot == nfs_mountroot) {
		rootdv = bootdv;
		rootdev = dumpdev = swapdev = NODEV;
#endif
	} else if (mountroot == NULL && rootdev == NODEV) {
		/*
		 * `swap generic'
		 */
		rootdv = bootdv;

		if (bootdv->dv_class == DV_DISK) {
			if (!duid_iszero(rootduid)) {
				TAILQ_FOREACH(dk, &disklist, dk_link)
					if ((dk->dk_flags & DKF_LABELVALID) &&
					    dk->dk_label && duid_equal(
					    dk->dk_label->d_uid, rootduid))
						break;
				if (dk == NULL)
					panic("root device (%s) not found",
					    duid_format(rootduid));
				rootdv = dk->dk_device;
			}
		}

		majdev = findblkmajor(rootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on the disk.
			 * Assume swap is on partition b.
			 */
			rootdev = MAKEDISKDEV(majdev, rootdv->dv_unit, part);
			nswapdev = MAKEDISKDEV(majdev, rootdv->dv_unit, 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			nswapdev = NODEV;
		}
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */
	} else {
		/* Completely pre-configured, but we want rootdv .. */
		majdev = major(rootdev);
		if (findblkname(majdev) == NULL)
			return;
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		snprintf(buf, sizeof buf, "%s%d%c",
		    findblkname(majdev), unit, 'a' + part);
		rootdv = parsedisk(buf, strlen(buf), 0, &nrootdev);
		if (rootdv == NULL)
			panic("root device (%s) not found", buf);
	}

	if (bootdv != NULL && bootdv->dv_class == DV_IFNET)
		ifp = ifunit(bootdv->dv_xname);

	if (ifp)
		if_addgroup(ifp, "netboot");

	switch (rootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = rootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		part = DISKPART(rootdev);
		break;
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	printf("root on %s%c", rootdv->dv_xname, 'a' + part);

	if (dk && dk->dk_device == rootdv)
		printf(" (%s.%c)", duid_format(rootduid), 'a' + part);

	/*
	 * Make the swap partition on the root drive the primary swap.
	 */
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (major(rootdev) == major(swp->sw_dev) &&
		    DISKUNIT(rootdev) == DISKUNIT(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev != NODEV) {
		/*
		 * If dumpdev was the same as the old primary swap device,
		 * move it to the new primary swap device.
		 */
		if (temp == dumpdev)
			dumpdev = swdevt[0].sw_dev;
	}
	if (swdevt[0].sw_dev != NODEV)
		printf(" swap on %s%d%c", findblkname(major(swdevt[0].sw_dev)),
		    DISKUNIT(swdevt[0].sw_dev),
		    'a' + DISKPART(swdevt[0].sw_dev));
	if (dumpdev != NODEV)
		printf(" dump on %s%d%c", findblkname(major(dumpdev)),
		    DISKUNIT(dumpdev), 'a' + DISKPART(dumpdev));
	printf("\n");
}

extern struct nam2blk nam2blk[];

int
findblkmajor(struct device *dv)
{
	char buf[16], *p;
	int i;

	if (strlcpy(buf, dv->dv_xname, sizeof buf) >= sizeof buf)
		return (-1);
	for (p = buf; *p; p++)
		if (*p >= '0' && *p <= '9')
			*p = '\0';

	for (i = 0; nam2blk[i].name; i++)
		if (!strcmp(buf, nam2blk[i].name))
			return (nam2blk[i].maj);
	return (-1);
}

char *
findblkname(int maj)
{
	int i;

	for (i = 0; nam2blk[i].name; i++)
		if (nam2blk[i].maj == maj)
			return (nam2blk[i].name);
	return (NULL);
}

char *
disk_readlabel(struct disklabel *dl, dev_t dev, char *errbuf, size_t errsize)
{
	struct vnode *vn;
	dev_t chrdev, rawdev;
	int error;

	chrdev = blktochr(dev);
	rawdev = MAKEDISKDEV(major(chrdev), DISKUNIT(chrdev), RAW_PART);

#ifdef DEBUG
	printf("dev=0x%x chrdev=0x%x rawdev=0x%x\n", dev, chrdev, rawdev);
#endif

	if (cdevvp(rawdev, &vn)) {
		snprintf(errbuf, errsize,
		    "cannot obtain vnode for 0x%x/0x%x", dev, rawdev);
		return (errbuf);
	}

	error = VOP_OPEN(vn, FREAD, NOCRED, curproc);
	if (error) {
		snprintf(errbuf, errsize,
		    "cannot open disk, 0x%x/0x%x, error %d",
		    dev, rawdev, error);
		goto done;
	}

	error = VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)dl, FREAD, NOCRED, curproc);
	if (error) {
		snprintf(errbuf, errsize,
		    "cannot read disk label, 0x%x/0x%x, error %d",
		    dev, rawdev, error);
	}
done:
	VOP_CLOSE(vn, FREAD, NOCRED, curproc);
	vput(vn);
	if (error)
		return (errbuf);
	return (NULL);
}

int
disk_map(char *path, char *mappath, int size, int flags)
{
	struct disk *dk, *mdk;
	u_char uid[8];
	char c, part;
	int i;

	/*
	 * Attempt to map a request for a disklabel UID to the correct device.
	 * We should be supplied with a disklabel UID which has the following
	 * format:
	 *
	 * [disklabel uid] . [partition]
	 *
	 * Alternatively, if the DM_OPENPART flag is set the disklabel UID can
	 * based passed on its own.
	 */

	if (strchr(path, '/') != NULL)
		return -1;

	/* Verify that the device name is properly formed. */
	if (!((strlen(path) == 16 && (flags & DM_OPENPART)) ||
	    (strlen(path) == 18 && path[16] == '.')))
		return -1;

	/* Get partition. */
	if (flags & DM_OPENPART)
		part = 'a' + RAW_PART;
	else
		part = path[17];

	if (part < 'a' || part >= 'a' + MAXPARTITIONS)
		return -1;

	/* Derive label UID. */
	memset(uid, 0, sizeof(uid));
	for (i = 0; i < 16; i++) {
		c = path[i];
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'a' && c <= 'f')
			c -= ('a' - 10);
		else
			return -1;

		uid[i / 2] <<= 4;
		uid[i / 2] |= c & 0xf;
	}

	mdk = NULL;
	TAILQ_FOREACH(dk, &disklist, dk_link) {
		if ((dk->dk_flags & DKF_LABELVALID) && dk->dk_label &&
		    memcmp(dk->dk_label->d_uid, uid,
		    sizeof(dk->dk_label->d_uid)) == 0) {
			/* Fail if there are duplicate UIDs! */
			if (mdk != NULL)
				return -1;
			mdk = dk;
		}
	}

	if (mdk == NULL || mdk->dk_name == NULL)
		return -1;

	snprintf(mappath, size, "/dev/%s%s%c",
	    (flags & DM_OPENBLCK) ? "" : "r", mdk->dk_name, part);

	return 0;
}

/*
 * Lookup a disk device and verify that it has completed attaching.
 */
struct device *
disk_lookup(struct cfdriver *cd, int unit)
{
	struct device *dv;
	struct disk *dk;

	dv = device_lookup(cd, unit);
	if (dv == NULL)
		return (NULL);

	TAILQ_FOREACH(dk, &disklist, dk_link)
		if (dk->dk_device == dv)
			break;

	if (dk == NULL) {
		device_unref(dv);
		return (NULL);
	}

	return (dv);
}

int
duid_equal(u_char *duid1, u_char *duid2)
{
	return (memcmp(duid1, duid2, DUID_SIZE) == 0);
}

int
duid_iszero(u_char *duid)
{
	u_char zeroduid[DUID_SIZE];

	memset(zeroduid, 0, sizeof(zeroduid));

	return (duid_equal(duid, zeroduid));
}

const char *
duid_format(u_char *duid)
{
	static char duid_str[17];

	snprintf(duid_str, sizeof(duid_str),
	    "%02x%02x%02x%02x%02x%02x%02x%02x",
	    duid[0], duid[1], duid[2], duid[3],
	    duid[4], duid[5], duid[6], duid[7]);

	return (duid_str);
}

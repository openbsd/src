/*	$OpenBSD: efidev.c,v 1.4 2015/09/07 05:10:52 yasuoka Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
 * Copyright (c) 2003 Tobias Weingartner
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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
#include <lib/libz/zlib.h>

#include "libsa.h"
#include "disk.h"

#ifdef SOFTRAID
#include <dev/softraidvar.h>
#include "softraid.h"
#endif

#include <efi.h>
#include "eficall.h"

extern int debug;

#include "efidev.h"
#include "biosdev.h"	/* for dklookup() */

#define EFI_BLKSPERSEC(_ed)	((_ed)->blkio->Media->BlockSize / DEV_BSIZE)
#define EFI_SECTOBLK(_ed, _n)	((_n) * EFI_BLKSPERSEC(_ed))

struct efi_diskinfo {
	EFI_BLOCK_IO		*blkio;
	UINT32			 mediaid;
};

int bios_bootdev;
static EFI_STATUS
		 efid_io(int, efi_diskinfo_t, u_int, int, void *);
static int	 efid_diskio(int, struct diskinfo *, u_int, int, void *);
static u_int	 findopenbsd(efi_diskinfo_t, const char **);
static uint64_t	 findopenbsd_gpt(efi_diskinfo_t, const char **);

void
efid_init(struct diskinfo *dip, void *handle)
{
	EFI_BLOCK_IO		*blkio = handle;

	memset(dip, 0, sizeof(struct diskinfo));
	dip->efi_info = alloc(sizeof(struct efi_diskinfo));
	dip->efi_info->blkio = blkio;
	dip->efi_info->mediaid = blkio->Media->MediaId;
	dip->diskio = efid_diskio;
	dip->strategy = efistrategy;
}

static EFI_STATUS
efid_io(int rw, efi_diskinfo_t ed, u_int off, int nsect, void *buf)
{
	u_int		 blks, lba, i_lblks, i_tblks, i_nblks;
	EFI_STATUS	 status = EFI_SUCCESS;
	static u_char	*iblk = NULL;
	static u_int	 iblksz = 0;

	/* block count of the intrisic block size in DEV_BSIZE */
	blks = EFI_BLKSPERSEC(ed);
	lba = off / blks;

	/* leading and trailing unaligned blocks in intrisic block */
	i_lblks = ((off % blks) == 0)? 0 : blks - (off % blks);
	i_tblks = (off + nsect) % blks;

	/* aligned blocks in intrisic block */
	i_nblks = nsect - (i_lblks + i_tblks);

	switch (rw) {
	case F_READ:
		/* allocate the space for reading unaligned blocks */
		if (ed->blkio->Media->BlockSize != DEV_BSIZE) {
			if (iblk && iblksz < ed->blkio->Media->BlockSize)
				free(iblk, iblksz);
			if (iblk == NULL) {
				iblk = alloc(ed->blkio->Media->BlockSize);
				iblksz = ed->blkio->Media->BlockSize;
			}
		}
		if (i_lblks > 0) {
			status = EFI_CALL(ed->blkio->ReadBlocks,
			    ed->blkio, ed->mediaid, lba - 1,
			    ed->blkio->Media->BlockSize, iblk);
			if (EFI_ERROR(status))
				goto on_eio;
			memcpy(buf, iblk + (blks - i_lblks),
			    i_lblks * DEV_BSIZE);
		}
		if (i_nblks > 0) {
			status = EFI_CALL(ed->blkio->ReadBlocks,
			    ed->blkio, ed->mediaid, lba,
			    ed->blkio->Media->BlockSize * (i_nblks / blks),
			    buf + (i_lblks * DEV_BSIZE));
			if (EFI_ERROR(status))
				goto on_eio;
		}
		if (i_tblks > 0) {
			status = EFI_CALL(ed->blkio->ReadBlocks,
			    ed->blkio, ed->mediaid, lba + (i_nblks / blks),
			    ed->blkio->Media->BlockSize, iblk);
			if (EFI_ERROR(status))
				goto on_eio;
			memcpy(buf + (i_lblks + i_nblks) * DEV_BSIZE, iblk,
			    i_tblks * DEV_BSIZE);
		}
		break;
	case F_WRITE:
		if (ed->blkio->Media->ReadOnly)
			goto on_eio;
		/* XXX not yet */
		goto on_eio;
		break;
	}
	return (EFI_SUCCESS);

on_eio:
	return (status);
}

static int
efid_diskio(int rw, struct diskinfo *dip, u_int off, int nsect, void *buf)
{
	EFI_STATUS status;

	status = efid_io(rw, dip->efi_info, off, nsect, buf);

	return ((EFI_ERROR(status))? -1 : 0);
}

/*
 * Try to read the bsd label on the given BIOS device.
 */
static u_int
findopenbsd(efi_diskinfo_t ed, const char **err)
{
	EFI_STATUS status;
	struct dos_mbr mbr;
	struct dos_partition *dp;
	u_int mbroff = DOSBBSECTOR;
	u_int mbr_eoff = DOSBBSECTOR;	/* Offset of MBR extended partition. */
	int i, maxebr = DOS_MAXEBR, nextebr;

again:
	if (!maxebr--) {
		*err = "too many extended partitions";
		return (-1);
	}

	/* Read MBR */
	bzero(&mbr, sizeof(mbr));
	status = efid_io(F_READ, ed, mbroff, 1, &mbr);
	if (EFI_ERROR(status)) {
		*err = "Disk I/O Error";
		return (-1);
	}

	/* check mbr signature */
	if (mbr.dmbr_sign != DOSMBR_SIGNATURE) {
		*err = "bad MBR signature\n";
		return (-1);
	}

	/* Search for OpenBSD partition */
	nextebr = 0;
	for (i = 0; i < NDOSPART; i++) {
		dp = &mbr.dmbr_parts[i];
		if (!dp->dp_size)
			continue;
#ifdef BIOS_DEBUG
		if (debug)
			printf("found partition %u: "
			    "type %u (0x%x) offset %u (0x%x)\n",
			    (int)(dp - mbr.dmbr_parts),
			    dp->dp_typ, dp->dp_typ,
			    dp->dp_start, dp->dp_start);
#endif
		if (dp->dp_typ == DOSPTYP_OPENBSD) {
			if (dp->dp_start > (dp->dp_start + mbroff))
				continue;
			return (dp->dp_start + mbroff);
		}

		/*
		 * Record location of next ebr if and only if this is the first
		 * extended partition in this boot record!
		 */
		if (!nextebr && (dp->dp_typ == DOSPTYP_EXTEND ||
		    dp->dp_typ == DOSPTYP_EXTENDL)) {
			nextebr = dp->dp_start + mbr_eoff;
			if (nextebr < dp->dp_start)
				nextebr = (u_int)-1;
			if (mbr_eoff == DOSBBSECTOR)
				mbr_eoff = dp->dp_start;
		}

		if (dp->dp_typ == DOSPTYP_EFI) {
			uint64_t gptoff = findopenbsd_gpt(ed, err);
			if (gptoff > UINT_MAX ||
			    EFI_SECTOBLK(ed, gptoff) > UINT_MAX) {
				*err = "Paritition LBA > 2**32";
				return (-1);
			}
			return EFI_SECTOBLK(ed, gptoff);
		}
	}

	if (nextebr && nextebr != (u_int)-1) {
		mbroff = nextebr;
		goto again;
	}

	return (-1);
}

/* call this only if LBA1 == GPT */
static uint64_t
findopenbsd_gpt(efi_diskinfo_t ed, const char **err)
{
	EFI_STATUS		 status;
	struct			 gpt_header gh;
	int			 i, part;
	uint64_t		 lba;
	uint32_t		 orig_csum, new_csum;
	uint32_t		 ghpartsize, ghpartnum, ghpartspersec;
	const char		 openbsd_uuid_code[] = GPT_UUID_OPENBSD;
	static struct uuid	*openbsd_uuid = NULL, openbsd_uuid_space;
	static struct gpt_partition
				 gp[NGPTPARTITIONS];
	static u_char		 buf[4096];

	/* Prepare OpenBSD UUID */
	if (openbsd_uuid == NULL) {
		/* XXX: should be replaced by uuid_dec_be() */
		memcpy(&openbsd_uuid_space, openbsd_uuid_code,
		    sizeof(openbsd_uuid_space));
		openbsd_uuid_space.time_low =
		    betoh32(openbsd_uuid_space.time_low);
		openbsd_uuid_space.time_mid =
		    betoh16(openbsd_uuid_space.time_mid);
		openbsd_uuid_space.time_hi_and_version =
		    betoh16(openbsd_uuid_space.time_hi_and_version);

		openbsd_uuid = &openbsd_uuid_space;
	}

	if (EFI_BLKSPERSEC(ed) > 8) {
		*err = "disk sector > 4096 bytes\n";
		return (-1);
	}

	/* LBA1: GPT Header */
	lba = 1;
	status = efid_io(F_READ, ed, EFI_SECTOBLK(ed, lba), EFI_BLKSPERSEC(ed),
	    buf);
	if (EFI_ERROR(status)) {
		*err = "Disk I/O Error";
		return (-1);
	}
	memcpy(&gh, buf, sizeof(gh));

	/* Check signature */
	if (letoh64(gh.gh_sig) != GPTSIGNATURE) {
		*err = "bad GPT signature\n";
		return (-1);
	}

	/* Check checksum */
	orig_csum = gh.gh_csum;
	gh.gh_csum = 0;
	new_csum = crc32(0, (unsigned char *)&gh, letoh32(gh.gh_size));
	gh.gh_csum = orig_csum;
	if (letoh32(orig_csum) != new_csum) {
		*err = "bad GPT header checksum\n";
		return (1);
	}

	lba = letoh64(gh.gh_part_lba);
	ghpartsize = letoh32(gh.gh_part_size);
	ghpartspersec = ed->blkio->Media->BlockSize / ghpartsize;
	ghpartnum = letoh32(gh.gh_part_num);
	for (i = 0; i < (ghpartnum + ghpartspersec - 1) / ghpartspersec;
	    i++, lba++) {
		status = efid_io(F_READ, ed, EFI_SECTOBLK(ed, lba),
		    EFI_BLKSPERSEC(ed), buf);
		if (EFI_ERROR(status)) {
			*err = "Disk I/O Error";
			return (-1);
		}
		memcpy(gp + i * ghpartspersec, buf,
		    ghpartspersec * sizeof(struct gpt_partition));
	}
	new_csum = crc32(0, (unsigned char *)&gp, ghpartnum * ghpartsize);
	if (new_csum != letoh32(gh.gh_part_csum)) {
		*err = "bad GPT partitions checksum\n";
		return (-1);
	}

	for (part = 0; part < ghpartnum; part++) {
		if (memcmp(&gp[part].gp_type, openbsd_uuid,
		    sizeof(struct uuid)) == 0)
			return letoh64(gp[part].gp_lba_start);
	}

	return (-1);
}

const char *
efi_getdisklabel(efi_diskinfo_t ed, struct disklabel *label)
{
	u_int start = 0;
	char buf[DEV_BSIZE];
	const char *err = NULL;
	int error;

	/* Sanity check */
	/* XXX */

	start = findopenbsd(ed, &err);
	if (start == (u_int)-1) {
		if (err != NULL)
			return (err);
		return "no OpenBSD partition\n";
	}
	start = LABELSECTOR + start;

	/* Load BSD disklabel */
#ifdef BIOS_DEBUG
	if (debug)
		printf("loading disklabel @ %u\n", start);
#endif
	/* read disklabel */
	error = efid_io(F_READ, ed, start, 1, buf);

	if (error)
		return "failed to read disklabel";

	/* Fill in disklabel */
	return (getdisklabel(buf, label));
}

int
efiopen(struct open_file *f, ...)
{
#ifdef SOFTRAID
	struct sr_boot_volume *bv;
#endif
	register char *cp, **file;
	dev_t maj, unit, part;
	struct diskinfo *dip;
	int biosdev, devlen;
#if 0
	const char *st;
#endif
	va_list ap;
	char *dev;

	va_start(ap, f);
	cp = *(file = va_arg(ap, char **));
	va_end(ap);

#ifdef EFI_DEBUG
	if (debug)
		printf("%s\n", cp);
#endif

	f->f_devdata = NULL;

	/* Search for device specification. */
	dev = cp;
	if (cp[4] == ':')
		devlen = 2;
	else if (cp[5] == ':')
		devlen = 3;
	else
		return ENOENT;
	cp += devlen;

	/* Get unit. */
	if ('0' <= *cp && *cp <= '9')
		unit = *cp++ - '0';
	else {
		printf("Bad unit number\n");
		return EUNIT;
	}

	/* Get partition. */
	if ('a' <= *cp && *cp <= 'p')
		part = *cp++ - 'a';
	else {
		printf("Bad partition\n");
		return EPART;
	}

	/* Get filename. */
	cp++;	/* skip ':' */
	if (*cp != 0)
		*file = cp;
	else
		f->f_flags |= F_RAW;

#ifdef SOFTRAID
	/* Intercept softraid disks. */
	if (strncmp("sr", dev, 2) == 0) {

		/* Create a fake diskinfo for this softraid volume. */
		SLIST_FOREACH(bv, &sr_volumes, sbv_link)
			if (bv->sbv_unit == unit)
				break;
		if (bv == NULL) {
			printf("Unknown device: sr%d\n", unit);
			return EADAPT;
		}

		if (bv->sbv_level == 'C' && bv->sbv_keys == NULL)
			if (sr_crypto_decrypt_keys(bv) != 0)
				return EPERM;

		if (bv->sbv_diskinfo == NULL) {
			dip = alloc(sizeof(struct diskinfo));
			bzero(dip, sizeof(*dip));
			dip->diskio = efid_diskio;
			dip->strategy = efistrategy;
			bv->sbv_diskinfo = dip;
			dip->sr_vol = bv;
			dip->bios_info.flags |= BDI_BADLABEL;
		}

		dip = bv->sbv_diskinfo;

		if (dip->bios_info.flags & BDI_BADLABEL) {
			/* Attempt to read disklabel. */
			bv->sbv_part = 'c';
			if (sr_getdisklabel(bv, &dip->disklabel))
				return ERDLAB;
			dip->bios_info.flags &= ~BDI_BADLABEL;
		}

		bv->sbv_part = part + 'a';

		bootdev_dip = dip;
		f->f_devdata = dip;

		return 0;
	}
#endif
	for (maj = 0; maj < nbdevs &&
	    strncmp(dev, bdevs[maj], devlen); maj++);
	if (maj >= nbdevs) {
		printf("Unknown device: ");
		for (cp = *file; *cp != ':'; cp++)
			putchar(*cp);
		putchar('\n');
		return EADAPT;
	}

	biosdev = unit;
	switch (maj) {
	case 0:  /* wd */
	case 4:  /* sd */
	case 17: /* hd */
		biosdev |= 0x80;
		break;
	case 2:  /* fd */
		break;
	case 6:  /* cd */
		biosdev = bios_bootdev & 0xff;
		break;
	default:
		return ENXIO;
	}

	/* Find device */
	dip = dklookup(biosdev);
	if (dip == NULL)
		return ENXIO;
	bootdev_dip = dip;

	/* Fix up bootdev */
	{ dev_t bsd_dev;
		bsd_dev = dip->bios_info.bsd_dev;
		dip->bsddev = MAKEBOOTDEV(B_TYPE(bsd_dev), B_ADAPTOR(bsd_dev),
		    B_CONTROLLER(bsd_dev), unit, part);
		dip->bootdev = MAKEBOOTDEV(B_TYPE(bsd_dev), B_ADAPTOR(bsd_dev),
		    B_CONTROLLER(bsd_dev), B_UNIT(bsd_dev), part);
	}

#if 0
	dip->bios_info.bsd_dev = dip->bootdev;
	bootdev = dip->bootdev;
#endif

#ifdef EFI_DEBUG
	if (debug) {
		printf("BIOS geometry: heads=%u, s/t=%u; EDD=%d\n",
		    dip->bios_info.bios_heads, dip->bios_info.bios_sectors,
		    dip->bios_info.bios_edd);
	}
#endif

#if 0
/*
 * XXX In UEFI, media change can be detected by MediaID
 */
	/* Try for disklabel again (might be removable media) */
	if (dip->bios_info.flags & BDI_BADLABEL) {
		st = efi_getdisklabel(dip->efi_info, &dip->disklabel);
#ifdef EFI_DEBUG
		if (debug && st)
			printf("%s\n", st);
#endif
		if (!st) {
			dip->bios_info.flags &= ~BDI_BADLABEL;
			dip->bios_info.flags |= BDI_GOODLABEL;
		} else
			return ERDLAB;
	}
#endif
	f->f_devdata = dip;

	return 0;
}

int
efistrategy(void *devdata, int rw, daddr32_t blk, size_t size, void *buf,
    size_t *rsize)
{
	struct diskinfo *dip = (struct diskinfo *)devdata;
	u_int8_t error = 0;
	size_t nsect;

#ifdef SOFTRAID
	/* Intercept strategy for softraid volumes. */
	if (dip->sr_vol)
		return sr_strategy(dip->sr_vol, rw, blk, size, buf, rsize);
#endif
	nsect = (size + DEV_BSIZE - 1) / DEV_BSIZE;
	blk += dip->disklabel.d_partitions[B_PARTITION(dip->bsddev)].p_offset;

	if (blk < 0)
		error = EINVAL;
	else
		error = dip->diskio(rw, dip, blk, nsect, buf);

#ifdef EFI_DEBUG
	if (debug) {
		if (error != 0)
			printf("=0x%x(%s)", error, error);
		putchar('\n');
	}
#endif
	if (rsize != NULL)
		*rsize = nsect * DEV_BSIZE;

	return (error);
}

int
eficlose(struct open_file *f)
{
	f->f_devdata = NULL;

	return 0;
}

int
efiioctl(struct open_file *f, u_long cmd, void *data)
{

	return 0;
}

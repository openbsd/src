/*	$OpenBSD: efidev.c,v 1.2 2016/05/20 11:53:19 kettenis Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
 * Copyright (c) 2016 Mark Kettenis
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

#include <efi.h>
#include "eficall.h"

extern EFI_BOOT_SERVICES *BS;

extern int debug;

#include "disk.h"
#include "efidev.h"

#define EFI_BLKSPERSEC(_ed)	((_ed)->blkio->Media->BlockSize / DEV_BSIZE)
#define EFI_SECTOBLK(_ed, _n)	((_n) * EFI_BLKSPERSEC(_ed))

extern EFI_BLOCK_IO *disk;
struct diskinfo diskinfo;

static EFI_STATUS
		 efid_io(int, efi_diskinfo_t, u_int, int, void *);
static int	 efid_diskio(int, struct diskinfo *, u_int, int, void *);

static EFI_STATUS
efid_io(int rw, efi_diskinfo_t ed, u_int off, int nsect, void *buf)
{
	EFI_STATUS status = EFI_SUCCESS;
	EFI_PHYSICAL_ADDRESS addr;
	caddr_t data;

	if (ed->blkio->Media->BlockSize != DEV_BSIZE)
		return (EFI_UNSUPPORTED);

	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(nsect * DEV_BSIZE), &addr);
	if (EFI_ERROR(status))
		goto on_eio;
	data = (caddr_t)(uintptr_t)addr;

	switch (rw) {
	case F_READ:
		status = EFI_CALL(ed->blkio->ReadBlocks,
		    ed->blkio, ed->mediaid, off,
		    nsect * DEV_BSIZE, data);
		if (EFI_ERROR(status))
			goto on_eio;
		memcpy(buf, data, nsect * DEV_BSIZE);
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
	BS->FreePages(addr, EFI_SIZE_TO_PAGES(nsect * DEV_BSIZE));

	return (status);
}

static int
efid_diskio(int rw, struct diskinfo *dip, u_int off, int nsect, void *buf)
{
	EFI_STATUS status;

	status = efid_io(rw, &dip->ed, off, nsect, buf);

	return ((EFI_ERROR(status))? -1 : 0);
}

/*
 * Read disk label from the device.
 */
int
efi_getdisklabel(struct diskinfo *dip)
{
	char *msg;
	int sector;
	size_t rsize;
	struct disklabel *lp;
	unsigned char buf[DEV_BSIZE];

	/*
	 * Find OpenBSD Partition in DOS partition table.
	 */
	sector = 0;
	if (efistrategy(dip, F_READ, DOSBBSECTOR, DEV_BSIZE, buf, &rsize))
		return EOFFSET;

	if (*(u_int16_t *)&buf[DOSMBR_SIGNATURE_OFF] == DOSMBR_SIGNATURE) {
		int i;
		struct dos_partition *dp = (struct dos_partition *)buf;

		/*
		 * Lookup OpenBSD slice. If there is none, go ahead
		 * and try to read the disklabel off sector #0.
		 */

		memcpy(dp, &buf[DOSPARTOFF], NDOSPART * sizeof(*dp));
		for (i = 0; i < NDOSPART; i++) {
			if (dp[i].dp_typ == DOSPTYP_OPENBSD) {
				sector = letoh32(dp[i].dp_start);
				break;
			}
		}
	}

	if (efistrategy(dip, F_READ, sector + DOS_LABELSECTOR, DEV_BSIZE,
			buf, &rsize))
		return EOFFSET;

	if ((msg = getdisklabel(buf + LABELOFFSET, &dip->disklabel)))
		printf("sd%d: getdisklabel: %s\n", 0, msg);

	lp = &dip->disklabel;

	/* check partition */
	if ((dip->sc_part >= lp->d_npartitions) ||
	    (lp->d_partitions[dip->sc_part].p_fstype == FS_UNUSED)) {
		DPRINTF(("illegal partition\n"));
		return (EPART);
	}

	return (0);
}

int
efiopen(struct open_file *f, ...)
{
	struct diskinfo *dip = &diskinfo;
	va_list ap;
	u_int unit, part;
	int error;

	if (disk == NULL)
		return (ENXIO);

	va_start(ap, f);
	unit = va_arg(ap, u_int);
	part = va_arg(ap, u_int);
	va_end(ap);

	if (unit != 0)
		return (ENXIO);

	diskinfo.ed.blkio = disk;
	diskinfo.ed.mediaid = disk->Media->MediaId;
	diskinfo.sc_part = part;

	error = efi_getdisklabel(&diskinfo);
	if (error)
		return (error);

	f->f_devdata = dip;

	return 0;
}

int
efistrategy(void *devdata, int rw, daddr32_t blk, size_t size, void *buf,
    size_t *rsize)
{
	struct diskinfo *dip = (struct diskinfo *)devdata;
	int error = 0;
	size_t nsect;

	nsect = (size + DEV_BSIZE - 1) / DEV_BSIZE;
	blk += dip->disklabel.d_partitions[B_PARTITION(dip->sc_part)].p_offset;

	if (blk < 0)
		error = EINVAL;
	else
		error = efid_diskio(rw, dip, blk, nsect, buf);

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

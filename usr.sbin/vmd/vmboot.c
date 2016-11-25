/*	$OpenBSD: vmboot.c,v 1.2 2016/11/25 22:28:58 reyk Exp $	*/

/*
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/disklabel.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "vmd.h"
#include "vmboot.h"

int	 vmboot_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
off_t	 vmboot_findopenbsd(struct open_file *, off_t);
void	*vmboot_loadfile(struct open_file *, char *, size_t *);

/*
 * For ufs.c
 */

struct devsw vmboot_devsw = {
	.dv_name =	"vmboot",
	.dv_strategy =	vmboot_strategy,
	/* other fields are not needed */
};

struct open_file vmboot_file = {
	.f_dev =	&vmboot_devsw,
	.f_devdata =	 NULL
};

struct vmboot {
	int		 fd;
	off_t		 partoff;
};

int
vmboot_strategy(void *devdata, int rw,
    daddr32_t blk, size_t size, void *buf, size_t *rsize)
{
	struct vmboot	*vmboot = devdata;
	ssize_t		 rlen;

	if (vmboot->fd == -1)
		return (EIO);

	switch (rw) {
	case F_READ:
		rlen = pread(vmboot->fd, buf, size,
		    (blk + vmboot->partoff) * DEV_BSIZE);
		if (rlen == -1)
			return (errno);
		*rsize = (size_t)rlen;
		break;
	case F_WRITE:
		rlen = pwrite(vmboot->fd, buf, size,
		    (blk + vmboot->partoff) * DEV_BSIZE);
		if (rlen == -1)
			return (errno);
		*rsize = (size_t)rlen;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Based on findopenbsd() from biosdev.c that was partially written by me.
 */
off_t
vmboot_findopenbsd(struct open_file *f, off_t mbroff)
{
	struct dos_mbr		 mbr;
	struct dos_partition	*dp;
	off_t			 mbr_eoff = DOSBBSECTOR, nextebr;
	int			 ret, i;
	static int 		 maxebr = DOS_MAXEBR;
	size_t			 rsize;

	if (!maxebr--) {
		log_debug("%s: too many extended partitions", __func__);
		return (-1);
	}

	memset(&mbr, 0, sizeof(mbr));
	ret = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
	    mbroff, sizeof(mbr), &mbr, &rsize);
	if (ret != 0 || rsize != sizeof(mbr)) {
		log_debug("%s: failed to read MBR", __func__);
		return (-1);
	}

	if (mbr.dmbr_sign != DOSMBR_SIGNATURE) {
		log_debug("%s: bad MBR signature", __func__);
		return (-1);
	}

	/* Search for the first OpenBSD partition */
	nextebr = 0;
	for (i = 0; i < NDOSPART; i++) {
		dp = &mbr.dmbr_parts[i];
		if (!dp->dp_size)
			continue;

		if (dp->dp_typ == DOSPTYP_OPENBSD) {
			if (dp->dp_start > (dp->dp_start + mbroff))
				continue;
			return (dp->dp_start + mbroff);
		}

		if (!nextebr && (dp->dp_typ == DOSPTYP_EXTEND ||
		    dp->dp_typ == DOSPTYP_EXTENDL)) {
			nextebr = dp->dp_start + mbr_eoff;
			if (nextebr < dp->dp_start)
				nextebr = -1;
			if (mbr_eoff == DOSBBSECTOR)
				mbr_eoff = dp->dp_start;
		}
	}

	if (nextebr && nextebr != -1) {
		mbroff = nextebr;
		return (vmboot_findopenbsd(f, mbroff));
	}

	return (-1);
}

void *
vmboot_loadfile(struct open_file *f, char *file, size_t *size)
{
	char		*buf = NULL;
	struct stat	 st;
	size_t		 rsize;
	int		 ret;

	*size = 0;

	if ((ret = ufs_open(file, f)) != 0) {
		log_debug("%s: failed to open hd0a:%s", __func__, file);
		return (NULL);
	}

	if ((ret = ufs_stat(f, &st)) != 0) {
		log_debug("%s: failed to stat hd0a:%s", __func__, file);
		goto done;
	}

	if ((buf = calloc(1, roundup(st.st_size, DEV_BSIZE))) == NULL) {
		log_debug("%s: failed to allocate buffer", __func__);
		goto done;
	}

	if ((ret = ufs_read(f, buf, st.st_size, &rsize)) != 0) {
		log_debug("%s: failed to read hd0a:%s", __func__, file);
		goto done;
	}

	*size = st.st_size;
 done:
	ufs_close(f);
	return (buf);
}

FILE *
vmboot_open(int kernel_fd, int disk_fd, void **boot)
{
	char		 file[PATH_MAX];
	char		*buf = NULL;
	struct vmboot	 vmboot;
	size_t		 size;
	FILE		*fp = NULL;

	*boot = NULL;

	/* First open kernel directly if specified by fd */
	if (kernel_fd != -1)
		return (fdopen(kernel_fd, "r"));

	if (disk_fd == -1)
		return (NULL);

	memset(&vmboot, 0, sizeof(vmboot));
	vmboot.fd = disk_fd;
	vmboot_file.f_devdata = &vmboot;

	/* XXX try and parse hd0a:/etc/boot.conf */
	strlcpy(file, VM_DEFAULT_KERNEL, sizeof(file));

	if ((vmboot.partoff = vmboot_findopenbsd(&vmboot_file, 0)) == -1) {
		log_debug("%s: could not find openbsd partition", __func__);
		return (NULL);
	}

	if ((buf = vmboot_loadfile(&vmboot_file, file, &size)) == NULL)
		return (NULL);
	*boot = buf;

	if ((fp = fmemopen(buf, size, "r")) == NULL) {
		log_debug("%s: failed to open memory stream", __func__);
		free(buf);
		*boot = NULL;
	}

	return (fp);
}

void
vmboot_close(FILE *fp, void *boot)
{
	fclose(fp);
	free(boot);
}

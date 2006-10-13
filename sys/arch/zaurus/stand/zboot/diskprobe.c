/*	$OpenBSD: diskprobe.c,v 1.3 2006/10/13 00:00:55 krw Exp $	*/

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

/* We want the disk type names from disklabel.h */
#undef DKTYPENAMES

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <stand/boot/bootarg.h>
#if 0
#include <machine/biosvar.h>
#endif
#include <lib/libz/zlib.h>
#include "disk.h"
#if 0
#include "biosdev.h"
#endif
#include "libsa.h"

#define MAX_CKSUMLEN MAXBSIZE / DEV_BSIZE	/* Max # of blks to cksum */

/* Disk spin-up wait timeout. */
static u_int timeout = 10;

/* Local Prototypes */
static void hardprobe(void);

/* List of disk devices we found/probed */
struct disklist_lh disklist;

/*
 * Probe for all hard disks.
 */
static void
hardprobe(void)
{
	struct diskinfo *dip;
	int i, order[] = { 0x80, 0x82 }; /* XXX probe disks in this order */
	u_int bsdunit, type;
	u_int scsi = 0, ide = 0;
	u_int disk = 0;

	/* Hard disks */
	for (i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
		dip = alloc(sizeof(struct diskinfo));
		bzero(dip, sizeof(*dip));

		if (bios_getdiskinfo(order[i], &dip->bios_info) != NULL) {
			free(dip, 0);
			continue;
		}

		printf("hd%u", disk++);

		/* Try to find the label, to figure out device type. */
		if (bios_getdisklabel(&dip->bios_info, &dip->disklabel)
		    == NULL) {
			printf("*");
			bsdunit = ide++;
			type = 0;	/* XXX let it be IDE */
		} else {
			/* Best guess */
			switch (dip->disklabel.d_type) {
			case DTYPE_SCSI:
				type = 4;
				bsdunit = scsi++;
				dip->bios_info.flags |= BDI_GOODLABEL;
				break;

			case DTYPE_ESDI:
			case DTYPE_ST506:
				type = 0;
				bsdunit = ide++;
				dip->bios_info.flags |= BDI_GOODLABEL;
				break;

			default:
				dip->bios_info.flags |= BDI_BADLABEL;
				type = 0;	/* XXX Suggest IDE */
				bsdunit = ide++;
			}
		}

		dip->bios_info.checksum = 0; /* just in case */
		/* Fill out best we can. */
		dip->bios_info.bsd_dev =
		    MAKEBOOTDEV(type, 0, 0, bsdunit, RAW_PART);

		/* Add to queue of disks. */
		TAILQ_INSERT_TAIL(&disklist, dip, list);

		printf(" ");
	}
}

/* Probe for all BIOS supported disks */
u_int32_t bios_cksumlen;
void
diskprobe(void)
{
	struct diskinfo *dip;
	int i;

	/* These get passed to kernel */
	bios_diskinfo_t *bios_diskinfo;

	/* Init stuff */
	TAILQ_INIT(&disklist);

	/* Do probes */
	hardprobe();

#if 0
	/* Checksumming of hard disks */
	for (i = 0; disksum(i++) && i < MAX_CKSUMLEN; )
		;
	bios_cksumlen = i;

	/* Get space for passing bios_diskinfo stuff to kernel */
	for (i = 0, dip = TAILQ_FIRST(&disklist); dip;
	    dip = TAILQ_NEXT(dip, list))
		i++;
	bios_diskinfo = alloc(++i * sizeof(bios_diskinfo_t));

	/* Copy out the bios_diskinfo stuff */
	for (i = 0, dip = TAILQ_FIRST(&disklist); dip;
	    dip = TAILQ_NEXT(dip, list))
		bios_diskinfo[i++] = dip->bios_info;

	bios_diskinfo[i++].bios_number = -1;
	/* Register for kernel use */
	addbootarg(BOOTARG_CKSUMLEN, sizeof(u_int32_t), &bios_cksumlen);
	addbootarg(BOOTARG_DISKINFO, i * sizeof(bios_diskinfo_t),
	    bios_diskinfo);
#endif
}

/*
 * Find info on the disk given by major + unit number.
 */
struct diskinfo *
dkdevice(dev_t maj, dev_t unit)
{
	struct diskinfo *dip;

	for (dip = TAILQ_FIRST(&disklist); dip;
	     dip = TAILQ_NEXT(dip, list)) {
		/* XXX skip non-matching entries according to maj. */

		if (unit-- == 0)
			return dip;
	}

	return NULL;
}

/*
 * Find the Linux device path that corresponds to the given "BIOS" disk,
 * where 0x80 corresponds to /dev/hda, 0x81 to /dev/hdb, and so on.
 */
void
bios_devpath(int dev, int part, char *p)
{
	*p++ = '/';
	*p++ = 'd';
	*p++ = 'e';
	*p++ = 'v';
	*p++ = '/';
	if ((dev & 0x80) != 0)
		*p++ = 'h';
	else
		*p++ = 'f';
	*p++ = 'd';
	*p++ = 'a' + (dev & 0x7f);
	if (part != -1)
		*p++ = '1' + part;
	*p = '\0';
}

/*
 * Fill out a bios_diskinfo_t for this device.
 */
char *
bios_getdiskinfo(int dev, bios_diskinfo_t *bdi)
{
	static char path[PATH_MAX];
	struct linux_stat sb;
	char *p;

	bzero(bdi, sizeof *bdi);
	bdi->bios_number = -1;

	bios_devpath(dev, -1, path);

	if (ustat(path, &sb) != 0)
		return "no device node";

	bdi->bios_number = dev;

	if (bios_getdospart(bdi) < 0)
		return "no OpenBSD partition";

	return 0;
}

int
bios_getdospart(bios_diskinfo_t *bdi)
{
	char path[PATH_MAX];
	char buf[DEV_BSIZE];
	struct dos_partition *dp;
	int fd;
	u_int part;
	size_t rsize;

	bios_devpath(bdi->bios_number, -1, path);

	/*
	 * Give disk devices some time to become ready when the first open
	 * fails.  Even when open succeeds the disk is sometimes not ready.
	 */
	if ((fd = uopen(path, O_RDONLY)) == -1 && errno == ENXIO) {
		int t;

		while (fd == -1 && timeout > 0) {
			timeout--;
			sleep(1);
			fd = uopen(path, O_RDONLY);
		}
		if (fd != -1)
			sleep(2);
	}
	if (fd == -1)
		return -1;

	/* Read the disk's MBR. */
	if (unixstrategy((void *)fd, F_READ, DOSBBSECTOR,
	    DEV_BSIZE, buf, &rsize) != 0 || rsize != DEV_BSIZE) {
		uclose(fd);
		errno = EIO;
		return -1;
	}

	/* Find OpenBSD primary partition in the disk's MBR. */
	dp = (struct dos_partition *)&buf[DOSPARTOFF];
	for (part = 0; part < NDOSPART; part++)
		if (dp[part].dp_typ == DOSPTYP_OPENBSD)
			break;
	if (part == NDOSPART) {
		uclose(fd);
		errno = ERDLAB;
		return -1;
	}
	uclose(fd);

	return part;
}

char *
bios_getdisklabel(bios_diskinfo_t *bdi, struct disklabel *label)
{
	char path[PATH_MAX];
	char buf[DEV_BSIZE];
	int part;
	int fd;
	size_t rsize;

	part = bios_getdospart(bdi);
	if (part < 0)
		return "no OpenBSD partition";

	bios_devpath(bdi->bios_number, part, path);

	/* Test if the OpenBSD partition has a valid disklabel. */
	if ((fd = uopen(path, O_RDONLY)) != -1) {
		char *msg = "failed to read disklabel";

		if (unixstrategy((void *)fd, F_READ, LABELSECTOR,
		    DEV_BSIZE, buf, &rsize) == 0 && rsize == DEV_BSIZE)
			msg = getdisklabel(buf, label);
		uclose(fd);
		/* Don't wait for other disks if this label is ok. */
		if (msg == NULL)
			timeout = 0;
		return (msg);
	}

	return "failed to open partition";
}

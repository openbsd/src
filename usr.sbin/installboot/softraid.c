/*	$OpenBSD: softraid.c,v 1.3 2015/01/16 00:05:12 deraadt Exp $	*/
/*
 * Copyright (c) 2012 Joel Sing <jsing@openbsd.org>
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

#include <sys/dkio.h>
#include <sys/ioctl.h>

#include <dev/biovar.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "installboot.h"

static int sr_volume(int, char *, int *, int *);

void
sr_installboot(int devfd, char *dev)
{
	int	vol = -1, ndisks = 0, disk;

	/* Use the normal process if this is not a softraid volume. */
	if (!sr_volume(devfd, dev, &vol, &ndisks)) {
		md_installboot(devfd, dev);
		return;
	}
	
	/* Install boot loader in softraid volume. */
	sr_install_bootldr(devfd, dev);

	/* Install boot block on each disk that is part of this volume. */
	for (disk = 0; disk < ndisks; disk++)
		sr_install_bootblk(devfd, vol, disk);
}

int
sr_volume(int devfd, char *dev, int *vol, int *disks)
{
	struct	bioc_inq bi;
	struct	bioc_vol bv;
	int	i;

	/*
	 * Determine if the given device is a softraid volume.
	 */

	/* Get volume information. */
	memset(&bi, 0, sizeof(bi));
	if (ioctl(devfd, BIOCINQ, &bi) == -1)
		return 0;

	/* XXX - softraid volumes will always have a "softraid0" controller. */
	if (strncmp(bi.bi_dev, "softraid0", sizeof("softraid0")))
		return 0;

	/*
	 * XXX - this only works with the real disk name (e.g. sd0) - this
	 * should be extracted from the device name, or better yet, fixed in
	 * the softraid ioctl.
	 */
	/* Locate specific softraid volume. */
	for (i = 0; i < bi.bi_novol; i++) {
		memset(&bv, 0, sizeof(bv));
		bv.bv_volid = i;
		if (ioctl(devfd, BIOCVOL, &bv) == -1)
			err(1, "BIOCVOL");

		if (strncmp(dev, bv.bv_dev, sizeof(bv.bv_dev)) == 0) {
			*vol = i;
			*disks = bv.bv_nodisk;
			break;
		}
	}

	if (verbose)
		fprintf(stderr, "%s: softraid volume with %i disk(s)\n",
		    dev, *disks);

	return 1;
}

/*	$OpenBSD: conf.c,v 1.2 2013/12/28 02:53:04 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Jasper Lievisse Adriaanse <jasper@openbsd.org>
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
#include <dev/cons.h>

#include "libsa.h"
#include <lib/libsa/ufs.h>
#include <lib/libsa/cd9660.h>

const char version[] = "0.2";

/*
 * Device configuration
 */
struct devsw devsw[] = {
	/* ATA storage device */
	/* XXX */
	{ "wd",		NULL, NULL, NULL, noioctl }
};
int ndevs = nitems(devsw);

/*
 * Filesystem configuration
 */
struct fs_ops file_system[] = {
	/* ufs filesystem */
	{	ufs_open,	ufs_close,	ufs_read,	ufs_write,
		ufs_seek,	ufs_stat,	ufs_readdir	},
	/* cd9660 filesystem - in case a cd image is dd'ed on non USB media */
	{	cd9660_open,	cd9660_close,	cd9660_read,	cd9660_write,
		cd9660_seek,	cd9660_stat,	cd9660_readdir	}
};
int nfsys = nitems(file_system);

/*
 * Console configuration
 */
struct consdev constab[] = {
	{ cn30xxuartcnprobe, cn30xxuartcninit, cn30xxuartcngetc, cn30xxuartcnputc },
	{ NULL }
};
struct consdev *cn_tab;

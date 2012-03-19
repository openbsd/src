/*	$OpenBSD: netio.c,v 1.1 2012/03/19 17:38:31 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
#include <lib/libkern/libkern.h>
#include <stand.h>

#include <mips64/arcbios.h>

int
netstrategy(void *devdata, int rw, daddr32_t bn, size_t reqcnt, void *addr,
    size_t *cnt)
{
	long fd = (long)devdata;
	long result;
	int rc;

	rc = Bios_Read(fd, addr, reqcnt, &result);
	if (rc != 0)
		return (EIO);

	*cnt = result;
	return 0;
}

int
netopen(struct open_file *f, ...)
{
	char *path;
	long fd;
	int rc;
	va_list ap;

	va_start(ap, f);
	path = va_arg(ap, char *);
	va_end(ap);

	/* to match netfs.c filename buffers... */
	if (strlen(path) > 128 - 1)
		return ENAMETOOLONG;

	rc = Bios_Open(path, 0, &fd);
	if (rc != 0) {
		switch (rc) {
		case arc_EACCES:
			return EACCES;
		case arc_EISDIR:
			return EISDIR;
		case arc_ENOENT:
			return ENOENT;
		default:
			return ENXIO;
		}
	}

	f->f_devdata = (void *)fd;

	return 0;
}

int
netclose(struct open_file *f)
{
	long fd = (long)f->f_devdata;

	(void)Bios_Close(fd);
	return 0;
}

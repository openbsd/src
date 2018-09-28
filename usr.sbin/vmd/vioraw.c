/*	$OpenBSD: vioraw.c,v 1.3 2018/09/28 12:35:32 reyk Exp $	*/
/*
 * Copyright (c) 2018 Ori Bernstein <ori@eigenstate.org>
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

#include <sys/types.h>

#include <machine/vmmvar.h>
#include <dev/pci/pcireg.h>

#include <stdlib.h>
#include <unistd.h>

#include "vmd.h"
#include "vmm.h"
#include "virtio.h"

static ssize_t
raw_pread(void *file, char *buf, size_t len, off_t off)
{
	return pread(*(int *)file, buf, len, off);
}

static ssize_t
raw_pwrite(void *file, char *buf, size_t len, off_t off)
{
	return pwrite(*(int *)file, buf, len, off);
}

static void
raw_close(void *file, int stayopen)
{
	if (!stayopen)
		close(*(int *)file);
	free(file);
}

/*
 * Initializes a raw disk image backing file from an fd.
 * Stores the number of 512 byte sectors in *szp,
 * returning -1 for error, 0 for success.
 */
int
virtio_init_raw(struct virtio_backing *file, off_t *szp, int fd)
{
	off_t sz;
	int *fdp;

	sz = lseek(fd, 0, SEEK_END);
	if (sz == -1)
		return -1;

	fdp = malloc(sizeof(int));
	if (!fdp)
		return -1;
	*fdp = fd;
	file->p = fdp;
	file->pread = raw_pread;
	file->pwrite = raw_pwrite;
	file->close = raw_close;
	*szp = sz;
	return 0;
}


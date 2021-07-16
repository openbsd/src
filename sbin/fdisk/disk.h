/*	$OpenBSD: disk.h,v 1.30 2021/07/16 13:26:04 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#ifndef _DISK_H
#define _DISK_H

struct disk {
	struct prt	 dk_bootprt;
	char		*dk_name;
	int		 dk_fd;
	uint32_t	 dk_cylinders;
	uint32_t	 dk_heads;
	uint32_t	 dk_sectors;
	uint32_t	 dk_size;
};

/* Align partition starts/sizes on 32K-byte boundaries. */
#define	BLOCKALIGNMENT	64

void		 DISK_open(const char *, const int);
void		 DISK_printgeometry(const char *);
char		*DISK_readsector(const uint64_t);
int		 DISK_writesector(const char *, const uint64_t);

extern struct disk		disk;
extern struct disklabel		dl;

#endif /* _DISK_H */

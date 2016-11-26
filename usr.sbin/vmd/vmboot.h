/*	$OpenBSD: vmboot.h,v 1.2 2016/11/26 20:03:42 reyk Exp $	*/

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stdarg.h>
#include <sys/stdint.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef VMBOOT_H
#define VMBOOT_H

#define	F_READ		1
#define	F_WRITE		2

struct open_file;

struct fs_ops {
	int	(*open)(char *, struct open_file *);
	int	(*close)(struct open_file *);
	int	(*read)(struct open_file *, void *, size_t, size_t *);
	int	(*write)(struct open_file *, void *, size_t, size_t *);
	off_t	(*seek)(struct open_file *, off_t, int);
	int	(*stat)(struct open_file *, struct stat *);
	int	(*readdir)(struct open_file *, char *);
};

struct devsw {
	char	*dv_name;
	int	(*dv_strategy)(void *, int, daddr32_t, size_t,
	    void *, size_t *);
};

struct open_file {
	int		f_flags;
	struct devsw	*f_dev;
	void		*f_devdata;
	struct fs_ops	*f_ops;
	void		*f_fsdata;
	off_t		f_offset;
};

struct disklabel;

u_int	 dkcksum(struct disklabel *);
char	*getdisklabel(char *, struct disklabel *);

int	ufs_open(char *, struct open_file *);
int	ufs_close(struct open_file *);
int	ufs_read(struct open_file *, void *, size_t, size_t *);
int	ufs_write(struct open_file *, void *, size_t, size_t *);
off_t	ufs_seek(struct open_file *, off_t offset, int);
int	ufs_stat(struct open_file *, struct stat *);
int	ufs_readdir(struct open_file *, char *);

#endif /* VMBOOT_H */

/*	$NetBSD: stand.h,v 1.13 1996/01/13 22:25:42 leo Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stand.h	8.1 (Berkeley) 6/11/93
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include "saioctl.h"
#include "saerrno.h"

#ifndef NULL
#define	NULL	0
#endif

struct open_file;

/*
 * This structure is used to define file system operations in a file system
 * independent way.
 */
struct fs_ops {
	int	(*open) __P((char *path, struct open_file *f));
	int	(*close) __P((struct open_file *f));
	int	(*read) __P((struct open_file *f, void *buf,
			     size_t size, size_t *resid));
	int	(*write) __P((struct open_file *f, void *buf,
			     size_t size, size_t *resid));
	off_t	(*seek) __P((struct open_file *f, off_t offset, int where));
	int	(*stat) __P((struct open_file *f, struct stat *sb));
};

extern struct fs_ops file_system[];
extern int nfsys;

/* where values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

/* Device switch */
struct devsw {
	char	*dv_name;
	int	(*dv_strategy) __P((void *devdata, int rw,
				    daddr_t blk, size_t size,
				    void *buf, size_t *rsize));
	int	(*dv_open) __P((struct open_file *f, ...));
	int	(*dv_close) __P((struct open_file *f));
	int	(*dv_ioctl) __P((struct open_file *f, u_long cmd, void *data));
};

extern struct devsw devsw[];	/* device array */
extern int ndevs;		/* number of elements in devsw[] */

struct open_file {
	int		f_flags;	/* see F_* below */
	struct devsw	*f_dev;		/* pointer to device operations */
	void		*f_devdata;	/* device specific data */
	struct fs_ops	*f_ops;		/* pointer to file system operations */
	void		*f_fsdata;	/* file system specific data */
};

#define	SOPEN_MAX	4
extern struct open_file files[];

/* f_flags values */
#define	F_READ		0x0001	/* file opened for reading */
#define	F_WRITE		0x0002	/* file opened for writing */
#define	F_RAW		0x0004	/* raw device open - no file system */
#define F_NODEV		0x0008	/* network open - no device */

#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define tolower(c)	((c) - 'A' + 'a')
#define isspace(c)	((c) == ' ' || (c) == '\t')
#define isdigit(c)	((c) >= '0' && (c) <= '9')

int	devopen __P((struct open_file *, const char *, char **));
void	*alloc __P((unsigned int));
void	free __P((void *, unsigned int));
struct	disklabel;
char	*getdisklabel __P((const char *, struct disklabel *));
int	dkcksum __P((struct disklabel *));

void	printf __P((const char *, ...));
void	sprintf __P((char *, const char *, ...));
void	twiddle __P((void));
void	gets __P((char *));
__dead void	panic __P((const char *, ...)) __attribute__((noreturn));
__dead void	_rtt __P((void)) __attribute__((noreturn));
void	bcopy __P((const void *, void *, size_t));
void	*memcpy __P((void *, const void *, size_t));
void	exec __P((char *, char *, int));
int	open __P((const char *, int));
int	close __P((int));
void	closeall __P((void));
ssize_t	read __P((int, void *, size_t));
ssize_t	write __P((int, void *, size_t));
    
int	nodev __P((void));
int	noioctl __P((struct open_file *, u_long, void *));
void	nullsys __P((void));

int	null_open __P((char *path, struct open_file *f));
int	null_close __P((struct open_file *f));
ssize_t	null_read __P((struct open_file *f, void *buf,
			size_t size, size_t *resid));
ssize_t	null_write __P((struct open_file *f, void *buf,
			size_t size, size_t *resid));
off_t	null_seek __P((struct open_file *f, off_t offset, int where));
int	null_stat __P((struct open_file *f, struct stat *sb));

/* Machine dependent functions */
void	machdep_start __P((char *, int, char *, char *, char *));
int	getchar __P((void));
void	putchar __P((int));    

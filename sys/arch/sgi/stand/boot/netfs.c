/*	$OpenBSD: netfs.c,v 1.2 2016/03/14 23:08:05 krw Exp $	*/

/*-
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	TFTP file system.
 */

#include <sys/param.h>
#include <lib/libkern/libkern.h>
#include <stand.h>

#include <sys/stat.h>

#include "netfs.h"

/*
 * In-core open file.
 */
struct tftp_file {
	char 		filename[128];
	off_t		f_seekp;	/* seek pointer */
	char		*f_buf;		/* buffer for data block */
	off_t		f_off;		/* index into buffer for data block */
	daddr32_t	f_buf_blkno;	/* block number of data block */
	size_t		f_buf_size;
};

#define TFTP_BLOCK_SHIFT	9
#define TFTP_BLOCK_SIZE		(1<<TFTP_BLOCK_SHIFT) /* 512 by tftp convention */
#define TFTP_BLOCK_NO(x)	((x >> TFTP_BLOCK_SHIFT) + 1)
#define TFTP_BLOCK_OFF(x)	(x % TFTP_BLOCK_SIZE)

static int	tftp_read_file(struct open_file *, char **, size_t *);

/*
 * Read a portion of a file into an internal buffer.  Return
 * the location in the buffer and the amount in the buffer.
 */

char	tftp_buf[TFTP_BLOCK_SIZE];	/* static */
struct	tftp_file tftp_ctrl;

static int
tftp_read_file(f, buf_p, size_p)
	struct open_file *f;
	char **buf_p;		/* out */
	size_t *size_p;		/* out */
{
	struct tftp_file *fp = (struct tftp_file *)f->f_fsdata;
	long off;
	daddr32_t file_block;
	size_t block_size;
	int i, rc;

	off = TFTP_BLOCK_OFF(fp->f_seekp);
	file_block = TFTP_BLOCK_NO(fp->f_seekp);
	block_size = TFTP_BLOCK_SIZE;

	if (file_block == fp->f_buf_blkno + 1) {
		/*
		 * Normal, incremental block transfer.
		 */
		rc = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
			file_block, block_size, fp->f_buf, &fp->f_buf_size);
		if (rc)
			return (rc);
		if (!(file_block % 4)) /* twiddle every 4 blocks */
			twiddle();
		fp->f_buf_blkno = file_block;
	} else if (file_block > fp->f_buf_blkno + 1) {
		/*
		 * Read ahead to the requested block;  If we need
		 * those we skipped, see below.
		 */
		for (i = (fp->f_buf_blkno + 1); i <= file_block; i++) {
			rc = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
				i, block_size, fp->f_buf, &fp->f_buf_size);
			if (rc)
				return (rc);
		}
		fp->f_buf_blkno = file_block;
	} else if (file_block < fp->f_buf_blkno) {
		/*
		 * Uh oh...  We can't rewind.  Reopen the file
		 * and start again.
		 */
		char filename[128];

		strlcpy(filename, fp->filename, sizeof filename);
                netfs_close(f);
                netfs_open(filename, f);

		/* restore f_seekp reset by netfs_open() */
		fp->f_seekp = (file_block - 1) * TFTP_BLOCK_SIZE + off;
		for (i = 1; i <= file_block; i++) {
			rc = (f->f_dev->dv_strategy)(f->f_devdata, F_READ,
				i, block_size, fp->f_buf, &fp->f_buf_size);
			if (rc)
				return (rc);
		}
		fp->f_buf_blkno = file_block;
	}

	/*
	 * Return address of byte in buffer corresponding to
	 * offset, and size of remainder of buffer after that
	 * byte.
	 */
	*buf_p = fp->f_buf + off;
	*size_p = fp->f_buf_size - off;

	/*
	 * But truncate buffer at end of file.
	 */
	if (fp->f_buf_size > block_size){
		twiddle();
		return(EIO);
	}


	return (0);
}

/*
 * Open a file.
 */
int
netfs_open(path, f)
	char *path;
	struct open_file *f;
{
	struct tftp_file *fp;
	int rc = 0;
extern int netstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
	
	if (f->f_dev->dv_strategy != netstrategy)
		return EINVAL;

	/* locate file system specific data structure and zero it.*/
	fp = &tftp_ctrl;
	bzero(fp, sizeof(struct tftp_file));
	f->f_fsdata = (void *)fp;
	fp->f_seekp = 0;
	fp->f_buf = tftp_buf;
	bzero(fp->f_buf, TFTP_BLOCK_SIZE);
	fp->f_buf_size = 0;

        strlcpy(fp->filename, path, sizeof fp->filename);
	
	twiddle();
        rc = (f->f_dev->dv_open)(f, path);
	return (rc);
}

int
netfs_close(f)
	struct open_file *f;
{
	struct tftp_file *fp = (struct tftp_file *)f->f_fsdata;

	fp->f_buf = NULL;
	f->f_fsdata = NULL;
        (f->f_dev->dv_close)(f);
	return (0);
}

/*
 * Copy a portion of a file into kernel memory.
 * Cross block boundaries when necessary.
 */
int
netfs_read(f, start, size, resid)
	struct open_file *f;
	void *start;
	size_t size;
	size_t *resid;	/* out */
{
	struct tftp_file *fp = (struct tftp_file *)f->f_fsdata;
	size_t csize;
	char *buf;
	size_t buf_size;
	int rc = 0;
	char *addr = start;

	while (size != 0) {
		rc = tftp_read_file(f, &buf, &buf_size);
		if (rc)
			break;

		csize = size;
		if (csize > buf_size)
			csize = buf_size;

		bcopy(buf, addr, csize);

		fp->f_seekp += csize;
		addr += csize;
		size -= csize;
	}
	if (resid)
		*resid = size;
	return (rc);
}

/*
 * Not implemented.
 */
int
netfs_write(f, start, size, resid)
	struct open_file *f;
	void *start;
	size_t size;
	size_t *resid;	/* out */
{

	return (EROFS);
}

/*
 * We only see forward.  We can't rewind.
 */
off_t
netfs_seek(f, offset, where)
	struct open_file *f;
	off_t offset;
	int where;
{
	struct tftp_file *fp = (struct tftp_file *)f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		fp->f_seekp = offset;
		break;
	case SEEK_CUR:
		fp->f_seekp += offset;
		break;
	case SEEK_END:
		errno = EIO;
		return (-1);
		break;
	default:
		return (-1);
	}
	return (fp->f_seekp);
}

int
netfs_stat(f, sb)
	struct open_file *f;
	struct stat *sb;
{
	return EIO;
}

#ifndef NO_READDIR
int
netfs_readdir (struct open_file *f, char *name)
{
	return EIO;
}
#endif


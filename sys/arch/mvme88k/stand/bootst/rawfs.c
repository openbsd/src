/*	$OpenBSD: rawfs.c,v 1.6 2013/04/14 19:05:19 miod Exp $ */

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Raw file system - for stream devices like tapes.
 * No random access, only sequential read allowed.
 * This exists only to allow upper level code to be
 * shielded from the fact that the device must be
 * read only with whole block position and size.
 */

#include <sys/param.h>
#include <stand.h>
#include <rawfs.h>

/*
 * Must be a multiple of MVMEPROM_BLOCK_SIZE, and at least as large as
 * Z_BUFSIZE from libsa cread.c to be able to correctly emulate forward
 * seek.
 */
#define	RAWFS_BSIZE	4096

/*
 * In-core open file.
 */
struct cfile {
	off_t		fs_bufpos;	/* file position of fs_buf */
	int		fs_len;		/* amount left in f_buf */
	char *		fs_ptr;		/* read pointer into f_buf */
	char		fs_buf[RAWFS_BSIZE];
};

static int rawfs_get_block(struct open_file *);
static off_t rawfs_get_pos(struct cfile *);

int
rawfs_open(char *path, struct open_file *f)
{
	struct cfile *fs;

	/*
	 * The actual PROM driver has already been opened.
	 * Just allocate the I/O buffer, etc.
	 */
	fs = alloc(sizeof(struct cfile));
	fs->fs_bufpos = 0;
	fs->fs_len = 0;	
	fs->fs_ptr = NULL;	/* nothing read yet */

	f->f_fsdata = fs;
	return (0);
}

int
rawfs_close(struct open_file *f)
{
	struct cfile *fs;

	fs = (struct cfile *) f->f_fsdata;
	f->f_fsdata = NULL;

	if (fs != NULL)
		free(fs, sizeof(*fs));

	return (0);
}

int
rawfs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct cfile *fs = (struct cfile *)f->f_fsdata;
	char *addr = start;
	int error = 0;
	size_t csize;

	while (size != 0) {
		if (fs->fs_len == 0)
			if ((error = rawfs_get_block(f)) != 0)
				break;

		if (fs->fs_len <= 0)
			break;	/* EOF */

		csize = size;
		if (csize > fs->fs_len)
			csize = fs->fs_len;

		bcopy(fs->fs_ptr, addr, csize);
		fs->fs_ptr += csize;
		fs->fs_len -= csize;
		addr += csize;
		size -= csize;
	}
	if (resid)
		*resid = size;
	return (error);
}

int
rawfs_write(struct open_file *f, void *start, size_t size, size_t *resid)
{
	return (EROFS);
}

off_t
rawfs_seek(struct open_file *f, off_t offset, int where)
{
	struct cfile *fs = (struct cfile *)f->f_fsdata;
	off_t oldpos, basepos;
	int error;

	oldpos = rawfs_get_pos(fs);
	switch (where) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += oldpos;
		break;
	default:
	case SEEK_END:
		return -1;
	}

	basepos = (offset / RAWFS_BSIZE) * RAWFS_BSIZE;
	offset %= RAWFS_BSIZE;

	/* backward seek are not possible */
	if (basepos < fs->fs_bufpos) {
		errno = EIO;
		return -1;
	}

	if (basepos == fs->fs_bufpos) {
		/* if we seek before the first read... */
		if (fs->fs_ptr == NULL) {
			if ((error = rawfs_get_block(f)) != 0) {
				errno = error;
				return -1;
			}
		}
		/* rewind to start of the buffer */
		fs->fs_len += (fs->fs_ptr - fs->fs_buf);
		fs->fs_ptr = fs->fs_buf;
	} else {
		while (basepos != fs->fs_bufpos)
			if ((error = rawfs_get_block(f)) != 0) {
				errno = error;
				return -1;
			}
	}
	/* now move forward within the buffer */
	if (offset > fs->fs_len)
		offset = fs->fs_len;	/* EOF */
	fs->fs_len -= offset;
	fs->fs_ptr += offset;

	return rawfs_get_pos(fs);
}

int
rawfs_stat(struct open_file *f, struct stat *sb)
{
	return (EFTYPE);
}

/*
 * Read a block from the underlying stream device
 * (In our case, a tape drive.)
 */
static int
rawfs_get_block(struct open_file *f)
{
	struct cfile *fs;
	int error;
	size_t len;
	off_t readpos;

	fs = (struct cfile *)f->f_fsdata;

	twiddle();
	if (fs->fs_ptr != NULL)
		readpos = fs->fs_bufpos + RAWFS_BSIZE;
	else
		readpos = fs->fs_bufpos;	/* first read */
	error = f->f_dev->dv_strategy(f->f_devdata, F_READ,
		readpos / DEV_BSIZE, RAWFS_BSIZE, fs->fs_buf, &len);

	if (error == 0) {
		fs->fs_ptr = fs->fs_buf;
		fs->fs_len = len;
		fs->fs_bufpos = readpos;
	}

	return (error);
}

static off_t
rawfs_get_pos(struct cfile *fs)
{
	return fs->fs_bufpos + (fs->fs_ptr - fs->fs_buf);
}

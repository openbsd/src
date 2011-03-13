/*	$OpenBSD: rawfs.c,v 1.5 2011/03/13 00:13:52 deraadt Exp $	*/
/*	$NetBSD: rawfs.c,v 1.2 1996/10/06 19:07:53 thorpej Exp $	*/

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

#include <lib/libsa/stand.h>
#include "rawfs.h"

extern int debug;

/* Our devices are generally willing to do 8K transfers. */
#define	RAWFS_BSIZE	0x2000

/*
 * In-core open file.
 */
struct rawfs_file {
	daddr32_t	fs_nextblk;	/* block number to read next */
	int		fs_len;		/* amount left in f_buf */
	char *		fs_ptr;		/* read pointer into f_buf */
	char		fs_buf[RAWFS_BSIZE];
};

static int
rawfs_get_block(struct open_file *);

int
rawfs_open(char *path, struct open_file *f)
{
	struct rawfs_file *fs;

	/*
	 * The actual tape driver has already been opened.
	 * Just allocate the I/O buffer, etc.
	 */
	fs = alloc(sizeof(struct rawfs_file));
	fs->fs_nextblk = 0;
	fs->fs_len = 0;
	fs->fs_ptr = fs->fs_buf;

#ifdef	DEBUG_RAWFS
	printf("rawfs_open: fs=0x%x\n", (u_long)fs);
#endif

	f->f_fsdata = fs;
	return (0);
}

int
rawfs_close(struct open_file *f)
{
	struct rawfs_file *fs;

	fs = (struct rawfs_file *) f->f_fsdata;
	f->f_fsdata = (void *)0;

#ifdef	DEBUG_RAWFS
	printf("rawfs_close: fs=0x%x\n", (u_long)fs);
#endif

	if (fs != (struct rawfs_file *)0)
		free(fs, sizeof(*fs));

	return (0);
}

int
rawfs_read(struct open_file *f, void *start, size_t size, size_t *resid)
{
	struct rawfs_file *fs = (struct rawfs_file *)f->f_fsdata;
	char *addr = start;
	int error = 0;
	size_t csize;

#ifdef DEBUG_RAWFS
	printf("rawfs_read: file=0x%x, start=0x%x, size=%d, resid=0x%x\n",
	    (u_long)f, (u_long)start, size, resid);
	printf("            fs=0x%x\n", (u_long)fs);
#endif

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
#ifdef	DEBUG_RAWFS
	printf("rawfs_write: YOU'RE NOT SUPPOSED TO GET HERE!\n");
#endif
	return (EROFS);
}

off_t
rawfs_seek(struct open_file *f, off_t offset, int where)
{
#ifdef	DEBUG_RAWFS
	printf("rawfs_seek: YOU'RE NOT SUPPOSED TO GET HERE!\n");
#endif
	return (EFTYPE);
}

int
rawfs_stat(struct open_file *f, struct stat *sb)
{
#ifdef	DEBUG_RAWFS
	printf("rawfs_stat: I'll let you live only because of exec.c\n");
#endif
	/*
	 * Clear out the stat buffer so that the uid check
	 * won't fail.  See sys/lib/libsa/exec.c
	 */
	bzero(sb, sizeof(*sb));

	return (EFTYPE);
}

/*
 * Read a block from the underlying stream device
 * (In our case, a tape drive.)
 */
static int
rawfs_get_block(struct open_file *f)
{
	struct rawfs_file *fs;
	size_t len;
	int error;

	fs = (struct rawfs_file *)f->f_fsdata;
	fs->fs_ptr = fs->fs_buf;

	twiddle();
#ifdef DEBUG_RAWFS
	printf("rawfs_get_block: calling strategy\n");
#endif
	error = f->f_dev->dv_strategy(f->f_devdata, F_READ,
		fs->fs_nextblk, RAWFS_BSIZE, fs->fs_buf, &len);
#ifdef DEBUG_RAWFS
	printf("rawfs_get_block: strategy returned %d\n", error);
#endif

	if (!error) {
		fs->fs_len = len;
		fs->fs_nextblk += (RAWFS_BSIZE / DEV_BSIZE);
	}

	return (error);
}

/*
 * $OpenBSD: xd.c,v 1.2 2002/03/14 01:26:29 millert Exp $
 * $NetBSD: xd.c,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 * Copyright (c) 1996 Ignatios Souvatzis.
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/types.h>

#include <stand.h>
#include <ufs.h>


#include "samachdep.h"
#include "amigaio.h"
#include "libstubs.h"

static int xdstrategy(void *, int, daddr_t, size_t, void *, size_t *);
static int xdopen(struct open_file *, ...);
static int xdclose(struct open_file *);
static int xdioctl(struct open_file *, u_long, void *);

static u_int32_t aio_base;
static struct AmigaIO *aio_save;

static struct devsw devsw[] = {
        { "xd", xdstrategy, xdopen, xdclose, xdioctl }
};              

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
};

int nfsys = sizeof(file_system)/sizeof(struct fs_ops);



/* called from configure */

void
xdinit(aio)
	void *aio;
{
	aio_save = aio;
	aio_base = aio_save->offset;
}

/* 
 * Kernel ist loaded from device and partition the kickstart
 * menu or boot priority has chosen:
 */

int
devopen(f, fname, file)
	struct open_file *f;
	const char *fname;
	char **file;
{
	f->f_devdata = aio_save;
	f->f_dev = &devsw[0];
	*file = (char *)fname;
	return 0;
}

/* tell kickstart to do the real work */

static int
xdstrategy (devd, flag, dblk, size, buf, rsize)
	void	*devd;
	int     flag;
	daddr_t dblk;
	size_t  size;
	void    *buf;
	size_t  *rsize; 
{
	struct AmigaIO *aio = (struct AmigaIO *)devd;

	if (flag != F_READ)
		return EIO;

	aio->cmd = Cmd_Rd;
	aio->length = size;
	aio->offset = aio_base + (dblk << 9);
	aio->buf = buf;

#ifdef XDDEBUG
	printf("strategy called: %ld(%ld), %ld, 0x%lx\n",
	    (long)dblk, (long)aio->offset, (long)size, (unsigned long)buf);
#endif

	DoIO(aio);

#ifdef XDDEBUG
	printf("strategy got err %ld, rsize %ld\n", aio->err, aio->actual);
#endif

	if (aio->err) {
		*rsize = 0;
		return EIO;
	}

	*rsize = aio->actual;
	return 0;
}


/* nothing do do for these: */

static int
xdopen(f)
	struct open_file *f;
{
	return 0;
}

static int
xdclose(f)
	struct open_file *f;
{
	return 0;
}

static int
xdioctl (f, cmd, data)
	struct open_file *f;
	u_long  cmd;
	void    *data;
{
	return EIO;
}

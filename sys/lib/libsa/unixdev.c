/*	$OpenBSD: unixdev.c,v 1.4 1998/05/25 18:37:30 mickey Exp $	*/

/*
 * Copyright (c) 1996-1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#define open uopen
#include <sys/fcntl.h>
#include <dev/cons.h>
#undef open
#include "libsa.h"
#include <lib/libsa/unixdev.h>

int
unixstrategy(devdata, rw, blk, size, buf, rsize)
	void *devdata;
	int rw;
	daddr_t blk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	int	rc = 0;

#ifdef	UNIX_DEBUG
	printf("unixstrategy: %s %d bytes @ %d\n",
		(rw==F_READ?"reading":"writing"), size, blk);
#endif
	if ((rc = ulseek((int)devdata, blk * DEV_BSIZE, 0)) >= 0)
		rc = rw==F_READ? uread((int)devdata, buf, size) :
			uwrite((int)devdata, buf, size);

	if (rc >= 0) {
		*rsize = (size_t)rc;
		rc = 0;
	} else
		rc = errno;

	return rc;
}

int
unixopen(struct open_file *f, ...)
{
	register int fd;
	register va_list ap;
	register char **file, *p = NULL;

	va_start(ap, f);
	file = va_arg(ap, char **);
	va_end(ap);

#ifdef	UNIX_DEBUG
	printf("unixopen: %s\n", *file);
#endif

	if (strncmp("/dev/", *file, 5) == 0) {
		/* p = strchr(p + 5, '/') */
		for (p = *file + 5; *p != '\0' && *p != '/'; p++);
		if (*p == '/')
			*p = '\0';
	}

	f->f_devdata = (void *)(fd = uopen(*file, O_RDWR, 0));

	*file = p;
	if (p != NULL)
		*p = '/';

	return fd<0? -1: 0;
}

int
unixclose(f)
	struct open_file *f;
{
	return uclose((int)f->f_devdata);
}

int
unixioctl(f, cmd, data)
	struct open_file *f;
	u_long cmd;
	void *data;
{
	return uioctl((int)f->f_devdata, cmd, data);
}

off_t
ulseek( fd, off, wh)
	int fd;
	off_t off;
	int wh;
{
	return __syscall((quad_t)SYS_lseek, fd, 0, off, wh);
}


void
unix_probe(cn)
	struct consdev *cn;
{
	cn->cn_pri = CN_INTERNAL;
	cn->cn_dev = makedev(0,0);
	printf("ux%d ", minor(cn->cn_dev));
}

void
unix_init(cn)
	struct consdev *cn;
{
}

void
unix_putc(dev, c)
	dev_t dev;
	int c;
{
	uwrite(1, &c, 1);
}

int
unix_getc(dev)
	dev_t dev;
{
	if (dev & 0x80) {
		struct timeval tv;
		fd_set fdset;
		int rc;

		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		FD_ZERO(&fdset);
		FD_SET(0, &fdset);

		if ((rc = syscall(SYS_select, 1, &fdset, NULL, NULL, &tv)) <= 0)
			return 0;
		else
			return 1;
	} else {
		char c;
		return uread(0, &c, 1)<1? -1: c;
	}
}

time_t
getsecs()
{
	return 1;
}

void
time_print()
{
}

void
atexit()
{
}

int
cnspeed(dev, sp)
	dev_t dev;
	int sp;
{
	return 9600;
}

void
__main()
{
}


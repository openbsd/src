/*	$OpenBSD: unixdev.c,v 1.3 2005/01/24 22:20:33 uwe Exp $	*/

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
unixstrategy(void *devdata, int rw, daddr_t blk, size_t size, void *buf,
    size_t *rsize)
{
	int	rc = 0;
	off_t	off;

#ifdef	UNIX_DEBUG
	printf("unixstrategy: %s %d bytes @ %d\n",
	    (rw==F_READ?"reading":"writing"), size, blk);
#endif

	off = (off_t)blk * DEV_BSIZE;
	if ((rc = ulseek((int)devdata, off, SEEK_SET)) >= 0)
		rc = (rw==F_READ) ? uread((int)devdata, buf, size) :
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
	char **file, *p = NULL;
	va_list ap;
	int fd;
	int c;

	va_start(ap, f);
	file = va_arg(ap, char **);
	va_end(ap);

#ifdef	UNIX_DEBUG
	printf("unixopen: %s\n", *file);
#endif

	/* p = strchr(p, ':') */
	for (p = *file; *p != '\0' && *p != ':'; p++)
		;

	c = *p;
	*p = '\0';
#if 0
	f->f_devdata = (void *)(fd = uopen(*file, O_RDWR, 0));
#else
	f->f_devdata = (void *)(fd = uopen(*file, O_RDONLY, 0));
#endif
	*p = c;

	if (*p == '\0')
		*file = p;
	else if (*(p+1) == '\0')
		*file = (char *)"/";
	else
		*file = p+1;

	return fd < 0 ? -1 : 0;
}

int
unixclose(struct open_file *f)
{
	return uclose((int)f->f_devdata);
}

int
unixioctl(struct open_file *f, u_long cmd, void *data)
{
	return uioctl((int)f->f_devdata, cmd, data);
}

off_t
ulseek(int fd, off_t off, int wh)
{
	extern	long ulseek32(int, long, int);
	off_t	r;

	/* XXX only SEEK_SET is used, so anything else can fail for now. */

	if (wh == SEEK_SET) {
		if (ulseek32(fd, 0, SEEK_SET) != 0)
			return -1;
		while (off > OFFT_OFFSET_MAX) {
			off -= OFFT_OFFSET_MAX;
			if (ulseek32(fd, OFFT_OFFSET_MAX, SEEK_CUR) < 0 &&
			    errno != EOVERFLOW)
				return -1;
		}
		r = ulseek32(fd, (long)off, SEEK_CUR);
		if (r == -1 && errno == EOVERFLOW)
			r = off;
	} else
		r = ulseek32(fd, (long)off, wh);

	return r;
}


void
unix_probe(struct consdev *cn)
{
	cn->cn_pri = CN_INTERNAL;
	cn->cn_dev = makedev(0,0);
	printf("ux%d ", minor(cn->cn_dev));
}

void
unix_init(struct consdev *cn)
{
}

void
unix_putc(dev_t dev, int c)
{
	uwrite(1, &c, 1);
}

int
unix_getc(dev_t dev)
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
getsecs(void)
{
	return 1;
}

void
time_print(void)
{
}

void
atexit(void)
{
}

int
cnspeed(dev_t dev, int sp)
{
	return 9600;
}

void
__main(void)
{
}

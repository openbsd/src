/*	$OpenBSD: unixdev.c,v 1.6 2007/06/16 00:26:33 deraadt Exp $	*/

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
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <sys/syscall.h>
#include <sys/time.h>
#define open uopen
#include <sys/fcntl.h>
#include <dev/cons.h>
#undef open
#include "disk.h"
#include "libsa.h"
#include <lib/libsa/unixdev.h>

int
unixstrategy(void *devdata, int rw, daddr_t blk, size_t size, void *buf,
    size_t *rsize)
{
	int	rc = 0;
	off_t	off;

#ifdef UNIX_DEBUG
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
	va_list	ap;
	char	path[PATH_MAX];
	char	*cp, **file;
	dev_t	maj, unit, part, bsd_dev;
	struct diskinfo *dip;

	va_start(ap, f);
	cp = *(file = va_arg(ap, char **));
	va_end(ap);

#ifdef UNIX_DEBUG
	if (debug)
		printf("unixopen: %s\n", cp);
#endif

	f->f_devdata = NULL;
	/* Search for device specification. */
	if (strlen(cp) < 4)
		return ENOENT;
	cp += 2;
	if (cp[2] != ':') {
		if (cp[3] != ':')
			return ENOENT;
		else
			cp++;
	}

	for (maj = 0;
	     maj < nbdevs && strncmp(*file, bdevs[maj], cp - *file) != 0;
	     maj++)
		;
	if (maj >= nbdevs) {
		printf("Unknown device: ");
		for (cp = *file; *cp != ':'; cp++)
			putchar(*cp);
		putchar('\n');
		return EADAPT;
	}

	/* Get unit. */
	if ('0' <= *cp && *cp <= '9')
		unit = *cp++ - '0';
	else {
		printf("Bad unit number\n");
		return EUNIT;
	}

	/* Get partition. */
	if ('a' <= *cp && *cp <= 'p')
		part = *cp++ - 'a';
	else {
		printf("Bad partition id\n");
		return EPART;
	}

	cp++;	/* skip ':' */
	if (*cp != 0)
		*file = cp;
	else
		f->f_flags |= F_RAW;

	/* Find device. */
	dip = dkdevice(maj, unit);
	if (dip == (struct diskinfo *)NULL)
		return ENOENT;

	/* Fix up bootdev. */
	bsd_dev = dip->bios_info.bsd_dev;
	dip->bsddev = MAKEBOOTDEV(B_TYPE(bsd_dev), B_ADAPTOR(bsd_dev),
	    B_CONTROLLER(bsd_dev), unit, part);
	dip->bootdev = MAKEBOOTDEV(B_TYPE(bsd_dev), B_ADAPTOR(bsd_dev),
	    B_CONTROLLER(bsd_dev), B_UNIT(bsd_dev), part);

	/* Try for disklabel again (might be removable media). */
	if (dip->bios_info.flags & BDI_BADLABEL) {
		const char *st = bios_getdisklabel(&dip->bios_info,
		    &dip->disklabel);
#ifdef UNIX_DEBUG
		if (debug && st)
			printf("%s\n", st);
#endif
		if (!st) {
			dip->bios_info.flags &= ~BDI_BADLABEL;
			dip->bios_info.flags |= BDI_GOODLABEL;
		} else
			return ERDLAB;
	}

	part = bios_getdospart(&dip->bios_info);
	bios_devpath(dip->bios_info.bios_number, part, path);
	f->f_devdata = (void *)uopen(path, O_RDONLY);
	if ((int)f->f_devdata == -1)
		return errno;

	return 0;
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
			    errno != LINUX_EOVERFLOW)
				return -1;
		}
		r = ulseek32(fd, (long)off, SEEK_CUR);
		if (r == -1 && errno == LINUX_EOVERFLOW)
			r = off;
	} else
		r = ulseek32(fd, (long)off, wh);

	return r;
}

time_t
getsecs(void)
{
	return (time_t)syscall(__NR_time, NULL);
}

unsigned int
sleep(unsigned int seconds)
{
	unsigned int start;

	start = getsecs();
	while (getsecs() - start < seconds)
		;

	return (0);
}

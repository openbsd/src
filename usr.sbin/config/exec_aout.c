/*	$OpenBSD: exec_aout.c,v 1.10 2009/10/27 23:59:51 deraadt Exp $ */

/*
 * Copyright (c) 1999 Mats O Jansson.  All rights reserved.
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/exec.h>
#include <sys/types.h>

#include "config.h"
#include "ukc.h"

caddr_t		aout_p, aout_r;
unsigned long	aout_psz = 0, aout_rsz = 0;
struct exec	aout_ex;
unsigned long	aout_adjvalue = 0;
unsigned long	aout_datashift = 0;

void
aout_computeadj(void)
{
	aout_adjvalue = (unsigned long)aout_p +
	    N_TXTOFF(aout_ex) - nl[P_KERNEL_TEXT].n_value;

	/*
	 * On m68k a.out ZMAGIC kernel, kernel_text begins _after_ the a.out
	 * header, so compensate for it.
	 */
	if (nl[P_KERNEL_TEXT].n_value & (__LDPGSZ - 1))
		aout_adjvalue += sizeof(aout_ex);

	/*
	 * On NMAGIC kernel, we need an extra relocation for the data area
	 */
	aout_datashift = (N_DATADDR(aout_ex) - N_TXTADDR(aout_ex)) -
	    aout_ex.a_text;
}

/* ``kernel'' vaddr -> in-memory address */
caddr_t
aout_adjust(caddr_t x)
{

	if (aout_adjvalue == 0)
		aout_computeadj();

	if (aout_datashift != 0 &&
	    (unsigned long)x >= N_DATADDR(aout_ex) - N_TXTADDR(aout_ex))
		x -= aout_datashift;

	return (x + aout_adjvalue);
}

/* in-memory address -> ``kernel'' vaddr */
caddr_t
aout_readjust(x)
	caddr_t x;
{
	caddr_t y;

#if 0	/* unnecessary, aout_adjust() is always invoked first */
	if (aout_adjvalue == 0)
		aout_computeadj();
#endif

	y = x - aout_adjvalue;
	if (aout_datashift != 0 &&
	    (unsigned long)y >= N_TXTADDR(aout_ex) + aout_ex.a_text)
		y += aout_datashift;

	return (y);
}

int
aout_check(file)
	char *file;
{
	int fd, ret = 1;

	if ((fd = open(file, O_RDONLY | O_EXLOCK, 0)) < 0)
		return (0);
	if (read(fd, (char *)&aout_ex, sizeof(aout_ex)) != sizeof(aout_ex))
		ret = 0;
	if (ret) {
		if (N_BADMAG(aout_ex))
			ret = 0;
	}

	close(fd);
	return (ret);
}

void
aout_loadkernel(char *file)
{
	int fd;
	off_t cur, end;

	if ((fd = open(file, O_RDONLY | O_EXLOCK, 0)) < 0)
		err(1, "%s", file);

	if (read(fd, (char *)&aout_ex, sizeof(aout_ex)) != sizeof(aout_ex))
		errx(1, "can't read a.out header");

	if (N_BADMAG(aout_ex))
		errx(1, "bad a.out magic");

	lseek(fd, (off_t)0, SEEK_SET);

	aout_psz = (int)(aout_ex.a_text + N_TXTOFF(aout_ex) +
	    aout_ex.a_data);

	aout_p = emalloc(aout_psz);

	if (read(fd, aout_p, aout_psz) != aout_psz)
		errx(1, "can't read a.out text and data");

	cur = lseek(fd, (off_t)0, SEEK_CUR);
	end = lseek(fd, (off_t)0, SEEK_END);
	(void)lseek(fd, cur, SEEK_SET);

	aout_rsz = (int)(end - cur);

	aout_r = emalloc(aout_rsz);

	if (read(fd, aout_r, aout_rsz) != aout_rsz)
		errx(1, "can't read rest of file %s", file);

	close(fd);
}

void
aout_savekernel(char *outfile)
{
	int fd;

	if ((fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0755)) < 0)
		err(1, "%s", outfile);

	if (write(fd, aout_p, aout_psz) != aout_psz)
		errx(1, "can't write a.out text and data");

	if (write(fd, aout_r, aout_rsz) != aout_rsz)
		errx(1, "can't write rest of file %s", outfile);

	close(fd);
}

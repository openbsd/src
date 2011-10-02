/*	$OpenBSD: exec_ecoff.c,v 1.11 2011/10/02 22:20:49 edd Exp $ */

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

#include <sys/types.h>
#include <sys/exec.h>
#include <sys/exec_ecoff.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ukc.h"

caddr_t		ecoff_p, ecoff_r, ecoff_b;
int		ecoff_psz = 0, ecoff_rsz = 0, ecoff_bsz = 0;
struct ecoff_exechdr	ecoff_ex;

caddr_t
ecoff_adjust(caddr_t x)
{
	unsigned long y;

	y = (unsigned long)x - nl[P_KERNEL_TEXT].n_value + (unsigned long)ecoff_p;

	return((caddr_t)y);
}

caddr_t
ecoff_readjust(caddr_t x)
{
	unsigned long y;

	y = (unsigned long)x - (unsigned long)ecoff_p + nl[P_KERNEL_TEXT].n_value;

	return((caddr_t)y);
}

int
ecoff_check(char *file)
{
	int fd, ret = 1;

	if ((fd = open(file, O_RDONLY | O_EXLOCK, 0)) < 0)
		return (0);

	if (read(fd,(char *)&ecoff_ex, sizeof(ecoff_ex)) != sizeof(ecoff_ex))
		ret = 0;

	if (ret) {
		if (ECOFF_BADMAG(&ecoff_ex))
			ret = 0;
	}

	close(fd);
	return (ret);
}

void
ecoff_loadkernel(char *file)
{
	int fd;
	off_t beg, cur, end;

	if ((fd = open(file, O_RDONLY | O_EXLOCK, 0)) < 0)
		err(1, "%s", file);

	if (read(fd, (char *)&ecoff_ex, sizeof(ecoff_ex)) != sizeof(ecoff_ex))
		errx(1, "can't read ecoff header");

	if (ECOFF_BADMAG(&ecoff_ex))
		errx(1, "bad ecoff magic");

	ecoff_psz = ecoff_ex.a.tsize + ecoff_ex.a.dsize;
	beg = lseek(fd, ECOFF_TXTOFF(&ecoff_ex), SEEK_SET);

	ecoff_bsz = (int)beg;
	ecoff_b = emalloc(ecoff_bsz);

	ecoff_p = emalloc(ecoff_psz);

	if (read(fd, ecoff_p, ecoff_psz) != ecoff_psz)
		errx(1, "can't read ecoff text and data");

	cur = lseek(fd, (off_t)0, SEEK_CUR);
	end = lseek(fd, (off_t)0, SEEK_END);
	(void)lseek(fd, (off_t)0, SEEK_SET);
	if (read(fd, ecoff_b, ecoff_bsz) != ecoff_bsz)
		errx(1, "can't read begining of file %s", file);
	(void)lseek(fd, cur, SEEK_SET);

	ecoff_rsz = (int)(end - cur);

	ecoff_r = emalloc(ecoff_rsz);

	if (read(fd, ecoff_r, ecoff_rsz) != ecoff_rsz)
		errx(1, "can't read rest of file %s", file);

	close(fd);
}

void
ecoff_savekernel(char *outfile)
{
	int fd;

	if ((fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0755)) < 0)
		err(1, "%s", outfile);

	if (write(fd, ecoff_b, ecoff_bsz) != ecoff_bsz)
		errx(1, "can't write beginning of file %s",outfile);

	if (write(fd, ecoff_p, ecoff_psz) != ecoff_psz)
		errx(1, "can't write ecoff text and data");

	if (write(fd, ecoff_r, ecoff_rsz) != ecoff_rsz)
		errx(1, "can't write rest of file %s", outfile);

	close(fd);
}

/*	$OpenBSD: exec_aout.c,v 1.1 1999/10/04 20:00:51 deraadt Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef LINT
static char rcsid[] = "$OpenBSD: exec_aout.c,v 1.1 1999/10/04 20:00:51 deraadt Exp $";
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/exec.h>
#include <sys/types.h>

#include "ukc.h"

caddr_t		aout_p,aout_r;
int		aout_psz = 0, aout_rsz = 0;
struct exec	aout_ex;

caddr_t 
aout_adjust(x)
	caddr_t x;
{
	unsigned long y;

	y = (unsigned long)x - nl[P_KERNEL_TEXT].n_value + (unsigned long)aout_p +
	    N_TXTOFF(aout_ex);
	return((caddr_t)y);
}

caddr_t
aout_readjust(x)
	caddr_t x;
{
	unsigned long y;

	y = (unsigned long)x - (unsigned long)aout_p + nl[P_KERNEL_TEXT].n_value -
	    N_TXTOFF(aout_ex);
	return((caddr_t)y);
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
aout_loadkernel(file)
	char *file;
{
	int fd;
	off_t cur,end;

	if ((fd = open(file, O_RDONLY | O_EXLOCK, 0)) < 0)
		err(1, file, errno);
	  
	if (read(fd, (char *)&aout_ex, sizeof(aout_ex)) != sizeof(aout_ex))
		errx(1, "can't read a.out header");
	
	if (N_BADMAG(aout_ex))
		errx(1, "bad a.out magic\n");
	
	(void)lseek(fd, (off_t)0, SEEK_SET);

	aout_psz = (int)(aout_ex.a_text+aout_ex.a_data);
	
	aout_p = malloc(aout_psz);
	
	if (read(fd, aout_p, aout_psz) != aout_psz)
		errx(1, "can't read a.out text and data");

	cur = lseek(fd, (off_t)0, SEEK_CUR);
	end = lseek(fd, (off_t)0, SEEK_END);
	(void)lseek(fd, cur, SEEK_SET);

	aout_rsz = (int)(end - cur);

	aout_r = malloc(aout_rsz);

	if (read(fd, aout_r, aout_rsz) != aout_rsz)
		errx(1, "can't read rest of file %s", file);
	
	close(fd);
}

void
aout_savekernel(outfile)
	char *outfile;
{
	int fd;

	if ((fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0755)) < 0)
		err(1, outfile, errno);

	if (write(fd, aout_p, aout_psz) != aout_psz)
		errx(1, "can't write a.out text and data");

	if (write(fd, aout_r, aout_rsz) != aout_rsz)
		errx(1, "can't write rest of file %s", outfile);

	close(fd);
}

/*	$OpenBSD: readdir.c,v 1.1 1997/02/16 14:48:06 mickey Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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

#ifndef NO_READDIR

#include <sys/types.h>
#define _KERNEL
#include <sys/fcntl.h>
#undef _KERNEL
#include "stand.h"


int
opendir(name)
	char *name;
{
	int fd;

#ifdef __INTERNAL_LIBSA_CREAD
	if ((fd = oopen(name, O_RDONLY)) >= 0)
#else
	if ((fd = open(name, O_RDONLY)) >= 0)
#endif
		/* XXX   it's needed for some dirs */
#ifdef __INTERNAL_LIBSA_CREAD
		olseek(fd, 0, 0);
#else
		lseek(fd, 0, 0);
#endif

	return fd;
}
	
int
readdir(fd, dest)
	int fd;
	char *dest;
{
	register struct open_file *f = &files[fd];

	if ((unsigned)fd >= SOPEN_MAX || !(f->f_flags & F_READ)) {
		errno = EBADF;
		return (-1);
	}
	if (f->f_flags & F_RAW) {
		errno = EINVAL;
		return (-1);
	}
	if ((errno = (f->f_ops->readdir)(f, dest)))
		return (-1);

	return 0;
}

void
closedir(fd)
	int fd;
{
#ifdef __INTERNAL_LIBSA_CREAD
	oclose(fd);
#else
	close(fd);
#endif
}

#endif /* NO_READDIR */

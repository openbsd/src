/*	$OpenBSD: syscall.h,v 1.8 2002/07/24 01:05:11 deraadt Exp $ */

/*
 * Copyright (c) 2001 Niklas Hallqvist
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
 *	This product includes software developed under OpenBSD by
 *	Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __DL_SYSCALL_H__
#define __DL_SYSCALL_H__

#include <sys/syscall.h>
#include <sys/stat.h>

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_check_error(__res) \
    ((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)

int	_dl_close(int);
int	_dl_exit(int);
int	_dl_issetugid(void);
long	_dl__syscall(quad_t, ...);
int	_dl_mprotect(const void *, size_t, int);
int	_dl_munmap(const void*, size_t);
int	_dl_open(const char*, int);
ssize_t	_dl_read(int, const char*, size_t);
int	_dl_stat(const char *, struct stat *);
ssize_t	_dl_write(int, const char*, size_t);
int	_dl_fstat(int, struct stat *);
int	_dl_fcntl(int, int, ...);
int	_dl_getdirentries(int, char*, int, long *);

static inline off_t
_dl_lseek(int fildes, off_t offset, int whence)
{
	return _dl__syscall((quad_t)SYS_lseek, fildes, 0, offset, whence);
}

#endif /*__DL_SYSCALL_H__*/

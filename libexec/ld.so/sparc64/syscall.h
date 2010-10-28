/*	$OpenBSD: syscall.h,v 1.17 2010/10/28 15:02:41 millert Exp $ */

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
#include <sys/signal.h>

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 512L
#endif
#define _dl_mmap_error(__res) \
    ((long)__res < 0 && (long)__res >= -_dl_MAX_ERRNO)

int	_dl_close(int);
int	_dl_exit(int);
int	_dl_issetugid(void);
long	_dl__syscall(quad_t, ...);
int	_dl_mprotect(const void *, size_t, int);
int	_dl_munmap(const void*, size_t);
int	_dl_open(const char*, int);
ssize_t	_dl_read(int, const char*, size_t);
int	_dl_stat(const char *, struct stat *);
int	_dl_fstat(int, struct stat *);
int	_dl_fcntl(int, int, ...);
int	_dl_getdirentries(int, char*, int, off_t *);
int	_dl_sigprocmask(int, const sigset_t *, sigset_t *);
int	_dl_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	_dl_gettimeofday(struct timeval *tp, struct timezone *tzp);

static inline off_t
_dl_lseek(int fildes, off_t offset, int whence)
{
	return _dl__syscall((quad_t)SYS_lseek, fildes, 0, offset, whence);
}

#endif /*__DL_SYSCALL_H__*/

/*	$OpenBSD: syscall.h,v 1.6 2018/07/09 10:12:14 deraadt Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 512L
#endif
#define _dl_mmap_error(__res) \
    ((long)__res < 0 && (long)__res >= -_dl_MAX_ERRNO)

int	_dl_close(int);
__dead
void	_dl_exit(int);
int	_dl_fstat(int, struct stat *);
int	_dl___getcwd(char *, size_t);
ssize_t	_dl_getdents(int, char *, size_t);
int	_dl_issetugid(void);
int	_dl_getthrid(void);
int	_dl_mprotect(const void *, size_t, int);
int	_dl_munmap(const void *, size_t);
int	_dl_open(const char *, int);
ssize_t	_dl_read(int, const char *, size_t);
ssize_t	_dl_readlink(const char *, char *, size_t);
int	_dl_pledge(const char *, const char **);
long	_dl___syscall(quad_t, ...);
int	_dl_sysctl(const int *, u_int, void *, size_t *, void *, size_t);
int	_dl_utrace(const char *, const void *, size_t);
int	_dl_getentropy(char *, size_t);
int	_dl_sendsyslog(const char *, size_t, int);
void	_dl___set_tcb(void *);
__dead
void	_dl_thrkill(pid_t, int, void *);

static inline void *
_dl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	return (void *)_dl___syscall(SYS_mmap, addr, len, prot,
	    flags, fd, 0, offset);
}

#endif /*__DL_SYSCALL_H__*/

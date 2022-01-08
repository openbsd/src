/*	$OpenBSD: syscall.h,v 1.2 2022/01/08 06:49:41 guenther Exp $ */

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

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 512L
#endif
#define _dl_mmap_error(__res) \
    ((long)__res < 0 && (long)__res >= -_dl_MAX_ERRNO)

struct __kbind;
struct stat;

int	_dl_close(int);
__dead
void	_dl_exit(int);
int	_dl_fstat(int, struct stat *);
ssize_t	_dl_getdents(int, char *, size_t);
int	_dl_getentropy(char *, size_t);
int	_dl_getthrid(void);
int	_dl_issetugid(void);
int	_dl_kbind(const struct __kbind *, size_t, int64_t);
void   *_dl_mmap(void *, size_t, int, int, int, off_t);
int	_dl_mprotect(const void *, size_t, int);
void   *_dl_mquery(void *, size_t, int, int, int, off_t);
int	_dl_msyscall(void *addr, size_t len);
int	_dl_munmap(const void *, size_t);
int	_dl_open(const char *, int);
int	_dl_pledge(const char *, const char **);
ssize_t	_dl_read(int, const char *, size_t);
int	_dl_sendsyslog(const char *, size_t, int);
void	_dl___set_tcb(void *);
int	_dl_sysctl(const int *, u_int, void *, size_t *, void *, size_t);
__dead
void	_dl_thrkill(pid_t, int, void *);
int	_dl_utrace(const char *, const void *, size_t);

#endif /*__DL_SYSCALL_H__*/

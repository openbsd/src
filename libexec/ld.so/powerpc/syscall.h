/*	$OpenBSD: syscall.h,v 1.14 2002/12/18 19:20:02 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
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

#include <sys/stat.h>

#include <sys/syscall.h>
#include <sys/signal.h>


static off_t	_dl_lseek(int, off_t, int);

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_check_error(__res) \
	((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)

/*
 *  Inlined system call functions that can be used before
 *  any dynamic address resolving has been done.
 */

static inline int
_dl_exit (int status)
{
	register int __status __asm__ ("3");

	__asm__ volatile ("mr  0,%1\n\t"
	    "mr  3,%2\n\t"
	    "sc"
	    : "=r" (__status)
	    : "r" (SYS_exit), "r" (status) : "0", "3");

	while (1)
		;
}

static inline int
_dl_open (const char* addr, int flags)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_open), "r" (addr), "r" (flags)
	    : "0", "3", "4" );
	return status;
}

static inline int
_dl_close (int fd)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_close), "r" (fd)
	    : "0", "3");
	return status;
}

static inline ssize_t
_dl_write (int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "mr    5,%4\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_write), "r" (fd), "r" (buf), "r" (len)
	    : "0", "3", "4", "5" );
	return status;
}

static inline ssize_t
_dl_read (int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "mr    5,%4\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_read), "r" (fd), "r" (buf), "r" (len)
	    : "0", "3", "4", "5");
	return status;
}

#define STRINGIFY(x)  #x
#define XSTRINGIFY(x) STRINGIFY(x)
long _dl__syscall(quad_t val, ...);
__asm__(".align 2\n\t"
	".type _dl__syscall,@function\n"
	"_dl__syscall:\n\t"
	"li 0, " XSTRINGIFY(SYS___syscall) "\n\t"
	"sc\n\t"
	"cmpwi	0, 0\n\t"
	"beq	1f\n\t"
	"li	3, -1\n\t"
	"1:\n\t"
	"blr");

static inline void *
_dl_mmap (void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
	    flags, fd, 0, offset));
}

static inline int
_dl_munmap (const void* addr, size_t len)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_munmap), "r" (addr), "r" (len)
	    : "0", "3", "4");
	return status;
}

static inline int
_dl_mprotect (const void *addr, size_t size, int prot)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "mr    5,%4\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_mprotect), "r" (addr), "r" (size), "r" (prot)
	    : "0", "3", "4", "5");
	return status;
}

static inline int
_dl_stat (const char *addr, struct stat *sb)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_stat), "r" (addr), "r" (sb)
	    : "0", "3", "4");
	return status;
}

static inline int
_dl_fstat (int fd, struct stat *sb)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_fstat), "r" (fd), "r" (sb)
	    : "0", "3", "4");
	return status;
}

static inline int
_dl_fcntl (int fd, int cmd, int flag)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "mr    5,%4\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_fcntl), "r" (fd), "r" (cmd), "r"(flag)
	    : "0", "3", "4", "5");
	return status;
}

static inline int
_dl_getdirentries(int fd, char *buf, int nbytes, long *basep)
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "mr    5,%4\n\t"
	    "mr    6,%5\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_getdirentries), "r" (fd), "r" (buf), "r"(nbytes),
	    "r" (basep)
	    : "0", "3", "4", "5", "6");
	return status;
}

static inline int
_dl_issetugid()
{
	register int status __asm__ ("3");

	__asm__ volatile ("mr    0,%1\n\t"
	    "sc\n\t"
	    "cmpwi   0, 0\n\t"
	    "beq   1f\n\t"
	    "li    3,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "r" (SYS_issetugid)
	    : "0", "3");
	return status;
}

static inline off_t
_dl_lseek(int fildes, off_t offset, int whence)
{
	return _dl__syscall((quad_t)SYS_lseek, fildes, 0, offset, whence);
}

static inline int
_dl_sigprocmask (int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t sig_store;
	sigset_t sig_store1;
	if (set != NULL) {
		sig_store1 = *set;
	} else {
		sig_store1 = 0;
	}

	__asm__ volatile ("li    0,%1\n\t"
	    "mr    3,%2\n\t"
	    "mr    4,%3\n\t"
	    "sc\n\t"
	    "mr    %0, 3"
	    : "=r" (sig_store)
	    : "I" (SYS_sigprocmask), "r" (how), "r" (sig_store1)
	    : "0", "3", "4");
	if (oset != NULL)
		*oset = sig_store;

	return 0;
}
#endif /*__DL_SYSCALL_H__*/

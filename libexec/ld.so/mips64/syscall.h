/*	$OpenBSD: syscall.h,v 1.3 2005/09/22 04:07:11 deraadt Exp $ */

/*
 * Copyright (c) 1998-2002 Opsycon AB, Sweden.
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

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/signal.h>

extern long _dl__syscall(quad_t val, ...);

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_check_error(__res)	\
	((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)

/*
 *  Inlined system call functions that can be used before
 *  any dynamic address resolving has been done.
 */

extern inline void
_dl_exit(int status)
{
	register int __status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "li    $2,%1\n\t"
	    "syscall"
	    : "=r" (__status)
	    : "I" (SYS_exit), "r" (status)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	while (1)
		;
}

extern inline int
_dl_open(const char* addr, int flags)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_open), "r" (addr), "r" (flags)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_close(int fd)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_close), "r" (fd)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_write(int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_write), "r" (fd), "r" (buf), "r" (len)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_read(int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_read), "r" (fd), "r" (buf), "r" (len)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline void *
_dl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	return((void *)_dl__syscall((quad_t)SYS_mmap, addr, len, prot,
	    flags, fd, 0, offset));
}

extern inline int
_dl_munmap(const void* addr, size_t len)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_munmap), "r" (addr), "r" (len)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_mprotect(const void *addr, size_t size, int prot)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "li    $2,%1\n\t"
	    "syscall"
	    : "=r" (status)
	    : "I" (SYS_mprotect), "r" (addr), "r" (size), "r" (prot)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_stat(const char *addr, struct stat *sb)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "li    $2,%1\n\t"
	    "syscall"
	    : "=r" (status)
	    : "I" (SYS_stat), "r" (addr), "r" (sb)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_fstat(const int fd, struct stat *sb)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "li    $2,%1\n\t"
	    "syscall"
	    : "=r" (status)
	    : "I" (SYS_fstat), "r" (fd), "r" (sb)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_fcntl(int fd, int cmd, int flag)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_fcntl), "r" (fd), "r" (cmd), "r" (flag)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_getdirentries(int fd, char *buf, int nbytes, long *basep)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "move  $7,%5\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS_getdirentries), "r" (fd), "r" (buf), "r" (nbytes), "r" (basep)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_issetugid(void)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "li    $2,%1\n\t"
	    "syscall"
	    : "=r" (status)
	    : "I" (SYS_issetugid)
	    :  "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline off_t
_dl_lseek(int fd, off_t offset, int whence)
{
	return _dl__syscall((quad_t)SYS_lseek, fd, 0, offset, whence);
}

extern inline int
_dl_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	sigset_t sig_store;
	sigset_t sig_store1;

	if (set != NULL)
		sig_store1 = *set;
	else
		sig_store1 = 0;

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "move    %0, $2"
	    : "=r" (sig_store)
	    : "I" (SYS_sigprocmask), "r" (how), "r" (sig_store1)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	if (oset != NULL)
		*oset = sig_store;

	return 0;
}
static inline int
_dl_sysctl(int *name, u_int namelen, void *oldp, size_t *oldplen, void *newp,
    size_t newlen)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "move  $7,%5\n\t"
	    "move  $8,%6\n\t"
	    "move  $9,%7\n\t"
	    "li    $2,%1\n\t"
	    "syscall\n\t"
	    "beqz   $2,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "I" (SYS___sysctl), "r" (name), "r" (namelen), "r" (oldp),
	    "r" (oldplen), "r" (newp), "r" (newlen)
	    : "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

#endif /*__DL_SYSCALL_H__*/

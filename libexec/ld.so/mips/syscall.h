/*	$OpenBSD: syscall.h,v 1.10 2002/10/23 12:38:29 pefo Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Opsycon AB, Sweden.
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

extern inline int
_dl_exit (int status)
{
	register int __status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "syscall"
	    : "=r" (__status)
	    : "0" (SYS_exit), "r" (status)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	while (1)
		;
}

extern inline int
_dl_open (const char* addr, int flags)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_open), "r" (addr), "r" (flags)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_close (int fd)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_close), "r" (fd)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_write (int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_write), "r" (fd), "r" (buf), "r" (len)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_read (int fd, const char* buf, size_t len)
{
	register ssize_t status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_read), "r" (fd), "r" (buf), "r" (len)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline void *
_dl_mmap (void *addr, size_t size, int prot, int flags, int fd, off_t f_offset)
{
	register void * malloc_buffer __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "addiu $29,-40\n\t"
	    "move  $6,%2\n\t"
	    "move  $7,%3\n\t"
	    "sw    %4,16($29)\n\t"
	    "sw    %5,20($29)\n\t"
#ifdef __MIPSEL__
	    "li    $4,197\n\t"
	    "li    $5,0\n\t"
	    "sw    %6,24($29)\n\t"
	    "sw    $0,28($29)\n\t"
	    "sw    %7,32($29)\n\t"
	    "sw    $0,36($29)\n\t"
#endif
#ifdef __MIPSEB__
	    "li    $4,0\n\t"
	    "li    $5,197\n\t"
	    "sw    %6,24($29)\n\t"
	    "sw    $0,28($29)\n\t"
	    "sw    $0,32($29)\n\t"
	    "sw    %7,36($29)\n\t"
#endif
	    "syscall\n\t"
	    "addiu $29,40"
	    : "=r" (malloc_buffer)
	    : "0" (SYS___syscall), "r" (addr), "r" (size), "r" (prot),
	    "r" (flags), "r" (fd), "r" (f_offset)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return malloc_buffer;
}

extern inline int
_dl_munmap (const void* addr, size_t len)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_munmap), "r" (addr), "r" (len)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_mprotect (const void *addr, size_t size, int prot)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "syscall"
	    : "=r" (status)
	    : "0" (SYS_mprotect), "r" (addr), "r" (size), "r" (prot)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_stat (const char *addr, struct stat *sb)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "syscall"
	    : "=r" (status)
	    : "0" (SYS_stat), "r" (addr), "r" (sb)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_fstat (const int fd, struct stat *sb)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "syscall"
	    : "=r" (status)
	    : "0" (SYS_fstat), "r" (fd), "r" (sb)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_fcntl (int fd, int cmd, int flag)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_fcntl), "r" (fd), "r" (cmd), "r" (flag)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline ssize_t
_dl_getdirentries (int fd, char *buf, int nbytes, long *basep)
{
	register int status __asm__ ("$2");

	__asm__ volatile ("move  $2,%1\n\t"
	    "move  $4,%2\n\t"
	    "move  $5,%3\n\t"
	    "move  $6,%4\n\t"
	    "move  $7,%5\n\t"
	    "syscall\n\t"
	    "beq   $7,$0,1f\n\t"
	    "li    $2,-1\n\t"
	    "1:"
	    : "=r" (status)
	    : "0" (SYS_getdirentries), "r" (fd), "r" (buf), "r" (nbytes), "r" (basep)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline int
_dl_issetugid (void)
{
	register int status __asm__ ("$2");

	__asm__ volatile (
	    "move  $2,%1\n\t"
	    "syscall"
	    : "=r" (status)
	    : "0" (SYS_issetugid)
	    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
	    "$10","$11","$12","$13","$14","$15","$24","$25");
	return status;
}

extern inline off_t
_dl_lseek (int fd, off_t offset, int whence)
{
	return _dl__syscall((quad_t)SYS_lseek, fd, 0, offset, whence);
}
#endif /*__DL_SYSCALL_H__*/

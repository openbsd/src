/*	$OpenBSD: syscall.h,v 1.1 2001/09/21 14:54:19 jason Exp $ */

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

#ifdef USE_CACHE
#include <sys/stat.h>
#endif

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_check_error(__res) \
    ((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)

int	_dl_close __P((int));
int	_dl_exit __P((int));
int	_dl_getegid __P((void));
int	_dl_geteuid __P((void));
int	_dl_getgid __P((void));
int	_dl_getuid __P((void));
long	_dl_mmap __P((void *, unsigned int, unsigned int, unsigned int, int,
		      off_t));
int	_dl_mprotect __P((const void *, int, int));
int	_dl_munmap __P((const void*, unsigned int));
int	_dl_open __P((const char*, unsigned int));
void	_dl_printf __P((const char *, ...));
int	_dl_read __P((int, const char*, int));
#ifdef USE_CACHE
int	_dl_stat __P((const char *, struct stat *));
#endif
int	_dl_write __P((int, const char*, int));

/*
 * Not an actual syscall, but we need something in assembly to say
 * whether this is OK or not.
 */

static inline int
_dl_suid_ok (void)
{
	unsigned int uid, euid, gid, egid;

	uid = _dl_getuid();
	euid = _dl_geteuid();
	gid = _dl_getgid();
	egid = _dl_getegid();
  	return (uid == euid && gid == egid);
}

#include <elf_abi.h>

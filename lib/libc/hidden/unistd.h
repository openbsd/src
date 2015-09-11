/*	$OpenBSD: unistd.h,v 1.2 2015/09/11 15:38:33 guenther Exp $	*/
/*
 * Copyright (c) 2015 Philip Guenther <guenther@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LIBC_UNISTD_H_
#define	_LIBC_UNISTD_H_

#include_next <unistd.h>

PROTO_NORMAL(_exit);
PROTO_NORMAL(access);
PROTO_NORMAL(acct);
PROTO_DEPRECATED(brk);
PROTO_NORMAL(chdir);
PROTO_NORMAL(chown);
PROTO_NORMAL(chroot);
PROTO_NORMAL(dup);
PROTO_NORMAL(dup2);
PROTO_NORMAL(dup3);
PROTO_NORMAL(execve);
PROTO_NORMAL(faccessat);
PROTO_NORMAL(fchdir);
PROTO_NORMAL(fchown);
PROTO_NORMAL(fchownat);
PROTO_NORMAL(fpathconf);
PROTO_NORMAL(ftruncate);
PROTO_NORMAL(getdtablecount);
PROTO_NORMAL(getegid);
PROTO_NORMAL(getentropy);
PROTO_NORMAL(geteuid);
PROTO_NORMAL(getgid);
PROTO_NORMAL(getgroups);
PROTO_NORMAL(getpgid);
PROTO_NORMAL(getpgrp);
PROTO_NORMAL(getpid);
PROTO_NORMAL(getppid);
PROTO_NORMAL(getresgid);
PROTO_NORMAL(getresuid);
PROTO_NORMAL(getsid);
PROTO_NORMAL(getthrid);
PROTO_NORMAL(getuid);
PROTO_NORMAL(issetugid);
PROTO_NORMAL(lchown);
PROTO_NORMAL(link);
PROTO_NORMAL(linkat);
PROTO_NORMAL(lseek);
PROTO_NORMAL(nfssvc);
PROTO_NORMAL(pathconf);
PROTO_NORMAL(pipe);
PROTO_NORMAL(pipe2);
PROTO_NORMAL(profil);
PROTO_NORMAL(quotactl);
PROTO_NORMAL(readlink);
PROTO_NORMAL(readlinkat);
PROTO_NORMAL(reboot);
PROTO_NORMAL(revoke);
PROTO_NORMAL(rmdir);
PROTO_DEPRECATED(sbrk);
PROTO_NORMAL(setegid);
PROTO_NORMAL(seteuid);
PROTO_NORMAL(setgid);
PROTO_NORMAL(setgroups);
PROTO_NORMAL(setpgid);
PROTO_NORMAL(setregid);
PROTO_NORMAL(setresgid);
PROTO_NORMAL(setresuid);
PROTO_NORMAL(setreuid);
PROTO_NORMAL(setsid);
PROTO_NORMAL(setuid);
PROTO_NORMAL(swapctl);
PROTO_NORMAL(symlink);
PROTO_NORMAL(symlinkat);
PROTO_NORMAL(sync);
PROTO_NORMAL(truncate);
PROTO_NORMAL(unlink);
PROTO_NORMAL(unlinkat);

#endif /* !_LIBC_UNISTD_H_ */

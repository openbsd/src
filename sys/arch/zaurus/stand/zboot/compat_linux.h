/*	$OpenBSD: compat_linux.h,v 1.6 2005/05/12 05:10:30 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
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

/* This file must be included late, for redefinitions to take effect. */

#ifndef _LOCORE

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_fcntl.h>
#include <compat/linux/linux_termios.h>
struct proc;
#include <compat/linux/linux_ioctl.h>

#define	INT_LIMIT(x)		(~((x)1 << (sizeof(x)*8 - 1)))
#define	OFFSET_MAX		INT_LIMIT(long long)
#define	OFFT_OFFSET_MAX		INT_LIMIT(long)

#undef	O_RDONLY
#undef	O_WRONLY
#undef	SEEK_SET
#undef	SEEK_CUR

#define O_RDONLY		LINUX_O_RDONLY
#define O_WRONLY		LINUX_O_WRONLY
#define SEEK_SET		0
#define SEEK_CUR		1

#define	EOVERFLOW		75

#define termios			linux_termios

#define IMAXBEL			LINUX_IMAXBEL
#define IGNBRK			LINUX_IGNBRK
#define BRKINT			LINUX_BRKINT
#define PARMRK			LINUX_PARMRK
#define ISTRIP			LINUX_ISTRIP
#define INLCR			LINUX_INLCR
#define IGNCR			LINUX_IGNCR
#define ICRNL			LINUX_ICRNL
#define IXON			LINUX_IXON
#define OPOST			LINUX_OPOST
#define ECHO			LINUX_ECHO
#define ECHONL			LINUX_ECHONL
#define ICANON			LINUX_ICANON
#define ISIG			LINUX_ISIG
#define IEXTEN			LINUX_IEXTEN
#define CSIZE			LINUX_CSIZE
#define PARENB			LINUX_PARENB
#define CS8			LINUX_CS8

#define TIOCGETA		LINUX_TCGETS
#define TIOCSETA		LINUX_TCGETS
#define TIOCSETAW		LINUX_TCSETSW
#define TIOCSETAF		LINUX_TCSETSF

#define TCSANOW			LINUX_TCSANOW
#define TCSADRAIN		LINUX_TCSADRAIN
#define TCSAFLUSH		LINUX_TCSAFLUSH

void	cfmakeraw(struct termios *);
int	tcgetattr(int, struct termios *);
int	tcsetattr(int, int, struct termios *);

#endif /* !_LOCORE */

/* linux/asm/unistd.h */
#define __NR_SYSCALL_BASE	0x900000
#define __NR_exit		(__NR_SYSCALL_BASE+  1)
#define __NR_read		(__NR_SYSCALL_BASE+  3)
#define __NR_write		(__NR_SYSCALL_BASE+  4)
#define __NR_open		(__NR_SYSCALL_BASE+  5)
#define __NR_close		(__NR_SYSCALL_BASE+  6)
#define __NR_time		(__NR_SYSCALL_BASE+ 13)
#define __NR_lseek32		(__NR_SYSCALL_BASE+ 19)
#define __NR_ioctl		(__NR_SYSCALL_BASE+ 54)
#define __NR__new_select	(__NR_SYSCALL_BASE+142)
#define __NR_select		__NR__new_select /* XXX */
#define __NR_syscall		(__NR_SYSCALL_BASE+113)

#undef	SYS_select
#define SYS_select		__NR__new_select

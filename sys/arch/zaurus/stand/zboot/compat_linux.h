/*	$OpenBSD: compat_linux.h,v 1.10 2016/03/02 15:14:44 naddy Exp $	*/

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

#define	INT_LIMIT(x)		(~((x)1 << (sizeof(x)*8 - 1)))
#define	OFFSET_MAX		INT_LIMIT(long long)
#define	OFFT_OFFSET_MAX		INT_LIMIT(long)

#undef	O_RDONLY
#undef	O_WRONLY
#undef	O_RDWR
#undef	SEEK_SET
#undef	SEEK_CUR

#define O_RDONLY		0x0000
#define O_WRONLY		0x0001
#define O_RDWR			0x0002
#define SEEK_SET		0
#define SEEK_CUR		1

#define	LINUX_EOVERFLOW		75

struct linux_stat {
	unsigned short		lst_dev;
	unsigned short		pad1;
	unsigned long		lst_ino;
	unsigned short		lst_mode;
	unsigned short		lst_nlink;
	unsigned short		lst_uid;
	unsigned short		lst_gid;
	unsigned short		lst_rdev;
	unsigned short		pad2;
	long			lst_size;
	unsigned long		lst_blksize;
	unsigned long		lst_blocks;
	long			lst_atime;
	unsigned long		unused1;
	long			lst_mtime;
	unsigned long		unused2;
	long			lst_ctime;
	unsigned long		unused3;
	unsigned long		unused4;
	unsigned long		unused5;
};

struct termios {
	unsigned long		c_iflag;
	unsigned long		c_oflag;
	unsigned long		c_cflag;
	unsigned long		c_lflag;
	unsigned char		c_line;
	unsigned char		c_cc[19];
};

#define IGNBRK			0x0000001
#define BRKINT			0x0000002
#define PARMRK			0x0000008
#define ISTRIP			0x0000020
#define INLCR			0x0000040
#define IGNCR			0x0000080
#define ICRNL			0x0000100
#define IXON			0x0000400
#define IMAXBEL			0x0002000

#define OPOST			0x0000001

#define ISIG			0x00000001
#define ICANON			0x00000002
#define ECHO			0x00000008
#define ECHONL			0x00000040
#define IEXTEN			0x00008000

#define CBAUD			0x0000100f
#define B0			0x00000000
#define B50			0x00000001
#define B75			0x00000002
#define B110			0x00000003
#define B134			0x00000004
#define B150			0x00000005
#define B200			0x00000006
#define B300			0x00000007
#define B600			0x00000008
#define B1200			0x00000009
#define B1800			0x0000000a
#define B2400			0x0000000b
#define B4800			0x0000000c
#define B9600			0x0000000d
#define B19200			0x0000000e
#define B38400			0x0000000f
#define B57600			0x00001001
#define B115200			0x00001002
#define B230400			0x00001003

#define CSIZE			0x00000030
#define PARENB			0x00000100
#define CS8			0x00000030

#define TIOCGETA		(('T' << 8) | 1)
#define TIOCSETA		(('T' << 8) | 2)
#define TIOCSETAW		(('T' << 8) | 3)
#define TIOCSETAF		(('T' << 8) | 4)

#define TCSANOW			0
#define TCSADRAIN		1
#define TCSAFLUSH		2

typedef unsigned int speed_t;

void	cfmakeraw(struct termios *);
int	cfsetspeed(struct termios *, speed_t);
int	tcgetattr(int, struct termios *);
int	tcsetattr(int, int, struct termios *);

#endif /* !_LOCORE */

/* linux/asm/unistd.h */
#define __NR_SYSCALL_BASE	0x900000
#define __NR_exit		(__NR_SYSCALL_BASE+1)
#define __NR_read		(__NR_SYSCALL_BASE+3)
#define __NR_write		(__NR_SYSCALL_BASE+4)
#define __NR_open		(__NR_SYSCALL_BASE+5)
#define __NR_close		(__NR_SYSCALL_BASE+6)
#define __NR_time		(__NR_SYSCALL_BASE+13)
#define __NR_lseek32		(__NR_SYSCALL_BASE+19)
#define __NR_ioctl		(__NR_SYSCALL_BASE+54)
#define __NR_stat		(__NR_SYSCALL_BASE+106)
#define __NR_syscall		(__NR_SYSCALL_BASE+113)
#define __NR_select		(__NR_SYSCALL_BASE+142)

#undef	SYS_select
#define SYS_select		__NR_select

/*	$OpenBSD: linux_fcntl.h,v 1.3 2002/02/04 20:04:52 provos Exp $	*/
/*	$NetBSD: linux_fcntl.h,v 1.1 1995/02/28 23:25:40 fvdl Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Various flag values used in Linux for open(2) and fcntl(2).
 */

#ifndef _LINUX_FCNTL_H
#define _LINUX_FCNTL_H

/* read/write mode for open(2) (as usual) */
#define LINUX_O_RDONLY		0x0000
#define LINUX_O_WRONLY		0x0001
#define LINUX_O_RDWR		0x0002
#define LINUX_O_ACCMODE		0x0003

/* flags used in open(2) */
#define LINUX_O_CREAT		0x0040
#define LINUX_O_EXCL		0x0080
#define LINUX_O_NOCTTY		0x0100
#define LINUX_O_TRUNC		0x0200
#define LINUX_O_APPEND		0x0400
#define LINUX_O_NDELAY		0x0800
#define LINUX_O_SYNC		0x1000

#define LINUX_FASYNC		0x2000

/* fcntl(2) operations */
#define LINUX_F_DUPFD		0
#define LINUX_F_GETFD		1
#define LINUX_F_SETFD		2
#define LINUX_F_GETFL		3
#define LINUX_F_SETFL		4
#define LINUX_F_GETLK		5
#define LINUX_F_SETLK		6
#define LINUX_F_SETLKW		7
#define LINUX_F_SETOWN		8
#define LINUX_F_GETOWN		9

#define	LINUX_F_GETLK64		12
#define	LINUX_F_SETLK64		13
#define	LINUX_F_SETLKW64	14

#define LINUX_F_RDLCK		0
#define LINUX_F_WRLCK		1
#define LINUX_F_UNLCK		2

#define LINUX_LOCK_EX		4
#define LINUX_LOCK_SH		8

/*
 * The arguments in the flock structure have a different order from the
 * BSD structure.
 */

struct linux_flock {
	short       l_type;
	short       l_whence;
	linux_off_t l_start;
	linux_off_t l_len;
	linux_pid_t l_pid;
};

struct linux_flock64 {
        short  l_type;
        short  l_whence;
        linux_loff_t l_start;
        linux_loff_t l_len;
        linux_pid_t  l_pid;
};

#endif /* _LINUX_FCNTL_H */

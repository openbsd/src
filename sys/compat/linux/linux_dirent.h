/*	$OpenBSD: linux_dirent.h,v 1.3 2002/10/28 03:39:30 fgsch Exp $	*/
/*	$NetBSD: linux_dirent.h,v 1.3 1995/10/07 06:26:59 mycroft Exp $	*/

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

#ifndef _LINUX_DIRENT_H
#define _LINUX_DIRENT_H

#define LINUX_MAXNAMLEN	255

struct linux_dirent {
	linux_ino_t	d_ino;
	linux_off_t	d_off;
	u_short		d_reclen;
	char		d_name[LINUX_MAXNAMLEN + 1];
};

struct linux_dirent64 {
	linux_ino64_t	d_ino;
	linux_off64_t	d_off;
	u_short		d_reclen;
	u_char		d_type;
	char		d_name[LINUX_MAXNAMLEN + 1];
};

#define LINUX_NAMEOFF(dp)       ((char *)&(dp)->d_name - (char *)dp)
#define LINUX_RECLEN(de,namlen) ALIGN((LINUX_NAMEOFF(de) + (namlen) + 1))

#endif /* !_LINUX_DIRENT_H */

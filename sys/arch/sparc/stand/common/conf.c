/*	$OpenBSD: conf.c,v 1.1 1997/09/17 10:46:17 downsj Exp $	*/
/*	$NetBSD: conf.c,v 1.2 1995/09/18 21:31:45 pk Exp $ */

/*
 * Copyright (c) 1993 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <lib/libsa/stand.h>
#include <lib/libsa/ufs.h>
#include <netinet/in.h>
#include <lib/libsa/cd9660.h>
#include <lib/libsa/nfs.h>
 
struct fs_ops file_system_ufs[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
};
struct fs_ops file_system_cd9660[] = {
	{ cd9660_open, cd9660_close, cd9660_read, cd9660_write, cd9660_seek,
	  cd9660_stat },
};
struct fs_ops file_system_nfs[] = {
	{ nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat },
};
struct fs_ops file_system[2];
int nfsys = 1;


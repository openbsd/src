/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $arla: xfs_debug.h,v 1.5 2002/09/14 09:56:01 tol Exp $ */

#ifndef __NNPFS_DEBUG_H
#define __NNPFS_DEBUG_H

/* 
 * These are GLOBAL xfs debugging masks
 *
 *   Define HAVE_XDEB in your local xfs_deb.h if
 *   you want your fs to handle the debugging flags.
 */

/* Masks for the debug macro */
#define XDEBDEV		0x00000001	/* device handling */
#define XDEBMSG		0x00000002	/* message sending */
#define XDEBDNLC	0x00000004	/* name cache */
#define XDEBNODE	0x00000008	/* xfs nodes */
#define XDEBVNOPS	0x00000010	/* vnode operations */
#define XDEBVFOPS	0x00000020	/* vfs operations */
#define XDEBLKM         0x00000040	/* LKM handling */
#define XDEBSYS	        0x00000080	/* syscalls */
#define XDEBMEM		0x00000100	/* memory allocation */
#define XDEBREADDIR     0x00000200      /* readdir (linux) */
#define XDEBLOCK	0x00000400	/* locking (linux) */
#define XDEBCACHE       0x00000800      /* Cache handeling (linux) */
#define XDEBREF         0x00001000      /* track reference count */

#endif

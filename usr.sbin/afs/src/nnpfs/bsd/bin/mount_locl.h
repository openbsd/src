/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

/* $arla: mount_locl.h,v 1.10 2002/09/17 18:54:51 lha Exp $ */

#ifndef __mount_locl_h__
#define __mount_locl_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

#if defined(__osf__)

/* rename macros */
#define MNT_RDONLY		M_RDONLY
#define MNT_EXRDONLY		M_EXRDONLY
#define MNT_EXPORTED		M_EXPORTED
#define MNT_UPDATE		M_UPDATE
#define MNT_NOEXEC		M_NOEXEC
#define MNT_NOSUID		M_NOSUID
#define MNT_NODEV		M_NODEV
#define MNT_SYNCHRONOUS		M_SYNCHRONOUS
#define MNT_QUOTA		M_QUOTA
#define MNT_LOCAL		M_LOCAL
#define MNT_VISFLAGMASK		M_VISFLAGMASK
#define MNT_FMOUNT		M_FMOUNT
/* these are not defined */
#define MNT_NOACCESSTIME	0
#define MNT_NOATIME		0
#define MNT_UNION		0
#define MNT_ASYNC		0
#define MNT_RELOAD		0

#define unmount umount

#define MOUNT_NNPFS MOUNT_PC

#endif

#if defined(HAVE_OPTRESET) && !defined(HAVE_OPTRESET_DECLARATION)
extern int optreset;
#endif

#endif /* __mount_locl_h__ */

/*	$OpenBSD: linux_mmap.h,v 1.2 1996/04/17 05:23:56 mickey Exp $	*/
/*	$NetBSD: linux_mmap.h,v 1.1 1995/02/28 23:25:52 fvdl Exp $	*/
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

#ifndef _LINUX_MMAP_H
#define _LINUX_MMAP_H

#define LINUX_PROT_NONE		0x00
#define LINUX_PROT_READ		0x01
#define LINUX_PROT_WRITE	0x02
#define LINUX_PROT_EXEC		0x04

#define LINUX_MAP_SHARED	0x0001
#define LINUX_MAP_PRIVATE	0x0002

#define LINUX_MAP_FIXED		0x0010
#define LINUX_MAP_ANON		0x0020

/* the following flags are silently ignored */

#define LINUX_MAP_GROWSDOWN	0x0400
#define LINUX_MAP_DENYWRITE	0x0800
#define LINUX_MAP_EXECUTABLE	0x1000

#endif /* !_LINUX_MMAP_H */

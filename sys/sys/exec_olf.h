/*	$OpenBSD: exec_olf.h,v 1.10 2008/06/04 22:12:53 deraadt Exp $	*/
/*
 * Copyright (c) 1996 Erik Theisen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * OLF is a modified ELF that attempts to fix two serious shortcomings in
 * the SVR4 ABI.  Namely a lack of an operating system and strip tag.
 */

#ifndef _SYS_EXEC_OLF_H_
#define _SYS_EXEC_OLF_H_

#include <sys/exec_elf.h>

/*
 * Please help make this list definative.
 */
/* e_ident[] system */
#define OOS_NULL	0		/* invalid */
#define OOS_OPENBSD	1		/* OpenBSD */
#define OOS_NETBSD	2		/* NetBSD */
#define OOS_FREEBSD	3		/* FreeBSD */
#define OOS_44BSD	4		/* 4.4BSD */
#define OOS_LINUX	5		/* Linux */
#define OOS_SVR4	6		/* AT&T System V Release 4 */
#define OOS_ESIX	7		/* esix UNIX */
#define OOS_SOLARIS	8		/* SunSoft Solaris */
#define OOS_IRIX	9		/* SGI IRIX */
#define OOS_SCO		10		/* SCO UNIX */
#define OOS_DELL	11		/* DELL SVR4 */
#define OOS_NCR		12		/* NCR SVR4 */
#define OOS_NUM		13		/* Number of systems */

/* The rest of the types and defines come from the ELF header file */
#endif /* _SYS_EXEC_OLF_H_ */

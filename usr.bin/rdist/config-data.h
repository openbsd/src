/*	$OpenBSD: config-data.h,v 1.4 1998/06/26 21:21:02 millert Exp $	*/

/*
 * Copyright (c) 1993 Michael A. Cooper
 * Copyright (c) 1993 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $From: config-data.h,v 6.3 1995/12/13 01:55:26 mcooper Exp $
 * @(#)configdata.h
 */

#ifndef __configdata_h__
#define __configdata_h__

/*
 * Configuration data
 */

/*
 * Define the read and write values for the file descriptor array
 * used by pipe().
 */
#define PIPE_READ		0
#define PIPE_WRITE		1

/*
 * Directory information
 */
#if	DIR_TYPE == DIR_DIRECT
#include 	<sys/dir.h>
typedef 	struct direct		DIRENTRY;
#define 	D_NAMLEN(p)		((p)->d_namlen)
#endif	/* DIR_DIRECT */

#if	DIR_TYPE == DIR_DIRENT
#include 	<dirent.h>
typedef 	struct dirent		DIRENTRY;
#define 	D_NAMLEN(p)		(strlen((p)->d_name))
#endif	/* DIR_DIRENT */

/*
 * Set a default buffering type.
 */
#if	!defined(SETBUF_TYPE)
#define 	SETBUF_TYPE		SETBUF_SETLINEBUF
#endif	/* SETBUF_TYPE */

/*
 * Set a default get socket pair type.
 */
#if	!defined(SOCKPAIR_TYPE)
#define 	SOCKPAIR_TYPE		SOCKPAIR_SOCKETPAIR
#endif	/* SOCKPAIR_TYPE */

/*
 * Set default write(2) return and amount types.
 */
#if	!defined(WRITE_RETURN_T)
#define		WRITE_RETURN_T		int	/* What write() returns */
#endif	/* WRITE_RETURN_T */
#if	!defined(WRITE_AMT_T)
#define		WRITE_AMT_T		int	/* Amount to write */
#endif	/* WRITE_AMT_T */

#endif	/* __configdata_h__ */

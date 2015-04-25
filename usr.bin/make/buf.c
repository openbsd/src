/*	$OpenBSD: buf.c,v 1.26 2015/04/25 15:33:47 espie Exp $	*/
/*	$NetBSD: buf.c,v 1.9 1996/12/31 17:53:21 christos Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*-
 * buf.c --
 *	Functions for automatically expanded buffers.
 */

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "defines.h"
#include "buf.h"
#include "stats.h"
#include "memory.h"

#ifdef STATS_BUF
#define DO_STAT_BUF(bp, nb)					\
	STAT_BUFS_EXPANSION++;			\
	if ((bp)->endPtr - (bp)->buffer == 1)			\
		STAT_WEIRD_INEFFICIENT++;
#else
#define DO_STAT_BUF(a, b)
#endif

static void
fatal_overflow()
{
	fprintf(stderr, "buffer size overflow\n");
	exit(2);
}

/* BufExpand(bp, nb)
 *	Expand buffer bp to hold upto nb additional
 *	chars.	Makes sure there's room for an extra '\0' char at
 *	the end of the buffer to terminate the string.	*/
#define BufExpand(bp,nb)				\
do {							\
	size_t   occupied = (bp)->inPtr - (bp)->buffer;	\
	size_t   size = (bp)->endPtr - (bp)->buffer;	\
	DO_STAT_BUF(bp, nb);				\
							\
	do {						\
		if (size <= SIZE_MAX/2) {		\
			size *= 2 ;			\
		} else {				\
			fatal_overflow();		\
		}					\
	} while (size - occupied < (nb)+1+BUF_MARGIN);	\
	(bp)->buffer = (bp)->inPtr = (bp)->endPtr = 	\
		erealloc((bp)->buffer, size);		\
	(bp)->inPtr += occupied;			\
	(bp)->endPtr += size;				\
} while (0);

#define BUF_DEF_SIZE	256	/* Default buffer size */
#define BUF_MARGIN	256	/* Make sure we are comfortable */

/* the hard case for Buf_AddChar: buffer must be expanded to accommodate
 * one more char.  */
void
BufOverflow(Buffer bp)
{
	BufExpand(bp, 1);
}


void
Buf_AddChars(Buffer bp, size_t numBytes, const char *bytesPtr)
{

	if ((size_t)(bp->endPtr - bp->inPtr) < numBytes+1)
		BufExpand(bp, numBytes);

	memcpy(bp->inPtr, bytesPtr, numBytes);
	bp->inPtr += numBytes;
}

void
Buf_printf(Buffer bp, const char *fmt, ...)
{
	va_list va;
	int n;
	va_start(va, fmt);
	n = vsnprintf(bp->inPtr, bp->endPtr - bp->inPtr, fmt, va);
	va_end(va);
	if (n > bp->endPtr - bp->inPtr) {
		va_list vb;
		BufExpand(bp, n);
		va_start(vb, fmt);
		(void)vsnprintf(bp->inPtr, bp->endPtr - bp->inPtr, fmt, vb);
		va_end(vb);
	}
	bp->inPtr += n;
}

void
Buf_Init(Buffer bp, size_t size)
{
#ifdef STATS_BUF
	STAT_TOTAL_BUFS++;
	if (size == 0)
		STAT_DEFAULT_BUFS++;
	if (size == 1)
		STAT_WEIRD_BUFS++;
#endif
	if (size == 0)
		size = BUF_DEF_SIZE;
	bp->inPtr = bp->endPtr = bp->buffer = emalloc(size);
	bp->endPtr += size;
}

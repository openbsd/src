/*	$OpenBSD: buf.c,v 1.12 1999/12/16 17:02:45 espie Exp $	*/
/*	$NetBSD: buf.c,v 1.9 1996/12/31 17:53:21 christos Exp $	*/

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code modifications for the OpenBSD project.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)buf.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: buf.c,v 1.12 1999/12/16 17:02:45 espie Exp $";
#endif
#endif /* not lint */

/*-
 * buf.c --
 *	Functions for automatically-expanded buffers.
 */

#include    "sprite.h"
#include    "make.h"
#include    "buf.h"

/* BufExpand --
 * 	Expand the given buffer to hold the given number of additional
 *	chars.  Makes sure there's room for an extra '\0' char at 
 *	the end of the buffer to terminate the string.  */
#define BufExpand(bp,nb) 				\
do {							\
    size_t   occupied = (bp)->inPtr - (bp)->buffer;	\
    size_t   size = (bp)->endPtr - (bp)->buffer;	\
							\
    do { 						\
	size *= 2 ; 					\
    } while (size - occupied < (nb)+1+BUF_MARGIN);	\
    (bp)->buffer = (bp)->inPtr = (bp)->endPtr = 	\
	erealloc((bp)->buffer, size); 			\
    (bp)->inPtr += occupied;				\
    (bp)->endPtr += size;				\
} while (0);

#define BUF_DEF_SIZE	256 	/* Default buffer size */
#define BUF_MARGIN	256	/* Make sure we are comfortable */

/* Buf_AddChar hard case: buffer must be expanded to accommodate 
 * one more char.  */
void
BufOverflow(bp)
    Buffer bp;
{
    BufExpand(bp, 1);
}

void
Buf_AddChars(bp, numBytes, bytesPtr)
    Buffer 	bp;
    size_t	numBytes;
    const char 	*bytesPtr;
{

    if (bp->endPtr - bp->inPtr < numBytes+1)
	BufExpand(bp, numBytes);

    memcpy(bp->inPtr, bytesPtr, numBytes);
    bp->inPtr += numBytes;
}

void
Buf_Init(bp, size)
    Buffer 	bp;	/* New Buffer */
    size_t    	size;	/* Initial size for the buffer */
{
    if (size == 0) {
	size = BUF_DEF_SIZE;
    }
    bp->inPtr = bp->endPtr = bp->buffer = emalloc(size);
    bp->endPtr += size;
}

void
Buf_Destroy(buf)
    Buffer  buf;  	/* Buffer to destroy */
{
    free(buf->buffer);
}

void
Buf_ReplaceLastChar(buf, byte)
    Buffer 	buf;	/* buffer to augment */
    char 	byte;	/* byte to be written */
{
    if (buf->inPtr == buf->buffer)
        Buf_AddChar(buf, byte);
    else
        *(buf->inPtr - 1) = byte;
}

/*	$OpenBSD: buf.c,v 1.9 1999/12/16 16:27:12 espie Exp $	*/
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
static char rcsid[] = "$OpenBSD: buf.c,v 1.9 1999/12/16 16:27:12 espie Exp $";
#endif
#endif /* not lint */

/*-
 * buf.c --
 *	Functions for automatically-expanded buffers.
 */

#include    "sprite.h"
#include    "make.h"
#include    "buf.h"

#ifndef max
#define max(a,b)  ((a) > (b) ? (a) : (b))
#endif

/*
 * BufExpand --
 * 	Expand the given buffer to hold the given number of additional
 *	chars.
 *	Makes sure there's room for an extra NULL char at the end of the
 *	buffer in case it holds a string.
 */
#define BufExpand(bp,nb) 					\
do {								\
    char  *newBuf; 						\
    size_t   newSize = (bp)->size; 				\
								\
    do { 							\
	newSize *= 2 ; 						\
	(bp)->left = newSize - ((bp)->inPtr - (bp)->buffer); 	\
    } while ((bp)->left < (nb)+1+BUF_MARGIN);			\
    newBuf = erealloc((bp)->buffer, newSize); 		\
    (bp)->inPtr = newBuf + ((bp)->inPtr - (bp)->buffer); 	\
    (bp)->outPtr = newBuf + ((bp)->outPtr - (bp)->buffer); 	\
    (bp)->buffer = newBuf; 					\
    (bp)->size = newSize; 					\
} while (0)

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

/*-
 *-----------------------------------------------------------------------
 * Buf_AddChars --
 *	Add a number of chars to the buffer.
 *
 * Side Effects:
 *	Guess what?
 *
 *-----------------------------------------------------------------------
 */
void
Buf_AddChars(bp, numBytes, bytesPtr)
    Buffer 	bp;
    size_t	numBytes;
    const char 	*bytesPtr;
{

    if (bp->left < numBytes+1)
	BufExpand(bp, numBytes);

    memcpy(bp->inPtr, bytesPtr, numBytes);
    bp->inPtr += numBytes;
    bp->left -= numBytes;
}

/*-
 *-----------------------------------------------------------------------
 * Buf_GetAll --
 *	Get all the available data at once.
 *
 * Results:
 *	A pointer to the data and the number of chars available.
 *
 *-----------------------------------------------------------------------
 */
char *
Buf_GetAll(bp, numBytesPtr)
    Buffer 	bp;
    size_t	*numBytesPtr;
{

    if (numBytesPtr != NULL) {
	*numBytesPtr = bp->inPtr - bp->outPtr;
    }

    *bp->inPtr = 0;
    return (bp->outPtr);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Reset -
 *	Throw away all chars in a buffer.
 *
 * Side Effects:
 *	The chars are discarded.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_Reset(bp)
    Buffer 	bp;
{

    bp->inPtr = bp->outPtr = bp->buffer;
    bp->left = bp->size;
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Size --
 *	Returns the number of chars in the given buffer. Doesn't include
 *	the null-terminating char.
 *
 * Results:
 *	The number of chars.
 *
 *-----------------------------------------------------------------------
 */
int
Buf_Size(buf)
    Buffer  buf;
{
    return (buf->inPtr - buf->outPtr);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Init --
 *	Initialize a buffer. If no initial size is given, a reasonable
 *	default is used.
 *
 * Results:
 *	A buffer to be given to other functions in this library.
 *
 * Side Effects:
 *	The buffer is created, the space allocated and pointers
 *	initialized.
 *
 *-----------------------------------------------------------------------
 */
Buffer
Buf_Init(size)
    size_t    	size;	/* Initial size for the buffer */
{
    Buffer 	bp;	/* New Buffer */

    bp = (Buffer)emalloc(sizeof(*bp));

    if (size == 0) {
	size = BUF_DEF_SIZE;
    }
    bp->left = bp->size = size;
    bp->buffer = emalloc(size);
    bp->inPtr = bp->outPtr = bp->buffer;

    return (bp);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Destroy --
 *	Nuke a buffer and all its resources.
 *
 * Side Effects:
 *	The buffer is freed.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_Destroy(buf, freeData)
    Buffer  buf;  	/* Buffer to destroy */
    Boolean freeData;	/* TRUE if the data should be destroyed as well */
{

    if (freeData) {
	free(buf->buffer);
    }
    free ((char *)buf);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_ReplaceLastChar --
 *     Replace the last char in a buffer.
 *
 * Side Effects:
 *     If the buffer was empty intially, then a new byte will be added.
 *     Otherwise, the last byte is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_ReplaceLastChar(buf, byte)
    Buffer 	buf;	/* buffer to augment */
    char 	byte;	/* byte to be written */
{
    if (buf->inPtr == buf->outPtr)
        Buf_AddChar(buf, byte);
    else
        *(buf->inPtr - 1) = byte;
}

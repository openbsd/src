/*	$OpenBSD: buf.h,v 1.11 1999/12/16 17:02:45 espie Exp $	*/
/*	$NetBSD: buf.h,v 1.7 1996/12/31 17:53:22 christos Exp $	*/

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
 *
 *	from: @(#)buf.h	8.1 (Berkeley) 6/6/93
 */

/*-
 * buf.h --
 *	Header for users of the buf library.
 */

#ifndef _BUF_H
#define _BUF_H

#include    "sprite.h"

typedef struct Buffer_ {
    char    *buffer;	/* The buffer itself. */
    char    *inPtr;	/* Place to write to. */
    char    *endPtr;	/* End of allocated space. */
} BUFFER;

typedef BUFFER *Buffer;

/* Internal support for Buf_AddChar.  */
void BufOverflow __P((Buffer));

/* User interface */

/* Buf_AddChar -- Add a single char to buffer. */
#define	Buf_AddChar(bp, byte) 			\
do {			      			\
	if ((bp)->endPtr - (bp)->inPtr <= 1)	\
	    BufOverflow(bp);			\
	*(bp)->inPtr++ = (byte);		\
} while (0)					

#define BUF_ERROR 256

/* Buf_AddChars -- Add a number of chars to the buffer.  */
void Buf_AddChars __P((Buffer, size_t, const char *));
/* Buf_Reset -- Remove all chars from a buffer.  */
#define Buf_Reset(bp)	((void)((bp)->inPtr = (bp)->buffer))
/* Buf_AddSpace -- Add a space to buffer.  */
#define Buf_AddSpace(b)			Buf_AddChar((b), ' ')
/* Buf_AddString -- Add the contents of a NULL terminated string to buffer.  */
#define Buf_AddString(b, s)		Buf_AddChars((b), strlen(s), (s))
/* Buf_AddInterval -- Add characters between pointers s and e to buffer.  */
#define Buf_AddInterval(b, s, e) 	Buf_AddChars((b), (e) - (s), (s))

/* Buf_Retrieve -- Retrieve data from a buffer, as a NULL terminated string.  */
#define Buf_Retrieve(bp)	(*(bp)->inPtr = '\0', (bp)->buffer)

/* Buf_Size -- Return the number of chars in the given buffer. 
 *	Doesn't include the null-terminating char.  */
#define Buf_Size(bp)	((size_t)((bp)->inPtr - (bp)->buffer))

/* Buf_Init -- Initialize a buffer. If no initial size is given, 
 *	a reasonable default is used.  */
void Buf_Init __P((Buffer, size_t));
/* Buf_Destroy -- Nuke a buffer and all its resources.  */
void Buf_Destroy __P((Buffer));
/* Buf_ReplaceLastChar -- Replace the last char in a buffer.  */
void Buf_ReplaceLastChar __P((Buffer, char));

#endif /* _BUF_H */

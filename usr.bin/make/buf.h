/*	$OpenBSD: buf.h,v 1.7 1999/12/09 18:18:24 espie Exp $	*/
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

typedef struct Buffer {
    size_t  size; 	/* Current size of the buffer */
    size_t  left;	/* Space left (== size - (inPtr - buffer)) */
    char    *buffer;	/* The buffer itself */
    char    *inPtr;	/* Place to write to */
    char    *outPtr;	/* Place to read from */
} *Buffer;

/* Buf_AddChar adds a single char to a buffer. */
#define	Buf_AddChar(bp, byte) \
	(void) (--(bp)->left == 0 ? Buf_OvAddChar(bp, byte), 1 : \
		(*(bp)->inPtr++ = (byte), *(bp)->inPtr = 0), 1)

#define BUF_ERROR 256

void Buf_OvAddChar __P((Buffer, char));

/* Buf_AddChars -- Add a number of chars to the buffer.  */
void Buf_AddChars __P((Buffer, size_t, const char *));
/* Buf_AddSpace -- Add a space to buffer.  */
#define Buf_AddSpace(b)			Buf_AddChar((b), ' ')
/* Buf_AddString -- Add the contents of a NULL terminated string to buffer.  */
#define Buf_AddString(b, s)		Buf_AddChars((b), strlen(s), (s))
/* Buf_AddInterval -- Add characters between pointers s and e to buffer.  */
#define Buf_AddInterval(b, s, e) 	Buf_AddChars((b), (e) - (s), (s))


char *Buf_GetAll __P((Buffer, size_t *));
void Buf_Discard __P((Buffer, size_t));
int Buf_Size __P((Buffer));
Buffer Buf_Init __P((size_t));
void Buf_Destroy __P((Buffer, Boolean));
void Buf_ReplaceLastChar __P((Buffer, char));

#endif /* _BUF_H */

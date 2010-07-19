#ifndef _BUF_H
#define _BUF_H

/*	$OpenBSD: buf.h,v 1.19 2010/07/19 19:46:43 espie Exp $	*/
/*	$NetBSD: buf.h,v 1.7 1996/12/31 17:53:22 christos Exp $ */

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
 *
 *	from: @(#)buf.h 8.1 (Berkeley) 6/6/93
 */

/*
 * buf
 *	Support for extensible char buffers.
 *	One adds chars to a buffer, then retrieves the contents using
 *	Buf_Retrieve (no copy involved), or releases the memory using
 *	Buf_Destroy.
 */


/* Internal data structures and functions. BUFFER is visible so
 * that users can allocate the memory themselves.  */
typedef struct Buffer_ {
    char    *buffer;	/* The buffer itself. */
    char    *inPtr;	/* Place to write to. */
    char    *endPtr;	/* End of allocated space. */
} BUFFER;

/* Internal support for Buf_AddChar.  */
extern void BufOverflow(Buffer);


/* User interface */

/* Buf_AddChars(buf, n, str);
 *	Adds n chars to buffer buf starting from str. */
extern void Buf_AddChars(Buffer, size_t, const char *);
/* Buf_Reset(buf);
 *	Empties buffer.  */
#define Buf_Reset(bp)	((void)((bp)->inPtr = (bp)->buffer))
/* n = Buf_Size(buf);
 *	Returns number of chars currently in buf.
 *	Doesn't include the null-terminating char.  */
#define Buf_Size(bp)	((size_t)((bp)->inPtr - (bp)->buffer))
/* Buf_Init(buf, init);
 *	Initializes a buffer, to hold approximately init chars.
 *	Set init to 0 if you have no idea.  */
extern void Buf_Init(Buffer, size_t);
/* Buf_Destroy(buf);
 * 	Nukes a buffer and all its resources.	*/
#define Buf_Destroy(bp) ((void)free((bp)->buffer))
/* str = Buf_Retrieve(buf);
 *	Retrieves data from a buffer, as a NULL terminated string.  */
#define Buf_Retrieve(bp)	(*(bp)->inPtr = '\0', (bp)->buffer)
/* Buf_AddChar(buf, c);
 *	Adds a single char to buffer.	*/
#define Buf_AddChar(bp, byte)			\
do {						\
	if ((bp)->endPtr - (bp)->inPtr <= 1)	\
	    BufOverflow(bp);			\
	*(bp)->inPtr++ = (byte);		\
} while (0)

/* Buf_AddSpace(buf);
 *	Adds a space to buffer.  */
#define Buf_AddSpace(b) 		Buf_AddChar((b), ' ')
/* Buf_AddString(buf, str);
 *	Adds the contents of a NULL terminated string to buffer.  */
#define Buf_AddString(b, s)		Buf_AddChars((b), strlen(s), (s))
/* Buf_Addi(buf, s, e);
 *	Adds characters between s and e to buffer.  */
#define Buf_Addi(b, s, e)	Buf_AddChars((b), (e) - (s), (s))

/* Buf_KillTrailingSpaces(buf);
 *	Removes non-backslashed spaces at the end of a buffer. */
extern void Buf_KillTrailingSpaces(Buffer);

#endif /* _BUF_H */

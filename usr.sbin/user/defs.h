/* $OpenBSD: defs.h,v 1.3 2002/08/12 00:42:56 aaron Exp $ */
/* $NetBSD: defs.h,v 1.5 1999/12/24 09:08:49 agc Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair G. Crooks.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef DEFS_H_
#define DEFS_H_

#define NEWARRAY(type,ptr,size,action) do {				\
	if ((ptr = (type *) calloc(size, sizeof(type))) == (type *) NULL) { \
		warn("can't allocate %ld bytes", (long)(size * sizeof(type))); \
		action;							\
	}								\
} while( /* CONSTCOND */ 0)

#define RENEW(type,ptr,size,action) do {				\
	if ((ptr = (type *) realloc(ptr, sizeof(type) * size)) == (type *) NULL) { \
		warn("can't realloc %ld bytes", (long)(size * sizeof(type))); \
		action;							\
	}								\
} while( /* CONSTCOND */ 0)

#define NEW(type, ptr, action)	NEWARRAY(type, ptr, 1, action)

#define FREE(ptr)	(void) free(ptr)

#ifndef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif

#endif /* !DEFS_H_ */

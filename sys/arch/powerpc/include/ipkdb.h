/*	$OpenBSD: ipkdb.h,v 1.4 2001/09/01 15:49:05 drahn Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* register array */
#define	FIX	0
#define	LR	32
#define	CR	33
#define	CTR	34
#define	XER	35
#define	PC	36
#define	MSR	37
#define	NREG	38

#ifndef _LOCORE
extern int ipkdbregs[NREG];

/* Doesn't handle overlapping regions */
__inline extern void
ipkdbcopy(s,d,n)
	void *s, *d;
	int n;
{
	char *sp = s, *dp = d;
	
	while (--n >= 0)
		*dp++ = *sp++;
}

__inline extern void
ipkdbzero(d,n)
	void *d;
	int n;
{
	char *dp = d;
	
	while (--n >= 0)
		*dp++ = 0;
}

__inline extern int
ipkdbcmp(s,d,n)
	void *s, *d;
{
	char *sp = s, *dp = d;
	
	while (--n >= 0)
		if (*sp++ != *dp++)
			return *--dp - *--sp;
	return 0;
}
#endif /* _LOCORE */

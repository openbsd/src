/*	$OpenBSD: file.h,v 1.6 2002/06/10 21:05:25 maja Exp $ */

/*
 * Copyright (c) 1993-95 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 *	$OpenBSD: file.h,v 1.6 2002/06/10 21:05:25 maja Exp $
 *
 */

#ifndef _FILE_H_
#define _FILE_H_

void	mopFilePutLX(u_char *, int, u_long, int);
void	mopFilePutBX(u_char *, int, u_long, int);
u_long	mopFileGetLX(u_char *, int, int);
u_long	mopFileGetBX(u_char *, int, int);
void	mopFileSwapX(u_char *, int, int);
int	CheckMopFile(int);
int	GetMopFileInfo(int, u_long *, u_long *);
int	CheckAOutFile(int);
int	GetAOutFileInfo(int, u_long *, u_long *, u_long *, u_long *,
			u_long *, u_long *, u_long *, u_long *, int *);
int	GetFileInfo(int, u_long *, u_long *, int *, u_long *, u_long *,
		    u_long *, u_long *, u_long *, u_long *);

#endif /* _FILE_H_ */

/*	$OpenBSD: get.h,v 1.2 1996/09/21 19:11:36 maja Exp $ */

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
 *	$OpenBSD: get.h,v 1.2 1996/09/21 19:11:36 maja Exp $
 *
 */

#ifndef _GET_H_
#define _GET_H_

#ifdef NO__P
u_char	mopGetChar   (/* u_char *, int * */);
u_short	mopGetShort  (/* u_char *, int * */);
u_long	mopGetLong   (/* u_char *, int * */);
void	mopGetMulti  (/* u_char *, int *,u_char *,int */);
int	mopGetTrans  (/* u_char *, int */);
void	mopGetHeader (/* u_char *, int *, u_char **, u_char **, u_short *,
			 int *, int */);
u_short	mopGetLength (/* u_char *, int */);
#else
__BEGIN_DECLS
u_char	mopGetChar   __P((u_char *,int *));
u_short	mopGetShort  __P((u_char *,int *));
u_long	mopGetLong   __P((u_char *,int *));
void	mopGetMulti  __P((u_char *,int *,u_char *,int));
int	mopGetTrans  __P((u_char *, int));
void	mopGetHeader __P((u_char *, int *, u_char **, u_char **, u_short *,
			  int *, int));
u_short	mopGetLength __P((u_char *, int));
__END_DECLS
#endif

#endif _GET_H_

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
 *	$Id: pf.h,v 1.1.1.1 1996/09/21 13:49:16 maja Exp $
 *
 */

#ifndef _PF_H_
#define _PF_H_

#ifdef NO__P
int	pfTrans	   (/* char * */);
int	pfInit     (/* char *, int, u_short, int */);
int	pfEthAddr  (/* int, u_char * */);
int	pfAddMulti (/* int, char *, char * */);
int	pfDelMulti (/* int, char *, char * */);
int	pfRead     (/* int, u_char *, int */);
int	pfWrite    (/* int, u_char *, int, int  */);
#else
__BEGIN_DECLS
int	pfTrans	   __P((char *));
int	pfInit     __P((char *, int, u_short, int));
int	pfEthAddr  __P((int, u_char *));
int	pfAddMulti __P((int, char *, char *));
int	pfDelMulti __P((int, char *, char *));
int	pfRead     __P((int, u_char *, int));
int	pfWrite    __P((int, u_char *, int, int));
__END_DECLS
#endif

#endif _PF_H_

/*	$OpenBSD: svr4_timod.h,v 1.3 1997/02/13 19:45:26 niklas Exp $	*/
/*	$NetBSD: svr4_timod.h,v 1.5 1996/08/30 23:06:42 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#ifndef	_SVR4_TIMOD_H_
#define	_SVR4_TIMOD_H_

#define	SVR4_TIMOD 		('T' << 8)
#define	SVR4_TI_GETINFO		(SVR4_TIMOD|140)
#define	SVR4_TI_OPTMGMT		(SVR4_TIMOD|141)
#define	SVR4_TI_BIND		(SVR4_TIMOD|142)
#define	SVR4_TI_UNBIND		(SVR4_TIMOD|143)
#define	SVR4_TI_GETMYNAME	(SVR4_TIMOD|144)
#define	SVR4_TI_GETPEERNAME	(SVR4_TIMOD|145)
#define	SVR4_TI_SETMYNAME	(SVR4_TIMOD|146)
#define	SVR4_TI_SETPEERNAME	(SVR4_TIMOD|147)

#define SVR4_TI_CONNECT_REQUEST		0x00
#define SVR4_TI_INFO_REQUEST		0x05
#define SVR4_TI_BIND_REQUEST		0x06
#define SVR4_TI_SENDTO_REQUEST		0x08

#define SVR4_TI_ACCEPT_REPLY		0x0b
#define SVR4_TI_CONNECT_REPLY		0x0c
#define SVR4_TI_INFO_REPLY		0x10
#define SVR4_TI_BIND_REPLY		0x11
#define SVR4_TI_OK_REPLY		0x13
#define SVR4_TI_RECVFROM_REPLY		0x14

#define SVR4_TI__ACCEPT_WAIT		0x10000001
#define SVR4_TI__ACCEPT_OK		0x10000002

struct svr4_netbuf {
	u_int 	 maxlen;
	u_int	 len;
	char	*buf;
};

#endif /* !_SVR4_TIMOD_H_ */

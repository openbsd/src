/*	$OpenBSD: svr4_jioctl.h,v 1.1 1997/11/04 07:45:34 niklas Exp $	 */

/*
 * Copyright (c) 1997 Niklas Hallqvist
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

/*
 * Deal with the "j" svr4 ioctls ("j" stands for "jerq", the first windowing
 * terminal).
 */

#ifndef _SVR4_SYS_JIOCTL_H_
#define _SVR4_SYS_JIOCTL_H_

#define SVR4_jIOC	('j' << 8)

#define SVR4_JBOOT	(SVR4_jIOC|1)
#define SVR4_JTERM	(SVR4_jIOC|2)
#define SVR4_JMPX	(SVR4_jIOC|3)
#define SVR4_JTIMO	(SVR4_jIOC|4)
#define SVR4_JWINSIZE	(SVR4_jIOC|5)
#define SVR4_JTIMOM	(SVR4_jIOC|6)
#define SVR4_JZOMBOOT	(SVR4_jIOC|7)
#define SVR4_JAGENT	(SVR4_jIOC|9)
#define SVR4_JTRUN	(SVR4_jIOC|10)
#define SVR4_JXTPROTO	(SVR4_jIOC|11)

struct svr4_jwinsize {
	u_int8_t	bytesx, bytesy;
	u_int16_t	bitsx, bitsy;
};

struct svr4_jerqmsg {
	u_int8_t	cmd, chan;
};

#define SVR4_C_SENDCHAR		1
#define SVR4_C_NEW		2
#define SVR4_C_UNBLK		3
#define SVR4_C_DELETE		4
#define SVR4_C_EXIT		5
#define SVR4_C_DEFUNCT		6
#define SVR4_C_SENDCHARS	7
#define SVR4_C_RESHAPE		8
#define SVR4_C_RUN		9
#define SVR4_C_NOFLOW		10
#define SVR4_C_YESFLOW		11

struct svr4_bagent {
	u_int32_t size;
	void *src;
	void *dest;
};

#endif	/* _SVR4_SYS_JIOCTL_H_ */

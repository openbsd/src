/*	$OpenBSD: iteioctl.h,v 1.3 1999/11/05 17:15:34 espie Exp $	*/
/*	$NetBSD: iteioctl.h,v 1.9 1994/10/26 02:04:02 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: iteioctl.h 1.1 90/07/09$
 *
 *	@(#)iteioctl.h	7.2 (Berkeley) 11/4/90
 */

struct itewinsize {
	int x;			/* leftedge offset to the right */
	int y;			/* topedge offset down */
	u_int width;		/* width of ite display */
	u_int height;		/* height of ite display */
	u_int depth;		/* depth of ite display */
};

struct itebell {
	u_int volume;		/* volume of bell (0-64) */
	u_int pitch;		/* pitch of bell (10-2000) */
	u_int msec;		/* duration of bell */
};
#define MAXBVOLUME (63)
#define MAXBPITCH (2000)
#define MINBPITCH (10)
#define MAXBTIME (5000)		/* 5 seconds */

struct iterepeat {
	int start;		/* number of 100/s before repeat start */
	int next;		/* number of 100/s before next repeat */
};
#define ITEMINREPEAT	5	/* mininum number of 100/s for key repeat */

#define ITEIOCSKMAP	_IOW('Z',0x70, struct kbdmap)
#define ITEIOCGKMAP	_IOR('Z',0x71, struct kbdmap)
#define ITEIOCGWINSZ	_IOR('Z',0x72, struct itewinsize)
#define ITEIOCSWINSZ	_IOW('Z',0x73, struct itewinsize)
#define ITEIOCDSPWIN	_IO('Z', 0x74)
#define ITEIOCREMWIN	_IO('Z', 0x75)
#define ITEIOCGBELL	_IOR('Z', 0x76, struct itebell)
#define ITEIOCSBELL	_IOW('Z', 0x77, struct itebell)
#define ITEIOCGREPT	_IOR('Z', 0x78, struct iterepeat)
#define ITEIOCSREPT	_IOW('Z', 0x79, struct iterepeat)
#define ITEIOCGBLKTIME _IOR('Z', 0x80, int)
#define ITEIOCSBLKTIME _IOW('Z', 0x80, int)


#define ITESWITCH	_IOW('Z',0x69, int)	/* XXX */


/*
 * $OpenBSD: amigaio.h,v 1.1 1997/01/16 09:26:25 niklas Exp $
 * $NetBSD: amigaio.h,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 */

/*
 * This describes the IO parameter block used by many standard
 * Amiga OS device drivers.
 */
#ifndef AMIGA_IO_H
#define AMIGA_IO_H

struct AmigaIO {
	u_int8_t	dum0[28];
	u_int16_t	cmd;
	u_int8_t	flags;
	int8_t		err;
	u_int32_t	actual, /* transferred */
			length;	/* please transfer this much */
	void 		*buf;	/* data buffer */
	u_int32_t	offset;	/* offset for block devices */
};

struct TimerIO {
	u_int8_t	dum0[28];
	u_int16_t	cmd;
	u_int8_t	flags;
	int8_t		err;
	u_int32_t	secs,
			usec;
};

/* flags */
#define	QuickIO		0x1

/* commands */
#define Cmd_Rst		0x1
#define Cmd_Rd		0x2
#define Cmd_Wr		0x3
#define Cmd_Upd		0x4
#define Cmd_Clr		0x5
#define Cmd_Stp		0x6
#define Cmd_Strt	0x7
#define Cmd_Flsh	0x8

#define Cmd_Addtimereq	0x9

#endif /* AMIGA_IO_H */

/* $NetBSD: qmouse.h,v 1.2 1996/03/14 23:11:40 mark Exp $ */

/*
 * Copyright (c) Scott Stevens 1995 All rights reserved
 * Copyright (c) Melvin Tang-Richardson 1995 All rights reserved
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
 *	This product includes software developed by the RiscBSD team.
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
 */

#define MOVEEVENT	0x40
#define B1TRANS		0x20
#define B2TRANS		0x10
#define B3TRANS		0x08
#define BTRANSMASK	0x38
#define B1VAL		0x04
#define B2VAL		0x02
#define B3VAL		0x01
#define BVALMASK	0x07

struct mousebufrec {
	int status;
	int x,y;
	struct timeval event_time;
};

struct mouse_state {
	signed short x, y;
	int buttons;
};

struct mouse_boundingbox {
	int x, y, a, b;
};

struct mouse_origin {
	int x, y;
};

#define QUADMOUSE_WRITEX	_IO ( 'M', 100 )
#define QUADMOUSE_WRITEY	_IO ( 'M', 101 )

#define QUADMOUSE_SETSTATE	_IOW ( 'M', 102, struct mouse_state )
#define QUADMOUSE_SETBOUNDS	_IOW ( 'M', 103, struct mouse_boundingbox )
#define QUADMOUSE_SETORIGIN	_IOW ( 'M', 104, struct mouse_origin )

#define QUADMOUSE_GETSTATE	_IOR ( 'M', 105, struct mouse_state )
#define QUADMOUSE_GETBOUNDS	_IOR ( 'M', 106, struct mouse_boundingbox )
#define QUADMOUSE_GETORIGIN	_IOR ( 'M', 107, struct mouse_origin )

#define QUADMOUSE_SETFORMAT	_IOW ( 'M', 108, char[20] )

/* End of qmouse.h */

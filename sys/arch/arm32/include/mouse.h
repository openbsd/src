/* $NetBSD: mouse.h,v 1.1 1996/03/27 20:57:18 mark Exp $ */

/*
 * Copyright (c) Mark Brinicombe 1996 All rights reserved
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

/*
#define MOUSE_BUTTON_RIGHT  0x10
#define MOUSE_BUTTON_MIDDLE 0x20
#define MOUSE_BUTTON_LEFT   0x40
*/

/* Used in pms.c */

#define BUTSTATMASK	0x07	/* Any mouse button down if any bit set */
#define BUTCHNGMASK	0x38	/* Any mouse button changed if any bit set */

#define BUT3STAT	0x01	/* Button 3 down if set */
#define BUT2STAT	0x02	/* Button 2 down if set */
#define BUT1STAT	0x04	/* Button 1 down if set */
#define BUT3CHNG	0x08	/* Button 3 changed if set */
#define BUT2CHNG	0x10	/* Button 2 changed if set */
#define BUT1CHNG	0x20	/* Button 1 changed if set */
#define MOVEMENT	0x40	/* Mouse movement detected */

/* Define user visible mouse structures */

struct mouseinfo {
	u_int status;
	int xmotion, ymotion;
};

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

/* Define mouse ioctls */

#define MOUSEIOC_WRITEX		_IO ( 'M', 100 )
#define MOUSEIOC_WRITEY		_IO ( 'M', 101 )

#define MOUSEIOC_SETSTATE	_IOW ( 'M', 102, struct mouse_state )
#define MOUSEIOC_SETBOUNDS	_IOW ( 'M', 103, struct mouse_boundingbox )
#define MOUSEIOC_SETORIGIN	_IOW ( 'M', 104, struct mouse_origin )

#define MOUSEIOC_GETSTATE	_IOR ( 'M', 105, struct mouse_state )
#define MOUSEIOC_READ		MOUSEIOC_GETSTATE
#define MOUSEIOC_GETBOUNDS	_IOR ( 'M', 106, struct mouse_boundingbox )
#define MOUSEIOC_GETORIGIN	_IOR ( 'M', 107, struct mouse_origin )

/*
 * For backwards compatibility with the current Xserver.
 * Eventually these will be removed.
 */

#define QUADMOUSE_WRITEX	MOUSEIOC_WRITEX
#define QUADMOUSE_WRITEY	MOUSEIOC_WRITEY

#define QUADMOUSE_SETSTATE	MOUSEIOC_SETSTATE
#define QUADMOUSE_SETBOUNDS	MOUSEIOC_SETBOUNDS
#define QUADMOUSE_SETORIGIN	MOUSEIOC_SETORIGIN

#define QUADMOUSE_GETSTATE	MOUSEIOC_GETSTATE
#define QUADMOUSE_GETBOUNDS	MOUSEIOC_GETBOUNDS
#define QUADMOUSE_GETORIGIN	MOUSEIOC_GETORIGIN

#define QUADMOUSE_SETFORMAT	_IOW ( 'M', 108, char[20] )

/* End of mouse.h */

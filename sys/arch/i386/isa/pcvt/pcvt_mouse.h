/*	$OpenBSD: pcvt_mouse.h,v 1.2 2000/10/07 03:12:47 aaron Exp $ */

/*
 * Copyright (c) 2000 Jean-Baptiste Marchand, Julien Montagne and Jerome Verdon
 * 
 * All rights reserved.
 *
 * This code is for mouse console support under the pcvt console driver.
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
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore, Joerg Wunsch, Scott Turner
 *	and Charles Hannum.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#define PCVTCTL_MINOR 255

/* Mouse buttons */

#define MOUSE_BUTTON1DOWN	0x0001	/* left */
#define MOUSE_BUTTON2DOWN	0x0002	/* middle */
#define MOUSE_BUTTON3DOWN	0x0004	/* right */
#define MOUSE_BUTTON4DOWN	0x0008
#define MOUSE_BUTTON5DOWN	0x0010
#define MOUSE_BUTTON6DOWN	0x0020
#define MOUSE_BUTTON7DOWN	0x0040
#define MOUSE_BUTTON8DOWN	0x0080
#define MOUSE_MAXBUTTON		31
#define MOUSE_STDBUTTONS	0x0007		/* buttons 1-3 */
#define MOUSE_EXTBUTTONS	0x7ffffff8	/* the others (28 of them!) */
#define MOUSE_BUTTONS		(MOUSE_STDBUTTONS | MOUSE_EXTBUTTONS)

#define MOUSE_COPY_BUTTON 	MOUSE_BUTTON1DOWN
#define MOUSE_PASTE_BUTTON	MOUSE_BUTTON2DOWN
#define MOUSE_EXTEND_BUTTON	MOUSE_BUTTON3DOWN

/* Motion event */

struct mouse_data {
	int	x;
	int 	y;
	int 	z;
	int 	buttons;
};

/* Click event */

struct mouse_event {
	int	id;	/* button clicked */
	int	value; 	/* number of click */
};

/* Mouse_info : either motion or click event */

typedef struct mouse_info {
	int	operation; 
	/* 
	 * The following operations are used to indicate the action
	 * when receiving a PCVT_MOUSECTL ioctl 
	 */

#define MOUSE_INIT		(1 << 0) /* Init of the cursor */
#define MOUSE_HIDE		(1 << 1) /* Hide the cursor */
#define MOUSE_MOTION_EVENT	(1 << 2) /* Motion event */
#define MOUSE_BUTTON_EVENT	(1 << 3) /* Button event */
#define MOUSED_ON               1
#define MOUSED_OFF              0
	
	union {
		struct mouse_data data;
		struct mouse_event event;
	}u;
} mouse_info_t;

struct proc *moused_proc;


/* $NetBSD: kbd.h,v 1.2 1996/03/14 23:11:21 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * kbd.h
 *
 * Keyboard ioctls
 *
 * Created      : 21/07/95
 */

struct kbd_data {
	int keycode;
	struct timeval event_time;
};

struct kbd_autorepeat {
	int ka_delay;
	int ka_rate;
};

#define KBD_GETAUTOREPEAT	_IOR( 'k', 100, struct kbd_autorepeat)
#define KBD_SETAUTOREPEAT	_IOW( 'k', 101, struct kbd_autorepeat)
#define KBD_SETLEDS		_IOW( 'k', 102, int)
#define KBD_XXX			_IOW( 'k', 103, int)

#define KBD_LED_SCROLL_LOCK	0x01
#define KBD_LED_NUM_LOCK	0x02
#define KBD_LED_CAPS_LOCK	0x04

#ifdef _KERNEL
void	kbdsetstate __P((int /*state*/));
int	kbdgetstate __P(());
#endif

/* End of kbd.h */

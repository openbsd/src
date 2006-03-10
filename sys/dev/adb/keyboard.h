/*	$OpenBSD: keyboard.h,v 1.4 2006/03/10 20:13:50 miod Exp $	*/
/*	$NetBSD: keyboard.h,v 1.1 1998/05/15 10:15:54 tsubai Exp $	*/

/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define ADBK_CAPSLOCK	0x39
#define	ADBK_RESET	0x7f

#define ADBK_KEYVAL(key)	((key) & 0x7f)
#define ADBK_PRESS(key)		(((key) & 0x80) == 0)
#define ADBK_KEYDOWN(key)	(key)
#define ADBK_KEYUP(key)		((key) | 0x80)

#ifndef KEYBOARD_ARRAY
extern unsigned char keyboard[128];
#else
#include <dev/wscons/wskbdraw.h>
unsigned char keyboard[128] = {
	RAWKEY_a,
	RAWKEY_s,
	RAWKEY_d,
	RAWKEY_f,
	RAWKEY_h,
	RAWKEY_g,
	RAWKEY_z,
	RAWKEY_x,
	RAWKEY_c,
	RAWKEY_v,
#ifdef FIX_SV_X_KBDBUG
	RAWKEY_grave,
#else
	RAWKEY_less,
#endif
	RAWKEY_b,
	RAWKEY_q,
	RAWKEY_w,
	RAWKEY_e,
	RAWKEY_r,
	RAWKEY_y,
	RAWKEY_t,
	RAWKEY_1,
	RAWKEY_2,
	RAWKEY_3,
	RAWKEY_4,
	RAWKEY_6,
	RAWKEY_5,
	RAWKEY_equal,
	RAWKEY_9,
	RAWKEY_7,
	RAWKEY_minus,
	RAWKEY_8,
	RAWKEY_0,
	RAWKEY_bracketright,
	RAWKEY_o,
	RAWKEY_u,
	RAWKEY_bracketleft,
	RAWKEY_i,
	RAWKEY_p,
	RAWKEY_Return,
	RAWKEY_l,
	RAWKEY_j,
	RAWKEY_apostrophe,
	RAWKEY_k,
	RAWKEY_semicolon,
	RAWKEY_backslash,
	RAWKEY_comma,
	RAWKEY_slash,
	RAWKEY_n,
	RAWKEY_m,
	RAWKEY_period,
	RAWKEY_Tab,
	RAWKEY_space,
#ifdef FIX_SV_X_KBDBUG
	RAWKEY_less,
#else
	RAWKEY_grave,
#endif
	RAWKEY_Delete,
	RAWKEY_KP_Enter,
	RAWKEY_Escape,
	RAWKEY_Control_L,
	219,			/* XXX */
	RAWKEY_Shift_L,
	RAWKEY_Caps_Lock,
	RAWKEY_Alt_L,
	RAWKEY_Left,
	RAWKEY_Right,
	RAWKEY_Down,
	RAWKEY_Up,
	0, /* Fn */
	0,
	RAWKEY_KP_Delete,
	0,
	RAWKEY_KP_Multiply,
	0,
	RAWKEY_KP_Add,
	0,
	RAWKEY_Num_Lock,
	0,
	0,
	0,
	RAWKEY_KP_Divide,
	RAWKEY_KP_Enter,
	0,
	RAWKEY_KP_Subtract,
	0,
	0,
	RAWKEY_KP_Equal,
	RAWKEY_KP_Insert,
	RAWKEY_KP_End,
	RAWKEY_KP_Down,
	RAWKEY_KP_Next,
	RAWKEY_KP_Left,
	RAWKEY_KP_Begin,
	RAWKEY_KP_Right,
	RAWKEY_KP_Home,
	0,
	RAWKEY_KP_Up,
	RAWKEY_KP_Prior,
	0,
	0,
	RAWKEY_KP_Delete,
	RAWKEY_f5,
	RAWKEY_f6,
	RAWKEY_f7,
	RAWKEY_f3,
	RAWKEY_f8,
	RAWKEY_f9,
	0,
	RAWKEY_f11,
	0,
	RAWKEY_Print_Screen,
	RAWKEY_KP_Enter,
	RAWKEY_Hold_Screen,
	0,
	RAWKEY_f10,
	0,
	RAWKEY_f12,
	0,
	RAWKEY_Pause,
	RAWKEY_Insert,
	RAWKEY_Home,
	RAWKEY_Prior,
	RAWKEY_Delete,
	RAWKEY_f4,
	RAWKEY_End,
	RAWKEY_f2,
	RAWKEY_Next,
	RAWKEY_f1,
	0,
	0,
	0,
	0,
	0
};
#endif /* KEYBOARD_ARRAY */

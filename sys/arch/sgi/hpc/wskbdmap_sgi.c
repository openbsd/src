/*	$OpenBSD: wskbdmap_sgi.c,v 1.1 2012/04/17 22:06:33 miod Exp $	*/
/*	$NetBSD: wskbdmap_sgi.c,v 1.5 2006/12/26 17:37:22 rumble Exp $	*/

/*
 * Copyright (c) 2004 Steve Rumble
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

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#define KC(n) KS_KEYCODE(n)

static const keysym_t wssgi_keydesctab_us[] = {
/*	pos		command			normal		shifted */
	KC(0x02),	KS_Cmd1,		KS_Control_L,
	KC(0x03),				KS_Caps_Lock,
	KC(0x04),				KS_Shift_R,
	KC(0x05),				KS_Shift_L,
	KC(0x06),	KS_Cmd_Debugger,	KS_Escape,
	KC(0x07),				KS_1,		KS_exclam,
	KC(0x08),				KS_Tab,
	KC(0x09),				KS_q,
	KC(0x0A),				KS_a,
	KC(0x0B),				KS_s,
	KC(0x0D),				KS_2,		KS_at,
	KC(0x0E),				KS_3,		KS_numbersign,
	KC(0x0F),				KS_w,
	KC(0x10),				KS_e,
	KC(0x11),				KS_d,
	KC(0x12),				KS_f,
	KC(0x13),				KS_z,
	KC(0x14),				KS_x,
	KC(0x15),				KS_4,		KS_dollar,
	KC(0x16),				KS_5,		KS_percent,
	KC(0x17),				KS_r,
	KC(0x18),				KS_t,
	KC(0x19),				KS_g,
	KC(0x1A),				KS_h,
	KC(0x1B),				KS_c,
	KC(0x1C),				KS_v,
	KC(0x1D),				KS_6,		KS_asciicircum,
	KC(0x1E),				KS_7,		KS_ampersand,
	KC(0x1F),				KS_y,
	KC(0x20),				KS_u,
	KC(0x21),				KS_j,
	KC(0x22),				KS_k,
	KC(0x23),				KS_b,
	KC(0x24),				KS_n,
	KC(0x25),				KS_8,		KS_asterisk,
	KC(0x26),				KS_9,		KS_parenleft,
	KC(0x27),				KS_i,
	KC(0x28),				KS_o,
	KC(0x29),				KS_l,
	KC(0x2A),				KS_semicolon,	KS_colon,
	KC(0x2B),				KS_m,
	KC(0x2C),				KS_comma,	KS_less,
	KC(0x2D),				KS_0,		KS_parenright,
	KC(0x2E),				KS_minus,	KS_underscore,
	KC(0x2F),				KS_p,
	KC(0x30),				KS_bracketleft,	KS_braceleft,
	KC(0x31),				KS_apostrophe,	KS_quotedbl,
	KC(0x32),				KS_Return,
	KC(0x33),				KS_period,	KS_greater,
	KC(0x34),				KS_slash,	KS_question,
	KC(0x35),				KS_equal,	KS_plus,
	KC(0x36),				KS_grave,	KS_asciitilde,
	KC(0x37),				KS_bracketright,KS_braceright,
	KC(0x38),				KS_backslash,	KS_bar,
	KC(0x39),				KS_KP_End,	KS_KP_1,
	KC(0x3A),				KS_KP_Insert,	KS_KP_0,
	KC(0x3C),				KS_Delete,
	KC(0x3D),				KS_Delete,
	KC(0x3E),				KS_KP_Left,	KS_KP_4,
	KC(0x3F),				KS_KP_Down,	KS_KP_2,
	KC(0x40),				KS_KP_Next,	KS_KP_3,
	KC(0x41),				KS_KP_Delete,	KS_KP_Decimal,
	KC(0x42),				KS_KP_Home,	KS_KP_7,
	KC(0x43),				KS_KP_Up,	KS_KP_8,
	KC(0x44),				KS_KP_Begin,	KS_KP_5,
	KC(0x45),				KS_KP_Right,	KS_KP_6,
	KC(0x48),				KS_Left,
	KC(0x49),				KS_Down,
	KC(0x4A),				KS_KP_Prior,	KS_KP_9,
	KC(0x4B),				KS_KP_Subtract,
	KC(0x4F),				KS_Right,
	KC(0x50),				KS_Up,
	KC(0x51),				KS_KP_Enter,
	KC(0x52),				KS_space,
	KC(0x53),	KS_Cmd2,		KS_Alt_L,
	KC(0x54),	KS_Cmd2,		KS_Alt_R,	KS_Multi_key,
	KC(0x55),	KS_Cmd1,		KS_Control_R,
	KC(0x56),				KS_f1,
	KC(0x57),				KS_f2,
	KC(0x58),				KS_f3,
	KC(0x59),				KS_f4,
	KC(0x5A),				KS_f5,
	KC(0x5B),				KS_f6,
	KC(0x5C),				KS_f7,
	KC(0x5D),				KS_f8,
	KC(0x5E),				KS_f9,
	KC(0x5F),				KS_f10,
	KC(0x60),				KS_f11,
	KC(0x61),				KS_f12,
	KC(0x62),				KS_Print_Screen,
	KC(0x63),				KS_Hold_Screen,
	KC(0x64),				KS_Pause,
	KC(0x65),				KS_Insert,
	KC(0x66),				KS_Home,
	KC(0x67),				KS_Prior,
	KC(0x68),				KS_End,
	KC(0x69),				KS_Next,
	KC(0x6A),				KS_Num_Lock,
	KC(0x6B),				KS_KP_Divide,
	KC(0x6C),				KS_KP_Multiply,
	KC(0x6D),				KS_KP_Add,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc wssgi_keydesctab[] = {
	KBD_MAP(KB_US,	0,	wssgi_keydesctab_us)
};

/*	$NetBSD: ukbdmap.c,v 1.5 2000/04/27 15:26:49 augustss Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <dev/usb/usb_port.h>

#define KC(n)		KS_KEYCODE(n)

Static const keysym_t ukbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(4), 			KS_a,
    KC(5), 			KS_b,
    KC(6), 			KS_c,
    KC(7), 			KS_d,
    KC(8), 			KS_e,
    KC(9), 			KS_f,
    KC(10), 			KS_g,
    KC(11), 			KS_h,
    KC(12), 			KS_i,
    KC(13), 			KS_j,
    KC(14), 			KS_k,
    KC(15), 			KS_l,
    KC(16), 			KS_m,
    KC(17), 			KS_n,
    KC(18), 			KS_o,
    KC(19), 			KS_p,
    KC(20), 			KS_q,
    KC(21), 			KS_r,
    KC(22), 			KS_s,
    KC(23), 			KS_t,
    KC(24), 			KS_u,
    KC(25), 			KS_v,
    KC(26), 			KS_w,
    KC(27), 			KS_x,
    KC(28), 			KS_y,
    KC(29), 			KS_z,
    KC(30),  			KS_1,		KS_exclam,
    KC(31),  			KS_2,		KS_at,
    KC(32),  			KS_3,		KS_numbersign,
    KC(33),  			KS_4,		KS_dollar,
    KC(34),  			KS_5,		KS_percent,
    KC(35),  			KS_6,		KS_asciicircum,
    KC(36),  			KS_7,		KS_ampersand,
    KC(37),  			KS_8,		KS_asterisk,
    KC(38), 			KS_9,		KS_parenleft,
    KC(39), 			KS_0,		KS_parenright,
    KC(40), 			KS_Return,
    KC(41),   KS_Cmd_Debugger,	KS_Escape,
    KC(42), 			KS_BackSpace,
    KC(43), 			KS_Tab,
    KC(44), 			KS_space,
    KC(45), 			KS_minus,	KS_underscore,
    KC(46), 			KS_equal,	KS_plus,
    KC(47), 			KS_bracketleft,	KS_braceleft,
    KC(48), 			KS_bracketright,KS_braceright,
    KC(49), 			KS_backslash,	KS_bar,
    KC(50),			KS_numbersign,	KS_asciitilde,
    KC(51), 			KS_semicolon,	KS_colon,
    KC(52), 			KS_apostrophe,	KS_quotedbl,
    KC(53), 			KS_grave,	KS_asciitilde,
    KC(54), 			KS_comma,	KS_less,
    KC(55), 			KS_period,	KS_greater,
    KC(56), 			KS_slash,	KS_question,
    KC(57), 			KS_Caps_Lock,
    KC(58),  KS_Cmd_Screen0,	KS_f1,
    KC(59),  KS_Cmd_Screen1,	KS_f2,
    KC(60),  KS_Cmd_Screen2,	KS_f3,
    KC(61),  KS_Cmd_Screen3,	KS_f4,
    KC(62),  KS_Cmd_Screen4,	KS_f5,
    KC(63),  KS_Cmd_Screen5,	KS_f6,
    KC(64),  KS_Cmd_Screen6,	KS_f7,
    KC(65),  KS_Cmd_Screen7,	KS_f8,
    KC(66),  KS_Cmd_Screen8,	KS_f9,
    KC(67),  KS_Cmd_Screen9,	KS_f10,
    KC(68), 			KS_f11,
    KC(69), 			KS_f12,
    KC(70),			KS_Print_Screen,
    KC(71), 			KS_Hold_Screen,
    KC(72),			KS_Pause,
    KC(73),			KS_Insert, 
    KC(74),			KS_Home,
    KC(75),			KS_Prior,
    KC(76),			KS_Delete,
    KC(77),			KS_End,
    KC(78),			KS_Next,
    KC(79),			KS_Right,
    KC(80),			KS_Left,
    KC(81),			KS_Down,
    KC(82),			KS_Up,
    KC(83), 			KS_Num_Lock,
    KC(84),			KS_KP_Divide,
    KC(85), 			KS_KP_Multiply,
    KC(86), 			KS_KP_Subtract,
    KC(87), 			KS_KP_Add,
    KC(88),			KS_KP_Enter,
    KC(89), 			KS_KP_End,	KS_KP_1,
    KC(90), 			KS_KP_Down,	KS_KP_2,
    KC(91), 			KS_KP_Next,	KS_KP_3,
    KC(92), 			KS_KP_Left,	KS_KP_4,
    KC(93), 			KS_KP_Begin,	KS_KP_5,
    KC(94), 			KS_KP_Right,	KS_KP_6,
    KC(95), 			KS_KP_Home,	KS_KP_7,
    KC(96), 			KS_KP_Up,	KS_KP_8,
    KC(97), 			KS_KP_Prior,	KS_KP_9,
    KC(98), 			KS_KP_Insert,	KS_KP_0,
    KC(99), 			KS_KP_Delete,	KS_KP_Decimal,
    KC(100),			KS_backslash,	KS_bar,
    KC(101),			KS_Menu,
/* ... many unmapped keys ... */
    KC(224),  KS_Cmd1,		KS_Control_L,
    KC(225), 			KS_Shift_L,
    KC(226),  KS_Cmd2,		KS_Alt_L,
    KC(227),			KS_Meta_L,
    KC(228),			KS_Control_R,
    KC(229), 			KS_Shift_R,
    KC(230),			KS_Alt_R,	KS_Multi_key,
    KC(231),			KS_Meta_R,
};

Static const keysym_t ukbd_keydesc_swapctrlcaps[] = {
/*  pos      command		normal		shifted */
    KC(57), 			KS_Control_L,
    KC(224), KS_Cmd1,		KS_Caps_Lock,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc ukbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	ukbd_keydesc_us),
	KBD_MAP(KB_US | KB_SWAPCTRLCAPS, KB_US,	ukbd_keydesc_swapctrlcaps),
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

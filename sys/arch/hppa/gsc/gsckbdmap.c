/*	$OpenBSD: gsckbdmap.c,v 1.1 2003/01/31 22:50:19 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <hppa/gsc/gsckbdmap.h>

#define KC(n) KS_KEYCODE(n)

static const keysym_t gsckbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(1),   KS_Cmd_Screen8,	KS_f9,
    KC(3),   KS_Cmd_Screen4,	KS_f5,
    KC(4),   KS_Cmd_Screen2,	KS_f3,
    KC(5),   KS_Cmd_Screen0,	KS_f1,
    KC(6),   KS_Cmd_Screen1,	KS_f2,
    KC(7),   KS_Cmd_Screen11,	KS_f12,
    KC(9),   KS_Cmd_Screen9,	KS_f10,
    KC(10),  KS_Cmd_Screen7,	KS_f8,
    KC(11),  KS_Cmd_Screen5,	KS_f6,
    KC(12),  KS_Cmd_Screen3,	KS_f4,
    KC(13),			KS_Tab,
    KC(14),			KS_grave,	KS_asciitilde,
    KC(17),  KS_Cmd2,		KS_Alt_L,
    KC(18),			KS_Shift_L,
    KC(20),  KS_Cmd1,		KS_Control_L,
    KC(21),			KS_q,
    KC(22),			KS_1,		KS_exclam,
    KC(26),			KS_z,
    KC(27),			KS_s,
    KC(28),			KS_a,
    KC(29),			KS_w,
    KC(30),			KS_2,		KS_at,
    KC(33),			KS_c,
    KC(34),			KS_x,
    KC(35),			KS_d,
    KC(36),			KS_e,
    KC(37),			KS_4,		KS_dollar,
    KC(38),			KS_3,		KS_numbersign,
    KC(41),			KS_space,
    KC(42),			KS_v,
    KC(43),			KS_f,
    KC(44),			KS_t,
    KC(45),			KS_r,
    KC(46),			KS_5,		KS_percent,
    KC(49),			KS_n,
    KC(50),			KS_b,
    KC(51),			KS_h,
    KC(52),			KS_g,
    KC(53),			KS_y,
    KC(54),			KS_6,		KS_asciicircum,
    KC(58),			KS_m,
    KC(59),			KS_j,
    KC(60),			KS_u,
    KC(61),			KS_7,		KS_ampersand,
    KC(62),			KS_8,		KS_asterisk,
    KC(65),			KS_comma,	KS_less,
    KC(66),			KS_k,
    KC(67),			KS_i,
    KC(68),			KS_o,
    KC(69),			KS_0,		KS_parenright,
    KC(70),			KS_9,		KS_parenleft,
    KC(73),			KS_period,	KS_greater,
    KC(74),			KS_slash,	KS_question,
    KC(75),			KS_l,
    KC(76),			KS_semicolon,	KS_colon,
    KC(77),			KS_p,
    KC(78),			KS_minus,	KS_underscore,
    KC(82),			KS_apostrophe,	KS_quotedbl,
    KC(84),			KS_bracketleft,	KS_braceleft,
    KC(85),			KS_equal,	KS_plus,
    KC(88),			KS_Caps_Lock,
    KC(89),			KS_Shift_R,
    KC(90),			KS_Return,
    KC(91),			KS_bracketright,KS_braceright,
    KC(93),			KS_backslash,	KS_bar,
    KC(102), KS_Cmd_ResetEmul,	KS_Delete, /* Backspace */
    KC(105),			KS_KP_End,	KS_KP_1,
    KC(107),			KS_KP_Left,	KS_KP_4,
    KC(108),			KS_KP_Home,	KS_KP_7,
    KC(112),			KS_KP_Insert,	KS_KP_0,
    KC(113), KS_Cmd_KbdReset,	KS_KP_Delete,	KS_KP_Decimal,
    KC(114),			KS_KP_Down,	KS_KP_2,
    KC(115),			KS_KP_Begin,	KS_KP_5,
    KC(116),			KS_KP_Right,	KS_KP_6,
    KC(117),			KS_KP_Up,	KS_KP_8,
    KC(118), KS_Cmd_Debugger,	KS_Escape,
    KC(119),			KS_Num_Lock,
    KC(120), KS_Cmd_Screen10,	KS_f11,
    KC(121),			KS_KP_Add,
    KC(122),			KS_KP_Next,	KS_KP_3,
    KC(123),			KS_KP_Subtract,
    KC(124),			KS_KP_Multiply,
    KC(125),			KS_KP_Prior,	KS_KP_9,
    KC(126),			KS_Hold_Screen,
    KC(127),			KS_Pause, /* Break */
    KC(131), KS_Cmd_Screen6,	KS_f7,
    KC(145), KS_Cmd2,		KS_Alt_R,	KS_Multi_key,
    KC(148), KS_Cmd1,		KS_Control_R,
    KC(202),			KS_KP_Divide,
    KC(218),			KS_KP_Enter,
    KC(233),			KS_End,
    KC(235),			KS_Left,
    KC(236),			KS_Home,
    KC(240),			KS_Insert,
    KC(241),			KS_Delete,
    KC(242),			KS_Down,
    KC(244),			KS_Right,
    KC(245),			KS_Up,
    KC(250), KS_Cmd_ScrollFwd,	KS_Next,
    /*
     * Print Screen produces E0 12 E0 7C when pressed, then E0 7C E0 12 when
     * depressed. Ignore the E0 12 code and recognize on E0 7C.
     */
    KC(252),			KS_Print_Screen,
    KC(253), KS_Cmd_ScrollBack,	KS_Prior,
    KC(254), KS_Cmd_ResetClose, /* CTL-Break */
};

static const keysym_t gsckbd_keydesc_swapctrlcaps[] = {
/*  pos      command		normal		shifted */
    KC(20),			KS_Caps_Lock,
    KC(88),  KS_Cmd1,		KS_Control_L,
};

static const keysym_t gsckbd_keydesc_uk[] = {
    KC(14),			KS_grave,	KS_notsign,
    KC(30),			KS_2,		KS_quotedbl,
    KC(38),			KS_3,		KS_sterling,
    KC(82),			KS_apostrophe,	KS_at,
    KC(93),			KS_numbersign,	KS_asciitilde,
    KC(97),			KS_backslash,	KS_bar,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc gsckbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	gsckbd_keydesc_us),
	KBD_MAP(KB_UK,			KB_US,	gsckbd_keydesc_uk),
	KBD_MAP(KB_US | KB_SWAPCTRLCAPS, KB_US,	gsckbd_keydesc_swapctrlcaps),
	{0, 0, 0, 0}
};

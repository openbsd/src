/*	$OpenBSD: gsckbdmap.c,v 1.2 2003/02/16 01:43:17 miod Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 */
/*	OpenBSD: wskbdmap_mfii.c,v 1.21 2003/01/04 13:40:08 maja Exp  */
/*	$NetBSD: wskbdmap_mfii.c,v 1.15 2000/05/19 16:40:04 drochner Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
#include <hppa/gsc/gsckbdmap.h>

#define KC(n) KS_KEYCODE(n)

static const keysym_t gsckbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(118),	KS_Cmd_Debugger,	KS_Escape,
    KC(22),	KS_1,	KS_exclam,
    KC(30),	KS_2,	KS_at,
    KC(38),	KS_3,	KS_numbersign,
    KC(37),	KS_4,	KS_dollar,
    KC(46),	KS_5,	KS_percent,
    KC(54),	KS_6,	KS_asciicircum,
    KC(61),	KS_7,	KS_ampersand,
    KC(62),	KS_8,	KS_asterisk,
    KC(70),	KS_9,	KS_parenleft,
    KC(69),	KS_0,	KS_parenright,
    KC(78),	KS_minus,	KS_underscore,
    KC(85),	KS_equal,	KS_plus,
    KC(102),	KS_Cmd_ResetEmul,	KS_Delete,
    KC(13),	KS_Tab,
    KC(21),	KS_q,
    KC(29),	KS_w,
    KC(36),	KS_e,
    KC(45),	KS_r,
    KC(44),	KS_t,
    KC(53),	KS_y,
    KC(60),	KS_u,
    KC(67),	KS_i,
    KC(68),	KS_o,
    KC(77),	KS_p,
    KC(84),	KS_bracketleft,	KS_braceleft,
    KC(91),	KS_bracketright,	KS_braceright,
    KC(90),	KS_Return,
    KC(20),	KS_Cmd1,	KS_Control_L,
    KC(28),	KS_a,
    KC(27),	KS_s,
    KC(35),	KS_d,
    KC(43),	KS_f,
    KC(52),	KS_g,
    KC(51),	KS_h,
    KC(59),	KS_j,
    KC(66),	KS_k,
    KC(75),	KS_l,
    KC(76),	KS_semicolon,	KS_colon,
    KC(82),	KS_apostrophe,	KS_quotedbl,
    KC(14),	KS_grave,	KS_asciitilde,
    KC(18),	KS_Shift_L,
    KC(93),	KS_backslash,	KS_bar,
    KC(26),	KS_z,
    KC(34),	KS_x,
    KC(33),	KS_c,
    KC(42),	KS_v,
    KC(50),	KS_b,
    KC(49),	KS_n,
    KC(58),	KS_m,
    KC(65),	KS_comma,	KS_less,
    KC(73),	KS_period,	KS_greater,
    KC(74),	KS_slash,	KS_question,
    KC(89),	KS_Shift_R,
    KC(124),	KS_KP_Multiply,
    KC(17),	KS_Cmd2,	KS_Alt_L,
    KC(41),	KS_space,
    KC(88),	KS_Caps_Lock,
    KC(5),	KS_Cmd_Screen0,	KS_f1,
    KC(6),	KS_Cmd_Screen1,	KS_f2,
    KC(4),	KS_Cmd_Screen2,	KS_f3,
    KC(12),	KS_Cmd_Screen3,	KS_f4,
    KC(3),	KS_Cmd_Screen4,	KS_f5,
    KC(11),	KS_Cmd_Screen5,	KS_f6,
    KC(131),	KS_Cmd_Screen6,	KS_f7,
    KC(10),	KS_Cmd_Screen7,	KS_f8,
    KC(1),	KS_Cmd_Screen8,	KS_f9,
    KC(9),	KS_Cmd_Screen9,	KS_f10,
    KC(119),	KS_Num_Lock,
    KC(126),	KS_Hold_Screen,
    KC(108),	KS_KP_Home,	KS_KP_7,
    KC(117),	KS_KP_Up,	KS_KP_8,
    KC(125),	KS_KP_Prior,	KS_KP_9,
    KC(123),	KS_KP_Subtract,
    KC(107),	KS_KP_Left,	KS_KP_4,
    KC(115),	KS_KP_Begin,	KS_KP_5,
    KC(116),	KS_KP_Right,	KS_KP_6,
    KC(121),	KS_KP_Add,
    KC(105),	KS_KP_End,	KS_KP_1,
    KC(114),	KS_KP_Down,	KS_KP_2,
    KC(122),	KS_KP_Next,	KS_KP_3,
    KC(112),	KS_KP_Insert,	KS_KP_0,
    KC(113),	KS_KP_Delete,	KS_KP_Decimal,
    KC(120),	KS_Cmd_Screen10,	KS_f11,
    KC(7),	KS_Cmd_Screen11,	KS_f12,
    KC(127),	KS_Pause,	/*	Break	*/
    KC(218),	KS_KP_Enter,
    KC(148),	KS_Cmd1,	KS_Control_R,
    KC(252),	KS_Print_Screen,
    KC(202),	KS_KP_Divide,
    KC(145),	KS_Cmd2,	KS_Alt_R,	KS_Multi_key,
#if 0
    KC(254),	KS_Cmd_ResetClose,	/*	CTL-Break	*/
#endif
    KC(236),	KS_Home,
    KC(245),	KS_Up,
    KC(253),	KS_Cmd_ScrollBack,	KS_Prior,
    KC(235),	KS_Left,
    KC(244),	KS_Right,
    KC(233),	KS_End,
    KC(242),	KS_Down,
    KC(250),	KS_Cmd_ScrollFwd,	KS_Next,
    KC(240),	KS_Insert,
    KC(113),	KS_Cmd_KbdReset,	KS_KP_Delete,
/* initially KC(219),	KS_Meta_L,	*/
/* initially KC(220),	KS_Meta_R,	*/
/* initially KC(221),	KS_Menu,	*/
    KC(241),	KS_Delete,
};

static const keysym_t gsckbd_keydesc_de[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),	KS_2,	KS_quotedbl,	KS_twosuperior,
    KC(38),	KS_3,	KS_section,	KS_threesuperior,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,	KS_braceleft,
    KC(62),	KS_8,	KS_parenleft,	KS_bracketleft,
    KC(70),	KS_9,	KS_parenright,	KS_bracketright,
    KC(69),	KS_0,	KS_equal,	KS_braceright,
    KC(78),	KS_ssharp,	KS_question,	KS_backslash,
    KC(85),	KS_dead_acute,	KS_dead_grave,
    KC(21),	KS_q,	KS_Q,	KS_at,
    KC(53),	KS_z,
    KC(84),	KS_udiaeresis,
    KC(91),	KS_plus,	KS_asterisk,	KS_dead_tilde,
    KC(76),	KS_odiaeresis,
    KC(82),	KS_adiaeresis,
    KC(14),	KS_dead_circumflex,KS_dead_abovering,
    KC(93),	KS_numbersign,	KS_apostrophe,
    KC(26),	KS_y,
    KC(58),	KS_m,	KS_M,	KS_mu,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,	KS_bar,	KS_brokenbar,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_de_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(85),	KS_apostrophe,	KS_grave,
    KC(91),	KS_plus,	KS_asterisk,	KS_asciitilde,
    KC(14),	KS_asciicircum,	KS_degree,
};

static const keysym_t gsckbd_keydesc_dk[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),	KS_2,	KS_quotedbl,	KS_at,
    KC(38),	KS_3,	KS_numbersign,	KS_sterling,
    KC(37),	KS_4,	KS_currency,	KS_dollar,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,	KS_braceleft,
    KC(62),	KS_8,	KS_parenleft,	KS_bracketleft,
    KC(70),	KS_9,	KS_parenright,	KS_bracketright,
    KC(69),	KS_0,	KS_equal,	KS_braceright,
    KC(78),	KS_plus,	KS_question,
    KC(85),	KS_dead_acute,	KS_dead_grave,	KS_bar,
    KC(84),	KS_aring,
    KC(91),	KS_dead_diaeresis,	KS_dead_circumflex,	KS_dead_tilde,
    KC(76),	KS_ae,
    KC(82),	KS_oslash,
    KC(14),	KS_onehalf,	KS_paragraph,
    KC(93),	KS_apostrophe,	KS_asterisk,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,	KS_backslash,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_dk_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(85),	KS_apostrophe,	KS_grave,	KS_bar,
    KC(91),	KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

static const keysym_t gsckbd_keydesc_sv[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(78),	KS_plus,	KS_question,	KS_backslash,
    KC(91),	KS_dead_diaeresis,	KS_dead_circumflex,	KS_dead_tilde,
    KC(76),	KS_odiaeresis,
    KC(82),	KS_adiaeresis,
    KC(14),	KS_paragraph,	KS_onehalf,
    KC(97),	KS_less,	KS_greater,	KS_bar,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_sv_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(85),	KS_apostrophe,	KS_grave,	KS_bar,
    KC(91),	KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

static const keysym_t gsckbd_keydesc_no[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(85),	KS_backslash,	KS_dead_grave,	KS_dead_acute,
    KC(91),	KS_dead_diaeresis,	KS_dead_circumflex,	KS_dead_tilde,
    KC(76),	KS_oslash,
    KC(82),	KS_ae,
    KC(14),	KS_bar,	KS_paragraph,
    KC(97),	KS_less,	KS_greater,
};

static const keysym_t gsckbd_keydesc_no_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(85),	KS_backslash,	KS_grave,	KS_acute,
    KC(91),	KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

static const keysym_t gsckbd_keydesc_fr[] = {
/*  pos	     normal		shifted		altgr		shift-altgr */
    KC(22),	KS_ampersand,	KS_1,
    KC(30),	KS_eacute,	KS_2,	KS_asciitilde,
    KC(38),	KS_quotedbl,	KS_3,	KS_numbersign,
    KC(37),	KS_apostrophe,	KS_4,	KS_braceleft,
    KC(46),	KS_parenleft,	KS_5,	KS_bracketleft,
    KC(54),	KS_minus,	KS_6,	KS_bar,
    KC(61),	KS_egrave,	KS_7,	KS_grave,
    KC(62),	KS_underscore,	KS_8,	KS_backslash,
    KC(70),	KS_ccedilla,	KS_9,	KS_asciicircum,
    KC(69),	KS_agrave,	KS_0,	KS_at,
    KC(78),	KS_parenright,	KS_degree,	KS_bracketright,
    KC(85),	KS_equal,	KS_plus,	KS_braceright,
    KC(21),	KS_a,
    KC(29),	KS_z,
    KC(84),	KS_dead_circumflex,	KS_dead_diaeresis,
    KC(91),	KS_dollar,	KS_sterling,	KS_currency,
    KC(28),	KS_q,
    KC(76),	KS_m,
    KC(82),	KS_ugrave,	KS_percent,
    KC(14),	KS_twosuperior,
    KC(93),	KS_asterisk,	KS_mu,
    KC(26),	KS_w,
    KC(58),	KS_comma,	KS_question,
    KC(65),	KS_semicolon,	KS_period,
    KC(73),	KS_colon,	KS_slash,
    KC(74),	KS_exclam,	KS_section,
    KC(97),	KS_less,	KS_greater,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_it[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),	KS_2,	KS_quotedbl,	KS_twosuperior,
    KC(38),	KS_3,	KS_sterling,	KS_threesuperior,
    KC(37),	KS_4,	KS_dollar,
    KC(46),	KS_5,	KS_percent,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,
    KC(62),	KS_8,	KS_parenleft,
    KC(70),	KS_9,	KS_parenright,
    KC(69),	KS_0,	KS_equal,
    KC(78),	KS_apostrophe,	KS_question,
    KC(85),	KS_igrave,	KS_asciicircum,
    KC(84),	KS_egrave,	KS_eacute,	KS_braceleft,	KS_bracketleft,
    KC(91),	KS_plus,	KS_asterisk,	KS_braceright,	KS_bracketright,
    KC(76),	KS_ograve,	KS_Ccedilla,	KS_at,
    KC(82),	KS_agrave,	KS_degree,	KS_numbersign,
    KC(14),	KS_backslash,	KS_bar,
    KC(93),	KS_ugrave,	KS_section,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_uk[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_1,	KS_exclam,	KS_plusminus,	KS_exclamdown,
    KC(30),	KS_2,	KS_quotedbl,	KS_twosuperior,	KS_cent,
    KC(38),	KS_3,	KS_sterling,	KS_threesuperior,
    KC(37),	KS_4,	KS_dollar,	KS_acute,	KS_currency,
    KC(46),	KS_5,	KS_percent,	KS_mu,	KS_yen,
    KC(54),	KS_6,	KS_asciicircum,	KS_paragraph,
    KC(61),	KS_7,	KS_ampersand,	KS_periodcentered,	KS_brokenbar,
    KC(62),	KS_8,	KS_asterisk,	KS_cedilla,	KS_ordfeminine,
    KC(70),	KS_9,	KS_parenleft,	KS_onesuperior,	KS_diaeresis,
    KC(69),	KS_0,	KS_parenright,	KS_masculine,	KS_copyright,
    KC(78),	KS_minus,	KS_underscore,	KS_hyphen,	KS_ssharp,
    KC(85),	KS_equal,	KS_plus,	KS_onehalf,	KS_guillemotleft,
    KC(82),	KS_apostrophe,	KS_at,	KS_section,	KS_Agrave,
    KC(14),	KS_grave,	KS_grave,	KS_agrave,	KS_agrave,
    KC(93),	KS_numbersign,	KS_asciitilde,	KS_sterling,	KS_thorn,
    KC(97),	KS_backslash,	KS_bar,	KS_Udiaeresis,
};

static const keysym_t gsckbd_keydesc_jp[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),	KS_2,	KS_quotedbl,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_apostrophe,
    KC(62),	KS_8,	KS_parenleft,
    KC(70),	KS_9,	KS_parenright,
    KC(69),	KS_0,
    KC(78),	KS_minus,	KS_equal,
    KC(85),	KS_asciicircum,	KS_asciitilde,
    KC(84),	KS_at,	KS_grave,
    KC(91),	KS_bracketleft,	KS_braceleft,
    KC(76),	KS_semicolon,	KS_plus,
    KC(82),	KS_colon,	KS_asterisk,
    KC(14),	KS_Zenkaku_Hankaku,	/*	replace	grave/tilde	*/
    KC(93),	KS_bracketright,	KS_braceright,
/* initially KC(112),	KS_Hiragana_Katakana,	*/
/* initially KC(115),	KS_backslash,	KS_underscore,	*/
/* initially KC(121),	KS_Henkan,	*/
/* initially KC(123),	KS_Muhenkan,	*/
/* initially KC(125),	KS_backslash,	KS_bar,	*/
};

static const keysym_t gsckbd_keydesc_es[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_1,	KS_exclam,	KS_bar,
    KC(30),	KS_2,	KS_quotedbl,	KS_at,
    KC(38),	KS_3,	KS_periodcentered,	KS_numbersign,
    KC(37),	KS_4,	KS_dollar,	KS_asciitilde,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,
    KC(62),	KS_8,	KS_parenleft,
    KC(70),	KS_9,	KS_parenright,
    KC(69),	KS_0,	KS_equal,
    KC(78),	KS_apostrophe,	KS_question,
    KC(85),	KS_exclamdown,	KS_questiondown,
    KC(84),	KS_dead_grave,	KS_dead_circumflex,	KS_bracketleft,
    KC(91),	KS_plus,	KS_asterisk,	KS_bracketright,
    KC(76),	KS_ntilde,
    KC(82),	KS_dead_acute,	KS_dead_diaeresis,	KS_braceleft,
    KC(14),	KS_degree,	KS_ordfeminine,	KS_backslash,
    KC(93),	KS_ccedilla,	KS_Ccedilla,	KS_braceright,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_lt[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_exclam,	KS_1,	KS_at,
    KC(30),	KS_minus,	KS_2,	KS_underscore,
    KC(38),	KS_slash,	KS_3,	KS_numbersign,
    KC(37),	KS_semicolon,	KS_4,	KS_dollar,
    KC(46),	KS_colon,	KS_5,	KS_paragraph,
    KC(54),	KS_comma,	KS_6,	KS_asciicircum,
    KC(61),	KS_period,	KS_7,	KS_ampersand,
    KC(62),	KS_equal,	KS_8,	KS_asterisk,
    KC(70),	KS_bracketleft,	KS_9,	KS_parenleft,
    KC(69),	KS_bracketright,	KS_0,	KS_parenright,
    KC(78),	KS_question,	KS_plus,	KS_apostrophe,
    KC(85),	KS_x,	KS_X,	KS_percent,
    KC(21),	KS_L7_aogonek,	KS_L7_Aogonek,
    KC(29),	KS_L7_zcaron,	KS_L7_Zcaron,
    KC(36),	KS_e,	KS_E,	KS_currency,
    KC(84),	KS_L7_iogonek,	KS_L7_Iogonek,	KS_braceleft,
    KC(91),	KS_w,	KS_W,	KS_braceright,
    KC(43),	KS_L7_scaron,	KS_L7_Scaron,
    KC(76),	KS_L7_uogonek,	KS_L7_Uogonek,
    KC(82),	KS_L7_edot,	KS_L7_Edot,	KS_quotedbl,
    KC(14),	KS_grave,	KS_asciitilde,
    KC(93),	KS_q,	KS_Q,	KS_bar,
    KC(34),	KS_L7_umacron,	KS_L7_Umacron,
    KC(65),	KS_L7_ccaron,	KS_L7_Ccaron,	KS_L7_dbllow9quot,
    KC(73),	KS_f,	KS_F,	KS_L7_leftdblquot,
    KC(74),	KS_L7_eogonek,	KS_L7_Eogonek,	KS_backslash,
    KC(41),	KS_space,	KS_space,	KS_nobreakspace,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_be[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_ampersand,	KS_1,	KS_bar,
    KC(30),	KS_eacute,	KS_2,	KS_at,
    KC(38),	KS_quotedbl,	KS_3,	KS_numbersign,
    KC(37),	KS_apostrophe,	KS_4,
    KC(46),	KS_parenleft,	KS_5,
    KC(54),	KS_currency,	KS_6,	KS_asciicircum,
    KC(61),	KS_egrave,	KS_7,
    KC(62),	KS_exclam,	KS_8,
    KC(70),	KS_ccedilla,	KS_9,	KS_braceleft,
    KC(69),	KS_agrave,	KS_0,	KS_braceright,
    KC(78),	KS_parenright,	KS_degree,
    KC(85),	KS_minus,	KS_underscore,
    KC(21),	KS_a,
    KC(29),	KS_z,
    KC(84),	KS_dead_circumflex,	KS_dead_diaeresis,	KS_bracketleft,
    KC(91),	KS_dollar,	KS_asterisk,	KS_bracketright,
    KC(28),	KS_q,
    KC(76),	KS_m,
    KC(82),	KS_ugrave,	KS_percent,	KS_section,
    KC(14),	KS_twosuperior,
    KC(93),	KS_mu,	KS_sterling,	KS_grave,
    KC(26),	KS_w,
    KC(58),	KS_comma,	KS_question,
    KC(65),	KS_semicolon,	KS_period,
    KC(73),	KS_colon,	KS_slash,
    KC(74),	KS_equal,	KS_plus,	KS_asciitilde,
    KC(97),	KS_less,	KS_greater,	KS_backslash,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_us_declk[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(118),	KS_grave,	KS_asciitilde,	/*	replace	escape	*/
    KC(14),	KS_less,	KS_greater,	/*	replace	grave/tilde	*/
/* initially KC(143),	KS_Multi_key,	left	compose	*/
    KC(148),	KS_Multi_key,	/*	right	compose,	replace	right	control	*/
    KC(120),	KS_Cmd_Debugger,	KS_Escape,	/*	replace	F11	*/
/* initially KC(189),	KS_f13,	*/
/* initially KC(190),	KS_f14,	*/
/* initially KC(191),	KS_Help,	*/
/* initially KC(192),	KS_Execute,	*/
/* initially KC(193),	KS_f17,	*/
    KC(126),	KS_f19,	/*	replace	scroll	lock	*/
    KC(127),	KS_f20,	/*	replace	break	*/
    KC(119),	KS_KP_F1,	/*	replace	num	lock	*/
    KC(202),	KS_KP_F2,	/*	replace	divide	*/
    KC(124),	KS_KP_F3,	/*	replace	multiply	*/
    KC(123),	KS_KP_F4,	/*	replace	subtract	*/

    /* keypad is numbers only - no num lock */
    KC(108),	KS_KP_7,
    KC(117),	KS_KP_8,
    KC(125),	KS_KP_9,
    KC(107),	KS_KP_4,
    KC(115),	KS_KP_5,
    KC(116),	KS_KP_6,
    KC(105),	KS_KP_1,
    KC(114),	KS_KP_2,
    KC(122),	KS_KP_3,
    KC(112),	KS_KP_0,
    KC(113),	KS_KP_Decimal,

/* initially KC(206),	KS_KP_Subtract,	*/
    KC(121),	KS_KP_Separator,	/*	replace	add	*/
    KC(236),	KS_Find,	/*	replace	home	*/
    KC(233),	KS_Select,	/*	replace	end	*/
};

static const keysym_t gsckbd_keydesc_us_dvorak[] = {
/*  pos      command		normal		shifted */
    KC(78),	KS_bracketleft,	KS_braceleft,
    KC(85),	KS_bracketright,	KS_braceright,
    KC(21),	KS_apostrophe,	KS_quotedbl,
    KC(29),	KS_comma,	KS_less,
    KC(36),	KS_period,	KS_greater,
    KC(45),	KS_p,
    KC(44),	KS_y,
    KC(53),	KS_f,
    KC(60),	KS_g,
    KC(67),	KS_c,
    KC(68),	KS_r,
    KC(77),	KS_l,
    KC(84),	KS_slash,	KS_question,
    KC(91),	KS_equal,	KS_plus,
    KC(27),	KS_o,
    KC(35),	KS_e,
    KC(43),	KS_u,
    KC(52),	KS_i,
    KC(51),	KS_d,
    KC(59),	KS_h,
    KC(66),	KS_t,
    KC(75),	KS_n,
    KC(76),	KS_s,
    KC(82),	KS_minus,	KS_underscore,
    KC(26),	KS_semicolon,	KS_colon,
    KC(34),	KS_q,
    KC(33),	KS_j,
    KC(42),	KS_k,
    KC(50),	KS_x,
    KC(49),	KS_b,
    KC(65),	KS_w,
    KC(73),	KS_v,
    KC(74),	KS_z,
};

static const keysym_t gsckbd_keydesc_swapctrlcaps[] = {
/*  pos      command		normal		shifted */
    KC(20),	KS_Caps_Lock,
    KC(88),	KS_Cmd1,	KS_Control_L,
};

static const keysym_t gsckbd_keydesc_iopener[] = {
/*  pos      command		normal		shifted */
    KC(5),	KS_Cmd_Debugger,	KS_Escape,
    KC(6),	KS_Cmd_Screen0,	KS_f1,
    KC(4),	KS_Cmd_Screen1,	KS_f2,
    KC(12),	KS_Cmd_Screen2,	KS_f3,
    KC(3),	KS_Cmd_Screen3,	KS_f4,
    KC(11),	KS_Cmd_Screen4,	KS_f5,
    KC(131),	KS_Cmd_Screen5,	KS_f6,
    KC(10),	KS_Cmd_Screen6,	KS_f7,
    KC(1),	KS_Cmd_Screen7,	KS_f8,
    KC(9),	KS_Cmd_Screen8,	KS_f9,
    KC(120),	KS_Cmd_Screen9,	KS_f10,
    KC(7),	KS_f11,
};

static const keysym_t gsckbd_keydesc_ru[] = {
/*  pos      normal		shifted		altgr			shift-altgr */
    KC(54),	KS_6,	KS_asciicircum,	KS_6,	KS_comma,
    KC(61),	KS_7,	KS_ampersand,	KS_7,	KS_period,
    KC(21),	KS_q,	KS_Q,	KS_Cyrillic_ishort,	KS_Cyrillic_ISHORT,
    KC(29),	KS_w,	KS_W,	KS_Cyrillic_tse,	KS_Cyrillic_TSE,
    KC(36),	KS_e,	KS_E,	KS_Cyrillic_u,	KS_Cyrillic_U,
    KC(45),	KS_r,	KS_R,	KS_Cyrillic_ka,	KS_Cyrillic_KA,
    KC(44),	KS_t,	KS_T,	KS_Cyrillic_ie,	KS_Cyrillic_IE,
    KC(53),	KS_y,	KS_Y,	KS_Cyrillic_en,	KS_Cyrillic_EN,
    KC(60),	KS_u,	KS_U,	KS_Cyrillic_ge,	KS_Cyrillic_GE,
    KC(67),	KS_i,	KS_I,	KS_Cyrillic_sha,	KS_Cyrillic_SHA,
    KC(68),	KS_o,	KS_O,	KS_Cyrillic_scha,	KS_Cyrillic_SCHA,
    KC(77),	KS_p,	KS_P,	KS_Cyrillic_ze,	KS_Cyrillic_ZE,
    KC(84),	KS_bracketleft,	KS_braceleft,	KS_Cyrillic_ha,	KS_Cyrillic_HA,
    KC(91),	KS_bracketright,	KS_braceright,	KS_Cyrillic_hsighn,	KS_Cyrillic_HSIGHN,
    KC(28),	KS_a,	KS_A,	KS_Cyrillic_ef,	KS_Cyrillic_EF,
    KC(27),	KS_s,	KS_S,	KS_Cyrillic_yeru,	KS_Cyrillic_YERU,
    KC(35),	KS_d,	KS_D,	KS_Cyrillic_ve,	KS_Cyrillic_VE,
    KC(43),	KS_f,	KS_F,	KS_Cyrillic_a,	KS_Cyrillic_A,
    KC(52),	KS_g,	KS_G,	KS_Cyrillic_pe,	KS_Cyrillic_PE,
    KC(51),	KS_h,	KS_H,	KS_Cyrillic_er,	KS_Cyrillic_ER,
    KC(59),	KS_j,	KS_J,	KS_Cyrillic_o,	KS_Cyrillic_O,
    KC(66),	KS_k,	KS_K,	KS_Cyrillic_el,	KS_Cyrillic_EL,
    KC(75),	KS_l,	KS_L,	KS_Cyrillic_de,	KS_Cyrillic_DE,
    KC(76),	KS_semicolon,	KS_colon,	KS_Cyrillic_zhe,	KS_Cyrillic_ZHE,
    KC(82),	KS_apostrophe,	KS_quotedbl,	KS_Cyrillic_e,	KS_Cyrillic_E,
    KC(26),	KS_z,	KS_Z,	KS_Cyrillic_ya,	KS_Cyrillic_YA,
    KC(34),	KS_x,	KS_X,	KS_Cyrillic_che,	KS_Cyrillic_CHE,
    KC(33),	KS_c,	KS_C,	KS_Cyrillic_es,	KS_Cyrillic_ES,
    KC(42),	KS_v,	KS_V,	KS_Cyrillic_em,	KS_Cyrillic_EM,
    KC(50),	KS_b,	KS_B,	KS_Cyrillic_i,	KS_Cyrillic_I,
    KC(49),	KS_n,	KS_N,	KS_Cyrillic_te,	KS_Cyrillic_TE,
    KC(58),	KS_m,	KS_M,	KS_Cyrillic_ssighn,	KS_Cyrillic_SSIGHN,
    KC(65),	KS_comma,	KS_less,	KS_Cyrillic_be,	KS_Cyrillic_BE,
    KC(73),	KS_period,	KS_greater,	KS_Cyrillic_yu,	KS_Cyrillic_YU,
    KC(74),	KS_slash,	KS_question,	KS_Cyrillic_yo,	KS_Cyrillic_YO,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_ua[] = {
/*  pos      normal		shifted		altgr			shift-altgr */
    KC(54),	KS_6,	KS_asciicircum,	KS_6,	KS_comma,
    KC(61),	KS_7,	KS_ampersand,	KS_7,	KS_period,
    KC(78),	KS_minus,	KS_underscore,	KS_Cyrillic_iukr,	KS_Cyrillic_IUKR,
    KC(85),	KS_equal,	KS_plus,	KS_Cyrillic_yeukr,	KS_Cyrillic_YEUKR,
    KC(21),	KS_q,	KS_Q,	KS_Cyrillic_ishort,	KS_Cyrillic_ISHORT,
    KC(29),	KS_w,	KS_W,	KS_Cyrillic_tse,	KS_Cyrillic_TSE,
    KC(36),	KS_e,	KS_E,	KS_Cyrillic_u,	KS_Cyrillic_U,
    KC(45),	KS_r,	KS_R,	KS_Cyrillic_ka,	KS_Cyrillic_KA,
    KC(44),	KS_t,	KS_T,	KS_Cyrillic_ie,	KS_Cyrillic_IE,
    KC(53),	KS_y,	KS_Y,	KS_Cyrillic_en,	KS_Cyrillic_EN,
    KC(60),	KS_u,	KS_U,	KS_Cyrillic_ge,	KS_Cyrillic_GE,
    KC(67),	KS_i,	KS_I,	KS_Cyrillic_sha,	KS_Cyrillic_SHA,
    KC(68),	KS_o,	KS_O,	KS_Cyrillic_scha,	KS_Cyrillic_SCHA,
    KC(77),	KS_p,	KS_P,	KS_Cyrillic_ze,	KS_Cyrillic_ZE,
    KC(84),	KS_bracketleft,	KS_braceleft,	KS_Cyrillic_ha,	KS_Cyrillic_HA,
    KC(91),	KS_bracketright,	KS_braceright,	KS_Cyrillic_hsighn,	KS_Cyrillic_HSIGHN,
    KC(28),	KS_a,	KS_A,	KS_Cyrillic_ef,	KS_Cyrillic_EF,
    KC(27),	KS_s,	KS_S,	KS_Cyrillic_yeru,	KS_Cyrillic_YERU,
    KC(35),	KS_d,	KS_D,	KS_Cyrillic_ve,	KS_Cyrillic_VE,
    KC(43),	KS_f,	KS_F,	KS_Cyrillic_a,	KS_Cyrillic_A,
    KC(52),	KS_g,	KS_G,	KS_Cyrillic_pe,	KS_Cyrillic_PE,
    KC(51),	KS_h,	KS_H,	KS_Cyrillic_er,	KS_Cyrillic_ER,
    KC(59),	KS_j,	KS_J,	KS_Cyrillic_o,	KS_Cyrillic_O,
    KC(66),	KS_k,	KS_K,	KS_Cyrillic_el,	KS_Cyrillic_EL,
    KC(75),	KS_l,	KS_L,	KS_Cyrillic_de,	KS_Cyrillic_DE,
    KC(76),	KS_semicolon,	KS_colon,	KS_Cyrillic_zhe,	KS_Cyrillic_ZHE,
    KC(82),	KS_apostrophe,	KS_quotedbl,	KS_Cyrillic_e,	KS_Cyrillic_E,
    KC(14),	KS_grave,	KS_asciitilde,	KS_Cyrillic_gheukr,	KS_Cyrillic_GHEUKR,
    KC(93),	KS_backslash,	KS_bar,	KS_Cyrillic_yi,	KS_Cyrillic_YI,
    KC(26),	KS_z,	KS_Z,	KS_Cyrillic_ya,	KS_Cyrillic_YA,
    KC(34),	KS_x,	KS_X,	KS_Cyrillic_che,	KS_Cyrillic_CHE,
    KC(33),	KS_c,	KS_C,	KS_Cyrillic_es,	KS_Cyrillic_ES,
    KC(42),	KS_v,	KS_V,	KS_Cyrillic_em,	KS_Cyrillic_EM,
    KC(50),	KS_b,	KS_B,	KS_Cyrillic_i,	KS_Cyrillic_I,
    KC(49),	KS_n,	KS_N,	KS_Cyrillic_te,	KS_Cyrillic_TE,
    KC(58),	KS_m,	KS_M,	KS_Cyrillic_ssighn,	KS_Cyrillic_SSIGHN,
    KC(65),	KS_comma,	KS_less,	KS_Cyrillic_be,	KS_Cyrillic_BE,
    KC(73),	KS_period,	KS_greater,	KS_Cyrillic_yu,	KS_Cyrillic_YU,
    KC(74),	KS_slash,	KS_question,	KS_Cyrillic_yo,	KS_Cyrillic_YO,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_sg[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_1,	KS_plus,	KS_bar,
    KC(30),	KS_2,	KS_quotedbl,	KS_at,
    KC(38),	KS_3,	KS_asterisk,	KS_numbersign,
    KC(37),	KS_4,	KS_ccedilla,
    KC(54),	KS_6,	KS_ampersand,	KS_notsign,
    KC(61),	KS_7,	KS_slash,	KS_brokenbar,
    KC(62),	KS_8,	KS_parenleft,	KS_cent,
    KC(70),	KS_9,	KS_parenright,
    KC(69),	KS_0,	KS_equal,
    KC(78),	KS_apostrophe,	KS_question,	KS_dead_acute,
    KC(85),	KS_dead_circumflex,KS_dead_grave,	KS_dead_tilde,
    KC(36),	KS_e,	KS_E,	KS_currency,
    KC(53),	KS_z,
    KC(84),	KS_udiaeresis,	KS_egrave,	KS_bracketleft,
    KC(91),	KS_dead_diaeresis,	KS_exclam,	KS_bracketright,
    KC(76),	KS_odiaeresis,	KS_eacute,
    KC(82),	KS_adiaeresis,	KS_agrave,	KS_braceleft,
    KC(14),	KS_section,	KS_degree,	KS_dead_abovering,
    KC(93),	KS_dollar,	KS_sterling,	KS_braceright,
    KC(26),	KS_y,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,	KS_backslash,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_sg_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(78),	KS_apostrophe,	KS_question,	KS_acute,
    KC(85),	KS_asciicircum,	KS_grave,	KS_asciitilde,
    KC(91),	KS_diaeresis,	KS_exclam,	KS_bracketright
};

static const keysym_t gsckbd_keydesc_sf[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(84),	KS_egrave,	KS_udiaeresis,	KS_bracketleft,
    KC(76),	KS_eacute,	KS_odiaeresis,
    KC(82),	KS_agrave,	KS_adiaeresis,	KS_braceleft
};

static const keysym_t gsckbd_keydesc_pt[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),	KS_2,	KS_quotedbl,	KS_at,
    KC(38),	KS_3,	KS_numbersign,	KS_sterling,
    KC(37),	KS_4,	KS_dollar,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,	KS_braceleft,
    KC(62),	KS_8,	KS_parenleft,	KS_bracketleft,
    KC(70),	KS_9,	KS_parenright,	KS_bracketright,
    KC(69),	KS_0,	KS_equal,	KS_braceright,
    KC(78),	KS_apostrophe,	KS_question,
    KC(85),	KS_less,	KS_greater,
    KC(84),	KS_plus,	KS_asterisk,
    KC(91),	KS_dead_acute,	KS_dead_grave,
    KC(76),	KS_ccedilla,	KS_Ccedilla,
    KC(82),	KS_masculine,	KS_ordfeminine,
    KC(14),	KS_backslash,	KS_bar,
    KC(93),	KS_dead_tilde,	KS_dead_circumflex,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_la[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(22),	KS_1,	KS_exclam,
    KC(30),	KS_2,	KS_quotedbl,
    KC(38),	KS_3,	KS_numbersign,
    KC(37),	KS_4,	KS_dollar,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,
    KC(62),	KS_8,	KS_parenleft,
    KC(70),	KS_9,	KS_parenright,
    KC(69),	KS_0,	KS_equal,
    KC(78),	KS_apostrophe,	KS_question,	KS_backslash,
    KC(85),	KS_questiondown,	KS_exclamdown,
    KC(21),	KS_q,	KS_Q,	KS_at,
    KC(84),	KS_dead_acute,	KS_dead_diaeresis,
    KC(91),	KS_plus,	KS_asterisk,	KS_asciitilde,
    KC(76),	KS_ntilde,
    KC(82),	KS_braceleft,	KS_bracketleft,	KS_dead_circumflex,
    KC(14),	KS_bar,	KS_degree,	KS_notsign,
    KC(93),	KS_braceright,	KS_bracketright,KS_dead_grave,
    KC(65),	KS_comma,	KS_semicolon,
    KC(73),	KS_period,	KS_colon,
    KC(74),	KS_minus,	KS_underscore,
    KC(97),	KS_less,	KS_greater,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_br[] = {
/*  pos      normal		shifted         altgr           shift-altgr */
    KC(22),	KS_1,	KS_exclam,	KS_onesuperior,
    KC(30),	KS_2,	KS_at,	KS_twosuperior,
    KC(38),	KS_3,	KS_numbersign,	KS_threesuperior,
    KC(37),	KS_4,	KS_dollar,	KS_sterling,
    KC(46),	KS_5,	KS_percent,	KS_cent,
    KC(54),	KS_6,	KS_dead_diaeresis,	KS_notsign,
    KC(85),	KS_equal,	KS_plus,	KS_section,
    KC(84),	KS_dead_acute,	KS_dead_grave,
    KC(91),	KS_bracketleft,	KS_braceleft,	KS_ordfeminine,
    KC(76),	KS_ccedilla,	KS_Ccedilla,
    KC(82),	KS_dead_tilde,	KS_dead_circumflex,
    KC(14),	KS_apostrophe,	KS_quotedbl,
    KC(93),	KS_bracketright,	KS_braceright,	KS_masculine,
    KC(74),	KS_semicolon,	KS_colon,
    KC(113),	KS_KP_Delete,	KS_KP_Decimal,
    KC(97),	KS_backslash,	KS_bar,
/* initially KC(115),	KS_slash,	KS_question,	KS_degree,	*/
};

static const keysym_t gsckbd_keydesc_tr[] = {
/*  pos      normal		shifted         altgr           shift-altgr */
    KC(30),	KS_2,	KS_apostrophe,	KS_sterling,
    KC(38),	KS_3,	KS_asciicircum,	KS_numbersign,
    KC(37),	KS_4,	KS_plus,	KS_dollar,
    KC(46),	KS_5,	KS_percent,	KS_onehalf,
    KC(54),	KS_6,	KS_ampersand,
    KC(61),	KS_7,	KS_slash,	KS_braceleft,
    KC(62),	KS_8,	KS_parenleft,	KS_bracketleft,
    KC(70),	KS_9,	KS_parenright,	KS_bracketright,
    KC(69),	KS_0,	KS_equal,	KS_braceright,
    KC(78),	KS_asterisk,	KS_question,	KS_backslash,
    KC(85),	KS_minus,	KS_underscore,
    KC(21),	KS_q,	KS_Q,	KS_at,
    KC(67),	KS_L5_idotless,	KS_I,
    KC(84),	KS_L5_gbreve,	KS_L5_Gbreve,	KS_dead_diaeresis,
    KC(91),	KS_udiaeresis,	KS_Udiaeresis,	KS_asciitilde,
    KC(76),	KS_L5_scedilla,	KS_L5_Scedilla,	KS_dead_acute,
    KC(82),	KS_i,	KS_L5_Idotabove,
    KC(14),	KS_quotedbl,	KS_eacute,
    KC(93),	KS_comma,	KS_semicolon,	KS_dead_grave,
    KC(65),	KS_odiaeresis,	KS_Odiaeresis,
    KC(73),	KS_ccedilla,	KS_Ccedilla,
    KC(74),	KS_period,	KS_colon,
    KC(97),	KS_less,	KS_greater,	KS_bar,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t gsckbd_keydesc_tr_nodead[] = {
/*  pos      normal		shifted         altgr           shift-altgr */
    KC(84),	KS_L5_gbreve,	KS_L5_Gbreve,
    KC(76),	KS_L5_scedilla,	KS_L5_Scedilla,	KS_apostrophe,
    KC(93),	KS_comma,	KS_semicolon,	KS_grave,
};

static const keysym_t gsckbd_keydesc_pl[] = {
/*  pos      normal		shifted         altgr           shift-altgr */
    KC(36),	KS_e,	KS_E,	KS_L2_eogonek,	KS_L2_Eogonek,
    KC(68),	KS_o,	KS_O,	KS_oacute,	KS_Oacute,
    KC(28),	KS_a,	KS_A,	KS_L2_aogonek,	KS_L2_Aogonek,
    KC(27),	KS_s,	KS_S,	KS_L2_sacute,	KS_L2_Sacute,
    KC(75),	KS_l,	KS_L,	KS_L2_lstroke,	KS_L2_Lstroke,
    KC(26),	KS_z,	KS_Z,	KS_L2_zdotabove,KS_L2_Zdotabove,
    KC(34),	KS_x,	KS_X,	KS_L2_zacute,	KS_L2_Zacute,
    KC(33),	KS_c,	KS_C,	KS_L2_cacute,	KS_L2_Cacute,
    KC(49),	KS_n,	KS_N,	KS_L2_nacute,	KS_L2_Nacute,
    KC(145),	KS_Mode_switch,	KS_Multi_key,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc gsckbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	gsckbd_keydesc_us),
	KBD_MAP(KB_DE,			KB_US,	gsckbd_keydesc_de),
	KBD_MAP(KB_DE | KB_NODEAD,	KB_DE,	gsckbd_keydesc_de_nodead),
	KBD_MAP(KB_FR,			KB_US,  gsckbd_keydesc_fr),
	KBD_MAP(KB_DK,			KB_US,	gsckbd_keydesc_dk),
	KBD_MAP(KB_DK | KB_NODEAD,	KB_DK,	gsckbd_keydesc_dk_nodead),
	KBD_MAP(KB_IT,			KB_US,	gsckbd_keydesc_it),
	KBD_MAP(KB_UK,			KB_US,	gsckbd_keydesc_uk),
	KBD_MAP(KB_JP,			KB_US,	gsckbd_keydesc_jp),
	KBD_MAP(KB_SV,			KB_DK,	gsckbd_keydesc_sv),
	KBD_MAP(KB_SV | KB_NODEAD,	KB_SV,	gsckbd_keydesc_sv_nodead),
	KBD_MAP(KB_NO,			KB_DK,	gsckbd_keydesc_no),
	KBD_MAP(KB_NO | KB_NODEAD,	KB_NO,	gsckbd_keydesc_no_nodead),
	KBD_MAP(KB_US | KB_DECLK,	KB_US,	gsckbd_keydesc_us_declk),
	KBD_MAP(KB_US | KB_DVORAK,	KB_US,	gsckbd_keydesc_us_dvorak),
	KBD_MAP(KB_US | KB_SWAPCTRLCAPS, KB_US,	gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_US | KB_IOPENER, KB_US,	gsckbd_keydesc_iopener),
	KBD_MAP(KB_JP | KB_SWAPCTRLCAPS, KB_JP, gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_FR | KB_SWAPCTRLCAPS, KB_FR, gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_BE | KB_SWAPCTRLCAPS, KB_BE, gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_US | KB_DVORAK | KB_SWAPCTRLCAPS,	KB_US | KB_DVORAK,
		gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_US | KB_IOPENER | KB_SWAPCTRLCAPS,	KB_US | KB_IOPENER,
		gsckbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_ES,			KB_US,	gsckbd_keydesc_es),
	KBD_MAP(KB_BE,			KB_US,	gsckbd_keydesc_be),
	KBD_MAP(KB_RU,			KB_US,	gsckbd_keydesc_ru),
	KBD_MAP(KB_UA,			KB_US,	gsckbd_keydesc_ua),
	KBD_MAP(KB_SG,			KB_US,	gsckbd_keydesc_sg),
	KBD_MAP(KB_SG | KB_NODEAD,	KB_SG,	gsckbd_keydesc_sg_nodead),
	KBD_MAP(KB_SF,			KB_SG,	gsckbd_keydesc_sf),
	KBD_MAP(KB_SF | KB_NODEAD,	KB_SF,	gsckbd_keydesc_sg_nodead),
	KBD_MAP(KB_PT,			KB_US,	gsckbd_keydesc_pt),
	KBD_MAP(KB_LT,			KB_US,	gsckbd_keydesc_lt),
	KBD_MAP(KB_LA,			KB_US,	gsckbd_keydesc_la),
	KBD_MAP(KB_BR,			KB_US,	gsckbd_keydesc_br),
	KBD_MAP(KB_TR,			KB_US,	gsckbd_keydesc_tr),
	KBD_MAP(KB_TR | KB_NODEAD,	KB_TR,	gsckbd_keydesc_tr_nodead),
	KBD_MAP(KB_PL,			KB_US,	gsckbd_keydesc_pl),
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

/*	$OpenBSD: ukbdmap.c,v 1.13 2002/07/29 03:04:16 nate Exp $ */
/*	$NetBSD: ukbdmap.c,v 1.11 2002/07/11 21:14:31 augustss Exp $	*/

/*
 * Copyright (c) 1999,2001 The NetBSD Foundation, Inc.
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
    KC(68),  KS_Cmd_Screen10,	KS_f11,
    KC(69),  KS_Cmd_Screen11,	KS_f12,
    KC(70),			KS_Print_Screen,
    KC(71), 			KS_Hold_Screen,
    KC(72),			KS_Pause,
    KC(73),			KS_Insert,
    KC(74),			KS_Home,
    KC(75), KS_Cmd_ScrollBack,	KS_Prior,
    KC(76),			KS_Delete,
    KC(77),			KS_End,
    KC(78), KS_Cmd_ScrollFwd,	KS_Next,
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
    KC(99),   KS_Cmd_KbdReset,	KS_KP_Delete,	KS_KP_Decimal,
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

Static const keysym_t ukbd_keydesc_jp[] = {
/*  pos		command		normal			shifted */
    KC(31),			KS_2,			KS_quotedbl,
    KC(35),			KS_6,			KS_ampersand,
    KC(36),			KS_7,			KS_apostrophe,
    KC(37),			KS_8,			KS_parenleft,
    KC(38),			KS_9,			KS_parenright,
    KC(39),			KS_0,
    KC(45),			KS_minus,		KS_equal,
    KC(46),			KS_asciicircum,		KS_asciitilde,
    KC(47),			KS_at,			KS_grave,
    KC(48),			KS_bracketleft,		KS_braceleft,
    KC(50),			KS_bracketright,	KS_braceright,
    KC(51),			KS_semicolon,		KS_plus,
    KC(52),			KS_colon,		KS_asterisk,
    KC(53),			KS_Zenkaku_Hankaku, /* replace grave/tilde */
    KC(135),			KS_backslash,		KS_underscore,
    KC(136),			KS_Hiragana_Katakana,
    KC(137),			KS_backslash,		KS_bar,
    KC(138),			KS_Henkan,
    KC(139),			KS_Muhenkan,
};

Static const keysym_t ukbd_keydesc_swapctrlcaps[] = {
/*  pos      command		normal		shifted */
    KC(57), 			KS_Control_L,
    KC(224), KS_Cmd1,		KS_Caps_Lock,
};

Static const keysym_t ukbd_keydesc_de[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(16),  KS_m,		KS_M,		KS_mu,
    KC(20),  KS_q,		KS_Q,		KS_at,
    KC(28),  KS_z,
    KC(29),  KS_y,
    KC(31),  KS_2,		KS_quotedbl,	KS_twosuperior,
    KC(32),  KS_3,		KS_section,	KS_threesuperior,
    KC(35),  KS_6,		KS_ampersand,
    KC(36),  KS_7,		KS_slash,	KS_braceleft,
    KC(37),  KS_8,		KS_parenleft,	KS_bracketleft,
    KC(38),  KS_9,		KS_parenright,	KS_bracketright,
    KC(39),  KS_0,		KS_equal,	KS_braceright,
    KC(45),  KS_ssharp,		KS_question,	KS_backslash,
    KC(46),  KS_dead_acute,	KS_dead_grave,
    KC(47),  KS_udiaeresis,
    KC(48),  KS_plus,		KS_asterisk,	KS_dead_tilde,
    KC(50),  KS_numbersign,	KS_apostrophe,
    KC(51),  KS_odiaeresis,
    KC(52),  KS_adiaeresis,
    KC(53),  KS_dead_circumflex,KS_dead_abovering,
    KC(54),  KS_comma,		KS_semicolon,
    KC(55),  KS_period,		KS_colon,
    KC(56),  KS_minus,		KS_underscore,
    KC(100), KS_less,		KS_greater,	KS_bar,		KS_brokenbar,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_de_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(46),  KS_apostrophe,	KS_grave,
    KC(48),  KS_plus,		KS_asterisk,	KS_asciitilde,
    KC(53),  KS_asciicircum,	KS_degree,
};

Static const keysym_t ukbd_keydesc_dk[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(31),  KS_2,		KS_quotedbl,	KS_at,
    KC(32),  KS_3,		KS_numbersign,	KS_sterling,
    KC(33),  KS_4,		KS_currency,	KS_dollar,
    KC(35),  KS_6,		KS_ampersand,
    KC(36),  KS_7,		KS_slash,	KS_braceleft,
    KC(37),  KS_8,		KS_parenleft,	KS_bracketleft,
    KC(38),  KS_9,		KS_parenright,	KS_bracketright,
    KC(39),  KS_0,		KS_equal,	KS_braceright,
    KC(45),  KS_plus,		KS_question,
    KC(46),  KS_dead_acute,	KS_dead_grave,	KS_bar,
    KC(47),  KS_aring,
    KC(48),  KS_dead_diaeresis,	KS_dead_circumflex, KS_dead_tilde,
    KC(50),  KS_apostrophe,	KS_asterisk,
    KC(51),  KS_ae,
    KC(52),  KS_oslash,
    KC(53),  KS_onehalf,	KS_paragraph,
    KC(54),  KS_comma,		KS_semicolon,
    KC(55),  KS_period,		KS_colon,
    KC(56),  KS_minus,		KS_underscore,
    KC(100), KS_less,		KS_greater,	KS_backslash,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_dk_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(46),  KS_apostrophe,	KS_grave,	KS_bar,
    KC(48),  KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

Static const keysym_t ukbd_keydesc_sv[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(45),  KS_plus,		KS_question,	KS_backslash,
    KC(48),  KS_dead_diaeresis,	KS_dead_circumflex, KS_dead_tilde,
    KC(51),  KS_odiaeresis,
    KC(52),  KS_adiaeresis,
    KC(53),  KS_paragraph,	KS_onehalf,
    KC(100), KS_less,		KS_greater,	KS_bar,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_sv_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(46),  KS_apostrophe,	KS_grave,	KS_bar,
    KC(48),  KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

Static const keysym_t ukbd_keydesc_no[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(46),  KS_backslash,	KS_dead_grave,	KS_dead_acute,
    KC(48),  KS_dead_diaeresis,	KS_dead_circumflex, KS_dead_tilde,
    KC(51),  KS_oslash,
    KC(52),  KS_ae,
    KC(53),  KS_bar,		KS_paragraph,
    KC(100), KS_less,		KS_greater,
};

Static const keysym_t ukbd_keydesc_no_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(46),  KS_backslash,	KS_grave,	KS_acute,
    KC(48),  KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

Static const keysym_t ukbd_keydesc_fr[] = {
/*  pos	     normal		shifted		altgr		shift-altgr */
    KC(4),   KS_q,
    KC(16),  KS_comma,		KS_question,
    KC(20),  KS_a,
    KC(26),  KS_z,
    KC(29),  KS_w,
    KC(30),  KS_ampersand,	KS_1,
    KC(31),  KS_eacute,		KS_2,		KS_asciitilde,
    KC(32),  KS_quotedbl,	KS_3,		KS_numbersign,
    KC(33),  KS_apostrophe,	KS_4,		KS_braceleft,
    KC(34),  KS_parenleft,	KS_5,		KS_bracketleft,
    KC(35),  KS_minus,		KS_6,		KS_bar,
    KC(36),  KS_egrave,		KS_7,		KS_grave,
    KC(37),  KS_underscore,	KS_8,		KS_backslash,
    KC(38),  KS_ccedilla,	KS_9,		KS_asciicircum,
    KC(39),  KS_agrave,		KS_0,		KS_at,
    KC(45),  KS_parenright,	KS_degree,	KS_bracketright,
    KC(46),  KS_equal,		KS_plus,	KS_braceright,
    KC(47),  KS_dead_circumflex, KS_dead_diaeresis,
    KC(48),  KS_dollar,		KS_sterling,	KS_currency,
    KC(50),  KS_asterisk,	KS_mu,
    KC(51),  KS_m,
    KC(52),  KS_ugrave,		KS_percent,
    KC(53),  KS_twosuperior,
    KC(54),  KS_semicolon,	KS_period,
    KC(55),  KS_colon,		KS_slash,
    KC(56),  KS_exclam,		KS_section,
    KC(100), KS_less,		KS_greater,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_it[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(31),  KS_2,	    	KS_quotedbl,	KS_twosuperior,
    KC(32),  KS_3,	    	KS_sterling,	KS_threesuperior,
    KC(33),  KS_4,	    	KS_dollar,
    KC(34),  KS_5,	    	KS_percent,
    KC(35),  KS_6,	    	KS_ampersand,
    KC(36),  KS_7,	    	KS_slash,
    KC(37),  KS_8,	    	KS_parenleft,
    KC(38),  KS_9,	    	KS_parenright,
    KC(39),  KS_0,	    	KS_equal,
    KC(45),  KS_apostrophe,	KS_question,
    KC(46),  KS_igrave,	    	KS_asciicircum,
    KC(47),  KS_egrave,		KS_eacute,	KS_braceleft,	KS_bracketleft,
    KC(48),  KS_plus,		KS_asterisk,	KS_braceright,	KS_bracketright,
    KC(49),  KS_ugrave,		KS_section,
    KC(51),  KS_ograve,		KS_Ccedilla,	KS_at,
    KC(52),  KS_agrave,		KS_degree,	KS_numbersign,
    KC(53),  KS_backslash,	KS_bar,
    KC(54),  KS_comma,		KS_semicolon,
    KC(55),  KS_period,		KS_colon,
    KC(56),  KS_minus,		KS_underscore,
    KC(100), KS_less,		KS_greater,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_uk[] = {
/*  pos      normal             shifted         altgr           shift-altgr */
    KC(30),  KS_1,              KS_exclam,      KS_plusminus,   KS_exclamdown,
    KC(31),  KS_2,              KS_quotedbl,    KS_twosuperior, KS_cent,
    KC(32),  KS_3,              KS_sterling,    KS_threesuperior,
    KC(33),  KS_4,              KS_dollar,      KS_acute,       KS_currency,
    KC(34),  KS_5,              KS_percent,     KS_mu,          KS_yen,
    KC(35),  KS_6,              KS_asciicircum, KS_paragraph,
    KC(36),  KS_7,              KS_ampersand,   KS_periodcentered, KS_brokenbar,
    KC(37),  KS_8,              KS_asterisk,    KS_cedilla,     KS_ordfeminine,
    KC(38),  KS_9,              KS_parenleft,   KS_onesuperior, KS_diaeresis,
    KC(39),  KS_0,              KS_parenright,  KS_masculine,   KS_copyright,
    KC(45),  KS_minus,          KS_underscore,  KS_hyphen,      KS_ssharp,
    KC(46),  KS_equal,          KS_plus,        KS_onehalf,    KS_guillemotleft,
    KC(49),  KS_numbersign,     KS_asciitilde,  KS_sterling,    KS_thorn,
    KC(52),  KS_apostrophe,     KS_at,          KS_section,     KS_Agrave,
    KC(53),  KS_grave,          KS_grave,       KS_agrave,      KS_agrave,
    KC(100), KS_backslash,      KS_bar,         KS_Udiaeresis,
};

Static const keysym_t ukbd_keydesc_es[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),  KS_1,		KS_exclam,	KS_bar,
    KC(31),  KS_2,		KS_quotedbl,	KS_at,
    KC(32),  KS_3,		KS_periodcentered, KS_numbersign,
    KC(33),  KS_4,		KS_dollar,	KS_asciitilde,
    KC(35),  KS_6,		KS_ampersand,
    KC(36),  KS_7,		KS_slash,
    KC(37),  KS_8,		KS_parenleft,
    KC(38),  KS_9,		KS_parenright,
    KC(39),  KS_0,		KS_equal,
    KC(45),  KS_grave,		KS_question,
    KC(46),  KS_exclamdown,	KS_questiondown,
    KC(47),  KS_dead_grave,	KS_dead_circumflex, KS_bracketleft,
    KC(48),  KS_plus,		KS_asterisk,	KS_bracketright,
    KC(49),  KS_ccedilla,	KS_Ccedilla,	KS_braceright,
    KC(51),  KS_ntilde,
    KC(52),  KS_dead_acute,	KS_dead_diaeresis, KS_braceleft,
    KC(53),  KS_degree,		KS_ordfeminine,	KS_backslash,
    KC(54),  KS_comma,		KS_semicolon,
    KC(55),  KS_period,		KS_colon,
    KC(56),  KS_minus,		KS_underscore,
    KC(100), KS_less,		KS_greater,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_sg[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(30),  KS_1,		KS_plus,	KS_bar,
    KC(31),  KS_2,		KS_quotedbl,	KS_at,
    KC(32),  KS_3,		KS_asterisk,	KS_numbersign,
    KC(33),  KS_4,		KS_ccedilla,
    KC(35),  KS_6,		KS_ampersand,	KS_notsign,
    KC(36),  KS_7,		KS_slash,	KS_brokenbar,
    KC(37),  KS_8,		KS_parenleft,	KS_cent,
    KC(38),  KS_9,		KS_parenright,
    KC(39),  KS_0,		KS_equal,
    KC(45),  KS_apostrophe,	KS_question,	KS_dead_acute,
    KC(46),  KS_dead_circumflex,KS_dead_grave,	KS_dead_tilde,
    KC(8),   KS_e,		KS_E,		KS_currency,
    KC(28),  KS_z,
    KC(47),  KS_udiaeresis,	KS_egrave,	KS_bracketleft,
    KC(48),  KS_dead_diaeresis,	KS_exclam,	KS_bracketright,
    KC(51),  KS_odiaeresis,	KS_eacute,
    KC(52),  KS_adiaeresis,	KS_agrave,	KS_braceleft,
    KC(53),  KS_section,	KS_degree,	KS_dead_abovering,
    KC(50),  KS_dollar,		KS_sterling,	KS_braceright,
    KC(29),  KS_y,
    KC(54),  KS_comma,		KS_semicolon,
    KC(55),  KS_period,		KS_colon,
    KC(56),  KS_minus,		KS_underscore,
    KC(100), KS_less,		KS_greater,	KS_backslash,
    KC(230), KS_Mode_switch,	KS_Multi_key,
};

Static const keysym_t ukbd_keydesc_sg_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(45),  KS_apostrophe,	KS_question,	KS_acute,
    KC(46),  KS_asciicircum,	KS_grave,	KS_asciitilde,
    KC(48),  KS_diaeresis,	KS_exclam,	KS_bracketright
};

Static const keysym_t ukbd_keydesc_sf[] = {
/*  pos      normal            shifted         altgr           shift-altgr */
    KC(47),  KS_egrave,        KS_udiaeresis,  KS_bracketleft,
    KC(51),  KS_eacute,        KS_odiaeresis,
    KC(52),  KS_agrave,        KS_adiaeresis,  KS_braceleft
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc ukbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	ukbd_keydesc_us),
	KBD_MAP(KB_US | KB_SWAPCTRLCAPS,KB_US,	ukbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_JP,			KB_US,	ukbd_keydesc_jp),
	KBD_MAP(KB_JP | KB_SWAPCTRLCAPS,KB_JP,	ukbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_DE,			KB_US,	ukbd_keydesc_de),
	KBD_MAP(KB_DE | KB_NODEAD,	KB_DE,	ukbd_keydesc_de_nodead),
	KBD_MAP(KB_FR,                  KB_US,	ukbd_keydesc_fr),
	KBD_MAP(KB_FR | KB_SWAPCTRLCAPS,KB_FR,	ukbd_keydesc_swapctrlcaps),
	KBD_MAP(KB_DK,			KB_US,	ukbd_keydesc_dk),
	KBD_MAP(KB_DK | KB_NODEAD,	KB_DK,	ukbd_keydesc_dk_nodead),
	KBD_MAP(KB_IT,			KB_US,	ukbd_keydesc_it),
	KBD_MAP(KB_UK,			KB_US,	ukbd_keydesc_uk),
	KBD_MAP(KB_SV,			KB_DK,	ukbd_keydesc_sv),
	KBD_MAP(KB_SV | KB_NODEAD,	KB_SV,	ukbd_keydesc_sv_nodead),
	KBD_MAP(KB_NO,			KB_DK,	ukbd_keydesc_no),
	KBD_MAP(KB_NO | KB_NODEAD,	KB_NO,	ukbd_keydesc_no_nodead),
	KBD_MAP(KB_ES ,			KB_US,	ukbd_keydesc_es),
	KBD_MAP(KB_SG,			KB_US,	ukbd_keydesc_sg),
	KBD_MAP(KB_SG | KB_NODEAD,	KB_SG,	ukbd_keydesc_sg_nodead),
	KBD_MAP(KB_SF,			KB_SG,	ukbd_keydesc_sf),
	KBD_MAP(KB_SF | KB_NODEAD,	KB_SF,	ukbd_keydesc_sg_nodead),
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

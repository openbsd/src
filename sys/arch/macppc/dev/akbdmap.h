/*	$OpenBSD: akbdmap.h,v 1.7 2003/02/09 14:33:15 miod Exp $	*/
/*	$NetBSD: akbdmap.h,v 1.3 2000/09/01 16:00:39 tsubai Exp $	*/

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

/* XXX This list is incomplete. */

#define KC(n) KS_KEYCODE(n)

static const keysym_t akbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(0),			KS_a,
    KC(1),			KS_s,
    KC(2),			KS_d,
    KC(3),			KS_f,
    KC(4),			KS_h,
    KC(5),			KS_g,
    KC(6),			KS_z,
    KC(7),			KS_x,
    KC(8),			KS_c,
    KC(9),			KS_v,

    KC(11),			KS_b,
    KC(12),			KS_q,
    KC(13),			KS_w,
    KC(14),			KS_e,
    KC(15),			KS_r,
    KC(16),			KS_y,
    KC(17),			KS_t,
    KC(18),			KS_1,		KS_exclam,
    KC(19),			KS_2,		KS_at,
    KC(20),			KS_3,		KS_numbersign,
    KC(21),			KS_4,		KS_dollar,
    KC(22),			KS_6,		KS_asciicircum,
    KC(23),			KS_5,		KS_percent,
    KC(24),			KS_equal,	KS_plus,
    KC(25),			KS_9,		KS_parenleft,
    KC(26),			KS_7,		KS_ampersand,
    KC(27),			KS_minus,	KS_underscore,
    KC(28),			KS_8,		KS_asterisk,
    KC(29),			KS_0,		KS_parenright,
    KC(30),			KS_bracketright, KS_braceright,
    KC(31),			KS_o,
    KC(32),			KS_u,
    KC(33),			KS_bracketleft,	KS_braceleft,
    KC(34),			KS_i,
    KC(35),			KS_p,
    KC(36),			KS_Return,
    KC(37),			KS_l,
    KC(38),			KS_j,
    KC(39),			KS_apostrophe,	KS_quotedbl,
    KC(40),			KS_k,
    KC(41),			KS_semicolon,	KS_colon,
    KC(42),			KS_backslash,	KS_bar,
    KC(43),			KS_comma,	KS_less,
    KC(44),			KS_slash,	KS_question,
    KC(45),			KS_n,
    KC(46),			KS_m,
    KC(47),			KS_period,	KS_greater,
    KC(48),			KS_Tab,
    KC(49),			KS_space,
    KC(50),			KS_grave,	KS_asciitilde,
    KC(51),			KS_Delete,

    KC(53),			KS_Escape,
    KC(54),			KS_Control_L,
    KC(55),  KS_Cmd,				/* Command */
    KC(56),			KS_Shift_L,
    KC(57),			KS_Caps_Lock,
    KC(58),  KS_Cmd1,				/* Option */
    KC(59),			KS_Left,
    KC(60),			KS_Right,
    KC(61),			KS_Down,
    KC(62),			KS_Up,

    KC(65),			KS_KP_Decimal,
    KC(67),			KS_KP_Multiply,
    KC(69),			KS_KP_Add,
    KC(71),			KS_Clear,
    KC(75),			KS_KP_Divide,
    KC(76),			KS_KP_Enter,
    KC(78),			KS_KP_Subtract,

    KC(81),			KS_KP_Equal,
    KC(82),			KS_KP_0,
    KC(83),			KS_KP_1,
    KC(84),			KS_KP_2,
    KC(85),			KS_KP_3,
    KC(86),			KS_KP_4,
    KC(87),			KS_KP_5,
    KC(88),			KS_KP_6,
    KC(89),			KS_KP_7,

    KC(91),			KS_KP_8,
    KC(92),			KS_KP_9,

    KC(95),			KS_comma,	/* XXX KS_KP_comma */

    KC(106),			KS_KP_Enter,

    KC(115),			KS_KP_Home,
    KC(116),			KS_KP_Prior,
    KC(119),			KS_KP_End,
    KC(121),			KS_KP_Next,

    KC(127),  KS_Cmd_Debugger,
};

static const keysym_t akbd_keydesc_fr[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(0),	KS_q,
    KC(1),	KS_s,		KS_S,		KS_Ograve,
    KC(4),	KS_h,		KS_H,		KS_Igrave,	KS_Icircumflex,
    KC(6),	KS_w,
    KC(10),	KS_at,		KS_numbersign,
    KC(11),	KS_b,		KS_B,		KS_ssharp,
    KC(12),	KS_a,		KS_A,		KS_ae,		KS_AE,
    KC(13),	KS_z,		KS_Z,		KS_Acircumflex,	KS_Aring,
    KC(14),	KS_e,		KS_E,		KS_ecircumflex,	KS_Ecircumflex,
    KC(15),	KS_r,		KS_R,		KS_registered,	KS_comma,
    KC(16),	KS_y,		KS_Y,		KS_Uacute,
    KC(18),	KS_ampersand,	KS_1,
    KC(19),	KS_eacute,	KS_2,		KS_ediaeresis,
    KC(20),	KS_quotedbl,	KS_3,
    KC(21),	KS_apostrophe,	KS_4,
    KC(22),	KS_section,	KS_6,
    KC(23),	KS_parenleft,	KS_5,		KS_braceleft,	KS_bracketleft,
    KC(24),	KS_minus,	KS_underscore,
    KC(25),	KS_ccedilla,	KS_9,		KS_Ccedilla,	KS_Aacute,
    KC(26),	KS_egrave,	KS_7,		KS_guillemotleft,
						KS_guillemotright,
    KC(27),	KS_parenright,	KS_degree,	KS_braceright,	KS_bracketright,
    KC(28),	KS_exclam,	KS_8,		KS_exclamdown,	KS_Ucircumflex,
    KC(29),	KS_agrave,	KS_0,		KS_oslash,	KS_Ooblique,
    KC(30),	KS_dollar,	KS_asterisk,	KS_cent,	KS_yen,
    KC(33),	KS_dead_circumflex, KS_dead_diaeresis,
						KS_ocircumflex,	KS_Ocircumflex,
    KC(34),	KS_i,		KS_I,		KS_icircumflex,	KS_idiaeresis,
    KC(37),	KS_l,		KS_L,		KS_voidSymbol,	KS_bar,
    KC(38),	KS_j,		KS_J,		KS_Idiaeresis,	KS_Iacute,
    KC(39),	KS_ugrave,	KS_percent,	KS_Ugrave,
    KC(40),	KS_k,		KS_K,		KS_Egrave,	KS_Ediaeresis,
    KC(41),	KS_m,		KS_M,		KS_mu,		KS_Oacute,
    KC(42),	KS_grave,	KS_sterling,	KS_at,		KS_numbersign,
    KC(43),	KS_semicolon,	KS_period,
    KC(44),	KS_equal,	KS_plus,
    KC(45),	KS_n,		KS_N,		KS_asciitilde,
    KC(46),	KS_comma,	KS_question,	KS_voidSymbol,	KS_questiondown,
    KC(47),	KS_colon,	KS_slash,	KS_voidSymbol,	KS_backslash,
    KC(50),	KS_less,	KS_greater,
    KC(52),	KS_Alt_R,
    KC(55),	KS_Meta_L,				/* Command */
    KC(58),	KS_Mode_switch,	KS_Multi_key,		/* Option */
};

#if 0
static const keysym_t akbd_keydesc_jp[] = {
/*  pos      command		normal		shifted */
    KC(42),			KS_grave,	KS_asciitilde,
    KC(93),			KS_backslash,	KS_bar,
};
#endif

static const keysym_t akbd_keydesc_uk[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(10),	KS_section,	KS_plusminus,
    KC(20),	KS_3,		KS_sterling,	KS_numbersign,
    KC(52),	KS_KP_Enter,
    KC(58),	KS_Mode_switch,	KS_Multi_key,		/* Option */
};

static const keysym_t akbd_keydesc_sv[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(10),	KS_section,	KS_degree,
    KC(19),	KS_2,		KS_quotedbl,	KS_at,
    KC(21),	KS_4,		KS_dollar,
    KC(22),	KS_6,		KS_ampersand,
    KC(24),	KS_dead_acute,	KS_dead_grave,
    KC(25),	KS_9,		KS_parenright,	KS_bracketright,
    KC(26),	KS_7,		KS_slash,	KS_braceleft,
    KC(27),	KS_plus,	KS_question,	KS_backslash,
    KC(28),	KS_8,		KS_parenleft,	KS_bracketleft,
    KC(29),	KS_0,		KS_equal,	KS_braceright,
    KC(30),	KS_dead_diaeresis, KS_dead_circumflex, KS_dead_tilde,
    KC(33),	KS_aring,
    KC(39),	KS_adiaeresis,
    KC(41),	KS_odiaeresis,
    KC(42),	KS_apostrophe,	KS_asterisk,
    KC(43),	KS_comma,	KS_semicolon,
    KC(44),	KS_minus,	KS_underscore,
    KC(47),	KS_period,	KS_colon,
    KC(50),	KS_less,	KS_greater,	KS_bar,
    KC(52),	KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t akbd_keydesc_sv_nodead[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(24),	KS_apostrophe,	KS_grave,
    KC(30),	KS_diaeresis,	KS_asciicircum,	KS_asciitilde,
};

static const keysym_t akbd_keydesc_de[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(0),	KS_a,		KS_A,		KS_aring,	KS_Aring,
    KC(1),	KS_s,		KS_S,		KS_voidSymbol,	KS_Iacute,
    KC(3),	KS_f,		KS_F,		KS_voidSymbol,	KS_Idiaeresis,
    KC(4),	KS_h,		KS_H,		KS_ordfeminine,	KS_Oacute,
    KC(5),	KS_g,		KS_G,		KS_copyright,	KS_Igrave,
    KC(6),	KS_y,		KS_Y,		KS_yen,
    KC(7),	KS_x,		KS_X,		KS_voidSymbol,	KS_Ugrave,
    KC(8),	KS_c,		KS_C,		KS_ccedilla,	KS_Ccedilla,
    KC(10),	KS_dead_circumflex, KS_degree,
    KC(12),	KS_q,		KS_Q,		KS_guillemotleft, KS_guillemotright,
    KC(15),	KS_r,		KS_R,		KS_registered,	KS_cedilla,
    KC(16),	KS_z,		KS_Z,
    KC(18),	KS_1,		KS_exclam,	KS_exclamdown,	KS_notsign,
    KC(19),	KS_2,		KS_quotedbl,
    KC(20),	KS_3,		KS_section,	KS_paragraph,	KS_numbersign,
    KC(21),	KS_4,		KS_dollar,	KS_cent,	KS_sterling,
    KC(22),	KS_6,		KS_ampersand,	KS_bracketright,KS_dead_circumflex,
    KC(23),	KS_5,		KS_percent,	KS_bracketleft,
    KC(24),	KS_dead_acute,	KS_dead_grave,	KS_apostrophe,
    KC(25),	KS_9,		KS_parenright,	KS_braceright,	KS_periodcentered,
    KC(26),	KS_7,		KS_slash,	KS_bar,		KS_backslash,
    KC(27),	KS_ssharp,	KS_question,	KS_questiondown,
    KC(28),	KS_8,		KS_parenleft,	KS_braceleft,
    KC(29),	KS_0,		KS_equal,	KS_voidSymbol,	KS_macron,
    KC(30),	KS_plus,	KS_asterisk,	KS_plusminus,
    KC(31),	KS_o,		KS_O,		KS_oslash,	KS_Ooblique,
    KC(32),	KS_u,		KS_U,		KS_dead_diaeresis, KS_Aacute,
    KC(33),	KS_udiaeresis,	KS_Udiaeresis,	KS_voidSymbol,	KS_degree,
    KC(34),	KS_i,		KS_I,		KS_voidSymbol,	KS_Ucircumflex,
    KC(37),	KS_l,		KS_L,		KS_at,
    KC(38),	KS_j,		KS_J,		KS_masculine,
    KC(39),	KS_adiaeresis,	KS_Adiaeresis,	KS_ae,		KS_AE,
    KC(41),	KS_odiaeresis,	KS_Odiaeresis,
    KC(42),	KS_numbersign,	KS_apostrophe,
    KC(43),	KS_comma,	KS_semicolon,
    KC(44),	KS_minus,	KS_underscore,
    KC(45),	KS_n,		KS_N,		KS_dead_tilde,
    KC(46),	KS_m,		KS_M,		KS_mu,
    KC(47),	KS_period,	KS_colon,
    KC(50),	KS_less,	KS_greater,
    KC(52),	KS_Multi_key,
    KC(58),	KS_Mode_switch,
};

static const keysym_t akbd_keydesc_de_nodead[] = {
/*  pos		normal		shifted		altgr		shift-altgr */
    KC(10),	KS_asciicircum, KS_degree,
    KC(22),	KS_6,		KS_ampersand,	KS_bracketright,KS_asciicircum,
    KC(24),	KS_acute,	KS_grave,	KS_apostrophe,
    KC(32),	KS_u,		KS_U,		KS_diaeresis,	KS_Aacute,
    KC(45),	KS_n,		KS_N,		KS_asciitilde,
};

static const keysym_t akbd_keydesc_sf[] = {
    KC(6),			KS_y,
    KC(10),	KS_paragraph,	KS_degree,
    KC(16),			KS_z,
    KC(18),	KS_plus,	KS_1,
    KC(19),	KS_quotedbl,	KS_2,
    KC(20),	KS_asterisk,	KS_3,
    KC(21),	KS_ccedilla,	KS_4,		KS_Ccedilla,
    KC(22),	KS_ampersand,	KS_6,
    KC(23),	KS_percent,	KS_5,
    KC(24),	KS_dead_circumflex, KS_grave,
    KC(25),	KS_parenright,	KS_9,
    KC(26),	KS_slash,	KS_7,
    KC(27),	KS_apostrophe,	KS_question,
    KC(28),	KS_parenleft,	KS_8,
    KC(29),	KS_equal,	KS_0,
    KC(30),	KS_dead_diaeresis, KS_exclam,
    KC(33),	KS_egrave,	KS_udiaeresis,
    KC(39),	KS_agrave,	KS_adiaeresis,
    KC(41),	KS_eacute,	KS_odiaeresis,
    KC(42),	KS_dollar,	KS_sterling,
    KC(43),	KS_period,	KS_colon,
    KC(46),	KS_comma,	KS_semicolon,
    KC(47),	KS_minus,	KS_underscore,
    KC(50),	KS_less,	KS_greater,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

static const struct wscons_keydesc akbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	akbd_keydesc_us),
	KBD_MAP(KB_FR,			KB_US,	akbd_keydesc_fr),
	KBD_MAP(KB_SF,			KB_US,	akbd_keydesc_sf),
	KBD_MAP(KB_SV,			KB_US,	akbd_keydesc_sv),
	KBD_MAP(KB_SV | KB_NODEAD,	KB_SV,	akbd_keydesc_sv_nodead),
	KBD_MAP(KB_DE,			KB_US,	akbd_keydesc_de),
	KBD_MAP(KB_DE | KB_NODEAD,	KB_DE,	akbd_keydesc_de_nodead),
	KBD_MAP(KB_UK,			KB_US,	akbd_keydesc_uk),
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

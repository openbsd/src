/* $OpenBSD: zaurus_kbdmap.h,v 1.16 2005/01/25 23:30:55 drahn Exp $ */

/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define KC(n) KS_KEYCODE(n)
#include <dev/wscons/wskbdraw.h>

static const keysym_t zkbd_keydesc_us[] = {
    KC(0),	KS_Control_L,
    KC(2),	KS_Tab,		KS_Tab,		KS_Caps_Lock,
    KC(3),	KS_Cmd_Screen1,	KS_f2,				/* Addr, */
    KC(4),	KS_Cmd_Screen0,	KS_f1,				/* Cal, */
    KC(5),	KS_Cmd_Screen2,	KS_f3,				/* Mail, */
    KC(6),	KS_Cmd_Screen3,	KS_f4,				/* Home, */
    KC(8),	KS_1,		KS_exclam,
    KC(9),	KS_2,		KS_quotedbl,
    KC(10),	KS_q,
    KC(11),	KS_w,		KS_W,		KS_asciicircum,
    KC(12),	KS_a,
    KC(13),	KS_z,
    KC(14),	KS_Cmd,		KS_Alt_L,
    KC(16),	KS_Cmd_BrightnessUp,	KS_3,	KS_numbersign,
    KC(17),	KS_Cmd_BrightnessDown,	KS_4,	KS_dollar,	
    KC(18),	KS_e,		KS_E,		KS_equal,
    KC(19),	KS_s,
    KC(20),	KS_d,		KS_D,		KS_grave,
    KC(21),	KS_x,
    /* KC(22),	^/t (right japanese) */
    KC(24),	KS_5,		KS_percent,
    KC(25),	KS_r,		KS_R,		KS_plus,
    KC(26),	KS_t,		KS_T,		KS_bracketleft,
    KC(27),	KS_f,		KS_F,		KS_backslash,
    KC(28),	KS_c,
    KC(29),	KS_minus,	KS_minus,	KS_at,
    KC(30),	KS_Escape, /* Cancel */
    KC(32),	KS_6,		KS_ampersand,
    KC(33),	KS_y,		KS_Y,		KS_bracketright,
    KC(34),	KS_g,		KS_G,		KS_semicolon,
    KC(35),	KS_v,
    KC(36),	KS_b,		KS_B,		KS_underscore,
    KC(37),	KS_space,
    KC(38),	KS_KP_Enter,	/* ok */
    KC(40),	KS_7,		KS_apostrophe,
    KC(41),	KS_8,		KS_parenleft,
    KC(42),	KS_u,		KS_U,		KS_braceleft,	
    KC(43),	KS_h,		KS_H,		KS_colon,
    KC(44),	KS_n,
    KC(45),	KS_comma,	KS_slash,	KS_less,
    KC(46),	KS_Cmd_Screen4,	KS_f5,				/* Menu, */
    KC(48),	KS_9,		KS_parenright,
    KC(49),	KS_i,		KS_I,		KS_braceright,
    KC(50),	KS_j,		KS_J,		KS_asterisk,
    KC(51),	KS_m,
    KC(52),	KS_period,	KS_question,	KS_greater,
    KC(54),	KS_KP_Left, /* left, */
    KC(56),	KS_0,		KS_asciitilde,
    KC(57),	KS_o,
    KC(58),	KS_k,
    KC(59),	KS_l,		KS_L,		KS_bar,
    KC(61),	KS_KP_Up, /* up, */
    KC(62),	KS_KP_Down, /* down, */
    KC(64),	KS_Delete,	KS_BackSpace,
    KC(65),	KS_p,
    KC(68),	KS_Return,
    KC(70),	KS_KP_Right, /* right, */
    KC(80),	KS_KP_Right, /* OK, (ext) */
    KC(81),	KS_KP_Down, /* tog left, */
    KC(83),	KS_Shift_R,
    KC(84),	KS_Shift_L,
    KC(88),	KS_KP_Left, /* cancel (ext), */
    KC(89),	KS_KP_Up, /* tog right, */
    KC(93),	KS_Mode_switch /* Fn */
};

#ifdef WSDISPLAY_COMPAT_RAWKBD
static const char xt_keymap[] = {
    /* KC(0), */	RAWKEY_Control_L,/* KS_Control_L, */
    /* KC(1), */	RAWKEY_Null, /* NC */
    /* KC(2), */	RAWKEY_Tab, /* KS_Tab,	KS_Tab,		KS_Caps_Lock, */
    /* KC(3), */	RAWKEY_f2, /* KS_Cmd_Screen1,	KS_f2,		Addr, */
    /* KC(4), */	RAWKEY_f1, /* KS_Cmd_Screen0,	KS_f1,		Cal, */
    /* KC(5), */	RAWKEY_f3, /* KS_Cmd_Screen2,	KS_f3,		Mail, */
    /* KC(6), */	RAWKEY_f4, /* KS_Cmd_Screen3,	KS_f4,		Home, */
    /* KC(7), */	RAWKEY_Null, /* NC */
    /* KC(8), */	RAWKEY_1, /* KS_1,		KS_exclam, */
    /* KC(9), */	RAWKEY_2, /* KS_2,		KS_quotedbl, */
    /* KC(10), */	RAWKEY_q, /* KS_q, */
    /* KC(11), */	RAWKEY_w, /* KS_w,	KS_W,	KS_asciicircum, */
    /* KC(12), */	RAWKEY_a, /* KS_a, */
    /* KC(13), */	RAWKEY_z, /* KS_z, */
    /* KC(14), */	RAWKEY_Alt_L, /* KS_Cmd,		KS_Alt_L, */
    /* KC(15), */	RAWKEY_Null, /* NC */
    /* KC(16), */	RAWKEY_3, /* KS_3,	KS_numbersign, */
    /* KC(17), */	RAWKEY_4, /* KS_4,	KS_dollar, */
    /* KC(18), */	RAWKEY_e, /* KS_e,	KS_E,		KS_equal, */
    /* KC(19), */	RAWKEY_s, /* KS_s, */
    /* KC(20), */	RAWKEY_d, /* KS_d,	KS_D,		KS_grave, */
    /* KC(21), */	RAWKEY_x, /* KS_x, */
    /* KC(22), */	RAWKEY_Null, /* ^/t (right japanese) */
    /* KC(23), */	RAWKEY_Null, /* NC */
    /* KC(24), */	RAWKEY_5, /* KS_5,	KS_percent, */
    /* KC(25), */	RAWKEY_r, /* KS_r,	KS_R,		KS_plus, */
    /* KC(26), */	RAWKEY_t, /* KS_t,	KS_T,		KS_bracketleft, */
    /* KC(27), */	RAWKEY_f, /* KS_f,		KS_F,		KS_backslash, */
    /* KC(28), */	RAWKEY_c, /* KS_c, */
    /* KC(29), */	RAWKEY_minus, /* KS_minus, KS_minus,	KS_at, */
    /* KC(30), */	RAWKEY_Escape, /* KS_Escape, Cancel */
    /* KC(31), */	RAWKEY_Null, /* NC */
    /* KC(32), */	RAWKEY_6, /* KS_6,		KS_ampersand, */
    /* KC(33), */	RAWKEY_y, /* KS_y,	KS_Y,	KS_bracketright, */
    /* KC(34), */	RAWKEY_g, /* KS_g,		KS_G,	KS_semicolon, */
    /* KC(35), */	RAWKEY_v, /* KS_v, */
    /* KC(36), */	RAWKEY_b, /* KS_b,	KS_B,	KS_underscore, */
    /* KC(37), */	RAWKEY_space, /* KS_space, */
    /* KC(38), */	RAWKEY_KP_Enter, /* KS_KP_Enter,	ok */
    /* KC(39), */	RAWKEY_Null, /* NC */
    /* KC(40), */	RAWKEY_7, /* KS_7,	KS_apostrophe, */
    /* KC(41), */	RAWKEY_8, /* KS_8,	KS_parenleft, */
    /* KC(42), */	RAWKEY_u, /* KS_u,	KS_U,		KS_braceleft,	 */
    /* KC(43), */	RAWKEY_h, /* KS_h,	KS_H,		KS_colon, */
    /* KC(44), */	RAWKEY_n, /* KS_n, */
    /* KC(45), */	RAWKEY_comma, /* KS_comma, KS_slash,	KS_less, */
    /* KC(46), */	RAWKEY_f5, /* KS_Cmd_Screen4,	KS_f5,		Menu, */
    /* KC(47), */	RAWKEY_Null, /* NC */
    /* KC(48), */	RAWKEY_9, /* KS_9,	KS_parenright, */
    /* KC(49), */	RAWKEY_i, /* KS_i,	KS_I,	KS_braceright, */
    /* KC(50), */	RAWKEY_j, /* KS_j,	KS_J,		KS_asterisk, */
    /* KC(51), */	RAWKEY_m, /* KS_m, */
    /* KC(52), */	RAWKEY_period, /* KS_period, KS_question, KS_greater, */
    /* KC(53), */	RAWKEY_Null, /* NC */
    /* KC(54), */	RAWKEY_Left, /* KS_KP_Left, left, */
    /* KC(55), */	RAWKEY_Null, /* NC */
    /* KC(56), */	RAWKEY_0, /* KS_0,	KS_asciitilde, */
    /* KC(57), */	RAWKEY_o, /* KS_o, */
    /* KC(58), */	RAWKEY_k, /* KS_k, */
    /* KC(59), */	RAWKEY_l, /* KS_l,	KS_L,		KS_bar, */
    /* KC(60), */	RAWKEY_Null, /* NC */
    /* KC(61), */	RAWKEY_Up, /* KS_KP_Up, up, */
    /* KC(62), */	RAWKEY_Down, /* KS_KP_Down, down, */
    /* KC(63), */	RAWKEY_Null, /* NC */
    /* KC(64), */	RAWKEY_BackSpace, /* KS_Delete,	KS_BackSpace, */
    /* KC(65), */	RAWKEY_p, /* KS_p, */
    /* KC(66), */	RAWKEY_Null, /* NC */
    /* KC(67), */	RAWKEY_Null, /* NC */
    /* KC(68), */	RAWKEY_Return, /* KS_Return, */
    /* KC(69), */	RAWKEY_Null, /* NC */
    /* KC(70), */	RAWKEY_Right, /* KS_KP_Right, right, */
    /* KC(71), */	RAWKEY_Null, /* NC */
    /* KC(72), */	RAWKEY_Null, /* NC */
    /* KC(73), */	RAWKEY_Null, /* NC */
    /* KC(74), */	RAWKEY_Null, /* NC */
    /* KC(75), */	RAWKEY_Null, /* NC */
    /* KC(76), */	RAWKEY_Null, /* NC */
    /* KC(77), */	RAWKEY_Null, /* NC */
    /* KC(78), */	RAWKEY_Null, /* NC */
    /* KC(79), */	RAWKEY_Null, /* NC */
    /* KC(80), */	RAWKEY_Right, /* KS_KP_Right, OK, (ext) */
    /* KC(81), */	RAWKEY_Down, /* KS_KP_Down, tog left, */
    /* KC(82), */	RAWKEY_Null, /* NC */
    /* KC(83), */	RAWKEY_Shift_R, /* KS_Shift_R, */
    /* KC(84), */	RAWKEY_Shift_L, /* KS_Shift_L, */
    /* KC(85), */	RAWKEY_Null, /* NC */
    /* KC(86), */	RAWKEY_Null, /* NC */
    /* KC(87), */	RAWKEY_Null, /* NC */
    /* KC(88), */	RAWKEY_Left, /* KS_KP_Left, cancel (ext), */
    /* KC(89), */	RAWKEY_Up, /* KS_KP_Up, tog right, */
    /* KC(90), */	RAWKEY_Null, /* NC */
    /* KC(91), */	RAWKEY_Null, /* NC */
    /* KC(92), */	RAWKEY_Null, /* NC */
    /* KC(93), */	RAWKEY_Alt_R, /* KS_Mode_switch Fn */
};
#endif

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

static const struct wscons_keydesc zkbd_keydesctab[] = {
        KBD_MAP(KB_US,                  0,      zkbd_keydesc_us),
        {0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

/* $OpenBSD: zaurus_kbdmap.h,v 1.5 2005/01/14 00:53:12 drahn Exp $ */

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

static const keysym_t zkbd_keydesc_us[] = {
    KC(0),	KS_Control_L,
    KC(2),	KS_Tab,		KS_Tab,		KS_Caps_Lock,
    /* KC(3),	Addr, */
    /* KC(4),	Cal, */
    /* KC(5),	Mail, */
    /* KC(6),	Home, */
    KC(8),	KS_1,		KS_exclam,
    KC(9),	KS_2,		KS_quotedbl,
    KC(10),	KS_q,
    KC(11),	KS_w,		 KS_W,		KS_asciicircum,
    KC(12),	KS_a,
    KC(13),	KS_z,
    /* KC(14),	US, (left japanese) */
    KC(16),	KS_3,		KS_ssharp,
    KC(17),	KS_4,		KS_dollar,
    KC(18),	KS_e,		KS_E,		KS_equal,
    KC(19),	KS_s,
    KC(20),	KS_d,
    KC(21),	KS_x,
    /* KC(22),	^/t (right japanese) */
    KC(24),	KS_5,		KS_percent,
    KC(25),	KS_r,		KS_R,		KS_plus,
    KC(26),	KS_t,		KS_T,		KS_braceleft,
    KC(27),	KS_f,		KS_F,		KS_backslash,
    KC(28),	KS_c,
    KC(29),	KS_minus,	KS_minus,	KS_at,
    KC(30),	KS_Escape, /* Cancel */
    KC(32),	KS_6,	
    KC(33),	KS_y,		KS_Y,		KS_braceright,
    KC(34),	KS_g,		KS_G,		KS_semicolon,
    KC(35),	KS_v,
    KC(36),	KS_b,		KS_B,		KS_underscore,
    KC(37),	KS_space,
    KC(38),	KS_KP_Enter,	/* ok */
    KC(40),	KS_7,		KS_apostrophe,
    KC(41),	KS_8,		KS_parenleft,
    KC(42),	KS_u,		KS_U,	
    KC(43),	KS_h,		KS_H,		KS_colon,
    KC(44),	KS_n,
    KC(45),	KS_comma,	KS_slash,	KS_greater,
    /* KC(46),	Menu, */
    KC(48),	KS_9,		KS_parenright,
    KC(49),	KS_i,		KS_I,	
    KC(50),	KS_j,		KS_J,		KS_asterisk,
    KC(51),	KS_m,
    KC(52),	KS_period,	KS_question,	KS_less,
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
    KC(80),	KS_KP_Next, /* OK, (ext) */
    KC(81),	KS_KP_Next, /* tog left, */
    KC(83),	KS_Shift_R,
    KC(84),	KS_Shift_L,
    KC(88),	KS_KP_Prior, /* cancel (ext), */
    KC(89),	KS_KP_Prior, /* tog right, */
    KC(93),	KS_Mode_switch /* Fn */
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

static const struct wscons_keydesc zkbd_keydesctab[] = {
        KBD_MAP(KB_US,                  0,      zkbd_keydesc_us),
        {0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

/* $OpenBSD: zaurus_kbdmap.h,v 1.3 2005/01/14 00:24:35 drahn Exp $ */

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
    KC(2),	KS_Tab,
    /* KC(3),	Addr, */
    /* KC(4),	Cal, */
    /* KC(5),	Mail, */
    /* KC(6),	Home, */
    KC(8),	KS_1,
    KC(9),	KS_2,
    KC(10),	KS_q,
    KC(11),	KS_w,		 KS_W,		KS_asciicircum,
    KC(12),	KS_a,
    KC(13),	KS_z,
    /* KC(14),	US, (left japanese) */
    KC(16),	KS_3,
    KC(17),	KS_4,
    KC(18),	KS_e,		KS_E,		KS_equal,
    KC(19),	KS_s,
    KC(20),	KS_d,
    KC(21),	KS_x,
    /* KC(22),	^/t (right japanese) */
    KC(24),	KS_5,
    KC(25),	KS_r,		KS_R,		KS_plus,
    KC(26),	KS_t,		KS_T,		KS_parenleft,
    KC(27),	KS_f,		KS_F,		KS_backslash,
    KC(28),	KS_c,
    KC(29),	KS_minus,
    /* KC(30),	Cancel */
    KC(32),	KS_6,
    KC(33),	KS_y,		KS_Y,		KS_parenright,
    KC(34),	KS_g,		KS_G,		KS_semicolon,
    KC(35),	KS_v,
    KC(36),	KS_b,		KS_B,		KS_underscore,
    KC(37),	KS_space,
    /* KC(38),	ok */
    KC(40),	KS_7,
    KC(41),	KS_8,
    KC(42),	KS_u,
    KC(43),	KS_h,		KS_H,		KS_colon,
    KC(44),	KS_n,
    KC(45),	KS_slash,
    /* KC(46),	Menu, */
    KC(48),	KS_9,
    KC(49),	KS_i,
    KC(50),	KS_j,		KS_J,		KS_asterisk,
    KC(51),	KS_m,
    KC(52),	KS_question,
    /* KC(54),	left, */
    KC(56),	KS_0,
    KC(57),	KS_o,
    KC(58),	KS_k,
    KC(59),	KS_l,		KS_L,		KS_bar,
    /* KC(61),	up, */
    /* KC(62),	down, */
    KC(64),	KS_Delete,	KS_BackSpace,
    KC(65),	KS_p,
    KC(68),	KS_Return,
    /* KC(70),	right, */
    /* KC(80),	OK, (ext) */
    /* KC(81),	tog left, */
    KC(83),	KS_Shift_R,
    KC(84),	KS_Shift_L,
    /* KC(88),	cancel (ext), */
    /* KC(89),	tog right, */
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

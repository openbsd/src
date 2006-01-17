/*	$OpenBSD: wskbdmap_lk201.c,v 1.2 2006/01/17 20:26:16 miod Exp $	*/
/* $NetBSD: wskbdmap_lk201.c,v 1.4 2000/12/02 16:57:41 ragge Exp $ */

#include <sys/types.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <vax/dec/wskbdmap_lk201.h>

#define KC(n) KS_KEYCODE((n) - MIN_LK201_KEY)

static const keysym_t zskbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(86),	KS_Cmd_Screen0,	KS_f1,
    KC(87),	KS_Cmd_Screen1,	KS_f2,
    KC(88),	KS_Cmd_Screen2,	KS_f3,
    KC(89),	KS_Cmd_Screen3,	KS_f4,
    KC(90),	KS_Cmd_Screen4,	KS_f5,
    KC(100),	KS_Cmd_Screen5,	KS_f6,
    KC(101),	KS_Cmd_Screen6,	KS_f7,
    KC(102),	KS_Cmd_Screen7,	KS_f8,
    KC(103),	KS_Cmd_Screen8,	KS_f9,
    KC(104),	KS_Cmd_Screen9,	KS_f10,
    KC(113),	KS_Cmd_Debugger,	KS_Escape, /* F11 */
    KC(114),			KS_f12,
    KC(115),			KS_f13,
    KC(116),			KS_f14,
    KC(124),			KS_Help,
    KC(125),	KS_Cmd,		KS_Execute,
    KC(128),			KS_f17,
    KC(129),			KS_f18,
    KC(130),			KS_f19,
    KC(131),			KS_f20,
    KC(138),			KS_Find,
    KC(139),			KS_Insert,
    KC(140),			KS_KP_Delete,
    KC(141),			KS_Select,
    KC(142),			KS_Prior,
    KC(143),			KS_Next,
    KC(146),			KS_KP_0,
    KC(148),			KS_KP_Decimal,
    KC(149),			KS_KP_Enter,
    KC(150),			KS_KP_1,
    KC(151),			KS_KP_2,
    KC(152),			KS_KP_3,
    KC(153),			KS_KP_4,
    KC(154),			KS_KP_5,
    KC(155),			KS_KP_6,
    KC(156),			KS_KP_Separator,
    KC(157),			KS_KP_7,
    KC(158),			KS_KP_8,
    KC(159),			KS_KP_9,
    KC(160),			KS_KP_Subtract,
    KC(161),			KS_KP_F1,
    KC(162),			KS_KP_F2,
    KC(163),			KS_KP_F3,
    KC(164),			KS_KP_F4,
    KC(167),			KS_Left,
    KC(168),			KS_Right,
    KC(169),			KS_Down,
    KC(170),			KS_Up,
    KC(174),			KS_Shift_L,
    KC(175),	KS_Cmd1,	KS_Control_L,
    KC(176),			KS_Caps_Lock,
    KC(177),	KS_Cmd2,	KS_Multi_key, /* (left) compose */
    KC(188),			KS_Delete,
    KC(189),			KS_Return,
    KC(190),			KS_Tab,
    KC(191),			KS_grave,	KS_asciitilde,
    KC(192),			KS_1,		KS_exclam,
    KC(193),			KS_q,
    KC(194),			KS_a,
    KC(195),			KS_z,
    KC(197),			KS_2,		KS_at,
    KC(198),			KS_w,
    KC(199),			KS_s,
    KC(200),			KS_x,
    KC(201),			KS_less,	KS_greater,
    KC(203),			KS_3,		KS_numbersign,
    KC(204),			KS_e,
    KC(205),			KS_d,
    KC(206),			KS_c,
    KC(208),			KS_4,		KS_dollar,
    KC(209),			KS_r,
    KC(210),			KS_f,
    KC(211),			KS_v,
    KC(212),			KS_space,
    KC(214),			KS_5,		KS_percent,
    KC(215),			KS_t,
    KC(216),			KS_g,
    KC(217),			KS_b,
    KC(219),			KS_6,		KS_asciicircum,
    KC(220),			KS_y,
    KC(221),			KS_h,
    KC(222),			KS_n,
    KC(224),			KS_7,		KS_ampersand,
    KC(225),			KS_u,
    KC(226),			KS_j,
    KC(227),			KS_m,
    KC(229),			KS_8,		KS_asterisk,
    KC(230),			KS_i,
    KC(231),			KS_k,
    KC(232),			KS_comma,	KS_less,
    KC(234),			KS_9,		KS_parenleft,
    KC(235),			KS_o,
    KC(236),			KS_l,
    KC(237),			KS_period,	KS_greater,
    KC(239),			KS_0,		KS_parenright,
    KC(240),			KS_p,
    KC(242),			KS_semicolon,	KS_colon,
    KC(243),			KS_slash,	KS_question,
    KC(245),			KS_equal,	KS_plus,
    KC(246),			KS_bracketright,	KS_braceright,
    KC(247),			KS_backslash,	KS_bar,
    KC(249),			KS_minus,	KS_underscore,
    KC(250),			KS_bracketleft,	KS_braceleft,
    KC(251),			KS_apostrophe,	KS_quotedbl,
};

static const keysym_t zskbd_keydesc_us_lk401[] = {
    KC(171),			KS_Shift_R,
    KC(172),	KS_Cmd2,	KS_Alt_L,
    KC(173),			KS_Multi_key, /* right compose */
    KC(177),			KS_Multi_key, /* left compose, not "cmd" */
    KC(178),			KS_Alt_R,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc zskbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	zskbd_keydesc_us),
	KBD_MAP(KB_US | KB_LK401,	KB_US,	zskbd_keydesc_us_lk401),
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC

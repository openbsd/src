/* $OpenBSD: input-keys.c,v 1.62 2017/06/28 11:36:39 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * This file is rather misleadingly named, it contains the code which takes a
 * key code and translates it into something suitable to be sent to the
 * application running in a pane (similar to input.c does in the other
 * direction with output).
 */

static void	 input_key_mouse(struct window_pane *, struct mouse_event *);

struct input_key_ent {
	key_code	 key;
	const char	*data;

	int		 flags;
#define INPUTKEY_KEYPAD 0x1	/* keypad key */
#define INPUTKEY_CURSOR 0x2	/* cursor key */
};

static const struct input_key_ent input_keys[] = {
	/* Backspace key. */
	{ KEYC_BSPACE,		"\177",		0 },

	/* Paste keys. */
	{ KEYC_PASTE_START,	"\033[200~",	0 },
	{ KEYC_PASTE_END,	"\033[201~",	0 },

	/* Function keys. */
	{ KEYC_F1,		"\033OP",	0 },
	{ KEYC_F2,		"\033OQ",	0 },
	{ KEYC_F3,		"\033OR",	0 },
	{ KEYC_F4,		"\033OS",	0 },
	{ KEYC_F5,		"\033[15~",	0 },
	{ KEYC_F6,		"\033[17~",	0 },
	{ KEYC_F7,		"\033[18~",	0 },
	{ KEYC_F8,		"\033[19~",	0 },
	{ KEYC_F9,		"\033[20~",	0 },
	{ KEYC_F10,		"\033[21~",	0 },
	{ KEYC_F11,		"\033[23~",	0 },
	{ KEYC_F12,		"\033[24~",	0 },
	{ KEYC_F1|KEYC_SHIFT,	"\033[25~",	0 },
	{ KEYC_F2|KEYC_SHIFT,	"\033[26~",	0 },
	{ KEYC_F3|KEYC_SHIFT,	"\033[28~",	0 },
	{ KEYC_F4|KEYC_SHIFT,	"\033[29~",	0 },
	{ KEYC_F5|KEYC_SHIFT,	"\033[31~",	0 },
	{ KEYC_F6|KEYC_SHIFT,	"\033[32~",	0 },
	{ KEYC_F7|KEYC_SHIFT,	"\033[33~",	0 },
	{ KEYC_F8|KEYC_SHIFT,	"\033[34~",	0 },
	{ KEYC_IC,		"\033[2~",	0 },
	{ KEYC_DC,		"\033[3~",	0 },
	{ KEYC_HOME,		"\033[1~",	0 },
	{ KEYC_END,		"\033[4~",	0 },
	{ KEYC_NPAGE,		"\033[6~",	0 },
	{ KEYC_PPAGE,		"\033[5~",	0 },
	{ KEYC_BTAB,		"\033[Z",	0 },

	/*
	 * Arrow keys. Cursor versions must come first. The codes are toggled
	 * between CSI and SS3 versions when ctrl is pressed.
	 */
	{ KEYC_UP|KEYC_CTRL,	"\033[A",	INPUTKEY_CURSOR },
	{ KEYC_DOWN|KEYC_CTRL,	"\033[B",	INPUTKEY_CURSOR },
	{ KEYC_RIGHT|KEYC_CTRL,	"\033[C",	INPUTKEY_CURSOR },
	{ KEYC_LEFT|KEYC_CTRL,	"\033[D",	INPUTKEY_CURSOR },

	{ KEYC_UP,		"\033OA",	INPUTKEY_CURSOR },
	{ KEYC_DOWN,		"\033OB",	INPUTKEY_CURSOR },
	{ KEYC_RIGHT,		"\033OC",	INPUTKEY_CURSOR },
	{ KEYC_LEFT,		"\033OD",	INPUTKEY_CURSOR },

	{ KEYC_UP|KEYC_CTRL,	"\033OA",	0 },
	{ KEYC_DOWN|KEYC_CTRL,	"\033OB",	0 },
	{ KEYC_RIGHT|KEYC_CTRL,	"\033OC",	0 },
	{ KEYC_LEFT|KEYC_CTRL,	"\033OD",	0 },

	{ KEYC_UP,		"\033[A",	0 },
	{ KEYC_DOWN,		"\033[B",	0 },
	{ KEYC_RIGHT,		"\033[C",	0 },
	{ KEYC_LEFT,		"\033[D",	0 },

	/* Keypad keys. Keypad versions must come first. */
	{ KEYC_KP_SLASH,	"\033Oo",	INPUTKEY_KEYPAD },
	{ KEYC_KP_STAR,		"\033Oj",	INPUTKEY_KEYPAD },
	{ KEYC_KP_MINUS,	"\033Om",	INPUTKEY_KEYPAD },
	{ KEYC_KP_SEVEN,	"\033Ow",	INPUTKEY_KEYPAD },
	{ KEYC_KP_EIGHT,	"\033Ox",	INPUTKEY_KEYPAD },
	{ KEYC_KP_NINE,		"\033Oy",	INPUTKEY_KEYPAD },
	{ KEYC_KP_PLUS,		"\033Ok",	INPUTKEY_KEYPAD },
	{ KEYC_KP_FOUR,		"\033Ot",	INPUTKEY_KEYPAD },
	{ KEYC_KP_FIVE,		"\033Ou",	INPUTKEY_KEYPAD },
	{ KEYC_KP_SIX,		"\033Ov",	INPUTKEY_KEYPAD },
	{ KEYC_KP_ONE,		"\033Oq",	INPUTKEY_KEYPAD },
	{ KEYC_KP_TWO,		"\033Or",	INPUTKEY_KEYPAD },
	{ KEYC_KP_THREE,	"\033Os",	INPUTKEY_KEYPAD },
	{ KEYC_KP_ENTER,	"\033OM",	INPUTKEY_KEYPAD },
	{ KEYC_KP_ZERO,		"\033Op",	INPUTKEY_KEYPAD },
	{ KEYC_KP_PERIOD,	"\033On",	INPUTKEY_KEYPAD },

	{ KEYC_KP_SLASH,	"/",		0 },
	{ KEYC_KP_STAR,		"*",		0 },
	{ KEYC_KP_MINUS,	"-",		0 },
	{ KEYC_KP_SEVEN,	"7",		0 },
	{ KEYC_KP_EIGHT,	"8",		0 },
	{ KEYC_KP_NINE,		"9",		0 },
	{ KEYC_KP_PLUS,		"+",		0 },
	{ KEYC_KP_FOUR,		"4",		0 },
	{ KEYC_KP_FIVE,		"5",		0 },
	{ KEYC_KP_SIX,		"6",		0 },
	{ KEYC_KP_ONE,		"1",		0 },
	{ KEYC_KP_TWO,		"2",		0 },
	{ KEYC_KP_THREE,	"3",		0 },
	{ KEYC_KP_ENTER,	"\n",		0 },
	{ KEYC_KP_ZERO,		"0",		0 },
	{ KEYC_KP_PERIOD,	".",		0 },
};

/* Split a character into two UTF-8 bytes. */
static size_t
input_split2(u_int c, u_char *dst)
{
	if (c > 0x7f) {
		dst[0] = (c >> 6) | 0xc0;
		dst[1] = (c & 0x3f) | 0x80;
		return (2);
	}
	dst[0] = c;
	return (1);
}

/* Translate a key code into an output key sequence. */
void
input_key(struct window_pane *wp, key_code key, struct mouse_event *m)
{
	const struct input_key_ent	*ike;
	u_int				 i;
	size_t				 dlen;
	char				*out;
	key_code			 justkey;
	struct utf8_data		 ud;

	log_debug("writing key 0x%llx (%s) to %%%u", key,
	    key_string_lookup_key(key), wp->id);

	/* If this is a mouse key, pass off to mouse function. */
	if (KEYC_IS_MOUSE(key)) {
		if (m != NULL && m->wp != -1 && (u_int)m->wp == wp->id)
			input_key_mouse(wp, m);
		return;
	}

	/*
	 * If this is a normal 7-bit key, just send it, with a leading escape
	 * if necessary. If it is a UTF-8 key, split it and send it.
	 */
	justkey = (key & ~(KEYC_XTERM|KEYC_ESCAPE));
	if (justkey <= 0x7f) {
		if (key & KEYC_ESCAPE)
			bufferevent_write(wp->event, "\033", 1);
		ud.data[0] = justkey;
		bufferevent_write(wp->event, &ud.data[0], 1);
		return;
	}
	if (justkey > 0x7f && justkey < KEYC_BASE) {
		if (utf8_split(justkey, &ud) != UTF8_DONE)
			return;
		if (key & KEYC_ESCAPE)
			bufferevent_write(wp->event, "\033", 1);
		bufferevent_write(wp->event, ud.data, ud.size);
		return;
	}

	/*
	 * Then try to look this up as an xterm key, if the flag to output them
	 * is set.
	 */
	if (options_get_number(wp->window->options, "xterm-keys")) {
		if ((out = xterm_keys_lookup(key)) != NULL) {
			bufferevent_write(wp->event, out, strlen(out));
			free(out);
			return;
		}
	}
	key &= ~KEYC_XTERM;

	/* Otherwise look the key up in the table. */
	for (i = 0; i < nitems(input_keys); i++) {
		ike = &input_keys[i];

		if ((ike->flags & INPUTKEY_KEYPAD) &&
		    !(wp->screen->mode & MODE_KKEYPAD))
			continue;
		if ((ike->flags & INPUTKEY_CURSOR) &&
		    !(wp->screen->mode & MODE_KCURSOR))
			continue;

		if ((key & KEYC_ESCAPE) && (ike->key | KEYC_ESCAPE) == key)
			break;
		if (ike->key == key)
			break;
	}
	if (i == nitems(input_keys)) {
		log_debug("key 0x%llx missing", key);
		return;
	}
	dlen = strlen(ike->data);
	log_debug("found key 0x%llx: \"%s\"", key, ike->data);

	/* Prefix a \033 for escape. */
	if (key & KEYC_ESCAPE)
		bufferevent_write(wp->event, "\033", 1);
	bufferevent_write(wp->event, ike->data, dlen);
}

/* Translate mouse and output. */
static void
input_key_mouse(struct window_pane *wp, struct mouse_event *m)
{
	struct screen	*s = wp->screen;
	int		 mode = s->mode;
	char		 buf[40];
	size_t		 len;
	u_int		 x, y;

	if ((mode & ALL_MOUSE_MODES) == 0)
		return;
	if (!window_pane_visible(wp))
		return;
	if (cmd_mouse_at(wp, m, &x, &y, 0) != 0)
		return;

	/* If this pane is not in button or all mode, discard motion events. */
	if (MOUSE_DRAG(m->b) &&
	    (mode & (MODE_MOUSE_BUTTON|MODE_MOUSE_ALL)) == 0)
	    return;

	/*
	 * If this event is a release event and not in all mode, discard it.
	 * In SGR mode we can tell absolutely because a release is normally
	 * shown by the last character. Without SGR, we check if the last
	 * buttons was also a release.
	 */
	if (m->sgr_type != ' ') {
		if (MOUSE_DRAG(m->sgr_b) &&
		    MOUSE_BUTTONS(m->sgr_b) == 3 &&
		    (~mode & MODE_MOUSE_ALL))
			return;
	} else {
		if (MOUSE_DRAG(m->b) &&
		    MOUSE_BUTTONS(m->b) == 3 &&
		    MOUSE_BUTTONS(m->lb) == 3 &&
		    (~mode & MODE_MOUSE_ALL))
			return;
	}

	/*
	 * Use the SGR (1006) extension only if the application requested it
	 * and the underlying terminal also sent the event in this format (this
	 * is because an old style mouse release event cannot be converted into
	 * the new SGR format, since the released button is unknown). Otherwise
	 * pretend that tmux doesn't speak this extension, and fall back to the
	 * UTF-8 (1005) extension if the application requested, or to the
	 * legacy format.
	 */
	if (m->sgr_type != ' ' && (s->mode & MODE_MOUSE_SGR)) {
		len = xsnprintf(buf, sizeof buf, "\033[<%u;%u;%u%c",
		    m->sgr_b, x + 1, y + 1, m->sgr_type);
	} else if (s->mode & MODE_MOUSE_UTF8) {
		if (m->b > 0x7ff - 32 || x > 0x7ff - 33 || y > 0x7ff - 33)
			return;
		len = xsnprintf(buf, sizeof buf, "\033[M");
		len += input_split2(m->b + 32, &buf[len]);
		len += input_split2(x + 33, &buf[len]);
		len += input_split2(y + 33, &buf[len]);
	} else {
		if (m->b > 223)
			return;
		len = xsnprintf(buf, sizeof buf, "\033[M");
		buf[len++] = m->b + 32;
		buf[len++] = x + 33;
		buf[len++] = y + 33;
	}
	log_debug("writing mouse %.*s to %%%u", (int)len, buf, wp->id);
	bufferevent_write(wp->event, buf, len);
}

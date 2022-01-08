/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to manipulate the "line buffer".
 * The line buffer holds a line of output as it is being built
 * in preparation for output to the screen.
 */

#include <wchar.h>
#include <wctype.h>

#include "charset.h"
#include "less.h"

static char *linebuf = NULL;	/* Buffer which holds the current output line */
static char *attr = NULL;	/* Extension of linebuf to hold attributes */
int size_linebuf = 0;		/* Size of line buffer (and attr buffer) */

static int cshift;		/* Current left-shift of output line buffer */
int hshift;			/* Desired left-shift of output line buffer */
int tabstops[TABSTOP_MAX] = { 0 }; /* Custom tabstops */
int ntabstops = 1;		/* Number of tabstops */
int tabdefault = 8;		/* Default repeated tabstops */
off_t highest_hilite;		/* Pos of last hilite in file found so far */

static int curr;		/* Total number of bytes in linebuf */
static int column;		/* Display columns needed to show linebuf */
static int overstrike;		/* Next char should overstrike previous char */
static int is_null_line;	/* There is no current line */
static int lmargin;		/* Index in linebuf of start of content */
static char pendc;
static off_t pendpos;
static char *end_ansi_chars;
static char *mid_ansi_chars;

static int attr_swidth(int);
static int attr_ewidth(int);
static int do_append(LWCHAR, char *, off_t);

extern int bs_mode;
extern int linenums;
extern int ctldisp;
extern int twiddle;
extern int binattr;
extern int status_col;
extern int auto_wrap, ignaw;
extern int bo_s_width, bo_e_width;
extern int ul_s_width, ul_e_width;
extern int bl_s_width, bl_e_width;
extern int so_s_width, so_e_width;
extern int sc_width, sc_height;
extern int utf_mode;
extern off_t start_attnpos;
extern off_t end_attnpos;

static char mbc_buf[MAX_UTF_CHAR_LEN];
static int mbc_buf_index = 0;
static off_t mbc_pos;

/*
 * Initialize from environment variables.
 */
void
init_line(void)
{
	end_ansi_chars = lgetenv("LESSANSIENDCHARS");
	if (end_ansi_chars == NULL || *end_ansi_chars == '\0')
		end_ansi_chars = "m";

	mid_ansi_chars = lgetenv("LESSANSIMIDCHARS");
	if (mid_ansi_chars == NULL || *mid_ansi_chars == '\0')
		mid_ansi_chars = "0123456789;[?!\"'#%()*+ ";

	linebuf = ecalloc(LINEBUF_SIZE, sizeof (char));
	attr = ecalloc(LINEBUF_SIZE, sizeof (char));
	size_linebuf = LINEBUF_SIZE;
}

/*
 * Expand the line buffer.
 */
static int
expand_linebuf(void)
{
	/* Double the size of the line buffer. */
	int new_size = size_linebuf * 2;

	/* Just realloc to expand the buffer, if we can. */
	char *new_buf = recallocarray(linebuf, size_linebuf, new_size, 1);
	if (new_buf != NULL) {
		char *new_attr = recallocarray(attr, size_linebuf, new_size, 1);
		linebuf = new_buf;
		if (new_attr != NULL) {
			attr = new_attr;
			size_linebuf = new_size;
			return (0);
		}
	}
	return (1);
}

/*
 * Is a character ASCII?
 */
static int
is_ascii_char(LWCHAR ch)
{
	return (ch <= 0x7F);
}

/*
 * Rewind the line buffer.
 */
void
prewind(void)
{
	curr = 0;
	column = 0;
	cshift = 0;
	overstrike = 0;
	is_null_line = 0;
	pendc = '\0';
	lmargin = 0;
	if (status_col)
		lmargin += 1;
}

/*
 * Insert the line number (of the given position) into the line buffer.
 */
void
plinenum(off_t pos)
{
	off_t linenum = 0;
	int i;

	if (linenums == OPT_ONPLUS) {
		/*
		 * Get the line number and put it in the current line.
		 * {{ Note: since find_linenum calls forw_raw_line,
		 *    it may seek in the input file, requiring the caller
		 *    of plinenum to re-seek if necessary. }}
		 * {{ Since forw_raw_line modifies linebuf, we must
		 *    do this first, before storing anything in linebuf. }}
		 */
		linenum = find_linenum(pos);
	}

	/*
	 * Display a status column if the -J option is set.
	 */
	if (status_col) {
		linebuf[curr] = ' ';
		if (start_attnpos != -1 &&
		    pos >= start_attnpos && pos < end_attnpos)
			attr[curr] = AT_NORMAL|AT_HILITE;
		else
			attr[curr] = AT_NORMAL;
		curr++;
		column++;
	}
	/*
	 * Display the line number at the start of each line
	 * if the -N option is set.
	 */
	if (linenums == OPT_ONPLUS) {
		char buf[23];
		int n;

		postoa(linenum, buf, sizeof(buf));
		n = strlen(buf);
		if (n < MIN_LINENUM_WIDTH)
			n = MIN_LINENUM_WIDTH;
		snprintf(linebuf+curr, size_linebuf-curr, "%*s ", n, buf);
		n++;	/* One space after the line number. */
		for (i = 0; i < n; i++)
			attr[curr+i] = AT_NORMAL;
		curr += n;
		column += n;
		lmargin += n;
	}

	/*
	 * Append enough spaces to bring us to the lmargin.
	 */
	while (column < lmargin) {
		linebuf[curr] = ' ';
		attr[curr++] = AT_NORMAL;
		column++;
	}
}

/*
 * Shift the input line left.
 * Starting at lmargin, some bytes are discarded from the linebuf,
 * until the number of display columns needed to show these bytes
 * would exceed the argument.
 */
static void
pshift(int shift)
{
	int shifted = 0;  /* Number of display columns already discarded. */
	int from;         /* Index in linebuf of the current character. */
	int to;           /* Index in linebuf to move this character to. */
	int len;          /* Number of bytes in this character. */
	int width = 0;    /* Display columns needed for this character. */
	int prev_attr;    /* Attributes of the preceding character. */
	int next_attr;    /* Attributes of the following character. */
	unsigned char c;  /* First byte of current character. */

	if (shift > column - lmargin)
		shift = column - lmargin;
	if (shift > curr - lmargin)
		shift = curr - lmargin;

	to = from = lmargin;
	/*
	 * We keep on going when shifted == shift
	 * to get all combining chars.
	 */
	while (shifted <= shift && from < curr) {
		c = linebuf[from];
		if (ctldisp == OPT_ONPLUS && c == ESC) {
			/* Keep cumulative effect.  */
			linebuf[to] = c;
			attr[to++] = attr[from++];
			while (from < curr && linebuf[from]) {
				linebuf[to] = linebuf[from];
				attr[to++] = attr[from];
				if (!is_ansi_middle(linebuf[from++]))
					break;
			}
			continue;
		}
		if (utf_mode && !isascii(c)) {
			wchar_t ch;
			/*
			 * Before this point, UTF-8 validity was already
			 * checked, but for additional safety, treat
			 * invalid bytes as single-width characters
			 * if they ever make it here.  Similarly, treat
			 * non-printable characters as width 1.
			 */
			len = mbtowc(&ch, linebuf + from, curr - from);
			if (len == -1)
				len = width = 1;
			else if ((width = wcwidth(ch)) == -1)
				width = 1;
		} else {
			len = 1;
			if (c == '\b')
				/* XXX - Incorrect if several '\b' in a row.  */
				width = width > 0 ? -width : -1;
			else
				width = iscntrl(c) ? 0 : 1;
		}

		if (width == 2 && shift - shifted == 1) {
			/*
			 * Move the first half of a double-width character
			 * off screen.  Print a space instead of the second
			 * half.  This should never happen when called
			 * by pshift_all().
			 */
			attr[to] = attr[from];
			linebuf[to++] = ' ';
			from += len;
			shifted++;
			break;
		}

		/* Adjust width for magic cookies. */
		prev_attr = (to > 0) ? attr[to-1] : AT_NORMAL;
		next_attr = (from + len < curr) ? attr[from + len] : prev_attr;
		if (!is_at_equiv(attr[from], prev_attr) &&
		    !is_at_equiv(attr[from], next_attr)) {
			width += attr_swidth(attr[from]);
			if (from + len < curr)
				width += attr_ewidth(attr[from]);
			if (is_at_equiv(prev_attr, next_attr)) {
				width += attr_ewidth(prev_attr);
				if (from + len < curr)
					width += attr_swidth(next_attr);
			}
		}

		if (shift - shifted < width)
			break;
		from += len;
		shifted += width;
		if (shifted < 0)
			shifted = 0;
	}
	while (from < curr) {
		linebuf[to] = linebuf[from];
		attr[to++] = attr[from++];
	}
	curr = to;
	column -= shifted;
	cshift += shifted;
}

/*
 *
 */
void
pshift_all(void)
{
	pshift(column);
}

/*
 * Return the printing width of the start (enter) sequence
 * for a given character attribute.
 */
static int
attr_swidth(int a)
{
	int w = 0;

	a = apply_at_specials(a);

	if (a & AT_UNDERLINE)
		w += ul_s_width;
	if (a & AT_BOLD)
		w += bo_s_width;
	if (a & AT_BLINK)
		w += bl_s_width;
	if (a & AT_STANDOUT)
		w += so_s_width;

	return (w);
}

/*
 * Return the printing width of the end (exit) sequence
 * for a given character attribute.
 */
static int
attr_ewidth(int a)
{
	int w = 0;

	a = apply_at_specials(a);

	if (a & AT_UNDERLINE)
		w += ul_e_width;
	if (a & AT_BOLD)
		w += bo_e_width;
	if (a & AT_BLINK)
		w += bl_e_width;
	if (a & AT_STANDOUT)
		w += so_e_width;

	return (w);
}

/*
 * Return the printing width of a given character and attribute,
 * if the character were added to the current position in the line buffer.
 * Adding a character with a given attribute may cause an enter or exit
 * attribute sequence to be inserted, so this must be taken into account.
 */
static int
pwidth(wchar_t ch, int a, wchar_t prev_ch)
{
	int w;

	/*
	 * In case of a backspace, back up by the width of the previous
	 * character.  If that is non-printable (for example another
	 * backspace) or zero width (for example a combining accent),
	 * the terminal may actually back up to a character even further
	 * back, but we no longer know how wide that may have been.
	 * The best guess possible at this point is that it was
	 * hopefully width one.
	 */
	if (ch == L'\b') {
		w = wcwidth(prev_ch);
		if (w <= 0)
			w = 1;
		return (-w);
	}

	w = wcwidth(ch);

	/*
	 * Non-printable characters can get here if the -r flag is in
	 * effect, and possibly in some other situations (XXX check that!).
	 * Treat them as zero width.
	 * That may not always match their actual behaviour,
	 * but there is no reasonable way to be more exact.
	 */
	if (w == -1)
		w = 0;

	/*
	 * Combining accents take up no space.
	 * Some terminals, upon failure to compose them with the
	 * characters that precede them, will actually take up one column
	 * for the combining accent; there isn't much we could do short
	 * of testing the (complex) composition process ourselves and
	 * printing a binary representation when it fails.
	 */
	if (w == 0)
		return (0);

	/*
	 * Other characters take one or two columns,
	 * plus the width of any attribute enter/exit sequence.
	 */
	if (curr > 0 && !is_at_equiv(attr[curr-1], a))
		w += attr_ewidth(attr[curr-1]);
	if ((apply_at_specials(a) != AT_NORMAL) &&
	    (curr == 0 || !is_at_equiv(attr[curr-1], a)))
		w += attr_swidth(a);
	return (w);
}

/*
 * Delete to the previous base character in the line buffer.
 * Return 1 if one is found.
 */
static int
backc(void)
{
	wchar_t	 ch, prev_ch;
	int	 len, width;

	if ((len = mbtowc_left(&ch, linebuf + curr, curr)) <= 0)
		return (0);
	curr -= len;

	/* This assumes that there is no '\b' in linebuf.  */
	while (curr >= lmargin && column > lmargin &&
	    !(attr[curr] & (AT_ANSI|AT_BINARY))) {
		if ((len = mbtowc_left(&prev_ch, linebuf + curr, curr)) <= 0)
			prev_ch = L'\0';
		width = pwidth(ch, attr[curr], prev_ch);
		column -= width;
		if (width > 0)
			return (1);
		curr -= len;
		if (prev_ch == L'\0')
			return (0);
		ch = prev_ch;
	}
	return (0);
}

/*
 * Is a character the end of an ANSI escape sequence?
 */
static int
is_ansi_end(LWCHAR ch)
{
	if (!is_ascii_char(ch))
		return (0);
	return (strchr(end_ansi_chars, (char)ch) != NULL);
}

/*
 *
 */
int
is_ansi_middle(LWCHAR ch)
{
	if (!is_ascii_char(ch))
		return (0);
	if (is_ansi_end(ch))
		return (0);
	return (strchr(mid_ansi_chars, (char)ch) != NULL);
}

/*
 * Append a character and attribute to the line buffer.
 */
static int
store_char(LWCHAR ch, char a, char *rep, off_t pos)
{
	int i;
	int w;
	int replen;
	char cs;
	int matches;

	if (is_hilited(pos, pos+1, 0, &matches)) {
		/*
		 * This character should be highlighted.
		 * Override the attribute passed in.
		 */
		if (a != AT_ANSI) {
			if (highest_hilite != -1 && pos > highest_hilite)
				highest_hilite = pos;
			a |= AT_HILITE;
		}
	}

	w = -1;
	if (ctldisp == OPT_ONPLUS) {
		/*
		 * Set i to the beginning of an ANSI escape sequence
		 * that was begun and not yet ended, or to -1 otherwise.
		 */
		for (i = curr - 1; i >= 0; i--) {
			if (linebuf[i] == ESC)
				break;
			if (!is_ansi_middle(linebuf[i]))
				i = 0;
		}
		if (i >= 0 && !is_ansi_end(ch) && !is_ansi_middle(ch)) {
			/* Remove whole unrecognized sequence.  */
			curr = i;
			return (0);
		}
		if (i >= 0 || ch == ESC) {
			a = AT_ANSI;  /* Will force re-AT_'ing around it. */
			w = 0;
		}
	}
	if (w == -1) {
		wchar_t prev_ch;
		if (mbtowc_left(&prev_ch, linebuf + curr, curr) <= 0)
			prev_ch = L' ';
		w = pwidth(ch, a, prev_ch);
	}

	if (ctldisp != OPT_ON && column + w + attr_ewidth(a) > sc_width)
		/*
		 * Won't fit on screen.
		 */
		return (1);

	if (rep == NULL) {
		cs = (char)ch;
		rep = &cs;
		replen = 1;
	} else {
		replen = utf_len(rep[0]);
	}
	if (curr + replen >= size_linebuf-6) {
		/*
		 * Won't fit in line buffer.
		 * Try to expand it.
		 */
		if (expand_linebuf())
			return (1);
	}

	while (replen-- > 0) {
		linebuf[curr] = *rep++;
		attr[curr] = a;
		curr++;
	}
	column += w;
	return (0);
}

/*
 * Append a tab to the line buffer.
 * Store spaces to represent the tab.
 */
static int
store_tab(int attr, off_t pos)
{
	int to_tab = column + cshift - lmargin;
	int i;

	if (ntabstops < 2 || to_tab >= tabstops[ntabstops-1])
		to_tab = tabdefault -
		    ((to_tab - tabstops[ntabstops-1]) % tabdefault);
	else {
		for (i = ntabstops - 2; i >= 0; i--)
			if (to_tab >= tabstops[i])
				break;
		to_tab = tabstops[i+1] - to_tab;
	}

	if (column + to_tab - 1 + pwidth(' ', attr, 0) +
	    attr_ewidth(attr) > sc_width)
		return (1);

	do {
		if (store_char(' ', attr, " ", pos))
			return (1);
	} while (--to_tab > 0);
	return (0);
}

static int
store_prchar(char c, off_t pos)
{
	char *s;

	/*
	 * Convert to printable representation.
	 */
	s = prchar(c);

	/*
	 * Make sure we can get the entire representation
	 * of the character on this line.
	 */
	if (column + (int)strlen(s) - 1 +
	    pwidth(' ', binattr, 0) + attr_ewidth(binattr) > sc_width)
		return (1);

	for (; *s != 0; s++) {
		if (store_char(*s, AT_BINARY, NULL, pos))
			return (1);
	}
	return (0);
}

static int
flush_mbc_buf(off_t pos)
{
	int i;

	for (i = 0; i < mbc_buf_index; i++) {
		if (store_prchar(mbc_buf[i], pos))
			return (mbc_buf_index - i);
	}
	return (0);
}

/*
 * Append a character to the line buffer.
 * Expand tabs into spaces, handle underlining, boldfacing, etc.
 * Returns 0 if ok, 1 if couldn't fit in buffer.
 */
int
pappend(char c, off_t pos)
{
	mbstate_t mbs;
	size_t sz;
	wchar_t ch;
	int r;

	if (pendc) {
		if (do_append(pendc, NULL, pendpos))
			/*
			 * Oops.  We've probably lost the char which
			 * was in pendc, since caller won't back up.
			 */
			return (1);
		pendc = '\0';
	}

	if (c == '\r' && bs_mode == BS_SPECIAL) {
		if (mbc_buf_index > 0)  /* utf_mode must be on. */ {
			/* Flush incomplete (truncated) sequence. */
			r = flush_mbc_buf(mbc_pos);
			mbc_buf_index = 0;
			if (r)
				return (r + 1);
		}

		/*
		 * Don't put the CR into the buffer until we see
		 * the next char.  If the next char is a newline,
		 * discard the CR.
		 */
		pendc = c;
		pendpos = pos;
		return (0);
	}

	if (!utf_mode) {
		r = do_append((LWCHAR) c, NULL, pos);
	} else {
		for (;;) {
			if (mbc_buf_index == 0)
				mbc_pos = pos;
			mbc_buf[mbc_buf_index++] = c;
			memset(&mbs, 0, sizeof(mbs));
			sz = mbrtowc(&ch, mbc_buf, mbc_buf_index, &mbs);

			/* Incomplete UTF-8: wait for more bytes. */
			if (sz == (size_t)-2)
				return (0);

			/* Valid UTF-8: use the character. */
			if (sz != (size_t)-1) {
				r = do_append(ch, mbc_buf, mbc_pos) ?
				    mbc_buf_index : 0;
				break;
			}

			/* Invalid start byte: encode it. */
			if (mbc_buf_index == 1) {
				r = store_prchar(c, pos);
				break;
			}

			/*
			 * Invalid continuation.
			 * Encode the preceding bytes.
			 * If they fit, handle the interrupting byte.
			 * Otherwise, tell the caller to back up
			 * by the  number of bytes that do not fit,
			 * plus one for the new byte.
			 */
			mbc_buf_index--;
			if ((r = flush_mbc_buf(mbc_pos) + 1) == 1)
				mbc_buf_index = 0;
			else
				break;
		}
	}

	/*
	 * If we need to shift the line, do it.
	 * But wait until we get to at least the middle of the screen,
	 * so shifting it doesn't affect the chars we're currently
	 * pappending.  (Bold & underline can get messed up otherwise.)
	 */
	if (cshift < hshift && column > sc_width / 2) {
		linebuf[curr] = '\0';
		pshift(hshift - cshift);
	}
	mbc_buf_index = 0;
	return (r);
}

static int
do_append(LWCHAR ch, char *rep, off_t pos)
{
	wchar_t prev_ch;
	int a;

	a = AT_NORMAL;

	if (ch == '\b') {
		if (bs_mode == BS_CONTROL)
			goto do_control_char;

		/*
		 * A better test is needed here so we don't
		 * backspace over part of the printed
		 * representation of a binary character.
		 */
		if (curr <= lmargin ||
		    column <= lmargin ||
		    (attr[curr - 1] & (AT_ANSI|AT_BINARY))) {
			if (store_prchar('\b', pos))
				return (1);
		} else if (bs_mode == BS_NORMAL) {
			if (store_char(ch, AT_NORMAL, NULL, pos))
				return (1);
		} else if (bs_mode == BS_SPECIAL) {
			overstrike = backc();
		}

		return (0);
	}

	if (overstrike > 0) {
		/*
		 * Overstrike the character at the current position
		 * in the line buffer.  This will cause either
		 * underline (if a "_" is overstruck),
		 * bold (if an identical character is overstruck),
		 * or just deletion of the character in the buffer.
		 */
		overstrike = utf_mode ? -1 : 0;
		/* To be correct, this must be a base character.  */
		if (mbtowc(&prev_ch, linebuf + curr, MB_CUR_MAX) == -1) {
			(void)mbtowc(NULL, NULL, MB_CUR_MAX);
			prev_ch = L'\0';
		}
		a = attr[curr];
		if (ch == prev_ch) {
			/*
			 * Overstriking a char with itself means make it bold.
			 * But overstriking an underscore with itself is
			 * ambiguous.  It could mean make it bold, or
			 * it could mean make it underlined.
			 * Use the previous overstrike to resolve it.
			 */
			if (ch == '_') {
				if ((a & (AT_BOLD|AT_UNDERLINE)) != AT_NORMAL)
					a |= (AT_BOLD|AT_UNDERLINE);
				else if (curr > 0 && attr[curr - 1] & AT_UNDERLINE)
					a |= AT_UNDERLINE;
				else if (curr > 0 && attr[curr - 1] & AT_BOLD)
					a |= AT_BOLD;
				else
					a |= AT_INDET;
			} else {
				a |= AT_BOLD;
			}
		} else if (ch == '_' && prev_ch != L'\0') {
			a |= AT_UNDERLINE;
			ch = prev_ch;
			rep = linebuf + curr;
		} else if (prev_ch == '_') {
			a |= AT_UNDERLINE;
		}
		/* Else we replace prev_ch, but we keep its attributes.  */
	} else if (overstrike < 0) {
		if (wcwidth(ch) == 0) {
			/* Continuation of the same overstrike.  */
			if (curr > 0)
				a = attr[curr - 1] & (AT_UNDERLINE | AT_BOLD);
			else
				a = AT_NORMAL;
		} else
			overstrike = 0;
	}

	if (ch == '\t') {
		/*
		 * Expand a tab into spaces.
		 */
		switch (bs_mode) {
		case BS_CONTROL:
			goto do_control_char;
		case BS_NORMAL:
		case BS_SPECIAL:
			if (store_tab(a, pos))
				return (1);
			break;
		}
	} else if ((!utf_mode || is_ascii_char(ch)) &&
	    !isprint((unsigned char)ch)) {
do_control_char:
		if (ctldisp == OPT_ON ||
		    (ctldisp == OPT_ONPLUS && ch == ESC)) {
			/*
			 * Output as a normal character.
			 */
			if (store_char(ch, AT_NORMAL, rep, pos))
				return (1);
		} else {
			if (store_prchar(ch, pos))
				return (1);
		}
	} else if (utf_mode && ctldisp != OPT_ON && !iswprint(ch)) {
		char *s;

		s = prutfchar(ch);

		if (column + (int)strlen(s) - 1 +
		    pwidth(' ', binattr, 0) + attr_ewidth(binattr) > sc_width)
			return (1);

		for (; *s != 0; s++) {
			if (store_char(*s, AT_BINARY, NULL, pos))
				return (1);
		}
	} else {
		if (store_char(ch, a, rep, pos))
			return (1);
	}
	return (0);
}

/*
 *
 */
int
pflushmbc(void)
{
	int r = 0;

	if (mbc_buf_index > 0) {
		/* Flush incomplete (truncated) sequence.  */
		r = flush_mbc_buf(mbc_pos);
		mbc_buf_index = 0;
	}
	return (r);
}

/*
 * Terminate the line in the line buffer.
 */
void
pdone(int endline, int forw)
{
	int i;

	(void) pflushmbc();

	if (pendc && (pendc != '\r' || !endline))
		/*
		 * If we had a pending character, put it in the buffer.
		 * But discard a pending CR if we are at end of line
		 * (that is, discard the CR in a CR/LF sequence).
		 */
		(void) do_append(pendc, NULL, pendpos);

	for (i = curr - 1; i >= 0; i--) {
		if (attr[i] & AT_INDET) {
			attr[i] &= ~AT_INDET;
			if (i < curr - 1 && attr[i + 1] & AT_BOLD)
				attr[i] |= AT_BOLD;
			else
				attr[i] |= AT_UNDERLINE;
		}
	}

	/*
	 * Make sure we've shifted the line, if we need to.
	 */
	if (cshift < hshift)
		pshift(hshift - cshift);

	if (ctldisp == OPT_ONPLUS && is_ansi_end('m')) {
		/* Switch to normal attribute at end of line. */
		char *p = "\033[m";
		for (; *p != '\0'; p++) {
			linebuf[curr] = *p;
			attr[curr++] = AT_ANSI;
		}
	}

	/*
	 * Add a newline if necessary,
	 * and append a '\0' to the end of the line.
	 * We output a newline if we're not at the right edge of the screen,
	 * or if the terminal doesn't auto wrap,
	 * or if this is really the end of the line AND the terminal ignores
	 * a newline at the right edge.
	 * (In the last case we don't want to output a newline if the terminal
	 * doesn't ignore it since that would produce an extra blank line.
	 * But we do want to output a newline if the terminal ignores it in case
	 * the next line is blank.  In that case the single newline output for
	 * that blank line would be ignored!)
	 */
	if (column < sc_width || !auto_wrap || (endline && ignaw) ||
	    ctldisp == OPT_ON) {
		linebuf[curr] = '\n';
		attr[curr] = AT_NORMAL;
		curr++;
	} else if (ignaw && column >= sc_width && forw) {
		/*
		 * Terminals with "ignaw" don't wrap until they *really* need
		 * to, i.e. when the character *after* the last one to fit on a
		 * line is output. But they are too hard to deal with when they
		 * get in the state where a full screen width of characters
		 * have been output but the cursor is sitting on the right edge
		 * instead of at the start of the next line.
		 * So we nudge them into wrapping by outputting a space
		 * character plus a backspace.  But do this only if moving
		 * forward; if we're moving backward and drawing this line at
		 * the top of the screen, the space would overwrite the first
		 * char on the next line.  We don't need to do this "nudge"
		 * at the top of the screen anyway.
		 */
		linebuf[curr] = ' ';
		attr[curr++] = AT_NORMAL;
		linebuf[curr] = '\b';
		attr[curr++] = AT_NORMAL;
	}
	linebuf[curr] = '\0';
	attr[curr] = AT_NORMAL;
}

/*
 *
 */
void
set_status_col(char c)
{
	linebuf[0] = c;
	attr[0] = AT_NORMAL|AT_HILITE;
}

/*
 * Get a character from the current line.
 * Return the character as the function return value,
 * and the character attribute in *ap.
 */
int
gline(int i, int *ap)
{
	if (is_null_line) {
		/*
		 * If there is no current line, we pretend the line is
		 * either "~" or "", depending on the "twiddle" flag.
		 */
		if (twiddle) {
			if (i == 0) {
				*ap = AT_BOLD;
				return ('~');
			}
			--i;
		}
		/* Make sure we're back to AT_NORMAL before the '\n'.  */
		*ap = AT_NORMAL;
		return (i ? '\0' : '\n');
	}

	*ap = attr[i];
	return (linebuf[i] & 0xFF);
}

/*
 * Indicate that there is no current line.
 */
void
null_line(void)
{
	is_null_line = 1;
	cshift = 0;
}

/*
 * Analogous to forw_line(), but deals with "raw lines":
 * lines which are not split for screen width.
 * {{ This is supposed to be more efficient than forw_line(). }}
 */
off_t
forw_raw_line(off_t curr_pos, char **linep, int *line_lenp)
{
	int n;
	int c;
	off_t new_pos;

	if (curr_pos == -1 || ch_seek(curr_pos) ||
	    (c = ch_forw_get()) == EOI)
		return (-1);

	n = 0;
	for (;;) {
		if (c == '\n' || c == EOI || abort_sigs()) {
			new_pos = ch_tell();
			break;
		}
		if (n >= size_linebuf-1) {
			if (expand_linebuf()) {
				/*
				 * Overflowed the input buffer.
				 * Pretend the line ended here.
				 */
				new_pos = ch_tell() - 1;
				break;
			}
		}
		linebuf[n++] = (char)c;
		c = ch_forw_get();
	}
	linebuf[n] = '\0';
	if (linep != NULL)
		*linep = linebuf;
	if (line_lenp != NULL)
		*line_lenp = n;
	return (new_pos);
}

/*
 * Analogous to back_line(), but deals with "raw lines".
 * {{ This is supposed to be more efficient than back_line(). }}
 */
off_t
back_raw_line(off_t curr_pos, char **linep, int *line_lenp)
{
	int n;
	int c;
	off_t new_pos;

	if (curr_pos == -1 || curr_pos <= ch_zero() || ch_seek(curr_pos - 1))
		return (-1);

	n = size_linebuf;
	linebuf[--n] = '\0';
	for (;;) {
		c = ch_back_get();
		if (c == '\n' || abort_sigs()) {
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI) {
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			new_pos = ch_zero();
			break;
		}
		if (n <= 0) {
			int old_size_linebuf = size_linebuf;
			if (expand_linebuf()) {
				/*
				 * Overflowed the input buffer.
				 * Pretend the line ended here.
				 */
				new_pos = ch_tell() + 1;
				break;
			}
			/*
			 * Shift the data to the end of the new linebuf.
			 */
			n = size_linebuf - old_size_linebuf;
			memmove(linebuf + n, linebuf, old_size_linebuf);
		}
		linebuf[--n] = c;
	}
	if (linep != NULL)
		*linep = &linebuf[n];
	if (line_lenp != NULL)
		*line_lenp = size_linebuf - 1 - n;
	return (new_pos);
}

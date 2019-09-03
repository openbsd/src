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
 * High level routines dealing with getting lines of input
 * from the file being viewed.
 *
 * When we speak of "lines" here, we mean PRINTABLE lines;
 * lines processed with respect to the screen width.
 * We use the term "raw line" to refer to lines simply
 * delimited by newlines; not processed with respect to screen width.
 */

#include "less.h"

extern int squeeze;
extern int chopline;
extern int hshift;
extern int quit_if_one_screen;
extern int ignore_eoi;
extern int status_col;
extern off_t start_attnpos;
extern off_t end_attnpos;
extern int hilite_search;
extern int size_linebuf;

/*
 * Get the next line.
 * A "current" position is passed and a "new" position is returned.
 * The current position is the position of the first character of
 * a line.  The new position is the position of the first character
 * of the NEXT line.  The line obtained is the line starting at curr_pos.
 */
off_t
forw_line(off_t curr_pos)
{
	off_t base_pos;
	off_t new_pos;
	int c;
	int blankline;
	int endline;
	int backchars;

get_forw_line:
	if (curr_pos == -1) {
		null_line();
		return (-1);
	}
	if (hilite_search == OPT_ONPLUS || is_filtering() || status_col)
		/*
		 * If we are ignoring EOI (command F), only prepare
		 * one line ahead, to avoid getting stuck waiting for
		 * slow data without displaying the data we already have.
		 * If we're not ignoring EOI, we *could* do the same, but
		 * for efficiency we prepare several lines ahead at once.
		 */
		prep_hilite(curr_pos, curr_pos + 3*size_linebuf,
		    ignore_eoi ? 1 : -1);
	if (ch_seek(curr_pos)) {
		null_line();
		return (-1);
	}

	/*
	 * Step back to the beginning of the line.
	 */
	base_pos = curr_pos;
	for (;;) {
		if (abort_sigs()) {
			null_line();
			return (-1);
		}
		c = ch_back_get();
		if (c == EOI)
			break;
		if (c == '\n') {
			(void) ch_forw_get();
			break;
		}
		--base_pos;
	}

	/*
	 * Read forward again to the position we should start at.
	 */
	prewind();
	plinenum(base_pos);
	(void) ch_seek(base_pos);
	new_pos = base_pos;
	while (new_pos < curr_pos) {
		if (abort_sigs()) {
			null_line();
			return (-1);
		}
		c = ch_forw_get();
		backchars = pappend(c, new_pos);
		new_pos++;
		if (backchars > 0) {
			pshift_all();
			new_pos -= backchars;
			while (--backchars >= 0)
				(void) ch_back_get();
		}
	}
	(void) pflushmbc();
	pshift_all();

	/*
	 * Read the first character to display.
	 */
	c = ch_forw_get();
	if (c == EOI) {
		null_line();
		return (-1);
	}
	blankline = (c == '\n' || c == '\r');

	/*
	 * Read each character in the line and append to the line buffer.
	 */
	for (;;) {
		if (abort_sigs()) {
			null_line();
			return (-1);
		}
		if (c == '\n' || c == EOI) {
			/*
			 * End of the line.
			 */
			backchars = pflushmbc();
			new_pos = ch_tell();
			if (backchars > 0 && !chopline && hshift == 0) {
				new_pos -= backchars + 1;
				endline = FALSE;
			} else
				endline = TRUE;
			break;
		}
		if (c != '\r')
			blankline = 0;

		/*
		 * Append the char to the line and get the next char.
		 */
		backchars = pappend(c, ch_tell()-1);
		if (backchars > 0) {
			/*
			 * The char won't fit in the line; the line
			 * is too long to print in the screen width.
			 * End the line here.
			 */
			if (chopline || hshift > 0) {
				do {
					if (abort_sigs()) {
						null_line();
						return (-1);
					}
					c = ch_forw_get();
				} while (c != '\n' && c != EOI);
				new_pos = ch_tell();
				endline = TRUE;
				quit_if_one_screen = FALSE;
			} else {
				new_pos = ch_tell() - backchars;
				endline = FALSE;
			}
			break;
		}
		c = ch_forw_get();
	}

	pdone(endline, 1);

	if (is_filtered(base_pos)) {
		/*
		 * We don't want to display this line.
		 * Get the next line.
		 */
		curr_pos = new_pos;
		goto get_forw_line;
	}

	if (status_col && is_hilited(base_pos, ch_tell()-1, 1, NULL))
		set_status_col('*');

	if (squeeze && blankline) {
		/*
		 * This line is blank.
		 * Skip down to the last contiguous blank line
		 * and pretend it is the one which we are returning.
		 */
		while ((c = ch_forw_get()) == '\n' || c == '\r')
			if (abort_sigs()) {
				null_line();
				return (-1);
			}
		if (c != EOI)
			(void) ch_back_get();
		new_pos = ch_tell();
	}

	return (new_pos);
}

/*
 * Get the previous line.
 * A "current" position is passed and a "new" position is returned.
 * The current position is the position of the first character of
 * a line.  The new position is the position of the first character
 * of the PREVIOUS line.  The line obtained is the one starting at new_pos.
 */
off_t
back_line(off_t curr_pos)
{
	off_t new_pos, begin_new_pos, base_pos;
	int c;
	int endline;
	int backchars;

get_back_line:
	if (curr_pos == -1 || curr_pos <= ch_zero()) {
		null_line();
		return (-1);
	}
	if (hilite_search == OPT_ONPLUS || is_filtering() || status_col)
		prep_hilite((curr_pos < 3*size_linebuf) ?
		    0 : curr_pos - 3*size_linebuf, curr_pos, -1);
	if (ch_seek(curr_pos-1)) {
		null_line();
		return (-1);
	}

	if (squeeze) {
		/*
		 * Find out if the "current" line was blank.
		 */
		(void) ch_forw_get();	/* Skip the newline */
		c = ch_forw_get();	/* First char of "current" line */
		(void) ch_back_get();	/* Restore our position */
		(void) ch_back_get();

		if (c == '\n' || c == '\r') {
			/*
			 * The "current" line was blank.
			 * Skip over any preceding blank lines,
			 * since we skipped them in forw_line().
			 */
			while ((c = ch_back_get()) == '\n' || c == '\r')
				if (abort_sigs()) {
					null_line();
					return (-1);
				}
			if (c == EOI) {
				null_line();
				return (-1);
			}
			(void) ch_forw_get();
		}
	}

	/*
	 * Scan backwards until we hit the beginning of the line.
	 */
	for (;;) {
		if (abort_sigs()) {
			null_line();
			return (-1);
		}
		c = ch_back_get();
		if (c == '\n') {
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			base_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI) {
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			base_pos = ch_tell();
			break;
		}
	}

	/*
	 * Now scan forwards from the beginning of this line.
	 * We keep discarding "printable lines" (based on screen width)
	 * until we reach the curr_pos.
	 *
	 * {{ This algorithm is pretty inefficient if the lines
	 *    are much longer than the screen width,
	 *    but I don't know of any better way. }}
	 */
	new_pos = base_pos;
	if (ch_seek(new_pos)) {
		null_line();
		return (-1);
	}
	endline = FALSE;
	prewind();
	plinenum(new_pos);
loop:
	begin_new_pos = new_pos;
	(void) ch_seek(new_pos);

	do {
		c = ch_forw_get();
		if (c == EOI || abort_sigs()) {
			null_line();
			return (-1);
		}
		new_pos++;
		if (c == '\n') {
			backchars = pflushmbc();
			if (backchars > 0 && !chopline && hshift == 0) {
				backchars++;
				goto shift;
			}
			endline = TRUE;
			break;
		}
		backchars = pappend(c, ch_tell()-1);
		if (backchars > 0) {
			/*
			 * Got a full printable line, but we haven't
			 * reached our curr_pos yet.  Discard the line
			 * and start a new one.
			 */
			if (chopline || hshift > 0) {
				endline = TRUE;
				quit_if_one_screen = FALSE;
				break;
			}
		shift:
			pshift_all();
			while (backchars-- > 0) {
				(void) ch_back_get();
				new_pos--;
			}
			goto loop;
		}
	} while (new_pos < curr_pos);

	pdone(endline, 0);

	if (is_filtered(base_pos)) {
		/*
		 * We don't want to display this line.
		 * Get the previous line.
		 */
		curr_pos = begin_new_pos;
		goto get_back_line;
	}

	if (status_col && curr_pos > 0 &&
	    is_hilited(base_pos, curr_pos-1, 1, NULL))
		set_status_col('*');

	return (begin_new_pos);
}

/*
 * Set attnpos.
 */
void
set_attnpos(off_t pos)
{
	int c;

	if (pos != -1) {
		if (ch_seek(pos))
			return;
		for (;;) {
			c = ch_forw_get();
			if (c == EOI)
				return;
			if (c != '\n' && c != '\r')
				break;
			pos++;
		}
	}
	start_attnpos = pos;
	for (;;) {
		c = ch_forw_get();
		pos++;
		if (c == EOI || c == '\n' || c == '\r')
			break;
	}
	end_attnpos = pos;
}

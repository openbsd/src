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
 * Routines which jump to a new location in the file.
 */

#include "less.h"
#include "position.h"

extern int jump_sline;
extern int squished;
extern int screen_trashed;
extern int sc_width, sc_height;
extern int show_attn;
extern int top_scroll;

/*
 * Jump to the end of the file.
 */
void
jump_forw(void)
{
	off_t pos;
	off_t end_pos;

	if (ch_end_seek()) {
		error("Cannot seek to end of file", NULL);
		return;
	}
	/*
	 * Note; lastmark will be called later by jump_loc, but it fails
	 * because the position table has been cleared by pos_clear below.
	 * So call it here before calling pos_clear.
	 */
	lastmark();
	/*
	 * Position the last line in the file at the last screen line.
	 * Go back one line from the end of the file
	 * to get to the beginning of the last line.
	 */
	pos_clear();
	end_pos = ch_tell();
	pos = back_line(end_pos);
	if (pos == -1) {
		jump_loc(0, sc_height-1);
	} else {
		jump_loc(pos, sc_height-1);
		if (position(sc_height-1) != end_pos)
			repaint();
	}
}

/*
 * Jump to line n in the file.
 */
void
jump_back(off_t linenum)
{
	off_t pos;
	PARG parg;

	/*
	 * Find the position of the specified line.
	 * If we can seek there, just jump to it.
	 * If we can't seek, but we're trying to go to line number 1,
	 * use ch_beg_seek() to get as close as we can.
	 */
	pos = find_pos(linenum);
	if (pos != -1 && ch_seek(pos) == 0) {
		if (show_attn)
			set_attnpos(pos);
		jump_loc(pos, jump_sline);
	} else if (linenum <= 1 && ch_beg_seek() == 0) {
		jump_loc(ch_tell(), jump_sline);
		error("Cannot seek to beginning of file", NULL);
	} else {
		parg.p_linenum = linenum;
		error("Cannot seek to line number %n", &parg);
	}
}

/*
 * Repaint the screen.
 */
void
repaint(void)
{
	struct scrpos scrpos;
	/*
	 * Start at the line currently at the top of the screen
	 * and redisplay the screen.
	 */
	get_scrpos(&scrpos);
	pos_clear();
	jump_loc(scrpos.pos, scrpos.ln);
}

/*
 * Jump to a specified percentage into the file.
 */
void
jump_percent(int percent, long fraction)
{
	off_t pos, len;

	/*
	 * Determine the position in the file
	 * (the specified percentage of the file's length).
	 */
	if ((len = ch_length()) == -1) {
		ierror("Determining length of file", NULL);
		ch_end_seek();
	}
	if ((len = ch_length()) == -1) {
		error("Don't know length of file", NULL);
		return;
	}
	pos = percent_pos(len, percent, fraction);
	if (pos >= len)
		pos = len-1;

	jump_line_loc(pos, jump_sline);
}

/*
 * Jump to a specified position in the file.
 * Like jump_loc, but the position need not be
 * the first character in a line.
 */
void
jump_line_loc(off_t pos, int sline)
{
	int c;

	if (ch_seek(pos) == 0) {
		/*
		 * Back up to the beginning of the line.
		 */
		while ((c = ch_back_get()) != '\n' && c != EOI)
			;
		if (c == '\n')
			(void) ch_forw_get();
		pos = ch_tell();
	}
	if (show_attn)
		set_attnpos(pos);
	jump_loc(pos, sline);
}

/*
 * Jump to a specified position in the file.
 * The position must be the first character in a line.
 * Place the target line on a specified line on the screen.
 */
void
jump_loc(off_t pos, int sline)
{
	int nline;
	off_t tpos;
	off_t bpos;

	/*
	 * Normalize sline.
	 */
	sline = adjsline(sline);

	if ((nline = onscreen(pos)) >= 0) {
		/*
		 * The line is currently displayed.
		 * Just scroll there.
		 */
		nline -= sline;
		if (nline > 0)
			forw(nline, position(BOTTOM_PLUS_ONE), 1, 0, 0);
		else
			back(-nline, position(TOP), 1, 0);
		if (show_attn)
			repaint_hilite(1);
		return;
	}

	/*
	 * Line is not on screen.
	 * Seek to the desired location.
	 */
	if (ch_seek(pos)) {
		error("Cannot seek to that file position", NULL);
		return;
	}

	/*
	 * See if the desired line is before or after
	 * the currently displayed screen.
	 */
	tpos = position(TOP);
	bpos = position(BOTTOM_PLUS_ONE);
	if (tpos == -1 || pos >= tpos) {
		/*
		 * The desired line is after the current screen.
		 * Move back in the file far enough so that we can
		 * call forw() and put the desired line at the
		 * sline-th line on the screen.
		 */
		for (nline = 0; nline < sline; nline++) {
			if (bpos != -1 && pos <= bpos) {
				/*
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				forw(sc_height-sline+nline-1, bpos, 1, 0, 0);
				if (show_attn)
					repaint_hilite(1);
				return;
			}
			pos = back_line(pos);
			if (pos == -1) {
				/*
				 * Oops.  Ran into the beginning of the file.
				 * Exit the loop here and rely on forw()
				 * below to draw the required number of
				 * blank lines at the top of the screen.
				 */
				break;
			}
		}
		lastmark();
		squished = 0;
		screen_trashed = 0;
		forw(sc_height-1, pos, 1, 0, sline-nline);
	} else {
		/*
		 * The desired line is before the current screen.
		 * Move forward in the file far enough so that we
		 * can call back() and put the desired line at the
		 * sline-th line on the screen.
		 */
		for (nline = sline; nline < sc_height - 1; nline++) {
			pos = forw_line(pos);
			if (pos == -1) {
				/*
				 * Ran into end of file.
				 * This shouldn't normally happen,
				 * but may if there is some kind of read error.
				 */
				break;
			}
			if (pos >= tpos) {
				/*
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				back(nline + 1, tpos, 1, 0);
				if (show_attn)
					repaint_hilite(1);
				return;
			}
		}
		lastmark();
		if (!top_scroll)
			do_clear();
		else
			home();
		screen_trashed = 0;
		add_back_pos(pos);
		back(sc_height-1, pos, 1, 0);
	}
}

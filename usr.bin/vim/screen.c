/*	$OpenBSD: screen.c,v 1.3 1996/10/14 03:55:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * screen.c: code for displaying on the screen
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"		/* For op_inclusive */

char *tgoto __PARMS((char *cm, int col, int line));

static int		canopt;			/* TRUE when cursor goto can be optimized */
static int		attributes = 0;	/* current attributes for screen character*/
static int 		highlight_attr = 0;	/* attributes when highlighting on */
#ifdef RIGHTLEFT
static int		rightleft = 0;	/* set to 1 for right to left in screen_fill */
#endif

static int win_line __ARGS((WIN *, linenr_t, int, int));
static void comp_Botline_sub __ARGS((WIN *wp, linenr_t lnum, int done));
static void screen_char __ARGS((char_u *, int, int));
static void screenclear2 __ARGS((void));
static void lineclear __ARGS((char_u *p));
static int screen_ins_lines __ARGS((int, int, int, int));

/*
 * updateline() - like updateScreen() but only for cursor line
 *
 * Check if the size of the cursor line has changed.  If it did change, lines
 * below the cursor will move up or down and we need to call the routine
 * updateScreen() to examine the entire screen.
 */
	void
updateline()
{
	int 		row;
	int 		n;

	if (!screen_valid(TRUE))
		return;

	if (must_redraw)					/* must redraw whole screen */
	{
		updateScreen(must_redraw);
		return;
	}

	if (RedrawingDisabled)
	{
		must_redraw = NOT_VALID;		/* remember to update later */
		return;
	}

	cursor_off();

	(void)set_highlight('v');
	row = win_line(curwin, curwin->w_cursor.lnum,
									   curwin->w_cline_row, curwin->w_height);

	if (row == curwin->w_height + 1)	/* line too long for window */
	{
			/* window needs to be scrolled up to show the cursor line */
		if (curwin->w_topline < curwin->w_cursor.lnum)
			++curwin->w_topline;
		updateScreen(VALID_TO_CURSCHAR);
		cursupdate();
	}
	else if (!dollar_vcol)
	{
		n = row - curwin->w_cline_row;
		if (n != curwin->w_cline_height)		/* line changed size */
		{
			if (n < curwin->w_cline_height) 	/* got smaller: delete lines */
				win_del_lines(curwin, row,
									 curwin->w_cline_height - n, FALSE, TRUE);
			else								/* got bigger: insert lines */
				win_ins_lines(curwin,
								 curwin->w_cline_row + curwin->w_cline_height,
									 n - curwin->w_cline_height, FALSE, TRUE);
			updateScreen(VALID_TO_CURSCHAR);
		}
		else if (clear_cmdline || redraw_cmdline)
			showmode();				/* clear cmdline, show mode and ruler */
	}
}

/*
 * update all windows that are editing the current buffer
 */
	void
update_curbuf(type)
	int			type;
{
	WIN				*wp;

	for (wp = firstwin; wp; wp = wp->w_next)
		if (wp->w_buffer == curbuf && wp->w_redr_type < type)
			wp->w_redr_type = type;
	updateScreen(type);
}

/*
 * updateScreen()
 *
 * Based on the current value of curwin->w_topline, transfer a screenfull
 * of stuff from Filemem to NextScreen, and update curwin->w_botline.
 */

	void
updateScreen(type)
	int 			type;
{
	WIN				*wp;

	if (!screen_valid(TRUE))
		return;

	dollar_vcol = 0;

	if (must_redraw)
	{
		if (type < must_redraw)		/* use maximal type */
			type = must_redraw;
		must_redraw = 0;
	}

	if (type == CURSUPD)		/* update cursor and then redraw NOT_VALID */
	{
		curwin->w_lsize_valid = 0;
		cursupdate();			/* will call updateScreen() */
		return;
	}
	if (curwin->w_lsize_valid == 0 && type < NOT_VALID)
		type = NOT_VALID;

 	if (RedrawingDisabled)
	{
		must_redraw = type;		/* remember type for next time */
		curwin->w_redr_type = type;
		curwin->w_lsize_valid = 0;		/* don't use w_lsize[] now */
		return;
	}

	/*
	 * if the screen was scrolled up when displaying a message, scroll it down
	 */
	if (msg_scrolled)
	{
		clear_cmdline = TRUE;
		if (msg_scrolled > Rows - 5)		/* clearing is faster */
			type = CLEAR;
		else if (type != CLEAR)
		{
			if (screen_ins_lines(0, 0, msg_scrolled, (int)Rows) == FAIL)
				type = CLEAR;
			win_rest_invalid(firstwin);		/* should do only first/last few */
		}
		msg_scrolled = 0;
		need_wait_return = FALSE;
	}

	/*
	 * reset cmdline_row now (may have been changed temporarily)
	 */
	compute_cmdrow();

	if (type == CLEAR)			/* first clear screen */
	{
		screenclear();			/* will reset clear_cmdline */
		type = NOT_VALID;
	}

	if (clear_cmdline)			/* first clear cmdline */
	{
		if (emsg_on_display)
		{
			mch_delay(1000L, TRUE);
			emsg_on_display = FALSE;
		}
		msg_row = cmdline_row;
		msg_col = 0;
		msg_clr_eos();			/* will reset clear_cmdline */
	}

/* return if there is nothing to do */
	if (!((type == VALID && curwin->w_topline == curwin->w_lsize_lnum[0]) ||
			(type == INVERTED &&
					curwin->w_old_cursor_lnum == curwin->w_cursor.lnum &&
					curwin->w_old_cursor_vcol == curwin->w_virtcol &&
					curwin->w_old_curswant == curwin->w_curswant)))
	{
		/*
		 * go from top to bottom through the windows, redrawing the ones that
		 * need it
		 */
		curwin->w_redr_type = type;
		cursor_off();
		for (wp = firstwin; wp; wp = wp->w_next)
		{
			if (wp->w_redr_type)
				win_update(wp);
			if (wp->w_redr_status)
				win_redr_status(wp);
		}
	}
	if (redraw_cmdline)
		showmode();
}

#ifdef USE_GUI
/*
 * Update a single window, its status line and maybe the command line msg.
 * Used for the GUI scrollbar.
 */
	void
updateWindow(wp)
	WIN		*wp;
{
	win_update(wp);
	if (wp->w_redr_status)
		win_redr_status(wp);
	if (redraw_cmdline)
		showmode();
}
#endif

/*
 * update a single window
 *
 * This may cause the windows below it also to be redrawn
 */
	void
win_update(wp)
	WIN		*wp;
{
	int				type = wp->w_redr_type;
	register int	row;
	register int	endrow;
	linenr_t		lnum;
	linenr_t		lastline = 0;	/* only valid if endrow != Rows -1 */
	int				done;			/* if TRUE, we hit the end of the file */
	int				didline;		/* if TRUE, we finished the last line */
	int 			srow = 0;		/* starting row of the current line */
	int 			idx;
	int 			i;
	long 			j;

	if (type == NOT_VALID)
	{
		wp->w_redr_status = TRUE;
		wp->w_lsize_valid = 0;
	}

	idx = 0;
	row = 0;
	lnum = wp->w_topline;

	/* The number of rows shown is w_height. */
	/* The default last row is the status/command line. */
	endrow = wp->w_height;

	if (type == VALID || type == VALID_TO_CURSCHAR)
	{
		/*
		 * We handle two special cases:
		 * 1: we are off the top of the screen by a few lines: scroll down
		 * 2: wp->w_topline is below wp->w_lsize_lnum[0]: may scroll up
		 */
		if (wp->w_topline < wp->w_lsize_lnum[0])	/* may scroll down */
		{
			j = wp->w_lsize_lnum[0] - wp->w_topline;
			if (j < wp->w_height - 2)				/* not too far off */
			{
				lastline = wp->w_lsize_lnum[0] - 1;
				i = plines_m_win(wp, wp->w_topline, lastline);
				if (i < wp->w_height - 2)		/* less than a screen off */
				{
					/*
					 * Try to insert the correct number of lines.
					 * If not the last window, delete the lines at the bottom.
					 * win_ins_lines may fail.
					 */
					if (win_ins_lines(wp, 0, i, FALSE, wp == firstwin) == OK &&
													wp->w_lsize_valid)
					{
						endrow = i;

						if ((wp->w_lsize_valid += j) > wp->w_height)
							wp->w_lsize_valid = wp->w_height;
						for (idx = wp->w_lsize_valid; idx - j >= 0; idx--)
						{
							wp->w_lsize_lnum[idx] = wp->w_lsize_lnum[idx - j];
							wp->w_lsize[idx] = wp->w_lsize[idx - j];
						}
						idx = 0;
					}
				}
				else if (lastwin == firstwin)
					screenclear();	/* far off: clearing the screen is faster */
			}
			else if (lastwin == firstwin)
				screenclear();		/* far off: clearing the screen is faster */
		}
		else							/* may scroll up */
		{
			j = -1;
						/* try to find wp->w_topline in wp->w_lsize_lnum[] */
			for (i = 0; i < wp->w_lsize_valid; i++)
			{
				if (wp->w_lsize_lnum[i] == wp->w_topline)
				{
					j = i;
					break;
				}
				row += wp->w_lsize[i];
			}
			if (j == -1)	/* wp->w_topline is not in wp->w_lsize_lnum */
			{
				row = 0;
				if (lastwin == firstwin)
					screenclear();	/* far off: clearing the screen is faster */
			}
			else
			{
				/*
				 * Try to delete the correct number of lines.
				 * wp->w_topline is at wp->w_lsize_lnum[i].
				 */
				if ((row == 0 || win_del_lines(wp, 0, row,
							FALSE, wp == firstwin) == OK) && wp->w_lsize_valid)
				{
					srow = row;
					row = 0;
					for (;;)
					{
						if (type == VALID_TO_CURSCHAR &&
													lnum == wp->w_cursor.lnum)
								break;
						if (row + srow + (int)wp->w_lsize[j] >= wp->w_height)
								break;
						wp->w_lsize[idx] = wp->w_lsize[j];
						wp->w_lsize_lnum[idx] = lnum++;

						row += wp->w_lsize[idx++];
						if ((int)++j >= wp->w_lsize_valid)
							break;
					}
					wp->w_lsize_valid = idx;
				}
				else
					row = 0;		/* update all lines */
			}
		}
		if (endrow == wp->w_height && idx == 0) 	/* no scrolling */
				wp->w_lsize_valid = 0;
	}

	done = didline = FALSE;

	if (VIsual_active)		/* check if we are updating the inverted part */
	{
		linenr_t	from, to;

	/* find the line numbers that need to be updated */
		if (curwin->w_cursor.lnum < wp->w_old_cursor_lnum)
		{
			from = curwin->w_cursor.lnum;
			to = wp->w_old_cursor_lnum;
		}
		else
		{
			from = wp->w_old_cursor_lnum;
			to = curwin->w_cursor.lnum;
		}
			/* if VIsual changed, update the maximal area */
		if (VIsual.lnum != wp->w_old_visual_lnum)
		{
			if (wp->w_old_visual_lnum < from)
				from = wp->w_old_visual_lnum;
			if (wp->w_old_visual_lnum > to)
				to = wp->w_old_visual_lnum;
			if (VIsual.lnum < from)
				from = VIsual.lnum;
			if (VIsual.lnum > to)
				to = VIsual.lnum;
		}
	/* if in block mode and changed column or wp->w_curswant: update all
	 * lines */
		if (VIsual_mode == Ctrl('V') &&
						(curwin->w_virtcol != wp->w_old_cursor_vcol ||
						wp->w_curswant != wp->w_old_curswant))
		{
			if (from > VIsual.lnum)
				from = VIsual.lnum;
			if (to < VIsual.lnum)
				to = VIsual.lnum;
		}

		if (from < wp->w_topline)
			from = wp->w_topline;
		if (from >= wp->w_botline)
			from = wp->w_botline - 1;
		if (to >= wp->w_botline)
			to = wp->w_botline - 1;

	/* find the minimal part to be updated */
		if (type == INVERTED)
		{
			while (lnum < from)						/* find start */
			{
				row += wp->w_lsize[idx++];
				++lnum;
			}
			srow = row;
			for (j = idx; j < wp->w_lsize_valid; ++j)	/* find end */
			{
				if (wp->w_lsize_lnum[j] == to + 1)
				{
					endrow = srow;
					break;
				}
				srow += wp->w_lsize[j];
			}
		}

	/* if we update the lines between from and to set old_cursor */
		if (type == INVERTED || (lnum <= from &&
								  (endrow == wp->w_height || lastline >= to)))
		{
			wp->w_old_cursor_lnum = curwin->w_cursor.lnum;
			wp->w_old_cursor_vcol = curwin->w_virtcol;
			wp->w_old_visual_lnum = VIsual.lnum;
			wp->w_old_curswant = wp->w_curswant;
		}
	}
	else
	{
		wp->w_old_cursor_lnum = 0;
		wp->w_old_visual_lnum = 0;
	}

	(void)set_highlight('v');

	/*
	 * Update the screen rows from "row" to "endrow".
	 * Start at line "lnum" which is at wp->w_lsize_lnum[idx].
	 */
	for (;;)
	{
		if (lnum > wp->w_buffer->b_ml.ml_line_count)
		{
			done = TRUE;		/* hit the end of the file */
			break;
		}
		srow = row;
		row = win_line(wp, lnum, srow, endrow);
		if (row > endrow)		/* past end of screen */
		{						/* we may need the size of that */
			wp->w_lsize[idx] = plines_win(wp, lnum);
			wp->w_lsize_lnum[idx++] = lnum;		/* too long line later on */
			break;
		}

		wp->w_lsize[idx] = row - srow;
		wp->w_lsize_lnum[idx++] = lnum;
		if (++lnum > wp->w_buffer->b_ml.ml_line_count)
		{
			done = TRUE;
			break;
		}

		if (row == endrow)
		{
			didline = TRUE;
			break;
		}
	}
	if (idx > wp->w_lsize_valid)
		wp->w_lsize_valid = idx;

	/* Do we have to do off the top of the screen processing ? */
	if (endrow != wp->w_height)
	{
		row = 0;
		for (idx = 0; idx < wp->w_lsize_valid && row < wp->w_height; idx++)
			row += wp->w_lsize[idx];

		if (row < wp->w_height)
		{
			done = TRUE;
		}
		else if (row > wp->w_height)	/* Need to blank out the last line */
		{
			lnum = wp->w_lsize_lnum[idx - 1];
			srow = row - wp->w_lsize[idx - 1];
			didline = FALSE;
		}
		else
		{
			lnum = wp->w_lsize_lnum[idx - 1] + 1;
			didline = TRUE;
		}
	}

	wp->w_empty_rows = 0;
	/*
	 * If we didn't hit the end of the file, and we didn't finish the last
	 * line we were working on, then the line didn't fit.
	 */
	if (!done && !didline)
	{
		if (lnum == wp->w_topline)
		{
			/*
			 * Single line that does not fit!
			 * Fill last line with '@' characters.
			 */
			screen_fill(wp->w_winpos + wp->w_height - 1,
					wp->w_winpos + wp->w_height, 0, (int)Columns, '@', '@');
			wp->w_botline = lnum + 1;
		}
		else
		{
			/*
			 * Clear the rest of the screen and mark the unused lines.
			 */
#ifdef RIGHTLEFT
			if (wp->w_p_rl)
				rightleft = 1;
#endif
			screen_fill(wp->w_winpos + srow,
					wp->w_winpos + wp->w_height, 0, (int)Columns, '@', ' ');
#ifdef RIGHTLEFT
			rightleft = 0;
#endif
			wp->w_botline = lnum;
			wp->w_empty_rows = wp->w_height - srow;
		}
	}
	else
	{
		/* make sure the rest of the screen is blank */
		/* put '~'s on rows that aren't part of the file. */
#ifdef RIGHTLEFT
		if (wp->w_p_rl)
			rightleft = 1;
#endif
		screen_fill(wp->w_winpos + row,
					wp->w_winpos + wp->w_height, 0, (int)Columns, '~', ' ');
#ifdef RIGHTLEFT
		rightleft = 0;
#endif
		wp->w_empty_rows = wp->w_height - row;

		if (done)				/* we hit the end of the file */
			wp->w_botline = wp->w_buffer->b_ml.ml_line_count + 1;
		else
			wp->w_botline = lnum;
	}

	wp->w_redr_type = 0;
}

/*
 * mark all status lines for redraw; used after first :cd
 */
	void
status_redraw_all()
{
	WIN		*wp;

	for (wp = firstwin; wp; wp = wp->w_next)
		wp->w_redr_status = TRUE;
	updateScreen(NOT_VALID);
}

/*
 * Redraw the status line of window wp.
 *
 * If inversion is possible we use it. Else '=' characters are used.
 */
	void
win_redr_status(wp)
	WIN		*wp;
{
	int		row;
	char_u	*p;
	int		len;
	int		fillchar;

	if (wp->w_status_height)					/* if there is a status line */
	{
		if (set_highlight('s') == OK)			/* can highlight */
		{
			fillchar = ' ';
			start_highlight();
		}
		else									/* can't highlight, use '=' */
			fillchar = '=';

		p = wp->w_buffer->b_xfilename;
		if (p == NULL)
			STRCPY(NameBuff, "[No File]");
		else
		{
			home_replace(wp->w_buffer, p, NameBuff, MAXPATHL);
			trans_characters(NameBuff, MAXPATHL);
		}
		p = NameBuff;
		len = STRLEN(p);

		if (wp->w_buffer->b_help || wp->w_buffer->b_changed ||
														 wp->w_buffer->b_p_ro)
			*(p + len++) = ' ';
		if (wp->w_buffer->b_help)
		{
			STRCPY(p + len, "[help]");
			len += 6;
		}
		if (wp->w_buffer->b_changed)
		{
			STRCPY(p + len, "[+]");
			len += 3;
		}
		if (wp->w_buffer->b_p_ro)
		{
			STRCPY(p + len, "[RO]");
			len += 4;
		}

		if (len > ru_col - 1)
		{
			p += len - (ru_col - 1);
			*p = '<';
			len = ru_col - 1;
		}

		row = wp->w_winpos + wp->w_height;
		screen_msg(p, row, 0);
		screen_fill(row, row + 1, len, ru_col, fillchar, fillchar);

		stop_highlight();
		win_redr_ruler(wp, TRUE);
	}
	else	/* no status line, can only be last window */
		redraw_cmdline = TRUE;
	wp->w_redr_status = FALSE;
}

/*
 * display line "lnum" of window 'wp' on the screen
 * Start at row "startrow", stop when "endrow" is reached.
 * Return the number of last row the line occupies.
 */

	static int
win_line(wp, lnum, startrow, endrow)
	WIN				*wp;
	linenr_t		lnum;
	int 			startrow;
	int 			endrow;
{
	char_u 			*screenp;
	int				c;
	int				col;				/* visual column on screen */
	long			vcol;				/* visual column for tabs */
	int				row;				/* row in the window, excl w_winpos */
	int				screen_row;			/* row on the screen, incl w_winpos */
	char_u			*ptr;
	char_u			extra[16];			/* "%ld" must fit in here */
	char_u			*p_extra;
	char_u			*showbreak = NULL;
	int 			n_extra;
	int				n_spaces = 0;

	int				fromcol, tocol;		/* start/end of inverting */
	int				noinvcur = FALSE;	/* don't invert the cursor */
	FPOS			*top, *bot;

	if (startrow > endrow)				/* past the end already! */
		return startrow;

	row = startrow;
	screen_row = row + wp->w_winpos;
	col = 0;
	vcol = 0;
	fromcol = -10;
	tocol = MAXCOL;
	canopt = TRUE;

	/*
	 * handle visual active in this window
	 */
	if (VIsual_active && wp->w_buffer == curwin->w_buffer)
	{
										/* Visual is after curwin->w_cursor */
		if (ltoreq(curwin->w_cursor, VIsual))
		{
			top = &curwin->w_cursor;
			bot = &VIsual;
		}
		else							/* Visual is before curwin->w_cursor */
		{
			top = &VIsual;
			bot = &curwin->w_cursor;
		}
		if (VIsual_mode == Ctrl('V'))	/* block mode */
		{
			if (lnum >= top->lnum && lnum <= bot->lnum)
			{
				colnr_t		from, to;

				getvcol(wp, top, (colnr_t *)&fromcol, NULL, (colnr_t *)&tocol);
				getvcol(wp, bot, &from, NULL, &to);
				if ((int)from < fromcol)
					fromcol = from;
				if ((int)to > tocol)
					tocol = to;
				++tocol;

				if (wp->w_curswant == MAXCOL)
					tocol = MAXCOL;
			}
		}
		else							/* non-block mode */
		{
			if (lnum > top->lnum && lnum <= bot->lnum)
				fromcol = 0;
			else if (lnum == top->lnum)
				getvcol(wp, top, (colnr_t *)&fromcol, NULL, NULL);
			if (lnum == bot->lnum)
			{
				getvcol(wp, bot, NULL, NULL, (colnr_t *)&tocol);
				++tocol;
			}

			if (VIsual_mode == 'V')		/* linewise */
			{
				if (fromcol > 0)
					fromcol = 0;
				tocol = MAXCOL;
			}
		}
			/* if the cursor can't be switched off, don't invert the
			 * character where the cursor is */
#ifndef MSDOS
		if (!highlight_match && *T_VI == NUL &&
							lnum == curwin->w_cursor.lnum && wp == curwin)
			noinvcur = TRUE;
#endif

		if (tocol <= (int)wp->w_leftcol)	/* inverting is left of screen */
			fromcol = 0;
										/* start of invert is left of screen */
		else if (fromcol >= 0 && fromcol < (int)wp->w_leftcol)
			fromcol = wp->w_leftcol;

		/* if inverting in this line, can't optimize cursor positioning */
		if (fromcol >= 0)
			canopt = FALSE;
	}
	/*
	 * handle incremental search position highlighting
	 */
	else if (highlight_match && wp == curwin && search_match_len)
	{
		if (lnum == curwin->w_cursor.lnum)
		{
			getvcol(curwin, &(curwin->w_cursor),
											(colnr_t *)&fromcol, NULL, NULL);
			curwin->w_cursor.col += search_match_len;
			getvcol(curwin, &(curwin->w_cursor),
											(colnr_t *)&tocol, NULL, NULL);
			curwin->w_cursor.col -= search_match_len;
			canopt = FALSE;
			if (fromcol == tocol)		/* do at least one character */
				tocol = fromcol + 1;	/* happens when past end of line */
		}
	}

	ptr = ml_get_buf(wp->w_buffer, lnum, FALSE);
	if (!wp->w_p_wrap)		/* advance to first character to be displayed */
	{
		while ((colnr_t)vcol < wp->w_leftcol && *ptr)
			vcol += win_chartabsize(wp, *ptr++, (colnr_t)vcol);
		if ((colnr_t)vcol > wp->w_leftcol)
		{
			n_spaces = vcol - wp->w_leftcol;	/* begin with some spaces */
			vcol = wp->w_leftcol;
		}
	}
	screenp = LinePointers[screen_row];
#ifdef RIGHTLEFT
	if (wp->w_p_rl)
	{
		col = Columns - 1;					/* col follows screenp here */
		screenp += Columns - 1;
	}
#endif
	if (wp->w_p_nu)
	{
#ifdef RIGHTLEFT
        if (wp->w_p_rl)						/* reverse line numbers */
		{
			char_u *c1, *c2, t;

			sprintf((char *)extra, " %-7ld", (long)lnum);
			for (c1 = extra, c2 = extra + STRLEN(extra) - 1; c1 < c2;
																   c1++, c2--)
			{
				t = *c1;
				*c1 = *c2;
				*c2 = t;
			}
		}
		else
#endif
			sprintf((char *)extra, "%7ld ", (long)lnum);
		p_extra = extra;
		n_extra = 8;
		vcol -= 8;		/* so vcol is 0 when line number has been printed */
	}
	else
	{
		p_extra = NULL;
		n_extra = 0;
	}
	for (;;)
	{
		if (!canopt)	/* Visual or match highlighting in this line */
		{
			if (((vcol == fromcol && !(noinvcur &&
										   (colnr_t)vcol == wp->w_virtcol)) ||
					(noinvcur && (colnr_t)vcol == wp->w_virtcol + 1 &&
							vcol >= fromcol)) && vcol < tocol)
				start_highlight();		/* start highlighting */
			else if (attributes && (vcol == tocol ||
								(noinvcur && (colnr_t)vcol == wp->w_virtcol)))
				stop_highlight();		/* stop highlighting */
		}

	/* Get the next character to put on the screen. */

		/*
		 * if 'showbreak' is set it contains the characters to put at the
		 * start of each broken line
		 */
		if (
#ifdef RIGHTLEFT
			(wp->w_p_rl ? col == -1 : col == Columns)
#else
			col == Columns
#endif
			&& (*ptr != NUL || (wp->w_p_list && n_extra == 0) ||
										(n_extra && *p_extra) || n_spaces) &&
											  vcol != 0 && STRLEN(p_sbr) != 0)
			showbreak = p_sbr;
		if (showbreak != NULL)
		{
			c = *showbreak++;
			if (*showbreak == NUL)
				showbreak = NULL;
		}
		/*
		 * The 'extra' array contains the extra stuff that is inserted to
		 * represent special characters (non-printable stuff).
		 */
		else if (n_extra)
		{
			c = *p_extra++;
			n_extra--;
		}
		else if (n_spaces)
		{
			c = ' ';
			n_spaces--;
		}
		else
		{
			c = *ptr++;
			/*
			 * Found last space before word: check for line break
			 */
			if (wp->w_p_lbr && isbreak(c) && !isbreak(*ptr) && !wp->w_p_list)
			{
				n_spaces = win_lbr_chartabsize(wp, ptr - 1,
													 (colnr_t)vcol, NULL) - 1;
				if (vim_iswhite(c))
					c = ' ';
			}
			else if (!isprintchar(c))
			{
				/*
				 * when getting a character from the file, we may have to turn
				 * it into something else on the way to putting it into
				 * 'NextScreen'.
				 */
				if (c == TAB && !wp->w_p_list)
				{
					/* tab amount depends on current column */
					n_spaces = (int)wp->w_buffer->b_p_ts -
									vcol % (int)wp->w_buffer->b_p_ts - 1;
					c = ' ';
				}
				else if (c == NUL && wp->w_p_list)
				{
					p_extra = (char_u *)"";
					n_extra = 1;
					c = '$';
					--ptr;			/* put it back at the NUL */
				}
				else if (c != NUL)
				{
					p_extra = transchar(c);
					n_extra = charsize(c) - 1;
					c = *p_extra++;
				}
			}
		}

		if (c == NUL)
		{
			if (attributes)
			{
				/* invert at least one char, used for Visual and empty line or
				 * highlight match at end of line. If it's beyond the last
				 * char on the screen, just overwrite that one (tricky!) */
				if (vcol == fromcol)
				{
#ifdef RIGHTLEFT
					if (wp->w_p_rl)
					{
						if (col < 0)
						{
							++screenp;
							++col;
						}
					}
					else
#endif
					{
						if (col >= Columns)
						{
							--screenp;
							--col;
						}
					}
					if (*screenp != ' ' || *(screenp + Columns) != attributes)
					{
							*screenp = ' ';
							*(screenp + Columns) = attributes;
							screen_char(screenp, screen_row, col);
					}
#ifdef RIGHTLEFT
					if (wp->w_p_rl)
					{
						--screenp;
						--col;
					}
					else
#endif
					{
						++screenp;
						++col;
					}
				}
				stop_highlight();
			}
			/* 
			 * blank out the rest of this row, if necessary
			 */
#ifdef RIGHTLEFT
			if (wp->w_p_rl)
			{
				while (col >= 0 && *screenp == ' ' &&
													*(screenp + Columns) == 0)
				{
					--screenp;
					--col;
				}
				if (col >= 0)
					screen_fill(screen_row, screen_row + 1,
														0, col + 1, ' ', ' ');
			}
			else
#endif
			{
				while (col < Columns && *screenp == ' ' &&
													*(screenp + Columns) == 0)
				{
					++screenp;
					++col;
				}
				if (col < Columns)
					screen_fill(screen_row, screen_row + 1,
												col, (int)Columns, ' ', ' ');
			}
			row++;
			break;
		}
		if (
#ifdef RIGHTLEFT
			wp->w_p_rl ? (col < 0) : 
#endif
									(col >= Columns)
													)
		{
			col = 0;
			++row;
			++screen_row;
			if (!wp->w_p_wrap)
				break;
			if (row == endrow)		/* line got too long for screen */
			{
				++row;
				break;
			}
			screenp = LinePointers[screen_row];
#ifdef RIGHTLEFT
			if (wp->w_p_rl)
			{
				col = Columns - 1;		/* col is not used if breaking! */
				screenp += Columns - 1;
			}
#endif
		}

		/*
		 * Store the character in NextScreen.
		 */
		if (*screenp != c || *(screenp + Columns) != attributes)
		{
			/*
			 * Special trick to make copy/paste of wrapped lines work with
			 * xterm/screen:
			 *   If the first column is to be written, write the preceding
			 *   char twice.  This will work with all terminal types
			 *   (regardless of the xn,am settings).
			 * Only do this on a fast tty.
			 */
			if (p_tf && row > startrow && col == 0 &&
					LinePointers[screen_row - 1][Columns - 1 + Columns] ==
						attributes)
			{
				if (screen_cur_row != screen_row - 1 ||
													screen_cur_col != Columns)
					screen_char(LinePointers[screen_row - 1] + Columns - 1,
										  screen_row - 1, (int)(Columns - 1));
				screen_char(LinePointers[screen_row - 1] + Columns - 1,
												screen_row - 1, (int)Columns);	
				screen_start();
			}

			*screenp = c;
			*(screenp + Columns) = attributes;
			screen_char(screenp, screen_row, col);
		}
#ifdef RIGHTLEFT
		if (wp->w_p_rl)
		{
			--screenp;
			--col;
		}
		else
#endif
		{
			++screenp;
			++col;
		}
		++vcol;
			/* stop before '$' of change command */
		if (wp == curwin && dollar_vcol && vcol >= (long)wp->w_virtcol)
			break;
	}

	stop_highlight();
	return (row);
}

/*
 * Called when p_dollar is set: display a '$' at the end of the changed text
 * Only works when cursor is in the line that changes.
 */
	void
display_dollar(col)
	colnr_t		col;
{
	colnr_t	save_col;

	if (RedrawingDisabled)
		return;

	cursor_off();
	save_col = curwin->w_cursor.col;
	curwin->w_cursor.col = col;
	curs_columns(FALSE);
	if (!curwin->w_p_wrap)
		curwin->w_col -= curwin->w_leftcol;
	if (curwin->w_col < Columns)
	{
		screen_msg((char_u *)"$", curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
				curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
															   curwin->w_col);
		dollar_vcol = curwin->w_virtcol;
	}
	curwin->w_cursor.col = save_col;
}

/*
 * Call this function before moving the cursor from the normal insert position
 * in insert mode.
 */
	void
undisplay_dollar()
{
	if (dollar_vcol)
	{
		dollar_vcol = 0;
		updateline();
	}
}

/*
 * output a single character directly to the screen
 * update NextScreen
 */
	void
screen_outchar(c, row, col)
	int		c;
	int		row, col;
{
	char_u		buf[2];

	buf[0] = c;
	buf[1] = NUL;
	screen_msg(buf, row, col);
}
	
/*
 * put string '*text' on the screen at position 'row' and 'col'
 * update NextScreen
 * Note: only outputs within one row, message is truncated at screen boundary!
 * Note: if NextScreen, row and/or col is invalid, nothing is done.
 */
	void
screen_msg(text, row, col)
	char_u	*text;
	int		row;
	int		col;
{
	char_u	*screenp;

	if (NextScreen != NULL && row < Rows)			/* safety check */
	{
		screenp = LinePointers[row] + col;
		while (*text && col < Columns)
		{
			if (*screenp != *text || *(screenp + Columns) != attributes)
			{
				*screenp = *text;
				*(screenp + Columns) = attributes;
				screen_char(screenp, row, col);
			}
			++screenp;
			++col;
			++text;
		}
	}
}

/*
 * Reset cursor position. Use whenever cursor was moved because of outputting
 * something directly to the screen (shell commands) or a terminal control
 * code.
 */
	void
screen_start()
{
	screen_cur_row = screen_cur_col = 9999;
}

/*
 * set_highlight - set highlight depending on 'highlight' option and context.
 *
 * return FAIL if highlighting is not possible, OK otherwise
 */
	int
set_highlight(context)
	int		context;
{
	int		i;
	int		mode;
	char_u	*p;

	/*
	 * Try to find the mode in the 'highlight' option.
	 * If not found, try the default for the 'highlight' option.
	 * If still not found, use 'r' (should not happen).
	 */
	mode = 'r';
	for (i = 0; i < 2; ++i)
	{
		if (i)
			p = get_highlight_default();
		else
			p = p_hl;
		if (p == NULL)
			continue;

		while (*p)
		{
			if (*p == context)				/* found what we are looking for */
				break;
			while (*p && *p != ',')			/* skip to comma */
				++p;
			p = skip_to_option_part(p);		/* skip comma and spaces */
		}
		if (p[0] && p[1])
		{
			mode = p[1];
			break;
		}
	}

	switch (mode)
	{
		case 'b':	highlight = T_MD;		/* bold */
					unhighlight = T_ME;
					highlight_attr = CHAR_BOLD;
					break;
		case 's':	highlight = T_SO;		/* standout */
					unhighlight = T_SE;
					highlight_attr = CHAR_STDOUT;
					break;
		case 'n':	highlight = NULL;		/* no highlighting */
					unhighlight = NULL;
					highlight_attr = 0;
					break;
		case 'u':	highlight = T_US;		/* underline */
					unhighlight = T_UE;
					highlight_attr = CHAR_UNDERL;
					break;
		case 'i':	highlight = T_CZH;		/* italic */
					unhighlight = T_CZR;
					highlight_attr = CHAR_ITALIC;
					break;
		default:	highlight = T_MR;		/* reverse (invert) */
					unhighlight = T_ME;
					highlight_attr = CHAR_INVERT;
					break;
	}
	if (highlight == NULL || *highlight == NUL ||
						unhighlight == NULL || *unhighlight == NUL)
	{
		highlight = NULL;
		return FAIL;
	}
	return OK;
}

	void
start_highlight()
{
	if (full_screen &&
#ifdef WIN32
						termcap_active &&
#endif
											highlight != NULL)
	{
		outstr(highlight);
		attributes = highlight_attr;
	}
}

	void
stop_highlight()
{
	if (attributes)
	{
		outstr(unhighlight);
		attributes = 0;
	}
}

/*
 * variables used for one level depth of highlighting
 * Used for "-- More --" message.
 */

static char_u	*old_highlight = NULL;
static char_u	*old_unhighlight = NULL;
static int		old_highlight_attr = 0;

	void
remember_highlight()
{
	old_highlight = highlight;
	old_unhighlight = unhighlight;
	old_highlight_attr = highlight_attr;
}

	void
recover_old_highlight()
{
	highlight = old_highlight;
	unhighlight = old_unhighlight;
	highlight_attr = old_highlight_attr;
}

/*
 * put character '*p' on the screen at position 'row' and 'col'
 */
	static void
screen_char(p, row, col)
	char_u	*p;
	int 	row;
	int 	col;
{
	int			c;
	int			noinvcurs;

	/*
	 * Outputting the last character on the screen may scrollup the screen.
	 * Don't to it!
	 */
	if (col == Columns - 1 && row == Rows - 1)
		return;
	if (screen_cur_col != col || screen_cur_row != row)
	{
		/* check if no cursor movement is allowed in standout mode */
		if (attributes && !p_wiv && *T_MS == NUL)
			noinvcurs = 7;
		else
			noinvcurs = 0;

		/*
		 * If we're on the same row (which happens a lot!), try to
		 * avoid a windgoto().
		 * If we are only a few characters off, output the
		 * characters. That is faster than cursor positioning.
		 * This can't be used when switching between inverting and not
		 * inverting.
		 */
		if (screen_cur_row == row && screen_cur_col < col)
		{
			register int i;

			i = col - screen_cur_col;
			if (i <= 4 + noinvcurs)
			{
				/* stop at the first character that has different attributes
				 * from the ones that are active */
				while (i && *(p - i + Columns) == attributes)
				{
					c = *(p - i--);
					outchar(c);
				}
			}
			if (i)
			{
				if (noinvcurs)
					stop_highlight();
			
				if (*T_CRI != NUL)	/* use tgoto interface! jw */
					OUTSTR(tgoto((char *)T_CRI, 0, i));
				else
					windgoto(row, col);
			
				if (noinvcurs)
					start_highlight();
			}
		}
		/*
		 * If the cursor is at the line above where we want to be, use CR LF,
		 * this is quicker than windgoto().
		 * Don't do this if the cursor went beyond the last column, the cursor
		 * position is unknown then (some terminals wrap, some don't )
		 */
		else if (screen_cur_row + 1 == row && col == 0 &&
													 screen_cur_col < Columns)
		{
			if (noinvcurs)
				stop_highlight();
			outchar('\n');
			if (noinvcurs)
				start_highlight();
		}
		else
		{
			if (noinvcurs)
				stop_highlight();
			windgoto(row, col);
			if (noinvcurs)
				start_highlight();
		}
		screen_cur_row = row;
		screen_cur_col = col;
	}

	/*
	 * For weird invert mechanism: output (un)highlight before every char
	 * Lots of extra output, but works.
	 */
	if (p_wiv)
	{
		if (attributes)                                      
			outstr(highlight);                            
		else if (full_screen)                                            
			outstr(unhighlight);
	}
	outchar(*p);
	screen_cur_col++;
}

/*
 * Fill the screen from 'start_row' to 'end_row', from 'start_col' to 'end_col'
 * with character 'c1' in first column followed by 'c2' in the other columns.
 */
	void
screen_fill(start_row, end_row, start_col, end_col, c1, c2)
	int 	start_row, end_row;
	int		start_col, end_col;
	int		c1, c2;
{
	int				row;
	int				col;
	char_u			*screenp;
	char_u			*attrp;
	int				did_delete;
	int				c;

	if (end_row > Rows)				/* safety check */
		end_row = Rows;
	if (end_col > Columns)			/* safety check */
		end_col = Columns;
	if (NextScreen == NULL ||
			start_row >= end_row || start_col >= end_col)	/* nothing to do */
		return;

	for (row = start_row; row < end_row; ++row)
	{
			/* try to use delete-line termcap code */
		did_delete = FALSE;
		if (attributes == 0 && c2 == ' ' && end_col == Columns && *T_CE != NUL
#ifdef RIGHTLEFT
					&& !rightleft
#endif
									)
		{
			/*
			 * check if we really need to clear something
			 */
			col = start_col;
			screenp = LinePointers[row] + start_col;
			if (c1 != ' ')						/* don't clear first char */
			{
				++col;
				++screenp;
			}

			/* skip blanks (used often, keep it fast!) */
			attrp = screenp + Columns;
			while (col < end_col && *screenp == ' ' && *attrp == 0)
			{
				++col;
				++screenp;
				++attrp;
			}
			if (col < end_col)			/* something to be cleared */
			{
				windgoto(row, col);		/* clear rest of this screen line */
				outstr(T_CE);
				screen_start();			/* don't know where cursor is now */
				col = end_col - col;
				while (col--)			/* clear chars in NextScreen */
				{
					*attrp++ = 0;
					*screenp++ = ' ';
				}
			}
			did_delete = TRUE;			/* the chars are cleared now */
		}

		screenp = LinePointers[row] +
#ifdef RIGHTLEFT
			(rightleft ? (int)Columns - 1 - start_col : start_col);
#else
													start_col;
#endif
		c = c1;
		for (col = start_col; col < end_col; ++col)
		{
			if (*screenp != c || *(screenp + Columns) != attributes)
			{
				*screenp = c;
				*(screenp + Columns) = attributes;
				if (!did_delete || c != ' ')
					screen_char(screenp, row,
#ifdef RIGHTLEFT
							rightleft ? Columns - 1 - col :
#endif
															col);
			}
#ifdef RIGHTLEFT
			if (rightleft)
				--screenp;
			else
#endif
				++screenp;
			if (col == start_col)
			{
				if (did_delete)
					break;
				c = c2;
			}
		}
		if (row == Rows - 1)			/* overwritten the command line */
		{
			redraw_cmdline = TRUE;
			if (c1 == ' ' && c2 == ' ')
				clear_cmdline = FALSE;	/* command line has been cleared */
		}
	}
}

/*
 * recompute all w_botline's. Called after Rows changed.
 */
	void
comp_Botline_all()
{
	WIN		*wp;

	for (wp = firstwin; wp; wp = wp->w_next)
		comp_Botline(wp);
}

/*
 * compute wp->w_botline. Can be called after wp->w_topline changed.
 */
	void
comp_Botline(wp)
	WIN			*wp;
{
	comp_Botline_sub(wp, wp->w_topline, 0);
}

/*
 * Compute wp->w_botline, may have a start at the cursor position.
 * Code shared between comp_Botline() and cursupdate().
 */
	static void
comp_Botline_sub(wp, lnum, done)
	WIN			*wp;
	linenr_t	lnum;
	int			done;
{
	int			n;

	for ( ; lnum <= wp->w_buffer->b_ml.ml_line_count; ++lnum)
	{
		n = plines_win(wp, lnum);
		if (done + n > wp->w_height)
			break;
		done += n;
	}

	/* wp->w_botline is the line that is just below the window */
	wp->w_botline = lnum;

	/* Also set wp->w_empty_rows, otherwise scroll_cursor_bot() won't work */
	if (done == 0)
		wp->w_empty_rows = 0;		/* single line that doesn't fit */
	else
		wp->w_empty_rows = wp->w_height - done;
}

	void
screenalloc(clear)
	int		clear;
{
	register int	new_row, old_row;
	WIN				*wp;
	int				outofmem = FALSE;
	int				len;
	char_u			*new_NextScreen;
	char_u			**new_LinePointers;

	/*
	 * Allocation of the screen buffers is done only when the size changes
	 * and when Rows and Columns have been set and we are doing full screen
	 * stuff.
	 */
	if ((NextScreen != NULL && Rows == screen_Rows && Columns == screen_Columns)
							|| Rows == 0 || Columns == 0 || !full_screen)
		return;

	comp_col();			/* recompute columns for shown command and ruler */

	/*
	 * We're changing the size of the screen.
	 * - Allocate new arrays for NextScreen.
	 * - Move lines from the old arrays into the new arrays, clear extra
	 *   lines (unless the screen is going to be cleared).
	 * - Free the old arrays.
	 */
	for (wp = firstwin; wp; wp = wp->w_next)
		win_free_lsize(wp);

	new_NextScreen = (char_u *)malloc((size_t) (Rows * Columns * 2));
	new_LinePointers = (char_u **)malloc(sizeof(char_u *) * Rows);

	for (wp = firstwin; wp; wp = wp->w_next)
	{
		if (win_alloc_lsize(wp) == FAIL)
		{
			outofmem = TRUE;
			break;
		}
	}

	if (new_NextScreen == NULL || new_LinePointers == NULL || outofmem)
	{
		do_outofmem_msg();
		vim_free(new_NextScreen);
		new_NextScreen = NULL;
	}
	else
	{
		for (new_row = 0; new_row < Rows; ++new_row)
		{
			new_LinePointers[new_row] = new_NextScreen + new_row * Columns * 2;

			/*
			 * If the screen is not going to be cleared, copy as much as
			 * possible from the old screen to the new one and clear the rest
			 * (used when resizing the window at the "--more--" prompt or when
			 * executing an external command, for the GUI).
			 */
			if (!clear)
			{
				lineclear(new_LinePointers[new_row]);
				old_row = new_row + (screen_Rows - Rows);
				if (old_row >= 0)
				{
					if (screen_Columns < Columns)
						len = screen_Columns;
					else
						len = Columns;
					vim_memmove(new_LinePointers[new_row],
							LinePointers[old_row], (size_t)len);
					vim_memmove(new_LinePointers[new_row] + Columns,
							LinePointers[old_row] + screen_Columns, (size_t)len);
				}
			}
		}
	}

	vim_free(NextScreen);
	vim_free(LinePointers);
	NextScreen = new_NextScreen;
	LinePointers = new_LinePointers;

	must_redraw = CLEAR;		/* need to clear the screen later */
	if (clear)
		screenclear2();

#ifdef USE_GUI
	else if (gui.in_use && NextScreen != NULL && Rows != screen_Rows)
	{
		gui_redraw_block(0, 0, Rows - 1, Columns - 1);
		/*
		 * Adjust the position of the cursor, for when executing an external
		 * command.
		 */
		if (msg_row >= Rows)				/* Rows got smaller */
			msg_row = Rows - 1;				/* put cursor at last row */
		else if (Rows > screen_Rows)		/* Rows got bigger */
			msg_row += Rows - screen_Rows;	/* put cursor in same place */
		if (msg_col >= Columns)				/* Columns got smaller */
			msg_col = Columns - 1;			/* put cursor at last column */
	}
#endif

	screen_Rows = Rows;
	screen_Columns = Columns;
}

	void
screenclear()
{
	if (emsg_on_display)
	{
		mch_delay(1000L, TRUE);
		emsg_on_display = FALSE;
	}
	screenalloc(FALSE);		/* allocate screen buffers if size changed */
	screenclear2();			/* clear the screen */
}

	static void
screenclear2()
{
	int		i;

	if (starting || NextScreen == NULL)
		return;

	outstr(T_CL);				/* clear the display */

								/* blank out NextScreen */
	for (i = 0; i < Rows; ++i)
		lineclear(LinePointers[i]);

	screen_cleared = TRUE;			/* can use contents of NextScreen now */

	win_rest_invalid(firstwin);
	clear_cmdline = FALSE;
	redraw_cmdline = TRUE;
	if (must_redraw == CLEAR)		/* no need to clear again */
		must_redraw = NOT_VALID;
	compute_cmdrow();
	msg_pos((int)Rows - 1, 0);		/* put cursor on last line for messages */
	screen_start();					/* don't know where cursor is now */
	msg_scrolled = 0;				/* can't scroll back */
	msg_didany = FALSE;
	msg_didout = FALSE;
}

/*
 * Clear one line in NextScreen.
 */
	static void
lineclear(p)
	char_u	*p;
{
	(void)vim_memset(p, ' ', (size_t)Columns);
	(void)vim_memset(p + Columns, 0, (size_t)Columns);
}

/*
 * check cursor for a valid lnum
 */
	void
check_cursor()
{
	if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
	if (curwin->w_cursor.lnum <= 0)
		curwin->w_cursor.lnum = 1;
}

	void
cursupdate()
{
	linenr_t		lnum;
	long 			line_count;
	int 			sline;
	int				done;
	int 			temp;

	if (!screen_valid(TRUE))
		return;

	/*
	 * Make sure the cursor is on a valid line number
	 */
	check_cursor();

	/*
	 * If the buffer is empty, always set topline to 1.
	 */
	if (bufempty()) 			/* special case - file is empty */
	{
		curwin->w_topline = 1;
		curwin->w_cursor.lnum = 1;
		curwin->w_cursor.col = 0;
		curwin->w_lsize[0] = 0;
		if (curwin->w_lsize_valid == 0)	/* don't know about screen contents */
			updateScreen(NOT_VALID);
		curwin->w_lsize_valid = 1;
	}

	/*
	 * If the cursor is above the top of the window, scroll the window to put
	 * it at the top of the window.
	 * If we weren't very close to begin with, we scroll to put the cursor in
	 * the middle of the window.
	 */
	else if (curwin->w_cursor.lnum < curwin->w_topline + p_so &&
														curwin->w_topline > 1)
	{
		temp = curwin->w_height / 2 - 1;
		if (temp < 2)
			temp = 2;
								/* not very close, put cursor halfway screen */
		if (curwin->w_topline + p_so - curwin->w_cursor.lnum >= temp)
			scroll_cursor_halfway(FALSE);
		else
			scroll_cursor_top((int)p_sj, FALSE);
		updateScreen(VALID);
	}

	/*
	 * If the cursor is below the bottom of the window, scroll the window
	 * to put the cursor on the window. If the cursor is less than a
	 * windowheight down compute the number of lines at the top which have
	 * the same or more rows than the rows of the lines below the bottom.
	 * Note: After curwin->w_botline was computed lines may have been
	 * added or deleted, it may be greater than ml_line_count.
	 */
	else if ((long)curwin->w_cursor.lnum >= (long)curwin->w_botline - p_so &&
								curwin->w_botline <= curbuf->b_ml.ml_line_count)
	{
		line_count = curwin->w_cursor.lnum - curwin->w_botline + 1 + p_so;
		if (line_count <= curwin->w_height + 1)
			scroll_cursor_bot((int)p_sj, FALSE);
		else
			scroll_cursor_halfway(FALSE);
		updateScreen(VALID);
	}

	/*
	 * If the window contents is unknown, need to update the screen.
	 */
	else if (curwin->w_lsize_valid == 0)
		updateScreen(NOT_VALID);

	/*
	 * Figure out the row number of the cursor line.
	 * This is used often, keep it fast!
	 */
	curwin->w_row = sline = 0;
											/* curwin->w_lsize[] invalid */
	if (RedrawingDisabled || curwin->w_lsize_valid == 0)
	{
		done = 0;
		for (lnum = curwin->w_topline; lnum != curwin->w_cursor.lnum; ++lnum)
			done += plines(lnum);
		curwin->w_row = done;

		/*
		 * Also need to compute w_botline and w_empty_rows, because
		 * updateScreen() will not have done that.
		 */
		comp_Botline_sub(curwin, lnum, done);
	}
	else
	{
		for (done = curwin->w_cursor.lnum - curwin->w_topline; done > 0; --done)
			curwin->w_row += curwin->w_lsize[sline++];
	}

	curwin->w_cline_row = curwin->w_row;
	curwin->w_col = curwin->w_virtcol = 0;
	if (!RedrawingDisabled && sline > curwin->w_lsize_valid)
								/* Should only happen with a line that is too */
								/* long to fit on the last screen line. */
		curwin->w_cline_height = 0;
	else
	{
							/* curwin->w_lsize[] invalid */
		if (RedrawingDisabled || curwin->w_lsize_valid == 0)
		    curwin->w_cline_height = plines(curwin->w_cursor.lnum);
        else
			curwin->w_cline_height = curwin->w_lsize[sline];
							/* compute curwin->w_virtcol and curwin->w_col */
		curs_columns(!RedrawingDisabled);
		if (must_redraw)
			updateScreen(must_redraw);
	}

	if (curwin->w_set_curswant)
	{
		curwin->w_curswant = curwin->w_virtcol;
		curwin->w_set_curswant = FALSE;
	}
}

/*
 * Recompute topline to put the cursor at the top of the window.
 * Scroll at least "min_scroll" lines.
 * If "always" is TRUE, always set topline (for "zt").
 */
	void
scroll_cursor_top(min_scroll, always)
	int		min_scroll;
	int		always;
{
	int		scrolled = 0;
	int		extra = 0;
	int		used;
	int		i;
	int		sline;		/* screen line for cursor */

	/*
	 * Decrease topline until:
	 * - it has become 1
	 * - (part of) the cursor line is moved off the screen or
	 * - moved at least 'scrolljump' lines and
	 * - at least 'scrolloff' lines above and below the cursor
	 */
	used = plines(curwin->w_cursor.lnum);
	for (sline = 1; sline < curwin->w_cursor.lnum; ++sline)
	{
		i = plines(curwin->w_cursor.lnum - sline);
		used += i;
		extra += i;
		if (extra <= p_so &&
				   curwin->w_cursor.lnum + sline < curbuf->b_ml.ml_line_count)
			used += plines(curwin->w_cursor.lnum + sline);
		if (used > curwin->w_height)
			break;
		if (curwin->w_cursor.lnum - sline < curwin->w_topline)
			scrolled += i;

		/*
		 * If scrolling is needed, scroll at least 'sj' lines.
		 */
		if ((curwin->w_cursor.lnum - (sline - 1) >= curwin->w_topline ||
									  scrolled >= min_scroll) && extra > p_so)
			break;
	}

	/*
	 * If we don't have enough space, put cursor in the middle.
	 * This makes sure we get the same position when using "k" and "j"
	 * in a small window.
	 */
	if (used > curwin->w_height)
		scroll_cursor_halfway(FALSE);
	else
	{
		/*
		 * If "always" is FALSE, only adjust topline to a lower value, higher
		 * value may happen with wrapping lines
		 */
		if (curwin->w_cursor.lnum - (sline - 1) < curwin->w_topline || always)
			curwin->w_topline = curwin->w_cursor.lnum - (sline - 1);
		if (curwin->w_topline > curwin->w_cursor.lnum)
			curwin->w_topline = curwin->w_cursor.lnum;
	}
}

/*
 * Recompute topline to put the cursor at the bottom of the window.
 * Scroll at least "min_scroll" lines.
 * If "set_topline" is TRUE, set topline and botline first (for "zb").
 * This is messy stuff!!!
 */
	void
scroll_cursor_bot(min_scroll, set_topline)
	int		min_scroll;
	int		set_topline;
{
	int			used;
	int			scrolled = 0;
	int			extra = 0;
	int			sline;			/* screen line for cursor from bottom */
	int			i;
	linenr_t	lnum;
	linenr_t	line_count;
	linenr_t	old_topline = curwin->w_topline;
	linenr_t	old_botline = curwin->w_botline;
	int			old_empty_rows = curwin->w_empty_rows;
	linenr_t	cln;				/* Cursor Line Number */

	cln = curwin->w_cursor.lnum;
	if (set_topline)
	{
		used = 0;
		curwin->w_botline = cln + 1;
		for (curwin->w_topline = curwin->w_botline;
				curwin->w_topline != 1;
				--curwin->w_topline)
		{
			i = plines(curwin->w_topline - 1);
			if (used + i > curwin->w_height)
				break;
			used += i;
		}
		curwin->w_empty_rows = curwin->w_height - used;
	}

	used = plines(cln);
	if (cln >= curwin->w_botline)
	{
		scrolled = used;
		if (cln == curwin->w_botline)
			scrolled -= curwin->w_empty_rows;
	}

	/*
	 * Stop counting lines to scroll when
	 * - hitting start of the file
	 * - scrolled nothing or at least 'sj' lines
	 * - at least 'so' lines below the cursor
	 * - lines between botline and cursor have been counted
	 */
	for (sline = 1; sline < cln; ++sline)
	{
		if ((((scrolled <= 0 || scrolled >= min_scroll) && extra >= p_so) ||
				cln + sline > curbuf->b_ml.ml_line_count) &&
				cln - sline < curwin->w_botline)
			break;
		i = plines(cln - sline);
		used += i;
		if (used > curwin->w_height)
			break;
		if (cln - sline >= curwin->w_botline)
		{
			scrolled += i;
			if (cln - sline == curwin->w_botline)
				scrolled -= curwin->w_empty_rows;
		}
		if (cln + sline <= curbuf->b_ml.ml_line_count)
		{
			i = plines(cln + sline);
			used += i;
			if (used > curwin->w_height)
				break;
			if (extra < p_so || scrolled < min_scroll)
			{
				extra += i;
				if (cln + sline >= curwin->w_botline)
				{
					scrolled += i;
					if (cln + sline == curwin->w_botline)
						scrolled -= curwin->w_empty_rows;
				}
			}
		}
	}
	/* curwin->w_empty_rows is larger, no need to scroll */
	if (scrolled <= 0)
		line_count = 0;
	/* more than a screenfull, don't scroll but redraw */
	else if (used > curwin->w_height)
		line_count = used;
	/* scroll minimal number of lines */
	else
	{
		for (i = 0, lnum = curwin->w_topline;
				i < scrolled && lnum < curwin->w_botline; ++lnum)
			i += plines(lnum);
		if (i >= scrolled)		/* it's possible to scroll */
			line_count = lnum - curwin->w_topline;
		else				/* below curwin->w_botline, don't scroll */
			line_count = 9999;
	}

	/*
	 * Scroll up if the cursor is off the bottom of the screen a bit.
	 * Otherwise put it at 1/2 of the screen.
	 */
	if (line_count >= curwin->w_height && line_count > min_scroll)
		scroll_cursor_halfway(FALSE);
	else
		scrollup(line_count);

	/*
	 * If topline didn't change we need to restore w_botline and w_empty_rows
	 * (we changed them).
	 * If topline did change, updateScreen() will set botline.
	 */
	if (curwin->w_topline == old_topline && set_topline)
	{
		curwin->w_botline = old_botline;
		curwin->w_empty_rows = old_empty_rows;
	}
}

/*
 * Recompute topline to put the cursor halfway the window
 * If "atend" is TRUE, also put it halfway at the end of the file.
 */
	void
scroll_cursor_halfway(atend)
	int		atend;
{
	int			above = 0;
	linenr_t	topline;
	int			below = 0;
	linenr_t	botline;
	int			used;
	int			i;
	linenr_t	cln;				/* Cursor Line Number */

	topline = botline = cln = curwin->w_cursor.lnum;
	used = plines(cln);
	while (topline > 1)
	{
		if (below <= above)			/* add a line below the cursor */
		{
			if (botline + 1 <= curbuf->b_ml.ml_line_count)
			{
				i = plines(botline + 1);
				used += i;
				if (used > curwin->w_height)
					break;
				below += i;
				++botline;
			}
			else
			{
				++below;			/* count a "~" line */
				if (atend)
					++used;
			}
		}

		if (below > above)			/* add a line above the cursor */
		{
			i = plines(topline - 1);
			used += i;
			if (used > curwin->w_height)
				break;
			above += i;
			--topline;
		}
	}
	curwin->w_topline = topline;
}

/*
 * Correct the cursor position so that it is in a part of the screen at least
 * 'so' lines from the top and bottom, if possible.
 * If not possible, put it at the same position as scroll_cursor_halfway().
 * When called topline and botline must be valid!
 */
	void
cursor_correct()
{
	int			above = 0;			/* screen lines above topline */
	linenr_t	topline;
	int			below = 0;			/* screen lines below botline */
	linenr_t	botline;
	int			above_wanted, below_wanted;
	linenr_t	cln;				/* Cursor Line Number */
	int			max_off;

	/*
	 * How many lines we would like to have above/below the cursor depends on
	 * whether the first/last line of the file is on screen.
	 */
	above_wanted = p_so;
	below_wanted = p_so;
	if (curwin->w_topline == 1)
	{
		above_wanted = 0;
		max_off = curwin->w_height / 2;
		if (below_wanted > max_off)
			below_wanted = max_off;
	}
	if (curwin->w_botline == curbuf->b_ml.ml_line_count + 1)
	{
		below_wanted = 0;
		max_off = (curwin->w_height - 1) / 2;
		if (above_wanted > max_off)
			above_wanted = max_off;
	}

	/*
	 * If there are sufficient file-lines above and below the cursor, we can
	 * return now.
	 */
	cln = curwin->w_cursor.lnum;
	if (cln >= curwin->w_topline + above_wanted && 
									  cln < curwin->w_botline - below_wanted)
		return;

	/*
	 * Narrow down the area where the cursor can be put by taking lines from
	 * the top and the bottom until:
	 * - the desired context lines are found
	 * - the lines from the top is past the lines from the bottom
	 */
	topline = curwin->w_topline;
	botline = curwin->w_botline - 1;
	while ((above < above_wanted || below < below_wanted) && topline < botline)
	{
		if (below < below_wanted && (below <= above || above >= above_wanted))
		{
			below += plines(botline);
			--botline;
		}
		if (above < above_wanted && (above < below || below >= below_wanted))
		{
			above += plines(topline);
			++topline;
		}
	}
	if (topline == botline || botline == 0)
		curwin->w_cursor.lnum = topline;
	else if (topline > botline)
		curwin->w_cursor.lnum = botline;
	else
	{
		if (cln < topline && curwin->w_topline > 1)
			curwin->w_cursor.lnum = topline;
		if (cln > botline && curwin->w_botline <= curbuf->b_ml.ml_line_count)
			curwin->w_cursor.lnum = botline;
	}
}

/*
 * Compute curwin->w_row.
 * Can be called when topline and botline have not been updated.
 * return OK when cursor is in the window, FAIL when it isn't.
 */
	int
curs_rows()
{
	linenr_t	lnum;
	int			i;

	if (curwin->w_cursor.lnum < curwin->w_topline ||
						 curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count ||
								   curwin->w_cursor.lnum >= curwin->w_botline)
		return FAIL;

	curwin->w_row = i = 0;
	for (lnum = curwin->w_topline; lnum != curwin->w_cursor.lnum; ++lnum)
		if (RedrawingDisabled)		/* curwin->w_lsize[] invalid */
			curwin->w_row += plines(lnum);
		else
			curwin->w_row += curwin->w_lsize[i++];
	return OK;
}

/*
 * compute curwin->w_col and curwin->w_virtcol
 */
	void
curs_columns(scroll)
	int scroll;			/* when TRUE, may scroll horizontally */
{
	int		diff;
	int		extra;
	int		new_leftcol;
	colnr_t	startcol;
	colnr_t endcol;

	getvcol(curwin, &curwin->w_cursor,
								&startcol, &(curwin->w_virtcol), &endcol);

		/* remove '$' from change command when cursor moves onto it */
	if (startcol > dollar_vcol)
		dollar_vcol = 0;

	curwin->w_col = curwin->w_virtcol;
	if (curwin->w_p_nu)
	{
		curwin->w_col += 8;
		endcol += 8;
	}

	curwin->w_row = curwin->w_cline_row;
	if (curwin->w_p_wrap)		/* long line wrapping, adjust curwin->w_row */
	{
		while (curwin->w_col >= Columns)
		{
			curwin->w_col -= Columns;
			curwin->w_row++;
		}
	}
	else if (scroll)	/* no line wrapping, compute curwin->w_leftcol if
						 * scrolling is on.  If scrolling is off,
						 * curwin->w_leftcol is assumed to be 0 */
	{
						/* If Cursor is left of the screen, scroll rightwards */
						/* If Cursor is right of the screen, scroll leftwards */
		if ((extra = (int)startcol - (int)curwin->w_leftcol) < 0 ||
			 (extra = (int)endcol - (int)(curwin->w_leftcol + Columns) + 1) > 0)
		{
			if (extra < 0)
				diff = -extra;
			else
				diff = extra;

				/* far off, put cursor in middle of window */
			if (p_ss == 0 || diff >= Columns / 2)
				new_leftcol = curwin->w_col - Columns / 2;
			else
			{
				if (diff < p_ss)
					diff = p_ss;
				if (extra < 0)
					new_leftcol = curwin->w_leftcol - diff;
				else
					new_leftcol = curwin->w_leftcol + diff;
			}
			if (new_leftcol < 0)
				curwin->w_leftcol = 0;
			else
				curwin->w_leftcol = new_leftcol;
					/* screen has to be redrawn with new curwin->w_leftcol */
			redraw_later(NOT_VALID);
		}
		curwin->w_col -= curwin->w_leftcol;
	}
		/* Cursor past end of screen */
		/* happens with line that does not fit on screen */
	if (curwin->w_row > curwin->w_height - 1)
		curwin->w_row = curwin->w_height - 1;
}

	void
scrolldown(line_count)
	long	line_count;
{
	register long	done = 0;	/* total # of physical lines done */

	/* Scroll up 'line_count' lines. */
	while (line_count--)
	{
		if (curwin->w_topline == 1)
			break;
		done += plines(--curwin->w_topline);
	}
	/*
	 * Compute the row number of the last row of the cursor line
	 * and move it onto the screen.
	 */
	curwin->w_row += done;
	if (curwin->w_p_wrap)
		curwin->w_row += plines(curwin->w_cursor.lnum) -
											  1 - curwin->w_virtcol / Columns;
	while (curwin->w_row >= curwin->w_height && curwin->w_cursor.lnum > 1)
		curwin->w_row -= plines(curwin->w_cursor.lnum--);
	comp_Botline(curwin);
}

	void
scrollup(line_count)
	long	line_count;
{
	curwin->w_topline += line_count;
	if (curwin->w_topline > curbuf->b_ml.ml_line_count)
		curwin->w_topline = curbuf->b_ml.ml_line_count;
	if (curwin->w_cursor.lnum < curwin->w_topline)
		curwin->w_cursor.lnum = curwin->w_topline;
	comp_Botline(curwin);
}

/*
 * Scroll the screen one line down, but don't do it if it would move the
 * cursor off the screen.
 */
	void
scrolldown_clamp()
{
	int		end_row;

	if (curwin->w_topline == 1)
		return;

	/*
	 * Compute the row number of the last row of the cursor line
	 * and make sure it doesn't go off the screen. Make sure the cursor
	 * doesn't go past 'scrolloff' lines from the screen end.
	 */
	end_row = curwin->w_row + plines(curwin->w_topline - 1);
	if (curwin->w_p_wrap)
		end_row += plines(curwin->w_cursor.lnum) - 1 -
												  curwin->w_virtcol / Columns;
	if (end_row < curwin->w_height - p_so)
		--curwin->w_topline;
}

/*
 * Scroll the screen one line up, but don't do it if it would move the cursor
 * off the screen.
 */
	void
scrollup_clamp()
{
	int		start_row;

	if (curwin->w_topline == curbuf->b_ml.ml_line_count)
		return;

	/*
	 * Compute the row number of the first row of the cursor line
	 * and make sure it doesn't go off the screen. Make sure the cursor
	 * doesn't go before 'scrolloff' lines from the screen start.
	 */
	start_row = curwin->w_row - plines(curwin->w_topline);
	if (curwin->w_p_wrap)
		start_row -= curwin->w_virtcol / Columns;
	if (start_row >= p_so)
		++curwin->w_topline;
}

/*
 * insert 'line_count' lines at 'row' in window 'wp'
 * if 'invalid' is TRUE the wp->w_lsize_lnum[] is invalidated.
 * if 'mayclear' is TRUE the screen will be cleared if it is faster than
 * scrolling.
 * Returns FAIL if the lines are not inserted, OK for success.
 */
	int
win_ins_lines(wp, row, line_count, invalid, mayclear)
	WIN		*wp;
	int		row;
	int		line_count;
	int		invalid;
	int		mayclear;
{
	int		did_delete;
	int		nextrow;
	int		lastrow;
	int		retval;

	if (invalid)
		wp->w_lsize_valid = 0;

	if (RedrawingDisabled || line_count <= 0 || wp->w_height < 5)
		return FAIL;
	
	if (line_count > wp->w_height - row)
		line_count = wp->w_height - row;

	if (mayclear && Rows - line_count < 5)	/* only a few lines left: redraw is faster */
	{
		screenclear();		/* will set wp->w_lsize_valid to 0 */
		return FAIL;
	}

	/*
	 * Delete all remaining lines
	 */
	if (row + line_count >= wp->w_height)
	{
		screen_fill(wp->w_winpos + row, wp->w_winpos + wp->w_height,
												   0, (int)Columns, ' ', ' ');
		return OK;
	}

	/*
	 * when scrolling, the message on the command line should be cleared,
	 * otherwise it will stay there forever.
	 */
	clear_cmdline = TRUE;

	/*
	 * if the terminal can set a scroll region, use that
	 */
	if (scroll_region)
	{
		scroll_region_set(wp, row);
		retval = screen_ins_lines(wp->w_winpos + row, 0, line_count,
														  wp->w_height - row);
		scroll_region_reset();
		return retval;
	}

	if (wp->w_next && p_tf)		/* don't delete/insert on fast terminal */
		return FAIL;

	/*
	 * If there is a next window or a status line, we first try to delete the
	 * lines at the bottom to avoid messing what is after the window.
	 * If this fails and there are following windows, don't do anything to avoid
	 * messing up those windows, better just redraw.
	 */
	did_delete = FALSE;
	if (wp->w_next || wp->w_status_height)
	{
		if (screen_del_lines(0, wp->w_winpos + wp->w_height - line_count,
										  line_count, (int)Rows, FALSE) == OK)
			did_delete = TRUE;
		else if (wp->w_next)
			return FAIL;
	}
	/*
	 * if no lines deleted, blank the lines that will end up below the window
	 */
	if (!did_delete)
	{
		wp->w_redr_status = TRUE;
		redraw_cmdline = TRUE;
		nextrow = wp->w_winpos + wp->w_height + wp->w_status_height;
		lastrow = nextrow + line_count;
		if (lastrow > Rows)
			lastrow = Rows;
		screen_fill(nextrow - line_count, lastrow - line_count,
												   0, (int)Columns, ' ', ' ');
	}

	if (screen_ins_lines(0, wp->w_winpos + row, line_count, (int)Rows) == FAIL)
	{
			/* deletion will have messed up other windows */
		if (did_delete)
		{
			wp->w_redr_status = TRUE;
			win_rest_invalid(wp->w_next);
		}
		return FAIL;
	}

	return OK;
}

/*
 * delete 'line_count' lines at 'row' in window 'wp'
 * If 'invalid' is TRUE curwin->w_lsize_lnum[] is invalidated.
 * If 'mayclear' is TRUE the screen will be cleared if it is faster than
 * scrolling
 * Return OK for success, FAIL if the lines are not deleted.
 */
	int
win_del_lines(wp, row, line_count, invalid, mayclear)
	WIN				*wp;
	int 			row;
	int 			line_count;
	int				invalid;
	int				mayclear;
{
	int			retval;

	if (invalid)
		wp->w_lsize_valid = 0;

	if (RedrawingDisabled || line_count <= 0)
		return FAIL;
	
	if (line_count > wp->w_height - row)
		line_count = wp->w_height - row;

	/* only a few lines left: redraw is faster */
	if (mayclear && Rows - line_count < 5)
	{
		screenclear();		/* will set wp->w_lsize_valid to 0 */
		return FAIL;
	}

	/*
	 * Delete all remaining lines
	 */
	if (row + line_count >= wp->w_height)
	{
		screen_fill(wp->w_winpos + row, wp->w_winpos + wp->w_height,
												   0, (int)Columns, ' ', ' ');
		return OK;
	}

	/*
	 * when scrolling, the message on the command line should be cleared,
	 * otherwise it will stay there forever.
	 */
	clear_cmdline = TRUE;

	/*
	 * if the terminal can set a scroll region, use that
	 */
	if (scroll_region)
	{
		scroll_region_set(wp, row);
		retval = screen_del_lines(wp->w_winpos + row, 0, line_count,
												   wp->w_height - row, FALSE);
		scroll_region_reset();
		return retval;
	}

	if (wp->w_next && p_tf)		/* don't delete/insert on fast terminal */
		return FAIL;

	if (screen_del_lines(0, wp->w_winpos + row, line_count,
													(int)Rows, FALSE) == FAIL)
		return FAIL;

	/*
	 * If there are windows or status lines below, try to put them at the
	 * correct place. If we can't do that, they have to be redrawn.
	 */
	if (wp->w_next || wp->w_status_height || cmdline_row < Rows - 1)
	{
		if (screen_ins_lines(0, wp->w_winpos + wp->w_height - line_count,
											   line_count, (int)Rows) == FAIL)
		{
			wp->w_redr_status = TRUE;
			win_rest_invalid(wp->w_next);
		}
	}
	/*
	 * If this is the last window and there is no status line, redraw the
	 * command line later.
	 */
	else
		redraw_cmdline = TRUE;
	return OK;
}

/*
 * window 'wp' and everything after it is messed up, mark it for redraw
 */
	void
win_rest_invalid(wp)
	WIN			*wp;
{
	while (wp)
	{
		wp->w_lsize_valid = 0;
		wp->w_redr_type = NOT_VALID;
		wp->w_redr_status = TRUE;
		wp = wp->w_next;
	}
	redraw_cmdline = TRUE;
}

/*
 * The rest of the routines in this file perform screen manipulations. The
 * given operation is performed physically on the screen. The corresponding
 * change is also made to the internal screen image. In this way, the editor
 * anticipates the effect of editing changes on the appearance of the screen.
 * That way, when we call screenupdate a complete redraw isn't usually
 * necessary. Another advantage is that we can keep adding code to anticipate
 * screen changes, and in the meantime, everything still works.
 */

/*
 * types for inserting or deleting lines
 */
#define USE_T_CAL	1
#define USE_T_CDL	2
#define USE_T_AL	3
#define USE_T_CE	4
#define USE_T_DL	5
#define USE_T_SR	6
#define USE_NL		7
#define USE_T_CD	8

/*
 * insert lines on the screen and update NextScreen
 * 'end' is the line after the scrolled part. Normally it is Rows.
 * When scrolling region used 'off' is the offset from the top for the region.
 * 'row' and 'end' are relative to the start of the region.
 *
 * return FAIL for failure, OK for success.
 */
	static int
screen_ins_lines(off, row, line_count, end)
	int			off;
	int 		row;
	int 		line_count;
	int			end;
{
	int 		i;
	int 		j;
	char_u		*temp;
	int			cursor_row;
	int			type;
	int			result_empty;

	/*
	 * FAIL if
	 * - there is no valid screen 
	 * - the screen has to be redrawn completely
	 * - the line count is less than one
	 * - the line count is more than 'ttyscroll'
	 */
	if (!screen_valid(TRUE) || line_count <= 0 || line_count > p_ttyscroll)
		return FAIL;

	/*
	 * There are seven ways to insert lines:
	 * 1. Use T_CD (clear to end of display) if it exists and the result of
	 *	  the insert is just empty lines
	 * 2. Use T_CAL (insert multiple lines) if it exists and T_AL is not
	 *    present or line_count > 1. It looks better if we do all the inserts
	 *    at once.
	 * 3. Use T_CDL (delete multiple lines) if it exists and the result of the
	 *    insert is just empty lines and T_CE is not present or line_count >
	 *    1.
	 * 4. Use T_AL (insert line) if it exists.
	 * 5. Use T_CE (erase line) if it exists and the result of the insert is
	 *    just empty lines.
	 * 6. Use T_DL (delete line) if it exists and the result of the insert is
	 *    just empty lines.
	 * 7. Use T_SR (scroll reverse) if it exists and inserting at row 0 and
	 *    the 'da' flag is not set or we have clear line capability.
	 *
	 * Careful: In a hpterm scroll reverse doesn't work as expected, it moves
	 * the scrollbar for the window. It does have insert line, use that if it
	 * exists.
	 */
	result_empty = (row + line_count >= end);
	if (*T_CD != NUL && result_empty)
		type = USE_T_CD;
	else if (*T_CAL != NUL && (line_count > 1 || *T_AL == NUL))
		type = USE_T_CAL;
	else if (*T_CDL != NUL && result_empty && (line_count > 1 || *T_CE == NUL))
		type = USE_T_CDL;
	else if (*T_AL != NUL)
		type = USE_T_AL;
	else if (*T_CE != NUL && result_empty)
		type = USE_T_CE;
	else if (*T_DL != NUL && result_empty)
		type = USE_T_DL;
	else if (*T_SR != NUL && row == 0 && (*T_DA == NUL || *T_CE))
		type = USE_T_SR;
	else
		return FAIL;
	
	/*
	 * For clearing the lines screen_del_lines is used. This will also take
	 * care of t_db if necessary.
	 */
	if (type == USE_T_CD || type == USE_T_CDL ||
										 type == USE_T_CE || type == USE_T_DL)
		return screen_del_lines(off, row, line_count, end, FALSE);

	/*
	 * If text is retained below the screen, first clear or delete as many
	 * lines at the bottom of the window as are about to be inserted so that
	 * the deleted lines won't later surface during a screen_del_lines.
	 */
	if (*T_DB)
		screen_del_lines(off, end - line_count, line_count, end, FALSE);

	if (*T_CSC != NUL)     /* cursor relative to region */
		cursor_row = row;
	else
		cursor_row = row + off;

	/*
	 * Shift LinePointers line_count down to reflect the inserted lines.
	 * Clear the inserted lines in NextScreen.
	 */
	row += off;
	end += off;
	for (i = 0; i < line_count; ++i)
	{
		j = end - 1 - i;
		temp = LinePointers[j];
		while ((j -= line_count) >= row)
			LinePointers[j + line_count] = LinePointers[j];
		LinePointers[j + line_count] = temp;
		lineclear(temp);
	}

    windgoto(cursor_row, 0);
    if (type == USE_T_CAL)
	{
		OUTSTR(tgoto((char *)T_CAL, 0, line_count));
		screen_start();			/* don't know where cursor is now */
	}
    else
	{
        for (i = 0; i < line_count; i++) 
        {
			if (type == USE_T_AL)
			{
				if (i && cursor_row != 0)
					windgoto(cursor_row, 0);
				outstr(T_AL);
			}
			else  /* type == USE_T_SR */
				outstr(T_SR);
			screen_start();			/* don't know where cursor is now */
        }
    }

	/*
	 * With scroll-reverse and 'da' flag set we need to clear the lines that
	 * have been scrolled down into the region.
	 */
	if (type == USE_T_SR && *T_DA)
	{
        for (i = 0; i < line_count; ++i) 
        {
			windgoto(off + i, 0);
			outstr(T_CE);
			screen_start();			/* don't know where cursor is now */
		}
	}

	return OK;
}

/*
 * delete lines on the screen and update NextScreen
 * 'end' is the line after the scrolled part. Normally it is Rows.
 * When scrolling region used 'off' is the offset from the top for the region.
 * 'row' and 'end' are relative to the start of the region.
 *
 * Return OK for success, FAIL if the lines are not deleted.
 */
	int
screen_del_lines(off, row, line_count, end, force)
	int				off;
	int 			row;
	int 			line_count;
	int				end;
	int				force;		/* even when line_count > p_ttyscroll */
{
	int 		j;
	int 		i;
	char_u		*temp;
	int			cursor_row;
	int			cursor_end;
	int			result_empty;	/* result is empty until end of region */
	int			can_delete;		/* deleting line codes can be used */
	int			type;

	/*
	 * FAIL if
	 * - there is no valid screen 
	 * - the screen has to be redrawn completely
	 * - the line count is less than one
	 * - the line count is more than 'ttyscroll'
	 */
	if (!screen_valid(TRUE) || line_count <= 0 ||
										 (!force && line_count > p_ttyscroll))
		return FAIL;

	/*
	 * Check if the rest of the current region will become empty.
	 */
	result_empty = row + line_count >= end;

	/*
	 * We can delete lines only when 'db' flag not set or when 'ce' option
	 * available.
	 */
	can_delete = (*T_DB == NUL || *T_CE);

	/*
	 * There are four ways to delete lines:
	 * 1. Use T_CD if it exists and the result is empty.
	 * 2. Use newlines if row == 0 and count == 1 or T_CDL does not exist.
	 * 3. Use T_CDL (delete multiple lines) if it exists and line_count > 1 or
	 *    none of the other ways work.
	 * 4. Use T_CE (erase line) if the result is empty.
	 * 5. Use T_DL (delete line) if it exists.
	 */
	if (*T_CD != NUL && result_empty)
		type = USE_T_CD;
	else if (row == 0 && (line_count == 1 || *T_CDL == NUL))
		type = USE_NL;
	else if (*T_CDL != NUL && line_count > 1 && can_delete)
		type = USE_T_CDL;
	else if (*T_CE != NUL && result_empty)
		type = USE_T_CE;
	else if (*T_DL != NUL && can_delete)
		type = USE_T_DL;
	else if (*T_CDL != NUL && can_delete)
		type = USE_T_CDL;
	else
		return FAIL;

	if (*T_CSC != NUL)		/* cursor relative to region */
	{
		cursor_row = row;
		cursor_end = end;
	}
	else
	{
		cursor_row = row + off;
		cursor_end = end + off;
	}

	/*
	 * Now shift LinePointers line_count up to reflect the deleted lines.
	 * Clear the inserted lines in NextScreen.
	 */
	row += off;
	end += off;
	for (i = 0; i < line_count; ++i)
	{
		j = row + i;
		temp = LinePointers[j];
		while ((j += line_count) <= end - 1)
			LinePointers[j - line_count] = LinePointers[j];
		LinePointers[j - line_count] = temp;
		lineclear(temp);
	}

	/* delete the lines */
	if (type == USE_T_CD)
	{
		windgoto(cursor_row, 0);
		outstr(T_CD);
		screen_start();					/* don't know where cursor is now */
	}
	else if (type == USE_T_CDL)
	{
		windgoto(cursor_row, 0);
		OUTSTR(tgoto((char *)T_CDL, 0, line_count));
		screen_start();					/* don't know where cursor is now */
	}
		/*
		 * Deleting lines at top of the screen or scroll region: Just scroll
		 * the whole screen (scroll region) up by outputting newlines on the
		 * last line.
		 */
	else if (type == USE_NL)
	{
		windgoto(cursor_end - 1, 0);
		for (i = line_count; --i >= 0; )
			outchar('\n');				/* cursor will remain on same line */
	}
	else
	{
		for (i = line_count; --i >= 0; )
		{
			if (type == USE_T_DL)
			{
				windgoto(cursor_row, 0);
				outstr(T_DL);           /* delete a line */
			}
			else /* type == USE_T_CE */
			{
				windgoto(cursor_row + i, 0);
				outstr(T_CE);           /* erase a line */
			}
			screen_start();				/* don't know where cursor is now */
		}
	}

	/*
	 * If the 'db' flag is set, we need to clear the lines that have been
	 * scrolled up at the bottom of the region.
	 */
	if (*T_DB && (type == USE_T_DL || type == USE_T_CDL))
	{
		for (i = line_count; i > 0; --i)
		{
			windgoto(cursor_end - i, 0);
			outstr(T_CE);           	/* erase a line */
			screen_start();				/* don't know where cursor is now */
		}
	}
	return OK;
}

/*
 * show the current mode and ruler
 *
 * If clear_cmdline is TRUE, clear the rest of the cmdline.
 * If clear_cmdline is FALSE there may be a message there that needs to be
 * cleared only if a mode is shown.
 * Return the lenght of the message (0 if no message).
 */
	int
showmode()
{
	int		need_clear = FALSE;
	int		length = 0;
	int		do_mode = (p_smd &&
						 ((State & INSERT) || restart_edit || VIsual_active));

	if (do_mode || Recording)
	{
		if (emsg_on_display)
		{
			mch_delay(1000L, TRUE);
			emsg_on_display = FALSE;
		}
		msg_didout = FALSE;				/* never scroll up */
		msg_col = 0;
		gotocmdline(FALSE);
		set_highlight('M');		/* Highlight mode */
		if (do_mode)
		{
			start_highlight();
			MSG_OUTSTR("--");
#ifdef INSERT_EXPAND
			if (edit_submode != NULL)		/* CTRL-X in Insert mode */
			{
				msg_outstr(edit_submode);
				if (edit_submode_extra != NULL)
				{
					msg_outchar(' ');		/* add a space in between */
					if (edit_submode_highl != NUL)
					{
						stop_highlight();
						set_highlight(edit_submode_highl);	/* Highlight mode */
						start_highlight();
					}
					msg_outstr(edit_submode_extra);
					if (edit_submode_highl != NUL)
					{
						stop_highlight();
						set_highlight('M');		/* Highlight mode */
						start_highlight();
					}
				}
			}
			else
#endif
			{
				if (State == INSERT)
				{
#ifdef RIGHTLEFT
					if (p_ri)
						MSG_OUTSTR(" REVERSE");
#endif
					MSG_OUTSTR(" INSERT");
				}
				else if (State == REPLACE)
					MSG_OUTSTR(" REPLACE");
				else if (restart_edit == 'I')
					MSG_OUTSTR(" (insert)");
				else if (restart_edit == 'R')
					MSG_OUTSTR(" (replace)");
#ifdef RIGHTLEFT
				if (p_hkmap)
					MSG_OUTSTR(" Hebrew");
#endif
				if ((State & INSERT) && p_paste)
					MSG_OUTSTR(" (paste)");
				if (VIsual_active)
				{
					MSG_OUTSTR(" VISUAL");
					if (VIsual_mode == Ctrl('V'))
						MSG_OUTSTR(" BLOCK");
					else if (VIsual_mode == 'V')
						MSG_OUTSTR(" LINE");
				}
			}
			MSG_OUTSTR(" --");
			need_clear = TRUE;
		}
		if (Recording)
		{
			if (!need_clear)
				start_highlight();
			MSG_OUTSTR("recording");
			need_clear = TRUE;
		}
		if (need_clear)
			stop_highlight();
		if (need_clear || clear_cmdline)
			msg_clr_eos();
		msg_didout = FALSE;				/* overwrite this message */
		length = msg_col;
		msg_col = 0;
	}
	else if (clear_cmdline)				/* just clear anything */
	{
		msg_row = cmdline_row;
		msg_col = 0;
		msg_clr_eos();					/* will reset clear_cmdline */
	}
	win_redr_ruler(lastwin, TRUE);
	redraw_cmdline = FALSE;

	return length;
}

/*
 * delete mode message
 */
	void
delmode()
{
	if (Recording)
		MSG("recording");
	else
		MSG("");
}

/*
 * if ruler option is set: show current cursor position
 * if always is FALSE, only print if position has changed
 */
	void
showruler(always)
	int		always;
{
	win_redr_ruler(curwin, always);
}

	void
win_redr_ruler(wp, always)
	WIN		*wp;
	int		always;
{
	static linenr_t	old_lnum = 0;
	static colnr_t	old_col = 0;
	char_u			buffer[30];
	int				row;
	int				fillchar;

	if (p_ru && (redraw_cmdline || always ||
				wp->w_cursor.lnum != old_lnum || wp->w_virtcol != old_col))
	{
		cursor_off();
		if (wp->w_status_height)
		{
			row = wp->w_winpos + wp->w_height;
			if (set_highlight('s') == OK)		/* can use highlighting */
			{
				fillchar = ' ';
				start_highlight();
			}
			else
				fillchar = '=';
		}
		else
		{
			row = Rows - 1;
			fillchar = ' ';
		}
		/*
		 * Some sprintfs return the length, some return a pointer.
		 * To avoid portability problems we use strlen() here.
		 */
		
		sprintf((char *)buffer, "%ld,",
				(wp->w_buffer->b_ml.ml_flags & ML_EMPTY) ?
					0L :
					(long)(wp->w_cursor.lnum));
		/*
		 * Check if cursor.lnum is valid, since win_redr_ruler() may be called
		 * after deleting lines, before cursor.lnum is corrected.
		 */
		if (wp->w_cursor.lnum <= wp->w_buffer->b_ml.ml_line_count)
			col_print(buffer + STRLEN(buffer),
				!(State & INSERT) &&
				*ml_get_buf(wp->w_buffer, wp->w_cursor.lnum, FALSE) == NUL ?
					0 :
					(int)wp->w_cursor.col + 1,
					(int)wp->w_virtcol + 1);

		screen_msg(buffer, row, ru_col);
		screen_fill(row, row + 1, ru_col + (int)STRLEN(buffer),
											(int)Columns, fillchar, fillchar);
		old_lnum = wp->w_cursor.lnum;
		old_col = wp->w_virtcol;
		stop_highlight();
	}
}

/*
 * screen_valid -  allocate screen buffers if size changed 
 *   If "clear" is TRUE: clear screen if it has been resized.
 *		Returns TRUE if there is a valid screen to write to.
 *		Returns FALSE when starting up and screen not initialized yet.
 */
	int
screen_valid(clear)
	int		clear;
{
	screenalloc(clear);		/* allocate screen buffers if size changed */
	return (NextScreen != NULL);
}

#ifdef USE_MOUSE
/*
 * Move the cursor to the specified row and column on the screen.
 * Change current window if neccesary.  Returns an integer with the
 * CURSOR_MOVED bit set if the cursor has moved or unset otherwise.
 *
 * If flags has MOUSE_FOCUS, then the current window will not be changed, and
 * if the mouse is outside the window then the text will scroll, or if the
 * mouse was previously on a status line, then the status line may be dragged.
 *
 * If flags has MOUSE_MAY_VIS, then VIsual mode will be started before the
 * cursor is moved unless the cursor was on a status line.  Ignoring the
 * CURSOR_MOVED bit, this function returns one of IN_UNKNOWN, IN_BUFFER, or
 * IN_STATUS_LINE depending on where the cursor was clicked.
 *
 * If flags has MOUSE_DID_MOVE, nothing is done if the mouse didn't move since
 * the last call.
 *
 * If flags has MOUSE_SETPOS, nothing is done, only the current position is
 * remembered.
 */
	int
jump_to_mouse(flags)
	int		flags;
{
	static int on_status_line = 0;		/* #lines below bottom of window */
	static int prev_row = -1;
	static int prev_col = -1;

	WIN		*wp, *old_curwin;
	FPOS	old_cursor;
	int		count;
	int		first;
	int		row = mouse_row;
	int		col = mouse_col;

	mouse_past_bottom = FALSE;
	mouse_past_eol = FALSE;

	if ((flags & MOUSE_DID_MOVE) && prev_row == mouse_row &&
														prev_col == mouse_col)
		return IN_BUFFER;				/* mouse pointer didn't move */

	prev_row = mouse_row;
	prev_col = mouse_col;

	if ((flags & MOUSE_SETPOS))
		return IN_BUFFER;				/* mouse pointer didn't move */

	old_curwin = curwin;
	old_cursor = curwin->w_cursor;

	if (!(flags & MOUSE_FOCUS))
	{
		if (row < 0 || col < 0)		/* check if it makes sense */
			return IN_UNKNOWN;

		/* find the window where the row is in */
		for (wp = firstwin; wp->w_next; wp = wp->w_next)
			if (row < wp->w_next->w_winpos)
				break;
		/*
		 * winpos and height may change in win_enter()!
		 */
		row -= wp->w_winpos;
		if (row >= wp->w_height)	/* In (or below) status line */
			on_status_line = row - wp->w_height + 1;
		else
			on_status_line = 0;
		win_enter(wp, TRUE);		/* can make wp invalid! */
		if (on_status_line)			/* In (or below) status line */
		{
			/* Don't use start_arrow() if we're in the same window */
			if (curwin == old_curwin)
				return IN_STATUS_LINE;
			else
				return IN_STATUS_LINE | CURSOR_MOVED;
		}

		curwin->w_cursor.lnum = curwin->w_topline;
	}
	else if (on_status_line)
	{
		/* Drag the status line */
		count = row - curwin->w_winpos - curwin->w_height + 1 - on_status_line;
		win_drag_status_line(count);
		return IN_STATUS_LINE;		/* Cursor didn't move */
	}
	else /* keep_window_focus must be TRUE */
	{
		row -= curwin->w_winpos;

		/*
		 * When clicking beyond the end of the window, scroll the screen.
		 * Scroll by however many rows outside the window we are.
		 */
		if (row < 0)
		{
			count = 0;
			for (first = TRUE; curwin->w_topline > 1; --curwin->w_topline)
			{
				count += plines(curwin->w_topline - 1);
				if (!first && count > -row)
					break;
				first = FALSE;
			}
			redraw_later(VALID);
			row = 0;
		}
		else if (row >= curwin->w_height)
		{
			count = 0;
			for (first = TRUE; curwin->w_topline < curbuf->b_ml.ml_line_count;
														++curwin->w_topline)
			{
				count += plines(curwin->w_topline);
				if (!first && count > row - curwin->w_height + 1)
					break;
				first = FALSE;
			}
			redraw_later(VALID);
			row = curwin->w_height - 1;
		}
		curwin->w_cursor.lnum = curwin->w_topline;
	}

#ifdef RIGHTLEFT
	if (curwin->w_p_rl)
		col = Columns - 1 - col;
#endif

	if (curwin->w_p_wrap)		/* lines wrap */
	{
		while (row)
		{
			count = plines(curwin->w_cursor.lnum);
			if (count > row)
			{
				col += row * Columns;
				break;
			}
			if (curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count)
			{
				mouse_past_bottom = TRUE;
				break;
			}
			row -= count;
			++curwin->w_cursor.lnum;
		}
	}
	else						/* lines don't wrap */
	{
		curwin->w_cursor.lnum += row;
		if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		{
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			mouse_past_bottom = TRUE;
		}
		col += curwin->w_leftcol;
	}

	if (curwin->w_p_nu)			/* skip number in front of the line */
		if ((col -= 8) < 0)
			col = 0;

	curwin->w_curswant = col;
	curwin->w_set_curswant = FALSE;		/* May still have been TRUE */
	if (coladvance(col) == FAIL)
	{
		/* Mouse click beyond end of line */
		op_inclusive = TRUE;
		mouse_past_eol = TRUE;
	}
	else
		op_inclusive = FALSE;

	if ((flags & MOUSE_MAY_VIS) && !VIsual_active)
	{
		start_visual_highlight();
		VIsual = old_cursor;
		VIsual_active = TRUE;
#ifdef USE_MOUSE
		setmouse();
#endif
		if (p_smd)
			redraw_cmdline = TRUE;			/* show visual mode later */
	}

	if (curwin == old_curwin && curwin->w_cursor.lnum == old_cursor.lnum &&
									   curwin->w_cursor.col == old_cursor.col)
		return IN_BUFFER;				/* Cursor has not moved */
	return IN_BUFFER | CURSOR_MOVED;	/* Cursor has moved */
}
#endif /* USE_MOUSE */

/*
 * Redraw the screen later, with UpdateScreen(type).
 * Set must_redraw only of not already set to a higher value.
 * e.g. if must_redraw is CLEAR, type == NOT_VALID will do nothing.
 */
	void
redraw_later(type)
	int		type;
{
	if (must_redraw < type)
		must_redraw = type;
}

/*	$OpenBSD: window.c,v 1.2 1996/09/21 06:23:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read a list of people who contributed.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

static void reset_VIsual __ARGS((void));
static int win_comp_pos __ARGS((void));
static void win_exchange __ARGS((long));
static void win_rotate __ARGS((int, int));
static void win_append __ARGS((WIN *, WIN *));
static void win_remove __ARGS((WIN *));
static void win_new_height __ARGS((WIN *, int));

static WIN		*prevwin = NULL;		/* previous window */

/*
 * all CTRL-W window commands are handled here, called from normal().
 */
	void
do_window(nchar, Prenum)
	int		nchar;
	long	Prenum;
{
	long	Prenum1;
	WIN		*wp;
	char_u	*ptr;
	int		len;
	int		type = -1;
	WIN		*wp2;

	if (Prenum == 0)
		Prenum1 = 1;
	else
		Prenum1 = Prenum;

	switch (nchar)
	{
/* split current window in two parts */
	case 'S':
	case Ctrl('S'):
	case 's':	reset_VIsual();					/* stop Visual mode */
				win_split((int)Prenum, TRUE);
				break;

/* split current window and edit alternate file */
	case K_CCIRCM:
	case '^':
				reset_VIsual();					/* stop Visual mode */
				stuffReadbuff((char_u *)":split #");
				if (Prenum)
					stuffnumReadbuff(Prenum);	/* buffer number */
				stuffcharReadbuff('\n');
				break;

/* open new window */
	case Ctrl('N'):
	case 'n':	reset_VIsual();					/* stop Visual mode */
				stuffcharReadbuff(':');
				if (Prenum)
					stuffnumReadbuff(Prenum);		/* window height */
				stuffReadbuff((char_u *)"new\n");	/* it is cmdline.c */
				break;

/* quit current window */
	case Ctrl('Q'):
	case 'q':	reset_VIsual();					/* stop Visual mode */
				stuffReadbuff((char_u *)":quit\n");	/* it is cmdline.c */
				break;

/* close current window */
	case Ctrl('C'):
	case 'c':	reset_VIsual();					/* stop Visual mode */
				stuffReadbuff((char_u *)":close\n");	/* it is cmdline.c */
				break;

/* close all but current window */
	case Ctrl('O'):
	case 'o':	reset_VIsual();					/* stop Visual mode */
				stuffReadbuff((char_u *)":only\n");	/* it is cmdline.c */
				break;

/* cursor to next window */
	case 'j':
	case K_DOWN:
	case Ctrl('J'):
				for (wp = curwin; wp->w_next != NULL && Prenum1-- > 0;
															wp = wp->w_next)
					;
new_win:
				/*
				 * When jumping to another buffer, stop visual mode
				 * Do this before changing windows so we can yank the
				 * selection into the '"*' register.
				 */
				if (wp->w_buffer != curbuf && VIsual_active)
				{
					end_visual_mode();
					for (wp2 = firstwin; wp2 != NULL; wp2 = wp2->w_next)
						if (wp2->w_buffer == curbuf &&
											wp2->w_redr_type < NOT_VALID)
						{
							wp2->w_redr_type = NOT_VALID;
							redraw_later(NOT_VALID);
						}
				}
				win_enter(wp, TRUE);
				cursupdate();
				break;

/* cursor to next window with wrap around */
	case Ctrl('W'):
	case 'w':
/* cursor to previous window with wrap around */
	case 'W':
				if (lastwin == firstwin)		/* just one window */
					beep_flush();
				else
				{
					if (Prenum)					/* go to specified window */
					{
						for (wp = firstwin; --Prenum > 0; )
						{
							if (wp->w_next == NULL)
								break;
							else
								wp = wp->w_next;
						}
					}
					else
					{
						if (nchar == 'W')			/* go to previous window */
						{
							wp = curwin->w_prev;
							if (wp == NULL)
								wp = lastwin;		/* wrap around */
						}
						else						/* go to next window */
						{
							wp = curwin->w_next;
							if (wp == NULL)
								wp = firstwin;		/* wrap around */
						}
					}
					goto new_win;
				}
				break;

/* cursor to window above */
	case 'k':
	case K_UP:
	case Ctrl('K'):
				for (wp = curwin; wp->w_prev != NULL && Prenum1-- > 0;
															wp = wp->w_prev)
					;
				goto new_win;

/* cursor to top window */
	case 't':
	case Ctrl('T'):
				wp = firstwin;
				goto new_win;

/* cursor to bottom window */
	case 'b':
	case Ctrl('B'):
				wp = lastwin;
				goto new_win;

/* cursor to last accessed (previous) window */
	case 'p':
	case Ctrl('P'):
				if (prevwin == NULL)
					beep_flush();
				else
				{
					wp = prevwin;
					goto new_win;
				}
				break;

/* exchange current and next window */
	case 'x':
	case Ctrl('X'):
				win_exchange(Prenum);
				break;

/* rotate windows downwards */
	case Ctrl('R'):
	case 'r':	reset_VIsual();					/* stop Visual mode */
				win_rotate(FALSE, (int)Prenum1);	/* downwards */
				break;

/* rotate windows upwards */
	case 'R':	reset_VIsual();					/* stop Visual mode */
				win_rotate(TRUE, (int)Prenum1);		/* upwards */
				break;

/* make all windows the same height */
	case '=':	win_equal(NULL, TRUE);
				break;

/* increase current window height */
	case '+':	win_setheight(curwin->w_height + (int)Prenum1);
				break;

/* decrease current window height */
	case '-':	win_setheight(curwin->w_height - (int)Prenum1);
				break;

/* set current window height */
	case Ctrl('_'):
	case '_':	win_setheight(Prenum ? (int)Prenum : 9999);
				break;

/* jump to tag and split window if tag exists */
	case ']':
	case Ctrl(']'):
				reset_VIsual();					/* stop Visual mode */
				postponed_split = TRUE;
				stuffcharReadbuff(Ctrl(']'));
				break;

/* edit file name under cursor in a new window */
	case 'f':
	case Ctrl('F'):
				reset_VIsual();					/* stop Visual mode */
				ptr = file_name_at_cursor(FNAME_MESS|FNAME_HYP|FNAME_EXP);
				if (ptr != NULL)
				{
					setpcmark();
					if (win_split(0, FALSE) == OK)
						(void)do_ecmd(0, ptr, NULL, NULL, (linenr_t)0,
													   p_hid ? ECMD_HIDE : 0);
					vim_free(ptr);
				}
				break;

/* Go to the first occurence of the identifier under cursor along path in a
 * new window -- webb
 */
	case 'i':						/* Go to any match */
	case Ctrl('I'):
				type = FIND_ANY;
				/* FALLTHROUGH */
	case 'd':						/* Go to definition, using p_def */
	case Ctrl('D'):
				if (type == -1)
					type = FIND_DEFINE;

				if ((len = find_ident_under_cursor(&ptr, FIND_IDENT)) == 0)
					break;
				find_pattern_in_path(ptr, len, TRUE, TRUE, type,
					   Prenum1, ACTION_SPLIT, (linenr_t)1, (linenr_t)MAXLNUM);
				curwin->w_set_curswant = TRUE;
				break;

	default:	beep_flush();
				break;
	}
}

	static void
reset_VIsual()
{
	if (VIsual_active)
	{
		end_visual_mode();
		update_curbuf(NOT_VALID);		/* delete the inversion */
	}
}

/*
 * split the current window, implements CTRL-W s and :split
 *
 * new_height is the height for the new window, 0 to make half of current
 * height redraw is TRUE when redraw now
 *
 * return FAIL for failure, OK otherwise
 */
	int
win_split(new_height, redraw)
	int		new_height;
	int		redraw;
{
	WIN			*wp;
	linenr_t	lnum;
	int			h;
	int			i;
	int			need_status;
	int			do_equal = (p_ea && new_height == 0);
	int			needed;
	int			available;
	int			curwin_height;
	
		/* add a status line when p_ls == 1 and splitting the first window */
	if (lastwin == firstwin && p_ls == 1 && curwin->w_status_height == 0)
		need_status = STATUS_HEIGHT;
	else
		need_status = 0;

/*
 * check if we are able to split the current window and compute its height
 */
	available = curwin->w_height;
 	needed = 2 * MIN_ROWS + STATUS_HEIGHT + need_status;
	if (p_ea)
	{
		for (wp = firstwin; wp != NULL; wp = wp->w_next)
			if (wp != curwin)
			{
				available += wp->w_height;
				needed += MIN_ROWS;
			}
	}
 	if (available < needed)
	{
		EMSG(e_noroom);
		return FAIL;
	}
	curwin_height = curwin->w_height;
	if (need_status)
	{
		curwin->w_status_height = STATUS_HEIGHT;
		curwin_height -= STATUS_HEIGHT;
	}
	if (new_height == 0)
		new_height = curwin_height / 2;

	if (new_height > curwin_height - MIN_ROWS - STATUS_HEIGHT)
		new_height = curwin_height - MIN_ROWS - STATUS_HEIGHT;

	if (new_height < MIN_ROWS)
		new_height = MIN_ROWS;

		/* if it doesn't fit in the current window, need win_equal() */
	if (curwin_height - new_height - STATUS_HEIGHT < MIN_ROWS)
		do_equal = TRUE;
/*
 * allocate new window structure and link it in the window list
 */
	if (p_sb)		/* new window below current one */
		wp = win_alloc(curwin);
	else
		wp = win_alloc(curwin->w_prev);
	if (wp == NULL)
		return FAIL;
/*
 * compute the new screen positions
 */
	win_new_height(wp, new_height);
	win_new_height(curwin, curwin_height - (new_height + STATUS_HEIGHT));
	if (p_sb)		/* new window below current one */
	{
		wp->w_winpos = curwin->w_winpos + curwin->w_height + STATUS_HEIGHT;
		wp->w_status_height = curwin->w_status_height;
		curwin->w_status_height = STATUS_HEIGHT;
	}
	else			/* new window above current one */
	{
		wp->w_winpos = curwin->w_winpos;
		wp->w_status_height = STATUS_HEIGHT;
		curwin->w_winpos = wp->w_winpos + wp->w_height + STATUS_HEIGHT;
	}
/*
 * make the contents of the new window the same as the current one
 */
	wp->w_buffer = curbuf;
	curbuf->b_nwindows++;
	wp->w_cursor = curwin->w_cursor;
	wp->w_row = curwin->w_row;
	wp->w_col = curwin->w_col;
	wp->w_virtcol = curwin->w_virtcol;
	wp->w_curswant = curwin->w_curswant;
	wp->w_set_curswant = curwin->w_set_curswant;
	wp->w_empty_rows = curwin->w_empty_rows;
	wp->w_leftcol = curwin->w_leftcol;
	wp->w_pcmark = curwin->w_pcmark;
	wp->w_prev_pcmark = curwin->w_prev_pcmark;
	wp->w_alt_fnum = curwin->w_alt_fnum;

	wp->w_arg_idx = curwin->w_arg_idx;
	/*
	 * copy tagstack and options from existing window
	 */
	for (i = 0; i < curwin->w_tagstacklen; i++)
	{
		wp->w_tagstack[i].fmark = curwin->w_tagstack[i].fmark;
		wp->w_tagstack[i].tagname = strsave(curwin->w_tagstack[i].tagname);
	}
	wp->w_tagstackidx = curwin->w_tagstackidx;
	wp->w_tagstacklen = curwin->w_tagstacklen;
	win_copy_options(curwin, wp);
/*
 * Both windows need redrawing
 */
 	wp->w_redr_type = NOT_VALID;
	wp->w_redr_status = TRUE;
 	curwin->w_redr_type = NOT_VALID;
	curwin->w_redr_status = TRUE;
/*
 * Cursor is put in middle of window in both windows
 */
	if (wp->w_height < curwin->w_height)	/* use smallest of two heights */
		h = wp->w_height;
	else
		h = curwin->w_height;
	h >>= 1;
	for (lnum = wp->w_cursor.lnum; lnum > 1; --lnum)
	{
		h -= plines(lnum);
		if (h <= 0)
			break;
	}
	wp->w_topline = lnum;
	curwin->w_topline = lnum;
	if (need_status)
	{
		msg_pos((int)Rows - 1, sc_col);
		msg_clr_eos();		/* Old command/ruler may still be there -- webb */
		comp_col();
		msg_pos((int)Rows - 1, 0);	/* put position back at start of line */
	}
/*
 * make the new window the current window and redraw
 */
	if (do_equal)
		win_equal(wp, FALSE);
 	win_enter(wp, FALSE);

	if (redraw)
		updateScreen(NOT_VALID);

	return OK;
}

/*
 * Check if "win" is a pointer to an existing window.
 */
	int
win_valid(win)
	WIN		*win;
{
	WIN 	*wp;

	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (wp == win)
			return TRUE;
	return FALSE;
}

/*
 * Return the number of windows.
 */
	int
win_count()
{
	WIN 	*wp;
	int		count = 0;

	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		++count;
	return count;
}

/*
 * Make 'count' windows on the screen.
 * Return actual number of windows on the screen.
 * Must be called when there is just one window, filling the whole screen
 * (excluding the command line).
 */
	int
make_windows(count)
	int		count;
{
	int		maxcount;
	int		todo;
	int		p_sb_save;

/*
 * Each window needs at least MIN_ROWS lines and a status line.
 * Add 4 lines for one window, otherwise we may end up with all one-line
 * windows. Use value of 'winheight' if it is set
 */
	maxcount = (curwin->w_height + curwin->w_status_height -
						(p_wh ? (p_wh - 1) : 4)) / (MIN_ROWS + STATUS_HEIGHT);
	if (maxcount < 2)
		maxcount = 2;
	if (count > maxcount)
		count = maxcount;

	/*
	 * add status line now, otherwise first window will be too big
	 */
	if ((p_ls == 2 || (count > 1 && p_ls == 1)) && curwin->w_status_height == 0)
	{
		curwin->w_status_height = STATUS_HEIGHT;
		win_new_height(curwin, curwin->w_height - STATUS_HEIGHT);
	}

#ifdef AUTOCMD
/*
 * Don't execute autocommands while creating the windows.  Must do that
 * when putting the buffers in the windows.
 */
	++autocmd_busy;
#endif

/*
 * set 'splitbelow' off for a moment, don't want that now
 */
	p_sb_save = p_sb;
	p_sb = FALSE;
		/* todo is number of windows left to create */
	for (todo = count - 1; todo > 0; --todo)
		if (win_split(curwin->w_height - (curwin->w_height - todo
				* STATUS_HEIGHT) / (todo + 1) - STATUS_HEIGHT, FALSE) == FAIL)
			break;
	p_sb = p_sb_save;

#ifdef AUTOCMD
	--autocmd_busy;
#endif

		/* return actual number of windows */
	return (count - todo);
}

/*
 * Exchange current and next window
 */
	static void
win_exchange(Prenum)
	long		Prenum;
{
	WIN		*wp;
	WIN		*wp2;
	int		temp;

	if (lastwin == firstwin)		/* just one window */
	{
		beep_flush();
		return;
	}

/*
 * find window to exchange with
 */
	if (Prenum)
	{
		wp = firstwin;
		while (wp != NULL && --Prenum > 0)
			wp = wp->w_next;
	}
	else if (curwin->w_next != NULL)	/* Swap with next */
		wp = curwin->w_next;
	else	/* Swap last window with previous */
		wp = curwin->w_prev;

	if (wp == curwin || wp == NULL)
		return;

/*
 * 1. remove curwin from the list. Remember after which window it was in wp2
 * 2. insert curwin before wp in the list
 * if wp != wp2
 *    3. remove wp from the list
 *    4. insert wp after wp2
 * 5. exchange the status line height
 */
	wp2 = curwin->w_prev;
	win_remove(curwin);
	win_append(wp->w_prev, curwin);
	if (wp != wp2)
	{
		win_remove(wp);
		win_append(wp2, wp);
	}
	temp = curwin->w_status_height;
	curwin->w_status_height = wp->w_status_height;
	wp->w_status_height = temp;

	win_comp_pos();				/* recompute window positions */

	win_enter(wp, TRUE);
	cursupdate();
	updateScreen(CLEAR);

#ifdef USE_GUI
	if (gui.in_use)
	{
		if (gui.which_scrollbars[SB_LEFT])
			gui_mch_reorder_scrollbars(SB_LEFT);
		if (gui.which_scrollbars[SB_RIGHT])
			gui_mch_reorder_scrollbars(SB_RIGHT);
	}
#endif
}

/*
 * rotate windows: if upwards TRUE the second window becomes the first one
 *				   if upwards FALSE the first window becomes the second one
 */
	static void
win_rotate(upwards, count)
	int		upwards;
	int		count;
{
	WIN			 *wp;
	int			 height;

	if (firstwin == lastwin)			/* nothing to do */
	{
		beep_flush();
		return;
	}
	while (count--)
	{
		if (upwards)			/* first window becomes last window */
		{
			wp = firstwin;
			win_remove(wp);
			win_append(lastwin, wp);
			wp = lastwin->w_prev;			/* previously last window */
		}
		else					/* last window becomes first window */
		{
			wp = lastwin;
			win_remove(lastwin);
			win_append(NULL, wp);
			wp = firstwin;					/* previously last window */
		}
			/* exchange status height of old and new last window */
		height = lastwin->w_status_height;
		lastwin->w_status_height = wp->w_status_height;
		wp->w_status_height = height;

			/* recompute w_winpos for all windows */
		(void) win_comp_pos();
	}

	cursupdate();
	updateScreen(CLEAR);

#ifdef USE_GUI
	if (gui.in_use)
	{
		if (gui.which_scrollbars[SB_LEFT])
			gui_mch_reorder_scrollbars(SB_LEFT);
		if (gui.which_scrollbars[SB_RIGHT])
			gui_mch_reorder_scrollbars(SB_RIGHT);
	}
#endif
}

/*
 * Make all windows the same height.
 * 'next_curwin' will soon be the current window, make sure it has enough
 * rows.
 */
	void
win_equal(next_curwin, redraw)
	WIN		*next_curwin;			/* pointer to current window to be */
	int		redraw;
{
	int		total;
	int		less;
	int		wincount;
	int		winpos;
	int		temp;
	WIN		*wp;
	int		new_height;

/*
 * count the number of lines available
 */
	total = 0;
	wincount = 0;
	for (wp = firstwin; wp; wp = wp->w_next)
	{
		total += wp->w_height - MIN_ROWS;
		wincount++;
	}

/*
 * If next_curwin given and 'winheight' set, make next_curwin p_wh lines.
 */
	less = 0;
	if (next_curwin != NULL)
	{
		if (p_wh)
		{
			if (p_wh - MIN_ROWS > total)	/* all lines go to current window */
				less = total;
			else
			{
				less = p_wh - MIN_ROWS - total / wincount;
				if (less < 0)
					less = 0;
			}
		}
	}

/*
 * spread the available lines over the windows
 */
	winpos = 0;
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
	{
		if (wp == next_curwin && less)
		{
			less = 0;
			temp = p_wh - MIN_ROWS;
			if (temp > total)
				temp = total;
		}
		else
			temp = (total - less + (wincount >> 1)) / wincount;
		new_height = MIN_ROWS + temp;
		if (wp->w_winpos != winpos || wp->w_height != new_height)
		{
			wp->w_redr_type = NOT_VALID;
			wp->w_redr_status = TRUE;
		}
		wp->w_winpos = winpos;
		win_new_height(wp, new_height);
		total -= temp;
		--wincount;
		winpos += wp->w_height + wp->w_status_height;
	}
	if (redraw)
	{
		cursupdate();
		updateScreen(CLEAR);
	}
}

/*
 * close all windows for buffer 'buf'
 */
	void
close_windows(buf)
	BUF		*buf;
{
	WIN 	*win;

	++RedrawingDisabled;
	for (win = firstwin; win != NULL && lastwin != firstwin; )
	{
		if (win->w_buffer == buf)
		{
			close_window(win, FALSE);
			win = firstwin;			/* go back to the start */
		}
		else
			win = win->w_next;
	}
	--RedrawingDisabled;
}

/*
 * close window "win"
 * If "free_buf" is TRUE related buffer may be freed.
 *
 * called by :quit, :close, :xit, :wq and findtag()
 */
	void
close_window(win, free_buf)
	WIN		*win;
	int		free_buf;
{
	WIN 	*wp;
#ifdef AUTOCMD
	int		other_buffer = FALSE;
#endif

	if (lastwin == firstwin)
	{
		EMSG("Cannot close last window");
		return;
	}

#ifdef AUTOCMD
	if (win == curwin)
	{
		/*
		 * Guess which window is going to be the new current window.
		 * This may change because of the autocommands (sigh).
		 */
		if ((!p_sb && win->w_next != NULL) || win->w_prev == NULL)
			wp = win->w_next;
		else
			wp = win->w_prev;

		/*
		 * Be careful: If autocommands delete the window, return now.
		 */
		if (wp->w_buffer != curbuf)
		{
			other_buffer = TRUE;
			apply_autocmds(EVENT_BUFLEAVE, NULL, NULL);
			if (!win_valid(win))
				return;
		}
		apply_autocmds(EVENT_WINLEAVE, NULL, NULL);
		if (!win_valid(win))
			return;
	}
#endif

/*
 * Remove the window.
 * if 'splitbelow' the free space goes to the window above it.
 * if 'nosplitbelow' the free space goes to the window below it.
 * This makes opening a window and closing it immediately keep the same window
 * layout.
 */
									/* freed space goes to next window */
	if ((!p_sb && win->w_next != NULL) || win->w_prev == NULL)
	{
		wp = win->w_next;
		wp->w_winpos = win->w_winpos;
	}
	else							/* freed space goes to previous window */
		wp = win->w_prev;
	win_new_height(wp, wp->w_height + win->w_height + win->w_status_height);

/*
 * Close the link to the buffer.
 */
	close_buffer(win, win->w_buffer, free_buf, FALSE);

	win_free(win);
	if (win == curwin)
		curwin = NULL;
	if (p_ea)
		win_equal(wp, FALSE);
	if (curwin == NULL)
	{
		win_enter(wp, FALSE);
#ifdef AUTOCMD
		if (other_buffer)
			/* careful: after this wp and win may be invalid! */
			apply_autocmds(EVENT_BUFENTER, NULL, NULL);
#endif
	}

	/*
	 * if last window has status line now and we don't want one,
	 * remove the status line
	 */
	if (lastwin->w_status_height &&
						(p_ls == 0 || (p_ls == 1 && firstwin == lastwin)))
	{
		win_new_height(lastwin, lastwin->w_height + lastwin->w_status_height);
		lastwin->w_status_height = 0;
		comp_col();
	}

	updateScreen(NOT_VALID);
	if (RedrawingDisabled && win_valid(wp))
		comp_Botline(wp);			/* need to do this before cursupdate() */
}

/*
 * close all windows except current one
 * buffers in the windows become hidden
 *
 * called by :only and do_arg_all();
 */
	void
close_others(message)
	int		message;
{
	WIN 	*wp;
	WIN 	*nextwp;

	if (lastwin == firstwin)
	{
		if (message
#ifdef AUTOCMD
					&& !autocmd_busy
#endif
									)
			MSG("Already only one window");
		return;
	}

	for (wp = firstwin; wp != NULL; wp = nextwp)
	{
		nextwp = wp->w_next;
		if (wp == curwin)				/* don't close current window */
			continue;
		/*
		 * Close the link to the buffer.
		 */
		close_buffer(wp, wp->w_buffer, FALSE, FALSE);

		/*
		 * Remove the window. All lines go to current window.
		 */
		win_new_height(curwin,
					   curwin->w_height + wp->w_height + wp->w_status_height);
		win_free(wp);
	}

	/*
	 * If current window has status line and we don't want one,
	 * remove the status line.
	 */
	if (curwin->w_status_height && p_ls != 2)
	{
		win_new_height(curwin, curwin->w_height + curwin->w_status_height);
		curwin->w_status_height = 0;
	}
	curwin->w_winpos = 0;		/* put current window at top of the screen */
	if (message)
		updateScreen(NOT_VALID);
}

/*
 * init the cursor in the window
 *
 * called when a new file is being edited
 */
	void
win_init(wp)
	WIN		*wp;
{
	wp->w_redr_type = NOT_VALID;
	wp->w_cursor.lnum = 1;
	wp->w_curswant = wp->w_cursor.col = 0;
	wp->w_pcmark.lnum = 1;		/* pcmark not cleared but set to line 1 */
	wp->w_pcmark.col = 0;
	wp->w_prev_pcmark.lnum = 0;
	wp->w_prev_pcmark.col = 0;
	wp->w_topline = 1;
	wp->w_botline = 2;
}

/*
 * Make window wp the current window.
 * Can be called when curwin == NULL, if curwin already has been closed.
 */
	void
win_enter(wp, undo_sync)
	WIN		*wp;
	int		undo_sync;
{
#ifdef AUTOCMD
	int			other_buffer = FALSE;
#endif

	if (wp == curwin)			/* nothing to do */
		return;

#ifdef AUTOCMD
	if (curwin != NULL)
	{
		/*
		 * Be careful: If autocommands delete the window, return now.
		 */
		if (wp->w_buffer != curbuf)
		{
			apply_autocmds(EVENT_BUFLEAVE, NULL, NULL);
			other_buffer = TRUE;
			if (!win_valid(wp))
				return;
		}
		apply_autocmds(EVENT_WINLEAVE, NULL, NULL);
		if (!win_valid(wp))
			return;
	}
#endif

		/* sync undo before leaving the current buffer */
	if (undo_sync && curbuf != wp->w_buffer)
		u_sync();
		/* may have to copy the buffer options when 'cpo' contains 'S' */
	if (wp->w_buffer != curbuf)
		buf_copy_options(curbuf, wp->w_buffer, TRUE, FALSE);
	if (curwin != NULL)
		prevwin = curwin;		/* remember for CTRL-W p */
	curwin = wp;
	curbuf = wp->w_buffer;

#ifdef AUTOCMD
	apply_autocmds(EVENT_WINENTER, NULL, NULL);
	if (other_buffer)
		apply_autocmds(EVENT_BUFENTER, NULL, NULL);
#endif

	maketitle();
			/* set window height to desired minimal value */
	if (p_wh && curwin->w_height < p_wh)
		win_setheight((int)p_wh);
#ifdef USE_MOUSE
	setmouse();					/* in case jumped to/from help buffer */
#endif
}

/*
 * allocate a window structure and link it in the window list
 */
	WIN *
win_alloc(after)
	WIN		*after;
{
	WIN		*newwin;

/*
 * allocate window structure and linesizes arrays
 */
	newwin = (WIN *)alloc((unsigned)sizeof(WIN));
	if (newwin)
	{
/*
 * most stucture members have to be zero
 */
 		(void)vim_memset(newwin, 0, sizeof(WIN));
/*
 * link the window in the window list
 */
		win_append(after, newwin);

		win_alloc_lsize(newwin);

		/* position the display and the cursor at the top of the file. */
		newwin->w_topline = 1;
		newwin->w_botline = 2;
		newwin->w_cursor.lnum = 1;

#ifdef USE_GUI
		/* Let the GUI know this is a new scrollbar */
		newwin->w_scrollbar.height = 0;
#endif /* USE_GUI */
	}
	return newwin;
}

/*
 * remove window 'wp' from the window list and free the structure
 */
	void
win_free(wp)
	WIN		*wp;
{
	int		i;

	if (prevwin == wp)
		prevwin = NULL;
	win_free_lsize(wp);

	for (i = 0; i < wp->w_tagstacklen; ++i)
		free(wp->w_tagstack[i].tagname);

#ifdef USE_GUI
	if (gui.in_use)
		gui_mch_destroy_scrollbar(wp);
#endif /* USE_GUI */

	win_remove(wp);
	vim_free(wp);
}

	static void
win_append(after, wp)
	WIN		*after, *wp;
{
	WIN 	*before;

	if (after == NULL)		/* after NULL is in front of the first */
		before = firstwin;
	else
		before = after->w_next;

	wp->w_next = before;
	wp->w_prev = after;
	if (after == NULL)
		firstwin = wp;
	else
		after->w_next = wp;
	if (before == NULL)
		lastwin = wp;
	else
		before->w_prev = wp;
}

/*
 * remove window from the window list
 */
	static void
win_remove(wp)
	WIN		*wp;
{
	if (wp->w_prev)
		wp->w_prev->w_next = wp->w_next;
	else
		firstwin = wp->w_next;
	if (wp->w_next)
		wp->w_next->w_prev = wp->w_prev;
	else
		lastwin = wp->w_prev;
}

/*
 * allocate lsize arrays for a window
 * return FAIL for failure, OK for success
 */
	int
win_alloc_lsize(wp)
	WIN		*wp;
{
	wp->w_lsize_valid = 0;
	wp->w_lsize_lnum = (linenr_t *) malloc((size_t) (Rows * sizeof(linenr_t)));
	wp->w_lsize = (char_u *)malloc((size_t) Rows);
	if (wp->w_lsize_lnum == NULL || wp->w_lsize == NULL)
	{
		win_free_lsize(wp);		/* one of the two may have worked */
		wp->w_lsize_lnum = NULL;
		wp->w_lsize = NULL;
		return FAIL;
	}
	return OK;
}

/*
 * free lsize arrays for a window
 */
 	void
win_free_lsize(wp)
	WIN		*wp;
{
	vim_free(wp->w_lsize_lnum);
	vim_free(wp->w_lsize);
}

/*
 * call this fuction whenever Rows changes value
 */
	void
screen_new_rows()
{
	WIN		*wp;
	int		extra_lines;

	if (firstwin == NULL)		/* not initialized yet */
		return;
/*
 * the number of extra lines is the difference between the position where
 * the command line should be and where it is now
 */
	compute_cmdrow();
	extra_lines = Rows - p_ch - cmdline_row;
	if (extra_lines < 0)						/* reduce windows height */
	{
		for (wp = lastwin; wp; wp = wp->w_prev)
		{
			if (wp->w_height - MIN_ROWS < -extra_lines)
			{
				extra_lines += wp->w_height - MIN_ROWS;
				win_new_height(wp, MIN_ROWS);
			}
			else
			{
				win_new_height(wp, wp->w_height + extra_lines);
				break;
			}
		}
		(void)win_comp_pos();				/* compute w_winpos */
	}
	else if (extra_lines > 0)				/* increase height of last window */
		win_new_height(lastwin, lastwin->w_height + extra_lines);

	compute_cmdrow();

	if (p_ea)
		win_equal(curwin, FALSE);
}

/*
 * update the w_winpos field for all windows
 * returns the row just after the last window
 */
	static int
win_comp_pos()
{
	WIN		*wp;
	int		row;

	row = 0;
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
	{
		if (wp->w_winpos != row)		/* if position changes, redraw */
		{
			wp->w_winpos = row;
			wp->w_redr_type = NOT_VALID;
			wp->w_redr_status = TRUE;
		}
		row += wp->w_height + wp->w_status_height;
	}
	return row;
}

/*
 * set current window height
 */
	void
win_setheight(height)
	int		height;
{
	WIN		*wp;
	int		room;				/* total number of lines available */
	int		take;				/* number of lines taken from other windows */
	int		room_cmdline;		/* lines available from cmdline */
	int		row;
	int		run;

	if (height < MIN_ROWS)		/* need at least some lines */
		height = MIN_ROWS;
/*
 * compute the room we have from all the windows
 */
	room = MIN_ROWS;			/* count the MIN_ROWS for the current window */
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		room += wp->w_height - MIN_ROWS;
/*
 * compute the room available from the command line
 */
	room_cmdline = Rows - p_ch - cmdline_row;
/*
 * limit new height to the room available
 */
	if (height > room + room_cmdline)		/* can't make it that large */
		height = room + room_cmdline;		/* use all available room */
/*
 * compute the number of lines we will take from the windows (can be negative)
 */
	take = height - curwin->w_height;
	if (take == 0)							/* no change, nothing to do */
		return;

	if (take > 0)
	{
		take -= room_cmdline;				/* use lines from cmdline first */
		if (take < 0)
			take = 0;
	}
/*
 * set the current window to the new height
 */
	win_new_height(curwin, height);

/*
 * First take lines from the windows below the current window.
 * If that is not enough, takes lines from windows above the current window.
 */
	for (run = 0; run < 2; ++run)
	{
		if (run == 0)
			wp = curwin->w_next;		/* 1st run: start with next window */
		else
			wp = curwin->w_prev;		/* 2nd run: start with prev window */
		while (wp != NULL && take != 0)
		{
			if (wp->w_height - take < MIN_ROWS)
			{
				take -= wp->w_height - MIN_ROWS;
				win_new_height(wp, MIN_ROWS);
			}
			else
			{
				win_new_height(wp, wp->w_height - take);
				take = 0;
			}
			if (run == 0)
				wp = wp->w_next;
			else
				wp = wp->w_prev;
		}
	}

/* recompute the window positions */
	row = win_comp_pos();

/*
 * If there is extra space created between the last window and the command line,
 * clear it.
 */
	screen_fill(row, cmdline_row, 0, (int)Columns, ' ', ' ');
	cmdline_row = row;

	updateScreen(NOT_VALID);
}

#ifdef USE_MOUSE
	void
win_drag_status_line(offset)
	int		offset;
{
	WIN		*wp;
	int		room;
	int		row;
	int		up;				/* if TRUE, drag status line up, otherwise down */

	if (offset < 0)
	{
		up = TRUE;
		offset = -offset;
	}
	else
		up = FALSE;

	if (up)	/* drag up */
	{
		room = 0;
		for (wp = curwin; wp != NULL && room < offset; wp = wp->w_prev)
			room += wp->w_height - MIN_ROWS;
		wp = curwin->w_next;				/* put wp at window that grows */
	}
	else	/* drag down */
	{
		/*
		 * Only dragging the last status line can reduce p_ch.
		 */
		room = Rows - cmdline_row;
		if (curwin->w_next == NULL)
			room -= 1;
		else
			room -= p_ch;
		for (wp = curwin->w_next; wp != NULL && room < offset; wp = wp->w_next)
			room += wp->w_height - MIN_ROWS;
		wp = curwin;						/* put wp at window that grows */
	}

	if (room < offset)		/* Not enough room */
		offset = room;		/* Move as far as we can */
	if (offset <= 0)
		return;

	if (wp != NULL)			/* grow window wp by offset lines */
		win_new_height(wp, wp->w_height + offset);

	if (up)
		wp = curwin;				/* current window gets smaller */
	else
		wp = curwin->w_next;		/* next window gets smaller */

	while (wp != NULL && offset > 0)
	{
		if (wp->w_height - offset < MIN_ROWS)
		{
			offset -= wp->w_height - MIN_ROWS;
			win_new_height(wp, MIN_ROWS);
		}
		else
		{
			win_new_height(wp, wp->w_height - offset);
			offset = 0;
		}
		if (up)
			wp = wp->w_prev;
		else
			wp = wp->w_next;
	}
	row = win_comp_pos();
	screen_fill(row, cmdline_row, 0, (int)Columns, ' ', ' ');
	cmdline_row = row;
	p_ch = Rows - cmdline_row;
	updateScreen(NOT_VALID);
	showmode();
}
#endif /* USE_MOUSE */

/*
 * Set new window height.
 */
	static void
win_new_height(wp, height)
	WIN		*wp;
	int		height;
{
	/* should adjust topline to keep cursor at same relative postition */

	wp->w_height = height;
	win_comp_scroll(wp);
	wp->w_redr_type = NOT_VALID;
	wp->w_redr_status = TRUE;
}

	void
win_comp_scroll(wp)
	WIN		*wp;
{
	wp->w_p_scroll = (wp->w_height >> 1);
	if (wp->w_p_scroll == 0)
		wp->w_p_scroll = 1;
}

/*
 * command_height: called whenever p_ch has been changed
 */
	void
command_height()
{
	int		current;

	current = Rows - cmdline_row;
	if (p_ch > current)				/* p_ch got bigger */
	{
		if (lastwin->w_height - (p_ch - current) < MIN_ROWS)
		{
			emsg(e_noroom);
			p_ch = lastwin->w_height - MIN_ROWS + current;
		}
		/* clear the lines added to cmdline */
		screen_fill((int)(Rows - p_ch), (int)Rows, 0, (int)Columns, ' ', ' ');
	}
	win_new_height(lastwin, lastwin->w_height + current - (int)p_ch);
	cmdline_row = Rows - p_ch;
	redraw_cmdline = TRUE;
}

	void
last_status()
{
	if (lastwin->w_status_height)
	{
					/* remove status line */
		if (p_ls == 0 || (p_ls == 1 && firstwin == lastwin))
		{
			win_new_height(lastwin, lastwin->w_height + 1);
			lastwin->w_status_height = 0;
		}
	}
	else
	{
					/* add status line */
		if (p_ls == 2 || (p_ls == 1 && firstwin != lastwin))
		{
			if (lastwin->w_height <= MIN_ROWS)		/* can't do it */
				emsg(e_noroom);
			else
			{
				win_new_height(lastwin, lastwin->w_height - 1);
				lastwin->w_status_height = 1;
			}
		}
	}
}

/*
 * file_name_at_cursor()
 *
 * Return the name of the file under (or to the right of) the cursor.  The
 * p_path variable is searched if the file name does not start with '/'.
 * The string returned has been alloc'ed and should be freed by the caller.
 * NULL is returned if the file name or file is not found.
 */
	char_u *
file_name_at_cursor(options)
	int		options;
{
	return get_file_name_in_path(ml_get_curline(),
											   curwin->w_cursor.col, options);
}
/* options:
 * FNAME_MESS		give error messages
 * FNAME_EXP		expand to path
 * FNAME_HYP		check for hypertext link
 */
	char_u *
get_file_name_in_path(ptr, col, options)
	char_u	*ptr;
	int		col;
	int		options;
{
	char_u	*dir;
	char_u	*file_name;
	char_u	save_char;
	char_u	*curr_path = NULL;
	int		curr_path_len;
	int		len;

		/* search forward for what could be the start of a file name */
	while (ptr[col] != NUL && !isfilechar(ptr[col]))
		++col;
	if (ptr[col] == NUL)			/* nothing found */
	{
		if (options & FNAME_MESS)
			EMSG("No file name under cursor");
		return NULL;
	}

		/* search backward for char that cannot be in a file name */
	while (col >= 0 && isfilechar(ptr[col]))
		--col;
	ptr += col + 1;
	col = 0;

		/* search forward for a char that cannot be in a file name */
	while (isfilechar(ptr[col]))
		++col;

	if (options & FNAME_HYP)
	{
		/* For hypertext links, ignore the name of the machine.
		 * Such a link looks like "type://machine/path". Only "/path" is used.
		 * First search for the string "://", then for the extra '/'
		 */
		if ((file_name = vim_strchr(ptr, ':')) != NULL &&
				STRNCMP(file_name, "://", (size_t)3) == 0 &&
				(file_name = vim_strchr(file_name + 3, '/')) != NULL &&
				file_name < ptr + col)
		{
			col -= file_name - ptr;
			ptr = file_name;
			if (ptr[1] == '~')		/* skip '/' for /~user/path */
			{
				++ptr;
				--col;
			}
		}
	}

	if (!(options & FNAME_EXP))
		return strnsave(ptr, col);

		/* copy file name into NameBuff, expanding environment variables */
	save_char = ptr[col];
	ptr[col] = NUL;
	expand_env(ptr, NameBuff, MAXPATHL);
	ptr[col] = save_char;

	if (isFullName(NameBuff))			/* absolute path */
	{
		if ((file_name = strsave(NameBuff)) == NULL)
			return NULL;
		if (getperm(file_name) >= 0)
			return file_name;
		if (options & FNAME_MESS)
		{
			sprintf((char *)IObuff, "Can't find file `%s'", NameBuff);
			emsg(IObuff);
		}
	}
	else							/* relative path, use 'path' option */
	{
		if (curbuf->b_xfilename != NULL)
		{
			curr_path = curbuf->b_xfilename;
			ptr = gettail(curr_path);
			curr_path_len = ptr - curr_path;
		}
		else
			curr_path_len = 0;
		if ((file_name = alloc((int)(curr_path_len + STRLEN(p_path) +
											STRLEN(NameBuff) + 3))) == NULL)
			return NULL;

		for (dir = p_path; *dir;)
		{
			len = copy_option_part(&dir, file_name, 31000, " ,");
			/* len == 0 means: use current directory */
			if (len != 0)
			{
								/* Look for file relative to current file */
				if (file_name[0] == '.' && curr_path_len > 0 &&
										(len == 1 || ispathsep(file_name[1])))
				{
					if (len == 1)		/* just a "." */
						len = 0;
					else				/* "./path": move "path" */
					{
						len -= 2;
						vim_memmove(file_name + curr_path_len, file_name + 2,
																 (size_t)len);
					}
					STRNCPY(file_name, curr_path, curr_path_len);
					len += curr_path_len;
				}
				if (!ispathsep(file_name[len - 1]))
					file_name[len++] = PATHSEP;
			}
			STRCPY(file_name + len, NameBuff);
			if (getperm(file_name) >= 0)
				return file_name;
		}
		if (options & FNAME_MESS)
			EMSG2("Can't find file \"%s\" in path", NameBuff);
	}
	vim_free(file_name);			/* file doesn't exist */
	return NULL;
}

/*
 * Return the minimal number of rows that is needed on the screen to display
 * the current number of windows.
 */
	int
min_rows()
{
	WIN		*wp;
	int		total;

	if (firstwin == NULL)		/* not initialized yet */
		return MIN_ROWS + 1;	/* one window plus command line */

	total = p_ch;		/* count the room for the status line */
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		total += MIN_ROWS + wp->w_status_height;
	return total;
}

/*
 * Return TRUE if there is only one window, not counting a help window, unless
 * it is the current window.
 */
	int
only_one_window()
{
	int		count = 0;
	WIN		*wp;

	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (!wp->w_buffer->b_help || wp == curwin)
			++count;
	return (count <= 1);
}

/*	$OpenBSD: message.c,v 1.2 1996/09/21 06:23:09 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * message.c: functions for displaying messages on the command line
 */

#include "vim.h"
#include "globals.h"
#define MESSAGE			/* don't include prototype for smsg() */
#include "proto.h"
#include "option.h"
#ifdef __QNX__
# include <stdarg.h>
#endif

static void msg_screen_outchar __ARGS((int c));
static int msg_check_screen __ARGS((void));

static int	lines_left = -1;			/* lines left for listing */

/*
 * msg(s) - displays the string 's' on the status line
 * When terminal not initialized (yet) fprintf(stderr,..) is used.
 * return TRUE if wait_return not called
 */
	int
msg(s)
	char_u		   *s;
{
	msg_start();
	if (msg_highlight)
		start_highlight();
	msg_outtrans(s);
	if (msg_highlight)
	{
		stop_highlight();
		msg_highlight = FALSE;		/* clear for next call */
	}
	msg_clr_eos();
	return msg_end();
}

/*
 * automatic prototype generation does not understand this function
 */
#ifndef PROTO
#ifndef __QNX__
int smsg __ARGS((char_u *, long, long, long,
						long, long, long, long, long, long, long));

/* VARARGS */
	int
smsg(s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
	char_u		*s;
	long		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
{
	sprintf((char *)IObuff, (char *)s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	return msg(IObuff);
}
#else /* __QNX__ */
void smsg(char_u *s, ...)
{
	va_list arglist;
	va_start(arglist, s);
	vsprintf((char *)IObuff, (char *)s, arglist);
	va_end(arglist);
	msg(IObuff);
}
#endif /* __QNX__ */
#endif

/*
 * emsg() - display an error message
 *
 * Rings the bell, if appropriate, and calls message() to do the real work
 * When terminal not initialized (yet) fprintf(stderr,..) is used.
 *
 * return TRUE if wait_return not called
 */
	int
emsg(s)
	char_u		   *s;
{
	char_u			*Buf;
#ifdef SLEEP_IN_EMSG
	int				retval;
#endif
	static int		last_lnum = 0;
	static char_u	*last_sourcing_name = NULL;

	if (emsg_off)				/* no error messages at the moment */
		return TRUE;

	if (global_busy)			/* break :global command */
		++global_busy;

	if (p_eb)
		beep_flush();			/* also includes flush_buffers() */
	else
		flush_buffers(FALSE);	/* flush internal buffers */
	did_emsg = TRUE;			/* flag for DoOneCmd() */
	++msg_scroll;				/* don't overwrite a previous message */
	(void)set_highlight('e');	/* set highlight mode for error messages */
	msg_highlight = TRUE;
	if (msg_scrolled)
		need_wait_return = TRUE;	/* needed in case emsg() is called after
									 * wait_return has reset need_wait_return
									 * and a redraw is expected because
									 * msg_scrolled is non-zero */

/*
 * First output name and line number of source of error message
 */
	if (sourcing_name != NULL &&
		   (sourcing_name != last_sourcing_name || sourcing_lnum != last_lnum)
									  && (Buf = alloc(MAXPATHL + 30)) != NULL)
	{
		++no_wait_return;
		if (sourcing_name != last_sourcing_name)
		{
			sprintf((char *)Buf, "Error detected while processing %s:",
											sourcing_name);
			msg(Buf);
			msg_highlight = TRUE;
		}
			/* lnum is 0 when executing a command from the command line
			 * argument, we don't want a line number then */
		if (sourcing_lnum != 0)
		{
			(void)set_highlight('n');	/* highlight mode for line numbers */
			sprintf((char *)Buf, "line %4ld:", sourcing_lnum);
			msg(Buf);
			(void)set_highlight('e');	/* highlight mode for error messages */
			msg_highlight = TRUE;
		}
		--no_wait_return;
		last_lnum = sourcing_lnum;	/* only once for each line */
		vim_free(Buf);
	}
	last_sourcing_name = sourcing_name;	/* do this also when it is NULL */

#ifdef SLEEP_IN_EMSG
/*
 * Msg returns TRUE if wait_return() was not called.
 * In that case may call sleep() to give the user a chance to read the message.
 * Don't call sleep() if dont_sleep is set.
 */
	retval = msg(s);
	if (retval)
	{
		if (dont_sleep || need_wait_return)
			need_sleep = TRUE;	/* sleep before removing the message */
		else
			mch_delay(1000L, TRUE);	/* give user chance to read message */
	}
	/* --msg_scroll;			don't overwrite this message */
	return retval;
#else
	emsg_on_display = TRUE;		/* remember there is an error message */
	return msg(s);
#endif
}

	int
emsg2(s, a1)
	char_u *s, *a1;
{
	/* Check for NULL strings (just in case) */
	if (a1 == NULL)
		a1 = (char_u *)"[NULL]";
	/* Check for very long strings (can happen with ":help ^A<CR>") */
	if (STRLEN(s) + STRLEN(a1) >= (size_t)IOSIZE)
		a1 = (char_u *)"[string too long]";
	sprintf((char *)IObuff, (char *)s, (char *)a1);
	return emsg(IObuff);
}

	int
emsgn(s, n)
	char_u *s;
	long    n;
{
	sprintf((char *)IObuff, (char *)s, n);
	return emsg(IObuff);
}

/*
 * Like msg(), but truncate to a single line if p_shm contains 't'.
 * Careful: The string may be changed!
 */
	int
msg_trunc(s)
	char_u	*s;
{
	int		n;

	if (shortmess(SHM_TRUNC) && (n = (int)STRLEN(s) -
					(int)(Rows - cmdline_row - 1) * Columns - sc_col + 1) > 0)
	{
		s[n] = '<';
		return msg(s + n);
	}
	else
		return msg(s);
}

/*
 * wait for the user to hit a key (normally a return)
 * if 'redraw' is TRUE, clear and redraw the screen
 * if 'redraw' is FALSE, just redraw the screen
 * if 'redraw' is -1, don't redraw at all
 */
	void
wait_return(redraw)
	int		redraw;
{
	int				c;
	int				oldState;
	int				tmpState;

	if (redraw == TRUE)
		must_redraw = CLEAR;

/*
 * With the global command (and some others) we only need one return at the
 * end. Adjust cmdline_row to avoid the next message overwriting the last one.
 */
	if (no_wait_return)
	{
		need_wait_return = TRUE;
		cmdline_row = msg_row;
		return;
	}
	oldState = State;
	if (quit_more)
	{
		c = CR;						/* just pretend CR was hit */
		quit_more = FALSE;
		got_int = FALSE;
	}
	else
	{
		State = HITRETURN;
#ifdef USE_MOUSE
		setmouse();
#endif
		if (msg_didout)				/* start on a new line */
			msg_outchar('\n');
		if (got_int)
			MSG_OUTSTR("Interrupt: ");

		(void)set_highlight('r');
		start_highlight();
#ifdef ORG_HITRETURN
		MSG_OUTSTR("Press RETURN to continue");
		stop_highlight();
		do {
			c = vgetc();
		} while (vim_strchr((char_u *)"\r\n: ", c) == NULL);
		if (c == ':')			 		/* this can vi too (but not always!) */
			stuffcharReadbuff(c);
#else
		MSG_OUTSTR("Press RETURN or enter command to continue");
		stop_highlight();
		do
		{
			c = vgetc();
			got_int = FALSE;
		} while (c == Ctrl('C')
#ifdef USE_GUI
								|| c == K_SCROLLBAR || c == K_HORIZ_SCROLLBAR
#endif
#ifdef USE_MOUSE
								|| c == K_LEFTDRAG   || c == K_LEFTRELEASE
								|| c == K_MIDDLEDRAG || c == K_MIDDLERELEASE
								|| c == K_RIGHTDRAG  || c == K_RIGHTRELEASE
								|| c == K_IGNORE	 ||
								(!mouse_has(MOUSE_RETURN) &&
									 (c == K_LEFTMOUSE ||
									  c == K_MIDDLEMOUSE ||
									  c == K_RIGHTMOUSE))
#endif
								);
		mch_breakcheck();
#ifdef USE_MOUSE
		/*
		 * Avoid that the mouse-up event causes visual mode to start.
		 */
		if (c == K_LEFTMOUSE || c == K_MIDDLEMOUSE || c == K_RIGHTMOUSE)
			jump_to_mouse(MOUSE_SETPOS);
		else
#endif
			if (vim_strchr((char_u *)"\r\n ", c) == NULL)
		{
			stuffcharReadbuff(c);
			do_redraw = TRUE;		/* need a redraw even though there is
									   something in the stuff buffer */
		}
#endif
	}

	/*
	 * If the user hits ':', '?' or '/' we get a command line from the next
	 * line.
	 */
	if (c == ':' || c == '?' || c == '/')
	{
		cmdline_row = msg_row;
		skip_redraw = TRUE;			/* skip redraw once */
		do_redraw = FALSE;
	}

/*
 * If the window size changed set_winsize() will redraw the screen.
 * Otherwise the screen is only redrawn if 'redraw' is set and no ':' typed.
 */
	tmpState = State;
	State = oldState;				/* restore State before set_winsize */
#ifdef USE_MOUSE
	setmouse();
#endif
	msg_check();

	need_wait_return = FALSE;
	emsg_on_display = FALSE;	/* can delete error message now */
#ifdef SLEEP_IN_EMSG
	need_sleep = FALSE;			/* no need to call sleep() anymore */
#endif
	msg_didany = FALSE;			/* reset lines_left at next msg_start() */
	lines_left = -1;
	if (keep_msg != NULL && linetabsize(keep_msg) >=
								  (Rows - cmdline_row - 1) * Columns + sc_col)
		keep_msg = NULL;			/* don't redisplay message, it's too long */

	if (tmpState == SETWSIZE)		/* got resize event while in vgetc() */
	{
		starttermcap();				/* start termcap before redrawing */
		set_winsize(0, 0, FALSE);
	}
	else if (!skip_redraw && (redraw == TRUE || (msg_scrolled && redraw != -1)))
	{
		starttermcap();				/* start termcap before redrawing */
		updateScreen(VALID);
	}

	dont_wait_return = TRUE;		/* don't wait again in main() */
}

/*
 * Prepare for outputting characters in the command line.
 */
	void
msg_start()
{
	keep_msg = NULL;						/* don't display old message now */
	keep_msg_highlight = 0;
	if (!msg_scroll && full_screen)			/* overwrite last message */
		msg_pos(cmdline_row, 0);
	else if (msg_didout)					/* start message on next line */
	{
		msg_outchar('\n');
		cmdline_row = msg_row;
	}
	if (!msg_didany)
		lines_left = cmdline_row;
	msg_didout = FALSE;						/* no output on current line yet */
	cursor_off();
}

/*
 * Move message position. This should always be used after moving the cursor.
 * Use negative value if row or col does not have to be changed.
 */
	void
msg_pos(row, col)
	int		row, col;
{
	if (row >= 0)
		msg_row = row;
	if (col >= 0)
		msg_col = col;
}

	void
msg_outchar(c)
	int		c;
{
	char_u		buf[4];

	if (IS_SPECIAL(c))
	{
		buf[0] = K_SPECIAL;
		buf[1] = K_SECOND(c);
		buf[2] = K_THIRD(c);
		buf[3] = NUL;
	}
	else
	{
		buf[0] = c;
		buf[1] = NUL;
	}
	msg_outstr(buf);
}

	void
msg_outnum(n)
	long		n;
{
	char_u		buf[20];

	sprintf((char *)buf, "%ld", n);
	msg_outstr(buf);
}

	void
msg_home_replace(fname)
	char_u	*fname;
{
	char_u		*name;

	name = home_replace_save(NULL, fname);
	if (name != NULL)
		msg_outtrans(name);
	vim_free(name);
}

/*
 * output 'len' characters in 'str' (including NULs) with translation
 * if 'len' is -1, output upto a NUL character
 * return the number of characters it takes on the screen
 */
	int
msg_outtrans(str)
	register char_u *str;
{
	return msg_outtrans_len(str, (int)STRLEN(str));
}

	int
msg_outtrans_len(str, len)
	register char_u *str;
	register int   len;
{
	int retval = 0;

	while (--len >= 0)
	{
		msg_outstr(transchar(*str));
		retval += charsize(*str);
		++str;
	}
	return retval;
}

/*
 * Output the string 'str' upto a NUL character.
 * Return the number of characters it takes on the screen.
 *
 * If K_SPECIAL is encountered, then it is taken in conjunction with the
 * following character and shown as <F1>, <S-Up> etc.  In addition, if 'all'
 * is TRUE, then any other character which has its 8th bit set is shown as
 * <M-x>, where x is the equivalent character without its 8th bit set.  If a
 * character is displayed in one of these special ways, is also highlighted
 * (its highlight name is '8' in the p_hl variable).
 * This function is used to show mappings, where we want to see how to type
 * the character/string -- webb
 */
	int
msg_outtrans_special(str, all)
	register char_u *str;
	register int	all;	/* <M-a> etc as well as <F1> etc */
{
	int		retval = 0;
	char_u	*string;
	int		c;
	int		modifiers;

	set_highlight('8');
	for (; *str; ++str)
	{
		c = *str;
		if (c == K_SPECIAL && str[1] != NUL && str[2] != NUL)
		{
			modifiers = 0x0;
			if (str[1] == KS_MODIFIER)
			{
				modifiers = str[2];
				str += 3;
				c = *str;
			}
			if (c == K_SPECIAL)
			{
				c = TO_SPECIAL(str[1], str[2]);
				str += 2;
				if (c == K_ZERO)		/* display <Nul> as ^@ */
					c = NUL;
			}
			if (IS_SPECIAL(c) || modifiers)		/* special key */
			{
				string = get_special_key_name(c, modifiers);
				start_highlight();
				msg_outstr(string);
				retval += STRLEN(string);
				stop_highlight();
				flushbuf();			/* Otherwise gets overwritten by spaces */
				continue;
			}
		}
		if ((c & 0x80) && all)
		{
			start_highlight();
			MSG_OUTSTR("<M-");
			msg_outstr(transchar(c & 0x7f));
			retval += 2 + charsize(c & 0x7f);
			MSG_OUTSTR(">");
			stop_highlight();
		}
		else
		{
			msg_outstr(transchar(c));
			retval += charsize(c);
		}
	}
	return retval;
}

/*
 * print line for :p command
 */
	void
msg_prt_line(s)
	char_u		   *s;
{
	register int	si = 0;
	register int	c;
	register int	col = 0;

	int 			n_extra = 0;
	int             n_spaces = 0;
	char_u			*p = NULL;			/* init to make SASC shut up */
	int 			n;

	for (;;)
	{
		if (n_extra)
		{
			--n_extra;
			c = *p++;
		}
		else if (n_spaces)
		{
		    --n_spaces;
			c = ' ';
		}
		else
		{
			c = s[si++];
			if (c == TAB && !curwin->w_p_list)
			{
				/* tab amount depends on current column */
				n_spaces = curbuf->b_p_ts - col % curbuf->b_p_ts - 1;
				c = ' ';
			}
			else if (c == NUL && curwin->w_p_list)
			{
				p = (char_u *)"";
				n_extra = 1;
				c = '$';
			}
			else if (c != NUL && (n = charsize(c)) > 1)
			{
				n_extra = n - 1;
				p = transchar(c);
				c = *p++;
			}
		}

		if (c == NUL)
			break;

		msg_outchar(c);
		col++;
	}
}

/*
 * output a string to the screen at position msg_row, msg_col
 * Update msg_row and msg_col for the next message.
 */
	void
msg_outstr(s)
	char_u		*s;
{
	int		oldState;
	char_u	buf[20];

	/*
	 * If there is no valid screen, use fprintf so we can see error messages.
	 * If termcap is not active, we may be writing in an alternate console
	 * window, cursor positioning may not work correctly (window size may be
	 * different, e.g. for WIN32 console).
	 */
	if (!msg_check_screen()
#ifdef WIN32
							|| !termcap_active
#endif
												)
	{
#ifdef WIN32
		mch_settmode(0);	/* cook so that \r and \n are handled correctly */
#endif
		fprintf(stderr, (char *)s);
		msg_didout = TRUE;			/* assume that line is not empty */
#ifdef WIN32
		mch_settmode(1);
#endif
		return;
	}

	msg_didany = TRUE;			/* remember that something was outputted */
	while (*s)
	{
		/*
		 * The screen is scrolled up when:
		 * - When outputting a newline in the last row
		 * - when outputting a character in the last column of the last row
		 *   (some terminals scroll automatically, some don't. To avoid
		 *   problems we scroll ourselves)
		 */
		if (msg_row >= Rows - 1 && (*s == '\n' || msg_col >= Columns - 1 ||
							  (*s == TAB && msg_col >= ((Columns - 1) & ~7))))
		{
			screen_del_lines(0, 0, 1, (int)Rows, TRUE);	/* always works */
			msg_row = Rows - 2;
			if (msg_col >= Columns)		/* can happen after screen resize */
				msg_col = Columns - 1;
			++msg_scrolled;
			need_wait_return = TRUE;	/* may need wait_return in main() */
			if (cmdline_row > 0)
				--cmdline_row;
			/*
			 * if screen is completely filled wait for a character
			 */
			if (p_more && --lines_left == 0 && State != HITRETURN)
			{
				oldState = State;
				State = ASKMORE;
#ifdef USE_MOUSE
				setmouse();
#endif
				msg_moremsg(FALSE);
				for (;;)
				{
					/*
					 * Get a typed character directly from the user.
					 * Don't use vgetc(), it syncs undo and eats mapped
					 * characters.  Disadvantage: Special keys and mouse
					 * cannot be used here, typeahead is ignored.
					 */
					flushbuf();
					(void)mch_inchar(buf, 20, -1L);
					switch (buf[0])
					{
					case CR:			/* one extra line */
					case NL:
						lines_left = 1;
						break;
					case ':':			/* start new command line */
						stuffcharReadbuff(':');
						cmdline_row = Rows - 1;		/* put ':' on this line */
						skip_redraw = TRUE;			/* skip redraw once */
						dont_wait_return = TRUE;	/* don't wait in main() */
						/*FALLTHROUGH*/
					case 'q':			/* quit */
					case Ctrl('C'):
						got_int = TRUE;
						quit_more = TRUE;
						break;
					case 'd':			/* Down half a page */
						lines_left = Rows / 2;
						break;
					case ' ':			/* one extra page */
						lines_left = Rows - 1;
						break;
					default:			/* no valid response */
						msg_moremsg(TRUE);
						continue;
					}
					break;
				}
				/* clear the --more-- message */
				screen_fill((int)Rows - 1, (int)Rows,
												   0, (int)Columns, ' ', ' ');
				State = oldState;
#ifdef USE_MOUSE
				setmouse();
#endif
				if (quit_more)
				{
					msg_row = Rows - 1;
					msg_col = 0;
					return;			/* the string is not displayed! */
				}
			}
		}
		if (*s == '\n')				/* go to next line */
		{
			msg_didout = FALSE;		/* remember that line is empty */
			msg_col = 0;
			if (++msg_row >= Rows)	/* safety check */
				msg_row = Rows - 1;
		}
		else if (*s == '\r')		/* go to column 0 */
		{
			msg_col = 0;
		}
		else if (*s == '\b')		/* go to previous char */
		{
			if (msg_col)
				--msg_col;
		}
		else if (*s == TAB)			/* translate into spaces */
		{
			do
				msg_screen_outchar(' ');
			while (msg_col & 7);
		}
		else
			msg_screen_outchar(*s);
		++s;
	}
}

	static void
msg_screen_outchar(c)
	int		c;
{
	msg_didout = TRUE;		/* remember that line is not empty */
	screen_outchar(c, msg_row, msg_col);
	if (++msg_col >= Columns)
	{
		msg_col = 0;
		++msg_row;
	}
}

	void
msg_moremsg(full)
	int		full;
{
	/*
	 * Need to restore old highlighting when we've finished with it
	 * because the output that's paging may be relying on it not
	 * changing -- webb
	 */
	remember_highlight();
	set_highlight('m');
	start_highlight();
	screen_msg((char_u *)"-- More --", (int)Rows - 1, 0);
	if (full)
		screen_msg((char_u *)" (RET: line, SPACE: page, d: half page, q: quit)",
														   (int)Rows - 1, 10);
	stop_highlight();
	recover_old_highlight();
}

/*
 * msg_check_screen - check if the screen is initialized.
 * Also check msg_row and msg_col, if they are too big it may cause a crash.
 */
	static int
msg_check_screen()
{
	if (!full_screen || !screen_valid(FALSE))
		return FALSE;
	
	if (msg_row >= Rows)
		msg_row = Rows - 1;
	if (msg_col >= Columns)
		msg_col = Columns - 1;
	return TRUE;
}

/*
 * clear from current message position to end of screen
 * Note: msg_col is not updated, so we remember the end of the message
 * for msg_check().
 */
	void
msg_clr_eos()
{
	if (!msg_check_screen()
#ifdef WIN32
							|| !termcap_active
#endif
												)
		return;
	screen_fill(msg_row, msg_row + 1, msg_col, (int)Columns, ' ', ' ');
	screen_fill(msg_row + 1, (int)Rows, 0, (int)Columns, ' ', ' ');
}

/*
 * end putting a message on the screen
 * call wait_return if the message does not fit in the available space
 * return TRUE if wait_return not called.
 */
	int
msg_end()
{
	/*
	 * if the string is larger than the window,
	 * or the ruler option is set and we run into it,
	 * we have to redraw the window.
	 * Do not do this if we are abandoning the file or editing the command line.
	 */
	if (!exiting && msg_check() && State != CMDLINE)
	{
		wait_return(FALSE);
		return FALSE;
	}
	flushbuf();
	return TRUE;
}

/*
 * If the written message has caused the screen to scroll up, or if we
 * run into the shown command or ruler, we have to redraw the window later.
 */
	int
msg_check()
{
	if (msg_scrolled || (msg_row == Rows - 1 && msg_col >= sc_col))
	{
		redraw_later(NOT_VALID);
		redraw_cmdline = TRUE;
		return TRUE;
	}
	return FALSE;
}

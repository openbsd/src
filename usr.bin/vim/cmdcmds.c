/*	$OpenBSD: cmdcmds.c,v 1.4 1996/09/24 17:53:49 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * cmdcmds.c: functions for command line commands
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

static void do_filter __ARGS((linenr_t line1, linenr_t line2,
									char_u *buff, int do_in, int do_out));
#ifdef VIMINFO
static char_u *viminfo_filename __ARGS((char_u 	*));
static void do_viminfo __ARGS((FILE *fp_in, FILE *fp_out, int want_info,
											 int want_marks, int force_read));
static int read_viminfo_up_to_marks __ARGS((char_u *line, FILE *fp,
																int forceit));
#endif /* VIMINFO */

	void
do_ascii()
{
	int		c;
	char	buf1[20];
	char	buf2[20];
	char_u	buf3[3];

	c = gchar_cursor();
	if (c == NUL)
	{
		MSG("empty line");
		return;
	}
	if (c == NL)			/* NUL is stored as NL */
		c = NUL;
	if (isprintchar(c) && (c < ' ' || c > '~'))
	{
		transchar_nonprint(buf3, c);
		sprintf(buf1, "  <%s>", (char *)buf3);
	}
	else
		buf1[0] = NUL;
	if (c >= 0x80)
		sprintf(buf2, "  <M-%s>", transchar(c & 0x7f));
	else
		buf2[0] = NUL;
	sprintf((char *)IObuff, "<%s>%s%s  %d,  Hex %02x,  Octal %03o",
										   transchar(c), buf1, buf2, c, c, c);
	msg(IObuff);
}

/*
 * align text:
 * type = -1  left aligned
 * type = 0   centered
 * type = 1   right aligned
 */
	void
do_align(start, end, width, type)
	linenr_t	start;
	linenr_t	end;
	int			width;
	int			type;
{
	FPOS	pos;
	int		len;
	int		indent = 0;
	int		new_indent = 0;			/* init for GCC */
	char_u	*first;
	char_u	*last;
	int		save;

#ifdef RIGHTLEFT
	if (curwin->w_p_rl)
		type = -type;	/* switch left and right aligning */
#endif

	pos = curwin->w_cursor;
	if (type == -1)		/* left align: width is used for new indent */
	{
		if (width >= 0)
			indent = width;
	}
	else
	{
		/*
		 * if 'textwidth' set, use it
		 * else if 'wrapmargin' set, use it
		 * if invalid value, use 80
		 */
		if (width <= 0)
			width = curbuf->b_p_tw;
		if (width == 0 && curbuf->b_p_wm > 0)
			width = Columns - curbuf->b_p_wm;
		if (width <= 0)
			width = 80;
	}

	if (u_save((linenr_t)(start - 1), (linenr_t)(end + 1)) == FAIL)
		return;
	for (curwin->w_cursor.lnum = start;
						curwin->w_cursor.lnum <= end; ++curwin->w_cursor.lnum)
	{
			/* find the first non-blank character */
		first = skipwhite(ml_get_curline());
			/* find the character after the last non-blank character */
		for (last = first + STRLEN(first);
								last > first && vim_iswhite(last[-1]); --last)
			;
		save = *last;
		*last = NUL;
		len = linetabsize(first);					/* get line length */
		*last = save;
		if (len == 0)								/* skip blank lines */
			continue;
		switch (type)
		{
			case -1:	new_indent = indent;			/* left align */
						break;
			case 0:		new_indent = (width - len) / 2;	/* center */
						break;
			case 1:		new_indent = width - len;		/* right align */
						break;
		}
		if (new_indent < 0)
			new_indent = 0;
		set_indent(new_indent, TRUE);			/* set indent */
	}
	curwin->w_cursor = pos;
	beginline(TRUE);
	updateScreen(NOT_VALID);
}

	void
do_retab(start, end, new_ts, forceit)
	linenr_t	start;
	linenr_t	end;
	int			new_ts;
	int			forceit;
{
	linenr_t	lnum;
	int			got_tab = FALSE;
	long		num_spaces = 0;
	long		num_tabs;
	long		len;
	long		col;
	long		vcol;
	long		start_col = 0;			/* For start of white-space string */
	long		start_vcol = 0;			/* For start of white-space string */
	int			temp;
	long		old_len;
	char_u		*ptr;
	char_u		*new_line = (char_u *)1;	/* init to non-NULL */
	int			did_something = FALSE;
	int			did_undo;				/* called u_save for current line */

	if (new_ts == 0)
		new_ts = curbuf->b_p_ts;
	for (lnum = start; !got_int && lnum <= end; ++lnum)
	{
		ptr = ml_get(lnum);
		col = 0;
		vcol = 0;
		did_undo = FALSE;
		for (;;)
		{
			if (vim_iswhite(ptr[col]))
			{
				if (!got_tab && num_spaces == 0)
				{
					/* First consecutive white-space */
					start_vcol = vcol;
					start_col = col;
				}
				if (ptr[col] == ' ')
					num_spaces++;
				else
					got_tab = TRUE;
			}
			else
			{
				if (got_tab || (forceit && num_spaces > 1))
				{
					/* Retabulate this string of white-space */

					/* len is virtual length of white string */
					len = num_spaces = vcol - start_vcol;
					num_tabs = 0;
					if (!curbuf->b_p_et)
					{
						temp = new_ts - (start_vcol % new_ts);
						if (num_spaces >= temp)
						{
							num_spaces -= temp;
							num_tabs++;
						}
						num_tabs += num_spaces / new_ts;
						num_spaces -= (num_spaces / new_ts) * new_ts;
					}
					if (curbuf->b_p_et || got_tab ||
										(num_spaces + num_tabs < len))
					{
						if (did_undo == FALSE)
						{
							did_undo = TRUE;
							if (u_save((linenr_t)(lnum - 1),
												(linenr_t)(lnum + 1)) == FAIL)
							{
								new_line = NULL;		/* flag out-of-memory */
								break;
							}
						}

						/* len is actual number of white characters used */
						len = num_spaces + num_tabs;
						old_len = STRLEN(ptr);
						new_line = lalloc(old_len - col + start_col + len + 1,
																		TRUE);
						if (new_line == NULL)
							break;
						if (start_col > 0)
							vim_memmove(new_line, ptr, (size_t)start_col);
						vim_memmove(new_line + start_col + len,
									  ptr + col, (size_t)(old_len - col + 1));
						ptr = new_line + start_col;
						for (col = 0; col < len; col++)
							ptr[col] = (col < num_tabs) ? '\t' : ' ';
						ml_replace(lnum, new_line, FALSE);
						did_something = TRUE;
						ptr = new_line;
						col = start_col + len;
					}
				}
				got_tab = FALSE;
				num_spaces = 0;
			}
			if (ptr[col] == NUL)
				break;
			vcol += chartabsize(ptr[col++], (colnr_t)vcol);
		}
		if (new_line == NULL)				/* out of memory */
			break;
		line_breakcheck();
	}
	if (got_int)
		emsg(e_interr);
	if (did_something)
		CHANGED;
	curbuf->b_p_ts = new_ts;
	coladvance(curwin->w_curswant);
}

/*
 * :move command - move lines line1-line2 to line n
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_move(line1, line2, n)
	linenr_t	line1;
	linenr_t	line2;
	linenr_t	n;
{
	char_u		*str;
	linenr_t	l;
	linenr_t	extra;		/* Num lines added before line1 */
	linenr_t	num_lines;	/* Num lines moved */
	linenr_t	last_line;	/* Last line in file after adding new text */
	int			has_mark;

	if (n >= line1 && n < line2)
	{
		EMSG("Move lines into themselves");
		return FAIL;
	}

	num_lines = line2 - line1 + 1;

	/*
	 * First we copy the old text to its new location -- webb
	 */
	if (u_save(n, n + 1) == FAIL)
		return FAIL;
	for (extra = 0, l = line1; l <= line2; l++)
	{
		str = strsave(ml_get(l + extra));
		if (str != NULL)
		{
			has_mark = ml_has_mark(l + extra);
			ml_append(n + l - line1, str, (colnr_t)0, FALSE);
			vim_free(str);
			if (has_mark)
				ml_setmarked(n + l - line1 + 1);
			if (n < line1)
				extra++;
		}
	}

	/*
	 * Now we must be careful adjusting our marks so that we don't overlap our
	 * mark_adjust() calls.
	 *
	 * We adjust the marks within the old text so that they refer to the
	 * last lines of the file (temporarily), because we know no other marks
	 * will be set there since these line numbers did not exist until we added
	 * our new lines.
	 *
	 * Then we adjust the marks on lines between the old and new text positions
	 * (either forwards or backwards).
	 *
	 * And Finally we adjust the marks we put at the end of the file back to
	 * their final destination at the new text position -- webb
	 */
	last_line = curbuf->b_ml.ml_line_count;
	mark_adjust(line1, line2, last_line - line2, 0L);
	if (n >= line2)
		mark_adjust(line2 + 1, n, -num_lines, 0L);
	else
		mark_adjust(n + 1, line1 - 1, num_lines, 0L);
	mark_adjust(last_line - num_lines + 1, last_line,
												-(last_line - n - extra), 0L);
	
	/*
	 * Now we delete the original text -- webb
	 */
	if (u_save(line1 + extra - 1, line2 + extra + 1) == FAIL)
		return FAIL;

	for (l = line1; l <= line2; l++)
		ml_delete(line1 + extra, TRUE);

	CHANGED;
	if (!global_busy && num_lines > p_report)
		smsg((char_u *)"%ld line%s moved", num_lines, plural(num_lines));

	/*
	 * Leave the cursor on the last of the moved lines.
	 */
	if (n >= line1)
		curwin->w_cursor.lnum = n;
	else
		curwin->w_cursor.lnum = n + (line2 - line1) + 1;

	return OK;
}

/*
 * :copy command - copy lines line1-line2 to line n
 */
	void
do_copy(line1, line2, n)
	linenr_t	line1;
	linenr_t	line2;
	linenr_t	n;
{
	linenr_t		lnum;
	char_u			*p;

	mark_adjust(n + 1, MAXLNUM, line2 - line1 + 1, 0L);

	/*
	 * there are three situations:
	 * 1. destination is above line1
	 * 2. destination is between line1 and line2
	 * 3. destination is below line2
	 *
	 * n = destination (when starting)
	 * curwin->w_cursor.lnum = destination (while copying)
	 * line1 = start of source (while copying)
	 * line2 = end of source (while copying)
	 */
	if (u_save(n, n + 1) == FAIL)
		return;
	curwin->w_cursor.lnum = n;
	lnum = line2 - line1 + 1;
	while (line1 <= line2)
	{
		/* need to use strsave() because the line will be unlocked
			within ml_append */
		p = strsave(ml_get(line1));
		if (p != NULL)
		{
			ml_append(curwin->w_cursor.lnum, p, (colnr_t)0, FALSE);
			vim_free(p);
		}
				/* situation 2: skip already copied lines */
		if (line1 == n)
			line1 = curwin->w_cursor.lnum;
		++line1;
		if (curwin->w_cursor.lnum < line1)
			++line1;
		if (curwin->w_cursor.lnum < line2)
			++line2;
		++curwin->w_cursor.lnum;
	}
	CHANGED;
	msgmore((long)lnum);
}

/*
 * Handle the ":!cmd" command.  Also for ":r !cmd" and ":w !cmd"
 * Bangs in the argument are replaced with the previously entered command.
 * Remember the argument.
 */
	void
do_bang(addr_count, line1, line2, forceit, arg, do_in, do_out)
	int			addr_count;
	linenr_t	line1, line2;
	int			forceit;
	char_u		*arg;
	int			do_in, do_out;
{
	static	char_u	*prevcmd = NULL;		/* the previous command */
	char_u			*newcmd = NULL;			/* the new command */
	int				ins_prevcmd;
	char_u			*t;
	char_u			*p;
	char_u			*trailarg;
	int 			len;
	int				scroll_save = msg_scroll;

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		return;
	}

	if (addr_count == 0)				/* :! */
	{
		msg_scroll = FALSE;			/* don't scroll here */
		autowrite_all();
		msg_scroll = scroll_save;
	}

	/*
	 * Try to find an embedded bang, like in :!<cmd> ! [args]
	 * (:!! is indicated by the 'forceit' variable)
	 */
	ins_prevcmd = forceit;
	trailarg = arg;
	do
	{
		len = STRLEN(trailarg) + 1;
		if (newcmd != NULL)
			len += STRLEN(newcmd);
		if (ins_prevcmd)
		{
			if (prevcmd == NULL)
			{
				emsg(e_noprev);
				vim_free(newcmd);
				return;
			}
			len += STRLEN(prevcmd);
		}
		if ((t = alloc(len)) == NULL)
		{
			vim_free(newcmd);
			return;
		}
		*t = NUL;
		if (newcmd != NULL)
			STRCAT(t, newcmd);
		if (ins_prevcmd)
			STRCAT(t, prevcmd);
		p = t + STRLEN(t);
		STRCAT(t, trailarg);
		vim_free(newcmd);
		newcmd = t;

		/*
		 * Scan the rest of the argument for '!', which is replaced by the
		 * previous command.  "\!" is replaced by "!" (this is vi compatible).
		 */
		trailarg = NULL;
		while (*p)
		{
			if (*p == '!')
			{
				if (p > newcmd && p[-1] == '\\')
					vim_memmove(p - 1, p, (size_t)(STRLEN(p) + 1));
				else
				{
					trailarg = p;
					*trailarg++ = NUL;
					ins_prevcmd = TRUE;
					break;
				}
			}
			++p;
		}
	} while (trailarg != NULL);

	vim_free(prevcmd);
	prevcmd = newcmd;

	if (bangredo)			/* put cmd in redo buffer for ! command */
	{
		AppendToRedobuff(prevcmd);
		AppendToRedobuff((char_u *)"\n");
		bangredo = FALSE;
	}
	/*
	 * Add quotes around the command, for shells that need them.
	 */
	if (*p_shq != NUL)
	{
		newcmd = alloc((unsigned)(STRLEN(prevcmd) + 2 * STRLEN(p_shq) + 1));
		if (newcmd == NULL)
			return;
		STRCPY(newcmd, p_shq);
		STRCAT(newcmd, prevcmd);
		STRCAT(newcmd, p_shq);
	}
	if (addr_count == 0)				/* :! */
	{
			/* echo the command */
		msg_start();
		msg_outchar(':');
		msg_outchar('!');
		msg_outtrans(newcmd);
		msg_clr_eos();
		windgoto(msg_row, msg_col);

		do_shell(newcmd); 
	}
	else								/* :range! */
		do_filter(line1, line2, newcmd, do_in, do_out);
	if (newcmd != prevcmd)
		vim_free(newcmd);
}

/*
 * call a shell to execute a command
 */
	void
do_shell(cmd)
	char_u	*cmd;
{
	BUF		*buf;
	int		save_nwr;

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		msg_end();
		return;
	}

#ifdef WIN32
	/*
	 * Check if external commands are allowed now.
	 */
	if (can_end_termcap_mode(TRUE) == FALSE)
		return;
#endif

	/*
	 * For autocommands we want to get the output on the current screen, to
	 * avoid having to type return below.
	 */
	msg_outchar('\r');					/* put cursor at start of line */
#ifdef AUTOCMD
	if (!autocmd_busy)
#endif
		stoptermcap();
	msg_outchar('\n');					/* may shift screen one line up */

		/* warning message before calling the shell */
	if (p_warn
#ifdef AUTOCMD
				&& !autocmd_busy
#endif
								   )
		for (buf = firstbuf; buf; buf = buf->b_next)
			if (buf->b_changed)
			{
				MSG_OUTSTR("[No write since last change]\n");
				break;
			}

/* This windgoto is required for when the '\n' resulted in a "delete line 1"
 * command to the terminal. */

	windgoto(msg_row, msg_col);
	cursor_on();
	(void)call_shell(cmd, SHELL_COOKED);
	need_check_timestamps = TRUE;

/*
 * put the message cursor at the end of the screen, avoids wait_return() to
 * overwrite the text that the external command showed
 */
	msg_pos((int)Rows - 1, 0);

#ifdef AUTOCMD
	if (autocmd_busy)
		must_redraw = CLEAR;
	else
#endif
	{
		/*
		 * If K_TI is defined, we assume that we switch screens when
		 * starttermcap() is called. In that case we really want to wait for
		 * "hit return to continue".
		 */
		save_nwr = no_wait_return;
		if (*T_TI != NUL)
			no_wait_return = FALSE;
#ifdef AMIGA
		wait_return(term_console ? -1 : TRUE);		/* see below */
#else
		wait_return(TRUE);
#endif
		no_wait_return = save_nwr;
		starttermcap();		/* start termcap if not done by wait_return() */

		/*
		 * In an Amiga window redrawing is caused by asking the window size.
		 * If we got an interrupt this will not work. The chance that the
		 * window size is wrong is very small, but we need to redraw the
		 * screen.  Don't do this if ':' hit in wait_return().  THIS IS UGLY
		 * but it saves an extra redraw.
		 */
#ifdef AMIGA
		if (skip_redraw)				/* ':' hit in wait_return() */
			must_redraw = CLEAR;
		else if (term_console)
		{
			OUTSTR("\033[0 q"); 		/* get window size */
			if (got_int)
				must_redraw = CLEAR;	/* if got_int is TRUE, redraw needed */
			else
				must_redraw = 0;		/* no extra redraw needed */
		}
#endif /* AMIGA */
	}
}

/*
 * do_filter: filter lines through a command given by the user
 *
 * We use temp files and the call_shell() routine here. This would normally
 * be done using pipes on a UNIX machine, but this is more portable to
 * non-unix machines. The call_shell() routine needs to be able
 * to deal with redirection somehow, and should handle things like looking
 * at the PATH env. variable, and adding reasonable extensions to the
 * command name given by the user. All reasonable versions of call_shell()
 * do this.
 * We use input redirection if do_in is TRUE.
 * We use output redirection if do_out is TRUE.
 */
	static void
do_filter(line1, line2, buff, do_in, do_out)
	linenr_t	line1, line2;
	char_u		*buff;
	int			do_in, do_out;
{
	char_u		*itmp = NULL;
	char_u		*otmp = NULL;
	linenr_t 	linecount;
	FPOS		cursor_save;
#ifdef AUTOCMD
	BUF			*old_curbuf = curbuf;
#endif

	/*
	 * Disallow shell commands from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		return;
	}
	if (*buff == NUL)		/* no filter command */
		return;

#ifdef WIN32
	/*
	 * Check if external commands are allowed now.
	 */
	if (can_end_termcap_mode(TRUE) == FALSE)
		return;
#endif

	cursor_save = curwin->w_cursor;
	linecount = line2 - line1 + 1;
	curwin->w_cursor.lnum = line1;
	curwin->w_cursor.col = 0;

	/*
	 * 1. Form temp file names
	 * 2. Write the lines to a temp file
	 * 3. Run the filter command on the temp file
	 * 4. Read the output of the command into the buffer
	 * 5. Delete the original lines to be filtered
	 * 6. Remove the temp files
	 */

	if ((do_in && (itmp = vim_tempname('i')) == NULL) ||
							   (do_out && (otmp = vim_tempname('o')) == NULL))
	{
		emsg(e_notmp);
		goto filterend;
	}

/*
 * The writing and reading of temp files will not be shown.
 * Vi also doesn't do this and the messages are not very informative.
 */
	++no_wait_return;			/* don't call wait_return() while busy */
	if (do_in && buf_write(curbuf, itmp, NULL, line1, line2,
										   FALSE, FALSE, FALSE, TRUE) == FAIL)
	{
		msg_outchar('\n');					/* keep message from buf_write() */
		--no_wait_return;
		(void)emsg2(e_notcreate, itmp);		/* will call wait_return */
		goto filterend;
	}
#ifdef AUTOCMD
	if (curbuf != old_curbuf)
		goto filterend;
#endif

	if (!do_out)
		msg_outchar('\n');

#if (defined(UNIX) && !defined(ARCHIE)) || defined(OS2)
/*
 * put braces around the command (for concatenated commands)
 */
 	sprintf((char *)IObuff, "(%s)", (char *)buff);
	if (do_in)
	{
		STRCAT(IObuff, " < ");
		STRCAT(IObuff, itmp);
	}
#else
/*
 * for shells that don't understand braces around commands, at least allow
 * the use of commands in a pipe.
 */
	STRCPY(IObuff, buff);
	if (do_in)
	{
		char_u		*p;
	/*
	 * If there is a pipe, we have to put the '<' in front of it.
	 * Don't do this when 'shellquote' is not empty, otherwise the redirection
	 * would be inside the quotes.
	 */
		p = vim_strchr(IObuff, '|');
		if (p && *p_shq == NUL)
			*p = NUL;
		STRCAT(IObuff, " < ");
		STRCAT(IObuff, itmp);
		p = vim_strchr(buff, '|');
		if (p && *p_shq == NUL)
			STRCAT(IObuff, p);
	}
#endif
	if (do_out)
	{
		char_u *p;

		if ((p = vim_strchr(p_srr, '%')) != NULL && p[1] == 's')
		{
			p = IObuff + STRLEN(IObuff);
			*p++ = ' '; /* not really needed? Not with sh, ksh or bash */
			sprintf((char *)p, (char *)p_srr, (char *)otmp);
		}
		else
			sprintf((char *)IObuff + STRLEN(IObuff), " %s %s",
												 (char *)p_srr, (char *)otmp);
	}

	windgoto((int)Rows - 1, 0);
	cursor_on();

	/*
	 * When not redirecting the output the command can write anything to the
	 * screen. If 'shellredir' is equal to ">", screen may be messed up by
	 * stderr output of external command. Clear the screen later.
	 * If do_in is FALSE, this could be something like ":r !cat", which may
	 * also mess up the screen, clear it later.
	 */
	if (!do_out || STRCMP(p_srr, ">") == 0 || !do_in)
		must_redraw = CLEAR;
	else
		redraw_later(NOT_VALID);

	/*
	 * When call_shell() fails wait_return() is called to give the user a
	 * chance to read the error messages. Otherwise errors are ignored, so you
	 * can see the error messages from the command that appear on stdout; use
	 * 'u' to fix the text
	 * Switch to cooked mode when not redirecting stdin, avoids that something
	 * like ":r !cat" hangs.
	 */
	if (call_shell(IObuff, SHELL_FILTER | SHELL_COOKED) == FAIL)
	{
		must_redraw = CLEAR;
		wait_return(FALSE);
	}
	need_check_timestamps = TRUE;

	if (do_out)
	{
		if (u_save((linenr_t)(line2), (linenr_t)(line2 + 1)) == FAIL)
		{
			goto error;
		}
		if (readfile(otmp, NULL, line2, FALSE, (linenr_t)0, MAXLNUM, TRUE)
																	  == FAIL)
		{
			msg_outchar('\n');
			emsg2(e_notread, otmp);
			goto error;
		}
#ifdef AUTOCMD
		if (curbuf != old_curbuf)
			goto filterend;
#endif

		if (do_in)
		{
			/* put cursor on first filtered line for ":range!cmd" */
			curwin->w_cursor.lnum = line1;
			dellines(linecount, TRUE, TRUE);
			curbuf->b_op_start.lnum -= linecount;		/* adjust '[ */
			curbuf->b_op_end.lnum -= linecount;			/* adjust '] */
			write_lnum_adjust(-linecount);				/* adjust last line
														   for next write */
		}
		else
		{
			/* put cursor on last new line for ":r !cmd" */
			curwin->w_cursor.lnum = curbuf->b_op_end.lnum;
			linecount = curbuf->b_op_end.lnum - curbuf->b_op_start.lnum + 1;
		}
		beginline(TRUE);				/* cursor on first non-blank */
		--no_wait_return;

		if (linecount > p_report)
		{
			if (do_in)
			{
				sprintf((char *)msg_buf, "%ld lines filtered", (long)linecount);
				if (msg(msg_buf) && !msg_scroll)
					keep_msg = msg_buf;		/* display message after redraw */
			}
			else
				msgmore((long)linecount);
		}
	}
	else
	{
error:
		/* put cursor back in same position for ":w !cmd" */
		curwin->w_cursor = cursor_save;
		--no_wait_return;
		wait_return(FALSE);
	}

filterend:

#ifdef AUTOCMD
	if (curbuf != old_curbuf)
	{
		--no_wait_return;
		EMSG("*Filter* Autocommands must not change current buffer");
	}
#endif
	if (itmp != NULL)
		vim_remove(itmp);
	if (otmp != NULL)
		vim_remove(otmp);
	vim_free(itmp);
	vim_free(otmp);
}

#ifdef VIMINFO

static int no_viminfo __ARGS((void));
static int	viminfo_errcnt;

	static int
no_viminfo()
{
	/* "vim -i NONE" does not read or write a viminfo file */
	return (use_viminfo != NULL && STRCMP(use_viminfo, "NONE") == 0);
}

/*
 * Report an error for reading a viminfo file.
 * Count the number of errors.  When there are more than 10, return TRUE.
 */
	int
viminfo_error(message, line)
	char	*message;
	char_u	*line;
{
	sprintf((char *)IObuff, "viminfo: %s in line: ", message);
	STRNCAT(IObuff, line, IOSIZE - STRLEN(IObuff));
	emsg(IObuff);
	if (++viminfo_errcnt >= 10)
	{
		EMSG("viminfo: Too many errors, skipping rest of file");
		return TRUE;
	}
	return FALSE;
}

/*
 * read_viminfo() -- Read the viminfo file.  Registers etc. which are already
 * set are not over-written unless force is TRUE. -- webb
 */
	int
read_viminfo(file, want_info, want_marks, forceit)
	char_u	*file;
	int		want_info;
	int		want_marks;
	int		forceit;
{
	FILE	*fp;

	if (no_viminfo())
		return FAIL;

	file = viminfo_filename(file);			/* may set to default if NULL */
	if ((fp = fopen((char *)file, READBIN)) == NULL)
		return FAIL;

	viminfo_errcnt = 0;
	do_viminfo(fp, NULL, want_info, want_marks, forceit);

	fclose(fp);

	return OK;
}

/*
 * write_viminfo() -- Write the viminfo file.  The old one is read in first so
 * that effectively a merge of current info and old info is done.  This allows
 * multiple vims to run simultaneously, without losing any marks etc.  If
 * forceit is TRUE, then the old file is not read in, and only internal info is
 * written to the file. -- webb
 */
	void
write_viminfo(file, forceit)
	char_u	*file;
	int		forceit;
{
	FILE	*fp_in = NULL;
	FILE	*fp_out = NULL;
	char_u	*tempname = NULL;

	if (no_viminfo())
		return;

	file = viminfo_filename(file);		/* may set to default if NULL */
	file = strsave(file);				/* make a copy, don't want NameBuff */
	if (file != NULL)
	{
		fp_in = fopen((char *)file, READBIN);
		if (fp_in == NULL)
			fp_out = fopen((char *)file, WRITEBIN);
		else if ((tempname = vim_tempname('o')) != NULL)
			fp_out = fopen((char *)tempname, WRITEBIN);
	}
	if (file == NULL || fp_out == NULL)
	{
		EMSG2("Can't write viminfo file %s!", file == NULL ? (char_u *)"" :
											  fp_in == NULL ? file : tempname);
		if (fp_in != NULL)
			fclose(fp_in);
		goto end;
	}

	viminfo_errcnt = 0;
	do_viminfo(fp_in, fp_out, !forceit, !forceit, FALSE);

	fclose(fp_out);			/* errors are ignored !? */
	if (fp_in != NULL)
	{
		fclose(fp_in);
		/*
		 * In case of an error, don't overwrite the original viminfo file.
		 */
		if (viminfo_errcnt || vim_rename(tempname, file) == -1)
			vim_remove(tempname);
	}
end:
	vim_free(file);
	vim_free(tempname);
}

	static char_u *
viminfo_filename(file)
	char_u		*file;
{
	if (file == NULL || *file == NUL)
	{
		expand_env(use_viminfo == NULL ? (char_u *)VIMINFO_FILE : use_viminfo,
														  NameBuff, MAXPATHL);
		return NameBuff;
	}
	return file;
}

/*
 * do_viminfo() -- Should only be called from read_viminfo() & write_viminfo().
 */
	static void
do_viminfo(fp_in, fp_out, want_info, want_marks, force_read)
	FILE	*fp_in;
	FILE	*fp_out;
	int		want_info;
	int		want_marks;
	int		force_read;
{
	int		count = 0;
	int		eof = FALSE;
	char_u	*line;

	if ((line = alloc(LSIZE)) == NULL)
		return;

	if (fp_in != NULL)
	{
		if (want_info)
			eof = read_viminfo_up_to_marks(line, fp_in, force_read);
		else
			/* Skip info, find start of marks */
			while (!(eof = vim_fgets(line, LSIZE, fp_in)) && line[0] != '>')
				;
	}
	if (fp_out != NULL)
	{
		/* Write the info: */
		fprintf(fp_out, "# This viminfo file was generated by vim\n");
		fprintf(fp_out, "# You may edit it if you're careful!\n\n");
		write_viminfo_search_pattern(fp_out);
		write_viminfo_sub_string(fp_out);
		write_viminfo_history(fp_out);
		write_viminfo_registers(fp_out);
		write_viminfo_filemarks(fp_out);
		count = write_viminfo_marks(fp_out);
	}
	if (fp_in != NULL && want_marks)
		copy_viminfo_marks(line, fp_in, fp_out, count, eof);
	vim_free(line);
}

/*
 * read_viminfo_up_to_marks() -- Only called from do_viminfo().  Reads in the
 * first part of the viminfo file which contains everything but the marks that
 * are local to a file.  Returns TRUE when end-of-file is reached. -- webb
 */
	static int
read_viminfo_up_to_marks(line, fp, forceit)
	char_u	*line;
	FILE	*fp;
	int		forceit;
{
	int		eof;

	prepare_viminfo_history(forceit ? 9999 : 0);
	eof = vim_fgets(line, LSIZE, fp);
	while (!eof && line[0] != '>')
	{
		switch (line[0])
		{
			case NUL:
			case '\r':
			case '\n':
			case '#':		/* A comment */
				eof = vim_fgets(line, LSIZE, fp);
				break;
			case '"':
				eof = read_viminfo_register(line, fp, forceit);
				break;
			case '/':		/* Search string */
			case '&':		/* Substitute search string */
			case '~':		/* Last search string, followed by '/' or '&' */
				eof = read_viminfo_search_pattern(line, fp, forceit);
				break;
			case '$':
				eof = read_viminfo_sub_string(line, fp, forceit);
				break;
			case ':':
			case '?':
				eof = read_viminfo_history(line, fp);
				break;
			case '\'':
				/* How do we have a file mark when the file is not in the
				 * buffer list?
				 */
				eof = read_viminfo_filemark(line, fp, forceit);
				break;
#if 0
			case '+':
				/* eg: "+40 /path/dir file", for running vim with no args */
				eof = vim_fgets(line, LSIZE, fp);
				break;
#endif
			default:
				if (viminfo_error("Illegal starting char", line))
					eof = TRUE;
				else
					eof = vim_fgets(line, LSIZE, fp);
				break;
		}
	}
	finish_viminfo_history();
	return eof;
}

/*
 * check string read from viminfo file
 * remove '\n' at the end of the line
 * - replace CTRL-V CTRL-V with CTRL-V
 * - replace CTRL-V 'n'    with '\n'
 */
	void
viminfo_readstring(p)
	char_u		*p;
{
	while (*p != NUL && *p != '\n')
	{
		if (*p == Ctrl('V'))
		{
			if (p[1] == 'n')
				p[0] = '\n';
			vim_memmove(p + 1, p + 2, STRLEN(p));
		}
		++p;
	}
	*p = NUL;
}

/*
 * write string to viminfo file
 * - replace CTRL-V with CTRL-V CTRL-V
 * - replace '\n'   with CTRL-V 'n'
 * - add a '\n' at the end
 */
	void
viminfo_writestring(fd, p)
	FILE	*fd;
	char_u	*p;
{
	register int	c;

	while ((c = *p++) != NUL)
	{
		if (c == Ctrl('V') || c == '\n')
		{
			putc(Ctrl('V'), fd);
			if (c == '\n')
				c = 'n';
		}
		putc(c, fd);
	}
	putc('\n', fd);
}
#endif /* VIMINFO */

/*
 * Implementation of ":fixdel", also used by get_stty().
 *  <BS>    resulting <Del>
 *   ^?        ^H
 * not ^?      ^?
 */
	void
do_fixdel()
{
	char_u	*p;

	p = find_termcode((char_u *)"kb");
	add_termcode((char_u *)"kD", p != NULL && *p == 0x7f ?
										 (char_u *)"\010" : (char_u *)"\177");
}

	static void
print_line_no_prefix(lnum, use_number)
	linenr_t	lnum;
	int			use_number;
{
	char_u		numbuf[20];

	if (curwin->w_p_nu || use_number)
	{
		sprintf((char *)numbuf, "%7ld ", (long)lnum);
		set_highlight('n');		/* Highlight line numbers */
		start_highlight();
		msg_outstr(numbuf);
		stop_highlight();
	}
	msg_prt_line(ml_get(lnum));
}

    void
print_line(lnum, use_number)
	linenr_t	lnum;
	int			use_number;
{
	msg_outchar('\n');
	print_line_no_prefix (lnum, use_number);
}

    void
print_line_cr(lnum, use_number)
	linenr_t	lnum;
	int			use_number;
{
	msg_outchar('\r');
	print_line_no_prefix (lnum, use_number);
}

/*
 * Implementation of ":file[!] [fname]".
 */
	void
do_file(arg, forceit)
	char_u	*arg;
	int		forceit;
{
	char_u		*fname, *sfname;
	BUF			*buf;

	if (*arg != NUL)
	{
		/*
		 * The name of the current buffer will be changed.
		 * A new buffer entry needs to be made to hold the old
		 * file name, which will become the alternate file name.
		 */
		fname = curbuf->b_filename;
		sfname = curbuf->b_sfilename;
		curbuf->b_filename = NULL;
		curbuf->b_sfilename = NULL;
		if (setfname(arg, NULL, TRUE) == FAIL)
		{
			curbuf->b_filename = fname;
			curbuf->b_sfilename = sfname;
			return;
		}
		curbuf->b_notedited = TRUE;
		buf = buflist_new(fname, sfname, curwin->w_cursor.lnum, FALSE);
		if (buf != NULL)
			curwin->w_alt_fnum = buf->b_fnum;
		vim_free(fname);
		vim_free(sfname);
	}
	/* print full filename if :cd used */
	fileinfo(did_cd, FALSE, forceit);
}

/*
 * do the Ex mode :insert and :append commands
 */

void
ex_insert (int before, linenr_t whatline)
{
	/* put the cursor somewhere sane if we insert nothing */

	if (whatline > curbuf->b_ml.ml_line_count) {
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
	} else {
		curwin->w_cursor.lnum = whatline;
	}

	while (1) {
		char_u *theline;

		if (((theline = getcmdline (' ', 1L)) == 0) ||
			((theline[0] == '.') && (theline[1] == 0))) {
			break;
		}

		if (before) {
			mark_adjust (whatline, MAXLNUM, 1, 0L);
			ml_append (whatline - 1, theline, (colnr_t) 0, FALSE);
			curwin->w_cursor.lnum = whatline;
		} else {
			mark_adjust (whatline + 1, MAXLNUM, 1, 0L);
			ml_append (whatline, theline, (colnr_t) 0, FALSE);
			curwin->w_cursor.lnum = whatline + 1;
		}

		vim_free (theline);
		whatline++;
	}

	CHANGED;
	beginline (MAYBE);
	updateScreen (NOT_VALID);
}

/*
 * do the Ex mode :change command
 */

void
ex_change (linenr_t start, linenr_t end)
{
	while (end >= start) {
		ml_delete (start, FALSE);
		end--;
	}

	ex_insert (TRUE, start);
}

void
ex_z (linenr_t line, char_u *arg)
{
	char_u *x;
	int bigness = curwin->w_height - 3;
	char_u kind;
	int minus = 0;
	linenr_t start, end, curs, i;

	if (arg == 0) { /* is this possible?  I don't remember */
		arg = "";
	}

	if (bigness < 1) {
		bigness = 1;
	}

	x = arg;
	if (*x == '-' || *x == '+' || *x == '=' || *x == '^' || *x == '.') x++;

	if (*x != 0) {
		if (!isdigit (*x)) {
			EMSG ("non-numeric argument to :z");
			return;
		} else {
			bigness = atoi (x);
		}
	}

	kind = *arg;

	switch (kind) {
		case '-':
			start = line - bigness;
			end = line;
			curs = line;
			break;

		case '=':
			start = line - bigness / 2 + 1;
			end = line + bigness / 2 - 1;
			curs = line;
			minus = 1;
			break;

		case '^':
			start = line - bigness * 2;
			end = line - bigness;
			curs = line - bigness;
			break;

		case '.':
			start = line - bigness / 2;
			end = line + bigness / 2;
			curs = end;
			break;

		default:  /* '+' */
			start = line;
			end = line + bigness;
			curs = end;
			break;
	}

	if (start < 1) {
		start = 1;
	}

	if (end > curbuf->b_ml.ml_line_count) {
		end = curbuf->b_ml.ml_line_count;
	}

	if (curs > curbuf->b_ml.ml_line_count) {
		curs = curbuf->b_ml.ml_line_count;
	}

	for (i = start; i <= end; i++) {
		int j;

		if (minus && (i == line)) {
			msg_outchar ('\n');

			for (j = 1; j < Columns; j++) {
				msg_outchar ('-');
			}
		}

		print_line (i, FALSE);

		if (minus && (i == line)) {
			msg_outchar ('\n');

			for (j = 1; j < Columns; j++) {
				msg_outchar ('-');
			}
		}
	}

	curwin->w_cursor.lnum = curs;
}

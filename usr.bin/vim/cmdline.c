/*	$OpenBSD: cmdline.c,v 1.3 1996/09/21 23:23:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * cmdline.c: functions for reading in the command line and executing it
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "cmdtab.h"
#include "ops.h"			/* included because we call functions in ops.c */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>			/* for chdir() */
#endif

/*
 * variables shared between getcmdline() and redrawcmdline()
 */
static char_u	*cmdbuff;		/* pointer to command line buffer */
static int		 cmdbufflen;	/* length of cmdbuff */
static int		 cmdlen;		/* number of chars on command line */
static int		 cmdpos;		/* current cursor position */
static int		 cmdspos;		/* cursor column on screen */
static int		 cmdfirstc; 	/* ':', '/' or '?' */

/*
 * Typing mode on the command line.  Shared by getcmdline() and
 * put_on_cmdline().
 */
static int		overstrike = FALSE;	/* typing mode */

static int			quitmore = 0;
static int  		cmd_numfiles = -1;	  /* number of files found by
													filename completion */
/*
 * There are two history tables:
 * 0: colon commands
 * 1: search commands
 */
static char_u		**(history[2]) = {NULL, NULL};	/* history tables */
static int			hisidx[2] = {-1, -1};			/* last entered entry */
static int			hislen = 0; 		/* actual lengt of history tables */

#ifdef RIGHTLEFT
static int			cmd_hkmap = 0;		/* Hebrew mapping during command line */
#endif

static void		init_history __ARGS((void));

static int		is_in_history __ARGS((int, char_u *, int));
static void		putcmdline __ARGS((int));
static void		redrawcmd __ARGS((void));
static void		cursorcmd __ARGS((void));
static int		ccheck_abbr __ARGS((int));
static char_u	*do_one_cmd __ARGS((char_u **, int *, int));
static int		buf_write_all __ARGS((BUF *));
static int		do_write __ARGS((char_u *, linenr_t, linenr_t, int, int));
static char_u	*getargcmd __ARGS((char_u **));
static void		backslash_halve __ARGS((char_u *p, int expand_wildcards));
static void		do_make __ARGS((char_u *));
static int		do_arglist __ARGS((char_u *));
static int		is_backslash __ARGS((char_u *str));
static int		check_readonly __ARGS((int));
static int		check_changed __ARGS((BUF *, int, int, int));
static int		check_changed_any __ARGS((void));
static int		check_more __ARGS((int, int));
static void		vim_strncpy __ARGS((char_u *, char_u *, int));
static int		nextwild __ARGS((int));
static int		showmatches __ARGS((char_u *));
static linenr_t get_address __ARGS((char_u **));
static void		set_expand_context __ARGS((int, char_u *));
static char_u	*set_one_cmd_context __ARGS((int, char_u *));
static int		ExpandFromContext __ARGS((char_u *, int *, char_u ***, int, int));
static int		ExpandCommands __ARGS((regexp *, int *, char_u ***));

/*
 * init_history() - initialize the command line history
 */
	static void
init_history()
{
	int		newlen;			/* new length of history table */
	char_u	**temp;
	register int i;
	int		j;
	int		type;

	/*
	 * If size of history table changed, reallocate it
	 */
	newlen = (int)p_hi;
	if (newlen != hislen)						/* history length changed */
	{
		for (type = 0; type <= 1; ++type)		/* adjust both history tables */
		{
			if (newlen)
				temp = (char_u **)lalloc((long_u)(newlen * sizeof(char_u *)),
									TRUE);
			else
				temp = NULL;
			if (newlen == 0 || temp != NULL)
			{
				if (hisidx[type] < 0)			/* there are no entries yet */
				{
					for (i = 0; i < newlen; ++i)
						temp[i] = NULL;
				}
				else if (newlen > hislen)		/* array becomes bigger */
				{
					for (i = 0; i <= hisidx[type]; ++i)
						temp[i] = history[type][i];
					j = i;
					for ( ; i <= newlen - (hislen - hisidx[type]); ++i)
						temp[i] = NULL;
					for ( ; j < hislen; ++i, ++j)
						temp[i] = history[type][j];
				}
				else							/* array becomes smaller or 0 */
				{
					j = hisidx[type];
					for (i = newlen - 1; ; --i)
					{
						if (i >= 0)				/* copy newest entries */
							temp[i] = history[type][j];
						else					/* remove older entries */
							vim_free(history[type][j]);
						if (--j < 0)
							j = hislen - 1;
						if (j == hisidx[type])
							break;
					}
					hisidx[type] = newlen - 1;
				}
				vim_free(history[type]);
				history[type] = temp;
			}
		}
		hislen = newlen;
	}
}

/*
 * check if command line 'str' is already in history
 * 'type' is 0 for ':' commands, '1' for search commands
 * if 'move_to_front' is TRUE, matching entry is moved to end of history
 */
	static int
is_in_history(type, str, move_to_front)
	int		type;
	char_u	*str;
	int		move_to_front;		/* Move the entry to the front if it exists */
{
	int		i;
	int		last_i = -1;

	if (hisidx[type] < 0)
		return FALSE;
	i = hisidx[type];
	do
	{
		if (history[type][i] == NULL)
			return FALSE;
		if (STRCMP(str, history[type][i]) == 0)
		{
			if (!move_to_front)
				return TRUE;
			last_i = i;
			break;
		}
		if (--i < 0)
			i = hislen - 1;
	} while (i != hisidx[type]);

	if (last_i >= 0)
	{
		str = history[type][i];
		while (i != hisidx[type])
		{
			if (++i >= hislen)
				i = 0;
			history[type][last_i] = history[type][i];
			last_i = i;
		}
		history[type][i] = str;
		return TRUE;
	}
	return FALSE;
}

/*
 * Add the given string to the given history.  If the string is already in the
 * history then it is moved to the front.  histype may be 0 for the ':'
 * history, or 1 for the '/' history.
 */
	void
add_to_history(histype, new_entry)
	int		histype;
	char_u	*new_entry;
{
	if (hislen != 0 && !is_in_history(histype, new_entry, TRUE))
	{
		if (++hisidx[histype] == hislen)
			hisidx[histype] = 0;
		vim_free(history[histype][hisidx[histype]]);
		history[histype][hisidx[histype]] = strsave(new_entry);
	}
}


/*
 * getcmdline() - accept a command line starting with ':', '/', or '?'
 *
 * The line is collected in cmdbuff, which is reallocated to fit the command
 * line.
 *
 * Return pointer to allocated string if there is a commandline, NULL
 * otherwise.
 */

	char_u *
getcmdline(firstc, count)
	int			firstc; 	/* either ':', '/', or '?' */
	long		count;		/* only used for incremental search */
{
	register int	 	c;
#ifdef DIGRAPHS
			 int		cc;
#endif
	register int		i;
			 int		j;
			 char_u		*p;
			 int		hiscnt;				/* current history line in use */
			 char_u		*lookfor = NULL;	/* string to match */
			 int		gotesc = FALSE;		/* TRUE when <ESC> just typed */
			 int		do_abbr;			/* when TRUE check for abbr. */
			 int		histype;			/* history type to be used */
			 FPOS		old_cursor;
			 colnr_t	old_curswant;
			 int		did_incsearch = FALSE;
			 int		incsearch_postponed = FALSE;
			 int		save_msg_scroll = msg_scroll;
			 int		some_key_typed = FALSE;	/* one of the keys was typed */
#ifdef USE_MOUSE
			 /* mouse drag and release events are ignored, unless they are
			  * preceded with a mouse down event */
			 int		ignore_drag_release = TRUE;
#endif

	overstrike = FALSE;						/* always start in insert mode */
	old_cursor = curwin->w_cursor;			/* needs to be restored later */
	old_curswant = curwin->w_curswant;
/*
 * set some variables for redrawcmd()
 */
	cmdfirstc = firstc;
	alloc_cmdbuff(0);					/* allocate initial cmdbuff */
	if (cmdbuff == NULL)
		return NULL;					/* out of memory */
	cmdlen = cmdpos = 0;
	cmdspos = 1;
	State = CMDLINE;
#ifdef USE_MOUSE
	setmouse();
#endif
	gotocmdline(TRUE);
	msg_outchar(firstc);
	/*
	 * Avoid scrolling when called by a recursive do_cmdline(), e.g. when doing
	 * ":@0" when register 0 doesn't contain a CR.
	 */
	msg_scroll = FALSE;

	init_history();
	hiscnt = hislen;			/* set hiscnt to impossible history value */
	histype = (firstc == ':' ? 0 : 1);

#ifdef DIGRAPHS
	do_digraph(-1);				/* init digraph typahead */
#endif

	/* collect the command string, handling editing keys */
	for (;;)
	{
		cursorcmd();			/* set the cursor on the right spot */
		c = vgetc();
		if (KeyTyped)
		{
			some_key_typed = TRUE;
#ifdef RIGHTLEFT
			if (cmd_hkmap)
				c = hkmap(c);
#endif
		}
		if (c == Ctrl('C'))
			got_int = FALSE;	/* ignore got_int when CTRL-C was typed here */

			/* free old command line when finished moving around in the
			 * history list */
		if (lookfor && c != K_S_DOWN && c != K_S_UP &&
				c != K_DOWN && c != K_UP &&
				c != K_PAGEDOWN && c != K_PAGEUP &&
				c != K_KPAGEDOWN && c != K_KPAGEUP &&
				(cmd_numfiles > 0 || (c != Ctrl('P') && c != Ctrl('N'))))
		{
			vim_free(lookfor);
			lookfor = NULL;
		}

		/*
		 * <S-Tab> works like CTRL-P (unless 'wc' is <S-Tab>).
		 */
		if (c != p_wc && c == K_S_TAB)
			c = Ctrl('P');

			/* free expanded names when finished walking through matches */
		if (cmd_numfiles != -1 && !(c == p_wc && KeyTyped) && c != Ctrl('N') &&
						c != Ctrl('P') && c != Ctrl('A') && c != Ctrl('L'))
			(void)ExpandOne(NULL, NULL, 0, WILD_FREE);

#ifdef DIGRAPHS
		c = do_digraph(c);
#endif

		if (c == '\n' || c == '\r' || (c == ESC && (!KeyTyped || 
										 vim_strchr(p_cpo, CPO_ESC) != NULL)))
		{
			if (ccheck_abbr(c + ABBR_OFF))
				goto cmdline_changed;
			outchar('\r');		/* show that we got the return */
			screen_cur_col = 0;
			flushbuf();
			break;
		}

			/* hitting <ESC> twice means: abandon command line */
			/* wildcard expansion is only done when the key is really typed,
			 * not when it comes from a macro */
		if (c == p_wc && !gotesc && KeyTyped)
		{
			if (cmd_numfiles > 0)	/* typed p_wc twice */
				i = nextwild(WILD_NEXT);
			else					/* typed p_wc first time */
				i = nextwild(WILD_EXPAND_KEEP);
			if (c == ESC)
				gotesc = TRUE;
			if (i)
				goto cmdline_changed;
		}
		gotesc = FALSE;

		if (c == NUL || c == K_ZERO)		/* NUL is stored as NL */
			c = NL;

		do_abbr = TRUE;			/* default: check for abbreviation */
		switch (c)
		{
		case K_BS:
		case Ctrl('H'):
		case K_DEL:
		case Ctrl('W'):
				/*
				 * delete current character is the same as backspace on next
				 * character, except at end of line
				 */
				if (c == K_DEL && cmdpos != cmdlen)
					++cmdpos;
				if (cmdpos > 0)
				{
					j = cmdpos;
					if (c == Ctrl('W'))
					{
						while (cmdpos && vim_isspace(cmdbuff[cmdpos - 1]))
							--cmdpos;
						i = iswordchar(cmdbuff[cmdpos - 1]);
						while (cmdpos && !vim_isspace(cmdbuff[cmdpos - 1]) &&
										 iswordchar(cmdbuff[cmdpos - 1]) == i)
							--cmdpos;
					}
					else
						--cmdpos;
					cmdlen -= j - cmdpos;
					i = cmdpos;
					while (i < cmdlen)
						cmdbuff[i++] = cmdbuff[j++];
					redrawcmd();
				}
				else if (cmdlen == 0 && c != Ctrl('W'))
				{
					vim_free(cmdbuff);		/* no commandline to return */
					cmdbuff = NULL;
					msg_pos(-1, 0);
					msg_outchar(' ');	/* delete ':' */
					redraw_cmdline = TRUE;
					goto returncmd; 	/* back to cmd mode */
				}
				goto cmdline_changed;

		case K_INS:
				overstrike = !overstrike;
				/* should change shape of cursor */
				goto cmdline_not_changed;

/*		case '@':	only in very old vi */
		case Ctrl('U'):
				cmdpos = 0;
				cmdlen = 0;
				cmdspos = 1;
				redrawcmd();
				goto cmdline_changed;

		case ESC:		/* get here if p_wc != ESC or when ESC typed twice */
		case Ctrl('C'):
				gotesc = TRUE;		/* will free cmdbuff after putting it in
										history */
				goto returncmd; 	/* back to cmd mode */

		case Ctrl('R'):				/* insert register */
				putcmdline('"');
				++no_mapping;
			  	c = vgetc();
				--no_mapping;
				if (c != ESC)		/* use ESC to cancel inserting register */
					cmdline_paste(c);
				redrawcmd();
				goto cmdline_changed;

		case Ctrl('D'):
			{
				if (showmatches(cmdbuff) == FAIL)
					break;		/* Use ^D as normal char instead */

				redrawcmd();
				continue;		/* don't do incremental search now */
			}

		case K_RIGHT:
		case K_S_RIGHT:
				do
				{
						if (cmdpos >= cmdlen)
								break;
						cmdspos += charsize(cmdbuff[cmdpos]);
						++cmdpos;
				}
				while (c == K_S_RIGHT && cmdbuff[cmdpos] != ' ');
				goto cmdline_not_changed;

		case K_LEFT:
		case K_S_LEFT:
				do
				{
						if (cmdpos <= 0)
								break;
						--cmdpos;
						cmdspos -= charsize(cmdbuff[cmdpos]);
				}
				while (c == K_S_LEFT && cmdbuff[cmdpos - 1] != ' ');
				goto cmdline_not_changed;

#ifdef USE_MOUSE
		case K_MIDDLEDRAG:
		case K_MIDDLERELEASE:
		case K_IGNORE:
				goto cmdline_not_changed;	/* Ignore mouse */

		case K_MIDDLEMOUSE:
# ifdef USE_GUI
				/* When GUI is active, also paste when 'mouse' is empty */
				if (!gui.in_use)
# endif
					if (!mouse_has(MOUSE_COMMAND))
						goto cmdline_not_changed;	/* Ignore mouse */
# ifdef USE_GUI
				if (gui.in_use && yankbuffer == 0)
					cmdline_paste('*');
				else
# endif
					cmdline_paste(yankbuffer);
				redrawcmd();
				goto cmdline_changed;

		case K_LEFTDRAG:
		case K_LEFTRELEASE:
		case K_RIGHTDRAG:
		case K_RIGHTRELEASE:
				if (ignore_drag_release)
					goto cmdline_not_changed;
				/* FALLTHROUGH */
		case K_LEFTMOUSE:
		case K_RIGHTMOUSE:
				if (c == K_LEFTRELEASE || c == K_RIGHTRELEASE)
					ignore_drag_release = TRUE;
				else
					ignore_drag_release = FALSE;
# ifdef USE_GUI
				/* When GUI is active, also move when 'mouse' is empty */
				if (!gui.in_use)
# endif
					if (!mouse_has(MOUSE_COMMAND))
						goto cmdline_not_changed;	/* Ignore mouse */
				cmdspos = 1;
				for (cmdpos = 0; cmdpos < cmdlen; ++cmdpos)
				{
					i = charsize(cmdbuff[cmdpos]);
					if (mouse_row <= cmdline_row + cmdspos / Columns &&
										mouse_col < cmdspos % Columns + i)
						break;
					cmdspos += i;
				}
				goto cmdline_not_changed;
#endif	/* USE_MOUSE */

#ifdef USE_GUI
		case K_SCROLLBAR:
				if (!msg_scrolled)
				{
					gui_do_scroll();
					redrawcmd();
				}
				goto cmdline_not_changed;

		case K_HORIZ_SCROLLBAR:
				if (!msg_scrolled)
				{
					gui_do_horiz_scroll();
					redrawcmd();
				}
				goto cmdline_not_changed;
#endif

		case Ctrl('B'):		/* begin of command line */
		case K_HOME:
		case K_KHOME:
				cmdpos = 0;
				cmdspos = 1;
				goto cmdline_not_changed;

		case Ctrl('E'):		/* end of command line */
		case K_END:
		case K_KEND:
				cmdpos = cmdlen;
				cmdbuff[cmdlen] = NUL;
				cmdspos = strsize(cmdbuff) + 1;
				goto cmdline_not_changed;

		case Ctrl('A'):		/* all matches */
				if (!nextwild(WILD_ALL))
					break;
				goto cmdline_changed;

		case Ctrl('L'):		/* longest common part */
				if (!nextwild(WILD_LONGEST))
					break;
				goto cmdline_changed;

		case Ctrl('N'):		/* next match */
		case Ctrl('P'):		/* previous match */
				if (cmd_numfiles > 0)
				{
					if (!nextwild((c == Ctrl('P')) ? WILD_PREV : WILD_NEXT))
						break;
					goto cmdline_changed;
				}

		case K_UP:
		case K_DOWN:
		case K_S_UP:
		case K_S_DOWN:
		case K_PAGEUP:
		case K_KPAGEUP:
		case K_PAGEDOWN:
		case K_KPAGEDOWN:
				if (hislen == 0)		/* no history */
					goto cmdline_not_changed;

				i = hiscnt;
			
				/* save current command string so it can be restored later */
				cmdbuff[cmdpos] = NUL;
				if (lookfor == NULL && (lookfor = strsave(cmdbuff)) == NULL)
					goto cmdline_not_changed;

				j = STRLEN(lookfor);
				for (;;)
				{
						/* one step backwards */
					if (c == K_UP || c == K_S_UP || c == Ctrl('P') ||
							c == K_PAGEUP || c == K_KPAGEUP)
					{
						if (hiscnt == hislen)	/* first time */
							hiscnt = hisidx[histype];
						else if (hiscnt == 0 && hisidx[histype] != hislen - 1)
							hiscnt = hislen - 1;
						else if (hiscnt != hisidx[histype] + 1)
							--hiscnt;
						else					/* at top of list */
						{
							hiscnt = i;
							break;
						}
					}
					else	/* one step forwards */
					{
									/* on last entry, clear the line */
						if (hiscnt == hisidx[histype])
						{
							hiscnt = hislen;
							break;
						}
									/* not on a history line, nothing to do */
						if (hiscnt == hislen)
							break;
						if (hiscnt == hislen - 1)	/* wrap around */
							hiscnt = 0;
						else
							++hiscnt;
					}
					if (hiscnt < 0 || history[histype][hiscnt] == NULL)
					{
						hiscnt = i;
						break;
					}
					if ((c != K_UP && c != K_DOWN) || hiscnt == i ||
							STRNCMP(history[histype][hiscnt],
													lookfor, (size_t)j) == 0)
						break;
				}

				if (hiscnt != i)		/* jumped to other entry */
				{
					vim_free(cmdbuff);
					if (hiscnt == hislen)
						p = lookfor;	/* back to the old one */
					else
						p = history[histype][hiscnt];

					alloc_cmdbuff((int)STRLEN(p));
					if (cmdbuff == NULL)
						goto returncmd;
					STRCPY(cmdbuff, p);

					cmdpos = cmdlen = STRLEN(cmdbuff);
					redrawcmd();
					goto cmdline_changed;
				}
				beep_flush();
				goto cmdline_not_changed;

		case Ctrl('V'):
		case Ctrl('Q'):
#ifdef USE_MOUSE
				ignore_drag_release = TRUE;
#endif
				putcmdline('^');
				c = get_literal();			/* get next (two) character(s) */
				do_abbr = FALSE;			/* don't do abbreviation now */
				break;

#ifdef DIGRAPHS
		case Ctrl('K'):
#ifdef USE_MOUSE
				ignore_drag_release = TRUE;
#endif
				putcmdline('?');
				++no_mapping;
				++allow_keys;
			  	c = vgetc();
				--no_mapping;
				--allow_keys;
				if (c != ESC)				/* ESC cancels CTRL-K */
				{
					if (IS_SPECIAL(c))			/* insert special key code */
						break;
					if (charsize(c) == 1)
						putcmdline(c);
					++no_mapping;
					++allow_keys;
					cc = vgetc();
					--no_mapping;
					--allow_keys;
					if (cc != ESC)			/* ESC cancels CTRL-K */
					{
						c = getdigraph(c, cc, TRUE);
						break;
					}
				}
				redrawcmd();
				goto cmdline_not_changed;
#endif /* DIGRAPHS */

#ifdef RIGHTLEFT
		case Ctrl('_'):		/* CTRL-_: switch language mode */
				cmd_hkmap = !cmd_hkmap;
				goto cmdline_not_changed;
#endif

		default:
				/*
				 * Normal character with no special meaning.  Just set mod_mask
				 * to 0x0 so that typing Shift-Space in the GUI doesn't enter
				 * the string <S-Space>.  This should only happen after ^V.
				 */
				if (!IS_SPECIAL(c))
					mod_mask = 0x0;
				break;
		}

		/* we come here if we have a normal character */

		if (do_abbr && (IS_SPECIAL(c) || !iswordchar(c)) && ccheck_abbr(c))
			goto cmdline_changed;

		/*
		 * put the character in the command line
		 */
		if (IS_SPECIAL(c) || mod_mask != 0x0)
			put_on_cmdline(get_special_key_name(c, mod_mask), -1, TRUE);
		else
		{
			IObuff[0] = c;
			put_on_cmdline(IObuff, 1, TRUE);
		}
		goto cmdline_changed;

/*
 * This part implements incremental searches for "/" and "?"
 * Jump to cmdline_not_changed when a character has been read but the command
 * line did not change. Then we only search and redraw if something changed in
 * the past.
 * Jump to cmdline_changed when the command line did change.
 * (Sorry for the goto's, I know it is ugly).
 */
cmdline_not_changed:
		if (!incsearch_postponed)
			continue;

cmdline_changed:
		if (p_is && (firstc == '/' || firstc == '?'))
		{
				/* if there is a character waiting, search and redraw later */
			if (char_avail())
			{
				incsearch_postponed = TRUE;
				continue;
			}
			incsearch_postponed = FALSE;
			curwin->w_cursor = old_cursor;	/* start at old position */

				/* If there is no command line, don't do anything */
			if (cmdlen == 0)
				i = 0;
			else
			{
				cmdbuff[cmdlen] = NUL;
				emsg_off = TRUE;	/* So it doesn't beep if bad expr */
				i = do_search(firstc, cmdbuff, count,
									  SEARCH_KEEP + SEARCH_OPT + SEARCH_NOOF);
				emsg_off = FALSE;
			}
			if (i)
			{
				highlight_match = TRUE;			/* highlight position */
				cursupdate();
			}
			else
			{
				highlight_match = FALSE;			/* don't highlight */
				/* vim_beep(); */ /* even beeps when invalid expr, e.g. "[" */
			}
			updateScreen(NOT_VALID);
			redrawcmdline();
			did_incsearch = TRUE;
		}
	}

returncmd:
	if (did_incsearch)
	{
		curwin->w_cursor = old_cursor;
		curwin->w_curswant = old_curswant;
		highlight_match = FALSE;
		redraw_later(NOT_VALID);
	}
	if (cmdbuff != NULL)
	{
		/*
		 * Put line in history buffer (":" only when it was typed).
		 */
		cmdbuff[cmdlen] = NUL;
		if (cmdlen != 0 && (some_key_typed || firstc != ':'))
		{
			add_to_history(histype, cmdbuff);
			if (firstc == ':')
			{
				vim_free(new_last_cmdline);
				new_last_cmdline = strsave(cmdbuff);
			}
		}

		if (gotesc)			/* abandon command line */
		{
			vim_free(cmdbuff);
			cmdbuff = NULL;
			MSG("");
			redraw_cmdline = TRUE;
		}
	}

	/*
	 * If the screen was shifted up, redraw the whole screen (later).
	 * If the line is too long, clear it, so ruler and shown command do
	 * not get printed in the middle of it.
	 */
	msg_check();
	msg_scroll = save_msg_scroll;
	State = NORMAL;
#ifdef USE_MOUSE
	setmouse();
#endif
	return cmdbuff;
}

/*
 * Put the given string, of the given length, onto the command line.
 * If len is -1, then STRLEN() is used to calculate the length.
 * If 'redraw' is TRUE then the new part of the command line, and the remaining
 * part will be redrawn, otherwise it will not.  If this function is called
 * twice in a row, then 'redraw' should be FALSE and redrawcmd() should be
 * called afterwards.
 */
	int
put_on_cmdline(str, len, redraw)
	char_u	*str;
	int		len;
	int		redraw;
{
	int		i;

	if (len < 0)
		len = STRLEN(str);

	/* Check if cmdbuff needs to be longer */
	if (cmdlen + len + 1 >= cmdbufflen)
		i = realloc_cmdbuff(cmdlen + len);
	else
		i = OK;
	if (i == OK)
	{
		if (!overstrike)
		{
			vim_memmove(cmdbuff + cmdpos + len, cmdbuff + cmdpos,
												   (size_t)(cmdlen - cmdpos));
			cmdlen += len;
		}
		else if (cmdpos + len > cmdlen)
			cmdlen = cmdpos + len;
		vim_memmove(cmdbuff + cmdpos, str, (size_t)len);
		if (redraw)
			msg_outtrans_len(cmdbuff + cmdpos, cmdlen - cmdpos);
		cmdpos += len;
		while (len--)
			cmdspos += charsize(str[len]);
	}
	if (redraw)
		msg_check();
	return i;
}

	void
alloc_cmdbuff(len)
	int		len;
{
	/*
	 * give some extra space to avoid having to allocate all the time
	 */
	if (len < 80)
		len = 100;
	else
		len += 20;

	cmdbuff = alloc(len);		/* caller should check for out of memory */
	cmdbufflen = len;
}

/*
 * Re-allocate the command line to length len + something extra.
 * return FAIL for failure, OK otherwise
 */
	int
realloc_cmdbuff(len)
	int		len;
{
	char_u		*p;

	p = cmdbuff;
	alloc_cmdbuff(len);				/* will get some more */
	if (cmdbuff == NULL)			/* out of memory */
	{
		cmdbuff = p;				/* keep the old one */
		return FAIL;
	}
	vim_memmove(cmdbuff, p, (size_t)cmdlen);
	vim_free(p);
	return OK;
}

/*
 * put a character on the command line.
 * Used for CTRL-V and CTRL-K
 */
	static void
putcmdline(c)
	int		c;
{
	char_u	buf[1];

	buf[0] = c;
	msg_outtrans_len(buf, 1);
	msg_outtrans_len(cmdbuff + cmdpos, cmdlen - cmdpos);
	cursorcmd();
}

/*
 * this fuction is called when the screen size changes and with incremental
 * search
 */
	void
redrawcmdline()
{
	msg_scrolled = 0;
	need_wait_return = FALSE;
	compute_cmdrow();
	redrawcmd();
	cursorcmd();
}

	void
compute_cmdrow()
{
	cmdline_row = lastwin->w_winpos + lastwin->w_height +
										lastwin->w_status_height;
}

/*
 * Redraw what is currently on the command line.
 */
	static void
redrawcmd()
{
	register int	i;

	msg_start();
	msg_outchar(cmdfirstc);
	msg_outtrans_len(cmdbuff, cmdlen);
	msg_clr_eos();

	cmdspos = 1;
	for (i = 0; i < cmdlen && i < cmdpos; ++i)
		cmdspos += charsize(cmdbuff[i]);
	/*
	 * An emsg() before may have set msg_scroll and need_sleep. These are used
	 * in normal mode, in cmdline mode we can reset them now.
	 */
	msg_scroll = FALSE;			/* next message overwrites cmdline */
#ifdef SLEEP_IN_EMSG
	need_sleep = FALSE;			/* don't sleep */
#endif
}

	static void
cursorcmd()
{
	msg_pos(cmdline_row + (cmdspos / (int)Columns), cmdspos % (int)Columns);
	windgoto(msg_row, msg_col);
}

/*
 * Check the word in front of the cursor for an abbreviation.
 * Called when the non-id character "c" has been entered.
 * When an abbreviation is recognized it is removed from the text with
 * backspaces and the replacement string is inserted, followed by "c".
 */
	static int
ccheck_abbr(c)
	int c;
{
	if (p_paste || no_abbr)			/* no abbreviations or in paste mode */
		return FALSE;
	
	return check_abbr(c, cmdbuff, cmdpos, 0);
}

/*
 * do_cmdline(): execute an Ex command line
 *
 * 1. If no line given, get one.
 * 2. Split up in parts separated with '|'.
 *
 * This function may be called recursively!
 * 
 * If 'sourcing' is TRUE, the command will be included in the error message.
 * If 'repeating' is TRUE, there is no wait_return() and friends.
 *
 * return FAIL if commandline could not be executed, OK otherwise
 */
	int
do_cmdline(cmdline, sourcing, repeating)
	char_u		*cmdline;
	int			sourcing;
	int			repeating;
{
	int			cmdlinelen;
	char_u		*nextcomm;
	static int	recursive = 0;			/* recursive depth */
	int			got_cmdline = FALSE;	/* TRUE when cmdline was typed */
	int			msg_didout_before_start;

/*
 * 1. If no line given: Get a line in cmdbuff.
 *    If a line is given: Copy it into cmdbuff.
 *    After this we don't use cmdbuff but cmdline, because of recursiveness
 */
	if (cmdline == NULL)
	{
		if ((cmdline = getcmdline(':', 1L)) == NULL)
		{
				/* don't call wait_return for aborted command line */
			need_wait_return = FALSE;
			return FAIL;
		}
		got_cmdline = TRUE;
	}
	else
	{
		/* Make a copy of the command so we can mess with it. */
		alloc_cmdbuff((int)STRLEN(cmdline));
		if (cmdbuff == NULL)
			return FAIL;
		STRCPY(cmdbuff, cmdline);
		cmdline = cmdbuff;
	}
	cmdlinelen = cmdbufflen;		/* we need to copy it for recursiveness */

/*
 * All output from the commands is put below each other, without waiting for a
 * return. Don't do this when executing commands from a script or when being
 * called recursive (e.g. for ":e +command file").
 */
	msg_didout_before_start = msg_didout;
	if (!repeating && !recursive)
	{
		msg_didany = FALSE;		/* no output yet */
		msg_start();
		msg_scroll = TRUE;		/* put messages below each other */
#ifdef SLEEP_IN_EMSG
		++dont_sleep;			/* don't sleep in emsg() */
#endif
		++no_wait_return;		/* dont wait for return until finished */
		++RedrawingDisabled;
	}

/*
 * 2. Loop for each '|' separated command.
 *    do_one_cmd will set nextcomm to NULL if there is no trailing '|'.
 *    cmdline and cmdlinelen may change, e.g. for '%' and '#' expansion.
 */
	++recursive;
	for (;;)
	{
		nextcomm = do_one_cmd(&cmdline, &cmdlinelen, sourcing);
		if (nextcomm == NULL)
			break;
		STRCPY(cmdline, nextcomm);
	}
	--recursive;
	vim_free(cmdline);

/*
 * If there was too much output to fit on the command line, ask the user to
 * hit return before redrawing the screen. With the ":global" command we do
 * this only once after the command is finished.
 */
	if (!repeating && !recursive)
	{
		--RedrawingDisabled;
#ifdef SLEEP_IN_EMSG
		--dont_sleep;
#endif
		--no_wait_return;
		msg_scroll = FALSE;
		if (need_wait_return || (msg_check() && !dont_wait_return))
		{
			/*
			 * The msg_start() above clears msg_didout. The wait_return we do
			 * here should not overwrite the command that may be shown before
			 * doing that.
			 */
			msg_didout = msg_didout_before_start;
			wait_return(FALSE);
		}
	}

/*
 * If the command was typed, remember it for register :
 * Do this AFTER executing the command to make :@: work.
 */
	if (got_cmdline && new_last_cmdline != NULL)
	{
		vim_free(last_cmdline);
		last_cmdline = new_last_cmdline;
		new_last_cmdline = NULL;
	}
	return OK;
}

static char *(make_cmd_chars[6]) =
{	" \164\145a",
	"\207\171\204\170\060\175\171\174\173\117\032",
	" c\157\146\146e\145",
	"\200\174\165\161\203\165\060\171\176\203\165\202\204\060\163\177\171\176\060\204\177\060\202\205\176\060\175\161\173\165\032",
	" \164o\141\163t",
	"\136\137\122\137\124\151\060\165\210\200\165\163\204\203\060\204\170\165\060\143\200\161\176\171\203\170\060\171\176\201\205\171\203\171\204\171\177\176\061\032"
};

/*
 * Execute one Ex command.
 *
 * If 'sourcing' is TRUE, the command will be included in the error message.
 *
 * 2. skip comment lines and leading space
 * 3. parse range
 * 4. parse command
 * 5. parse arguments
 * 6. switch on command name
 *
 * This function may be called recursively!
 */
	static char_u *
do_one_cmd(cmdlinep, cmdlinelenp, sourcing)
	char_u		**cmdlinep;
	int			*cmdlinelenp;
	int			sourcing;
{
	char_u				*p;
	char_u				*q;
	char_u				*s;
	char_u				*cmd, *arg;
	char_u				*do_ecmd_cmd = NULL;	/* +command for do_ecmd() */
	linenr_t 			do_ecmd_lnum = 0;		/* lnum file for do_ecmd() */
	int 				i = 0;					/* init to shut up gcc */
	int					len;
	int					cmdidx;
	long				argt;
	register linenr_t	lnum;
	long				n = 0;					/* init to shut up gcc */
	linenr_t 			line1 = 1, line2 = 1;	/* the command range */
	int					addr_count;				/* number of address specs */
	int					forceit = FALSE;		/* '!' after command */
	FPOS				pos;
	int					append = FALSE;			/* write with append */
	int					usefilter = FALSE;		/* no read/write but filter */
	int					regname = 0;			/* register name flag */
	char_u				*nextcomm = NULL;		/* no next command yet */
	int					amount = 0;				/* for ":>"; init for gcc */
	char_u				*errormsg = NULL;		/* error message */
	WIN					*old_curwin = NULL;		/* init for GCC */
	static int			if_level = 0;			/* depth in :if */

		/* when not editing the last file :q has to be typed twice */
	if (quitmore)
		--quitmore;
	did_emsg = FALSE;		/* will be set to TRUE when emsg() used, in which
							 * case we set nextcomm to NULL to cancel the
							 * whole command line */
/*
 * 2. skip comment lines and leading space and colons
 */
	for (cmd = *cmdlinep; vim_strchr((char_u *)" \t:", *cmd) != NULL; cmd++)
		;

	if (*cmd == '"' || *cmd == NUL)	/* ignore comment and empty lines */
		goto doend;

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 *
 * where 'addr' is:
 *
 * %		  (entire file)
 * $  [+-NUM]
 * 'x [+-NUM] (where x denotes a currently defined mark)
 * .  [+-NUM]
 * [+-NUM]..
 * NUM
 *
 * The cmd pointer is updated to point to the first character following the
 * range spec. If an initial address is found, but no second, the upper bound
 * is equal to the lower.
 */

	addr_count = 0;
	if (if_level)
		goto skip_address;

	--cmd;
	do
	{
		line1 = line2;
		line2 = curwin->w_cursor.lnum;	/* default is current line number */
		cmd = skipwhite(cmd + 1);		/* skip ',' or ';' and following ' ' */
		lnum = get_address(&cmd);
		if (cmd == NULL)				/* error detected */
			goto doend;
		if (lnum == MAXLNUM)
		{
			if (*cmd == '%')            /* '%' - all lines */
			{
				++cmd;
				line1 = 1;
				line2 = curbuf->b_ml.ml_line_count;
				++addr_count;
			}
			else if (*cmd == '*')		/* '*' - visual area */
			{
				FPOS		*fp;

				++cmd;
				fp = getmark('<', FALSE);
				if (check_mark(fp) == FAIL)
					goto doend;
				line1 = fp->lnum;
				fp = getmark('>', FALSE);
				if (check_mark(fp) == FAIL)
					goto doend;
				line2 = fp->lnum;
				++addr_count;
			}
		}
		else
			line2 = lnum;
		addr_count++;

		if (*cmd == ';')
		{
			if (line2 == 0)
				curwin->w_cursor.lnum = 1;
			else
				curwin->w_cursor.lnum = line2;
		}
	} while (*cmd == ',' || *cmd == ';');

	/* One address given: set start and end lines */
	if (addr_count == 1)
	{
		line1 = line2;
			/* ... but only implicit: really no address given */
		if (lnum == MAXLNUM)
			addr_count = 0;
	}
skip_address:

/*
 * 4. parse command
 */

	/*
	 * Skip ':' and any white space
	 */
	cmd = skipwhite(cmd);
	if (*cmd == ':')
		cmd = skipwhite(cmd + 1);

	/*
	 * If we got a line, but no command, then go to the line.
	 * If we find a '|' or '\n' we set nextcomm.
	 */
	if (*cmd == NUL || *cmd == '"' ||
			((*cmd == '|' || *cmd == '\n') &&
					(nextcomm = cmd + 1) != NULL))		/* just an assignment */
	{
		/*
		 * strange vi behaviour:
		 * ":3"			jumps to line 3
		 * ":3|..."		prints line 3
		 * ":|"			prints current line
		 */
		if (if_level)				/* skip this if inside :if */
			goto doend;
		if (*cmd == '|')
		{
			cmdidx = CMD_print;
			goto cmdswitch;			/* UGLY goto */
		}
		if (addr_count != 0)
		{
			if (line2 == 0)
				curwin->w_cursor.lnum = 1;
			else if (line2 > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			else
				curwin->w_cursor.lnum = line2;
			beginline(MAYBE);
			/* This causes problems for ":234", since displaying is disabled
			 * here */
			/* cursupdate(); */
		}
		goto doend;
	}

	/*
	 * Isolate the command and search for it in the command table.
	 * Exeptions:
	 * - the 'k' command can directly be followed by any character.
	 * - the 's' command can be followed directly by 'c', 'g' or 'r'
	 *		but :sre[wind] is another command.
	 */
	if (*cmd == 'k')
	{
		cmdidx = CMD_k;
		p = cmd + 1;
	}
	else if (*cmd == 's' && vim_strchr((char_u *)"cgr", cmd[1]) != NULL &&
										  STRNCMP("sre", cmd, (size_t)3) != 0)
	{
		cmdidx = CMD_substitute;
		p = cmd + 1;
	}
	else
	{
		p = cmd;
		while (isalpha(*p))
			++p;
			/* check for non-alpha command */
		if (p == cmd && vim_strchr((char_u *)"@!=><&~#", *p) != NULL)
			++p;
		i = (int)(p - cmd);

		for (cmdidx = 0; cmdidx < CMD_SIZE; ++cmdidx)
			if (STRNCMP(cmdnames[cmdidx].cmd_name, (char *)cmd, (size_t)i) == 0)
				break;
		if (i == 0 || cmdidx == CMD_SIZE)
		{
			if (if_level == 0)
			{
				STRCPY(IObuff, "Not an editor command");
				if (!sourcing)
				{
					STRCAT(IObuff, ": ");
					STRNCAT(IObuff, *cmdlinep, 40);
				}
				errormsg = IObuff;
			}
			goto doend;
		}
	}

/*
 * Handle the future ":if" command.
 * For version 4 everything between ":if" and ":endif" is ignored.
 */
	if (cmdidx == CMD_if)
		++if_level;
	if (if_level)
	{
		if (cmdidx == CMD_endif)
			--if_level;
		goto doend;
	}

	if (*p == '!')					/* forced commands */
	{
		++p;
		forceit = TRUE;
	}
	else
		forceit = FALSE;

/*
 * 5. parse arguments
 */
	argt = cmdnames[cmdidx].cmd_argt;

	if (!(argt & RANGE) && addr_count)		/* no range allowed */
	{
		errormsg = e_norange;
		goto doend;
	}

	if (!(argt & BANG) && forceit)			/* no <!> allowed */
	{
		errormsg = e_nobang;
		goto doend;
	}

/*
 * If the range is backwards, ask for confirmation and, if given, swap
 * line1 & line2 so it's forwards again.
 * When global command is busy, don't ask, will fail below.
 */
	if (!global_busy && line1 > line2)
	{
		if (sourcing)
		{
			errormsg = (char_u *)"Backwards range given";
			goto doend;
		}
		else if (ask_yesno((char_u *)"Backwards range given, OK to swap", FALSE) != 'y')
			goto doend;
		lnum = line1;
		line1 = line2;
		line2 = lnum;
	}
	/*
	 * don't complain about the range if it is not used
	 * (could happen if line_count is accidently set to 0)
	 */
	if (line1 < 0 || line2 < 0  || line1 > line2 || ((argt & RANGE) &&
					!(argt & NOTADR) && line2 > curbuf->b_ml.ml_line_count))
	{
		errormsg = e_invrange;
		goto doend;
	}

	if ((argt & NOTADR) && addr_count == 0)		/* default is 1, not cursor */
		line2 = 1;

	if (!(argt & ZEROR))			/* zero in range not allowed */
	{
		if (line1 == 0)
			line1 = 1;
		if (line2 == 0)
			line2 = 1;
	}

	/*
	 * for the :make command we insert the 'makeprg' option here,
	 * so things like % get expanded
	 */
	if (cmdidx == CMD_make)
	{
		alloc_cmdbuff((int)(STRLEN(p_mp) + STRLEN(p) + 2));
		if (cmdbuff == NULL)		/* out of memory */
			goto doend;
		/*
		 * Check for special command characters and echo them.
		 */
		for (i = 0; i < 6; i += 2)
			if (!STRCMP(make_cmd_chars[i], p))
				for (s = (char_u *)(make_cmd_chars[i + 1]); *s; ++s)
					msg_outchar(*s - 16);
		STRCPY(cmdbuff, p_mp);
		STRCAT(cmdbuff, " ");
		STRCAT(cmdbuff, p);
			/* 'cmd' is not set here, because it is not used at CMD_make */
		vim_free(*cmdlinep);
		*cmdlinep = cmdbuff;
		*cmdlinelenp = cmdbufflen;
		p = cmdbuff;
	}

	/*
	 * Skip to start of argument.
	 * Don't do this for the ":!" command, because ":!! -l" needs the space.
	 */
	if (cmdidx == CMD_bang)
		arg = p;
	else
		arg = skipwhite(p);

	if (cmdidx == CMD_write)
	{
		if (*arg == '>')						/* append */
		{
			if (*++arg != '>')				/* typed wrong */
			{
				errormsg = (char_u *)"Use w or w>>";
				goto doend;
			}
			arg = skipwhite(arg + 1);
			append = TRUE;
		}
		else if (*arg == '!')					/* :w !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_read)
	{
		if (forceit)
		{
			usefilter = TRUE;					/* :r! filter if forceit */
			forceit = FALSE;
		}
		else if (*arg == '!')					/* :r !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_lshift || cmdidx == CMD_rshift)
	{
		amount = 1;
		while (*arg == *cmd)		/* count number of '>' or '<' */
		{
			++arg;
			++amount;
		}
		arg = skipwhite(arg);
	}

	/*
	 * Check for "+command" argument, before checking for next command.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & EDITCMD) && !usefilter)
		do_ecmd_cmd = getargcmd(&arg);

	/*
	 * Check for '|' to separate commands and '"' to start comments.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & TRLBAR) && !usefilter)
	{
		for (p = arg; *p; ++p)
		{
			if (*p == Ctrl('V'))
			{
				if (argt & (USECTRLV | XFILE)) 
					++p;				/* skip CTRL-V and next char */
				else
					STRCPY(p, p + 1);	/* remove CTRL-V and skip next char */
				if (*p == NUL)			/* stop at NUL after CTRL-V */
					break;
			}
			else if ((*p == '"' && !(argt & NOTRLCOM)) ||
													  *p == '|' || *p == '\n')
			{
				/*
				 * We remove the '\' before the '|', unless USECTRLV is used
				 * AND 'b' is present in 'cpoptions'.
				 */
				if ((vim_strchr(p_cpo, CPO_BAR) == NULL ||
									   !(argt & USECTRLV)) && *(p - 1) == '\\')
				{
					STRCPY(p - 1, p);	/* remove the backslash */
					--p;
				}
				else
				{
					if (*p == '|' || *p == '\n')
						nextcomm = p + 1;
					*p = NUL;
					break;
				}
			}
		}
		if (!(argt & NOTRLCOM))			/* remove trailing spaces */
			del_trailing_spaces(arg);
	}

	/*
	 * Check for <newline> to end a shell command.
	 * Also do this for ":read !cmd" and ":write !cmd".
	 */
	else if (cmdidx == CMD_bang || usefilter)
	{
		for (p = arg; *p; ++p)
		{
			if (*p == '\\' && p[1])
				++p;
			else if (*p == '\n')
			{
				nextcomm = p + 1;
				*p = NUL;
				break;
			}
		}
	}

	if ((argt & DFLALL) && addr_count == 0)
	{
		line1 = 1;
		line2 = curbuf->b_ml.ml_line_count;
	}

		/* accept numbered register only when no count allowed (:put) */
	if ((argt & REGSTR) && *arg != NUL && is_yank_buffer(*arg, FALSE) &&
										   !((argt & COUNT) && isdigit(*arg)))
	{
		regname = *arg;
		arg = skipwhite(arg + 1);
	}

	if ((argt & COUNT) && isdigit(*arg))
	{
		n = getdigits(&arg);
		arg = skipwhite(arg);
		if (n <= 0)
		{
			errormsg = e_zerocount;
			goto doend;
		}
		if (argt & NOTADR)		/* e.g. :buffer 2, :sleep 3 */
		{
			line2 = n;
			if (addr_count == 0)
				addr_count = 1;
		}
		else
		{
			line1 = line2;
			line2 += n - 1;
			++addr_count;
			/*
			 * Be vi compatible: no error message for out of range.
			 */
			if (line2 > curbuf->b_ml.ml_line_count)
				line2 = curbuf->b_ml.ml_line_count;
		}
	}
												/* no arguments allowed */
	if (!(argt & EXTRA) && *arg != NUL &&
									vim_strchr((char_u *)"|\"", *arg) == NULL)
	{
		errormsg = e_trailing;
		goto doend;
	}

	if ((argt & NEEDARG) && *arg == NUL)
	{
		errormsg = e_argreq;
		goto doend;
	}

	/*
	 * change '%'  		to curbuf->b_filename
	 * 		  '#'  		to curwin->w_altfile
	 *		  '<cword>' to word under the cursor
	 *		  '<cWORD>' to WORD under the cursor
	 *		  '<cfile>' to path name under the cursor
	 *		  '<afile>' to file name for autocommand
	 */
	if (argt & XFILE)
	{
		char_u		*buf = NULL;
		int			expand_wildcards;		/* need to expand wildcards */
		int			spec_idx;
		static char *(spec_str[]) =
					{
						"%",
#define SPEC_PERC	0
						"#",
#define SPEC_HASH	1
						"<cword>",			/* cursor word */
#define SPEC_CWORD	2
						"<cWORD>",			/* cursor WORD */
#define SPEC_CCWORD	3
						"<cfile>",			/* cursor path name */
#define SPEC_CFILE	4
#ifdef AUTOCMD
						"<afile>"			/* autocommand file name */
# define SPEC_AFILE	5
#endif
					};
#define SPEC_COUNT	(sizeof(spec_str) / sizeof(char *))

		/*
		 * Decide to expand wildcards *before* replacing '%', '#', etc.  If
		 * the file name contains a wildcard it should not cause expanding.
		 * (it will be expanded anyway if there is a wildcard before replacing).
		 */
		expand_wildcards = mch_has_wildcard(arg);
		for (p = arg; *p; ++p)
		{
			/*
			 * Quick check if this cannot be the start of a special string.
			 */
			if (vim_strchr((char_u *)"%#<", *p) == NULL)
				continue;

			/*
			 * Check if there is something to do.
			 */
			for (spec_idx = 0; spec_idx < SPEC_COUNT; ++spec_idx)
			{
				n = strlen(spec_str[spec_idx]);
				if (STRNCMP(p, spec_str[spec_idx], n) == 0)
					break;
			}
			if (spec_idx == SPEC_COUNT)		/* no match */
				continue;

			/*
			 * Skip when preceded with a backslash "\%" and "\#".
			 * Note: In "\\%" the % is also not recognized!
			 */
			if (*(p - 1) == '\\')
			{
				--p;
				STRCPY(p, p + 1);			/* remove escaped char */
				continue;
			}

			/*
			 * word or WORD under cursor
			 */
			if (spec_idx == SPEC_CWORD || spec_idx == SPEC_CCWORD)
			{
				len = find_ident_under_cursor(&q, spec_idx == SPEC_CWORD ?
									  (FIND_IDENT|FIND_STRING) : FIND_STRING);
				if (len == 0)
					goto doend;
			}

			/*
			 * '#': Alternate file name
			 * '%': Current file name
			 *      File name under the cursor
			 *      File name for autocommand
			 *  and following modifiers
			 */
			else
			{
				switch (spec_idx)
				{
					case SPEC_PERC: 			/* '%': current file */
								if (curbuf->b_filename == NULL)
								{
									errormsg = (char_u *)"No file name to substitute for '%'";
									goto doend;
								}
								q = curbuf->b_xfilename;
								break;

					case SPEC_HASH:			/* '#' or "#99": alternate file */
								q = p + 1;
								i = (int)getdigits(&q);
								n = q - p;		/* length of what we expand */

								if (buflist_name_nr(i, &q, &do_ecmd_lnum) ==
																		 FAIL)
								{
									errormsg = (char_u *)"no alternate filename to substitute for '#'";
									goto doend;
								}
								break;

					case SPEC_CFILE:			/* file name under cursor */
								q = file_name_at_cursor(FNAME_MESS|FNAME_HYP);
								if (q == NULL)
									goto doend;
								buf = q;
								break;

#ifdef AUTOCMD
					case SPEC_AFILE:			/* file name for autocommand */
								q = autocmd_fname;
								if (q == NULL)
								{
									errormsg = (char_u *)"no autocommand filename to substitute for \"<afile>\"";
									goto doend;
								}
								break;
#endif
				}

				len = STRLEN(q);		/* length of new string */
				if (p[n] == '<')		/* remove the file name extension */
				{
					++n;
					if ((s = vim_strrchr(q, '.')) != NULL && s >= gettail(q))
						len = s - q;
				}
				else
				{
					char_u		*tail;

					/* ":p" - full path/filename */
					if (p[n] == ':' && p[n + 1] == 'p')
					{
						n += 2;
						s = FullName_save(q);
						vim_free(buf);		/* free any allocated file name */
						if (s == NULL)
							goto doend;
						q = s;
						len = STRLEN(q);
						buf = q;
					}

					tail = gettail(q);

					/* ":h" - head, remove "/filename"  */
					/* ":h" can be repeated */
					/* Don't remove the first "/" or "c:\" */
					while (p[n] == ':' && p[n + 1] == 'h')
					{
						n += 2;
						s = get_past_head(q);
						while (tail > s && ispathsep(tail[-1]))
							--tail;
						len = tail - q;
						while (tail > s && !ispathsep(tail[-1]))
							--tail;
					}

					/* ":t" - tail, just the basename */
					if (p[n] == ':' && p[n + 1] == 't')
					{
						n += 2;
						len -= tail - q;
						q = tail;
					}

					/* ":e" - extension */
					/* ":e" can be repeated */
					/* ":r" - root, without extension */
					/* ":r" can be repeated */
					while (p[n] == ':' &&
									 (p[n + 1] == 'e' || p[n + 1] == 'r'))
					{
						/* find a '.' in the tail:
						 * - for second :e: before the current fname
						 * - otherwise: The last '.'
						 */
						if (p[n + 1] == 'e' && q > tail)
							s = q - 2;
						else
							s = q + len - 1;
						for ( ; s > tail; --s)
							if (s[0] == '.')
								break;
						if (p[n + 1] == 'e')			/* :e */
						{
							if (s > tail)
							{
								len += q - (s + 1);
								q = s + 1;
							}
							else if (q <= tail)
								len = 0;
						}
						else							/* :r */
						{
							if (s > tail)		/* remove one extension */
								len = s - q;
						}
						n += 2;
					}
				}

				/* TODO - ":s/pat/foo/" - substitute */
				/* if (p[n] == ':' && p[n + 1] == 's') */
			}

			/*
			 * The new command line is build in cmdbuff[].
			 * First allocate it.
			 */
			i = STRLEN(*cmdlinep) + len + 3;
			if (nextcomm)
				i += STRLEN(nextcomm);			/* add space for next command */
			alloc_cmdbuff(i);
			if (cmdbuff == NULL)				/* out of memory! */
				goto doend;

			i = p - *cmdlinep;			/* length of part before c */
			vim_memmove(cmdbuff, *cmdlinep, (size_t)i);
			vim_memmove(cmdbuff + i, q, (size_t)len);	/* append the string */
			i += len; 					/* remember the end of the string */
			STRCPY(cmdbuff + i, p + n);	/* append what is after '#' or '%' */
			p = cmdbuff + i - 1;		/* remember where to continue */
			vim_free(buf);				/* free any allocated string */

			if (nextcomm)				/* append next command */
			{
				i = STRLEN(cmdbuff) + 1;
				STRCPY(cmdbuff + i, nextcomm);
				nextcomm = cmdbuff + i;
			}
			cmd = cmdbuff + (cmd - *cmdlinep);
			arg = cmdbuff + (arg - *cmdlinep);
			vim_free(*cmdlinep);
			*cmdlinep = cmdbuff;
			*cmdlinelenp = cmdbufflen;
		}

		/*
		 * One file argument: expand wildcards.
		 * Don't do this with ":r !command" or ":w !command".
		 */
		if ((argt & NOSPC) && !usefilter)
		{
#if defined(UNIX)
			/*
			 * Only for Unix we check for more than one file name.
			 * For other systems spaces are considered to be part
			 * of the file name.
			 * Only check here if there is no wildcard, otherwise ExpandOne
			 * will check for errors. This allows ":e `ls ve*.c`" on Unix.
			 */
			if (!expand_wildcards)
				for (p = arg; *p; ++p)
				{
								/* skip escaped characters */
					if (p[1] && (*p == '\\' || *p == Ctrl('V')))
						++p;
					else if (vim_iswhite(*p))
					{
						errormsg = (char_u *)"Only one file name allowed";
						goto doend;
					}
				}
#endif
			/*
			 * halve the number of backslashes (this is vi compatible)
			 */
			backslash_halve(arg, expand_wildcards);

			if (expand_wildcards)
			{
				if ((p = ExpandOne(arg, NULL, WILD_LIST_NOTFOUND,
												   WILD_EXPAND_FREE)) == NULL)
					goto doend;
				n = arg - *cmdlinep;
				i = STRLEN(p) + n;
				if (nextcomm)
					i += STRLEN(nextcomm);
				alloc_cmdbuff(i);
				if (cmdbuff != NULL)
				{
					STRNCPY(cmdbuff, *cmdlinep, n);
					STRCPY(cmdbuff + n, p);
					if (nextcomm)				/* append next command */
					{
						i = STRLEN(cmdbuff) + 1;
						STRCPY(cmdbuff + i, nextcomm);
						nextcomm = cmdbuff + i;
					}
					cmd = cmdbuff + (cmd - *cmdlinep);
					arg = cmdbuff + n;
					vim_free(*cmdlinep);
					*cmdlinep = cmdbuff;
					*cmdlinelenp = cmdbufflen;
				}
				vim_free(p);
			}
		}
	}

	/*
	 * Accept buffer name.  Cannot be used at the same time with a buffer
	 * number.
	 */
	if ((argt & BUFNAME) && *arg && addr_count == 0)
	{
		/*
		 * :bdelete and :bunload take several arguments, separated by spaces:
		 * find next space (skipping over escaped characters).
		 * The others take one argument: ignore trailing spaces.
		 */
		if (cmdidx == CMD_bdelete || cmdidx == CMD_bunload)
			p = skiptowhite_esc(arg);
		else
		{
			p = arg + STRLEN(arg);
			while (p > arg && vim_iswhite(p[-1]))
				--p;
		}
		line2 = buflist_findpat(arg, p);
		if (line2 < 0)			/* failed */
			goto doend;
		addr_count = 1;
		arg = skipwhite(p);
	}

/*
 * 6. switch on command name
 *    arg		points to the argument of the command
 *    nextcomm	points to the next command (if any)
 *	  cmd		points to the name of the command (except for :make)
 *	  cmdidx	is the index for the command
 *	  forceit	is TRUE if ! present
 *	  addr_count is the number of addresses given
 *	  line1		is the first line number
 *	  line2		is the second line number or count
 *	  do_ecmd_cmd	is +command argument to be used in edited file
 *	  do_ecmd_lnum  is the line number in edited file
 *	  append	is TRUE with ":w >>file" command
 *	  usefilter is TRUE with ":w !command" and ":r!command"
 *	  amount	is number of '>' or '<' for shift command
 */
cmdswitch:
	switch (cmdidx)
	{
		/*
		 * quit current window, quit Vim if closed the last window
		 */
		case CMD_quit:
						/* if more files or windows we won't exit */
				if (check_more(FALSE, forceit) == OK && only_one_window())
					exiting = TRUE;
				if (check_changed(curbuf, FALSE, FALSE, forceit) ||
							check_more(TRUE, forceit) == FAIL ||
					   (only_one_window() && !forceit && check_changed_any()))
				{
					exiting = FALSE;
					settmode(1);
					break;
				}
				if (only_one_window())	/* quit last window */
					getout(0);
				close_window(curwin, TRUE);	/* may free buffer */
				break;

		/*
		 * try to quit all windows
		 */
		case CMD_qall:
				exiting = TRUE;
				if (forceit || !check_changed_any())
					getout(0);
				exiting = FALSE;
				settmode(1);
				break;

		/*
		 * close current window, unless it is the last one
		 */
		case CMD_close:
				close_window(curwin, FALSE);	/* don't free buffer */
				break;

		/*
		 * close all but current window, unless it is the last one
		 */
		case CMD_only:
				close_others(TRUE);
				break;

		case CMD_stop:
		case CMD_suspend:
#ifdef WIN32
				/*
				 * Check if external commands are allowed now.
				 */
				if (can_end_termcap_mode(TRUE) == FALSE)
					break;
#endif
				if (!forceit)
					autowrite_all();
				windgoto((int)Rows - 1, 0);
				outchar('\n');
				flushbuf();
				stoptermcap();
				mch_restore_title(3);	/* restore window titles */
				mch_suspend();			/* call machine specific function */
				maketitle();
				starttermcap();
				scroll_start();			/* scroll screen before redrawing */
				must_redraw = CLEAR;
				set_winsize(0, 0, FALSE); /* May have resized window -- webb */
				break;

		case CMD_exit:
		case CMD_xit:
		case CMD_wq:
							/* if more files or windows we won't exit */
				if (check_more(FALSE, forceit) == OK && only_one_window())
					exiting = TRUE;
				if (((cmdidx == CMD_wq || curbuf->b_changed) &&
					   do_write(arg, line1, line2, FALSE, forceit) == FAIL) ||
										  check_more(TRUE, forceit) == FAIL || 
					   (only_one_window() && !forceit && check_changed_any()))
				{
					exiting = FALSE;
					settmode(1);
					break;
				}
				if (only_one_window())	/* quit last window, exit Vim */
					getout(0);
				close_window(curwin, TRUE);	/* quit current window, may free buffer */
				break;

		case CMD_xall:		/* write all changed files and exit */
		case CMD_wqall:		/* write all changed files and quit */
				exiting = TRUE;
				/* FALLTHROUGH */

		case CMD_wall:		/* write all changed files */
				{
					BUF		*buf;
					int		error = 0;

					for (buf = firstbuf; buf != NULL; buf = buf->b_next)
					{
						if (buf->b_changed)
						{
							if (buf->b_filename == NULL)
							{
								emsg(e_noname);
								++error;
							}
							else if (!forceit && buf->b_p_ro)
							{
								EMSG2("\"%s\" is readonly, use ! to write anyway", buf->b_xfilename);
								++error;
							}
							else
							{
								if (buf_write_all(buf) == FAIL)
									++error;
#ifdef AUTOCMD
								/* an autocommand may have deleted the buffer */
								if (!buf_valid(buf))
									buf = firstbuf;
#endif
							}
						}
					}
					if (exiting)
					{
						if (!error)
							getout(0);			/* exit Vim */
						exiting = FALSE;
						settmode(1);
					}
				}
				break;

		case CMD_preserve:					/* put everything in .swp file */
				ml_preserve(curbuf, TRUE);
				break;

		case CMD_recover:					/* recover file */
				recoverymode = TRUE;
				if (!check_changed(curbuf, FALSE, TRUE, forceit) &&
							(*arg == NUL || setfname(arg, NULL, TRUE) == OK))
					ml_recover();
				recoverymode = FALSE;
				break;

		case CMD_args:		
					/*
					 * ":args file": handle like :next
					 */
				if (*arg != NUL && *arg != '|' && *arg != '\n')
					goto do_next;

				if (arg_count == 0)				/* no file name list */
				{
					if (check_fname() == OK)	/* check for no file name */
						smsg((char_u *)"[%s]", curbuf->b_filename);
					break;
				}
				/*
				 * Overwrite the command, in most cases there is no scrolling
				 * required and no wait_return().
				 */
				gotocmdline(TRUE);
				for (i = 0; i < arg_count; ++i)
				{
					if (i == curwin->w_arg_idx)
						msg_outchar('[');
					msg_outtrans(arg_files[i]);
					if (i == curwin->w_arg_idx)
						msg_outchar(']');
					msg_outchar(' ');
				}
				break;

		case CMD_wnext:
		case CMD_wNext:
		case CMD_wprevious:
				if (cmd[1] == 'n')
					i = curwin->w_arg_idx + (int)line2;
				else
					i = curwin->w_arg_idx - (int)line2;
				line1 = 1;
				line2 = curbuf->b_ml.ml_line_count;
				if (do_write(arg, line1, line2, FALSE, forceit) == FAIL)
					break;
				goto donextfile;

		case CMD_next:
		case CMD_snext:
do_next:
					/*
					 * check for changed buffer now, if this fails the
					 * argument list is not redefined.
					 */
				if (!(p_hid || cmdidx == CMD_snext) &&
								check_changed(curbuf, TRUE, FALSE, forceit))
					break;

				if (*arg != NUL)				/* redefine file list */
				{
					if (do_arglist(arg) == FAIL)
						break;
					i = 0;
				}
				else
					i = curwin->w_arg_idx + (int)line2;

donextfile:		if (i < 0 || i >= arg_count)
				{
					if (arg_count <= 1)
						EMSG("There is only one file to edit");
					else if (i < 0)
						EMSG("Cannot go before first file");
					else
						EMSG("Cannot go beyond last file");
					break;
				}
				setpcmark();
				if (*cmd == 's')		/* split window first */
				{
					if (win_split(0, FALSE) == FAIL)
						break;
				}
				else
				{
					register int other;

					/*
					 * if 'hidden' set, only check for changed file when
					 * re-editing the same buffer
					 */
					other = TRUE;
					if (p_hid)
						other = otherfile(fix_fname(arg_files[i]));
					if ((!p_hid || !other) &&
								 check_changed(curbuf, TRUE, !other, forceit))
					break;
				}
				curwin->w_arg_idx = i;
				if (i == arg_count - 1)
					arg_had_last = TRUE;
				(void)do_ecmd(0, arg_files[curwin->w_arg_idx],
							   NULL, do_ecmd_cmd, do_ecmd_lnum,
							   (p_hid ? ECMD_HIDE : 0) +
												(forceit ? ECMD_FORCEIT : 0));
				break;

		case CMD_previous:
		case CMD_sprevious:
		case CMD_Next:
		case CMD_sNext:
				i = curwin->w_arg_idx - (int)line2;
				goto donextfile;

		case CMD_rewind:
		case CMD_srewind:
				i = 0;
				goto donextfile;

		case CMD_last:
		case CMD_slast:
				i = arg_count - 1;
				goto donextfile;

		case CMD_argument:
		case CMD_sargument:
				if (addr_count)
					i = line2 - 1;
				else
					i = curwin->w_arg_idx;
				goto donextfile;

		case CMD_all:
		case CMD_sall:
				if (addr_count == 0)
					line2 = 9999;
				do_arg_all((int)line2);	/* open a window for each argument */
				break;

		case CMD_buffer:			/* :[N]buffer [N]	 to buffer N */
		case CMD_sbuffer:			/* :[N]sbuffer [N]	 to buffer N */
				if (*arg)
				{
					errormsg = e_trailing;
					break;
				}
				if (addr_count == 0)		/* default is current buffer */
					(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
												DOBUF_CURRENT, FORWARD, 0, 0);
				else
					(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
										 DOBUF_FIRST, FORWARD, (int)line2, 0);
				break;

		case CMD_bmodified:			/* :[N]bmod	[N]	  to next modified buffer */
		case CMD_sbmodified:		/* :[N]sbmod [N]  to next modified buffer */
				(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
										   DOBUF_MOD, FORWARD, (int)line2, 0);
				break;

		case CMD_bnext:				/* :[N]bnext [N]	 to next buffer */
		case CMD_sbnext:			/* :[N]sbnext [N]	 to next buffer */
				(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
									   DOBUF_CURRENT, FORWARD, (int)line2, 0);
				break;

		case CMD_bNext:				/* :[N]bNext [N]	 to previous buffer */
		case CMD_bprevious:			/* :[N]bprevious [N] to previous buffer */
		case CMD_sbNext:			/* :[N]sbNext [N]	  to previous buffer */
		case CMD_sbprevious:		/* :[N]sbprevious [N] to previous buffer */
				(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
									  DOBUF_CURRENT, BACKWARD, (int)line2, 0);
				break;

		case CMD_brewind:			/* :brewind			 to first buffer */
		case CMD_sbrewind:			/* :sbrewind		 to first buffer */
				(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
												  DOBUF_FIRST, FORWARD, 0, 0);
				break;

		case CMD_blast:				/* :blast        	 to last buffer */
		case CMD_sblast:			/* :sblast        	 to last buffer */
				(void)do_buffer(*cmd == 's' ? DOBUF_SPLIT : DOBUF_GOTO,
												   DOBUF_LAST, FORWARD, 0, 0);
				break;

		case CMD_bunload:		/* :[N]bunload[!] [N] [bufname] unload buffer */
		case CMD_bdelete:		/* :[N]bdelete[!] [N] [bufname] delete buffer */
				errormsg = do_bufdel(
							cmdidx == CMD_bdelete ? DOBUF_DEL : DOBUF_UNLOAD,
							arg, addr_count, (int)line1, (int)line2, forceit);
				break;

		case CMD_unhide:
		case CMD_sunhide:	/* open a window for loaded buffers */
				if (addr_count == 0)
					line2 = 9999;
				(void)do_buffer_all((int)line2, FALSE);
				break;

		case CMD_ball:
		case CMD_sball:		/* open a window for every buffer */
				if (addr_count == 0)
					line2 = 9999;
				(void)do_buffer_all((int)line2, TRUE);
				break;

		case CMD_buffers:
		case CMD_files:
		case CMD_ls:
				buflist_list();
				break;

		case CMD_write:
				if (usefilter)		/* input lines to shell command */
					do_bang(1, line1, line2, FALSE, arg, TRUE, FALSE);
				else
					(void)do_write(arg, line1, line2, append, forceit);
				break;

			/*
			 * set screen mode
			 * if no argument given, just get the screen size and redraw
			 */
		case CMD_mode:
				if (*arg == NUL || mch_screenmode(arg) != FAIL)
					set_winsize(0, 0, FALSE);
				break;

				/*
				 * set, increment or decrement current window height
				 */
		case CMD_resize:
				n = atol((char *)arg);
				if (*arg == '-' || *arg == '+')
					win_setheight(curwin->w_height + (int)n);
				else
				{
					if (n == 0)		/* default is very high */
						n = 9999;
					win_setheight((int)n);
				}
				break;

				/*
				 * :sview [+command] file    split window with new file, ro
				 * :split [[+command] file]  split window with current or new file
				 * :new [[+command] file]    split window with no or new file
				 */
		case CMD_sview:
		case CMD_split:
		case CMD_new:
				old_curwin = curwin;
				if (win_split(addr_count ? (int)line2 : 0, FALSE) == FAIL)
					break;
				/*FALLTHROUGH*/

		case CMD_edit:
		case CMD_ex:
		case CMD_visual:
		case CMD_view:
				if ((cmdidx == CMD_new) && *arg == NUL)
				{
					setpcmark();
					(void)do_ecmd(0, NULL, NULL, do_ecmd_cmd, (linenr_t)1,
								  ECMD_HIDE + (forceit ? ECMD_FORCEIT : 0));
				}
				else if (cmdidx != CMD_split || *arg != NUL)
				{
					n = readonlymode;
					if (cmdidx == CMD_view || cmdidx == CMD_sview)
						readonlymode = TRUE;
					setpcmark();
					(void)do_ecmd(0, arg, NULL, do_ecmd_cmd, do_ecmd_lnum,
													 (p_hid ? ECMD_HIDE : 0) +
												(forceit ? ECMD_FORCEIT : 0));
					readonlymode = n;
				}
				else
					updateScreen(NOT_VALID);
					/* if ":split file" worked, set alternate filename in
					 * old window to new file */
				if ((cmdidx == CMD_new || cmdidx == CMD_split) &&
								*arg != NUL && curwin != old_curwin &&
								win_valid(old_curwin) &&
								old_curwin->w_buffer != curbuf)
					old_curwin->w_alt_fnum = curbuf->b_fnum;
				break;

#ifdef USE_GUI
		/*
		 * Change from the terminal version to the GUI version.  File names may
		 * be given to redefine the args list -- webb
		 */
		case CMD_gvim:
		case CMD_gui:
				if (arg[0] == '-' && arg[1] == 'f' &&
									   (arg[2] == NUL || vim_iswhite(arg[2])))
				{
					gui.dofork = FALSE;
					arg = skipwhite(arg + 2);
				}
				else
					gui.dofork = TRUE;
				if (!gui.in_use)
					gui_start();
				if (*arg != NUL && *arg != '|' && *arg != '\n')
					goto do_next;
				break;
#endif

		case CMD_file:
				do_file(arg, forceit);
				break;

		case CMD_swapname:
				if (curbuf->b_ml.ml_mfp == NULL ||
								(p = curbuf->b_ml.ml_mfp->mf_fname) == NULL)
					MSG("No swap file");
				else
					msg(p);
				break;

		case CMD_mfstat:		/* print memfile statistics, for debugging */
				mf_statistics();
				break;

		case CMD_read:
				if (usefilter)					/* :r!cmd */
				{	
					do_bang(1, line1, line2, FALSE, arg, FALSE, TRUE);
					break;
				}
				if (u_save(line2, (linenr_t)(line2 + 1)) == FAIL)
					break;
				if (*arg == NUL)
				{
					if (check_fname() == FAIL)	/* check for no file name */
						break;
					i = readfile(curbuf->b_filename, curbuf->b_sfilename,
									line2, FALSE, (linenr_t)0, MAXLNUM, FALSE);
				}
				else
				{
					i = readfile(arg, NULL,
									line2, FALSE, (linenr_t)0, MAXLNUM, FALSE);
				}
				if (i == FAIL)
				{
					emsg2(e_notopen, arg);
					break;
				}
				
				updateScreen(NOT_VALID);
				break;

		case CMD_cd:
		case CMD_chdir:
#ifdef UNIX
				/*
				 * for UNIX ":cd" means: go to home directory
				 */
				if (*arg == NUL)	 /* use NameBuff for home directory name */
				{
					expand_env((char_u *)"$HOME", NameBuff, MAXPATHL);
					arg = NameBuff;
				}
#endif
				if (*arg != NUL)
				{
					if (!did_cd)
					{
						BUF		*buf;

							/* use full path from now on for names of files
							 * being edited and swap files */
						for (buf = firstbuf; buf != NULL; buf = buf->b_next)
						{
							buf->b_xfilename = buf->b_filename;
							mf_fullname(buf->b_ml.ml_mfp);
						}
						status_redraw_all();
					}
					did_cd = TRUE;
					if (vim_chdir((char *)arg))
						emsg(e_failed);
					break;
				}
				/*FALLTHROUGH*/

		case CMD_pwd:
				if (mch_dirname(NameBuff, MAXPATHL) == OK)
					msg(NameBuff);
				else
					emsg(e_unknown);
				break;

		case CMD_equal:
				smsg((char_u *)"line %ld", (long)line2);
				break;

		case CMD_list:
				i = curwin->w_p_list;
				curwin->w_p_list = 1;
		case CMD_number:				/* :nu */
		case CMD_pound:					/* :# */
		case CMD_print:					/* :p */
				for ( ;!got_int; mch_breakcheck())
				{
					print_line(line1,
							   (cmdidx == CMD_number || cmdidx == CMD_pound));
					if (++line1 > line2)
						break;
					flushbuf();			/* show one line at a time */
				}
				setpcmark();
				curwin->w_cursor.lnum = line2;	/* put cursor at last line */

				if (cmdidx == CMD_list)
					curwin->w_p_list = i;

				break;

		case CMD_shell:
				do_shell(NULL);
				break;

		case CMD_sleep:
				n = curwin->w_winpos + curwin->w_row - msg_scrolled;
				if (n >= 0)
				{
					windgoto((int)n, curwin->w_col);
					flushbuf();
				}
				mch_delay(line2 * 1000L, TRUE);
				break;

		case CMD_stag:
				postponed_split = TRUE;
				/*FALLTHROUGH*/
		case CMD_tag:
				do_tag(arg, 0, addr_count ? (int)line2 : 1, forceit);
				break;

		case CMD_pop:
				do_tag((char_u *)"", 1, addr_count ? (int)line2 : 1, forceit);
				break;

		case CMD_tags:
				do_tags();
				break;

		case CMD_marks:
				do_marks(arg);
				break;

		case CMD_jumps:
				do_jumps();
				break;

		case CMD_ascii:
				do_ascii();
				break;

		case CMD_checkpath:
				find_pattern_in_path(NULL, 0, FALSE, FALSE, CHECK_PATH, 1L,
									  forceit ? ACTION_SHOW_ALL : ACTION_SHOW,
											(linenr_t)1, (linenr_t)MAXLNUM);
				break;

		case CMD_digraphs:
#ifdef DIGRAPHS
				if (*arg)
					putdigraph(arg);
				else
					listdigraphs();
#else
				EMSG("No digraphs in this version");
#endif /* DIGRAPHS */
				break;

		case CMD_set:
				(void)do_set(arg);
				break;

		case CMD_fixdel:
				do_fixdel();
				break;

#ifdef AUTOCMD
		case CMD_autocmd:
				/*
				 * Disallow auto commands from .exrc and .vimrc in current
				 * directory for security reasons.
				 */
				if (secure)
				{
					secure = 2;
					errormsg = e_curdir;
				}
				else
					do_autocmd(arg, forceit);	/* handle the auto commands */
				break;

		case CMD_doautocmd:
				do_doautocmd(arg);		/* apply the automatic commands */
				do_modelines();
				break;
#endif

		case CMD_abbreviate:
		case CMD_cabbrev:
		case CMD_iabbrev:
		case CMD_cnoreabbrev:
		case CMD_inoreabbrev:
		case CMD_noreabbrev:
		case CMD_unabbreviate:
		case CMD_cunabbrev:
		case CMD_iunabbrev:
				i = ABBREV;
				goto doabbr;		/* almost the same as mapping */

		case CMD_nmap:
		case CMD_vmap:
		case CMD_cmap:
		case CMD_imap:
		case CMD_map:
		case CMD_nnoremap:
		case CMD_vnoremap:
		case CMD_cnoremap:
		case CMD_inoremap:
		case CMD_noremap:
				/*
				 * If we are sourcing .exrc or .vimrc in current directory we
				 * print the mappings for security reasons.
				 */
				if (secure)
				{
					secure = 2;
					msg_outtrans(cmd);
					msg_outchar('\n');
				}
		case CMD_nunmap:
		case CMD_vunmap:
		case CMD_cunmap:
		case CMD_iunmap:
		case CMD_unmap:
				i = 0;
doabbr:
				if (*cmd == 'c')			/* cmap, cunmap, cnoremap, etc. */
				{
					i += CMDLINE;
					++cmd;
				}
				else if (*cmd == 'i')		/* imap, iunmap, inoremap, etc. */
				{
					i += INSERT;
					++cmd;
				}
											/* nmap, nunmap, nnoremap */
				else if (*cmd == 'n' && *(cmd + 1) != 'o')
				{
					i += NORMAL;
					++cmd;
				}
				else if (*cmd == 'v')		/* vmap, vunmap, vnoremap */
				{
					i += VISUAL;
					++cmd;
				}
				else if (forceit || i)		/* map!, unmap!, noremap!, abbrev */
					i += INSERT + CMDLINE;
				else						/* map, unmap, noremap */
					i += NORMAL + VISUAL;
				switch (do_map((*cmd == 'n') ? 2 : (*cmd == 'u'), arg, i))
				{
					case 1: emsg(e_invarg);
							break;
					case 2: emsg(e_nomap);
							break;
					case 3: emsg(e_ambmap);
							break;
				}
				break;

		case CMD_mapclear:
		case CMD_imapclear:
		case CMD_nmapclear:
		case CMD_vmapclear:
		case CMD_cmapclear:
				map_clear(*cmd, forceit, FALSE);
				break;

		case CMD_abclear:
		case CMD_iabclear:
		case CMD_cabclear:
				map_clear(*cmd, FALSE, TRUE);
				break;

#ifdef USE_GUI
		case CMD_menu:		case CMD_noremenu:		case CMD_unmenu:
		case CMD_nmenu:		case CMD_nnoremenu:		case CMD_nunmenu:
		case CMD_vmenu:		case CMD_vnoremenu:		case CMD_vunmenu:
		case CMD_imenu:		case CMD_inoremenu:		case CMD_iunmenu:
		case CMD_cmenu:		case CMD_cnoremenu:		case CMD_cunmenu:
				gui_do_menu(cmd, arg, forceit);
				break;
#endif /* USE_GUI */

		case CMD_display:
		case CMD_registers:
				do_dis(arg);		/* display buffer contents */
				break;

		case CMD_help:
				do_help(arg);
				break;

		case CMD_version:
				do_version(arg);
				break;

		case CMD_winsize:					/* obsolete command */
				line1 = getdigits(&arg);
				arg = skipwhite(arg);
				line2 = getdigits(&arg);
				set_winsize((int)line1, (int)line2, TRUE);
				break;

		case CMD_delete:
		case CMD_yank:
		case CMD_rshift:
		case CMD_lshift:
				yankbuffer = regname;
				curbuf->b_op_start.lnum = line1;
				curbuf->b_op_end.lnum = line2;
				op_line_count = line2 - line1 + 1;
				op_motion_type = MLINE;
				if (cmdidx != CMD_yank)		/* set cursor position for undo */
				{
					setpcmark();
					curwin->w_cursor.lnum = line1;
					beginline(MAYBE);
				}
				switch (cmdidx)
				{
				case CMD_delete:
					do_delete();
					break;
				case CMD_yank:
					(void)do_yank(FALSE, TRUE);
					break;
#ifdef RIGHTLEFT
				case CMD_rshift:
					do_shift(curwin->w_p_rl ? LSHIFT : RSHIFT, FALSE, amount);
					break;
				case CMD_lshift:
					do_shift(curwin->w_p_rl ? RSHIFT : LSHIFT, FALSE, amount);
					break;
#else
				case CMD_rshift:
					do_shift(RSHIFT, FALSE, amount);
					break;
				case CMD_lshift:
					do_shift(LSHIFT, FALSE, amount);
					break;
#endif
				}
				break;

		case CMD_put:
				yankbuffer = regname;
				/*
				 * ":0put" works like ":1put!".
				 */
				if (line2 == 0)
				{
					line2 = 1;
					forceit = TRUE;
				}
				curwin->w_cursor.lnum = line2;
				do_put(forceit ? BACKWARD : FORWARD, -1L, FALSE);
				break;

		case CMD_t:
		case CMD_copy:
		case CMD_move:
				n = get_address(&arg);
				if (arg == NULL)			/* error detected */
				{
					nextcomm = NULL;
					break;
				}
				/*
				 * move or copy lines from 'line1'-'line2' to below line 'n'
				 */
				if (n == MAXLNUM || n < 0 || n > curbuf->b_ml.ml_line_count)
				{
					emsg(e_invaddr);
					break;
				}

				if (cmdidx == CMD_move)
				{
					if (do_move(line1, line2, n) == FAIL)
						break;
				}
				else
					do_copy(line1, line2, n);
				u_clearline();
				beginline(MAYBE);
				updateScreen(NOT_VALID);
				break;

		case CMD_and:			/* :& */
		case CMD_tilde:			/* :~ */
		case CMD_substitute:	/* :s */
				do_sub(line1, line2, arg, &nextcomm,
							cmdidx == CMD_substitute ? 0 :
							cmdidx == CMD_and ? 1 : 2);
				break;

		case CMD_join:
				curwin->w_cursor.lnum = line1;
				if (line1 == line2)
				{
					if (addr_count >= 2)	/* :2,2join does nothing */
						break;
					if (line2 == curbuf->b_ml.ml_line_count)
					{
						beep_flush();
						break;
					}
					++line2;
				}
				do_do_join(line2 - line1 + 1, !forceit, FALSE);
				beginline(TRUE);
				break;

		case CMD_global:
				if (forceit)
					*cmd = 'v';
		case CMD_vglobal:
				do_glob(*cmd, line1, line2, arg);
				break;

		case CMD_at:				/* :[addr]@r */
				curwin->w_cursor.lnum = line2;
									/* put the register in mapbuf */
				if (do_execbuf(*arg, TRUE,
							  vim_strchr(p_cpo, CPO_EXECBUF) != NULL) == FAIL)
					beep_flush();
				else
									/* execute from the mapbuf */
					while (vpeekc() == ':')
					{
						(void)vgetc();
						(void)do_cmdline((char_u *)NULL, TRUE, TRUE);
					}
				break;

		case CMD_bang:
				do_bang(addr_count, line1, line2, forceit, arg, TRUE, TRUE);
				break;

		case CMD_undo:
				u_undo(1);
				break;

		case CMD_redo:
				u_redo(1);
				break;

		case CMD_source:
				if (forceit)					/* :so! read vi commands */
					(void)openscript(arg);
												/* :so read ex commands */
				else if (do_source(arg, FALSE) == FAIL)
					emsg2(e_notopen, arg);
				break;

#ifdef VIMINFO
		case CMD_rviminfo:
				p = p_viminfo;
				if (*p_viminfo == NUL)
					p_viminfo = (char_u *)"'100";
				if (read_viminfo(arg, TRUE, TRUE, forceit) == FAIL)
					EMSG("Cannot open viminfo file for reading");
				p_viminfo = p;
				break;

		case CMD_wviminfo:
				p = p_viminfo;
				if (*p_viminfo == NUL)
					p_viminfo = (char_u *)"'100";
				write_viminfo(arg, forceit);
				p_viminfo = p;
				break;
#endif /* VIMINFO */

		case CMD_mkvimrc:
				if (*arg == NUL)
					arg = (char_u *)VIMRC_FILE;
				/*FALLTHROUGH*/

		case CMD_mkexrc:
				{
					FILE	*fd;

					if (*arg == NUL)
						arg = (char_u *)EXRC_FILE;
#ifdef UNIX
					/* with Unix it is possible to open a directory */
					if (mch_isdir(arg))
					{
						EMSG2("\"%s\" is a directory", arg);
						break;
					}
#endif
					if (!forceit && vim_fexists(arg))
					{
						EMSG2("\"%s\" exists (use ! to override)", arg);
						break;
					}

					if ((fd = fopen((char *)arg, WRITEBIN)) == NULL)
					{
						EMSG2("Cannot open \"%s\" for writing", arg);
						break;
					}

					/* Write the version command for :mkvimrc */
					if (cmdidx == CMD_mkvimrc)
					{
#ifdef USE_CRNL
						fprintf(fd, "version 4.0\r\n");
#else
						fprintf(fd, "version 4.0\n");
#endif
					}

					if (makemap(fd) == FAIL || makeset(fd) == FAIL ||
																   fclose(fd))
						emsg(e_write);
					break;
				}

		case CMD_cc:
					qf_jump(0, addr_count ? (int)line2 : 0, forceit);
					break;

		case CMD_cfile:
					if (*arg != NUL)
					{
						/*
						 * Great trick: Insert 'ef=' before arg.
						 * Always ok, because "cf " must be there.
						 */
						arg -= 3;
						arg[0] = 'e';
						arg[1] = 'f';
						arg[2] = '=';
						(void)do_set(arg);
					}
					if (qf_init() == OK)
						qf_jump(0, 0, forceit);		/* display first error */
					break;

		case CMD_clist:
					qf_list(forceit);
					break;

		case CMD_cnext:
					qf_jump(FORWARD, addr_count ? (int)line2 : 1, forceit);
					break;

		case CMD_cNext:
		case CMD_cprevious:
					qf_jump(BACKWARD, addr_count ? (int)line2 : 1, forceit);
					break;

		case CMD_cquit:
					getout(1);		/* this does not always pass on the exit
									   code to the Manx compiler. why? */

		case CMD_mark:
		case CMD_k:
					pos = curwin->w_cursor;			/* save curwin->w_cursor */
					curwin->w_cursor.lnum = line2;
					beginline(MAYBE);
					(void)setmark(*arg);			/* set mark */
					curwin->w_cursor = pos;			/* restore curwin->w_cursor */
					break;

		case CMD_center:
		case CMD_right:
		case CMD_left:
					do_align(line1, line2, atoi((char *)arg),
							cmdidx == CMD_center ? 0 : cmdidx == CMD_right ? 1 : -1);
					break;

		case CMD_retab:
				n = getdigits(&arg);
				do_retab(line1, line2, (int)n, forceit);
				u_clearline();
				updateScreen(NOT_VALID);
				break;

		case CMD_make:
				do_make(arg);
				break;

				/*
				 * :normal[!] {commands} - execute normal mode commands
				 * Mostly used for ":autocmd".
				 */
		case CMD_normal:
				/*
				 * Stuff the argument into the typeahead buffer.
				 * Execute normal() until there is no more typeahead than
				 * there was before this command.
				 */
				len = typelen;
				ins_typebuf(arg, forceit ? -1 : 0, 0, TRUE);
				while ((!stuff_empty() ||
							 (!typebuf_typed() && typelen > len)) && !got_int)
				{
					adjust_cursor();	/* put cursor on an existing line */
					cursupdate();		/* update cursor position */
					normal();	 /* get and execute a normal mode command */
				}
				break;

		case CMD_isearch:
		case CMD_dsearch:
				i = ACTION_SHOW;
				goto find_pat;

		case CMD_ilist:
		case CMD_dlist:
				i = ACTION_SHOW_ALL;
				goto find_pat;

		case CMD_ijump:
		case CMD_djump:
				i = ACTION_GOTO;
				goto find_pat;

		case CMD_isplit:
		case CMD_dsplit:
				i = ACTION_SPLIT;
find_pat:
				{
					int		whole = TRUE;

					n = 1;
					if (isdigit(*arg))		/* get count */
					{
						n = getdigits(&arg);
						arg = skipwhite(arg);
					}
					if (*arg == '/')	/* Match regexp, not just whole words */
					{
						whole = FALSE;
						++arg;
						for (p = arg; *p && *p != '/'; p++)
							if (*p == '\\' && p[1] != NUL)
								p++;
						if (*p)
						{
							*p++ = NUL;
							p = skipwhite(p);

							/* Check for trailing illegal characters */
							if (*p && vim_strchr((char_u *)"|\"\n", *p) == NULL)
								errormsg = e_trailing;
							else
								nextcomm = p;
						}
					}
					find_pattern_in_path(arg, (int)STRLEN(arg), whole, !forceit,
						*cmd == 'd' ?  FIND_DEFINE : FIND_ANY,
						n, i, line1, line2);
				}
				break;

		default:
					/* Normal illegal commands have already been handled */
				errormsg = (char_u *)"Sorry, this command is not implemented";
	}


doend:
	if (errormsg != NULL)
	{
		emsg(errormsg);
		if (sourcing)
		{
			MSG_OUTSTR(": ");
			msg_outtrans(*cmdlinep);
		}
	}
	if (did_emsg)
		nextcomm = NULL;				/* cancel nextcomm at an error */
	if (nextcomm && *nextcomm == NUL)		/* not really a next command */
		nextcomm = NULL;
	return nextcomm;
}

/*
 * If 'autowrite' option set, try to write the file.
 *
 * return FAIL for failure, OK otherwise
 */
	int
autowrite(buf, forceit)
	BUF		*buf;
	int		forceit;
{
	if (!p_aw || (!forceit && buf->b_p_ro) || buf->b_filename == NULL)
		return FAIL;
	return buf_write_all(buf);
}

/*
 * flush all buffers, except the ones that are readonly
 */
	void
autowrite_all()
{
	BUF		*buf;

	if (!p_aw)
		return;
	for (buf = firstbuf; buf; buf = buf->b_next)
		if (buf->b_changed && !buf->b_p_ro)
		{
			(void)buf_write_all(buf);
#ifdef AUTOCMD
			/* an autocommand may have deleted the buffer */
			if (!buf_valid(buf))
				buf = firstbuf;
#endif
		}
}

/*
 * flush the contents of a buffer, unless it has no file name
 *
 * return FAIL for failure, OK otherwise
 */
	static int
buf_write_all(buf)
	BUF		*buf;
{
	int		retval;
#ifdef AUTOCMD
	BUF		*old_curbuf = curbuf;
#endif

	retval = (buf_write(buf, buf->b_filename, buf->b_sfilename,
										 (linenr_t)1, buf->b_ml.ml_line_count,
												  FALSE, FALSE, TRUE, FALSE));
#ifdef AUTOCMD
	if (curbuf != old_curbuf)
		MSG("Warning: Entered other buffer unexpectedly (check autocommands)");
#endif
	return retval;
}

/*
 * write current buffer to file 'fname'
 * if 'append' is TRUE, append to the file
 *
 * if *fname == NUL write to current file
 * if b_notedited is TRUE, check for overwriting current file
 *
 * return FAIL for failure, OK otherwise
 */
	static int
do_write(fname, line1, line2, append, forceit)
	char_u		*fname;
	linenr_t	line1, line2;
	int			append;
	int			forceit;
{
	int		other;
	char_u	*sfname = NULL;				/* init to shut up gcc */

	if (*fname == NUL)
		other = FALSE;
	else
	{
		sfname = fname;
		fname = fix_fname(fname);
		other = otherfile(fname);
	}

	/*
	 * if we have a new file name put it in the list of alternate file names
	 */
	if (other)
		setaltfname(fname, sfname, (linenr_t)1);

	/*
	 * writing to the current file is not allowed in readonly mode
	 * and need a file name
	 */
	if (!other && (check_readonly(forceit) || check_fname() == FAIL))
		return FAIL;

	if (!other)
	{
		fname = curbuf->b_filename;
		sfname = curbuf->b_sfilename;
		/*
		 * Not writing the whole file is only allowed with '!'.
		 */
		if ((line1 != 1 || line2 != curbuf->b_ml.ml_line_count) &&
												 !forceit && !append && !p_wa)
		{
			EMSG("Use ! to write partial buffer");
			return FAIL;
		}
	}

	/*
	 * write to other file or b_notedited set or not writing the whole file:
	 * overwriting only allowed with '!'
	 */
	if ((other || curbuf->b_notedited) && !forceit &&
									   !append && !p_wa && vim_fexists(fname))
	{								/* don't overwrite existing file */
#ifdef UNIX
			/* with UNIX it is possible to open a directory */
		if (mch_isdir(fname))
			EMSG2("\"%s\" is a directory", fname);
		else
#endif
			emsg(e_exists);
		return FAIL;
	}
	return (buf_write(curbuf, fname, sfname, line1, line2,
												append, forceit, TRUE, FALSE));
}

/*
 * start editing a new file
 *
 *     fnum: file number; if zero use fname/sfname
 *    fname: the file name
 *				- full path if sfname used,
 *				- any file name if sfname is NULL
 *				- empty string to re-edit with the same file name (but may be
 *					in a different directory)
 *				- NULL to start an empty buffer
 *   sfname: the short file name (or NULL)
 *  command: the command to be executed after loading the file
 *  newlnum: put cursor on this line number (if possible)
 *    flags:
 *         ECMD_HIDE: if TRUE don't free the current buffer
 *     ECMD_SET_HELP: set b_help flag of (new) buffer before opening file
 *       ECMD_OLDBUF: use existing buffer if it exists
 *      ECMD_FORCEIT: ! used for Ex command
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_ecmd(fnum, fname, sfname, command, newlnum, flags)
	int			fnum;
	char_u		*fname;
	char_u		*sfname;
	char_u		*command;
	linenr_t	newlnum;
	int			flags;
{
	int			other_file;				/* TRUE if editing another file */
	int			oldbuf;					/* TRUE if using existing buffer */
#ifdef AUTOCMD
	int			auto_buf = FALSE;		/* TRUE if autocommands brought us
										   into the buffer unexpectedly */
#endif
	BUF			*buf;

	if (fnum != 0)
	{
		if (fnum == curbuf->b_fnum)		/* file is already being edited */
			return OK;					/* nothing to do */
		other_file = TRUE;
	}
	else
	{
			/* if no short name given, use fname for short name */
		if (sfname == NULL)
			sfname = fname;
#ifdef USE_FNAME_CASE
# ifdef USE_LONG_FNAME
		if (USE_LONG_FNAME)
# endif
			fname_case(sfname);			/* set correct case for short filename */
#endif

		if (fname == NULL)
			other_file = TRUE;
											/* there is no file name */
		else if (*fname == NUL && curbuf->b_filename == NULL)
			other_file = FALSE;
		else
		{
			if (*fname == NUL)				/* re-edit with same file name */
			{
				fname = curbuf->b_filename;
				sfname = curbuf->b_sfilename;
			}
			fname = fix_fname(fname);		/* may expand to full path name */
			other_file = otherfile(fname);
		}
	}
/*
 * if the file was changed we may not be allowed to abandon it
 * - if we are going to re-edit the same file
 * - or if we are the only window on this file and if ECMD_HIDE is FALSE
 */
	if (((!other_file && !(flags & ECMD_OLDBUF)) ||
			(curbuf->b_nwindows == 1 && !(flags & ECMD_HIDE))) &&
			check_changed(curbuf, FALSE, !other_file, (flags & ECMD_FORCEIT)))
	{
		if (fnum == 0 && other_file && fname != NULL)
			setaltfname(fname, sfname, (linenr_t)1);
		return FAIL;
	}

/*
 * End Visual mode before switching to another buffer, so the text can be
 * copied into the GUI selection buffer.
 */
	if (VIsual_active)
		end_visual_mode();

/*
 * If we are starting to edit another file, open a (new) buffer.
 * Otherwise we re-use the current buffer.
 */
	if (other_file)
	{
		curwin->w_alt_fnum = curbuf->b_fnum;
		buflist_altlnum();

		if (fnum)
			buf = buflist_findnr(fnum);
		else
			buf = buflist_new(fname, sfname, 1L, TRUE);
		if (buf == NULL)
			return FAIL;
		if (buf->b_ml.ml_mfp == NULL)		/* no memfile yet */
		{
			oldbuf = FALSE;
			buf->b_nwindows = 0;
		}
		else								/* existing memfile */
		{
			oldbuf = TRUE;
			buf_check_timestamp(buf);
		}

		/*
		 * Make the (new) buffer the one used by the current window.
		 * If the old buffer becomes unused, free it if ECMD_HIDE is FALSE.
		 * If the current buffer was empty and has no file name, curbuf
		 * is returned by buflist_new().
		 */
		if (buf != curbuf)
		{
#ifdef AUTOCMD
			BUF		*old_curbuf;
			char_u	*new_name = NULL;

			/*
			 * Be careful: The autocommands may delete any buffer and change
			 * the current buffer.
			 * - If the buffer we are going to edit is deleted, give up.
			 * - If we ended up in the new buffer already, need to skip a few
			 *   things, set auto_buf.
			 */
			old_curbuf = curbuf;
			if (buf->b_xfilename != NULL)
				new_name = strsave(buf->b_xfilename);
			apply_autocmds(EVENT_BUFLEAVE, NULL, NULL);
			if (!buf_valid(buf))		/* new buffer has been deleted */
			{
				EMSG2("Autocommands unexpectedly deleted new buffer %s",
						new_name == NULL ? (char_u *)"" : new_name);
				vim_free(new_name);
				return FAIL;
			}
			vim_free(new_name);
			if (buf == curbuf)			/* already in new buffer */
				auto_buf = TRUE;
			else
			{
				if (curbuf == old_curbuf)
#endif
				{
#ifdef VIMINFO
					curbuf->b_last_cursor = curwin->w_cursor;
#endif
					buf_copy_options(curbuf, buf, TRUE, FALSE);
				}
				close_buffer(curwin, curbuf, !(flags & ECMD_HIDE), FALSE);
				curwin->w_buffer = buf;
				curbuf = buf;
				++curbuf->b_nwindows;
#ifdef AUTOCMD
			}
#endif
		}
		else
			++curbuf->b_nwindows;

		curwin->w_pcmark.lnum = 1;
		curwin->w_pcmark.col = 0;
	}
	else
	{
		if (check_fname() == FAIL)
			return FAIL;
		oldbuf = (flags & ECMD_OLDBUF);
	}

/*
 * If we get here we are sure to start editing
 */
		/* don't redraw until the cursor is in the right line */
	++RedrawingDisabled;
	if (flags & ECMD_SET_HELP)
		curbuf->b_help = TRUE;

/*
 * other_file	oldbuf
 *	FALSE		FALSE		re-edit same file, buffer is re-used
 *	FALSE		TRUE		re-edit same file, nothing changes
 *  TRUE		FALSE		start editing new file, new buffer
 *  TRUE		TRUE		start editing in existing buffer (nothing to do)
 */
	if (!other_file && !oldbuf)			/* re-use the buffer */
	{
		if (newlnum == 0)
			newlnum = curwin->w_cursor.lnum;
		buf_freeall(curbuf);			/* free all things for buffer */
		buf_clear(curbuf);
		curbuf->b_op_start.lnum = 0;	/* clear '[ and '] marks */
		curbuf->b_op_end.lnum = 0;
	}

	/*
	 * Reset cursor position, could be used by autocommands.
	 */
	adjust_cursor();

	/*
	 * Check if we are editing the w_arg_idx file in the argument list.
	 */
	check_arg_idx();

#ifdef AUTOCMD
	if (!auto_buf)
#endif
	{
		/*
		 * Careful: open_buffer() and apply_autocmds() may change the current
		 * buffer and window.
		 */
		if (!oldbuf)						/* need to read the file */
			(void)open_buffer();
#ifdef AUTOCMD
		else
			apply_autocmds(EVENT_BUFENTER, NULL, NULL);
		check_arg_idx();
#endif
		win_init(curwin);
		maketitle();
	}

	if (command == NULL)
	{
		if (newlnum)
		{
			curwin->w_cursor.lnum = newlnum;
			check_cursor();
			beginline(MAYBE);
		}
		else
			beginline(TRUE);
	}

	/*
	 * Did not read the file, need to show some info about the file.
	 * Do this after setting the cursor.
	 */
	if (oldbuf
#ifdef AUTOCMD
				&& !auto_buf
#endif
							)
		fileinfo(did_cd, TRUE, FALSE);

	if (command != NULL)
		do_cmdline(command, TRUE, FALSE);
	--RedrawingDisabled;
	if (!skip_redraw)
		updateScreen(CURSUPD);			/* redraw now */

	if (p_im)
		need_start_insertmode = TRUE;
	return OK;
}

/*
 * get + command from ex argument
 */
	static char_u *
getargcmd(argp)
	char_u **argp;
{
	char_u *arg = *argp;
	char_u *command = NULL;

	if (*arg == '+')		/* +[command] */
	{
		++arg;
		if (vim_isspace(*arg))
			command = (char_u *)"$";
		else
		{
			/*
			 * should check for "\ " (but vi has a bug that prevents it to work)
			 */
			command = arg;
			arg = skiptowhite(command);
			if (*arg)
				*arg++ = NUL;	/* terminate command with NUL */
		}
		
		arg = skipwhite(arg);	/* skip over spaces */
		*argp = arg;
	}
	return command;
}

/*
 * Halve the number of backslashes in a file name argument.
 * For MS-DOS we only do this if the character after the backslash
 * is not a normal file character.
 * For Unix, when wildcards are going to be expanded, don't remove
 * backslashes before special characters.
 */
	static void
backslash_halve(p, expand_wildcards)
	char_u	*p;
	int		expand_wildcards;		/* going to expand wildcards later */
{
	for ( ; *p; ++p)
		if (is_backslash(p)
#if defined(UNIX) || defined(OS2)
				&& !(expand_wildcards &&
						vim_strchr((char_u *)" *?[{`$\\", p[1]))
#endif
											   )
			STRCPY(p, p + 1);
}

	static void
do_make(arg)
	char_u *arg;
{
	if (*p_ef == NUL)
	{
		EMSG("errorfile option not set");
		return;
	}

	autowrite_all();
	vim_remove(p_ef);

	sprintf((char *)IObuff, "%s%s%s %s %s", p_shq, arg, p_shq, p_sp, p_ef);
	MSG_OUTSTR(":!");
	msg_outtrans(IObuff);				/* show what we are doing */
	do_shell(IObuff);

#ifdef AMIGA
	flushbuf();
				/* read window status report and redraw before message */
	(void)char_avail();
#endif

	if (qf_init() == OK)
		qf_jump(0, 0, FALSE);			/* display first error */

	vim_remove(p_ef);
}

/* 
 * Redefine the argument list to 'str'.
 *
 * Return FAIL for failure, OK otherwise.
 */
	static int
do_arglist(str)
	char_u *str;
{
	int		new_count = 0;
	char_u	**new_files = NULL;
	int		exp_count;
	char_u	**exp_files;
	char_u	**t;
	char_u	*p;
	int		inquote;
	int		i;

	while (*str)
	{
		/*
		 * create a new entry in new_files[]
		 */
		t = (char_u **)lalloc((long_u)(sizeof(char_u *) * (new_count + 1)), TRUE);
		if (t != NULL)
			for (i = new_count; --i >= 0; )
				t[i] = new_files[i];
		vim_free(new_files);
		if (t == NULL)
			return FAIL;
		new_files = t;
		new_files[new_count++] = str;

		/*
		 * isolate one argument, taking quotes
		 */
		inquote = FALSE;
		for (p = str; *str; ++str)
		{
			/*
			 * for MSDOS et.al. a backslash is part of a file name.
			 * Only skip ", space and tab.
			 */
			if (is_backslash(str))
				*p++ = *++str;
			else
			{
				if (!inquote && vim_isspace(*str))
					break;
				if (*str == '"')
					inquote ^= TRUE;
				else
					*p++ = *str;
			}
		}
		str = skipwhite(str);
		*p = NUL;
	}
	
	i = ExpandWildCards(new_count, new_files, &exp_count,
												&exp_files, FALSE, TRUE);
	vim_free(new_files);
	if (i == FAIL)
		return FAIL;
	if (exp_count == 0)
	{
		emsg(e_nomatch);
		return FAIL;
	}
	if (arg_exp)				/* arg_files[] has been allocated, free it */
		FreeWild(arg_count, arg_files);
	else
		arg_exp = TRUE;
	arg_files = exp_files;
	arg_count = exp_count;
	arg_had_last = FALSE;

	/*
	 * put all file names in the buffer list
	 */
	for (i = 0; i < arg_count; ++i)
		(void)buflist_add(arg_files[i]);

	return OK;
}

/*
 * Return TRUE if "str" starts with a backslash that should be removed.
 * For MS-DOS, WIN32 and OS/2 this is only done when the character after the
 * backslash is not a normal file name character.
 */
	static int
is_backslash(str)
	char_u	*str;
{
#ifdef BACKSLASH_IN_FILENAME
	return (str[0] == '\\' && str[1] != NUL && str[1] != '*' && str[1] != '?'
						&& !(isfilechar(str[1]) && str[1] != '\\'));
#else
	return (str[0] == '\\' && str[1] != NUL);
#endif
}

/*
 * Check if we are editing the w_arg_idx file in the argument list.
 */
	void
check_arg_idx()
{
	int		t;

	if (arg_count > 1 && (curbuf->b_filename == NULL ||
						  curwin->w_arg_idx >= arg_count ||
				(t = fullpathcmp(arg_files[curwin->w_arg_idx],
						   curbuf->b_filename)) == FPC_DIFF || t == FPC_DIFFX))
		curwin->w_arg_idx_invalid = TRUE;
	else
		curwin->w_arg_idx_invalid = FALSE;
}

	void
gotocmdline(clr)
	int				clr;
{
	msg_start();
	if (clr)				/* clear the bottom line(s) */
		msg_clr_eos();		/* will reset clear_cmdline */
	windgoto(cmdline_row, 0);
}

	static int
check_readonly(forceit)
	int		forceit;
{
	if (!forceit && curbuf->b_p_ro)
	{
		emsg(e_readonly);
		return TRUE;
	}
	return FALSE;
}

/*
 * return TRUE if buffer was changed and cannot be abandoned.
 */
	static int
check_changed(buf, checkaw, mult_win, forceit)
	BUF		*buf;
	int		checkaw;		/* do autowrite if buffer was changed */
	int		mult_win;		/* check also when several windows for the buffer */
	int		forceit;
{
	if (	!forceit &&
			buf->b_changed && (mult_win || buf->b_nwindows <= 1) &&
			(!checkaw || autowrite(buf, forceit) == FAIL))
	{
		emsg(e_nowrtmsg);
		return TRUE;
	}
	return FALSE;
}

/*
 * return TRUE if any buffer was changed and cannot be abandoned.
 * That changed buffer becomes the current buffer.
 */
	static int
check_changed_any()
{
	BUF		*buf;
	int		save;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
	{
		if (buf->b_changed)
		{
			/* There must be a wait_return for this message, do_buffer
			 * will cause a redraw */
			exiting = FALSE;
			if (EMSG2("No write since last change for buffer \"%s\"",
						  buf->b_xfilename == NULL ? (char_u *)"No File" :
														buf->b_xfilename))
			{
				save = no_wait_return;
				no_wait_return = FALSE;
				wait_return(FALSE);
				no_wait_return = save;
			}
			(void)do_buffer(DOBUF_GOTO, DOBUF_FIRST, FORWARD, buf->b_fnum, 0);
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * return FAIL if there is no filename, OK if there is one
 * give error message for FAIL
 */
	int
check_fname()
{
	if (curbuf->b_filename == NULL)
	{
		emsg(e_noname);
		return FAIL;
	}
	return OK;
}

/*
 * - if there are more files to edit
 * - and this is the last window
 * - and forceit not used
 * - and not repeated twice on a row
 *	  return FAIL and give error message if 'message' TRUE
 * return OK otherwise
 */
	static int
check_more(message, forceit)
	int message;			/* when FALSE check only, no messages */
	int forceit;
{
	if (!forceit && only_one_window() && arg_count > 1 && !arg_had_last &&
									quitmore == 0)
	{
		if (message)
		{
			EMSGN("%ld more files to edit", arg_count - curwin->w_arg_idx - 1);
			quitmore = 2;			/* next try to quit is allowed */
		}
		return FAIL;
	}
	return OK;
}

/*
 * try to abandon current file and edit a new or existing file
 * 'fnum' is the number of the file, if zero use fname/sfname
 *
 * return 1 for "normal" error, 2 for "not written" error, 0 for success
 * -1 for succesfully opening another file
 * 'lnum' is the line number for the cursor in the new file (if non-zero).
 */
	int
getfile(fnum, fname, sfname, setpm, lnum, forceit)
	int			fnum;
	char_u		*fname;
	char_u		*sfname;
	int			setpm;
	linenr_t	lnum;
	int			forceit;
{
	int other;

	if (fnum == 0)
	{
		fname_expand(&fname, &sfname);	/* make fname full path, set sfname */
		other = otherfile(fname);
	}
	else
		other = (fnum != curbuf->b_fnum);

	if (other)
		++no_wait_return;			/* don't wait for autowrite message */
	if (other && !forceit && curbuf->b_nwindows == 1 &&
			!p_hid && curbuf->b_changed && autowrite(curbuf, forceit) == FAIL)
	{
		if (other)
			--no_wait_return;
		emsg(e_nowrtmsg);
		return 2;		/* file has been changed */
	}
	if (other)
		--no_wait_return;
	if (setpm)
		setpcmark();
	if (!other)
	{
		if (lnum != 0)
			curwin->w_cursor.lnum = lnum;
		check_cursor();
		beginline(MAYBE);

		return 0;		/* it's in the same file */
	}
	if (do_ecmd(fnum, fname, sfname, NULL, lnum,
				(p_hid ? ECMD_HIDE : 0) + (forceit ? ECMD_FORCEIT : 0)) == OK)
		return -1;		/* opened another file */
	return 1;			/* error encountered */
}

/*
 * vim_strncpy()
 *
 * This is here because strncpy() does not guarantee successful results when
 * the to and from strings overlap.  It is only currently called from nextwild()
 * which copies part of the command line to another part of the command line.
 * This produced garbage when expanding files etc in the middle of the command
 * line (on my terminal, anyway) -- webb.
 */
	static void
vim_strncpy(to, from, len)
	char_u *to;
	char_u *from;
	int len;
{
	int i;

	if (to <= from)
	{
		while (len-- && *from)
			*to++ = *from++;
		if (len >= 0)
			*to = *from;	/* Copy NUL */
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			to++;
			if (*from++ == NUL)
			{
				i++;
				break;
			}
		}
		for (; i > 0; i--)
			*--to = *--from;
	}
}

/*
 * Return FALSE if this is not an appropriate context in which to do
 * completion of anything, & TRUE if it is (even if there are no matches).
 * For the caller, this means that the character is just passed through like a
 * normal character (instead of being expanded).  This allows :s/^I^D etc.
 */
	static int
nextwild(type)
	int		type;
{
	int		i;
	char_u	*p1;
	char_u	*p2;
	int		oldlen;
	int		difflen;
	int		v;

	if (cmd_numfiles == -1)
		set_expand_context(cmdfirstc, cmdbuff);
	if (expand_context == EXPAND_UNSUCCESSFUL)
	{
		beep_flush();
		return OK;	/* Something illegal on command line */
	}
	if (expand_context == EXPAND_NOTHING)
	{
		/* Caller can use the character as a normal char instead */
		return FAIL;
	}
	expand_interactively = TRUE;

	MSG_OUTSTR("...");		/* show that we are busy */
	flushbuf();

	i = expand_pattern - cmdbuff;
	oldlen = cmdpos - i;

	if (type == WILD_NEXT || type == WILD_PREV)
	{
		/*
		 * Get next/previous match for a previous expanded pattern.
		 */
		p2 = ExpandOne(NULL, NULL, 0, type);
	}
	else
	{
		/*
		 * Translate string into pattern and expand it.
		 */
		if ((p1 = addstar(&cmdbuff[i], oldlen)) == NULL)
			p2 = NULL;
		else
		{
			p2 = ExpandOne(p1, strnsave(&cmdbuff[i], oldlen),
													 WILD_HOME_REPLACE, type);
			vim_free(p1);
		}
	}

	if (p2 != NULL)
	{
		if (cmdlen + (difflen = STRLEN(p2) - oldlen) > cmdbufflen - 4)
			v = realloc_cmdbuff(cmdlen + difflen);
		else
			v = OK;
		if (v == OK)
		{
			vim_strncpy(&cmdbuff[cmdpos + difflen], &cmdbuff[cmdpos],
					cmdlen - cmdpos);
			STRNCPY(&cmdbuff[i], p2, STRLEN(p2));
			cmdlen += difflen;
			cmdpos += difflen;
		}
		vim_free(p2);
	}

	redrawcmd();
	if (cmd_numfiles <= 0 && p2 == NULL)
		beep_flush();
	else if (cmd_numfiles == 1)
		(void)ExpandOne(NULL, NULL, 0, WILD_FREE);	/* free expanded pattern */

	expand_interactively = FALSE;			/* reset for next call */
	return OK;
}

#define MAXSUFLEN 30		/* maximum length of a file suffix */

/*
 * Do wildcard expansion on the string 'str'.
 * Return a pointer to alloced memory containing the new string.
 * Return NULL for failure.
 *
 * mode = WILD_FREE:		just free previously expanded matches
 * mode = WILD_EXPAND_FREE:	normal expansion, do not keep matches
 * mode = WILD_EXPAND_KEEP:	normal expansion, keep matches
 * mode = WILD_NEXT:		use next match in multiple match, wrap to first
 * mode = WILD_PREV:		use previous match in multiple match, wrap to first
 * mode = WILD_ALL:			return all matches concatenated
 * mode = WILD_LONGEST:		return longest matched part
 *
 * options = WILD_LIST_NOTFOUND:	list entries without a match
 * options = WILD_HOME_REPLACE:		do home_replace() for buffer names
 */
	char_u *
ExpandOne(str, orig, options, mode)
	char_u	*str;
	char_u	*orig;			/* original string which is expanded */
	int		options;
	int		mode;
{
	char_u		*ss = NULL;
	static char_u **cmd_files = NULL;	/* list of input files */
	static int	findex;
	static char_u *orig_save = NULL;	/* kept value of orig */
	int			i, j;
	int			non_suf_match;			/* number without matching suffix */
	long_u		len;
	char_u		*setsuf;
	int			fnamelen, setsuflen;
	char_u		suf_buf[MAXSUFLEN];
	char_u		*p;

/*
 * first handle the case of using an old match
 */
	if (mode == WILD_NEXT || mode == WILD_PREV)
	{
		if (cmd_numfiles > 0)
		{
			if (mode == WILD_PREV)
			{
				if (findex == -1)
					findex = cmd_numfiles;
				--findex;
			}
			else	/* mode == WILD_NEXT */
				++findex;

			/*
			 * When wrapping around, return the original string, set findex to
			 * -1.
			 */
			if (findex < 0)
			{
				if (orig_save == NULL)
					findex = cmd_numfiles - 1;
				else
					findex = -1;
			}
			if (findex >= cmd_numfiles)
			{
				if (orig_save == NULL)
					findex = 0;
				else
					findex = -1;
			}
			if (findex == -1)
				return strsave(orig_save);
			return strsave(cmd_files[findex]);
		}
		else
			return NULL;
	}

/* free old names */
	if (cmd_numfiles != -1 && mode != WILD_ALL && mode != WILD_LONGEST)
	{
		FreeWild(cmd_numfiles, cmd_files);
		cmd_numfiles = -1;
		vim_free(orig_save);
		orig_save = NULL;
	}
	findex = 0;

	if (mode == WILD_FREE)		/* only release file name */
		return NULL;

	if (cmd_numfiles == -1)
	{
		vim_free(orig_save);
		orig_save = orig;
		if (ExpandFromContext(str, &cmd_numfiles, &cmd_files, FALSE,
															 options) == FAIL)
			/* error: do nothing */;
		else if (cmd_numfiles == 0)
		{
			if (!expand_interactively)
				emsg(e_nomatch);
		}
		else
		{
			/*
			 * May change home directory back to "~"
			 */
			if (options & WILD_HOME_REPLACE)
				tilde_replace(str, cmd_numfiles, cmd_files);

			/*
			 * Insert backslashes into a file name before a space, \, %, # and
			 * wildmatch characters, except '~'.
			 */
			if (expand_interactively &&
					(expand_context == EXPAND_FILES ||
					 expand_context == EXPAND_BUFFERS ||
					 expand_context == EXPAND_DIRECTORIES))
			{
				for (i = 0; i < cmd_numfiles; ++i)
				{
					p = strsave_escaped(cmd_files[i],
#ifdef BACKSLASH_IN_FILENAME
													(char_u *)" *?[{`$%#");
#else
													(char_u *)" *?[{`$\\%#");
#endif
					if (p != NULL)
					{
						vim_free(cmd_files[i]);
						cmd_files[i] = p;
					}
				}
			}

			if (mode != WILD_ALL && mode != WILD_LONGEST)
			{
				non_suf_match = 1;
				if (cmd_numfiles > 1)	/* more than one match; check suffix */
				{
					non_suf_match = 0;
					for (i = 0; i < cmd_numfiles; ++i)
					{
						fnamelen = STRLEN(cmd_files[i]);
						setsuflen = 0;
						for (setsuf = p_su; *setsuf; )
						{
							setsuflen = copy_option_part(&setsuf, suf_buf,
															  MAXSUFLEN, ".,");
							if (fnamelen >= setsuflen && STRNCMP(suf_buf,
										  cmd_files[i] + fnamelen - setsuflen,
													  (size_t)setsuflen) == 0)
								break;
							setsuflen = 0;
						}
						if (setsuflen)		/* suffix matched: ignore file */
							continue;
						/*
						 * Move the name without matching suffix to the front
						 * of the list.  This makes CTRL-N work nice.
						 */
						p = cmd_files[i];
						for (j = i; j > non_suf_match; --j)
							cmd_files[j] = cmd_files[j - 1];
						cmd_files[non_suf_match++] = p;
					}
				}
				if (non_suf_match != 1)
				{
					/* Can we ever get here unless it's while expanding
					 * interactively?  If not, we can get rid of this all
					 * together. Don't really want to wait for this message
					 * (and possibly have to hit return to continue!).
					 */
					if (!expand_interactively)
						emsg(e_toomany);
					else
						beep_flush();
				}
				if (!(non_suf_match != 1 && mode == WILD_EXPAND_FREE))
					ss = strsave(cmd_files[0]);
			}
		}
	}

	/* Find longest common part */
	if (mode == WILD_LONGEST && cmd_numfiles > 0)
	{
		for (len = 0; cmd_files[0][len]; ++len)
		{
			for (i = 0; i < cmd_numfiles; ++i)
			{
#ifdef CASE_INSENSITIVE_FILENAME
				if ((expand_context == EXPAND_DIRECTORIES ||
											 expand_context == EXPAND_FILES ||
										  expand_context == EXPAND_BUFFERS) &&
					 toupper(cmd_files[i][len]) != toupper(cmd_files[0][len]))
					break;
				else
#endif
				if (cmd_files[i][len] != cmd_files[0][len])
					break;
			}
			if (i < cmd_numfiles)
			{
				vim_beep();
				break;
			}
		}
		ss = alloc((unsigned)len + 1);
		if (ss)
		{
			STRNCPY(ss, cmd_files[0], len);
			ss[len] = NUL;
		}
		findex = -1;						/* next p_wc gets first one */
	}

	/* Concatenate all matching names */
	if (mode == WILD_ALL && cmd_numfiles > 0)
	{
		len = 0;
		for (i = 0; i < cmd_numfiles; ++i)
			len += STRLEN(cmd_files[i]) + 1;
		ss = lalloc(len, TRUE);
		if (ss)
		{
			*ss = NUL;
			for (i = 0; i < cmd_numfiles; ++i)
			{
				STRCAT(ss, cmd_files[i]);
				if (i != cmd_numfiles - 1)
					STRCAT(ss, " ");
			}
		}
	}

	if (mode == WILD_EXPAND_FREE || mode == WILD_ALL)
	{
		FreeWild(cmd_numfiles, cmd_files);
		cmd_numfiles = -1;
	}
	return ss;
}

/*
 * For each file name in files[num_files]:
 * If 'orig_pat' starts with "~/", replace the home directory with "~".
 */
	void
tilde_replace(orig_pat, num_files, files)
	char_u	*orig_pat;
	int		num_files;
	char_u	**files;
{
	int		i;
	char_u	*p;

	if (orig_pat[0] == '~' && ispathsep(orig_pat[1]))
	{
		for (i = 0; i < num_files; ++i)
		{
			p = home_replace_save(NULL, files[i]);
			if (p != NULL)
			{
				vim_free(files[i]);
				files[i] = p;
			}
		}
	}
}

/*
 * show all matches for completion on the command line
 */
	static int
showmatches(buff)
	char_u *buff;
{
	char_u		*file_str;
	int			num_files;
	char_u		**files_found;
	int			i, j, k;
	int			maxlen;
	int			lines;
	int			columns;
	char_u		*p;
	int			lastlen;

	set_expand_context(cmdfirstc, cmdbuff);
	if (expand_context == EXPAND_UNSUCCESSFUL)
	{
		beep_flush();
		return OK;	/* Something illegal on command line */
	}
	if (expand_context == EXPAND_NOTHING)
	{
		/* Caller can use the character as a normal char instead */
		return FAIL;
	}
	expand_interactively = TRUE;

	/* add star to file name, or convert to regexp if not expanding files! */
	file_str = addstar(expand_pattern, (int)(buff + cmdpos - expand_pattern));
	if (file_str == NULL)
	{
		expand_interactively = FALSE;
		return OK;
	}

	msg_didany = FALSE;					/* lines_left will be set */
	msg_start();						/* prepare for paging */
	msg_outchar('\n');
	flushbuf();
	cmdline_row = msg_row;
	msg_didany = FALSE;					/* lines_left will be set again */
	msg_start();						/* prepare for paging */

	/* find all files that match the description */
	if (ExpandFromContext(file_str, &num_files, &files_found, FALSE, 0) == FAIL)
	{
		num_files = 0;
		files_found = (char_u **)"";
	}

	/* find the length of the longest file name */
	maxlen = 0;
	for (i = 0; i < num_files; ++i)
	{
		if (expand_context == EXPAND_FILES || expand_context == EXPAND_BUFFERS)
		{
			home_replace(NULL, files_found[i], NameBuff, MAXPATHL);
			j = strsize(NameBuff);
		}
		else
			j = strsize(files_found[i]);
		if (j > maxlen)
			maxlen = j;
	}

	/* compute the number of columns and lines for the listing */
	maxlen += 2;	/* two spaces between file names */
	columns = ((int)Columns + 2) / maxlen;
	if (columns < 1)
		columns = 1;
	lines = (num_files + columns - 1) / columns;

	(void)set_highlight('d');	/* find out highlight mode for directories */

	/* list the files line by line */
	for (i = 0; i < lines; ++i)
	{
		lastlen = 999;
		for (k = i; k < num_files; k += lines)
		{
			for (j = maxlen - lastlen; --j >= 0; )
				msg_outchar(' ');
			if (expand_context == EXPAND_FILES ||
											 expand_context == EXPAND_BUFFERS)
			{
						/* highlight directories */
				j = (mch_isdir(files_found[k]));
				home_replace(NULL, files_found[k], NameBuff, MAXPATHL);
				p = NameBuff;
			}
			else
			{
				j = FALSE;
				p = files_found[k];
			}
			if (j)
				start_highlight();
			lastlen = msg_outtrans(p);
			if (j)
				stop_highlight();
		}
		msg_outchar('\n');
		flushbuf();					/* show one line at a time */
		if (got_int)
		{
			got_int = FALSE;
			break;
		}
	}
	vim_free(file_str);
	FreeWild(num_files, files_found);

/*
 * we redraw the command below the lines that we have just listed
 * This is a bit tricky, but it saves a lot of screen updating.
 */
	cmdline_row = msg_row;		/* will put it back later */

	expand_interactively = FALSE;
	return OK;
}

/*
 * Prepare a string for expansion.
 * When expanding file names:  The string will be used with ExpandWildCards().
 * Copy the file name into allocated memory and add a '*' at the end.
 * When expanding other names:  The string will be used with regcomp().  Copy
 * the name into allocated memory and add ".*" at the end.
 */
	char_u *
addstar(fname, len)
	char_u	*fname;
	int		len;
{
	char_u	*retval;
	int		i, j;
	int		new_len;
	char_u	*tail;

	if (expand_interactively && expand_context != EXPAND_FILES &&
										 expand_context != EXPAND_DIRECTORIES)
	{
		/*
		 * Matching will be done internally (on something other than files).
		 * So we convert the file-matching-type wildcards into our kind for
		 * use with vim_regcomp().  First work out how long it will be:
		 */

		/* for help tags the translation is done in find_help_tags() */
		if (expand_context == EXPAND_HELP)
			retval = strnsave(fname, len);
		else
		{
			new_len = len + 2;			/* +2 for '^' at start, NUL at end */
			for (i = 0; i < len; i++)
			{
				if (fname[i] == '*' || fname[i] == '~')
					new_len++;			/* '*' needs to be replaced by ".*"
										   '~' needs to be replaced by "\~" */

				/* Buffer names are like file names.  "." should be literal */
				if (expand_context == EXPAND_BUFFERS && fname[i] == '.')
					new_len++;			/* "." becomes "\." */
			}
			retval = alloc(new_len);
			if (retval != NULL)
			{
				retval[0] = '^';
				j = 1;
				for (i = 0; i < len; i++, j++)
				{
					if (fname[i] == '\\' && ++i == len)	/* skip backslash */
						break;

					switch (fname[i])
					{
						case '*':	retval[j++] = '.';
									break;
						case '~':	retval[j++] = '\\';
									break;
						case '?':	retval[j] = '.';
									continue;
						case '.':	if (expand_context == EXPAND_BUFFERS)
										retval[j++] = '\\';
									break;
					}
					retval[j] = fname[i];
				}
				retval[j] = NUL;
			}
		}
	}
	else
	{
		retval = alloc(len + 4);
		if (retval != NULL)
		{
			STRNCPY(retval, fname, len);
			retval[len] = NUL;
			backslash_halve(retval, TRUE);		/* remove some backslashes */
			len = STRLEN(retval);

			/*
			 * Don't add a star to ~, ~user, $var or `cmd`.
			 * ~ would be at the start of the tail.
			 * $ could be anywhere in the tail.
			 * ` could be anywhere in the file name.
			 */
			tail = gettail(retval);
			if (*tail != '~' && vim_strchr(tail, '$') == NULL
										   && vim_strchr(retval, '`') == NULL)
			{
#ifdef MSDOS
				/*
				 * if there is no dot in the file name, add "*.*" instead of
				 * "*".
				 */
				for (i = len - 1; i >= 0; --i)
					if (vim_strchr((char_u *)".\\/:", retval[i]) != NULL)
						break;
				if (i < 0 || retval[i] != '.')
				{
					retval[len++] = '*';
					retval[len++] = '.';
				}
#endif
				retval[len++] = '*';
			}
			retval[len] = NUL;
		}
	}
	return retval;
}

/*
 * do_source: read the file "fname" and execute its lines as EX commands
 *
 * This function may be called recursively!
 *
 * return FAIL if file could not be opened, OK otherwise
 */
	int
do_source(fname, check_other)
	register char_u *fname;
	int				check_other;		/* check for .vimrc and _vimrc */
{
	register FILE	*fp;
	register int	len;
#ifdef USE_CRNL
	int				has_cr;
	int				textmode = -1;	/* -1 = unknown, 0 = NL, 1 = CR-NL */
	int				error = FALSE;
#endif
									/* use NameBuff for expanded name */
	expand_env(fname, NameBuff, MAXPATHL);
	fp = fopen((char *)NameBuff, READBIN);
	if (fp == NULL && check_other)
	{
		/*
		 * Try again, replacing file name ".vimrc" by "_vimrc" or vice versa
		 * (if applicable)
		 */
		len = STRLEN(NameBuff);
		if (((len > 6 && ispathsep(NameBuff[len - 7])) || len == 6) &&
					 (NameBuff[len - 6] == '.' || NameBuff[len - 6] == '_') &&
								  (STRCMP(&NameBuff[len - 5], "vimrc") == 0))
		{
			if (NameBuff[len - 6] == '_')
				NameBuff[len - 6] = '.';
			else
				NameBuff[len - 6] = '_';
			fp = fopen((char *)NameBuff, READBIN);
		}
	}

	if (fp == NULL)
		return FAIL;

#ifdef USE_CRNL
		/* no automatic textmode: Set default to CR-NL */
	if (!p_ta)
		textmode = 1;
#endif
	sourcing_name = fname;
	sourcing_lnum = 1;
#ifdef SLEEP_IN_EMSG
	++dont_sleep;			/* don't call sleep() in emsg() */
#endif
	len = 0;
	while (fgets((char *)IObuff + len, IOSIZE - len, fp) != NULL && !got_int)
	{
		len = STRLEN(IObuff) - 1;
		if (len >= 0 && IObuff[len] == '\n')	/* remove trailing newline */
		{
#ifdef USE_CRNL
			has_cr = (len > 0 && IObuff[len - 1] == '\r');
			if (textmode == -1)
			{
				if (has_cr)
					textmode = 1;
				else
					textmode = 0;
			}

			if (textmode)
			{
				if (has_cr) 		/* remove trailing CR-LF */
					--len;
				else		/* lines like ":map xx yy^M" will have failed */
				{
					if (!error)
						EMSG("Warning: Wrong line separator, ^M may be missing");
					error = TRUE;
					textmode = 0;
				}
			}
#endif
				/* escaped newline, read more */
			if (len > 0 && len < IOSIZE && IObuff[len - 1] == Ctrl('V'))
			{
				IObuff[len - 1] = '\n';		/* remove CTRL-V */
				++sourcing_lnum;
				continue;
			}
			IObuff[len] = NUL;
		}
			/* check for ^C here, so recursive :so will be broken */
		mch_breakcheck();
		do_cmdline(IObuff, TRUE, TRUE);
		len = 0;
		++sourcing_lnum;
	}
	fclose(fp);
	if (got_int)
		emsg(e_interr);
#ifdef SLEEP_IN_EMSG
	--dont_sleep;
#endif
	sourcing_name = NULL;
	sourcing_lnum = 0;
	return OK;
}

/*
 * get a single EX address
 * 
 * Set ptr to the next character after the part that was interpreted.
 * Set ptr to NULL when an error is encountered.
 */
	static linenr_t
get_address(ptr)
	char_u		**ptr;
{
	linenr_t	cursor_lnum = curwin->w_cursor.lnum;
	int			c;
	int			i;
	long		n;
	char_u  	*cmd;
	FPOS		pos;
	FPOS		*fp;
	linenr_t	lnum;

	cmd = skipwhite(*ptr);
	lnum = MAXLNUM;
	do
	{
		switch (*cmd)
		{
			case '.': 						/* '.' - Cursor position */
						++cmd;
						lnum = cursor_lnum;
						break;

			case '$': 						/* '$' - last line */
						++cmd;
						lnum = curbuf->b_ml.ml_line_count;
						break;

			case '\'': 						/* ''' - mark */
						if (*++cmd == NUL || (check_mark(
										fp = getmark(*cmd++, FALSE)) == FAIL))
							goto error;
						lnum = fp->lnum;
						break;

			case '{':
			case '}':
						c = *cmd++;
						pos = curwin->w_cursor;
						curwin->w_cursor.col = -1;
						if(findpar((c=='}')?FORWARD:BACKWARD, 1, NUL, FALSE))
							lnum = curwin->w_cursor.lnum;
						curwin->w_cursor = pos;
						break;

			case '/':
			case '?':						/* '/' or '?' - search */
						c = *cmd++;
						pos = curwin->w_cursor;		/* save curwin->w_cursor */
						if (c == '/')	/* forward search, start on next line */
						{
							++curwin->w_cursor.lnum;
							curwin->w_cursor.col = 0;
						}
						else		   /* backward search, start on prev line */
						{
							--curwin->w_cursor.lnum;
							curwin->w_cursor.col = MAXCOL;
						}
						searchcmdlen = 0;
						if (!do_search(c, cmd, 1L,
									  SEARCH_HIS + SEARCH_MSG + SEARCH_START))
						{
							cmd = NULL;
							curwin->w_cursor = pos;
							goto error;
						}
						lnum = curwin->w_cursor.lnum;
						curwin->w_cursor = pos;
											/* adjust command string pointer */
						cmd += searchcmdlen;
						break;

			case '\\':				/* "\?", "\/" or "\&", repeat search */
						++cmd;
						if (*cmd == '&')
							i = RE_SUBST;
						else if (*cmd == '?' || *cmd == '/')
							i = RE_SEARCH;
						else
						{
							emsg(e_backslash);
							cmd = NULL;
							goto error;
						}

									/* forward search, start on next line */
						if (*cmd != '?')
						{
							pos.lnum = curwin->w_cursor.lnum + 1;
							pos.col = 0;
						}
									/* backward search, start on prev line */
						else		
						{
							pos.lnum = curwin->w_cursor.lnum - 1;
							pos.col = MAXCOL;
						}
						if (searchit(&pos, *cmd == '?' ? BACKWARD : FORWARD,
															 (char_u *)"", 1L,
										  SEARCH_MSG + SEARCH_START, i) == OK)
							lnum = pos.lnum;
						else
						{
							cmd = NULL;
							goto error;
						}
						++cmd;
						break;

			default:
						if (isdigit(*cmd))		/* absolute line number */
							lnum = getdigits(&cmd);
		}
		
		for (;;)
		{
			cmd = skipwhite(cmd);
			if (*cmd != '-' && *cmd != '+' && !isdigit(*cmd))
				break;

			if (lnum == MAXLNUM)
				lnum = cursor_lnum;		/* "+1" is same as ".+1" */
			if (isdigit(*cmd))
				i = '+';				/* "number" is same as "+number" */
			else
				i = *cmd++;
			if (!isdigit(*cmd))			/* '+' is '+1', but '+0' is not '+1' */
				n = 1;
			else 
				n = getdigits(&cmd);
			if (i == '-')
				lnum -= n;
			else
				lnum += n;
		}
		cursor_lnum = lnum;
	} while (*cmd == '/' || *cmd == '?');

error:
	*ptr = cmd;
	return lnum;
}


/*
 * Must parse the command line so far to work out what context we are in.
 * Completion can then be done based on that context.
 * This routine sets two global variables:
 *	char_u *expand_pattern	The start of the pattern to be expanded within
 *								the command line (ends at the cursor).
 *	int expand_context		The type of thing to expand.  Will be one of:
 *
 *	EXPAND_UNSUCCESSFUL		Used sometimes when there is something illegal on
 *							the command line, like an unknown command.  Caller
 *							should beep.
 *	EXPAND_NOTHING			Unrecognised context for completion, use char like
 *							a normal char, rather than for completion.  eg
 *							:s/^I/
 *	EXPAND_COMMANDS			Cursor is still touching the command, so complete
 *							it.
 *	EXPAND_BUFFERS			Complete file names for :buf and :sbuf commands.
 *	EXPAND_FILES			After command with XFILE set, or after setting
 *	  						with P_EXPAND set.  eg :e ^I, :w>>^I
 *	EXPAND_DIRECTORIES		In some cases this is used instead of the latter
 *	  						when we know only directories are of interest.  eg
 *	  						:set dir=^I
 *	EXPAND_SETTINGS			Complete variable names.  eg :set d^I
 *	EXPAND_BOOL_SETTINGS	Complete boolean variables only,  eg :set no^I
 *	EXPAND_TAGS				Complete tags from the files in p_tags.  eg :ta a^I
 *	EXPAND_HELP				Complete tags from the file 'helpfile'/vim_tags
 *	EXPAND_EVENTS			Complete event names
 *
 * -- webb.
 */
	static void
set_expand_context(firstc, buff)
	int			firstc; 	/* either ':', '/', or '?' */
	char_u		*buff;	 	/* buffer for command string */
{
	char_u		*nextcomm;
	char_u		old_char;

	old_char = cmdbuff[cmdpos];
	cmdbuff[cmdpos] = NUL;
	nextcomm = buff;
	while (nextcomm != NULL)
		nextcomm = set_one_cmd_context(firstc, nextcomm);
	cmdbuff[cmdpos] = old_char;
}

/*
 * This is all pretty much copied from do_one_cmd(), with all the extra stuff
 * we don't need/want deleted.  Maybe this could be done better if we didn't
 * repeat all this stuff.  The only problem is that they may not stay perfectly
 * compatible with each other, but then the command line syntax probably won't
 * change that much -- webb.
 */
	static char_u *
set_one_cmd_context(firstc, buff)
	int			firstc; 	/* either ':', '/', or '?' */
	char_u		*buff;	 	/* buffer for command string */
{
	char_u				*p;
	char_u				*cmd, *arg;
	int 				i;
	int					cmdidx;
	long				argt;
	char_u				delim;
	int					forceit = FALSE;
	int					usefilter = FALSE;	/* filter instead of file name */

	expand_pattern = buff;
	if (firstc != ':')
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}
	expand_context = EXPAND_COMMANDS;	/* Default until we get past command */

/*
 * 2. skip comment lines and leading space, colons or bars
 */
	for (cmd = buff; vim_strchr((char_u *)" \t:|", *cmd) != NULL; cmd++)
		;
	expand_pattern = cmd;

	if (*cmd == NUL)
		return NULL;
	if (*cmd == '"')		/* ignore comment lines */
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 */
	/* 
	 * Backslashed delimiters after / or ? will be skipped, and commands will
	 * not be expanded between /'s and ?'s or after "'". -- webb
	 */
	while (*cmd != NUL && (vim_isspace(*cmd) || isdigit(*cmd) ||
							vim_strchr((char_u *)".$%'/?-+,;{}", *cmd) != NULL))
	{
		if (*cmd == '\'')
		{
			if (*++cmd == NUL)
				expand_context = EXPAND_NOTHING;
		}
		else if (*cmd == '/' || *cmd == '?')
		{
			delim = *cmd++;
			while (*cmd != NUL && *cmd != delim)
				if (*cmd++ == '\\' && *cmd != NUL)
					++cmd;
			if (*cmd == NUL)
				expand_context = EXPAND_NOTHING;
		}
		if (*cmd != NUL)
			++cmd;
	}

/*
 * 4. parse command
 */

	cmd = skipwhite(cmd);
	expand_pattern = cmd;
	if (*cmd == NUL)
		return NULL;
	if (*cmd == '"')
	{
		expand_context = EXPAND_NOTHING;
		return NULL;
	}

	if (*cmd == '|' || *cmd == '\n')
		return cmd + 1;					/* There's another command */

	/*
	 * Isolate the command and search for it in the command table.
	 * Exeptions:
	 * - the 'k' command can directly be followed by any character.
	 * - the 's' command can be followed directly by 'c', 'g' or 'r'
	 */
	if (*cmd == 'k')
	{
		cmdidx = CMD_k;
		p = cmd + 1;
	}
	else
	{
		p = cmd;
		while (isalpha(*p) || *p == '*')	/* Allow * wild card */
			++p;
			/* check for non-alpha command */
		if (p == cmd && vim_strchr((char_u *)"@!=><&~#", *p) != NULL)
			++p;
		i = (int)(p - cmd);

		if (i == 0)
		{
			expand_context = EXPAND_UNSUCCESSFUL;
			return NULL;
		}
		for (cmdidx = 0; cmdidx < CMD_SIZE; ++cmdidx)
			if (STRNCMP(cmdnames[cmdidx].cmd_name, cmd, (size_t)i) == 0)
				break;
	}

	/*
	 * If the cursor is touching the command, and it ends in an alphabetic
	 * character, complete the command name.
	 */
	if (p == cmdbuff + cmdpos && isalpha(p[-1]))
		return NULL;

	if (cmdidx == CMD_SIZE)
	{
		if (*cmd == 's' && vim_strchr((char_u *)"cgr", cmd[1]) != NULL)
		{
			cmdidx = CMD_substitute;
			p = cmd + 1;
		}
		else
		{
			/* Not still touching the command and it was an illegal command */
			expand_context = EXPAND_UNSUCCESSFUL;
			return NULL;
		}
	}

	expand_context = EXPAND_NOTHING; /* Default now that we're past command */

	if (*p == '!')					/* forced commands */
	{
		forceit = TRUE;
		++p;
	}

/*
 * 5. parse arguments
 */
	argt = cmdnames[cmdidx].cmd_argt;

	arg = skipwhite(p);

	if (cmdidx == CMD_write)
	{
		if (*arg == '>')						/* append */
		{
			if (*++arg == '>')				/* It should be */
				++arg;
			arg = skipwhite(arg);
		}
		else if (*arg == '!')					/* :w !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_read)
	{
		usefilter = forceit;					/* :r! filter if forced */
		if (*arg == '!')						/* :r !filter */
		{
			++arg;
			usefilter = TRUE;
		}
	}

	if (cmdidx == CMD_lshift || cmdidx == CMD_rshift)
	{
		while (*arg == *cmd)		/* allow any number of '>' or '<' */
			++arg;
		arg = skipwhite(arg);
	}

	/* Does command allow "+command"? */
	if ((argt & EDITCMD) && !usefilter && *arg == '+')
	{
		/* Check if we're in the +command */
		p = arg + 1;
		arg = skiptowhite(arg);

		/* Still touching the command after '+'? */
		if (arg >= cmdbuff + cmdpos)
			return p;

		/* Skip space after +command to get to the real argument */
		arg = skipwhite(arg);
	}

	/*
	 * Check for '|' to separate commands and '"' to start comments.
	 * Don't do this for ":read !cmd" and ":write !cmd".
	 */
	if ((argt & TRLBAR) && !usefilter)
	{
		p = arg;
		while (*p)
		{
			if (*p == Ctrl('V'))
			{
				if (p[1] != NUL)
					++p;
			}
			else if ((*p == '"' && !(argt & NOTRLCOM)) || *p == '|' || *p == '\n')
			{
				if (*(p - 1) != '\\')
				{
					if (*p == '|' || *p == '\n')
						return p + 1;
					return NULL;	/* It's a comment */
				}
			}
			++p;
		}
	}

												/* no arguments allowed */
	if (!(argt & EXTRA) && *arg != NUL &&
									vim_strchr((char_u *)"|\"", *arg) == NULL)
		return NULL;

	/* Find start of last argument (argument just before cursor): */
	p = cmdbuff + cmdpos;
	while (p != arg && *p != ' ' && *p != TAB)
		p--;
	if (*p == ' ' || *p == TAB)
		p++;
	expand_pattern = p;

	if (argt & XFILE)
	{
		int in_quote = FALSE;
		char_u *bow = NULL;		/* Beginning of word */

		/*
		 * Allow spaces within back-quotes to count as part of the argument
		 * being expanded.
		 * Never accept '<' or '>' inside a file name.
		 */
		expand_pattern = skipwhite(arg);
		for (p = expand_pattern; *p; ++p)
		{
			if (*p == '\\' && p[1])
				++p;
			else if ((vim_iswhite(*p)
#ifdef SPACE_IN_FILENAME
					&& (!(argt & NOSPC) || usefilter)
#endif
					 ) || *p == '<' || *p == '>')
			{
				if (p[1] == '&')		/* skip ">&" */
					++p;
				if (p[1] == '!')		/* skip ">&!" */
					++p;
				if (in_quote)
					bow = p + 1;
				else
					expand_pattern = p + 1;
			}
			else if (*p == '`')
			{
				if (!in_quote)
				{
					expand_pattern = p;
					bow = p + 1;
				}
				in_quote = !in_quote;
			}
		}

		/*
		 * If we are still inside the quotes, and we passed a space, just
		 * expand from there.
		 */
		if (bow != NULL && in_quote)
			expand_pattern = bow;
		expand_context = EXPAND_FILES;
	}

/*
 * 6. switch on command name
 */
	switch (cmdidx)
	{
		case CMD_cd:
		case CMD_chdir:
			expand_context = EXPAND_DIRECTORIES;
			break;
		case CMD_global:
		case CMD_vglobal:
			delim = *arg; 			/* get the delimiter */
			if (delim)
				++arg;				/* skip delimiter if there is one */

			while (arg[0] != NUL && arg[0] != delim)
			{
				if (arg[0] == '\\' && arg[1] != NUL)
					++arg;
				++arg;
			}
			if (arg[0] != NUL)
				return arg + 1;
			break;
		case CMD_and:
		case CMD_substitute:
			delim = *arg;
			if (delim)
				++arg;
			for (i = 0; i < 2; i++)
			{
				while (arg[0] != NUL && arg[0] != delim)
				{
					if (arg[0] == '\\' && arg[1] != NUL)
						++arg;
					++arg;
				}
				if (arg[0] != NUL)		/* skip delimiter */
					++arg;
			}
			while (arg[0] && vim_strchr((char_u *)"|\"#", arg[0]) == NULL)
				++arg;
			if (arg[0] != NUL)
				return arg;
			break;
		case CMD_isearch:
		case CMD_dsearch:
		case CMD_ilist:
		case CMD_dlist:
		case CMD_ijump:
		case CMD_djump:
		case CMD_isplit:
		case CMD_dsplit:
			arg = skipwhite(skipdigits(arg));		/* skip count */
			if (*arg == '/')	/* Match regexp, not just whole words */
			{
				for (++arg; *arg && *arg != '/'; arg++)
					if (*arg == '\\' && arg[1] != NUL)
						arg++;
				if (*arg)
				{
					arg = skipwhite(arg + 1);

					/* Check for trailing illegal characters */
					if (*arg && vim_strchr((char_u *)"|\"\n", *arg) == NULL)
						expand_context = EXPAND_NOTHING;
					else
						return arg;
				}
			}
			break;
#ifdef AUTOCMD
		case CMD_autocmd:
			return set_context_in_autocmd(arg, FALSE);

		case CMD_doautocmd:
			return set_context_in_autocmd(arg, TRUE);
#endif
		case CMD_set:
			set_context_in_set_cmd(arg);
			break;
		case CMD_stag:
		case CMD_tag:
			expand_context = EXPAND_TAGS;
			expand_pattern = arg;
			break;
		case CMD_help:
			expand_context = EXPAND_HELP;
			expand_pattern = arg;
			break;
		case CMD_bdelete:
		case CMD_bunload:
			while ((expand_pattern = vim_strchr(arg, ' ')) != NULL)
				arg = expand_pattern + 1;
		case CMD_buffer:
		case CMD_sbuffer:
			expand_context = EXPAND_BUFFERS;
			expand_pattern = arg;
			break;
#ifdef USE_GUI
		case CMD_menu:		case CMD_noremenu:		case CMD_unmenu:
		case CMD_nmenu:		case CMD_nnoremenu:		case CMD_nunmenu:
		case CMD_vmenu:		case CMD_vnoremenu:		case CMD_vunmenu:
		case CMD_imenu:		case CMD_inoremenu:		case CMD_iunmenu:
		case CMD_cmenu:		case CMD_cnoremenu:		case CMD_cunmenu:
			return gui_set_context_in_menu_cmd(cmd, arg, forceit);
			break;
#endif
		default:
			break;
	}
	return NULL;
}

/*
 * Do the expansion based on the global variables expand_context and
 * expand_pattern -- webb.
 */
	static int
ExpandFromContext(pat, num_file, file, files_only, options)
	char_u	*pat;
	int		*num_file;
	char_u	***file;
	int		files_only;
	int		options;
{
	regexp	*prog;
	int		ret;
	int		i;
	int		count;

	if (!expand_interactively || expand_context == EXPAND_FILES)
		return ExpandWildCards(1, &pat, num_file, file, files_only,
											  (options & WILD_LIST_NOTFOUND));
	else if (expand_context == EXPAND_DIRECTORIES)
	{
		if (ExpandWildCards(1, &pat, num_file, file, files_only,
									  (options & WILD_LIST_NOTFOUND)) == FAIL)
			return FAIL;
		count = 0;
		for (i = 0; i < *num_file; i++)
			if (mch_isdir((*file)[i]))
				(*file)[count++] = (*file)[i];
			else
				vim_free((*file)[i]);
		if (count == 0)
		{
			vim_free(*file);
			*file = (char_u **)"";
			*num_file = -1;
			return FAIL;
		}
		*num_file = count;
		return OK;
	}
	*file = (char_u **)"";
	*num_file = 0;
	if (expand_context == EXPAND_OLD_SETTING)
		return ExpandOldSetting(num_file, file);

	if (expand_context == EXPAND_HELP)
		return find_help_tags(pat, num_file, file);

	set_reg_ic(pat);		/* set reg_ic according to p_ic, p_scs and pat */
#ifdef AUTOCMD
	if (expand_context == EXPAND_EVENTS)
		reg_ic = TRUE;		/* always ignore case for events */
#endif
	reg_magic = p_magic;

	if (expand_context == EXPAND_BUFFERS)
		return ExpandBufnames(pat, num_file, file, options);

	prog = vim_regcomp(pat);
	if (prog == NULL)
		return FAIL;

	if (expand_context == EXPAND_COMMANDS)
		ret = ExpandCommands(prog, num_file, file);
	else if (expand_context == EXPAND_SETTINGS ||
									   expand_context == EXPAND_BOOL_SETTINGS)
		ret = ExpandSettings(prog, num_file, file);
	else if (expand_context == EXPAND_TAGS)
		ret = find_tags(NULL, prog, num_file, file, FALSE, FALSE);
#ifdef AUTOCMD
	else if (expand_context == EXPAND_EVENTS)
		ret = ExpandEvents(prog, num_file, file);
#endif
#ifdef USE_GUI
	else if (expand_context == EXPAND_MENUS)
		ret = gui_ExpandMenuNames(prog, num_file, file);
#endif
	else
		ret = FAIL;

	vim_free(prog);
	return ret;
}

	static int
ExpandCommands(prog, num_file, file)
	regexp		*prog;
	int			*num_file;
	char_u		***file;
{
	int		cmdidx;
	int		count;
	int		round;

	/*
	 * round == 1: Count the matches.
	 * round == 2: Save the matches into the array.
	 */
	for (round = 1; round <= 2; ++round)
	{
		count = 0;
		for (cmdidx = 0; cmdidx < CMD_SIZE; cmdidx++)
			if (vim_regexec(prog, cmdnames[cmdidx].cmd_name, TRUE))
			{
				if (round == 1)
					count++;
				else
					(*file)[count++] = strsave(cmdnames[cmdidx].cmd_name);
			}
		if (round == 1)
		{
			*num_file = count;
			if (count == 0 || (*file = (char_u **)
						 alloc((unsigned)(count * sizeof(char_u *)))) == NULL)
				return FAIL;
		}
	}
	return OK;
}

#ifdef VIMINFO
static char_u **viminfo_history[2] = {NULL, NULL};
static int		viminfo_hisidx[2] = {0, 0};
static int		viminfo_hislen = 0;
static int		viminfo_add_at_front = FALSE;

	void
prepare_viminfo_history(len)
	int len;
{
	int i;
	int num;
	int	type;

	init_history();
	viminfo_add_at_front = (len != 0);
	if (len > hislen)
		len = hislen;

	for (type = 0; type <= 1; ++type)
	{
		/* If there are more spaces available than we request, then fill them
		 * up */
		for (i = 0, num = 0; i < hislen; i++)
			if (history[type][i] == NULL)
				num++;
		if (num > len)
			len = num;
		viminfo_hisidx[type] = 0;
		if (len <= 0)
			viminfo_history[type] = NULL;
		else
			viminfo_history[type] = (char_u **)lalloc(len * sizeof(char_u *),
																	   FALSE);
	}
	viminfo_hislen = len;
	if (viminfo_history[0] == NULL || viminfo_history[1] == NULL)
		viminfo_hislen = 0;
}

	int
read_viminfo_history(line, fp)
	char_u	*line;
	FILE	*fp;
{
	int		type;

	type = (line[0] == ':' ? 0 : 1);
	if (viminfo_hisidx[type] != viminfo_hislen)
	{
		viminfo_readstring(line);
		if (!is_in_history(type, line + 1, viminfo_add_at_front))
			viminfo_history[type][viminfo_hisidx[type]++] = strsave(line + 1);
	}
	return vim_fgets(line, LSIZE, fp);
}

	void
finish_viminfo_history()
{
	int idx;
	int i;
	int	type;

	for (type = 0; type <= 1; ++type)
	{
		if (history[type] == NULL)
			return;
		idx = hisidx[type] + viminfo_hisidx[type];
		if (idx >= hislen)
			idx -= hislen;
		if (viminfo_add_at_front)
			hisidx[type] = idx;
		else
		{
			if (hisidx[type] == -1)
				hisidx[type] = hislen - 1;
			do
			{
				if (history[type][idx] != NULL)
					break;
				if (++idx == hislen)
					idx = 0;
			} while (idx != hisidx[type]);
			if (idx != hisidx[type] && --idx < 0)
				idx = hislen - 1;
		}
		for (i = 0; i < viminfo_hisidx[type]; i++)
		{
			history[type][idx] = viminfo_history[type][i];
			if (--idx < 0)
				idx = hislen - 1;
		}
		vim_free(viminfo_history[type]);
		viminfo_history[type] = NULL;
	}
}

	void
write_viminfo_history(fp)
	FILE	*fp;
{
	int		i;
	int		type;
	int		num_saved;

	init_history();
	if (hislen == 0)
		return;
	for (type = 0; type <= 1; ++type)
	{
		num_saved = get_viminfo_parameter(type == 0 ? ':' : '/');
		if (num_saved == 0)
			continue;
		if (num_saved < 0)	/* Use default */
			num_saved = hislen;
		fprintf(fp, "\n# %s History (newest to oldest):\n",
							type == 0 ? "Command Line" : "Search String");
		if (num_saved > hislen)
			num_saved = hislen;
		i = hisidx[type];
		if (i >= 0)
			while (num_saved--)
			{
				if (history[type][i] != NULL)
				{
					putc(type == 0 ? ':' : '?', fp);
					viminfo_writestring(fp, history[type][i]);
				}
				if (--i < 0)
					i = hislen - 1;
			}
	}
}
#endif /* VIMINFO */

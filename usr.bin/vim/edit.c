/*	$OpenBSD: edit.c,v 1.2 1996/09/21 06:22:57 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * edit.c: functions for insert mode
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"	/* for op_type */

#ifdef INSERT_EXPAND
/*
 * definitions used for CTRL-X submode
 */
#define CTRL_X_WANT_IDENT		0x100

#define CTRL_X_NOT_DEFINED_YET	(1)
#define CTRL_X_SCROLL			(2)
#define CTRL_X_WHOLE_LINE		(3)
#define CTRL_X_FILES			(4)
#define CTRL_X_TAGS				(5 + CTRL_X_WANT_IDENT)
#define CTRL_X_PATH_PATTERNS	(6 + CTRL_X_WANT_IDENT)
#define CTRL_X_PATH_DEFINES		(7 + CTRL_X_WANT_IDENT)
#define CTRL_X_FINISHED			(8)
#define CTRL_X_DICTIONARY		(9 + CTRL_X_WANT_IDENT)
  
struct Completion
{
	char_u				*str;
	char_u				*fname;
	int					original;
	struct Completion	*next;
	struct Completion	*prev;
};

struct Completion *first_match = NULL;
struct Completion *curr_match = NULL;

static int add_completion __ARGS((char_u *str, int len, char_u *, int dir));
static int make_cyclic __ARGS((void));
static void complete_dictionaries __ARGS((char_u *, int));
static void free_completions __ARGS((void));
static int count_completions __ARGS((void));
#endif /* INSERT_EXPAND */

#define BACKSPACE_CHAR				1
#define BACKSPACE_WORD				2
#define BACKSPACE_WORD_NOT_SPACE	3
#define BACKSPACE_LINE				4

static void change_indent __ARGS((int type, int amount, int round));
static void insert_special __ARGS((int, int));
static void start_arrow __ARGS((FPOS *end_insert_pos));
static void stop_arrow __ARGS((void));
static void stop_insert __ARGS((FPOS *end_insert_pos));
static int echeck_abbr __ARGS((int));

static FPOS 	Insstart;		/* This is where the latest insert/append
								 * mode started. */
static colnr_t	Insstart_textlen;	/* length of line when insert started */
static colnr_t	Insstart_blank_vcol;	/* vcol for first inserted blank */

static char_u	*last_insert = NULL;
							/* the text of the previous insert */
static int		last_insert_skip;
							/* number of chars in front of previous insert */
static int		new_insert_skip;
							/* number of chars in front of the current insert */
#ifdef INSERT_EXPAND
static char_u	*original_text = NULL;
							/* Original text typed before completion */
#endif

#ifdef CINDENT
static int		can_cindent;	/* may do cindenting on this line */
#endif

/*
 * edit() returns TRUE if it returns because of a CTRL-O command
 */
	int
edit(initstr, startln, count)
	int			initstr;
	int 		startln;		/* if set, insert at start of line */
	long 		count;
{
	int			 c;
	int			 cc;
	char_u		*ptr;
	linenr_t	 lnum;
	int 		 temp = 0;
	int			 mode;
	int			 lastc = 0;
	colnr_t		 mincol;
	static linenr_t o_lnum = 0;
	static int	 o_eol = FALSE;
	int			 need_redraw = FALSE;
	int			 i;
	int			 did_backspace = TRUE;		/* previous char was backspace */
#ifdef RIGHTLEFT
	int			revins;						/* reverse insert mode */
	int			revinschars = 0;			/* how much to skip after edit */
	int			revinslegal = 0;			/* was the last char 'legal'? */
	int			revinsscol = -1;			/* start column of revins session */
#endif
#ifdef INSERT_EXPAND
	FPOS		 first_match_pos;
	FPOS		 last_match_pos;
	FPOS		*complete_pos;
	char_u		*complete_pat = NULL;
	char_u		*tmp_ptr;
	char_u		*mesg = NULL;				/* Message about completion */
	int			 started_completion = FALSE;
	colnr_t		 complete_col = 0;			/* init for gcc */
	int			 complete_direction;
	int			 done_dir = 0;				/* Found all matches in this
											 * direction */
	int			 num_matches;
	char_u		**matches;
	regexp		*prog;
	int			 save_sm = -1;				/* init for gcc */
	int			 save_p_scs;
#endif
#ifdef CINDENT
	int			 line_is_white = FALSE;		/* line is empty before insert */
#endif
	FPOS		 tpos;

#ifdef USE_MOUSE
	/*
	 * When doing a paste with the middle mouse button, Insstart is set to
	 * where the paste started.
	 */
	if (where_paste_started.lnum != 0)
		Insstart = where_paste_started;
	else
#endif
	{
		Insstart = curwin->w_cursor;
		if (startln)
			Insstart.col = 0;
	}
	Insstart_textlen = linetabsize(ml_get_curline());
	Insstart_blank_vcol = MAXCOL;

	if (initstr != NUL && !restart_edit)
	{
		ResetRedobuff();
		AppendNumberToRedobuff(count);
		AppendCharToRedobuff(initstr);
		if (initstr == 'g')					/* "gI" command */
			AppendCharToRedobuff('I');
	}

	if (initstr == 'R')
		State = REPLACE;
	else
		State = INSERT;

#ifdef USE_MOUSE
	setmouse();
#endif
	clear_showcmd();
#ifdef RIGHTLEFT
	revins = (State == INSERT && p_ri);	/* there is no reverse replace mode */
	if (revins)
		undisplay_dollar();
#endif

	/*
	 * When CTRL-O . is used to repeat an insert, we get here with
	 * restart_edit non-zero, but something in the stuff buffer
	 */
	if (restart_edit && stuff_empty())
	{
#ifdef USE_MOUSE
		/*
		 * After a paste we consider text typed to be part of the insert for
		 * the pasted text. You can backspace over the paste text too.
		 */
		if (where_paste_started.lnum)
			arrow_used = FALSE;
		else
#endif
			arrow_used = TRUE;
		restart_edit = 0;
		/*
		 * If the cursor was after the end-of-line before the CTRL-O
		 * and it is now at the end-of-line, put it after the end-of-line
		 * (this is not correct in very rare cases).
		 * Also do this if curswant is greater than the current virtual column.
		 * Eg after "^O$" or "^O80|".
		 */
		if (((o_eol && curwin->w_cursor.lnum == o_lnum) ||
								curwin->w_curswant > curwin->w_virtcol) &&
				*(ptr = ml_get_curline() + curwin->w_cursor.col)
																	!= NUL &&
				*(ptr + 1) == NUL)
			++curwin->w_cursor.col;
	}
	else
	{
		arrow_used = FALSE;
		o_eol = FALSE;
	}
#ifdef USE_MOUSE
	where_paste_started.lnum = 0;
#endif
#ifdef CINDENT
	can_cindent = TRUE;
#endif

	/*
	 * If 'showmode' is set, show the current (insert/replace/..) mode.
	 * A warning message for changing a readonly file is given here, before
	 * actually changing anything.  It's put after the mode, if any.
	 */
	i = 0;
	if (p_smd)
		i = showmode();

	if (!p_im)
		change_warning(i + 1);

#ifdef DIGRAPHS
	do_digraph(-1);					/* clear digraphs */
#endif

/*
 * Get the current length of the redo buffer, those characters have to be
 * skipped if we want to get to the inserted characters.
 */
	ptr = get_inserted();
	new_insert_skip = STRLEN(ptr);
	vim_free(ptr);

	old_indent = 0;

	for (;;)
	{
#ifdef RIGHTLEFT
		if (!revinslegal)
			revinsscol = -1;		/* reset on illegal motions */
		else
			revinslegal = 0;
#endif
		if (arrow_used)		/* don't repeat insert when arrow key used */
			count = 0;

			/* set curwin->w_curswant for next K_DOWN or K_UP */
		if (!arrow_used)
			curwin->w_set_curswant = TRUE;

			/* Figure out where the cursor is based on curwin->w_cursor. */
		mincol = curwin->w_col;
		i = curwin->w_row;
		cursupdate();

		/*
		 * When emsg() was called msg_scroll will have been set.
		 */
		msg_scroll = FALSE;

		/*
		 * If we inserted a character at the last position of the last line in
		 * the window, scroll the window one line up. This avoids an extra
		 * redraw.
		 * This is detected when the cursor column is smaller after inserting
		 * something.
		 */
		if (curwin->w_p_wrap && !did_backspace &&
						  (int)curwin->w_col < (int)mincol - curbuf->b_p_ts &&
							   i == curwin->w_winpos + curwin->w_height - 1 &&
								   curwin->w_cursor.lnum != curwin->w_topline)
		{
			++curwin->w_topline;
			updateScreen(VALID_TO_CURSCHAR);
			cursupdate();
			need_redraw = FALSE;
		}
		did_backspace = FALSE;

		/*
		 * redraw is postponed until after cursupdate() to make 'dollar'
		 * option work correctly.
		 */
		if (need_redraw)
		{
			updateline();
			need_redraw = FALSE;
		}

		showruler(0);
		setcursor();
		emsg_on_display = FALSE;		/* may remove error message now */

		c = vgetc();
		if (c == Ctrl('C'))
			got_int = FALSE;

#ifdef RIGHTLEFT
		if (p_hkmap && KeyTyped)
			c = hkmap(c);				/* Hebrew mode mapping */
#endif

#ifdef INSERT_EXPAND
		if (ctrl_x_mode == CTRL_X_NOT_DEFINED_YET)
		{
			/* We have just entered ctrl-x mode and aren't quite sure which
			 * ctrl-x mode it will be yet.  Now we decide -- webb
			 */
			switch (c)
			{
				case Ctrl('E'):
				case Ctrl('Y'):
					ctrl_x_mode = CTRL_X_SCROLL;
					if (State == INSERT)
						edit_submode = (char_u *)" (insert) Scroll (^E/^Y)";
					else
						edit_submode = (char_u *)" (replace) Scroll (^E/^Y)";
					break;
				case Ctrl('L'):
					ctrl_x_mode = CTRL_X_WHOLE_LINE;
					edit_submode = (char_u *)" Whole line completion (^L/^N/^P)";
					break;
				case Ctrl('F'):
					ctrl_x_mode = CTRL_X_FILES;
					edit_submode = (char_u *)" File name completion (^F/^N/^P)";
					break;
				case Ctrl('K'):
					ctrl_x_mode = CTRL_X_DICTIONARY;
					edit_submode = (char_u *)" Dictionary completion (^K/^N/^P)";
					break;
				case Ctrl(']'):
					ctrl_x_mode = CTRL_X_TAGS;
					edit_submode = (char_u *)" Tag completion (^]/^N/^P)";
					break;
				case Ctrl('I'):
					ctrl_x_mode = CTRL_X_PATH_PATTERNS;
					edit_submode = (char_u *)" Path pattern completion (^N/^P)";
					break;
				case Ctrl('D'):
					ctrl_x_mode = CTRL_X_PATH_DEFINES;
					edit_submode = (char_u *)" Definition completion (^D/^N/^P)";
					break;
				default:
					ctrl_x_mode = 0;
					break;
			}
			showmode();
		}
		else if (ctrl_x_mode)
		{
			/* We we're already in ctrl-x mode, do we stay in it? */
			if (!is_ctrl_x_key(c))
			{
				if (ctrl_x_mode == CTRL_X_SCROLL)
					ctrl_x_mode = 0;
				else
					ctrl_x_mode = CTRL_X_FINISHED;
				edit_submode = NULL;
			}
			showmode();
		}
		if (started_completion || ctrl_x_mode == CTRL_X_FINISHED)
		{
			/* Show error message from attempted keyword completion (probably
			 * 'Pattern not found') until another key is hit, then go back to
			 * showing what mode we are in.
			 */
			showmode();
			if ((ctrl_x_mode == 0 && c != Ctrl('N') && c != Ctrl('P')) ||
												ctrl_x_mode == CTRL_X_FINISHED)
			{
				/* Get here when we have finished typing a sequence of ^N and
				 * ^P or other completion characters in CTRL-X mode. Free up
				 * memory that was used, and make sure we can redo the insert
				 * -- webb.
				 */
				if (curr_match != NULL)
				{
					/*
					 * If any of the original typed text has been changed,
					 * eg when ignorecase is set, we must add back-spaces to
					 * the redo buffer.  We add as few as necessary to delete
					 * just the part of the original text that has changed
					 * -- webb
					 */
					ptr = curr_match->str;
					tmp_ptr = original_text;
					while (*tmp_ptr && *tmp_ptr == *ptr)
					{
						++tmp_ptr;
						++ptr;
					}
					for (temp = 0; tmp_ptr[temp]; ++temp)
						AppendCharToRedobuff(K_BS);
					if (*ptr)
						AppendToRedobuff(ptr);
				}
				/* Break line if it's too long */
				lnum = curwin->w_cursor.lnum;
				insertchar(NUL, FALSE, -1);
				if (lnum != curwin->w_cursor.lnum)
					updateScreen(CURSUPD);
				else
					need_redraw = TRUE;

				vim_free(complete_pat);
				complete_pat = NULL;
				vim_free(original_text);
				original_text = NULL;
				free_completions();
				started_completion = FALSE;
				ctrl_x_mode = 0;
				p_sm = save_sm;
				if (edit_submode != NULL)
				{
					edit_submode = NULL;
					showmode();
				}
			}
		}
#endif /* INSERT_EXPAND */

		if (c != Ctrl('D'))			/* remember to detect ^^D and 0^D */
			lastc = c;

#ifdef DIGRAPHS
		c = do_digraph(c);
#endif /* DIGRAPHS */

		if (c == Ctrl('V') || c == Ctrl('Q'))
		{
			if (NextScreen != NULL)
				screen_outchar('^', curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
					curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
															   curwin->w_col);
			AppendToRedobuff((char_u *)"\026");	/* CTRL-V */
			cursupdate();

			if (!add_to_showcmd(c, FALSE))
				setcursor();

			c = get_literal();
			clear_showcmd();
			insert_special(c, TRUE);
			need_redraw = TRUE;
#ifdef RIGHTLEFT
			revinschars++;
			revinslegal++;
#endif
			continue;
		}

#ifdef CINDENT
		if (curbuf->b_p_cin
# ifdef INSERT_EXPAND
							&& !ctrl_x_mode
# endif
											   )
		{
			line_is_white = inindent(0);

			/* 
			 * A key name preceded by a bang means that this
			 * key wasn't destined to be inserted.  Skip ahead
			 * to the re-indenting if we find one.
			 */
			if (in_cinkeys(c, '!', line_is_white))
				goto force_cindent;

			/* 
			 * A key name preceded by a star means that indenting
			 * has to be done before inserting the key.
			 */
			if (can_cindent && in_cinkeys(c, '*', line_is_white))
			{
				stop_arrow();

				/* re-indent the current line */
				fixthisline(get_c_indent);

				/* draw the changes on the screen later */
				need_redraw = TRUE;
			}
		}
#endif /* CINDENT */

#ifdef RIGHTLEFT
		if (curwin->w_p_rl)
			switch (c)
			{
				case K_LEFT:	c = K_RIGHT; break;
				case K_S_LEFT:	c = K_S_RIGHT; break;
				case K_RIGHT:	c = K_LEFT; break;
				case K_S_RIGHT: c = K_S_LEFT; break;
			}
#endif

		switch (c)		/* handle character in insert mode */
		{
			  case K_INS:			/* toggle insert/replace mode */
			    if (State == REPLACE)
					State = INSERT;
				else
					State = REPLACE;
				AppendCharToRedobuff(K_INS);
				showmode();
				break;

#ifdef INSERT_EXPAND
			  case Ctrl('X'):		/* Enter ctrl-x mode */
				/* We're not sure which ctrl-x mode it will be yet */
				ctrl_x_mode = CTRL_X_NOT_DEFINED_YET;
				MSG("^X mode (^E/^Y/^L/^]/^F/^I/^K/^D)");
				break;
#endif /* INSERT_EXPAND */

			  case Ctrl('O'):		/* execute one command */
			    if (echeck_abbr(Ctrl('O') + ABBR_OFF))
					break;
			  	count = 0;
				if (State == INSERT)
					restart_edit = 'I';
				else
					restart_edit = 'R';
				o_lnum = curwin->w_cursor.lnum;
				o_eol = (gchar_cursor() == NUL);
				goto doESCkey;

			  /* Hitting the help key in insert mode is like <ESC> <Help> */
			  case K_HELP:
			  case K_F1:
			  	stuffcharReadbuff(K_HELP);
				/*FALLTHROUGH*/

			  case ESC: 			/* an escape ends input mode */
			    if (echeck_abbr(ESC + ABBR_OFF))
					break;
				/*FALLTHROUGH*/

			  case Ctrl('C'):
doESCkey:
				temp = curwin->w_cursor.col;
				if (!arrow_used)
				{
					AppendToRedobuff(ESC_STR);

					if (--count > 0)		/* repeat what was typed */
					{
						(void)start_redo_ins();
						continue;
					}
					stop_insert(&curwin->w_cursor);
					if (dollar_vcol)
					{
						dollar_vcol = 0;
						/* may have to redraw status line if this was the
						 * first change, show "[+]" */
						if (curwin->w_redr_status == TRUE)
							must_redraw = NOT_VALID;
						else
							need_redraw = TRUE;
					}
				}
				if (need_redraw)
					updateline();

				/* When an autoindent was removed, curswant stays after the
				 * indent */
				if (!restart_edit && (colnr_t)temp == curwin->w_cursor.col)
					curwin->w_set_curswant = TRUE;

				/*
				 * The cursor should end up on the last inserted character.
				 */
				if (curwin->w_cursor.col != 0 &&
						  (!restart_edit || gchar_cursor() == NUL)
#ifdef RIGHTLEFT
									  && !revins
#endif
												  )
					--curwin->w_cursor.col;
				if (State == REPLACE)
					replace_flush();	/* free replace stack */
				State = NORMAL;
#ifdef USE_MOUSE
				setmouse();
#endif
					/* inchar() may have deleted the "INSERT" message */
					/* for CTRL-O we display -- INSERT COMMAND -- */
				if (Recording || restart_edit)
					showmode();
				else if (p_smd)
					MSG("");
				old_indent = 0;

				/*
				 * This is the ONLY return from edit().
				 */
				return (c == Ctrl('O'));

			  	/*
				 * Insert the previously inserted text.
				 * For ^@ the trailing ESC will end the insert, unless there
				 * is an error.
				 */
			  case K_ZERO:
			  case NUL:
			  case Ctrl('A'):
				if (stuff_inserted(NUL, 1L, (c == Ctrl('A'))) == FAIL &&
															   c != Ctrl('A'))
					goto doESCkey;			/* quit insert mode */
				break;

			  	/*
				 * insert the contents of a register
				 */
			  case Ctrl('R'):
				if (NextScreen != NULL)
					screen_outchar('"', curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
						curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
															curwin->w_col);
				if (!add_to_showcmd(c, FALSE))
					setcursor();
					/* don't map the register name. This also prevents the
					 * mode message to be deleted when ESC is hit */
				++no_mapping;
#ifdef HAVE_LANGMAP
				cc = vgetc();
				LANGMAP_ADJUST(cc, TRUE);
			  	if (insertbuf(cc) == FAIL)
#else
			  	if (insertbuf(vgetc()) == FAIL)
#endif
				{
					beep_flush();
					need_redraw = TRUE;		/* remove the '"' */
				}
				--no_mapping;
				clear_showcmd();
				break;

#ifdef RIGHTLEFT
			  case Ctrl('B'):			/* toggle reverse insert mode */
			  	p_ri = !p_ri;
				revins = (State == INSERT && p_ri);
				if (revins)
					undisplay_dollar();
				showmode();
				break;

			  case Ctrl('_'):		/* toggle language: khmap and revins */
									/* Move to end of reverse inserted text */
				if (revins && revinschars && revinsscol >= 0)
					while (gchar_cursor() != NUL && revinschars--)
						++curwin->w_cursor.col;
				p_ri = !p_ri;
				revins = (State == INSERT && p_ri);
				if (revins)
				{
					revinsscol = curwin->w_cursor.col;
					revinslegal++;
					revinschars = 0;
					undisplay_dollar();
				}
				else
					revinsscol = -1;
				p_hkmap = curwin->w_p_rl ^ p_ri;    /* be consistent! */
				showmode();
				break;
#endif

				/*
				 * If the cursor is on an indent, ^T/^D insert/delete one
				 * shiftwidth.  Otherwise ^T/^D behave like a "<<" or ">>".
				 * Always round the indent to 'shiftwith', this is compatible
				 * with vi.  But vi only supports ^T and ^D after an
				 * autoindent, we support it everywhere.
				 */
			  case Ctrl('D'): 		/* make indent one shiftwidth smaller */
#ifdef INSERT_EXPAND
				if (ctrl_x_mode == CTRL_X_PATH_DEFINES)
					goto docomplete;
#endif /* INSERT_EXPAND */
				/* FALLTHROUGH */
			  case Ctrl('T'):		/* make indent one shiftwidth greater */
				stop_arrow();
				AppendCharToRedobuff(c);

				/*
				 * 0^D and ^^D: remove all indent.
				 */
				if ((lastc == '0' || lastc == '^') && curwin->w_cursor.col)
				{
					--curwin->w_cursor.col;
					(void)delchar(FALSE);			/* delete the '^' or '0' */
					if (lastc == '^')
						old_indent = get_indent();	/* remember curr. indent */
					change_indent(INDENT_SET, 0, TRUE);
				}
				else
					change_indent(c == Ctrl('D') ? INDENT_DEC : INDENT_INC,
																	 0, TRUE);

				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
#ifdef CINDENT
				can_cindent = FALSE;		/* no cindenting after ^D or ^T */
#endif
		  		goto redraw;

			  case K_DEL:
				stop_arrow();
			  	if (gchar_cursor() == NUL)		/* delete newline */
				{
					temp = curwin->w_cursor.col;
					if (!p_bs ||				/* only if 'bs' set */
						u_save((linenr_t)(curwin->w_cursor.lnum - 1),
							(linenr_t)(curwin->w_cursor.lnum + 2)) == FAIL ||
								do_join(FALSE, TRUE) == FAIL)
						beep_flush();
					else
						curwin->w_cursor.col = temp;
				}
				else if (delchar(FALSE) == FAIL)/* delete char under cursor */
					beep_flush();
				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				AppendCharToRedobuff(c);
				goto redraw;

			  case K_BS:
			  case Ctrl('H'):
				mode = BACKSPACE_CHAR;
dodel:
				/* can't delete anything in an empty file */
				/* can't backup past first character in buffer */
				/* can't backup past starting point unless 'backspace' > 1 */
				/* can backup to a previous line if 'backspace' == 0 */
				if (bufempty() || (
#ifdef RIGHTLEFT
						!revins &&
#endif
						((curwin->w_cursor.lnum == 1 &&
									curwin->w_cursor.col <= 0) ||
						(p_bs < 2 && (arrow_used ||
							(curwin->w_cursor.lnum == Insstart.lnum &&
							curwin->w_cursor.col <= Insstart.col) ||
							(curwin->w_cursor.col <= 0 && p_bs == 0))))))
				{
					beep_flush();
					goto redraw;
				}

				stop_arrow();
#ifdef CINDENT
				if (inindent(0))
					can_cindent = FALSE;
#endif
#ifdef RIGHTLEFT
				if (revins)			/* put cursor after last inserted char */
					inc_cursor();
#endif
				if (curwin->w_cursor.col <= 0)		/* delete newline! */
				{
					lnum = Insstart.lnum;
					if (curwin->w_cursor.lnum == Insstart.lnum
#ifdef RIGHTLEFT
									|| revins
#endif
												)
					{
						if (u_save((linenr_t)(curwin->w_cursor.lnum - 2),
								(linenr_t)(curwin->w_cursor.lnum + 1)) == FAIL)
							goto redraw;
						--Insstart.lnum;
						Insstart.col = 0;
					}
					/*
					 * In replace mode:
					 * cc < 0: NL was inserted, delete it
					 * cc >= 0: NL was replaced, put original characters back
					 */
					cc = -1;
					if (State == REPLACE)
						cc = replace_pop();
				/* in replace mode, in the line we started replacing, we
														only move the cursor */
					if (State != REPLACE || curwin->w_cursor.lnum > lnum)
					{
						temp = gchar_cursor();		/* remember current char */
						--curwin->w_cursor.lnum;
						(void)do_join(FALSE, curs_rows() == OK);
						if (temp == NUL && gchar_cursor() != NUL)
							++curwin->w_cursor.col;
						/*
						 * in REPLACE mode we have to put back the text that
						 * was replace by the NL. On the replace stack is
						 * first a NUL-terminated sequence of characters that
						 * were deleted and then the character that NL
						 * replaced.
						 */
						if (State == REPLACE)
						{
							/*
							 * Do the next ins_char() in NORMAL state, to
							 * prevent ins_char() from replacing characters and
							 * avoiding showmatch().
							 */
							State = NORMAL;
							/*
							 * restore blanks deleted after cursor
							 */
							while (cc > 0)
							{
								temp = curwin->w_cursor.col;
								ins_char(cc);
								curwin->w_cursor.col = temp;
								cc = replace_pop();
							}
							cc = replace_pop();
							if (cc > 0)
							{
								ins_char(cc);
								dec_cursor();
							}
							State = REPLACE;
						}
					}
					else
						dec_cursor();
					did_ai = FALSE;
				}
				else
				{
#ifdef RIGHTLEFT
					if (revins)			/* put cursor on last inserted char */
						dec_cursor();
#endif
					mincol = 0;
															/* keep indent */
					if (mode == BACKSPACE_LINE && curbuf->b_p_ai
#ifdef RIGHTLEFT
							&& !revins
#endif
										)
					{
						temp = curwin->w_cursor.col;
						beginline(TRUE);
						if (curwin->w_cursor.col < (colnr_t)temp)
							mincol = curwin->w_cursor.col;
						curwin->w_cursor.col = temp;
					}

					/* delete upto starting point, start of line or previous
					 * word */
					do
					{
#ifdef RIGHTLEFT
						if (!revins)	/* put cursor on char to be deleted */
#endif
							dec_cursor();

								/* start of word? */
						if (mode == BACKSPACE_WORD &&
												!vim_isspace(gchar_cursor()))
						{
							mode = BACKSPACE_WORD_NOT_SPACE;
							temp = iswordchar(gchar_cursor());
						}
								/* end of word? */
						else if (mode == BACKSPACE_WORD_NOT_SPACE &&
										  (vim_isspace(cc = gchar_cursor()) ||
													  iswordchar(cc) != temp))
						{
#ifdef RIGHTLEFT
							if (!revins)
#endif
								inc_cursor();
#ifdef RIGHTLEFT
							else if (State == REPLACE)
								dec_cursor();
#endif
							break;
						}
						if (State == REPLACE)
						{
							/*
							 * cc < 0: replace stack empty, just move cursor
							 * cc == 0: character was inserted, delete it
							 * cc > 0: character was replace, put original back
							 */
							cc = replace_pop();
							if (cc > 0)
								pchar_cursor(cc);
							else if (cc == 0)
								(void)delchar(FALSE);
						}
						else  /* State != REPLACE */
						{
							(void)delchar(FALSE);
#ifdef RIGHTLEFT
							if (revinschars)
							{
								revinschars--;
								revinslegal++;
							}
							if (revins && gchar_cursor() == NUL)
								break;
#endif
						}
						/* Just a single backspace?: */
						if (mode == BACKSPACE_CHAR)
							break;
					} while (
#ifdef RIGHTLEFT
							revins ||
#endif
							(curwin->w_cursor.col > mincol &&
							(curwin->w_cursor.lnum != Insstart.lnum ||
							curwin->w_cursor.col != Insstart.col)));
					did_backspace = TRUE;
				}
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				if (curwin->w_cursor.col <= 1)
					did_ai = FALSE;
				/*
				 * It's a little strange to put backspaces into the redo
				 * buffer, but it makes auto-indent a lot easier to deal
				 * with.
				 */
				AppendCharToRedobuff(c);
redraw:
				need_redraw = TRUE;
				break;

			  case Ctrl('W'):		/* delete word before cursor */
			  	mode = BACKSPACE_WORD;
			  	goto dodel;

			  case Ctrl('U'):		/* delete inserted text in current line */
				mode = BACKSPACE_LINE;
			  	goto dodel;

#ifdef USE_MOUSE
			  case K_LEFTMOUSE:
			  case K_LEFTDRAG:
			  case K_LEFTRELEASE:
			  case K_MIDDLEMOUSE:
			  case K_MIDDLEDRAG:
			  case K_MIDDLERELEASE:
			  case K_RIGHTMOUSE:
			  case K_RIGHTDRAG:
			  case K_RIGHTRELEASE:
#ifdef USE_GUI
				/* When GUI is active, also move/paste when 'mouse' is empty */
				if (!gui.in_use)
#endif
					if (!mouse_has(MOUSE_INSERT))
						break;

				undisplay_dollar();
				tpos = curwin->w_cursor;
				if (do_mouse(c, BACKWARD, 1L, FALSE))
				{
					start_arrow(&tpos);
# ifdef CINDENT
					can_cindent = TRUE;
# endif
				}

				break;

			  case K_IGNORE:
				break;
#endif

#ifdef USE_GUI
			  case K_SCROLLBAR:
				undisplay_dollar();
				tpos = curwin->w_cursor;
				if (gui_do_scroll())
				{
					start_arrow(&tpos);
# ifdef CINDENT
					can_cindent = TRUE;
# endif
				}
				break;

			  case K_HORIZ_SCROLLBAR:
				undisplay_dollar();
				tpos = curwin->w_cursor;
				if (gui_do_horiz_scroll())
				{
					start_arrow(&tpos);
#ifdef CINDENT
					can_cindent = TRUE;
#endif
				}
				break;
#endif

			  case K_LEFT:
				undisplay_dollar();
				tpos = curwin->w_cursor;
			  	if (oneleft() == OK)
				{
					start_arrow(&tpos);
#ifdef RIGHTLEFT
					/* If exit reversed string, position is fixed */
					if (revinsscol != -1 &&
									  (int)curwin->w_cursor.col >= revinsscol)
						revinslegal++;
					revinschars++;
#endif
				}

				/*
				 * if 'whichwrap' set for cursor in insert mode may go to
				 * previous line
				 */
				else if (vim_strchr(p_ww, '[') != NULL &&
													curwin->w_cursor.lnum > 1)
				{
					start_arrow(&tpos);
					--(curwin->w_cursor.lnum);
					coladvance(MAXCOL);
					curwin->w_set_curswant = TRUE;	/* so we stay at the end */
				}
				else
					beep_flush();
				break;

			  case K_HOME:
			  case K_KHOME:
				undisplay_dollar();
				tpos = curwin->w_cursor;
				if ((mod_mask & MOD_MASK_CTRL))
					curwin->w_cursor.lnum = 1;
			  	curwin->w_cursor.col = 0;
				curwin->w_curswant = 0;
				start_arrow(&tpos);
				break;

			  case K_END:
			  case K_KEND:
				undisplay_dollar();
				tpos = curwin->w_cursor;
				if ((mod_mask & MOD_MASK_CTRL))
					curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				coladvance(MAXCOL);
				curwin->w_curswant = MAXCOL;
				start_arrow(&tpos);
				break;

			  case K_S_LEFT:
				undisplay_dollar();
			  	if (curwin->w_cursor.lnum > 1 || curwin->w_cursor.col > 0)
				{
					start_arrow(&curwin->w_cursor);
					(void)bck_word(1L, 0, FALSE);
				}
				else
					beep_flush();
				break;

			  case K_RIGHT:
				undisplay_dollar();
				if (gchar_cursor() != NUL)
				{
					start_arrow(&curwin->w_cursor);
					curwin->w_set_curswant = TRUE;
					++curwin->w_cursor.col;
#ifdef RIGHTLEFT
					revinslegal++;
					if (revinschars)
						revinschars--;
#endif
				}
					/* if 'whichwrap' set for cursor in insert mode may go
					 * to next line */
				else if (vim_strchr(p_ww, ']') != NULL &&
						   curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
				{
					start_arrow(&curwin->w_cursor);
					curwin->w_set_curswant = TRUE;
					++curwin->w_cursor.lnum;
					curwin->w_cursor.col = 0;
				}
				else
					beep_flush();
				break;

			  case K_S_RIGHT:
				undisplay_dollar();
			  	if (curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count ||
														gchar_cursor() != NUL)
				{
					start_arrow(&curwin->w_cursor);
					(void)fwd_word(1L, 0, 0);
				}
				else
					beep_flush();
				break;

			  case K_UP:
				undisplay_dollar();
				tpos = curwin->w_cursor;
			  	if (cursor_up(1L) == OK)
				{
					start_arrow(&tpos);
#ifdef CINDENT
					can_cindent = TRUE;
#endif
				}
				else
					beep_flush();
				break;

			  case K_S_UP:
			  case K_PAGEUP:
			  case K_KPAGEUP:
				undisplay_dollar();
				tpos = curwin->w_cursor;
			  	if (onepage(BACKWARD, 1L) == OK)
				{
					start_arrow(&tpos);
#ifdef CINDENT
					can_cindent = TRUE;
#endif
				}
				else
					beep_flush();
				break;

			  case K_DOWN:
				undisplay_dollar();
				tpos = curwin->w_cursor;
			  	if (cursor_down(1L) == OK)
				{
					start_arrow(&tpos);
#ifdef CINDENT
					can_cindent = TRUE;
#endif
				}
				else
					beep_flush();
				break;

			  case K_S_DOWN:
			  case K_PAGEDOWN:
			  case K_KPAGEDOWN:
				undisplay_dollar();
				tpos = curwin->w_cursor;
			  	if (onepage(FORWARD, 1L) == OK)
				{
					start_arrow(&tpos);
#ifdef CINDENT
					can_cindent = TRUE;
#endif
				}
				else
					beep_flush();
				break;

			  case TAB:				/* TAB or Complete patterns along path */
#ifdef INSERT_EXPAND
				if (ctrl_x_mode == CTRL_X_PATH_PATTERNS)
					goto docomplete;
#endif /* INSERT_EXPAND */

				if (Insstart_blank_vcol == MAXCOL &&
									   curwin->w_cursor.lnum == Insstart.lnum)
					Insstart_blank_vcol = curwin->w_virtcol;
			    if (echeck_abbr(TAB + ABBR_OFF))
					break;
				i = inindent(0);
#ifdef CINDENT
			    if (i)
					can_cindent = FALSE;
#endif
			  	if (!curbuf->b_p_et && !(p_sta && i))
					goto normalchar;

				stop_arrow();
				did_ai = FALSE;
				did_si = FALSE;
				can_si = FALSE;
				can_si_back = FALSE;
				AppendToRedobuff((char_u *)"\t");

				if (p_sta && i)					/* insert tab in indent */
				{
					change_indent(INDENT_INC, 0, p_sr);
					goto redraw;
				}

				/*
				 * p_et is set: expand a tab into spaces
				 */
				temp = (int)curbuf->b_p_ts;
				temp -= curwin->w_virtcol % temp;

				/*
				 * insert the first space with ins_char(); it will delete one
				 * char in replace mode. Insert the rest with ins_str(); it
				 * will not delete any chars
				 */
				ins_char(' ');
				while (--temp)
				{
					ins_str((char_u *)" ");
					if (State == REPLACE)		/* no char replaced */
						replace_push(NUL);
				}
				goto redraw;

			  case CR:
			  case NL:
			    if (echeck_abbr(c + ABBR_OFF))
					break;
				stop_arrow();
				if (State == REPLACE)
					replace_push(NUL);
#ifdef RIGHTLEFT
				/* NL in reverse insert will allways start in the end of
				 * current line. */
				if (revins)
					while (gchar_cursor() != NUL)
						++curwin->w_cursor.col;
#endif

				AppendToRedobuff(NL_STR);
				if (has_format_option(FO_RET_COMS))
					fo_do_comments = TRUE;
				i = Opencmd(FORWARD, TRUE, FALSE);
				fo_do_comments = FALSE;
#ifdef CINDENT
				can_cindent = TRUE;
#endif
				if (!i)
					goto doESCkey;		/* out of memory */
				break;

#ifdef DIGRAPHS
			  case Ctrl('K'):
#ifdef INSERT_EXPAND
				if (ctrl_x_mode == CTRL_X_DICTIONARY)
					goto docomplete;
#endif
				if (NextScreen != NULL)
					screen_outchar('?', curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
						curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
															curwin->w_col);
				if (!add_to_showcmd(c, FALSE))
					setcursor();
					/* don't map the digraph chars. This also prevents the
					 * mode message to be deleted when ESC is hit */
				++no_mapping;
				++allow_keys;
			  	c = vgetc();
				--no_mapping;
				--allow_keys;
				if (IS_SPECIAL(c))		/* special key */
				{
					clear_showcmd();
					insert_special(c, TRUE);
					need_redraw = TRUE;
					break;
				}
				if (c != ESC)
				{
					if (charsize(c) == 1 && NextScreen != NULL)
 						screen_outchar(c, curwin->w_winpos + curwin->w_row,
#ifdef RIGHTLEFT
							curwin->w_p_rl ? (int)Columns - 1 - curwin->w_col :
#endif
																curwin->w_col);
					if (!add_to_showcmd(c, FALSE))
						setcursor();
					++no_mapping;
					++allow_keys;
					cc = vgetc();
					--no_mapping;
					--allow_keys;
					if (cc != ESC)
					{
						AppendToRedobuff((char_u *)"\026");	/* CTRL-V */
						c = getdigraph(c, cc, TRUE);
						clear_showcmd();
						goto normalchar;
					}
				}
				clear_showcmd();
				need_redraw = TRUE;
				break;
#else /* DIGRAPHS */
# ifdef INSERT_EXPAND
			  case Ctrl('K'):
				if (ctrl_x_mode != CTRL_X_DICTIONARY)
					goto normalchar;
				goto docomplete;
# endif /* INSERT_EXPAND */
#endif /* DIGRAPHS */

#ifdef INSERT_EXPAND
			  case Ctrl(']'):			/* Tag name completion after ^X */
				if (ctrl_x_mode != CTRL_X_TAGS)
					goto normalchar;
				goto docomplete;

			  case Ctrl('F'):			/* File name completion after ^X */
				if (ctrl_x_mode != CTRL_X_FILES)
					goto normalchar;
				goto docomplete;

			  case Ctrl('L'):			/* Whole line completion after ^X */
				if (ctrl_x_mode != CTRL_X_WHOLE_LINE)
					goto normalchar;
				/* FALLTHROUGH */

			  case Ctrl('P'):			/* Do previous pattern completion */
			  case Ctrl('N'):			/* Do next pattern completion */
docomplete:
				if (c == Ctrl('P') || c == Ctrl('L'))
					complete_direction = BACKWARD;
				else
					complete_direction = FORWARD;
				mesg = NULL;			/* No message by default */
				if (!started_completion)
				{
					/* First time we hit ^N or ^P (in a row, I mean) */

					/* Turn off 'sm' so we don't show matches with ^X^L */
					save_sm = p_sm;
					p_sm = FALSE;

					if (ctrl_x_mode == 0)
					{
						edit_submode = (char_u *)" Keyword completion (^P/^N)";
						showmode();
					}
					did_ai = FALSE;
					did_si = FALSE;
					can_si = FALSE;
					can_si_back = FALSE;
					stop_arrow();
					done_dir = 0;
					first_match_pos = curwin->w_cursor;
					ptr = tmp_ptr = ml_get(first_match_pos.lnum);
					complete_col = first_match_pos.col;
					temp = (int)complete_col - 1;

					/* Work out completion pattern and original text -- webb */
					if (ctrl_x_mode == 0 || (ctrl_x_mode & CTRL_X_WANT_IDENT))
					{
						if (temp < 0 || !iswordchar(ptr[temp]))
						{
							/* Match any word of at least two chars */
							complete_pat = strsave((char_u *)"\\<\\k\\k");
							if (complete_pat == NULL)
								break;
							tmp_ptr += complete_col;
							temp = 0;
						}
						else
						{
							while (temp >= 0 && iswordchar(ptr[temp]))
								temp--;
							tmp_ptr += ++temp;
							if ((temp = (int)complete_col - temp) == 1)
							{
								/* Only match word with at least two
								 * chars -- webb
								 */
								sprintf((char *)IObuff, "\\<%c\\k", *tmp_ptr);
								complete_pat = strsave(IObuff);
								if (complete_pat == NULL)
									break;
							}
							else
							{
								complete_pat = alloc(temp + 3);
								if (complete_pat == NULL)
									break;
								sprintf((char *)complete_pat, "\\<%.*s", temp,
																	tmp_ptr);
							}
						}
					}
					else if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
					{
						tmp_ptr = skipwhite(ptr);
						temp = (int)complete_col - (tmp_ptr - ptr);
						complete_pat = strnsave(tmp_ptr, temp);
						if (complete_pat == NULL)
							break;
					}
					else if (ctrl_x_mode == CTRL_X_FILES)
					{
						while (temp >= 0 && isfilechar(ptr[temp]))
							temp--;
						tmp_ptr += ++temp;
						temp = (int)complete_col - temp;
						complete_pat = addstar(tmp_ptr, temp);
						if (complete_pat == NULL)
							break;
					}
					original_text = strnsave(tmp_ptr, temp);
					if (original_text == NULL)
					{
						vim_free(complete_pat);
						complete_pat = NULL;
						break;
					}

					complete_col = tmp_ptr - ptr;
					first_match_pos.col -= temp;

					/* So that ^N can match word immediately after cursor */
					if (ctrl_x_mode == 0)
						dec(&first_match_pos);

					last_match_pos = first_match_pos;

					/* Get list of all completions now, if appropriate */
					if (ctrl_x_mode == CTRL_X_PATH_PATTERNS ||
						ctrl_x_mode == CTRL_X_PATH_DEFINES)
					{
						started_completion = TRUE;
						find_pattern_in_path(complete_pat,
								(int)STRLEN(complete_pat), FALSE, FALSE,
							(ctrl_x_mode == CTRL_X_PATH_DEFINES) ? FIND_DEFINE
							: FIND_ANY, 1L, ACTION_EXPAND,
							(linenr_t)1, (linenr_t)MAXLNUM);

						if (make_cyclic() > 1)
						{
							sprintf((char *)IObuff, "There are %d matches",
								count_completions());
							mesg = IObuff;
						}
					}
					else if (ctrl_x_mode == CTRL_X_DICTIONARY)
					{
						started_completion = TRUE;
						if (*p_dict == NUL)
							mesg = (char_u *)"'dictionary' option is empty";
						else
						{
							complete_dictionaries(complete_pat,
														  complete_direction);
							if (make_cyclic() > 1)
							{
								sprintf((char *)IObuff,
									"There are %d matching words",
									count_completions());
								mesg = IObuff;
							}
						}
					}
					else if (ctrl_x_mode == CTRL_X_TAGS)
					{
						started_completion = TRUE;
							/* set reg_ic according to p_ic, p_scs and pat */
						set_reg_ic(complete_pat);
						prog = vim_regcomp(complete_pat);
						if (prog != NULL &&
							find_tags(NULL, prog, &num_matches, &matches,
									   FALSE, FALSE) == OK && num_matches > 0)
						{
							for (i = 0; i < num_matches; i++)
								if (add_completion(matches[i], -1, NULL,
														FORWARD) == RET_ERROR)
									break;
							FreeWild(num_matches, matches);
							vim_free(prog);
							if (make_cyclic() > 1)
							{
								sprintf((char *)IObuff,
									"There are %d matching tags",
									count_completions());
								mesg = IObuff;
							}
						}
						else
						{
							vim_free(prog);
							vim_free(complete_pat);
							complete_pat = NULL;
						}
					}
					else if (ctrl_x_mode == CTRL_X_FILES)
					{
						started_completion = TRUE;
						expand_interactively = TRUE;
						if (ExpandWildCards(1, &complete_pat, &num_matches,
												&matches, FALSE, FALSE) == OK)
						{
							/*
							 * May change home directory back to "~".
							 */
							tilde_replace(complete_pat, num_matches, matches);
							for (i = 0; i < num_matches; i++)
								if (add_completion(matches[i], -1, NULL,
														FORWARD) == RET_ERROR)
									break;
							FreeWild(num_matches, matches);
							if (make_cyclic() > 1)
							{
								sprintf((char *)IObuff,
									"There are %d matching file names",
									count_completions());
								mesg = IObuff;
							}
						}
						else
						{
							vim_free(complete_pat);
							complete_pat = NULL;
						}
						expand_interactively = FALSE;
					}
				}
				/*
				 * In insert mode: Delete the typed part.
				 * In replace mode: Put the old characters back, if any.
				 */
				while (curwin->w_cursor.col > complete_col)
				{
					curwin->w_cursor.col--;
					if (State == REPLACE)
					{
						if ((cc = replace_pop()) > 0)
							pchar(curwin->w_cursor, cc);
					}
					else
						delchar(FALSE);
				}
				complete_pos = NULL;
				if (started_completion && curr_match == NULL &&
										(p_ws || done_dir == BOTH_DIRECTIONS))
				{
					edit_submode_extra = e_patnotf;
					edit_submode_highl = TRUE;
				}
				else if (curr_match != NULL && complete_direction == FORWARD &&
											curr_match->next != NULL)
					curr_match = curr_match->next;
				else if (curr_match != NULL && complete_direction == BACKWARD &&
											curr_match->prev != NULL)
					curr_match = curr_match->prev;
				else
				{
					complete_pos = (complete_direction == FORWARD) ?
										&last_match_pos : &first_match_pos;
					/*
					 * If 'infercase' is set, don't use 'smartcase' here
					 */
					save_p_scs = p_scs;
					if (curbuf->b_p_inf)
						p_scs = FALSE;
					for (;;)
					{
						if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
							temp = search_for_exact_line(complete_pos,
									complete_direction, complete_pat);
						else
							temp = searchit(complete_pos, complete_direction,
									complete_pat, 1L,
									SEARCH_KEEP + SEARCH_NFMSG, RE_LAST);
						if (temp == FAIL)
						{
							if (!p_ws && done_dir != -complete_direction)
							{
								/*
								 * With nowrapscan, we haven't finished
								 * looking in the other direction yet -- webb
								 */
								temp = OK;
								done_dir = complete_direction;
							}
							else if (!p_ws)
								done_dir = BOTH_DIRECTIONS;
							break;
						}
						if (!started_completion)
						{
							started_completion = TRUE;
							first_match_pos = *complete_pos;
							last_match_pos = *complete_pos;
						}
						else if (first_match_pos.lnum == last_match_pos.lnum &&
						  first_match_pos.col == last_match_pos.col)
						{
							/* We have found all the matches in this file */
							temp = FAIL;
							break;
						}
						ptr = ml_get_pos(complete_pos);
						if (ctrl_x_mode == CTRL_X_WHOLE_LINE)
							temp = STRLEN(ptr);
						else
						{
							tmp_ptr = ptr;
							temp = 0;
							while (*tmp_ptr != NUL && iswordchar(*tmp_ptr++))
								temp++;
						}
						if (add_completion_and_infercase(ptr, temp, NULL,
												  complete_direction) != FAIL)
						{
							temp = OK;
							break;
						}
					}
					p_scs = save_p_scs;
				}
				if (complete_pos != NULL && temp == FAIL)
				{
					int tot;

					tot = count_completions();	/* Total num matches */
					if (curr_match != NULL)
						(void)make_cyclic();
					if (tot > 1)
					{
						sprintf((char *)IObuff,
							"All %d matches have now been found", tot);
						mesg = IObuff;
					}
					else if (tot == 0)
					{
						edit_submode_extra = e_patnotf;
						edit_submode_highl = TRUE;
					}
				}

				/* eat the ESC to avoid leaving insert mode */
				if (got_int)
				{
					(void)vgetc();
					got_int = FALSE;
				}

				/*
				 * When using match from another file, show the file name.
				 */
				if (curr_match != NULL)
					ptr = curr_match->str;
				else			/* back to what has been typed */
					ptr = original_text;

				if (edit_submode_extra == NULL)
				{
					if (curr_match == NULL || curr_match->original)
					{
						edit_submode_extra = (char_u *)"Back at original";
						edit_submode_highl = TRUE;
					}
					else if (first_match != NULL &&
							first_match->next != NULL &&
							(first_match->next == first_match ||
							 first_match->next->original))
					{
						edit_submode_extra = (char_u *)"(the only match)";
						edit_submode_highl = FALSE;
					}
				}

				/*
				 * Use ins_char() to insert the text, it is a bit slower than
				 * ins_str(), but it takes care of replace mode.
				 */
				if (ptr != NULL)
					while (*ptr)
						ins_char(*ptr++);

				started_completion = TRUE;
				need_redraw = TRUE;

				if (mesg != NULL)
				{
					(void)set_highlight('r');
					msg_highlight = TRUE;
					msg(mesg);
					mch_delay(2000L, FALSE);
				}
				if (edit_submode_extra != NULL)
				{
					showmode();
					edit_submode_extra = NULL;
				}

				/*
				 * If there is a file name for the match, overwrite any
				 * previous message, it's more interesting to know where the
				 * match comes from, except when using the dictionary.
				 * Truncate the file name to avoid a wait for return.
				 */
				if (curr_match != NULL && curr_match->fname != NULL &&
							(ctrl_x_mode != CTRL_X_DICTIONARY || mesg == NULL))
				{
					STRCPY(IObuff, "match in file ");
					i = (strsize(curr_match->fname) + 16) - sc_col;
					if (i <= 0)
						i = 0;
					else
						STRCAT(IObuff, "<");
					STRCAT(IObuff, curr_match->fname + i);
					msg(IObuff);
					redraw_cmdline = FALSE;		/* don't overwrite! */
				}

				break;
#endif /* INSERT_EXPAND */

			  case Ctrl('Y'):				/* copy from previous line */
#ifdef INSERT_EXPAND
				if (ctrl_x_mode == CTRL_X_SCROLL)
				{
					scrolldown_clamp();
					updateScreen(VALID);
					break;
				}
#endif /* INSERT_EXPAND */
				lnum = curwin->w_cursor.lnum - 1;
				goto copychar;

			  case Ctrl('E'):				/* copy from next line */
#ifdef INSERT_EXPAND
				if (ctrl_x_mode == CTRL_X_SCROLL)
				{
					scrollup_clamp();
					updateScreen(VALID);
					break;
				}
#endif /* INSERT_EXPAND */
				lnum = curwin->w_cursor.lnum + 1;
copychar:
				if (lnum < 1 || lnum > curbuf->b_ml.ml_line_count)
				{
					beep_flush();
					break;
				}

				/* try to advance to the cursor column */
				temp = 0;
				ptr = ml_get(lnum);
				while ((colnr_t)temp < curwin->w_virtcol && *ptr)
					temp += lbr_chartabsize(ptr++, (colnr_t)temp);

				if ((colnr_t)temp > curwin->w_virtcol)
					--ptr;
				if ((c = *ptr) == NUL)
				{
					beep_flush();
					break;
				}

				/*FALLTHROUGH*/
			  default:
normalchar:
				/*
				 * do some very smart indenting when entering '{' or '}'
				 */
				if (((did_si || can_si_back) && c == '{') ||
														 (can_si && c == '}'))
				{
					FPOS	*pos, old_pos;

						/* for '}' set indent equal to indent of line
						 * containing matching '{'
						 */
					if (c == '}' && (pos = findmatch('{')) != NULL)
					{
						old_pos = curwin->w_cursor;
						/*
						 * If the matching '{' has a ')' immediately before it
						 * (ignoring white-space), then line up with the start
						 * of the line containing the matching '(' if there is
						 * one.  This handles the case where an
						 * "if (..\n..) {" statement continues over multiple
						 * lines -- webb
						 */
						ptr = ml_get(pos->lnum);
						i = pos->col;
						if (i > 0)			/* skip blanks before '{' */
							while (--i > 0 && vim_iswhite(ptr[i]))
								;
						curwin->w_cursor.lnum = pos->lnum;
						curwin->w_cursor.col = i;
						if (ptr[i] == ')' && (pos = findmatch('(')) != NULL)
							curwin->w_cursor = *pos;
						i = get_indent();
						curwin->w_cursor = old_pos;
						set_indent(i, TRUE);
					}
					else if (curwin->w_cursor.col > 0)
					{
						/*
						 * when inserting '{' after "O" reduce indent, but not
						 * more than indent of previous line
						 */
						temp = TRUE;
						if (c == '{' && can_si_back &&
													curwin->w_cursor.lnum > 1)
						{
							old_pos = curwin->w_cursor;
							i = get_indent();
							while (curwin->w_cursor.lnum > 1)
							{
								ptr = skipwhite(
										   ml_get(--(curwin->w_cursor.lnum)));
								/* ignore empty lines and lines starting with
								 * '#'.
								 */
								if (*ptr != '#' && *ptr != NUL)
									break;
							}
							if (get_indent() >= i)
								temp = FALSE;
							curwin->w_cursor = old_pos;
						}
						if (temp)
							shift_line(TRUE, FALSE, 1);
					}
				}
					/* set indent of '#' always to 0 */
				if (curwin->w_cursor.col > 0 && can_si && c == '#')
				{
								/* remember current indent for next line */
					old_indent = get_indent();
					set_indent(0, TRUE);
				}

				if (c == ' ')
				{
#ifdef CINDENT
					if (inindent(0))
						can_cindent = FALSE;
#endif
					if (Insstart_blank_vcol == MAXCOL &&
									   curwin->w_cursor.lnum == Insstart.lnum)
						Insstart_blank_vcol = curwin->w_virtcol;
				}

				if (iswordchar(c) || !echeck_abbr(c))
				{
					insert_special(c, FALSE);
					need_redraw = TRUE;
#ifdef RIGHTLEFT
					revinslegal++;
					revinschars++;
#endif
				}
				break;
		}	/* end of switch (c) */

#ifdef CINDENT
		if (curbuf->b_p_cin && can_cindent
# ifdef INSERT_EXPAND
											&& !ctrl_x_mode
# endif
															   )
		{
force_cindent:
			/*
			 * Indent now if a key was typed that is in 'cinkeys'.
			 */
			if (in_cinkeys(c, ' ', line_is_white))
			{
				stop_arrow();

				/* re-indent the current line */
				fixthisline(get_c_indent);

				/* draw the changes on the screen later */
				need_redraw = TRUE;
			}
		}
#endif /* CINDENT */

	}	/* for (;;) */
}

/*
 * Insert an indent (for <Tab> or CTRL-T) or delete an indent (for CTRL-D).
 * Keep the cursor on the same character.
 * type == INDENT_INC	increase indent (for CTRL-T or <Tab>)
 * type == INDENT_DEC	decrease indent (for CTRL-D)
 * type == INDENT_SET	set indent to "amount"
 * if round is TRUE, round the indent to 'shiftwidth' (only with _INC and _Dec).
 */
	static void
change_indent(type, amount, round)
	int		type;
	int		amount;
	int		round;
{
	int			vcol;
	int			last_vcol;
	int			insstart_less;			/* reduction for Insstart.col */
	int			new_cursor_col;
	int			i;
	char_u		*ptr;
	int			save_p_list;

	/* for the following tricks we don't want list mode */
	save_p_list = curwin->w_p_list;
    if (save_p_list)
    {
        curwin->w_p_list = FALSE;
        curs_columns(FALSE);			/* recompute w_virtcol */
    }
	vcol = curwin->w_virtcol;

	/* determine offset from first non-blank */
	new_cursor_col = curwin->w_cursor.col;
	beginline(TRUE);
	new_cursor_col -= curwin->w_cursor.col;

	insstart_less = curwin->w_cursor.col;

	/*
	 * If the cursor is in the indent, compute how many screen columns the
	 * cursor is to the left of the first non-blank.
	 */
	if (new_cursor_col < 0)
		vcol = get_indent() - vcol;

	/*
	 * Set the new indent.  The cursor will be put on the first non-blank.
	 */
	if (type == INDENT_SET)
		set_indent(amount, TRUE);
	else
		shift_line(type == INDENT_DEC, round, 1);
	insstart_less -= curwin->w_cursor.col;

	/*
	 * Try to put cursor on same character.
	 * If the cursor is at or after the first non-blank in the line,
	 * compute the cursor column relative to the column of the first
	 * non-blank character.
	 * If we are not in insert mode, leave the cursor on the first non-blank.
	 * If the cursor is before the first non-blank, position it relative
	 * to the first non-blank, counted in screen columns.
	 */
	if (new_cursor_col >= 0)
		new_cursor_col += curwin->w_cursor.col;
	else if (!(State & INSERT))
		new_cursor_col = curwin->w_cursor.col;
	else
	{
		/*
		 * Compute the screen column where the cursor should be.
		 */
		vcol = get_indent() - vcol;
		curwin->w_virtcol = (vcol < 0) ? 0 : vcol;

		/*
		 * Advance the cursor until we reach the right screen column.
		 */
		vcol = last_vcol = 0;
		new_cursor_col = -1;
		ptr = ml_get_curline();
		while (vcol <= (int)curwin->w_virtcol)
		{
			last_vcol = vcol;
			++new_cursor_col;
			vcol += lbr_chartabsize(ptr + new_cursor_col, (colnr_t)vcol);
		}
		vcol = last_vcol;

		/*
		 * May need to insert spaces to be able to position the cursor on
		 * the right screen column.
		 */
		if (vcol != (int)curwin->w_virtcol)
		{
			curwin->w_cursor.col = new_cursor_col;
			i = (int)curwin->w_virtcol - vcol;
			ptr = alloc(i + 1);
			if (ptr != NULL)
			{
				new_cursor_col += i;
				ptr[i] = NUL;
				while (--i >= 0)
					ptr[i] = ' ';
				ins_str(ptr);
				vim_free(ptr);
			}
		}

		/*
		 * When changing the indent while the cursor is in it, reset
		 * Insstart_col to 0.
		 */
		insstart_less = Insstart.col;
	}

	curwin->w_p_list = save_p_list;

	if (new_cursor_col <= 0)
		curwin->w_cursor.col = 0;
	else
		curwin->w_cursor.col = new_cursor_col;
	curwin->w_set_curswant = TRUE;

	/*
	 * May have to adjust the start of the insert.
	 */
	if ((State & INSERT) && curwin->w_cursor.lnum == Insstart.lnum &&
															Insstart.col != 0)
	{
		if ((int)Insstart.col <= insstart_less)
			Insstart.col = 0;
		else
			Insstart.col -= insstart_less;
	}
}

#ifdef INSERT_EXPAND
/*
 * Is the character 'c' a valid key to keep us in the current ctrl-x mode?
 * -- webb
 */
	int
is_ctrl_x_key(c)
	int		c;
{
	switch (ctrl_x_mode)
	{
		case 0:				/* Not in any ctrl-x mode */
			break;
		case CTRL_X_NOT_DEFINED_YET:
			if (c == Ctrl('X') || c == Ctrl('Y') || c == Ctrl('E') ||
					c == Ctrl('L') || c == Ctrl('F') || c == Ctrl(']') ||
					c == Ctrl('I') || c == Ctrl('D') || c == Ctrl('P') ||
					c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_SCROLL:
			if (c == Ctrl('Y') || c == Ctrl('E'))
				return TRUE;
			break;
		case CTRL_X_WHOLE_LINE:
			if (c == Ctrl('L') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_FILES:
			if (c == Ctrl('F') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_DICTIONARY:
			if (c == Ctrl('K') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_TAGS:
			if (c == Ctrl(']') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_PATH_PATTERNS:
			if (c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		case CTRL_X_PATH_DEFINES:
			if (c == Ctrl('D') || c == Ctrl('P') || c == Ctrl('N'))
				return TRUE;
			break;
		default:
			emsg(e_internal);
			break;
	}
	return FALSE;
}

/*
 * This is like add_completion(), but if ic and inf are set, then the
 * case of the originally typed text is used, and the case of the completed
 * text is infered, ie this tries to work out what case you probably wanted
 * the rest of the word to be in -- webb
 */
	int
add_completion_and_infercase(str, len, fname, dir)
	char_u	*str;
	int		len;
	char_u	*fname;
	int		dir;
{
	int has_lower = FALSE;
	int was_letter = FALSE;
	int orig_len;
	int idx;

	if (p_ic && curbuf->b_p_inf && len < IOSIZE)
	{
		/* Infer case of completed part -- webb */
		orig_len = STRLEN(original_text);

		/* Use IObuff, str would change text in buffer! */
		STRNCPY(IObuff, str, len);
		IObuff[len] = NUL;

		/* Rule 1: Were any chars converted to lower? */
		for (idx = 0; idx < orig_len; ++idx)
		{
			if (islower(original_text[idx]))
			{
				has_lower = TRUE;
				if (isupper(IObuff[idx]))
				{
					/* Rule 1 is satisfied */
					for (idx = orig_len; idx < len; ++idx)
						IObuff[idx] = TO_LOWER(IObuff[idx]);
					break;
				}
			}
		}

		/*
		 * Rule 2: No lower case, 2nd consecutive letter converted to
		 * upper case.
		 */
		if (!has_lower)
		{
			for (idx = 0; idx < orig_len; ++idx)
			{
				if (was_letter && isupper(original_text[idx]) &&
					islower(IObuff[idx]))
				{
					/* Rule 2 is satisfied */
					for (idx = orig_len; idx < len; ++idx)
						IObuff[idx] = TO_UPPER(IObuff[idx]);
					break;
				}
				was_letter = isalpha(original_text[idx]);
			}
		}

		/* Copy the original case of the part we typed */
		STRNCPY(IObuff, original_text, orig_len);

		return add_completion(IObuff, len, fname, dir);
	}
	return add_completion(str, len, fname, dir);
}

/*
 * Add a match to the list of matches.
 * If the given string is already in the list of completions, then return
 * FAIL, otherwise add it to the list and return OK.  If there is an error,
 * maybe because alloc returns NULL, then RET_ERROR is returned -- webb.
 */
	static int
add_completion(str, len, fname, dir)
	char_u	*str;
	int		len;
	char_u	*fname;
	int		dir;
{
	struct Completion *match;

	mch_breakcheck();
	if (got_int)
		return RET_ERROR;
	if (len < 0)
		len = STRLEN(str);

	/*
	 * If the same match is already present, don't add it.
	 */
	if (first_match != NULL)
	{
		match = first_match;
		do
		{
			if (STRNCMP(match->str, str, (size_t)len) == 0 &&
													   match->str[len] == NUL)
				return FAIL;
			match = match->next;
		} while (match != NULL && match != first_match);
	}

	/*
	 * Allocate a new match structure.
	 * Copy the values to the new match structure.
	 */
	match = (struct Completion *)alloc((unsigned)sizeof(struct Completion));
	if (match == NULL)
		return RET_ERROR;
	if ((match->str = strnsave(str, len)) == NULL)
	{
		vim_free(match);
		return RET_ERROR;
	}
	if (fname != NULL)
		match->fname = strsave(fname);		/* ignore errors */
	else
		match->fname = NULL;
	match->original = FALSE;

	/*
	 * Link the new match structure in the list of matches.
	 */
	if (first_match == NULL)
	{
		first_match = curr_match = match;
		curr_match->next = curr_match->prev = NULL;
	}
	else
	{
		if (dir == FORWARD)
		{
			match->next = NULL;
			match->prev = curr_match;
			curr_match->next = match;
			curr_match = match;
		}
		else	/* BACKWARD */
		{
			match->prev = NULL;
			match->next = curr_match;
			curr_match->prev = match;
			first_match = curr_match = match;
		}
	}

	return OK;
}

/*
 * Make the completion list cyclic.  Add the original text at the end.
 * Return the number of matches (excluding the original).
 */
	static int
make_cyclic()
{
	struct Completion *match, *orig;
	int		count = 0;

	if (first_match != NULL)
	{
		/*
		 * Find the end of the list.
		 */
		match = first_match;
		count = 1;
		while (match->next != NULL)
		{
			match = match->next;
			++count;
		}

		if (original_text != NULL)
		{
			/*
			 * Allocate a new structure for the original text.
			 * Copy the original text to the new structure.
			 * Link it in the list at the end.
			 */
			orig = (struct Completion *)alloc((unsigned)sizeof(
														  struct Completion));
			if (orig != NULL)
			{
				if ((orig->str = strsave(original_text)) == NULL)
					vim_free(orig);
				else
				{
					orig->fname = NULL;
					orig->original = TRUE;
					orig->prev = match;
					match->next = orig;
					match = orig;
					curr_match = orig;
				}
			}
		}
		match->next = first_match;
		first_match->prev = match;
	}
	return count;
}

/*
 * Add any identifiers that match the given pattern to the list of
 * completions.
 */
	static void
complete_dictionaries(pat, dir)
	char_u	*pat;
	int		dir;
{
	struct Completion *save_curr_match = curr_match;
	char_u	*dict = p_dict;
	char_u	*ptr;
	char_u	*buf;
	char_u	*fname;
	int		at_start;
	FILE	*fp;
	struct regexp *prog = NULL;

	if ((buf = alloc(LSIZE)) == NULL)
		return;
	if (curr_match != NULL)
	{
		while (curr_match->next != NULL)
			curr_match = curr_match->next;
	}
	if (*dict != NUL)
	{
		(void)set_highlight('r');
		msg_highlight = TRUE;
		MSG("Please wait, searching dictionaries");
		set_reg_ic(pat);	/* set reg_ic according to p_ic, p_scs and pat */
		reg_magic = p_magic;
		prog = vim_regcomp(pat);
	}
	while (*dict != NUL && prog != NULL && !got_int)
	{
								/* copy one dictionary file name into buf */
		(void)copy_option_part(&dict, buf, LSIZE, ",");

		fp = fopen((char *)buf, "r");		/* open dictionary file */

		if (fp != NULL)
		{
			fname = strsave(buf);			/* keep name of file */
			/*
			 * Read dictionary file line by line.
			 * Check each line for a match.
			 */
			while (!got_int && !vim_fgets(buf, LSIZE, fp))
			{
				ptr = buf;
				at_start = TRUE;
				while (vim_regexec(prog, ptr, at_start))
				{
					at_start = FALSE;
					ptr = prog->startp[0];
					while (iswordchar(*ptr))
						++ptr;
					if (add_completion_and_infercase(prog->startp[0],
								 (int)(ptr - prog->startp[0]), fname, FORWARD)
																 == RET_ERROR)
						break;
				}
				line_breakcheck();
			}
			fclose(fp);
			vim_free(fname);
		}
	}
	vim_free(prog);
	if (save_curr_match != NULL)
		curr_match = save_curr_match;
	else if (dir == BACKWARD)
		curr_match = first_match;
	vim_free(buf);
}

/*
 * Free the list of completions
 */
	static void
free_completions()
{
	struct Completion *match;

	if (first_match == NULL)
		return;
	curr_match = first_match;
	do
	{
		match = curr_match;
		curr_match = curr_match->next;
		vim_free(match->str);
		vim_free(match->fname);
		vim_free(match);
	} while (curr_match != NULL && curr_match != first_match);
	first_match = curr_match = NULL;
}

/*
 * Return the number of items in the Completion list
 */
	static int
count_completions()
{
	struct Completion *match;
	int num = 0;

	if (first_match == NULL)
		return 0;
	match = first_match;
	do
	{
		if (!match->original)		/* original string doesn't count */
			num++;
		match = match->next;
	} while (match != NULL && match != first_match);
	return num;
}
#endif /* INSERT_EXPAND */

/*
 * Next character is interpreted literally.
 * A one, two or three digit decimal number is interpreted as its byte value.
 * If one or two digits are entered, the next character is given to vungetc().
 */
	int
get_literal()
{
	int			 cc;
	int			 nc;
	int			 i;

	if (got_int)
		return Ctrl('C');

#ifdef USE_GUI
	/*
	 * In GUI there is no point inserting the internal code for a special key.
	 * It is more useful to insert the string "<KEY>" instead.  This would
	 * probably be useful in a text window too, but it would not be
	 * vi-compatible (maybe there should be an option for it?) -- webb
	 */
	if (gui.in_use)
		++allow_keys;
#endif
	++no_mapping;			/* don't map the next key hits */
	cc = 0;
	for (i = 0; i < 3; ++i)
	{
		do
			nc = vgetc();
		while (nc == K_IGNORE || nc == K_SCROLLBAR || nc == K_HORIZ_SCROLLBAR);
		if (!(State & CMDLINE))
			add_to_showcmd(nc, FALSE);
		if (IS_SPECIAL(nc) || !isdigit(nc))
			break;
		cc = cc * 10 + nc - '0';
		if (cc > 255)
			cc = 255;			/* limit range to 0-255 */
		nc = 0;
	}
	if (i == 0)		/* no number entered */
	{
		if (nc == K_ZERO)	/* NUL is stored as NL */
		{
			cc = '\n';
			nc = 0;
		}
		else
		{
			cc = nc;
			nc = 0;
		}
	}

	if (cc == 0)		/* NUL is stored as NL */
		cc = '\n';

	--no_mapping;
#ifdef USE_GUI
	if (gui.in_use)
		--allow_keys;
#endif
	if (nc)
		vungetc(nc);
	got_int = FALSE;		/* CTRL-C typed after CTRL-V is not an interrupt */
	return cc;
}

/*
 * Insert character, taking care of special keys and mod_mask
 */
	static void
insert_special(c, allow_modmask)
	int		c;
	int		allow_modmask;
{
	char_u	*p;
	int		len;

	/*
	 * Special function key, translate into "<Key>". Up to the last '>' is
	 * inserted with ins_str(), so as not to replace characters in replace
	 * mode.
	 * Only use mod_mask for special keys, to avoid things like <S-Space>,
	 * unless 'allow_modmask' is TRUE.
	 */
	if (IS_SPECIAL(c) || (mod_mask && allow_modmask))
	{
		p = get_special_key_name(c, mod_mask);
		len = STRLEN(p);
		c = p[len - 1];
		if (len > 2)
		{
			p[len - 1] = NUL;
			ins_str(p);
			AppendToRedobuff(p);
		}
	}
	insertchar(c, FALSE, -1);
}

/*
 * Special characters in this context are those that need processing other
 * than the simple insertion that can be performed here. This includes ESC
 * which terminates the insert, and CR/NL which need special processing to
 * open up a new line. This routine tries to optimize insertions performed by
 * the "redo", "undo" or "put" commands, so it needs to know when it should
 * stop and defer processing to the "normal" mechanism.
 */
#define ISSPECIAL(c)	((c) < ' ' || (c) >= DEL)

	void
insertchar(c, force_formatting, second_indent)
	unsigned	c;
	int			force_formatting;		/* format line regardless of p_fo */
	int			second_indent;			/* indent for second line if >= 0 */
{
	int			haveto_redraw = FALSE;
	int			textwidth;
	colnr_t		leader_len;
	int			first_line = TRUE;
	int			fo_ins_blank;
	int			save_char = NUL;

	stop_arrow();

	/*
	 * find out textwidth to be used:
	 *	if 'textwidth' option is set, use it
	 *	else if 'wrapmargin' option is set, use Columns - 'wrapmargin'
	 *	if invalid value, use 0.
	 *  Set default to window width (maximum 79) for "Q" command.
	 */
	textwidth = curbuf->b_p_tw;
	if (textwidth == 0 && curbuf->b_p_wm)
		textwidth = Columns - curbuf->b_p_wm;
	if (textwidth < 0)
		textwidth = 0;
	if (force_formatting && textwidth == 0)
	{
		textwidth = Columns - 1;
		if (textwidth > 79)
			textwidth = 79;
	}

	fo_ins_blank = has_format_option(FO_INS_BLANK);

	/*
	 * Try to break the line in two or more pieces when:
	 * - Always do this if we have been called to do formatting only.
	 * - Otherwise:
	 *   - Don't do this if inserting a blank
	 *   - Don't do this if an existing character is being replaced.
	 *   - Do this if the cursor is not on the line where insert started
	 *   or - 'formatoptions' doesn't have 'l' or the line was not too long
	 *         before the insert.
	 *      - 'formatoptions' doesn't have 'b' or a blank was inserted at or
	 *        before 'textwidth'
	 */
	if (force_formatting || (!vim_iswhite(c) &&
							 !(State == REPLACE && *ml_get_cursor() != NUL) &&
									(curwin->w_cursor.lnum != Insstart.lnum ||
										  ((!has_format_option(FO_INS_LONG) ||
									Insstart_textlen <= (colnr_t)textwidth) &&
			  (!fo_ins_blank || Insstart_blank_vcol <= (colnr_t)textwidth)))))
	{
		/*
		 * When 'ai' is off we don't want a space under the cursor to be
		 * deleted.  Replace it with an 'x' temporarily.
		 */
		if (!curbuf->b_p_ai && vim_iswhite(gchar_cursor()))
		{
			save_char = gchar_cursor();
			pchar_cursor('x');
		}
		while (textwidth && curwin->w_virtcol >= (colnr_t)textwidth)
		{
			int		startcol;			/* Cursor column at entry */
			int		wantcol;			/* column at textwidth border */
			int		foundcol;			/* column for start of spaces */
			int		end_foundcol = 0;	/* column for start of word */
			colnr_t	len;

			if (!force_formatting && has_format_option(FO_WRAP_COMS))
				fo_do_comments = TRUE;

			/* Don't break until after the comment leader */
			leader_len = get_leader_len(ml_get_curline(), NULL);
			if (!force_formatting && leader_len == 0 &&
												  !has_format_option(FO_WRAP))

			{
				textwidth = 0;
				break;
			}
			if ((startcol = curwin->w_cursor.col) == 0)
				break;
										/* find column of textwidth border */
			coladvance((colnr_t)textwidth);
			wantcol = curwin->w_cursor.col;

			curwin->w_cursor.col = startcol - 1;
			foundcol = 0;
			/*
			 * Find position to break at.
			 * Stop at start of line.
			 * Stop at first entered white when 'formatoptions' has 'v'
			 */
			while (curwin->w_cursor.col > 0 &&
						  ((!fo_ins_blank && !has_format_option(FO_INS_VI)) ||
								 curwin->w_cursor.lnum != Insstart.lnum ||
									 curwin->w_cursor.col >= Insstart.col))
			{
				if (vim_iswhite(gchar_cursor()))
				{
						/* remember position of blank just before text */
					end_foundcol = curwin->w_cursor.col;
					while (curwin->w_cursor.col > 0 &&
												  vim_iswhite(gchar_cursor()))
						--curwin->w_cursor.col;
					if (curwin->w_cursor.col == 0 &&
												  vim_iswhite(gchar_cursor()))
						break;			/* only spaces in front of text */
					/* Don't break until after the comment leader */
					if (curwin->w_cursor.col < leader_len)
						break;
					foundcol = curwin->w_cursor.col + 1;
					if (curwin->w_cursor.col < (colnr_t)wantcol)
						break;
				}
				--curwin->w_cursor.col;
			}

			if (foundcol == 0)			/* no spaces, cannot break line */
			{
				curwin->w_cursor.col = startcol;
				break;
			}

			/*
			 * offset between cursor position and line break is used by
			 * replace stack functions
			 */
			replace_offset = startcol - end_foundcol - 1;

			/*
			 * adjust startcol for spaces that will be deleted and
			 * characters that will remain on top line
			 */
			curwin->w_cursor.col = foundcol;
			while (vim_iswhite(gchar_cursor()))
			{
				++curwin->w_cursor.col;
				--startcol;
			}
			startcol -= foundcol;
			if (startcol < 0)
				startcol = 0;

				/* put cursor after pos. to break line */
			curwin->w_cursor.col = foundcol;

			Opencmd(FORWARD, FALSE, TRUE);

			replace_offset = 0;
			if (second_indent >= 0 && first_line)
				set_indent(second_indent, TRUE);
			first_line = FALSE;

			/*
			 * check if cursor is not past the NUL off the line, cindent may
			 * have added or removed indent.
			 */
			curwin->w_cursor.col += startcol;
			len = STRLEN(ml_get_curline());
			if (curwin->w_cursor.col > len)
				curwin->w_cursor.col = len;

			curs_columns(FALSE);		/* update curwin->w_virtcol */
			haveto_redraw = TRUE;
#ifdef CINDENT
			can_cindent = TRUE;
#endif
		}

		if (save_char)					/* put back space after cursor */
			pchar_cursor(save_char);

		if (c == NUL)					/* formatting only */
			return;
		fo_do_comments = FALSE;
		if (haveto_redraw)
		{
			/*
			 * If the cursor ended up just below the screen we scroll up here
			 * to avoid a redraw of the whole screen in the most common cases.
			 */
 			if (curwin->w_cursor.lnum == curwin->w_botline &&
														!curwin->w_empty_rows)
				win_del_lines(curwin, 0, 1, TRUE, TRUE);
			updateScreen(CURSUPD);
		}
	}
	if (c == NUL)			/* only formatting was wanted */
		return;

	did_ai = FALSE;
	did_si = FALSE;
	can_si = FALSE;
	can_si_back = FALSE;

	/*
	 * If there's any pending input, grab up to INPUT_BUFLEN at once.
	 * This speeds up normal text input considerably.
	 */
#define INPUT_BUFLEN 100
	if (!ISSPECIAL(c) && vpeekc() != NUL && State != REPLACE
#ifdef RIGHTLEFT
																&& !p_ri
#endif
																		)
	{
		char_u			p[INPUT_BUFLEN + 1];
		int 			i;

		p[0] = c;
		i = 1;
		while ((c = vpeekc()) != NUL && !ISSPECIAL(c) && i < INPUT_BUFLEN &&
															(textwidth == 0 ||
			(curwin->w_virtcol += charsize(p[i - 1])) < (colnr_t)textwidth) &&
					!(!no_abbr && !iswordchar(c) && iswordchar(p[i - 1])))
		{
#ifdef RIGHTLEFT
			c = vgetc();
			if (p_hkmap && KeyTyped)
				c = hkmap(c);				/* Hebrew mode mapping */
			p[i++] = c;
#else
 			p[i++] = vgetc();
#endif
		}
			
#ifdef DIGRAPHS
		do_digraph(-1);					/* clear digraphs */
		do_digraph(p[i-1]);				/* may be the start of a digraph */
#endif
		p[i] = '\0';
		ins_str(p);
		AppendToRedobuff(p);
	}
	else
	{
		ins_char(c);
		AppendCharToRedobuff(c);
	}
}

/*
 * start_arrow() is called when an arrow key is used in insert mode.
 * It resembles hitting the <ESC> key.
 */
	static void
start_arrow(end_insert_pos)
	FPOS	*end_insert_pos;
{
	if (!arrow_used)		/* something has been inserted */
	{
		AppendToRedobuff(ESC_STR);
		arrow_used = TRUE;		/* this means we stopped the current insert */
		stop_insert(end_insert_pos);
	}
}

/*
 * stop_arrow() is called before a change is made in insert mode.
 * If an arrow key has been used, start a new insertion.
 */
	static void
stop_arrow()
{
	if (arrow_used)
	{
		(void)u_save_cursor();				/* errors are ignored! */
		Insstart = curwin->w_cursor;	/* new insertion starts here */
		Insstart_textlen = linetabsize(ml_get_curline());
		ResetRedobuff();
		AppendToRedobuff((char_u *)"1i");	/* pretend we start an insertion */
		arrow_used = FALSE;
	}
}

/*
 * do a few things to stop inserting
 */
	static void
stop_insert(end_insert_pos)
	FPOS	*end_insert_pos;		/* where insert ended */
{
	stop_redo_ins();

	/*
	 * save the inserted text for later redo with ^@
	 */
	vim_free(last_insert);
	last_insert = get_inserted();
	last_insert_skip = new_insert_skip;

	/*
	 * If we just did an auto-indent, remove the white space from the end of
	 * the line, and put the cursor back.
	 */
	if (did_ai && !arrow_used)
	{
		if (gchar_cursor() == NUL && curwin->w_cursor.col > 0)
			--curwin->w_cursor.col;
		while (vim_iswhite(gchar_cursor()))
			delchar(TRUE);
		if (gchar_cursor() != NUL)
			++curwin->w_cursor.col;		/* put cursor back on the NUL */
		if (curwin->w_p_list)			/* the deletion is only seen in list
										 * mode */
			updateline();
	}
	did_ai = FALSE;
	did_si = FALSE;
	can_si = FALSE;
	can_si_back = FALSE;

	/* set '[ and '] to the inserted text */
	curbuf->b_op_start = Insstart;
	curbuf->b_op_end = *end_insert_pos;
}

/*
 * Set the last inserted text to a single character.
 * Used for the replace command.
 */
	void
set_last_insert(c)
	int		c;
{
	vim_free(last_insert);
	last_insert = alloc(4);
	if (last_insert != NULL)
	{
		last_insert[0] = Ctrl('V');
		last_insert[1] = c;
		last_insert[2] = ESC;
		last_insert[3] = NUL;
			/* Use the CTRL-V only when not entering a digit */
		last_insert_skip = isdigit(c) ? 1 : 0;
	}
}

/*
 * move cursor to start of line
 * if flag == TRUE move to first non-white
 * if flag == MAYBE then move to first non-white if startofline is set,
 *		otherwise don't move at all.
 */
	void
beginline(flag)
	int			flag;
{
	if (flag == MAYBE && !p_sol)
		coladvance(curwin->w_curswant);
	else
	{
		curwin->w_cursor.col = 0;
		if (flag)
		{
			register char_u *ptr;

			for (ptr = ml_get_curline(); vim_iswhite(*ptr); ++ptr)
				++curwin->w_cursor.col;
		}
		curwin->w_set_curswant = TRUE;
	}
}

/*
 * oneright oneleft cursor_down cursor_up
 *
 * Move one char {right,left,down,up}.
 * Return OK when successful, FAIL when we hit a line of file boundary.
 */

	int
oneright()
{
	char_u *ptr;

	ptr = ml_get_cursor();
	if (*ptr++ == NUL || *ptr == NUL)
		return FAIL;
	curwin->w_set_curswant = TRUE;
	++curwin->w_cursor.col;
	return OK;
}

	int
oneleft()
{
	if (curwin->w_cursor.col == 0)
		return FAIL;
	curwin->w_set_curswant = TRUE;
	--curwin->w_cursor.col;
	return OK;
}

	int
cursor_up(n)
	long n;
{
	if (n != 0 && curwin->w_cursor.lnum == 1)
		return FAIL;
	if (n >= curwin->w_cursor.lnum)
		curwin->w_cursor.lnum = 1;
	else
		curwin->w_cursor.lnum -= n;

	/* try to advance to the column we want to be at */
	coladvance(curwin->w_curswant);

	if (op_type == NOP)
		cursupdate();				/* make sure curwin->w_topline is valid */

	return OK;
}

	int
cursor_down(n)
	long n;
{
	if (n != 0 && curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count)
		return FAIL;
	curwin->w_cursor.lnum += n;
	if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;

	/* try to advance to the column we want to be at */
	coladvance(curwin->w_curswant);

	if (op_type == NOP)
		cursupdate();				/* make sure curwin->w_topline is valid */

	return OK;
}

/*
 * screengo() --
 *
 * move 'dist' lines in direction 'dir', counting lines by *screen*
 * lines rather than lines in the file
 * 'dist' must be positive.
 *
 * return OK if able to move cursor, FAIL otherwise.
 */

	int
screengo(dir, dist)
	int		dir;
	long	dist;
{
	int		linelen = linetabsize(ml_get_curline());
	int		retval = OK;
	int		atend = FALSE;
	int		n;

	op_motion_type = MCHAR;
	op_inclusive = FALSE;

	/*
	 * Instead of sticking at the last character of the line in the file we
	 * try to stick in the last column of the screen
	 */
	if (curwin->w_curswant == MAXCOL)
	{
		atend = TRUE;
		curwin->w_curswant = ((curwin->w_virtcol +
					   (curwin->w_p_nu ? 8 : 0)) / Columns + 1) * Columns - 1;
		if (curwin->w_p_nu && curwin->w_curswant > 8)
			curwin->w_curswant -= 8;
	}
	else
		while (curwin->w_curswant >= (colnr_t)(linelen + Columns))
			curwin->w_curswant -= Columns;

	while (dist--)
	{
		if (dir == BACKWARD)
		{
												/* move back within line */
			if ((long)curwin->w_curswant >= Columns)
				curwin->w_curswant -= Columns;
			else								/* to previous line */
			{
				if (curwin->w_cursor.lnum == 1)
				{
					retval = FAIL;
					break;
				}
				--curwin->w_cursor.lnum;
				linelen = linetabsize(ml_get_curline());
				n = ((linelen + (curwin->w_p_nu ? 8 : 0) - 1) / Columns)
																	* Columns;
				if (curwin->w_p_nu &&
								 (long)curwin->w_curswant >= Columns - 8 && n)
					n -= Columns;
				curwin->w_curswant += n;
			}
		}
		else /* dir == FORWARD */
		{
			n = ((linelen + (curwin->w_p_nu ? 8 : 0) - 1) / Columns) * Columns;
			if (curwin->w_p_nu && n > 8)
				n -= 8;
												/* move forward within line */
			if (curwin->w_curswant < (colnr_t)n)
				curwin->w_curswant += Columns;
			else								/* to next line */
			{
				if (curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count)
				{
					retval = FAIL;
					break;
				}
				curwin->w_cursor.lnum++;
				linelen = linetabsize(ml_get_curline());
				curwin->w_curswant %= Columns;
			}
		}
	}
	coladvance(curwin->w_curswant);
	if (atend)
		curwin->w_curswant = MAXCOL;		/* stick in the last column */
	if (op_type == NOP)
		cursupdate();
	return retval;
}

/*
 * move screen 'count' pages up or down and update screen
 *
 * return FAIL for failure, OK otherwise
 */
	int
onepage(dir, count)
	int		dir;
	long	count;
{
	linenr_t		lp;
	long			n;
	int				off;

	if (curbuf->b_ml.ml_line_count == 1)	/* nothing to do */
		return FAIL;
	for ( ; count > 0; --count)
	{
		/*
		 * It's an error to move a page up when the first line is already on
		 * the screen.  It's an error to move a page down when the last line
		 * is on the screen and the topline is 'scrolloff' lines from the
		 * last line.
		 */
		if (dir == FORWARD
				? ((curwin->w_topline >= curbuf->b_ml.ml_line_count - p_so) &&
						curwin->w_botline > curbuf->b_ml.ml_line_count)
				: (curwin->w_topline == 1))
		{
			beep_flush();
			return FAIL;
		}
		if (dir == FORWARD)
		{
										/* at end of file */
			if (curwin->w_botline > curbuf->b_ml.ml_line_count)
				curwin->w_topline = curbuf->b_ml.ml_line_count;
			else
			{
				/*
				 * When there are three or less lines on the screen, move them
				 * all to above the screen.
				 */
				if (curwin->w_botline - curwin->w_topline <= 3)
					off = 0;
				/*
				 * Make sure at least w_botline gets onto the screen, also
				 * when 'scrolloff' is non-zero and with very long lines.
				 */
				else if (plines(curwin->w_botline) +
						plines(curwin->w_botline - 1) +
						plines(curwin->w_botline - 2) >= curwin->w_height - 2)
					off = 0;
				else
					off = 2;
				curwin->w_topline = curwin->w_botline - off;
				curwin->w_cursor.lnum = curwin->w_topline;
			}
			comp_Botline(curwin);
		}
		else	/* dir == BACKWARDS */
		{
			lp = curwin->w_topline;
			/*
			 * If the first two lines on the screen are not too big, we keep
			 * them on the screen.
			 */
			if ((n = plines(lp)) > curwin->w_height / 2)
				--lp;
			else if (lp < curbuf->b_ml.ml_line_count &&
									n + plines(lp + 1) < curwin->w_height / 2)
				++lp;
			curwin->w_cursor.lnum = lp;
			n = 0;
			while (n <= curwin->w_height && lp >= 1)
			{
				n += plines(lp);
				--lp;
			}
			if (n <= curwin->w_height)				/* at begin of file */
			{
				curwin->w_topline = 1;
				comp_Botline(curwin);
			}
			else if (lp >= curwin->w_topline - 2)	/* very long lines */
			{
				--curwin->w_topline;
				comp_Botline(curwin);
				curwin->w_cursor.lnum = curwin->w_botline - 1;
			}
			else
			{
				curwin->w_topline = lp + 2;
				comp_Botline(curwin);
			}
		}
	}
	cursor_correct();
	beginline(MAYBE);
	/*
	 * Avoid the screen jumping up and down when 'scrolloff' is non-zero.
	 */
	if (dir == FORWARD && curwin->w_cursor.lnum < curwin->w_topline + p_so)
		scroll_cursor_top(1, FALSE);
	updateScreen(VALID);
	return OK;
}

/* #define KEEP_SCREEN_LINE */

	void
halfpage(flag, Prenum)
	int			flag;
	linenr_t	Prenum;
{
	long		scrolled = 0;
	int			i;
	int			n;

	if (Prenum)
		curwin->w_p_scroll = (Prenum > curwin->w_height) ?
												curwin->w_height : Prenum;
	n = (curwin->w_p_scroll <= curwin->w_height) ?
									curwin->w_p_scroll : curwin->w_height;

	if (flag)		/* scroll down */
	{
		while (n > 0 && curwin->w_botline <= curbuf->b_ml.ml_line_count)
		{
			i = plines(curwin->w_topline);
			n -= i;
			if (n < 0 && scrolled)
				break;
			scrolled += i;
			++curwin->w_topline;
			comp_Botline(curwin);		/* compute curwin->w_botline */
#ifndef KEEP_SCREEN_LINE
			if (curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
				++curwin->w_cursor.lnum;
#endif
		}
#ifndef KEEP_SCREEN_LINE
		/*
		 * When hit bottom of the file: move cursor down.
		 */
		if (n > 0)
		{
			curwin->w_cursor.lnum += n;
			if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		}
#else
			/* try to put the cursor in the same screen line */
		while ((curwin->w_cursor.lnum < curwin->w_topline || scrolled > 0)
							 && curwin->w_cursor.lnum < curwin->w_botline - 1)
		{
			scrolled -= plines(curwin->w_cursor.lnum);
			if (scrolled < 0 && curwin->w_cursor.lnum >= curwin->w_topline)
				break;
			++curwin->w_cursor.lnum;
		}
#endif
	}
	else			/* scroll up */
	{
		while (n > 0 && curwin->w_topline > 1)
		{
			i = plines(curwin->w_topline - 1);
			n -= i;
			if (n < 0 && scrolled)
				break;
			scrolled += i;
			--curwin->w_topline;
#ifndef KEEP_SCREEN_LINE
			if (curwin->w_cursor.lnum > 1)
				--curwin->w_cursor.lnum;
#endif
		}
		comp_Botline(curwin);		/* compute curwin->w_botline */
#ifndef KEEP_SCREEN_LINE
		/*
		 * When hit top of the file: move cursor up.
		 */
		if (n > 0)
		{
			if (curwin->w_cursor.lnum > (linenr_t)n)
				curwin->w_cursor.lnum -= n;
			else
				curwin->w_cursor.lnum = 1;
		}
#else
			/* try to put the cursor in the same screen line */
		scrolled += n;		/* move cursor when topline is 1 */
		while (curwin->w_cursor.lnum > curwin->w_topline &&
				 (scrolled > 0 || curwin->w_cursor.lnum >= curwin->w_botline))
		{
			scrolled -= plines(curwin->w_cursor.lnum - 1);
			if (scrolled < 0 && curwin->w_cursor.lnum < curwin->w_botline)
				break;
			--curwin->w_cursor.lnum;
		}
#endif
	}
	cursor_correct();
	beginline(MAYBE);
	updateScreen(VALID);
}

/*
 * Stuff the last inserted text in the read buffer.
 * Last_insert actually is a copy of the redo buffer, so we
 * first have to remove the command.
 */
	int
stuff_inserted(c, count, no_esc)
	int		c;
	long	count;
	int		no_esc;
{
	char_u		*esc_ptr = NULL;
	char_u		*ptr;

	ptr = get_last_insert();
	if (ptr == NULL)
	{
		EMSG(e_noinstext);
		return FAIL;
	}

	if (c)
		stuffcharReadbuff(c);
	if (no_esc && (esc_ptr = (char_u *)vim_strrchr(ptr, 27)) != NULL)
		*esc_ptr = NUL;		/* remove the ESC */

	do
		stuffReadbuff(ptr);
	while (--count > 0);

	if (esc_ptr != NULL)
		*esc_ptr = 27;		/* put the ESC back */

	return OK;
}

	char_u *
get_last_insert()
{
	if (last_insert == NULL)
		return NULL;
	return last_insert + last_insert_skip;
}

/*
 * Check the word in front of the cursor for an abbreviation.
 * Called when the non-id character "c" has been entered.
 * When an abbreviation is recognized it is removed from the text and
 * the replacement string is inserted in typebuf[], followed by "c".
 */
	static int
echeck_abbr(c)
	int c;
{
	if (p_paste || no_abbr)			/* no abbreviations or in paste mode */
		return FALSE;

	return check_abbr(c, ml_get_curline(), curwin->w_cursor.col,
				curwin->w_cursor.lnum == Insstart.lnum ? Insstart.col : 0);
}

/*
 * replace-stack functions
 *
 * When replacing characters the replaced character is remembered
 * for each new character. This is used to re-insert the old text
 * when backspacing.
 *
 * replace_offset is normally 0, in which case replace_push will add a new
 * character at the end of the stack. If replace_offset is not 0, that many
 * characters will be left on the stack above the newly inserted character.
 */

char_u	*replace_stack = NULL;
long	replace_stack_nr = 0;		/* next entry in replace stack */
long	replace_stack_len = 0;		/* max. number of entries */

	void
replace_push(c)
	int		c;		/* character that is replaced (NUL is none) */
{
	char_u	*p;

	if (replace_stack_nr < replace_offset)		/* nothing to do */
		return;
	if (replace_stack_len <= replace_stack_nr)
	{
		replace_stack_len += 50;
		p = lalloc(sizeof(char_u) * replace_stack_len, TRUE);
		if (p == NULL)		/* out of memory */
		{
			replace_stack_len -= 50;
			return;
		}
		if (replace_stack != NULL)
		{
			vim_memmove(p, replace_stack,
								 (size_t)(replace_stack_nr * sizeof(char_u)));
			vim_free(replace_stack);
		}
		replace_stack = p;
	}
	p = replace_stack + replace_stack_nr - replace_offset;
	if (replace_offset)
		vim_memmove(p + 1, p, (size_t)(replace_offset * sizeof(char_u)));
	*p = c;
	++replace_stack_nr;
}

/*
 * pop one item from the replace stack
 * return -1 if stack empty
 * return 0 if no character was replaced
 * return replaced character otherwise
 */
	int
replace_pop()
{
	if (replace_stack_nr == 0)
		return -1;
	return (int)replace_stack[--replace_stack_nr];
}

/*
 * make the replace stack empty
 * (called when exiting replace mode)
 */
	void
replace_flush()
{
	vim_free(replace_stack);
	replace_stack = NULL;
	replace_stack_len = 0;
	replace_stack_nr = 0;
}

#if defined(LISPINDENT) || defined(CINDENT)
/*
 * Re-indent the current line, based on the current contents of it and the
 * surrounding lines. Fixing the cursor position seems really easy -- I'm very
 * confused what all the part that handles Control-T is doing that I'm not.
 * "get_the_indent" should be get_c_indent or get_lisp_indent.
 */

	void
fixthisline(get_the_indent)
	int (*get_the_indent) __ARGS((void));
{
	change_indent(INDENT_SET, get_the_indent(), FALSE);
	if (linewhite(curwin->w_cursor.lnum))
		did_ai = TRUE;		/* delete the indent if the line stays empty */
}
#endif /* defined(LISPINDENT) || defined(CINDENT) */

#ifdef CINDENT
/*
 * return TRUE if 'cinkeys' contains the key "keytyped",
 * when == '*':		Only if key is preceded with '*'	(indent before insert)
 * when == '!':		Only if key is prededed with '!'	(don't insert)
 * when == ' ':		Only if key is not preceded with '*'(indent afterwards)
 *
 * If line_is_empty is TRUE accept keys with '0' before them.
 */
	int
in_cinkeys(keytyped, when, line_is_empty)
	int			keytyped;
	int			when;
	int			line_is_empty;
{
	char_u	*look;
	int		try_match;
	char_u	*p;

	for (look = curbuf->b_p_cink; *look; )
	{
		/*
		 * Find out if we want to try a match with this key, depending on
		 * 'when' and a '*' or '!' before the key.
		 */
		switch (when)
		{
			case '*': try_match = (*look == '*'); break;
			case '!': try_match = (*look == '!'); break;
			 default: try_match = (*look != '*'); break;
		}
		if (*look == '*' || *look == '!')
			++look;

		/*
		 * If there is a '0', only accept a match if the line is empty.
		 */
		if (*look == '0')
		{
			if (!line_is_empty)
				try_match = FALSE;
			++look;
		}

		/*
		 * does it look like a control character?
		 */
		if (*look == '^' && look[1] >= '@' && look[1] <= '_')
		{
			if (try_match && keytyped == Ctrl(look[1]))
				return TRUE;
			look += 2;
		}
		/*
		 * 'o' means "o" command, open forward.
		 * 'O' means "O" command, open backward.
		 */
		else if (*look == 'o')
		{
			if (try_match && keytyped == KEY_OPEN_FORW)
				return TRUE;
			++look;
		}
		else if (*look == 'O')
		{
			if (try_match && keytyped == KEY_OPEN_BACK)
				return TRUE;
			++look;
		}

		/*
		 * 'e' means to check for "else" at start of line and just before the
		 * cursor.
		 */
		else if (*look == 'e')
		{
			if (try_match && keytyped == 'e' && curwin->w_cursor.col >= 4)
			{
				p = ml_get_curline();
				if (skipwhite(p) == p + curwin->w_cursor.col - 4 &&
						STRNCMP(p + curwin->w_cursor.col - 4, "else", 4) == 0)
					return TRUE;
			}
			++look;
		}

		/*
		 * ':' only causes an indent if it is at the end of a label or case
		 * statement.
		 */
		else if (*look == ':')
		{
			if (try_match && keytyped == ':')
			{
				p = ml_get_curline();
				if (iscase(p) || islabel(30))
					return TRUE;
			}
			++look;
		}


		/*
		 * Is it a key in <>, maybe?
		 */
		else if (*look == '<')
		{
			if (try_match)
			{
				/*
				 * make up some named keys <o>, <O>, <e>, <0>, <>>, <<>, <*>
				 * and <!> so that people can re-indent on o, O, e, 0, <, >, *
				 * and ! keys if they really really want to.
				 */
				if (vim_strchr((char_u *)"<>!*oOe0", look[1]) != NULL &&
														  keytyped == look[1])
					return TRUE;

				if (keytyped == get_special_key_code(look + 1))
					return TRUE;
			}
			while (*look && *look != '>')
				look++;
			while (*look == '>')
				look++;
		}

		/*
		 * ok, it's a boring generic character.
		 */
		else
		{
			if (try_match && *look == keytyped)
				return TRUE;
			++look;
		}

		/*
		 * Skip over ", ".
		 */
		look = skip_to_option_part(look);
	}
	return FALSE;
}
#endif /* CINDENT */

#if defined(RIGHTLEFT) || defined(PROTO)
/*
 * Map Hebrew keyboard when in hkmap mode.
 */
	int
hkmap(c)
	int c;
{
	switch(c)
	{
		case '`':	return ';';
		case '/':	return '.';
		case '\'':	return ',';
		case 'q':	return '/';
		case 'w':	return '\'';

		/* Hebrew letters - set offset from 'a' */
		case ',':	c = '{'; break;
		case '.':	c = 'v'; break;
		case ';':	c = 't'; break;
		default: {
					static char str[] = "zqbcxlsjphmkwonu ydafe rig";

					if (c < 'a' || c > 'z')
						return c;
					c = str[c - 'a'];
					break;
				}
		}

	return c - 'a' + p_aleph;
}
#endif

/*	$OpenBSD: normal.c,v 1.3 1996/09/22 01:18:06 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Contains the main routine for processing characters in command mode.
 * Communicates closely with the code in ops.c to handle the operators.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

#undef EXTERN
#undef INIT
#define EXTERN
#define INIT(x) x
#include "ops.h"

/*
 * Generally speaking, every command in normal() should either clear any
 * pending operator (with clearop()), or set the motion type variable.
 */

/*
 * If a count is given before the operator, it is saved in opnum.
 */
static linenr_t	opnum = 0;
static linenr_t	Prenum; 		/* The (optional) number before a command. */
static int		prechar = NUL;	/* prepended command char */
/*
 * The visual area is remembered for reselection.
 */
static int		resel_VIsual_mode = NUL;	/* 'v', 'V', or Ctrl-V */
static linenr_t	resel_VIsual_line_count;		/* number of lines */
static colnr_t	resel_VIsual_col;			/* number of cols or end column */

#ifdef USE_MOUSE
static void		find_start_of_word __ARGS((FPOS *));
static void		find_end_of_word __ARGS((FPOS *));
static int		get_mouse_class __ARGS((int));
#endif
static void		prep_redo __ARGS((long, int, int, int, int));
static int		checkclearop __ARGS((void));
static int		checkclearopq __ARGS((void));
static void		clearop __ARGS((void));
static void		clearopbeep __ARGS((void));
static void		del_from_showcmd __ARGS((int));
static void		do_gd __ARGS((int nchar));

/*
 * normal
 *
 * Execute a command in normal mode.
 *
 * This is basically a big switch with the cases arranged in rough categories
 * in the following order:
 *
 *	  0. Macros (q, @)
 *	  1. Screen positioning commands (^U, ^D, ^F, ^B, ^E, ^Y, z)
 *	  2. Control commands (:, <help>, ^L, ^G, ^^, ZZ, *, ^], ^T)
 *	  3. Cursor motions (G, H, M, L, l, K_RIGHT,  , h, K_LEFT, ^H, k, K_UP,
 *	     ^P, +, CR, LF, j, K_DOWN, ^N, _, |, B, b, W, w, E, e, $, ^, 0)
 *	  4. Searches (?, /, n, N, T, t, F, f, ,, ;, ], [, %, (, ), {, })
 *	  5. Edits (., u, K_UNDO, ^R, U, r, J, p, P, ^A, ^S)
 *	  6. Inserts (A, a, I, i, o, O, R)
 *	  7. Operators (~, d, c, y, >, <, !, =, Q)
 *	  8. Abbreviations (x, X, D, C, s, S, Y, &)
 *	  9. Marks (m, ', `, ^O, ^I)
 *	 10. Buffer setting (")
 *	 11. Visual (v, V, ^V)
 *   12. Suspend (^Z)
 *   13. Window commands (^W)
 *   14. extended commands (starting with 'g')
 *   15. mouse click
 *   16. scrollbar movement
 *   17. The end (ESC)
 */

	void
normal()
{
	register int	c;
	long 			n = 0;					/* init for GCC */
	int				flag = FALSE;
	int				flag2 = FALSE;
	int 			type = 0;				/* type of operation */
	int 			dir = FORWARD;			/* search direction */
	int				nchar = NUL;			/* next command char */
	int				finish_op;
	linenr_t		Prenum1;
	char_u			*searchbuff = NULL;		/* buffer for search string */
	FPOS			*pos = NULL;			/* init for gcc */
	char_u			*ptr = NULL;
	int				command_busy = FALSE;
	int				ctrl_w = FALSE;			/* got CTRL-W command */
	int				old_col = 0;
	int				dont_adjust_op_end = FALSE;
	static int		search_dont_set_mark = FALSE;	/* for "*" and "#" */
	FPOS			old_pos;				/* cursor position before command */

	Prenum = 0;
	/*
	 * If there is an operator pending, then the command we take this time
	 * will terminate it. Finish_op tells us to finish the operation before
	 * returning this time (unless the operation was cancelled).
	 */
	finish_op = (op_type != NOP);

	if (!finish_op && !yankbuffer)
		opnum = 0;

	State = NORMAL_BUSY;
	c = vgetc();
#ifdef HAVE_LANGMAP
	LANGMAP_ADJUST(c, TRUE);
#endif
	if (c == NUL)
		c = K_ZERO;
	(void)add_to_showcmd(c, FALSE);

getcount:
	/* Pick up any leading digits and compute 'Prenum' */
	while ((c >= '1' && c <= '9') || (Prenum != 0 && (c == K_DEL || c == '0')))
	{
		if (c == K_DEL)
		{
			Prenum /= 10;
			del_from_showcmd(4);		/* delete the digit and ~@% */
		}
		else
			Prenum = Prenum * 10 + (c - '0');
		if (Prenum < 0)			/* got too large! */
			Prenum = 999999999;
		if (ctrl_w)
		{
			++no_mapping;
			++allow_keys;				/* no mapping for nchar, but keys */
		}
		c = vgetc();
#ifdef HAVE_LANGMAP
		LANGMAP_ADJUST(c, TRUE);
#endif
		if (ctrl_w)
		{
			--no_mapping;
			--allow_keys;
		}
		(void)add_to_showcmd(c, FALSE);
	}

/*
 * If we got CTRL-W there may be a/another count
 */
	if (c == Ctrl('W') && !ctrl_w && op_type == NOP)
	{
		ctrl_w = TRUE;
		opnum = Prenum;						/* remember first count */
		Prenum = 0;
		++no_mapping;
		++allow_keys;						/* no mapping for nchar, but keys */
		c = vgetc();						/* get next character */
#ifdef HAVE_LANGMAP
		LANGMAP_ADJUST(c, TRUE);
#endif
		--no_mapping;
		--allow_keys;
		(void)add_to_showcmd(c, FALSE);
		goto getcount;						/* jump back */
	}

	/*
	 * If we're in the middle of an operator (including after entering a yank
	 * buffer with ") AND we had a count before the
	 * operator, then that count overrides the current value of Prenum. What
	 * this means effectively, is that commands like "3dw" get turned into
	 * "d3w" which makes things fall into place pretty neatly.
	 * If you give a count before AND after the operator, they are multiplied.
	 */
	if (opnum != 0)
	{
			if (Prenum)
				Prenum *= opnum;
			else
				Prenum = opnum;
			opnum = 0;
	}

	Prenum1 = (Prenum == 0 ? 1 : Prenum);		/* Prenum often defaults to 1 */

	/*
	 * Get an additional character if we need one.
	 * For CTRL-W we already got it when looking for a count.
	 */
	if (ctrl_w)
	{
		nchar = c;
		c = Ctrl('W');
	}
	else if ((op_type == NOP && vim_strchr((char_u *)"@zm\"", c) != NULL) ||
			(op_type == NOP && !VIsual_active &&
				 vim_strchr((char_u *)"rZ", c) != NULL) ||
			vim_strchr((char_u *)"tTfF[]g'`", c) != NULL ||
			(c == 'q' && !Recording && !Exec_reg))
	{
		++no_mapping;
		++allow_keys;			/* no mapping for nchar, but allow key codes */
		nchar = vgetc();
#ifdef HAVE_LANGMAP
		/* adjust chars > 127: tTfFr should leave lang of nchar unchanged! */
		LANGMAP_ADJUST(nchar, vim_strchr((char_u *)"tTfFr", c) == NULL);
#endif
#ifdef RIGHTLEFT
		if (p_hkmap && strchr("tTfFr", c) && KeyTyped) 	/* Hebrew mapped char */
			nchar = hkmap(nchar);
#endif
		--no_mapping;
		--allow_keys;
		(void)add_to_showcmd(nchar, FALSE);
	}
	if (p_sc)
		flushbuf();				/* flush the showcmd characters onto the
								 * screen so we can see them while the command
								 * is being executed
								 */

	State = NORMAL;
	if (nchar == ESC)
	{
		clearop();
		goto normal_end;
	}
	msg_didout = FALSE;		/* don't scroll screen up for normal command */
	msg_col = 0;
	old_pos = curwin->w_cursor;			/* remember where cursor was */

#ifdef RIGHTLEFT
	if (curwin->w_p_rl && KeyTyped)		/* invert horizontal operations */
		switch (c)
		{
			case 'l':       c = 'h'; break;
			case K_RIGHT:	c = K_LEFT; break;
			case 'h':		c = 'l'; break;
			case K_LEFT:	c = K_RIGHT; break;
			case '>':		c = '<'; break;
			case '<':		c = '>'; break;
		}
#endif
	switch (c)
	{

/*
 * 0: Macros
 */
	  case 'q': 		/* (stop) recording into a named register */
		if (checkclearop())
			break;
						/* command is ignored while executing a register */
		if (!Exec_reg && do_record(nchar) == FAIL)
			clearopbeep();
		break;

	 case '@':			/* execute a named buffer */
		if (checkclearop())
			break;
		while (Prenum1--)
		{
			if (do_execbuf(nchar, FALSE, FALSE) == FAIL)
			{
				clearopbeep();
				break;
			}
		}
		break;

/*
 * 1: Screen positioning commands
 */
	  case Ctrl('D'):
		flag = TRUE;

	  case Ctrl('U'):
		if ((c == Ctrl('U') && curwin->w_cursor.lnum == 1) ||
			(c == Ctrl('D') && curwin->w_cursor.lnum ==
												  curbuf->b_ml.ml_line_count))
				clearopbeep();
		else
		{
			if (checkclearop())
				break;
			halfpage(flag, Prenum);
		}
		break;

	  case Ctrl('B'):
	  case K_S_UP:
	  case K_PAGEUP:
	  case K_KPAGEUP:
		dir = BACKWARD;

	  case Ctrl('F'):
	  case K_S_DOWN:
	  case K_PAGEDOWN:
	  case K_KPAGEDOWN:
		if (checkclearop())
			break;
		(void)onepage(dir, Prenum1);
		break;

	  case Ctrl('E'):
		if (checkclearop())
			break;
		scrollup(Prenum1);
		if (p_so)
			cursor_correct();
		/* We may have moved to another line -- webb */
		coladvance(curwin->w_curswant);
		cursupdate();
		updateScreen(VALID);
		break;

	  case Ctrl('Y'):
		if (checkclearop())
			break;
		scrolldown(Prenum1);
		if (p_so)
			cursor_correct();
		/* We may have moved to another line -- webb */
		coladvance(curwin->w_curswant);
		updateScreen(VALID);
		break;

	  case 'z':
		if (checkclearop())
			break;
		if (nchar < 0x100 && isdigit(nchar))
		{
			Prenum = nchar - '0';
			for (;;)
			{
				++no_mapping;
				++allow_keys;	/* no mapping for nchar, but allow key codes */
				nchar = vgetc();
#ifdef HAVE_LANGMAP
				LANGMAP_ADJUST(c, TRUE);
#endif
				--no_mapping;
				--allow_keys;
				(void)add_to_showcmd(nchar, FALSE);
				if (c == K_DEL)
					Prenum /= 10;
				else if (nchar < 0x100 && isdigit(nchar))
					Prenum = Prenum * 10 + (nchar - '0');
				else if (nchar == CR)
				{
					win_setheight((int)Prenum);
					break;
				}
				else if (nchar == 'l' || nchar == 'h' ||
										  nchar == K_LEFT || nchar == K_RIGHT)
				{
					Prenum1 = Prenum ? Prenum : 1;
					goto dozet;
				}
				else
				{
					clearopbeep();
					break;
				}
			}
			op_type = NOP;
			break;
		}
dozet:
		/*
		 * If line number given, set cursor, except for "zh", "zl", "ze" and
		 * "zs"
		 */
		if (vim_strchr((char_u *)"hles", nchar) == NULL &&
										nchar != K_LEFT && nchar != K_RIGHT &&
									Prenum && Prenum != curwin->w_cursor.lnum)
		{
			setpcmark();
			if (Prenum > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			else
				curwin->w_cursor.lnum = Prenum;
		}
		switch (nchar)
		{
		  case NL:				/* put curwin->w_cursor at top of screen */
		  case CR:
			beginline(TRUE);
			/* FALLTHROUGH */
		  case 't':
			scroll_cursor_top(0, TRUE);
			break;

		  case '.': 			/* put curwin->w_cursor in middle of screen */
			beginline(TRUE);
			/* FALLTHROUGH */
		  case 'z':
			scroll_cursor_halfway(TRUE);
			break;

		  case '-': 			/* put curwin->w_cursor at bottom of screen */
			beginline(TRUE);
			/* FALLTHROUGH */
		  case 'b':
			scroll_cursor_bot(0, TRUE);
			break;

			/* "zh" - scroll screen to the right */
		  case 'h':
		  case K_LEFT:
			if (!curwin->w_p_wrap)
			{
				colnr_t		s, e;

				if ((colnr_t)Prenum1 > curwin->w_leftcol)
					curwin->w_leftcol = 0;
				else
					curwin->w_leftcol -= (colnr_t)Prenum1;
				n = curwin->w_leftcol + Columns -
					(curwin->w_p_nu ? 8 : 0) - 1;
				if (curwin->w_virtcol > (colnr_t)n)
					coladvance((colnr_t)n);

				getvcol(curwin, &curwin->w_cursor, &s, NULL, &e);
				if (e > (colnr_t)n)
					coladvance(s - 1);
				redraw_later(NOT_VALID);
			}
			break;

			/* "zl" - scroll screen to the left */
		  case 'l':
		  case K_RIGHT:
			if (!curwin->w_p_wrap)
			{
				colnr_t		s, e;

				/* scroll the window left */
				curwin->w_leftcol += (colnr_t)Prenum1;

				/* If the cursor has moved off the screen, put it at the
				 * first char on the screen */
				if (curwin->w_leftcol > curwin->w_virtcol)
					(void)coladvance(curwin->w_leftcol);

				/* If the start of the character under the cursor is not
				 * on the screen, advance the cursor one more char.  If
				 * this fails (last char of the line) adjust the
				 * scrolling. */
				getvcol(curwin, &curwin->w_cursor, &s, NULL, &e);
				if (s < curwin->w_leftcol)
					if (coladvance(e + 1) == FAIL)
						curwin->w_leftcol = s;

				redraw_later(NOT_VALID);
			}
			break;

			/* "zs" - scroll screen, cursor at the start */
		  case 's':
			if (!curwin->w_p_wrap)
			{
				colnr_t		s;

				getvcol(curwin, &curwin->w_cursor, &s, NULL, NULL);
				curwin->w_leftcol = s;
				redraw_later(NOT_VALID);
			}
			break;

			/* "ze" - scroll screen, cursor at the end */
		  case 'e':
			if (!curwin->w_p_wrap)
			{
				colnr_t		e;

				getvcol(curwin, &curwin->w_cursor, NULL, NULL, &e);
				if ((long)e < Columns)
					curwin->w_leftcol = 0;
				else
					curwin->w_leftcol = e - Columns + 1;
				redraw_later(NOT_VALID);
			}
			break;

		  case Ctrl('S'):	/* ignore CTRL-S and CTRL-Q to avoid problems */
		  case Ctrl('Q'):	/* with terminals that use xon/xoff */
		  	break;

		  default:
			clearopbeep();
		}
		updateScreen(VALID);
		break;

/*
 *	  2: Control commands
 */
	  case ':':
	    if (VIsual_active)
			goto dooperator;
		if (checkclearop())
			break;
		/*
		 * translate "count:" into ":.,.+(count - 1)"
		 */
		if (Prenum)
		{
			stuffReadbuff((char_u *)".");
			if (Prenum > 1)
			{
				stuffReadbuff((char_u *)",.+");
				stuffnumReadbuff((long)Prenum - 1L);
			}
		}
		do_cmdline(NULL, FALSE, FALSE);
		break;

	  case 'Q':
		do_exmode();
		updateScreen(CLEAR);
		break;

	  case K_HELP:
	  case K_F1:
		if (checkclearopq())
			break;
		do_help((char_u *)"");
		break;

	  case Ctrl('L'):
		if (checkclearop())
			break;
		updateScreen(CLEAR);
		break;

	  case Ctrl('G'):
		if (checkclearop())
			break;
			/* print full name if count given or :cd used */
		fileinfo(did_cd | (int)Prenum, FALSE, FALSE);
		break;

	  case K_CCIRCM:			/* CTRL-^, short for ":e #" */
		if (checkclearopq())
			break;
		(void)buflist_getfile((int)Prenum, (linenr_t)0,
												GETF_SETMARK|GETF_ALT, FALSE);
		break;

		/*
		 * "ZZ": write if changed, and exit window
		 * "ZQ": quit window (Elvis compatible)
		 */
	  case 'Z':
		if (checkclearopq())
			break;
		if (nchar == 'Z')
			stuffReadbuff((char_u *)":x\n");
		else if (nchar == 'Q')
			stuffReadbuff((char_u *)":q!\n");
		else
			clearopbeep();
		break;

	  case Ctrl(']'):			/* :ta to current identifier */
	  case 'K':					/* run program for current identifier */
		if (VIsual_active)		/* :ta to visual highlighted text */
		{
			if (VIsual.lnum != curwin->w_cursor.lnum)
			{
				clearopbeep();
				break;
			}
			if (lt(curwin->w_cursor, VIsual))
			{
				ptr = ml_get_pos(&curwin->w_cursor);
				n = VIsual.col - curwin->w_cursor.col + 1;
			}
			else
			{
				ptr = ml_get_pos(&VIsual);
				n = curwin->w_cursor.col - VIsual.col + 1;
			}
			end_visual_mode();
			++RedrawingDisabled;
			update_curbuf(NOT_VALID);		/* update the inversion later */
			--RedrawingDisabled;
		}
		if (checkclearopq())
			break;
		/*FALLTHROUGH*/

	  case 163:					/* the pound sign, '#' for English keyboards */
		if (c == 163)
	  		c = '#';
		/*FALLTHROUGH*/

	  case '*': 				/* / to current identifier or string */
	  case '#': 				/* ? to current identifier or string */
search_word:
		if (c == 'g')
			type = nchar;		/* "g*" or "g#" */
		else
			type = c;
		if (ptr == NULL && (n = find_ident_under_cursor(&ptr, (type == '*' ||
					type == '#') ? FIND_IDENT|FIND_STRING : FIND_IDENT)) == 0)
		{
			clearop();
			break;
		}

		if (Prenum)
			stuffnumReadbuff(Prenum);
		switch (type)
		{
			case '*':
				stuffReadbuff((char_u *)"/");
				/* FALLTHROUGH */

			case '#':
				if (type == '#')
					stuffReadbuff((char_u *)"?");

				/*
				 * Put cursor at start of word, makes search skip the word
				 * under the cursor.
				 * Call setpcmark() first, so "*``" puts the cursor back where
				 * it was, and set search_dont_set_mark to avoid doing it
				 * again when searching.
				 */
				setpcmark();
				search_dont_set_mark = TRUE;
				curwin->w_cursor.col = ptr - ml_get_curline();

				if (c != 'g' && iswordchar(*ptr))
					stuffReadbuff((char_u *)"\\<");
				no_smartcase = TRUE;		/* don't use 'smartcase' now */
				break;

			case 'K':
				if (*p_kp == NUL)
					stuffReadbuff((char_u *)":he ");
				else
				{
					stuffReadbuff((char_u *)":! ");
					stuffReadbuff(p_kp);
					stuffReadbuff((char_u *)" ");
				}
				break;
			default:
				if (curbuf->b_help)
					stuffReadbuff((char_u *)":he ");
				else
					stuffReadbuff((char_u *)":ta ");
		}

		/*
		 * Now grab the chars in the identifier
		 */
		while (n--)
		{
				/* put a backslash before \ and some others */
			if (*ptr == '\\' || (!(type == '*' || type == '#') &&
									  vim_strchr(escape_chars, *ptr) != NULL))
				stuffcharReadbuff('\\');
				/* don't interpret the characters as edit commands */
			if (*ptr < ' ' || *ptr > '~')
				stuffcharReadbuff(Ctrl('V'));
			stuffcharReadbuff(*ptr++);
		}

		if (c != 'g' && (type == '*' || type == '#') && iswordchar(ptr[-1]))
			stuffReadbuff((char_u *)"\\>");
		stuffReadbuff((char_u *)"\n");
		break;

	  case Ctrl('T'):		/* backwards in tag stack */
		if (checkclearopq())
			break;
		do_tag((char_u *)"", 2, (int)Prenum1, FALSE);
		break;

/*
 * Cursor motions
 */
	  case 'G':
goto_line:
		op_motion_type = MLINE;
		setpcmark();
		if (Prenum == 0 || Prenum > curbuf->b_ml.ml_line_count)
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		else
			curwin->w_cursor.lnum = Prenum;
		beginline(MAYBE);
		break;

	  case 'H':
	  case 'M':
		if (c == 'M')
		{
			int		used = 0;

			for (n = 0; curwin->w_topline + n < curbuf->b_ml.ml_line_count; ++n)
				if ((used += plines(curwin->w_topline + n)) >=
							(curwin->w_height - curwin->w_empty_rows + 1) / 2)
					break;
			if (n && used > curwin->w_height)
				--n;
		}
		else
			n = Prenum;
		op_motion_type = MLINE;
		setpcmark();
		curwin->w_cursor.lnum = curwin->w_topline + n;
		if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		cursor_correct();		/* correct for 'so' */
		beginline(MAYBE);
		break;

	  case 'L':
		op_motion_type = MLINE;
		setpcmark();
		curwin->w_cursor.lnum = curwin->w_botline - 1;
		if (Prenum >= curwin->w_cursor.lnum)
			curwin->w_cursor.lnum = 1;
		else
			curwin->w_cursor.lnum -= Prenum;
		cursor_correct();		/* correct for 'so' */
		beginline(MAYBE);
		break;

	  case 'l':
	  case K_RIGHT:
	  case ' ':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		n = Prenum1;
		while (n--)
		{
			if (oneright() == FAIL)
			{
					/* space wraps to next line if 'whichwrap' bit 1 set */
					/* 'l' wraps to next line if 'whichwrap' bit 2 set */
					/* CURS_RIGHT wraps to next line if 'whichwrap' bit 3 set */
				if (((c == ' '     && vim_strchr(p_ww, 's') != NULL) ||
					 (c == 'l'     && vim_strchr(p_ww, 'l') != NULL) ||
					 (c == K_RIGHT && vim_strchr(p_ww, '>') != NULL)) &&
					 	curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
				{
					/* When deleting we also count the NL as a character.
					 * Set op_inclusive when last char in the line is
					 * included, move to next line after that */
					if ((op_type == DELETE || op_type == CHANGE) &&
						   !op_inclusive && !lineempty(curwin->w_cursor.lnum))
						op_inclusive = TRUE;
					else
					{
						++curwin->w_cursor.lnum;
						curwin->w_cursor.col = 0;
						curwin->w_set_curswant = TRUE;
						op_inclusive = FALSE;
					}
					continue;
				}
				if (op_type == NOP)
					beep_flush();
				else
				{
					if (lineempty(curwin->w_cursor.lnum))
						clearopbeep();
					else
					{
						op_inclusive = TRUE;
						if (n)
							beep_flush();
					}
				}
				break;
			}
		}
		break;

	  case 'h':
	  case K_LEFT:
	  case K_BS:
	  case Ctrl('H'):
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		n = Prenum1;
		while (n--)
		{
			if (oneleft() == FAIL)
			{
					/* backspace and del wrap to previous line if 'whichwrap'
					 * 	  bit 0 set.
					 * 'h' wraps to previous line if 'whichwrap' bit 2 set.
					 * CURS_LEFT wraps to previous line if 'whichwrap' bit 3
					 * set. */
				if (   (((c == K_BS || c == Ctrl('H'))
									 && vim_strchr(p_ww, 'b') != NULL) ||
						(c == 'h'    && vim_strchr(p_ww, 'h') != NULL) ||
						(c == K_LEFT && vim_strchr(p_ww, '<') != NULL)) &&
							curwin->w_cursor.lnum > 1)
				{
					--(curwin->w_cursor.lnum);
					coladvance(MAXCOL);
					curwin->w_set_curswant = TRUE;

					/* When the NL before the first char has to be deleted we
					 * put the cursor on the NUL after the previous line.
					 * This is a very special case, be careful!
					 * don't adjust op_end now, otherwise it won't work */
					if ((op_type == DELETE || op_type == CHANGE) &&
											!lineempty(curwin->w_cursor.lnum))
					{
						++curwin->w_cursor.col;
						dont_adjust_op_end = TRUE;
					}
					continue;
				}
				else if (op_type != DELETE && op_type != CHANGE)
					beep_flush();
				else if (Prenum1 == 1)
					clearopbeep();
				break;
			}
		}
		break;

	  case '-':
		flag = TRUE;
		/* FALLTHROUGH */

	  case 'k':
	  case K_UP:
	  case Ctrl('P'):
normal_k:
		op_motion_type = MLINE;
		if (cursor_up(Prenum1) == FAIL)
			clearopbeep();
		else if (flag)
			beginline(TRUE);
		break;

	  case '+':
	  case CR:
		flag = TRUE;
		/* FALLTHROUGH */

	  case 'j':
	  case K_DOWN:
	  case Ctrl('N'):
	  case NL:
normal_j:
		op_motion_type = MLINE;
		if (cursor_down(Prenum1) == FAIL)
			clearopbeep();
		else if (flag)
			beginline(TRUE);
		break;

		/*
		 * This is a strange motion command that helps make operators more
		 * logical. It is actually implemented, but not documented in the
		 * real 'vi'. This motion command actually refers to "the current
		 * line". Commands like "dd" and "yy" are really an alternate form of
		 * "d_" and "y_". It does accept a count, so "d3_" works to delete 3
		 * lines.
		 */
	  case '_':
lineop:
		old_col = curwin->w_curswant;
		op_motion_type = MLINE;
		if (cursor_down((long)(Prenum1 - 1)) == FAIL)
			clearopbeep();
		if (op_type == DELETE || op_type == LSHIFT || op_type == RSHIFT)
			beginline(MAYBE);
		else if (op_type != YANK)			/* 'Y' does not move cursor */
			beginline(TRUE);
		break;

	  case K_HOME:
	  case K_KHOME:
		if ((mod_mask & MOD_MASK_CTRL))
			goto goto_line_one;
		Prenum = 1;
		/* FALLTHROUGH */

	  case '|':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		beginline(FALSE);
		if (Prenum > 0)
		{
			coladvance((colnr_t)(Prenum - 1));
			curwin->w_curswant = (colnr_t)(Prenum - 1);
		}
		else
			curwin->w_curswant = 0;
		/* keep curswant at the column where we wanted to go, not where
				we ended; differs is line is too short */
		curwin->w_set_curswant = FALSE;
		break;

		/*
		 * Word Motions
		 */

	  case 'B':
		type = 1;
		/* FALLTHROUGH */

	  case 'b':
	  case K_S_LEFT:
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		curwin->w_set_curswant = TRUE;
		if (bck_word(Prenum1, type, FALSE) == FAIL)
			clearopbeep();
		break;

	  case 'E':
		type = 1;
		/* FALLTHROUGH */

	  case 'e':
		op_inclusive = TRUE;
		goto dowrdcmd;

	  case 'W':
		type = 1;
		/* FALLTHROUGH */

	  case 'w':
	  case K_S_RIGHT:
		op_inclusive = FALSE;
		flag = TRUE;
		/*
		 * This is a little strange. To match what the real vi does, we
		 * effectively map 'cw' to 'ce', and 'cW' to 'cE', provided that we
		 * are not on a space or a TAB. This seems impolite at first, but it's
		 * really more what we mean when we say 'cw'.
		 * Another strangeness: When standing on the end of a word "ce" will
		 * change until the end of the next wordt, but "cw" will change only
		 * one character! This is done by setting type to 2.
		 */
		if (op_type == CHANGE && (n = gchar_cursor()) != ' ' && n != TAB &&
																n != NUL)
		{
			op_inclusive = TRUE;
			flag = FALSE;
			flag2 = TRUE;
		}

dowrdcmd:
		op_motion_type = MCHAR;
		curwin->w_set_curswant = TRUE;
		if (flag)
			n = fwd_word(Prenum1, type, op_type != NOP);
		else
			n = end_word(Prenum1, type, flag2, FALSE);
		if (n == FAIL)
			clearopbeep();
		break;

	  case K_END:
	  case K_KEND:
		if ((mod_mask & MOD_MASK_CTRL))
			goto goto_line;
		/* FALLTHROUGH */

	  case '$':
		op_motion_type = MCHAR;
		op_inclusive = TRUE;
		curwin->w_curswant = MAXCOL;				/* so we stay at the end */
		if (cursor_down((long)(Prenum1 - 1)) == FAIL)
		{
			clearopbeep();
			break;
		}
		break;

	  case '^':
		flag = TRUE;
		/* FALLTHROUGH */

	  case '0':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		beginline(flag);
		break;

/*
 * 4: Searches
 */
	  case '?':
	  case '/':
		if ((searchbuff = getcmdline(c, Prenum1)) == NULL)
		{
			clearop();
			break;
		}
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		curwin->w_set_curswant = TRUE;

		n = do_search(c, searchbuff, Prenum1,
				(search_dont_set_mark ? 0 : SEARCH_MARK) |
				SEARCH_OPT | SEARCH_ECHO | SEARCH_MSG);
		if (n == 0)
			clearop();
		else if (n == 2)
			op_motion_type = MLINE;
		search_dont_set_mark = FALSE;
		break;

	  case 'N':
		flag = SEARCH_REV;

	  case 'n':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		curwin->w_set_curswant = TRUE;
		if (!do_search(0, NULL, Prenum1,
				  SEARCH_MARK | SEARCH_OPT | SEARCH_ECHO | SEARCH_MSG | flag))
			clearop();
		break;

		/*
		 * Character searches
		 */
	  case 'T':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 't':
		type = 1;
		goto docsearch;

	  case 'F':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 'f':
docsearch:
		op_motion_type = MCHAR;
		if (dir == BACKWARD)
			op_inclusive = FALSE;
		else
			op_inclusive = TRUE;
		curwin->w_set_curswant = TRUE;
		if (nchar >= 0x100 || !searchc(nchar, dir, type, Prenum1))
			clearopbeep();
		break;

	  case ',':
		flag = 1;
		/* FALLTHROUGH */

	  case ';':
	    dir = flag;
	    goto docsearch;		/* nchar == NUL, thus repeat previous search */

		/*
		 * section or C function searches
		 */
	  case '[':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case ']':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;

		/*
		 * "[f" or "]f" : Edit file under the cursor (same as "gf")
		 */
		if (nchar == 'f')
			goto gotofile;

		/*
		 * Find the occurence(s) of the identifier or define under cursor
		 * in current and included files or jump to the first occurence.
		 *
		 * 					search 		 list		    jump 
		 * 				  fwd   bwd    fwd   bwd     fwd    bwd
		 * identifier     "]i"  "[i"   "]I"  "[I"   "]^I"  "[^I"
		 * define		  "]d"  "[d"   "]D"  "[D"   "]^D"  "[^D"
		 */
		if (nchar == 'i' || nchar == 'I' || nchar == Ctrl('I') ||
			nchar == 'd' || nchar == 'D' || nchar == Ctrl('D'))
		{
			int			len;

			if ((len = find_ident_under_cursor(&ptr, FIND_IDENT)) == 0)
			{
				clearop();
				break;
			}
			find_pattern_in_path(ptr, len, TRUE,
				Prenum == 0 ? !isupper(nchar) : FALSE,
				((nchar & 0xf) == ('d' & 0xf)) ?  FIND_DEFINE : FIND_ANY,
				Prenum1,
				isupper(nchar) ? ACTION_SHOW_ALL :
							islower(nchar) ? ACTION_SHOW : ACTION_GOTO,
				c == ']' ? curwin->w_cursor.lnum : (linenr_t)1,
				(linenr_t)MAXLNUM);
			curwin->w_set_curswant = TRUE;
			break;
		}

		/*
		 * "[{", "[(", "]}" or "])": go to Nth unclosed '{', '(', '}' or ')'
		 * "[#", "]#": go to start/end of Nth innermost #if..#endif construct.
		 * "[/", "[*", "]/", "]*": go to Nth comment start/end.
		 */
		if ((c == '[' && vim_strchr((char_u *)"{(*/#", nchar) != NULL) ||
		    (c == ']' && vim_strchr((char_u *)"})*/#", nchar) != NULL))
		{
			FPOS new_pos;

			if (nchar == '*')
				nchar = '/';
			new_pos.lnum = 0;
			while (Prenum1--)
			{
				if ((pos = findmatchlimit(nchar,
						   (c == '[') ? FM_BACKWARD : FM_FORWARD, 0)) == NULL)
				{
					if (new_pos.lnum == 0)	/* nothing found */
						clearopbeep();
					else
						pos = &new_pos;		/* use last one found */
					break;
				}
				curwin->w_cursor = *pos;
				new_pos= *pos;
			}
			curwin->w_cursor = old_pos;
			if (pos != NULL)
			{
				setpcmark();
				curwin->w_cursor = *pos;
				curwin->w_set_curswant = TRUE;
			}
			break;
		}

		/*
		 * "[[", "[]", "]]" and "][": move to start or end of function
		 */
		if (nchar == '[' || nchar == ']')
		{
			if (nchar == c)				/* "]]" or "[[" */
				flag = '{';
			else
				flag = '}';				/* "][" or "[]" */

			curwin->w_set_curswant = TRUE;
			/*
			 * Imitate strange vi behaviour: When using "]]" with an operator
			 * we also stop at '}'.
			 */
			if (!findpar(dir, Prenum1, flag,
						   (op_type != NOP && dir == FORWARD && flag == '{')))
				clearopbeep();
			else if (op_type == NOP)
				beginline(TRUE);
			break;
		}

		/*
		 * "[p", "[P", "]P" and "]p": put with indent adjustment
		 */
		if (nchar == 'p' || nchar == 'P')
		{
			if (checkclearopq())
				break;
			prep_redo(Prenum, NUL, c, nchar, NUL);
			do_put((c == ']' && nchar == 'p') ? FORWARD : BACKWARD,
															Prenum1, TRUE);
			break;
		}

#ifdef USE_MOUSE
		/*
		 * [ or ] followed by a middle mouse click: put selected text with
		 * indent adjustment.  Any other button just does as usual.
		 */
		if (nchar >= K_LEFTMOUSE && nchar <= K_RIGHTRELEASE)
		{
			(void)do_mouse(nchar, (c == ']') ? FORWARD : BACKWARD,
															   Prenum1, TRUE);
			break;
		}
#endif /* USE_MOUSE */

		/*
		 * end of '[' and ']': not a valid nchar
		 */
		clearopbeep();
		break;

	  case '%':
		op_inclusive = TRUE;
	    if (Prenum)		/* {cnt}% : goto {cnt} percentage in file */
		{
			if (Prenum > 100)
				clearopbeep();
			else
			{
				op_motion_type = MLINE;
				setpcmark();
						/* round up, so CTRL-G will give same value */
				curwin->w_cursor.lnum = (curbuf->b_ml.ml_line_count *
														   Prenum + 99) / 100;
				beginline(MAYBE);
			}
		}
		else			/* % : go to matching paren */
		{
			op_motion_type = MCHAR;
			if ((pos = findmatch(NUL)) == NULL)
				clearopbeep();
			else
			{
				setpcmark();
				curwin->w_cursor = *pos;
				curwin->w_set_curswant = TRUE;
			}
		}
		break;

	  case '(':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case ')':
		op_motion_type = MCHAR;
		if (c == ')')
			op_inclusive = FALSE;
		else
			op_inclusive = TRUE;
		curwin->w_set_curswant = TRUE;

		if (findsent(dir, Prenum1) == FAIL)
			clearopbeep();
		break;

	  case '{':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case '}':
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
		curwin->w_set_curswant = TRUE;
		if (!findpar(dir, Prenum1, NUL, FALSE))
			clearopbeep();
		break;

/*
 * 5: Edits
 */
	  case '.':				/* redo command */
		if (checkclearopq())
			break;
		/*
		 * if restart_edit is TRUE, the last but one command is repeated
		 * instead of the last command (inserting text). This is used for
		 * CTRL-O <.> in insert mode
		 */
		if (start_redo(Prenum, restart_edit && !arrow_used) == FAIL)
			clearopbeep();
		break;

	  case 'u':				/* undo */
	    if (VIsual_active || op_type == vim_strchr(opchars, 'u') - opchars + 1)
			goto dooperator;
	  case K_UNDO:
		if (checkclearopq())
			break;
		u_undo((int)Prenum1);
		curwin->w_set_curswant = TRUE;
		break;

	  case Ctrl('R'):		/* undo undo */
		if (checkclearopq())
			break;
	  	u_redo((int)Prenum1);
		curwin->w_set_curswant = TRUE;
		break;

	  case 'U':				/* Undo line */
	    if (VIsual_active || op_type == vim_strchr(opchars, 'U') - opchars + 1)
			goto dooperator;
		if (checkclearopq())
			break;
		u_undoline();
		curwin->w_set_curswant = TRUE;
		break;

	  case 'r':
	    if (VIsual_active)
		{
			c = 'c';
			goto dooperator;
		}
		if (checkclearop())
			break;
		ptr = ml_get_cursor();
			/* special key or not enough characters to replace */
		if (nchar >= 0x100 || STRLEN(ptr) < (unsigned)Prenum1)
		{
			clearopbeep();
			break;
		}
		/*
		 * Replacing with a TAB is done by edit(), because it is complicated
		 * when 'expandtab' is set.
		 * Other characters are done below to avoid problems with things like
		 * CTRL-V 048 (for edit() this would be R CTRL-V 0 ESC).
		 */
		if (nchar == '\t' && curbuf->b_p_et)
		{
			prep_redo(Prenum1, NUL, 'r', '\t', NUL);
			stuffnumReadbuff(Prenum1);
			stuffcharReadbuff('R');
			stuffcharReadbuff('\t');
			stuffcharReadbuff(ESC);
			break;
		}

		if (nchar == Ctrl('V'))				/* get another character */
		{
			c = Ctrl('V');
			nchar = get_literal();
		}
		else
			c = NUL;
		prep_redo(Prenum1, NUL, 'r', c, nchar);
		if (u_save_cursor() == FAIL)		/* save line for undo */
			break;
		/*
		 * Replace characters by a newline.
		 * Strange vi behaviour: Only one newline is inserted.
		 * Delete the characters here.
		 * Insert the newline with an insert command, takes care of
		 * autoindent.
		 */
		if (c != Ctrl('V') && (nchar == '\r' || nchar == '\n'))
		{
			while (Prenum1--)					/* delete the characters */
				delchar(FALSE);
				/* replacing the last character of a line is different */
			if (curwin->w_cursor.col > 0 && gchar_cursor() == NUL)
			{
				--curwin->w_cursor.col;
				stuffcharReadbuff('a');
			}
			else
				stuffcharReadbuff('i');
			stuffcharReadbuff('\r');
			stuffcharReadbuff(ESC);
		}
		else
		{
			while (Prenum1--)					/* replace the characters */
			{
				/*
				 * Replace a 'normal' character.
				 * Get ptr again, because u_save and/or showmatch() will have
				 * released the line.  At the same time we let know that the
				 * line will be changed.
				 */
				ptr = ml_get_buf(curbuf, curwin->w_cursor.lnum, TRUE);
				ptr[curwin->w_cursor.col] = nchar;
				if (p_sm && (nchar == ')' || nchar == '}' || nchar == ']'))
					showmatch();
				++curwin->w_cursor.col;
			}
			--curwin->w_cursor.col;		/* cursor on the last replaced char */
		}
		curwin->w_set_curswant = TRUE;
		CHANGED;
		updateline();
		set_last_insert(nchar);
		break;

	  case 'J':
	    if (VIsual_active)		/* join the visual lines */
			goto dooperator;
		if (checkclearop())
			break;
		if (Prenum <= 1)
			Prenum = 2; 			/* default for join is two lines! */
		if (curwin->w_cursor.lnum + Prenum - 1 > curbuf->b_ml.ml_line_count)
		{
			clearopbeep();			/* beyond last line */
			break;
		}

		prep_redo(Prenum, NUL, 'J', NUL, NUL);
		do_do_join(Prenum, TRUE, TRUE);
		break;

	  case 'P':
		dir = BACKWARD;
		/* FALLTHROUGH */

	  case 'p':
		/*
		 * 'P' after an operator or with Visual: Set current block.
		 * 'p' after an operator or with Visual: Set current paragraph.
		 */
	    if (op_type != NOP || VIsual_active)
		{
			if (c == 'P')
			{
				if (current_block('{', Prenum1) == FAIL)
					clearopbeep();
			}
			else
			{
				if (current_par(c, Prenum1) == FAIL)
					clearopbeep();
			}
			curwin->w_set_curswant = TRUE;
		}
		else
		{
			prep_redo(Prenum, NUL, c, NUL, NUL);
			do_put(dir, Prenum1, FALSE);
		}
		break;

	  case Ctrl('A'):			/* add to number */
	  case Ctrl('X'):			/* subtract from number */
		if (checkclearopq())
			break;
		if (do_addsub((int)c, Prenum1) == OK)
			prep_redo(Prenum1, NUL, c, NUL, NUL);
		break;

/*
 * 6: Inserts
 */
	  case 'A':
	  	type = 1;
		/* FALLTHROUGH */

	  case 'a':
		if (op_type != NOP || VIsual_active)
		{
			if (current_word(Prenum1, type) == FAIL)
				clearopbeep();
			curwin->w_set_curswant = TRUE;
		}
		else
		{
			if (c == 'A')
			{
				curwin->w_set_curswant = TRUE;
				while (oneright() == OK)
					;
			}

			/* Works just like an 'i'nsert on the next character. */
			if (u_save_cursor() == OK)
			{
				if (!lineempty(curwin->w_cursor.lnum))
					inc_cursor();
				command_busy = edit(c, FALSE, Prenum1);
			}
		}
		break;

	  case 'I':
		if (checkclearopq())
			break;
		beginline(TRUE);
		/* FALLTHROUGH */

	  case 'i':
	  case K_INS:
insert_command:
		if (checkclearopq())
			break;
		if (u_save_cursor() == OK)
			command_busy = edit(c, FALSE, Prenum1);
		break;

	  case 'o':
	  	if (VIsual_active)	/* switch start and end of visual */
		{
			Prenum = VIsual.lnum;
			VIsual.lnum = curwin->w_cursor.lnum;
			curwin->w_cursor.lnum = Prenum;
			n = VIsual.col;
			VIsual.col = curwin->w_cursor.col;
			curwin->w_cursor.col = (int)n;
			curwin->w_set_curswant = TRUE;
			break;
		}
		if (checkclearop())
			break;
		if (has_format_option(FO_OPEN_COMS))
			fo_do_comments = TRUE;
		if (u_save(curwin->w_cursor.lnum,
								(linenr_t)(curwin->w_cursor.lnum + 1)) == OK &&
						Opencmd(FORWARD, TRUE, FALSE))
			command_busy = edit('o', TRUE, Prenum1);
		fo_do_comments = FALSE;
		break;

	  case 'O':
		if (checkclearopq())
			break;
		if (has_format_option(FO_OPEN_COMS))
			fo_do_comments = TRUE;
		if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
			   curwin->w_cursor.lnum) == OK && Opencmd(BACKWARD, TRUE, FALSE))
			command_busy = edit('O', TRUE, Prenum1);
		fo_do_comments = FALSE;
		break;

	  case 'R':
	    if (VIsual_active)
		{
			c = 'c';
			VIsual_mode = 'V';
			goto dooperator;
		}
		if (checkclearopq())
			break;
		if (u_save_cursor() == OK)
			command_busy = edit('R', FALSE, Prenum1);
		break;

/*
 * 7: Operators
 */
	  case '~': 		/* swap case */
	  /*
	   * if tilde is not an operator and Visual is off: swap case
	   * of a single character
	   */
		if (!p_to && !VIsual_active &&
					op_type != vim_strchr(opchars, '~') - opchars + 1)
		{
			if (checkclearopq())
				break;
			if (lineempty(curwin->w_cursor.lnum))
			{
				clearopbeep();
				break;
			}
			prep_redo(Prenum, NUL, '~', NUL, NUL);

			if (u_save_cursor() == FAIL)
				break;

			for (; Prenum1 > 0; --Prenum1)
			{
				if (gchar_cursor() == NUL)
					break;
				swapchar(&curwin->w_cursor);
				inc_cursor();
			}

			curwin->w_set_curswant = TRUE;
			CHANGED;
			updateline();
			break;
		}
		/*FALLTHROUGH*/

	  case 'd':
	  case 'c':
	  case 'y':
	  case '>':
	  case '<':
	  case '!':
	  case '=':
dooperator:
		n = vim_strchr(opchars, c) - opchars + 1;
		if (n == op_type)		/* double operator works on lines */
			goto lineop;
		if (checkclearop())
			break;
		if (Prenum != 0)
			opnum = Prenum;
		curbuf->b_op_start = curwin->w_cursor;
		op_type = (int)n;
		break;

/*
 * 8: Abbreviations
 */

	 /* when Visual the next commands are operators */
	  case K_DEL:
	  		c = 'x';			/* DEL key behaves like 'x' */
	  case 'S':
	  case 'Y':
	  case 'D':
	  case 'C':
	  case 'x':
	  case 'X':
	  case 's':
		/*
		 * 's' or 'S' with an operator: Operate on sentence or section.
		 */
	    if (op_type != NOP || VIsual_active)
		{
			if (c == 's')		/* sentence */
			{
				if (current_sent(Prenum1) == FAIL)
					clearopbeep();
				curwin->w_set_curswant = TRUE;
				break;
			}
			if (c == 'S')		/* block with () */
			{
				if (current_block('(', Prenum1) == FAIL)
					clearopbeep();
				curwin->w_set_curswant = TRUE;
				break;
			}
		}
	  	if (VIsual_active)
		{
			static char_u trans[] = "YyDdCcxdXd";

											/* uppercase means linewise */
			if (isupper(c) && VIsual_mode != Ctrl('V'))
				VIsual_mode = 'V';
			c = *(vim_strchr(trans, c) + 1);
			goto dooperator;
		}

	  case '&':
		if (checkclearopq())
			break;
		if (Prenum)
			stuffnumReadbuff(Prenum);

		{
				static char_u *(ar[8]) = {(char_u *)"dl", (char_u *)"dh",
										  (char_u *)"d$", (char_u *)"c$",
										  (char_u *)"cl", (char_u *)"cc",
										  (char_u *)"yy", (char_u *)":s\r"};
				static char_u *str = (char_u *)"xXDCsSY&";

				stuffReadbuff(ar[(int)(vim_strchr(str, c) - str)]);
		}
		break;

/*
 * 9: Marks
 */

	  case 'm':
		if (checkclearop())
			break;
		if (setmark(nchar) == FAIL)
			clearopbeep();
		break;

	  case '\'':
		flag = TRUE;
		/* FALLTHROUGH */

	  case '`':
		pos = getmark(nchar, (op_type == NOP));
		if (pos == (FPOS *)-1)	/* jumped to other file */
		{
			if (flag)
				beginline(TRUE);
			break;
		}

cursormark:
		if (check_mark(pos) == FAIL)
			clearop();
		else
		{
			if (c == '\'' || c == '`')
				setpcmark();
			curwin->w_cursor = *pos;
			if (flag)
				beginline(TRUE);
		}
		op_motion_type = flag ? MLINE : MCHAR;
		op_inclusive = FALSE;		/* ignored if not MCHAR */
		curwin->w_set_curswant = TRUE;
		break;

	case Ctrl('O'):			/* goto older pcmark */
		Prenum1 = -Prenum1;
		/* FALLTHROUGH */

	case Ctrl('I'):			/* goto newer pcmark */
		if (checkclearopq())
			break;
		pos = movemark((int)Prenum1);
		if (pos == (FPOS *)-1)	/* jump to other file */
		{
			curwin->w_set_curswant = TRUE;
			break;
		}
		if (pos != NULL)	/* can jump */
			goto cursormark;
		clearopbeep();
		break;

/*
 * 10. Buffer setting
 */
	  case '"':
		if (checkclearop())
			break;
		if (nchar != NUL && is_yank_buffer(nchar, FALSE))
		{
			yankbuffer = nchar;
			opnum = Prenum;		/* remember count before '"' */
		}
		else
			clearopbeep();
		break;

/*
 * 11. Visual
 */
 	  case 'v':
	  case 'V':
	  case Ctrl('V'):
		if (checkclearop())
			break;

			/* change Visual mode */
		if (VIsual_active)
		{
			if (VIsual_mode == c)			/* stop visual mode */
			{
				end_visual_mode();
			}
			else							/* toggle char/block mode */
			{								/*     or char/line mode */
				VIsual_mode = c;
				showmode();
			}
			update_curbuf(NOT_VALID);		/* update the inversion */
		}
			/* start Visual mode */
		else
		{
			VIsual_save = VIsual;			/* keep for "gv" */
			VIsual_mode_save = VIsual_mode;
			start_visual_highlight();
			if (Prenum)						/* use previously selected part */
			{
				if (resel_VIsual_mode == NUL)	/* there is none */
				{
					beep_flush();
					break;
				}
				VIsual = curwin->w_cursor;
				VIsual_active = TRUE;
#ifdef USE_MOUSE
				setmouse();
#endif
				if (p_smd)
					redraw_cmdline = TRUE;		/* show visual mode later */
				/*
				 * For V and ^V, we multiply the number of lines even if there
				 * was only one -- webb
				 */
				if (resel_VIsual_mode != 'v' || resel_VIsual_line_count > 1)
				{
					curwin->w_cursor.lnum += resel_VIsual_line_count * Prenum - 1;
					if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
						curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				}
				VIsual_mode = resel_VIsual_mode;
				if (VIsual_mode == 'v')
				{
					if (resel_VIsual_line_count <= 1)
						curwin->w_cursor.col += resel_VIsual_col * Prenum - 1;
					else
						curwin->w_cursor.col = resel_VIsual_col;
				}
				if (resel_VIsual_col == MAXCOL)
				{
					curwin->w_curswant = MAXCOL;
					coladvance(MAXCOL);
				}
				else if (VIsual_mode == Ctrl('V'))
				{
					curwin->w_curswant = curwin->w_virtcol +
											resel_VIsual_col * Prenum - 1;
					coladvance((colnr_t)curwin->w_curswant);
				}
				else
					curwin->w_set_curswant = TRUE;
				curs_columns(TRUE);			/* recompute w_virtcol */
				update_curbuf(NOT_VALID);	/* show the inversion */
			}
			else
			{
				VIsual = curwin->w_cursor;
				VIsual_mode = c;
				VIsual_active = TRUE;
#ifdef USE_MOUSE
				setmouse();
#endif
				if (p_smd)
					redraw_cmdline = TRUE;	/* show visual mode later */
				updateline();				/* start the inversion */
			}
		}
		break;

/*
 * 12. Suspend
 */

 	case Ctrl('Z'):
		clearop();
		if (VIsual_active)
			end_visual_mode();				/* stop Visual */
		stuffReadbuff((char_u *)":st\r");	/* with autowrite */
		break;

/*
 * 13. Window commands
 */

 	case Ctrl('W'):
		if (checkclearop())
			break;
		do_window(nchar, Prenum);			/* everything is in window.c */
		break;

/*
 *   14. extended commands (starting with 'g')
 */
 	case 'g':
		switch (nchar)
		{
			/*
			 * "gv": reselect the previous visual area
			 */
			case 'v':
				if (checkclearop())
					break;
				if (VIsual_active)
					pos = &VIsual_save;
				else
					pos = &VIsual;
				if (pos->lnum == 0 || pos->lnum > curbuf->b_ml.ml_line_count ||
														 VIsual_end.lnum == 0)
					beep_flush();
				else
				{
					FPOS	tt;
					int		t;

					/* exchange previous and current visual area */
					if (VIsual_active)
					{
						tt = VIsual;
						VIsual = VIsual_save;
						VIsual_save = tt;
						t = VIsual_mode;
						VIsual_mode = VIsual_mode_save;
						VIsual_mode_save = t;
						tt = curwin->w_cursor;
					}
					curwin->w_cursor = VIsual_end;
					if (VIsual_active)
						VIsual_end = tt;
					check_cursor();
					VIsual_active = TRUE;
#ifdef USE_MOUSE
					setmouse();
#endif
					update_curbuf(NOT_VALID);
					showmode();
				}
				break;

			/*
			 * "gj" and "gk" two new funny movement keys -- up and down
			 * movement based on *screen* line rather than *file* line.
			 */
			case 'j':
			case K_DOWN:
				if (!curwin->w_p_wrap)
					goto normal_j;
				if (screengo(FORWARD, Prenum1) == FAIL)
					clearopbeep();
				break;

			case 'k':
			case K_UP:
				if (!curwin->w_p_wrap)
					goto normal_k;
				if (screengo(BACKWARD, Prenum1) == FAIL)
					clearopbeep();
				break;

			/*
			 * "g0", "g^" and "g$": Like "0", "^" and "$" but for screen lines.
			 */
			case '^':
				flag = TRUE;
				/* FALLTHROUGH */

			case '0':
			case K_HOME:
			case K_KHOME:
				op_motion_type = MCHAR;
				op_inclusive = FALSE;
				if (curwin->w_p_wrap)
				{
					n = ((curwin->w_virtcol + (curwin->w_p_nu ? 8 : 0)) /
														   Columns) * Columns;
					if (curwin->w_p_nu && n > 8)
						n -= 8;
				}
				else
					n = curwin->w_leftcol;
				coladvance((colnr_t)n);
				if (flag)
					while (vim_iswhite(gchar_cursor()) && oneright() == OK)
						;
				curwin->w_set_curswant = TRUE;
				break;

			case '$':
			case K_END:
			case K_KEND:
				op_motion_type = MCHAR;
				op_inclusive = TRUE;
				if (curwin->w_p_wrap)
				{
					curwin->w_curswant = MAXCOL;	/* so we stay at the end */
					if (Prenum1 == 1)
					{
						n = ((curwin->w_virtcol + (curwin->w_p_nu ? 8 : 0)) /
												   Columns + 1) * Columns - 1;
						if (curwin->w_p_nu && n > 8)
							n -= 8;
						coladvance((colnr_t)n);
					}
					else if (screengo(FORWARD, Prenum1 - 1) == FAIL)
						clearopbeep();
				}
				else
				{
					n = curwin->w_leftcol + Columns - 1;
					if (curwin->w_p_nu)
						n -= 8;
					coladvance((colnr_t)n);
					curwin->w_set_curswant = TRUE;
				}
				break;

			/*
			 * "g*" and "g#", like "*" and "#" but without using "\<" and "\>"
			 */
			case '*':
			case '#':
				goto search_word;

			/*
			 * ge and gE: go back to end of word
			 */
			case 'e':
			case 'E':
				op_motion_type = MCHAR;
				curwin->w_set_curswant = TRUE;
				op_inclusive = TRUE;
				if (bckend_word(Prenum1, nchar == 'E', FALSE) == FAIL)
					clearopbeep();
				break;

			/*
			 * g CTRL-G: display info about cursor position
			 */
			case Ctrl('G'):
				cursor_pos_info();
				break;

			/*
			 * "gI": Start insert in column 1.
			 */
			case 'I':
				beginline(FALSE);
				goto insert_command;

			/*
			 * "gf": goto file, edit file under cursor
			 * "]f" and "[f": can also be used.
			 */
			case 'f':
gotofile:
				ptr = file_name_at_cursor(FNAME_MESS|FNAME_HYP|FNAME_EXP);
				if (ptr != NULL)
				{
					/* do autowrite if necessary */
					if (curbuf->b_changed && curbuf->b_nwindows <= 1 && !p_hid)
						autowrite(curbuf, FALSE);
					setpcmark();
					(void)do_ecmd(0, ptr, NULL, NULL, (linenr_t)0,
													   p_hid ? ECMD_HIDE : 0);
					vim_free(ptr);
				}
				else
					clearop();
				break;

			/*
			 * "gs": Goto sleep, but keep on checking for CTRL-C
			 */
			case 's':
				while (Prenum1-- && !got_int)
				{
					mch_delay(1000L, TRUE);
					mch_breakcheck();
				}
				break;

			/*
			 * "ga": Display the ascii value of the character under the
			 * cursor.  It is displayed in decimal, hex, and octal. -- webb
			 */
			case 'a':
				do_ascii();
				break;

			/*
			 * "gg": Goto the first line in file.  With a count it goes to
			 * that line number like for G. -- webb
			 */
			case 'g':
goto_line_one:
				if (Prenum == 0)
					Prenum = 1;
				goto goto_line;

			/*
			 * Operater to format text:
			 *   gq		same as 'Q' operator.
			 * Operators to change the case of text:
			 *   g~		Toggle the case of the text.
			 *   gu		Change text to lower case.
			 *   gU		Change text to upper case.
			 *									--webb
			 */
			case 'q':
			case '~':
			case 'u':
			case 'U':
				prechar = c;
				c = nchar;
				goto dooperator;

		/*
		 * "gd": Find first occurence of pattern under the cursor in the
		 *       current function
		 * "gD": idem, but in the current file.
		 */
			case 'd':
			case 'D':
				do_gd(nchar);
				break;

#ifdef USE_MOUSE
			/*
			 * g<*Mouse> : <C-*mouse>
			 */
			case K_MIDDLEMOUSE:
			case K_MIDDLEDRAG:
			case K_MIDDLERELEASE:
			case K_LEFTMOUSE:
			case K_LEFTDRAG:
			case K_LEFTRELEASE:
			case K_RIGHTMOUSE:
			case K_RIGHTDRAG:
			case K_RIGHTRELEASE:
				mod_mask = MOD_MASK_CTRL;
				(void)do_mouse(nchar, BACKWARD, Prenum1, FALSE);
				break;

			case K_IGNORE:
				break;
#endif

			default:
				clearopbeep();
				break;
		}
		break;

/*
 * 15. mouse click
 */
#ifdef USE_MOUSE
	  case K_MIDDLEMOUSE:
	  case K_MIDDLEDRAG:
	  case K_MIDDLERELEASE:
	  case K_LEFTMOUSE:
	  case K_LEFTDRAG:
	  case K_LEFTRELEASE:
	  case K_RIGHTMOUSE:
	  case K_RIGHTDRAG:
	  case K_RIGHTRELEASE:
		(void)do_mouse(c, BACKWARD, Prenum1, FALSE);
		break;

	  case K_IGNORE:
		break;
#endif

#ifdef USE_GUI
/*
 * 16. scrollbar movement
 */
	  case K_SCROLLBAR:
		if (op_type != NOP)
			clearopbeep();

		/* Even if an operator was pending, we still want to scroll */
		gui_do_scroll();
		break;

	  case K_HORIZ_SCROLLBAR:
		if (op_type != NOP)
			clearopbeep();

		/* Even if an operator was pending, we still want to scroll */
		gui_do_horiz_scroll();
		break;
#endif

/*
 * 17. The end
 */
	  case ESC:
		/* Don't drop through and beep if we are canceling a command: */
		if (!VIsual_active && (op_type != NOP ||
											   opnum || Prenum || yankbuffer))
		{
			clearop();					/* don't beep */
			break;
		}
	    if (VIsual_active)
		{
			end_visual_mode();			/* stop Visual */
			update_curbuf(NOT_VALID);
			clearop();					/* don't beep */
			break;
		}
		/* ESC in normal mode: beep, but don't flush buffers */
		clearop();
		vim_beep();
		break;

	  default:					/* not a known command */
		clearopbeep();
		break;

	}	/* end of switch on command character */

/*
 * if we didn't start or finish an operator, reset yankbuffer, unless we
 * need it later.
 */
	if (!finish_op && !op_type && vim_strchr((char_u *)"\"DCYSsXx.", c) == NULL)
		yankbuffer = 0;

/*
 * If an operation is pending, handle it...
 */
	do_pending_operator(c, nchar, finish_op, searchbuff, 
					  &command_busy, old_col, FALSE, dont_adjust_op_end);

	/*
	 * Wait when a message is displayed that will be overwritten by the mode
	 * message.
	 * In Visual mode and with "^O" in Insert mode, a short message will be
	 * overwritten by the mode message.  Wait a bit, until a key is hit.
	 * In Visual mode, it's more important to keep the Visual area updated
	 * than keeping a message (e.g. from a /pat search).
	 * Only do this if the command was typed, not from a mapping.
	 * Also wait a bit after an error message, e.g. for "^O:".
	 * Don't redraw the screen, it would remove the message.
	 */
	if (((p_smd && ((VIsual_active && old_pos.lnum == curwin->w_cursor.lnum &&
			old_pos.col == curwin->w_cursor.col) || restart_edit) &&
			(clear_cmdline || redraw_cmdline) && msg_didany && KeyTyped) ||
			(restart_edit && !VIsual_active && (msg_scroll || emsg_on_display
#ifdef SLEEP_IN_EMSG
										|| need_sleep
#endif
														))) &&
			yankbuffer == 0 && !command_busy && stuff_empty() && op_type == NOP)
	{
		++RedrawingDisabled;
		cursupdate();
		--RedrawingDisabled;
		setcursor();
		flushbuf();
		if (msg_scroll || emsg_on_display
#ifdef SLEEP_IN_EMSG
											|| need_sleep
#endif
															)
			mch_delay(1000L, TRUE);		/* wait at least one second */
		mch_delay(10000L, FALSE);		/* wait up to ten seconds */

		msg_scroll = FALSE;
		emsg_on_display = FALSE;
#ifdef SLEEP_IN_EMSG
		need_sleep = FALSE;
#endif
	}

normal_end:
	if (op_type == NOP && yankbuffer == 0)
		clear_showcmd();

	if (restart_edit && op_type == NOP && !VIsual_active
						 && !command_busy && stuff_empty() && yankbuffer == 0)
		(void)edit(restart_edit, FALSE, 1L);

	if (!search_dont_set_mark)
		checkpcmark();			/* check if we moved since setting pcmark */
	vim_free(searchbuff);

/*
 * Update the other windows for the current buffer if modified has been set in
 * set_Changed() (This should be done more efficiently)
 */
	if (modified)
	{
		WIN		*wp;

        for (wp = firstwin; wp; wp = wp->w_next)
			if (wp != curwin && wp->w_buffer == curbuf)
			{
				cursor_off();
				wp->w_redr_type = NOT_VALID;
				/*
				 * don't do the actual redraw if wait_return() has just been
				 * called and the user typed a ":"
				 */
				if (!skip_redraw)
					win_update(wp);
			}
		modified = FALSE;
	}
}

/*
 * Handle an operator after visual mode or when the movement is finished
 */
	void
do_pending_operator(c, nchar, finish_op, searchbuff, command_busy,
										old_col, gui_yank, dont_adjust_op_end)
	register int	c;
	int				nchar;
	int				finish_op;
	char_u			*searchbuff;
	int				*command_busy;
	int				old_col;
	int				gui_yank;		/* yanking visual area for GUI */
	int				dont_adjust_op_end;
{
	/* The visual area is remembered for redo */
	static int		redo_VIsual_mode = NUL;	/* 'v', 'V', or Ctrl-V */
	static linenr_t	redo_VIsual_line_count;		/* number of lines */
	static colnr_t	redo_VIsual_col;		/* number of cols or end column */
	static long		redo_VIsual_Prenum;		/* Prenum for operator */

	linenr_t		Prenum1 = 1L;
	FPOS			old_cursor;
	int				VIsual_was_active = VIsual_active;
	int				redraw;

#ifdef USE_GUI
	/*
	 * Yank the visual area into the GUI selection register before we operate
	 * on it and lose it forever.  This could call do_pending_operator()
	 * recursively, but that's OK because gui_yank will be TRUE for the
	 * nested call.  Note also that we call gui_copy_selection() and not
	 * gui_auto_select().  This is because even when 'autoselect' is not set,
	 * if we operate on the text, eg by deleting it, then this is considered to
	 * be an explicit request for it to be put in the global cut buffer, so we
	 * always want to do it here. -- webb
	 */
	if (gui.in_use && op_type != NOP && !gui_yank && VIsual_active
														 && !redo_VIsual_busy)
		gui_copy_selection();
#endif
	old_cursor = curwin->w_cursor;

	/*
	 * If an operation is pending, handle it...
	 */
	if ((VIsual_active || finish_op) && op_type != NOP)
	{
		op_is_VIsual = VIsual_active;
		if (op_type != YANK && !VIsual_active)		/* can't redo yank */
		{
			prep_redo(Prenum, prechar, opchars[op_type - 1], c, nchar);
			if (c == '/' || c == '?')				/* was a search */
			{
				/*
				 * If 'cpoptions' does not contain 'r', insert the search
				 * pattern to really repeat the same command.
				 */
				if (vim_strchr(p_cpo, CPO_REDO) == NULL)
					AppendToRedobuff(searchbuff);
				AppendToRedobuff(NL_STR);
			}
		}

		if (redo_VIsual_busy)
		{
			curbuf->b_op_start = curwin->w_cursor;
			curwin->w_cursor.lnum += redo_VIsual_line_count - 1;
			if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			VIsual_mode = redo_VIsual_mode;
			if (VIsual_mode == 'v')
			{
				if (redo_VIsual_line_count <= 1)
					curwin->w_cursor.col += redo_VIsual_col - 1;
				else
					curwin->w_cursor.col = redo_VIsual_col;
			}
			if (redo_VIsual_col == MAXCOL)
			{
				curwin->w_curswant = MAXCOL;
				coladvance(MAXCOL);
			}
			Prenum = redo_VIsual_Prenum;
		}
		else if (VIsual_active)
		{
			curbuf->b_op_start = VIsual;
			VIsual_end = curwin->w_cursor;
			if (VIsual_mode == 'V')
				curbuf->b_op_start.col = 0;
		}

		if (lt(curbuf->b_op_start, curwin->w_cursor))
		{
			curbuf->b_op_end = curwin->w_cursor;
			curwin->w_cursor = curbuf->b_op_start;
		}
		else
		{
			curbuf->b_op_end = curbuf->b_op_start;
			curbuf->b_op_start = curwin->w_cursor;
		}
		op_line_count = curbuf->b_op_end.lnum - curbuf->b_op_start.lnum + 1;

		if (VIsual_active || redo_VIsual_busy)
		{
			if (VIsual_mode == Ctrl('V'))		/* block mode */
			{
				colnr_t		start, end;

				op_block_mode = TRUE;
				getvcol(curwin, &(curbuf->b_op_start),
										  &op_start_vcol, NULL, &op_end_vcol);
				if (!redo_VIsual_busy)
				{
					getvcol(curwin, &(curbuf->b_op_end), &start, NULL, &end);
					if (start < op_start_vcol)
						op_start_vcol = start;
					if (end > op_end_vcol)
						op_end_vcol = end;
				}

				/* if '$' was used, get op_end_vcol from longest line */
				if (curwin->w_curswant == MAXCOL)
				{
					curwin->w_cursor.col = MAXCOL;
					op_end_vcol = 0;
					for (curwin->w_cursor.lnum = curbuf->b_op_start.lnum;
							curwin->w_cursor.lnum <= curbuf->b_op_end.lnum;
							++curwin->w_cursor.lnum)
					{
						getvcol(curwin, &curwin->w_cursor, NULL, NULL, &end);
						if (end > op_end_vcol)
							op_end_vcol = end;
					}
					curwin->w_cursor = curbuf->b_op_start;
				}
				else if (redo_VIsual_busy)
					op_end_vcol = op_start_vcol + redo_VIsual_col - 1;
				coladvance(op_start_vcol);
			}

			if (!redo_VIsual_busy)
			{
				/*
				 * Prepare to reselect and redo Visual: this is based on the
				 * size of the Visual text
				 */
				resel_VIsual_mode = VIsual_mode;
				if (curwin->w_curswant == MAXCOL)
					resel_VIsual_col = MAXCOL;
				else if (VIsual_mode == Ctrl('V'))
					resel_VIsual_col = op_end_vcol - op_start_vcol + 1;
				else if (op_line_count > 1)
					resel_VIsual_col = curbuf->b_op_end.col;
				else
					resel_VIsual_col = curbuf->b_op_end.col -
												curbuf->b_op_start.col + 1;
				resel_VIsual_line_count = op_line_count;
			}
												/* can't redo yank and : */
			if (op_type != YANK && op_type != COLON)
			{
				prep_redo(0L, NUL, 'v', prechar, opchars[op_type - 1]);
				redo_VIsual_mode = resel_VIsual_mode;
				redo_VIsual_col = resel_VIsual_col;
				redo_VIsual_line_count = resel_VIsual_line_count;
				redo_VIsual_Prenum = Prenum;
			}

			/*
			 * Mincl defaults to TRUE.
			 * If op_end is on a NUL (empty line) op_inclusive becomes FALSE
			 * This makes "d}P" and "v}dP" work the same.
			 */
			op_inclusive = TRUE;
			if (VIsual_mode == 'V')
				op_motion_type = MLINE;
			else
			{
				op_motion_type = MCHAR;
				if (*ml_get_pos(&(curbuf->b_op_end)) == NUL)
					op_inclusive = FALSE;
			}

			redo_VIsual_busy = FALSE;
			/*
			 * Switch Visual off now, so screen updating does
			 * not show inverted text when the screen is redrawn.
			 * With YANK and sometimes with COLON and FILTER there is no screen
			 * redraw, so it is done here to remove the inverted part.
			 */
			if (!gui_yank)
			{
				VIsual_active = FALSE;
#ifdef USE_MOUSE
				setmouse();
#endif
				if (p_smd)
					clear_cmdline = TRUE;	/* unshow visual mode later */
				if (op_type == YANK || op_type == COLON || op_type == FILTER)
					update_curbuf(NOT_VALID);
			}

			/* set Prenum1 for LSHIFT and RSHIFT, e.g. "V3j2>" */
			if (Prenum == 0)
				Prenum1 = 1L;
			else
				Prenum1 = Prenum;
		}

		curwin->w_set_curswant = TRUE;

			/* op_empty is set when start and end are the same */
		op_empty = (op_motion_type == MCHAR && !op_inclusive &&
								 equal(curbuf->b_op_start, curbuf->b_op_end));

	/*
	 * If the end of an operator is in column one while op_motion_type is
	 * MCHAR and op_inclusive is FALSE, we put op_end after the last character
	 * in the previous line. If op_start is on or before the first non-blank
	 * in the line, the operator becomes linewise (strange, but that's the way
	 * vi does it).
	 */
		if (op_motion_type == MCHAR && op_inclusive == FALSE &&
						   !dont_adjust_op_end && curbuf->b_op_end.col == 0 &&
															op_line_count > 1)
		{
			op_end_adjusted = TRUE;		/* remember that we did this */
			--op_line_count;
			--curbuf->b_op_end.lnum;
			if (inindent(0))
				op_motion_type = MLINE;
			else
			{
				curbuf->b_op_end.col = STRLEN(ml_get(curbuf->b_op_end.lnum));
				if (curbuf->b_op_end.col)
				{
					--curbuf->b_op_end.col;
					op_inclusive = TRUE;
				}
			}
		}
		else
			op_end_adjusted = FALSE;
		switch (op_type)
		{
		  case LSHIFT:
		  case RSHIFT:
			do_shift(op_type, TRUE, (int)Prenum1);
			break;

		  case JOIN:
			if (op_line_count < 2)
				op_line_count = 2;
			if (curwin->w_cursor.lnum + op_line_count - 1 >
												   curbuf->b_ml.ml_line_count)
				beep_flush();
			else
			{
				/*
				 * If the cursor position has been changed, recompute the
				 * current cursor position in the window. If it's not visible,
				 * don't keep the window updated when joining the lines.
				 */
				if (old_cursor.lnum != curwin->w_cursor.lnum ||
									   old_cursor.col != curwin->w_cursor.col)
					redraw = (curs_rows() == OK);
				else
					redraw = TRUE;
				do_do_join(op_line_count, TRUE, redraw);
			}
			break;

		  case DELETE:
			if (!op_empty)
				do_delete();
			break;

		  case YANK:
			if (!op_empty)
				(void)do_yank(FALSE, !gui_yank);
			break;

		  case CHANGE:
			*command_busy = do_change();	/* will set op_type to NOP */
			break;

		  case FILTER:
			if (vim_strchr(p_cpo, CPO_FILTER) != NULL)
				AppendToRedobuff((char_u *)"!\r");	/* use any last used !cmd */
			else
				bangredo = TRUE;	/* do_bang() will put cmd in redo buffer */

		  case INDENT:
		  case COLON:

#if defined(LISPINDENT) || defined(CINDENT)
			/*
			 * If 'equalprg' is empty, do the indenting internally.
			 */
			if (op_type == INDENT && *p_ep == NUL)
			{
# ifdef LISPINDENT
				if (curbuf->b_p_lisp)
				{
					do_reindent(get_lisp_indent);
					break;
				}
# endif
# ifdef CINDENT
				do_reindent(get_c_indent);
				break;
# endif
			}
#endif /* defined(LISPINDENT) || defined(CINDENT) */

dofilter:
			if (VIsual_was_active)
				sprintf((char *)IObuff, ":'<,'>");
			else
				sprintf((char *)IObuff, ":%ld,%ld",
						(long)curbuf->b_op_start.lnum,
						(long)curbuf->b_op_end.lnum);
			stuffReadbuff(IObuff);
			if (op_type != COLON)
				stuffReadbuff((char_u *)"!");
			if (op_type == INDENT)
			{
#ifndef CINDENT
				if (*p_ep == NUL)
					stuffReadbuff((char_u *)"indent");
				else
#endif
					stuffReadbuff(p_ep);
				stuffReadbuff((char_u *)"\n");
			}
			else if (op_type == FORMAT || op_type == GFORMAT)
			{
				if (*p_fp == NUL)
					stuffReadbuff((char_u *)"fmt");
				else
					stuffReadbuff(p_fp);
				stuffReadbuff((char_u *)"\n");
			}
				/*	do_cmdline() does the rest */
			break;

		  case TILDE:
		  case UPPER:
		  case LOWER:
			if (!op_empty)
				do_tilde();
			break;

		  case FORMAT:
		  case GFORMAT:
			if (*p_fp != NUL)
				goto dofilter;		/* use external command */
			do_format();			/* use internal function */
			break;

		  default:
			clearopbeep();
		}
		prechar = NUL;
		if (!gui_yank)
		{
			/*
			 * if 'sol' not set, go back to old column for some commands
			 */
			if (!p_sol && op_motion_type == MLINE && (op_type == LSHIFT ||
									op_type == RSHIFT || op_type == DELETE))
				coladvance(curwin->w_curswant = old_col);
			op_type = NOP;
		}
		else
			curwin->w_cursor = old_cursor;
		op_block_mode = FALSE;
		yankbuffer = 0;
	}
}

#ifdef USE_MOUSE
/*
 * Do the appropriate action for the current mouse click in the current mode.
 *
 * Normal Mode:
 * event	     modi-  position	  visual	   change	action
 *               fier   cursor	  	               window
 * left press	  -		yes			end				yes
 * left press	  C		yes			end				yes		"^]" (2)
 * left press	  S		yes			end				yes		"*" (2)
 * left drag	  -		yes		start if moved  	no
 * left relse	  -		yes		start if moved		no
 * middle press	  -  	yes	     if not active		no		put register
 * middle press	  - 	yes	     if active			no		yank and put
 * right press	  -		yes		start or extend		yes
 * right press	  S		yes		no change      		yes		"#" (2)
 * right drag	  -		yes		extend				no
 * right relse	  -		yes		extend				no
 *
 * Insert or Replace Mode:
 * event	     modi-  position	  visual	   change	action
 *               fier   cursor	  	               window
 * left press	  -		yes		(cannot be active)	yes
 * left press	  C		yes		(cannot be active)	yes		"CTRL-O^]" (2)
 * left press	  S		yes		(cannot be active)	yes		"CTRL-O*" (2)
 * left drag	  -		yes		start or extend (1)	no		CTRL-O (1)
 * left relse	  -		yes		start or extend (1)	no		CTRL-O (1)
 * middle press	  - 	no	  	(cannot be active)	no		put register
 * right press	  -		yes		start or extend		yes		CTRL-O
 * right press	  S		yes		(cannot be active)	yes		"CTRL-O#" (2)
 *
 * (1) only if mouse pointer moved since press
 * (2) only if click is in same buffer
 *
 * Return TRUE if start_arrow() should be called for edit mode.
 */
	int
do_mouse(c, dir, count, fix_indent)
	int		c;				/* K_LEFTMOUSE, etc */
	int		dir;			/* Direction to 'put' if necessary */
	long	count;
	int		fix_indent;		/* Do we fix indent for 'put' if necessary? */
{
	static int	ignore_drag_release = FALSE;
	static FPOS	orig_cursor;
	static int	do_always = FALSE;		/* ignore 'mouse' setting next time */
	static int	got_click = FALSE;		/* got a click some time back */

	int		which_button;		/* MOUSE_LEFT, _MIDDLE or _RIGHT */
	int		is_click;			/* If FALSE it's a drag or release event */
	int		is_drag;			/* If TRUE it's a drag event */
	int		jump_flags = 0;		/* flags for jump_to_mouse() */
	FPOS	start_visual;
	FPOS	end_visual;
	BUF		*save_buffer;
	int		diff;
	int		moved;				/* Has cursor moved? */
	int		c1, c2;
	int		VIsual_was_active = VIsual_active;

	/*
	 * When GUI is active, always recognize mouse events, otherwise:
	 * - Ignore mouse event in normal mode if 'mouse' doesn't include 'n'.
	 * - Ignore mouse event in visual mode if 'mouse' doesn't include 'v'.
	 * - For command line and insert mode 'mouse' is checked before calling
	 *   do_mouse().
	 */
	if (do_always)
		do_always = FALSE;
	else
#ifdef USE_GUI
		if (!gui.in_use)
#endif
		{
			if (VIsual_active)
			{
				if (!mouse_has(MOUSE_VISUAL))
					return FALSE;
			}
			else if (State == NORMAL && !mouse_has(MOUSE_NORMAL))
				return FALSE;
		}

	which_button = get_mouse_button(KEY2TERMCAP1(c), &is_click, &is_drag);

	/*
	 * Ignore drag and release events if we didn't get a click.
	 */
	if (is_click)
		got_click = TRUE;
	else
	{
		if (!got_click)					/* didn't get click, ignore */
			return FALSE;
		if (!is_drag)					/* release, reset got_click */
			got_click = FALSE;
	}

	/*
	 * ALT is currently ignored
	 */
	if ((mod_mask & MOD_MASK_ALT))
		return FALSE;

	/*
	 * CTRL right mouse button does CTRL-T
	 */
	if (is_click && (mod_mask & MOD_MASK_CTRL) && which_button == MOUSE_RIGHT)
	{
		if (State & INSERT)
			stuffcharReadbuff(Ctrl('O'));
		stuffcharReadbuff(Ctrl('T'));
		got_click = FALSE;				/* ignore drag&release now */
		return FALSE;
	}

	/*
	 * CTRL only works with left mouse button
	 */
	if ((mod_mask & MOD_MASK_CTRL) && which_button != MOUSE_LEFT)
		return FALSE;

	/*
	 * When a modifier is down, ignore drag and release events, as well as
	 * multiple clicks and the middle mouse button. 
	 */
	if ((mod_mask & (MOD_MASK_SHIFT | MOD_MASK_CTRL | MOD_MASK_ALT)) &&
							(!is_click || (mod_mask & MOD_MASK_MULTI_CLICK) ||
												which_button == MOUSE_MIDDLE))
		return FALSE;

	/*
	 * If the button press was used as the movement command for an operator
	 * (eg "d<MOUSE>"), or it is the middle button that is held down, ignore
	 * drag/release events.
	 */
	if (!is_click && (ignore_drag_release || which_button == MOUSE_MIDDLE))
		return FALSE;

	/*
	 * Middle mouse button does a 'put' of the selected text
	 */
	if (which_button == MOUSE_MIDDLE)
	{
		if (State == NORMAL)
		{
			/*
			 * If an operator was pending, we don't know what the user wanted
			 * to do. Go back to normal mode: Clear the operator and beep().
			 */
			if (op_type != NOP)
			{
				clearopbeep();
				return FALSE;
			}

			/*
			 * If visual was active, yank the highlighted text and put it
			 * before the mouse pointer position.
			 */
			if (VIsual_active)
			{
				stuffcharReadbuff('y');
				stuffcharReadbuff(K_MIDDLEMOUSE);
				do_always = TRUE;		/* ignore 'mouse' setting next time */
				return FALSE;
			}
			/*
			 * The rest is below jump_to_mouse()
			 */
		}

		/*
		 * Middle click in insert mode doesn't move the mouse, just insert the
		 * contents of a register.  '.' register is special, can't insert that
		 * with do_put().
		 */
		else if (State & INSERT)
		{
			if (yankbuffer == '.')
				insertbuf(yankbuffer);
			else
			{
#ifdef USE_GUI
				if (gui.in_use && yankbuffer == 0)
					yankbuffer = '*';
#endif
				do_put(BACKWARD, 1L, fix_indent);

				/* Put cursor after the end of the just pasted text. */
				curwin->w_cursor = curbuf->b_op_end;
				if (gchar_cursor() != NUL)
					++curwin->w_cursor.col;

				/* Repeat it with CTRL-R x, not exactly the same, but mostly
				 * works fine. */
				AppendCharToRedobuff(Ctrl('R'));
				if (yankbuffer == 0)
					AppendCharToRedobuff('"');
				else
					AppendCharToRedobuff(yankbuffer);
			}
			return FALSE;
		}
		else
			return FALSE;
	}

	if (!is_click)
		jump_flags |= MOUSE_FOCUS;

	start_visual.lnum = 0;

	if ((State & (NORMAL | INSERT)) &&
							   !(mod_mask & (MOD_MASK_SHIFT | MOD_MASK_CTRL)))
	{
		if (which_button == MOUSE_LEFT)
		{
			if (is_click)
			{
				if (VIsual_active)
				{
					end_visual_mode();
					update_curbuf(NOT_VALID);
				}
			}
			else
				jump_flags |= MOUSE_MAY_VIS;
		}
		else if (which_button == MOUSE_RIGHT)
		{
			if (is_click && VIsual_active)
			{
				/*
				 * Remember the start and end of visual before moving the
				 * cursor.
				 */
				if (lt(curwin->w_cursor, VIsual))
				{
					start_visual = curwin->w_cursor;
					end_visual = VIsual;
				}
				else
				{
					start_visual = VIsual;
					end_visual = curwin->w_cursor;
				}
			}
			jump_flags |= MOUSE_MAY_VIS;
		}
	}

	if (!is_drag)
	{
		/*
		 * If an operator is pending, ignore all drags and releases until the
		 * next mouse click.
		 */
		ignore_drag_release = (op_type != NOP);
	}

	/*
	 * Jump!
	 */
	if (!is_click)
		jump_flags |= MOUSE_DID_MOVE;
	save_buffer = curbuf;
	moved = (jump_to_mouse(jump_flags) & CURSOR_MOVED);

	/* When jumping to another buffer, stop visual mode */
	if (curbuf != save_buffer && VIsual_active)
	{
		end_visual_mode();
		update_curbuf(NOT_VALID);		/* delete the inversion */
	}
	else if (start_visual.lnum)		/* right click in visual mode */
	{
		/*
		 * If the click is before the start of visual, change the start.  If
		 * the click is after the end of visual, change the end.  If the click
		 * is inside the visual, change the closest side.
		 */
		if (lt(curwin->w_cursor, start_visual))
			VIsual = end_visual;
		else if (lt(end_visual, curwin->w_cursor))
			VIsual = start_visual;
		else
		{
			/* In the same line, compare column number */
			if (end_visual.lnum == start_visual.lnum)
			{
				if (curwin->w_cursor.col - start_visual.col >
								end_visual.col - curwin->w_cursor.col)
					VIsual = start_visual;
				else
					VIsual = end_visual;
			}

			/* In different lines, compare line number */
			else
			{
				diff = (curwin->w_cursor.lnum - start_visual.lnum) -
							(end_visual.lnum - curwin->w_cursor.lnum);

				if (diff > 0)			/* closest to end */
					VIsual = start_visual;
				else if (diff < 0)		/* closest to start */
					VIsual = end_visual;
				else					/* in the middle line */
				{
					if (curwin->w_cursor.col <
									(start_visual.col + end_visual.col) / 2)
						VIsual = end_visual;
					else
						VIsual = start_visual;
				}
			}
		}
	}
	/*
	 * If Visual mode started in insert mode, execute "CTRL-O"
	 */
	else if ((State & INSERT) && VIsual_active)
		stuffcharReadbuff(Ctrl('O'));
	/*
	 * If cursor has moved, need to update Cline_row
	 */
	else if (moved)
		cursupdate();

	/*
	 * Middle mouse click: Put text before cursor.
	 */
	if (which_button == MOUSE_MIDDLE)
	{
#ifdef USE_GUI
		if (gui.in_use && yankbuffer == 0)
			yankbuffer = '*';
#endif
		if (yank_buffer_mline())
		{
			if (mouse_past_bottom)
				dir = FORWARD;
		}
		else if (mouse_past_eol)
			dir = FORWARD;

		if (fix_indent)
		{
			c1 = (dir == BACKWARD) ? '[' : ']';
			c2 = 'p';
		}
		else
		{
			c1 = (dir == FORWARD) ? 'p' : 'P';
			c2 = NUL;
		}
		prep_redo(Prenum, NUL, c1, c2, NUL);
		/*
		 * Remember where the paste started, so in edit() Insstart can be set
		 * to this position
		 */
		if (restart_edit)
			where_paste_started = curwin->w_cursor;
		do_put(dir, count, fix_indent);

		/* Put cursor at the end of the just pasted text. */
		curwin->w_cursor = curbuf->b_op_end;
		if (restart_edit && gchar_cursor() != NUL)
			++curwin->w_cursor.col;			/* put cursor after the text */
	}

	/*
	 * Ctrl-Mouse click jumps to the tag under the mouse pointer
	 */
	else if ((mod_mask & MOD_MASK_CTRL))
	{
		if (State & INSERT)
			stuffcharReadbuff(Ctrl('O'));
		stuffcharReadbuff(Ctrl(']'));
		ignore_drag_release = TRUE;		/* ignore drag and release now */
	}

	/*
	 * Shift-Mouse click searches for the next occurrence of the word under
	 * the mouse pointer
	 */
	else if ((mod_mask & MOD_MASK_SHIFT))
	{
		if (State & INSERT)
			stuffcharReadbuff(Ctrl('O'));
		if (which_button == MOUSE_LEFT)
			stuffcharReadbuff('*');
		else	/* MOUSE_RIGHT */
			stuffcharReadbuff('#');
	}

	/* Handle double clicks */
	else if ((mod_mask & MOD_MASK_MULTI_CLICK) && (State & (NORMAL | INSERT)))
	{
		if (is_click || !VIsual_active)
		{
			if (VIsual_active)
				orig_cursor = VIsual;
			else
			{
				start_visual_highlight();
				VIsual = curwin->w_cursor;
				orig_cursor = VIsual;
				VIsual_active = TRUE;
#ifdef USE_MOUSE
				setmouse();
#endif
				if (p_smd)
					redraw_cmdline = TRUE;	/* show visual mode later */
			}
			if (mod_mask & MOD_MASK_2CLICK)
				VIsual_mode = 'v';
			else if (mod_mask & MOD_MASK_3CLICK)
				VIsual_mode = 'V';
			else if (mod_mask & MOD_MASK_4CLICK)
				VIsual_mode = Ctrl('V');
		}
		if (mod_mask & MOD_MASK_2CLICK)
		{
			if (lt(curwin->w_cursor, orig_cursor))
			{
				find_start_of_word(&curwin->w_cursor);
				find_end_of_word(&VIsual);
			}
			else
			{
				find_start_of_word(&VIsual);
				find_end_of_word(&curwin->w_cursor);
			}
			curwin->w_set_curswant = TRUE;
		}
		if (is_click)
		{
			curs_columns(TRUE);				/* recompute w_virtcol */
			update_curbuf(NOT_VALID);		/* update the inversion */
		}
	}
	else if (VIsual_active && VIsual_was_active != VIsual_active)
		VIsual_mode = 'v';

	return moved;
}

	static void
find_start_of_word(pos)
	FPOS	*pos;
{
	char_u	*ptr;
	int		cclass;

	ptr = ml_get(pos->lnum);
	cclass = get_mouse_class(ptr[pos->col]);

	/* Can't test pos->col >= 0 because pos->col is unsigned */
	while (pos->col > 0 && get_mouse_class(ptr[pos->col]) == cclass)
		pos->col--;
	if (pos->col != 0 || get_mouse_class(ptr[0]) != cclass)
		pos->col++;
}

	static void
find_end_of_word(pos)
	FPOS	*pos;
{
	char_u	*ptr;
	int		cclass;

	ptr = ml_get(pos->lnum);
	cclass = get_mouse_class(ptr[pos->col]);
	while (ptr[pos->col] && get_mouse_class(ptr[pos->col]) == cclass)
		pos->col++;
	pos->col--;
}

	static int
get_mouse_class(c)
	int		c;
{
	if (c == ' ' || c == '\t')
		return ' ';

	if (isidchar(c))
		return 'a';

	/*
	 * There are a few special cases where we want certain combinations of
	 * characters to be considered as a single word.  These are things like
	 * "->", "/ *", "*=", "+=", "&=", "<=", ">=", "!=" etc.  Otherwise, each
	 * character is in it's own class.
	 */
	if (c != NUL && vim_strchr((char_u *)"-+*/%<>&|^!=", c) != NULL)
		return '=';
	return c;
}
#endif /* USE_MOUSE */

/*
 * start highlighting for visual mode
 */
	void
start_visual_highlight()
{
	static int		didwarn = FALSE;		/* warned for broken inversion */

	if (!didwarn && set_highlight('v') == FAIL)/* cannot highlight */
	{
		EMSG("Warning: terminal cannot highlight");
		didwarn = TRUE;
	}
}

/*
 * End visual mode.  If we are using the GUI, and autoselect is set, then
 * remember what was selected in case we need to paste it somewhere while we
 * still own the selection.  This function should ALWAYS be called to end
 * visual mode.
 */
	void
end_visual_mode()
{
#ifdef USE_GUI
	if (gui.in_use)
		gui_auto_select();
#endif
	VIsual_active = FALSE;
#ifdef USE_MOUSE
	setmouse();
#endif
	VIsual_end = curwin->w_cursor;		/* remember for '> mark */
	if (p_smd)
		clear_cmdline = TRUE;			/* unshow visual mode later */
}

/*
 * Find the identifier under or to the right of the cursor.  If none is
 * found and find_type has FIND_STRING, then find any non-white string.  The
 * length of the string is returned, or zero if no string is found.  If a
 * string is found, a pointer to the string is put in *string, but note that
 * the caller must use the length returned as this string may not be NUL
 * terminated.
 */
	int
find_ident_under_cursor(string, find_type)
	char_u	**string;
	int		find_type;
{
	char_u	*ptr;
	int		col = 0;		/* init to shut up GCC */
	int		i;

	/*
	 * if i == 0: try to find an identifier
	 * if i == 1: try to find any string
	 */
	ptr = ml_get_curline();
	for (i = (find_type & FIND_IDENT) ? 0 : 1;	i < 2; ++i)
	{
		/*
		 * skip to start of identifier/string
		 */
		col = curwin->w_cursor.col;
		while (ptr[col] != NUL &&
					(i == 0 ? !iswordchar(ptr[col]) : vim_iswhite(ptr[col])))
			++col;

		/*
		 * Back up to start of identifier/string. This doesn't match the
		 * real vi but I like it a little better and it shouldn't bother
		 * anyone.
		 * When FIND_IDENT isn't defined, we backup until a blank.
		 */
		while (col > 0 && (i == 0 ? iswordchar(ptr[col - 1]) :
					(!vim_iswhite(ptr[col - 1]) &&
				   (!(find_type & FIND_IDENT) || !iswordchar(ptr[col - 1])))))
			--col;

		/*
		 * if we don't want just any old string, or we've found an identifier,
		 * stop searching.
		 */
		if (!(find_type & FIND_STRING) || iswordchar(ptr[col]))
			break;
	}
	/*
	 * didn't find an identifier or string
	 */
	if (ptr[col] == NUL || (!iswordchar(ptr[col]) && i == 0))
	{
		if (find_type & FIND_STRING)
			EMSG("No string under cursor");
		else
			EMSG("No identifier under cursor");
		return 0;
	}
	ptr += col;
	*string = ptr;
	col = 0;
	while (i == 0 ? iswordchar(*ptr) : (*ptr != NUL && !vim_iswhite(*ptr)))
	{
		++ptr;
		++col;
	}
	return col;
}

	static void
prep_redo(num, pre_char, cmd, c, nchar)
	long 	num;
	int		pre_char;
	int		cmd;
	int		c;
	int		nchar;
{
	ResetRedobuff();
	if (yankbuffer != 0)	/* yank from specified buffer */
	{
		AppendCharToRedobuff('\"');
		AppendCharToRedobuff(yankbuffer);
	}
	if (num)
		AppendNumberToRedobuff(num);
	if (pre_char != NUL)
		AppendCharToRedobuff(pre_char);
	AppendCharToRedobuff(cmd);
	if (c != NUL)
		AppendCharToRedobuff(c);
	if (nchar != NUL)
		AppendCharToRedobuff(nchar);
}

/*
 * check for operator active and clear it
 *
 * return TRUE if operator was active
 */
	static int
checkclearop()
{
	if (op_type == NOP)
		return (FALSE);
	clearopbeep();
	return (TRUE);
}

/*
 * check for operator or Visual active and clear it
 *
 * return TRUE if operator was active
 */
	static int
checkclearopq()
{
	if (op_type == NOP && !VIsual_active)
		return (FALSE);
	clearopbeep();
	return (TRUE);
}

	static void
clearop()
{
	op_type = NOP;
	yankbuffer = 0;
	prechar = NUL;
}

	static void
clearopbeep()
{
	clearop();
	beep_flush();
}

/*
 * Routines for displaying a partly typed command
 */

static char_u	showcmd_buf[SHOWCMD_COLS + 1];
static char_u	old_showcmd_buf[SHOWCMD_COLS + 1];	/* For push_showcmd() */
static int		is_showcmd_clear = TRUE;

static void display_showcmd __ARGS((void));

	void
clear_showcmd()
{
	if (!p_sc)
		return;

	showcmd_buf[0] = NUL;

	/*
	 * Don't actually display something if there is nothing to clear.
	 */
	if (is_showcmd_clear)
		return;

	display_showcmd();
}

/*
 * Add 'c' to string of shown command chars.
 * Return TRUE if setcursor() has been called.
 */
	int
add_to_showcmd(c, display_always)
	int 	c;
	int		display_always;
{
	char_u	*p;
	int		old_len;
	int		extra_len;
	int		overflow;

	if (!p_sc)
		return FALSE;

	p = transchar(c);
	old_len = STRLEN(showcmd_buf);
	extra_len = STRLEN(p);
	overflow = old_len + extra_len - SHOWCMD_COLS;
	if (overflow > 0)
		STRCPY(showcmd_buf, showcmd_buf + overflow);
	STRCAT(showcmd_buf, p);

	if (!display_always && char_avail())
		return FALSE;

	display_showcmd();

	return TRUE;
}

/*
 * Delete 'len' characters from the end of the shown command.
 */
	static void
del_from_showcmd(len)
	int 	len;
{
	int		old_len;

	if (!p_sc)
		return;

	old_len = STRLEN(showcmd_buf);
	if (len > old_len)
		len = old_len;
	showcmd_buf[old_len - len] = NUL;

	if (!char_avail())
		display_showcmd();
}

	void
push_showcmd()
{
	if (p_sc)
		STRCPY(old_showcmd_buf, showcmd_buf);
}

	void
pop_showcmd()
{
	if (!p_sc)
		return;

	STRCPY(showcmd_buf, old_showcmd_buf);

	display_showcmd();
}

	static void
display_showcmd()
{
	int		len;

	cursor_off();

	len = STRLEN(showcmd_buf);
	if (len == 0)
		is_showcmd_clear = TRUE;
	else
	{
		screen_msg(showcmd_buf, (int)Rows - 1, sc_col);
		is_showcmd_clear = FALSE;
	}

	/*
	 * clear the rest of an old message by outputing up to SHOWCMD_COLS spaces
	 */
	screen_msg((char_u *)"          " + len, (int)Rows - 1, sc_col + len);

	setcursor();			/* put cursor back where it belongs */
}

/*
 * Implementation of "gd" and "gD" command.
 */
	static void
do_gd(nchar)
	int		nchar;
{
	int			len;
	char_u		*pat;
	FPOS		old_pos;
	int			t;
	int			save_p_ws;
	int			save_p_scs;
	char_u		*ptr;

	if ((len = find_ident_under_cursor(&ptr, FIND_IDENT)) == 0 ||
											   (pat = alloc(len + 5)) == NULL)
	{
		clearopbeep();
		return;
	}
	sprintf((char *)pat, iswordchar(*ptr) ? "\\<%.*s\\>" :
			"%.*s", len, ptr);
	old_pos = curwin->w_cursor;
	save_p_ws = p_ws;
	save_p_scs = p_scs;
	p_ws = FALSE;		/* don't wrap around end of file now */
	p_scs = FALSE;		/* don't switch ignorecase off now */
	fo_do_comments = TRUE;

	/*
	 * Search back for the end of the previous function.
	 * If this fails, and with "gD", go to line 1.
	 * Search forward for the identifier, ignore comment lines.
	 */
	if (nchar == 'D' || !findpar(BACKWARD, 1L, '}', FALSE))
	{
		setpcmark();					/* Set in findpar() otherwise */
		curwin->w_cursor.lnum = 1;
	}

	while ((t = searchit(&curwin->w_cursor, FORWARD, pat, 1L, 0, RE_LAST))
				== OK &&
			get_leader_len(ml_get_curline(), NULL) &&
			old_pos.lnum > curwin->w_cursor.lnum)
		++curwin->w_cursor.lnum;
	if (t == FAIL || old_pos.lnum <= curwin->w_cursor.lnum)
	{
		clearopbeep();
		curwin->w_cursor = old_pos;
	}
	else
		curwin->w_set_curswant = TRUE;

	vim_free(pat);
	p_ws = save_p_ws;
	p_scs = save_p_scs;
	fo_do_comments = FALSE;
}

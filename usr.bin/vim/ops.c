/*	$OpenBSD: ops.c,v 1.3 1996/10/14 03:55:22 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * ops.c: implementation of various operators: do_shift, do_delete, do_tilde,
 *		  do_change, do_yank, do_put, do_join
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"

/*
 * Number of registers.
 *      0 = unnamed register, for normal yanks and puts
 *   1..9 = number registers, for deletes
 * 10..35 = named registers
 *     36 = delete register (-)
 *     37 = GUI selection register (*). Only if USE_GUI defined
 */
#ifdef USE_GUI
# define NUM_REGISTERS			38
#else
# define NUM_REGISTERS			37
#endif

/*
 * Symbolic names for some registers.
 */
#define DELETION_REGISTER		36
#ifdef USE_GUI
# define GUI_SELECTION_REGISTER	37
#endif

/*
 * Each yank buffer is an array of pointers to lines.
 */
static struct yankbuf
{
	char_u		**y_array;		/* pointer to array of line pointers */
	linenr_t 	y_size; 		/* number of lines in y_array */
	char_u		y_type; 		/* MLINE, MCHAR or MBLOCK */
} y_buf[NUM_REGISTERS];

static struct	yankbuf *y_current;			/* ptr to current yank buffer */
static int		yankappend;					/* TRUE when appending */
static struct	yankbuf *y_previous = NULL; /* ptr to last written yank buffr */

/*
 * structure used by block_prep, do_delete and do_yank for blockwise operators
 */
struct block_def
{
	int			startspaces;
	int			endspaces;
	int			textlen;
	char_u		*textstart;
	colnr_t		textcol;
};

static void		get_yank_buffer __ARGS((int));
static int		stuff_yank __ARGS((int, char_u *));
static void		free_yank __ARGS((long));
static void		free_yank_all __ARGS((void));
static void		block_prep __ARGS((struct block_def *, linenr_t, int));
static int		same_leader __ARGS((int, char_u *, int, char_u *));
static int		fmt_end_block __ARGS((linenr_t, int *, char_u **));

/*
 * do_shift - handle a shift operation
 */
	void
do_shift(op, curs_top, amount)
	int 			op;
	int				curs_top;
	int				amount;
{
	register long	i;
	int				first_char;

	if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
				   (linenr_t)(curwin->w_cursor.lnum + op_line_count)) == FAIL)
		return;
	for (i = op_line_count; --i >= 0; )
	{
		first_char = *ml_get_curline();
		if (first_char == NUL)							/* empty line */
			curwin->w_cursor.col = 0;
		/*
		 * Don't move the line right if it starts with # and p_si is set.
		 */
		else
#if defined(SMARTINDENT) || defined(CINDENT)
			if (first_char != '#' || (
# ifdef SMARTINDENT
						 !curbuf->b_p_si
# endif
# if defined(SMARTINDENT) && defined(CINDENT)
						 	&&
# endif
# ifdef CINDENT
						 (!curbuf->b_p_cin || !in_cinkeys('#', ' ', TRUE))
# endif
										))
#endif
		{
			/* if (op_block_mode)
					shift the block, not the whole line
			else */
				shift_line(op == LSHIFT, p_sr, amount);
		}
		++curwin->w_cursor.lnum;
	}

	if (curs_top)			/* put cursor on first line, for ">>" */
	{
		curwin->w_cursor.lnum -= op_line_count;
		beginline(MAYBE);	/* shift_line() may have changed cursor.col */
	}
	else
		--curwin->w_cursor.lnum;		/* put cursor on last line, for ":>" */
	updateScreen(CURSUPD);

	if (op_line_count > p_report)
	   smsg((char_u *)"%ld line%s %ced %d time%s", op_line_count,
						   plural(op_line_count), (op == RSHIFT) ? '>' : '<',
						   amount, plural((long)amount));
}

/*
 * shift the current line one shiftwidth left (if left != 0) or right
 * leaves cursor on first blank in the line
 */
	void
shift_line(left, round, amount)
	int left;
	int	round;
	int	amount;
{
	register int count;
	register int i, j;
	int p_sw = (int)curbuf->b_p_sw;

	count = get_indent();			/* get current indent */

	if (round)						/* round off indent */
	{
		i = count / p_sw;			/* number of p_sw rounded down */
		j = count % p_sw;			/* extra spaces */
		if (j && left)				/* first remove extra spaces */
			--amount;
		if (left)
		{
			i -= amount;
			if (i < 0)
				i = 0;
		}
		else
			i += amount;
		count = i * p_sw;
	}
	else				/* original vi indent */
	{
		if (left)
		{
			count -= p_sw * amount;
			if (count < 0)
				count = 0;
		}
		else
			count += p_sw * amount;
	}
	set_indent(count, TRUE);		/* set new indent */
}

#if defined(LISPINDENT) || defined(CINDENT)
/*
 * do_reindent - handle reindenting a block of lines for C or lisp.
 *
 * mechanism copied from do_shift, above
 */
	void
do_reindent(how)
	int (*how) __ARGS((void));
{
	register long   i;
	char_u			*l;
	int				count;

	if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
					(linenr_t)(curwin->w_cursor.lnum + op_line_count)) == FAIL)
		return;

	for (i = op_line_count; --i >= 0 && !got_int; )
	{
		/* it's a slow thing to do, so give feedback so there's no worry that
		 * the computer's just hung. */

		if ((i % 50 == 0 || i == op_line_count - 1) && op_line_count > p_report)
			smsg((char_u *)"%ld line%s to indent... ", i, plural(i));

		/*
		 * Be vi-compatible: For lisp indenting the first line is not
		 * indented, unless there is only one line.
		 */
#ifdef LISPINDENT
		if (i != op_line_count - 1 || op_line_count == 1 ||
													   how != get_lisp_indent)
#endif
		{
			l = skipwhite(ml_get_curline());
			if (*l == NUL)					/* empty or blank line */
				count = 0;
			else
				count = how();				/* get the indent for this line */
			
			set_indent(count, TRUE);
		}
		++curwin->w_cursor.lnum;
	}

	/* put cursor on first non-blank of indented line */
	curwin->w_cursor.lnum -= op_line_count;
	beginline(MAYBE);

	updateScreen(CURSUPD);

	if (op_line_count > p_report)
	{
		i = op_line_count - (i + 1);
		smsg((char_u *)"%ld line%s indented ", i, plural(i));
	}
}
#endif /* defined(LISPINDENT) || defined(CINDENT) */

/*
 * check if character is name of yank buffer
 * Note: There is no check for 0 (default register), caller should do this
 */
 	int
is_yank_buffer(c, writing)
	int		c;
	int		writing;		/* if TRUE check for writable buffers */
{
	if (c > '~')
		return FALSE;
	if (isalnum(c) || (!writing && vim_strchr((char_u *)".%:", c) != NULL) ||
														  c == '"' || c == '-'
#ifdef USE_GUI
												   || (gui.in_use && c == '*')
#endif
														)
		return TRUE;
	return FALSE;
}

/*
 * Set y_current and yankappend, according to the value of yankbuffer.
 *
 * If yankbuffer is 0 and writing, use buffer 0
 * If yankbuffer is 0 and reading, use previous buffer
 */
	static void
get_yank_buffer(writing)
	int		writing;
{
	register int i;

	yankappend = FALSE;
	if (((yankbuffer == 0 && !writing) || yankbuffer == '"') &&
														   y_previous != NULL)
	{
		y_current = y_previous;
		return;
	}
	i = yankbuffer;
	if (isdigit(i))
		i -= '0';
	else if (islower(i))
		i -= 'a' - 10;
	else if (isupper(i))
	{
		i -= 'A' - 10;
		yankappend = TRUE;
	}
	else if (yankbuffer == '-')
		i = DELETION_REGISTER;
#ifdef USE_GUI
	else if (gui.in_use && yankbuffer == '*')
		i = GUI_SELECTION_REGISTER;
#endif
	else				/* not 0-9, a-z, A-Z or '-': use buffer 0 */
		i = 0;
	y_current = &(y_buf[i]);
	if (writing)		/* remember the buffer we write into for do_put() */
		y_previous = y_current;
}

/*
 * return TRUE if the current yank buffer has type MLINE
 */
	int
yank_buffer_mline()
{
	if (yankbuffer != 0 && !is_yank_buffer(yankbuffer, FALSE))
		return FALSE;
	get_yank_buffer(FALSE);
	return (y_current->y_type == MLINE);
}

/*
 * start or stop recording into a yank buffer
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_record(c)
	int c;
{
	char_u		*p;
	static int	bufname;
	int			retval;

	if (Recording == FALSE) 		/* start recording */
	{
						/* registers 0-9, a-z and " are allowed */
		if (c > '~' || (!isalnum(c) && c != '"'))
			retval = FAIL;
		else
		{
			Recording = TRUE;
			showmode();
			bufname = c;
			retval = OK;
		}
	}
	else							/* stop recording */
	{
		Recording = FALSE;
		MSG("");
		p = get_recorded();
		if (p == NULL)
			retval = FAIL;
		else
			retval = (stuff_yank(bufname, p));
	}
	return retval;
}

/*
 * stuff string 'p' into yank buffer 'bufname' (append if uppercase)
 * 'p' is assumed to be alloced.
 *
 * return FAIL for failure, OK otherwise
 */
	static int
stuff_yank(bufname, p)
	int bufname;
	char_u *p;
{
	char_u *lp;
	char_u **pp;

	yankbuffer = bufname;
											/* check for read-only buffer */
	if (yankbuffer != 0 && !is_yank_buffer(yankbuffer, TRUE))
		return FAIL;
	get_yank_buffer(TRUE);
	if (yankappend && y_current->y_array != NULL)
	{
		pp = &(y_current->y_array[y_current->y_size - 1]);
		lp = lalloc((long_u)(STRLEN(*pp) + STRLEN(p) + 1), TRUE);
		if (lp == NULL)
		{
			vim_free(p);
			return FAIL;
		}
		STRCPY(lp, *pp);
		STRCAT(lp, p);
		vim_free(p);
		vim_free(*pp);
		*pp = lp;
	}
	else
	{
		free_yank_all();
		if ((y_current->y_array =
						(char_u **)alloc((unsigned)sizeof(char_u *))) == NULL)
		{
			vim_free(p);
			return FAIL;
		}
		y_current->y_array[0] = p;
		y_current->y_size = 1;
		y_current->y_type = MCHAR;	/* used to be MLINE, why? */
	}
	return OK;
}

/*
 * execute a yank buffer (register): copy it into the stuff buffer
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_execbuf(c, colon, addcr)
	int c;
	int	colon;				/* insert ':' before each line */
	int addcr;				/* always add '\n' to end of line */
{
	static int	lastc = NUL;
	long		i;
	char_u		*p;
	int			truncated;
	int			retval;


	if (c == '@')					/* repeat previous one */
		c = lastc;
	if (c == '%' || !is_yank_buffer(c, FALSE))	/* check for valid buffer */
		return FAIL;
	lastc = c;

	if (c == ':')					/* use last command line */
	{
		if (last_cmdline == NULL)
		{
			EMSG(e_nolastcmd);
			return FAIL;
		}
		vim_free(new_last_cmdline);	/* don't keep the cmdline containing @: */
		new_last_cmdline = NULL;
		if (ins_typebuf((char_u *)"\n", FALSE, 0, TRUE) == FAIL)
			return FAIL;
		if (ins_typebuf(last_cmdline, FALSE, 0, TRUE) == FAIL)
			return FAIL;
		if (ins_typebuf((char_u *)":", FALSE, 0, TRUE) == FAIL)
			return FAIL;
	}
	else if (c == '.')				/* use last inserted text */
	{
		p = get_last_insert();
		if (p == NULL)
		{
			EMSG(e_noinstext);
			return FAIL;
		}
		i = STRLEN(p);
		if (i > 0 && p[i - 1] == ESC)	/* remove trailing ESC */
		{
			p[i - 1] = NUL;
			truncated = TRUE;
		}
		else
			truncated = FALSE;
		retval = ins_typebuf(p, FALSE, 0, TRUE);
		if (truncated)
			p[i - 1] = ESC;
		return retval;
	}
	else
	{
		yankbuffer = c;
		get_yank_buffer(FALSE);
		if (y_current->y_array == NULL)
			return FAIL;

		/*
		 * Insert lines into typeahead buffer, from last one to first one.
		 */
		for (i = y_current->y_size; --i >= 0; )
		{
		/* insert newline between lines and after last line if type is MLINE */
			if (y_current->y_type == MLINE || i < y_current->y_size - 1
																	 || addcr)
			{
				if (ins_typebuf((char_u *)"\n", FALSE, 0, TRUE) == FAIL)
					return FAIL;
			}
			if (ins_typebuf(y_current->y_array[i], FALSE, 0, TRUE) == FAIL)
				return FAIL;
			if (colon && ins_typebuf((char_u *)":", FALSE, 0, TRUE) == FAIL)
				return FAIL;
		}
		Exec_reg = TRUE;		/* disable the 'q' command */
	}
	return OK;
}

/*
 * Insert a yank buffer: copy it into the Read buffer.
 * Used by CTRL-R command and middle mouse button in insert mode.
 *
 * return FAIL for failure, OK otherwise
 */
	int
insertbuf(c)
	int c;
{
	long	i;
	int		retval = OK;

	/*
	 * It is possible to get into an endless loop by having CTRL-R a in
	 * register a and then, in insert mode, doing CTRL-R a.
	 * If you hit CTRL-C, the loop will be broken here.
	 */
	mch_breakcheck();
	if (got_int)
		return FAIL;

	/* check for valid buffer */
	if (c != NUL && !is_yank_buffer(c, FALSE))
		return FAIL;

#ifdef USE_GUI
	if (c == '*')
		gui_get_selection();			/* may fill * register */
#endif

	if (c == '.')						/* insert last inserted text */
		retval = stuff_inserted(NUL, 1L, TRUE);
	else if (c == '%')					/* insert file name */
	{
		if (check_fname() == FAIL)
			return FAIL;
		stuffReadbuff(curbuf->b_xfilename);
	}
	else if (c == ':')					/* insert last command line */
	{
		if (last_cmdline == NULL)
		{
			EMSG(e_nolastcmd);
			return FAIL;
		}
		stuffReadbuff(last_cmdline);
	}
	else								/* name or number register */
	{
		yankbuffer = c;
		get_yank_buffer(FALSE);
		if (y_current->y_array == NULL)
			retval = FAIL;
		else
		{

			for (i = 0; i < y_current->y_size; ++i)
			{
				stuffReadbuff(y_current->y_array[i]);
				/* insert newline between lines and after last line if type is
				 * MLINE */
				if (y_current->y_type == MLINE || i < y_current->y_size - 1)
					stuffReadbuff((char_u *)"\n");
			}
		}
	}

	return retval;
}

/*
 * paste a yank buffer into the command line.
 * used by CTRL-R command in command-line mode
 * insertbuf() can't be used here, because special characters from the
 * register contents will be interpreted as commands.
 *
 * return FAIL for failure, OK otherwise
 */
	int
cmdline_paste(c)
	int c;
{
	long i;

	if (!is_yank_buffer(c, FALSE))		/* check for valid buffer */
		return FAIL;

#ifdef USE_GUI
	if (c == '*')
		gui_get_selection();
#endif

	if (c == '.')						/* insert last inserted text */
		return FAIL;					/* Unimplemented */

	if (c == '%')						/* insert file name */
	{
		if (check_fname() == FAIL)
			return FAIL;
		return put_on_cmdline(curbuf->b_xfilename, -1, TRUE);
	}

	if (c == ':')						/* insert last command line */
	{
		if (last_cmdline == NULL)
			return FAIL;
		return put_on_cmdline(last_cmdline, -1, TRUE);
	}

	yankbuffer = c;
	get_yank_buffer(FALSE);
	if (y_current->y_array == NULL)
		return FAIL;

	for (i = 0; i < y_current->y_size; ++i)
	{
		put_on_cmdline(y_current->y_array[i], -1, FALSE);

		/* insert ^M between lines and after last line if type is MLINE */
		if (y_current->y_type == MLINE || i < y_current->y_size - 1)
			put_on_cmdline((char_u *)"\r", 1, FALSE);
	}
	return OK;
}

/*
 * do_delete - handle a delete operation
 */
	void
do_delete()
{
	register int		n;
	linenr_t			lnum;
	char_u				*ptr;
	char_u				*newp, *oldp;
	linenr_t			old_lcount = curbuf->b_ml.ml_line_count;
	int					did_yank = FALSE;
	struct block_def	bd;

	if (curbuf->b_ml.ml_flags & ML_EMPTY)			/* nothing to do */
		return;

/*
 * Imitate the strange Vi behaviour: If the delete spans more than one line
 * and op_motion_type == MCHAR and the result is a blank line, make the delete
 * linewise. Don't do this for the change command.
 */
	if (op_motion_type == MCHAR && op_line_count > 1 && op_type == DELETE)
	{
		ptr = ml_get(curbuf->b_op_end.lnum) + curbuf->b_op_end.col +
																 op_inclusive;
		ptr = skipwhite(ptr);
		if (*ptr == NUL && inindent(0))
			op_motion_type = MLINE;
	}

/*
 * Check for trying to delete (e.g. "D") in an empty line.
 * Note: For change command it is ok.
 */
	if (op_motion_type == MCHAR && op_line_count == 1 &&
				op_type == DELETE && *ml_get(curbuf->b_op_start.lnum) == NUL)
	{
		beep_flush();
		return;
	}

/*
 * Do a yank of whatever we're about to delete.
 * If a yank buffer was specified, put the deleted text into that buffer
 */
	if (yankbuffer != 0)
	{
										/* check for read-only buffer */
		if (!is_yank_buffer(yankbuffer, TRUE))
		{
			beep_flush();
			return;
		}
		get_yank_buffer(TRUE);			/* yank into specified buffer */
		if (do_yank(TRUE, FALSE) == OK)	/* yank without message */
			did_yank = TRUE;
	}

/*
 * Put deleted text into register 1 and shift number buffers if
 * the delete contains a line break, or when a yankbuffer has been specified!
 */
	if (yankbuffer != 0 || op_motion_type == MLINE || op_line_count > 1)
	{
		y_current = &y_buf[9];
		free_yank_all();				/* free buffer nine */
		for (n = 9; n > 1; --n)
			y_buf[n] = y_buf[n - 1];
		y_previous = y_current = &y_buf[1];
		y_buf[1].y_array = NULL;		/* set buffer one to empty */
		yankbuffer = 0;
	}
	else if (yankbuffer == 0)			/* yank into unnamed buffer */
	{
		yankbuffer = '-';				/* use special delete buffer */
		get_yank_buffer(TRUE);
		yankbuffer = 0;
	}

	if (yankbuffer == 0 && do_yank(TRUE, FALSE) == OK)
		did_yank = TRUE;

/*
 * If there's too much stuff to fit in the yank buffer, then get a
 * confirmation before doing the delete. This is crude, but simple. And it
 * avoids doing a delete of something we can't put back if we want.
 */
	if (!did_yank)
	{
		if (ask_yesno((char_u *)"cannot yank; delete anyway", TRUE) != 'y')
		{
			emsg(e_abort);
			return;
		}
	}

/*
 * block mode delete
 */
	if (op_block_mode)
	{
		if (u_save((linenr_t)(curbuf->b_op_start.lnum - 1),
							   (linenr_t)(curbuf->b_op_end.lnum + 1)) == FAIL)
			return;

		for (lnum = curwin->w_cursor.lnum;
							   curwin->w_cursor.lnum <= curbuf->b_op_end.lnum;
													  ++curwin->w_cursor.lnum)
		{
			block_prep(&bd, curwin->w_cursor.lnum, TRUE);
			if (bd.textlen == 0)		/* nothing to delete */
				continue;

		/*
		 * If we delete a TAB, it may be replaced by several characters.
		 * Thus the number of characters may increase!
		 */
			n = bd.textlen - bd.startspaces - bd.endspaces;		/* number of chars deleted */
			oldp = ml_get_curline();
			newp = alloc_check((unsigned)STRLEN(oldp) + 1 - n);
			if (newp == NULL)
				continue;
		/* copy up to deleted part */
			vim_memmove(newp, oldp, (size_t)bd.textcol);
		/* insert spaces */
			copy_spaces(newp + bd.textcol, (size_t)(bd.startspaces + bd.endspaces));
		/* copy the part after the deleted part */
			oldp += bd.textcol + bd.textlen;
			vim_memmove(newp + bd.textcol + bd.startspaces + bd.endspaces,
													  oldp, STRLEN(oldp) + 1);
		/* replace the line */
			ml_replace(curwin->w_cursor.lnum, newp, FALSE);
		}

		curwin->w_cursor.lnum = lnum;
		adjust_cursor();

		CHANGED;
		updateScreen(VALID_TO_CURSCHAR);
		op_line_count = 0;		/* no lines deleted */
	}
	else if (op_motion_type == MLINE)
	{
		if (op_type == CHANGE)
		{
				/* Delete the lines except the first one.
				 * Temporarily move the cursor to the next line.
				 * Save the current line number, if the last line is deleted
				 * it may be changed.
				 */
			if (op_line_count > 1)
			{
				lnum = curwin->w_cursor.lnum;
				++curwin->w_cursor.lnum;
				dellines((long)(op_line_count - 1), TRUE, TRUE);
				curwin->w_cursor.lnum = lnum;
			}
			if (u_save_cursor() == FAIL)
				return;
			if (curbuf->b_p_ai)				/* don't delete indent */
			{
				beginline(TRUE);			/* put cursor on first non-white */
				did_ai = TRUE;				/* delete the indent when ESC hit */
			}
			truncate_line(FALSE);
			if (curwin->w_cursor.col > 0)
				--curwin->w_cursor.col;		/* put cursor on last char in line */
		}
		else
		{
			dellines(op_line_count, TRUE, TRUE);
		}
		u_clearline();	/* "U" command should not be possible after "dd" */
		beginline(TRUE);
	}
	else if (op_line_count == 1)		/* delete characters within one line */
	{
		if (u_save_cursor() == FAIL)
			return;
			/* if 'cpoptions' contains '$', display '$' at end of change */
		if (vim_strchr(p_cpo, CPO_DOLLAR) != NULL && op_type == CHANGE &&
			 curbuf->b_op_end.lnum == curwin->w_cursor.lnum && !op_is_VIsual)
			display_dollar(curbuf->b_op_end.col - !op_inclusive);
		n = curbuf->b_op_end.col - curbuf->b_op_start.col + 1 - !op_inclusive;
		while (n-- > 0)
			if (delchar(TRUE) == FAIL)
				break;
	}
	else								/* delete characters between lines */
	{
		if (u_save_cursor() == FAIL)			/* save first line for undo */
			return;
		truncate_line(TRUE);			/* delete from cursor to end of line */

		curbuf->b_op_start = curwin->w_cursor;	/* remember curwin->w_cursor */
		++curwin->w_cursor.lnum;
												/* includes save for undo */
		dellines((long)(op_line_count - 2), TRUE, TRUE);

		if (u_save_cursor() == FAIL)			/* save last line for undo */
			return;
		n = curbuf->b_op_end.col - !op_inclusive;
		curwin->w_cursor.col = 0;
		while (n-- >= 0)			/* delete from start of line until op_end */
			if (delchar(TRUE) == FAIL)
				break;
		curwin->w_cursor = curbuf->b_op_start;	/* restore curwin->w_cursor */
		(void)do_join(FALSE, curs_rows() == OK);
	}

	if ((op_motion_type == MCHAR && op_line_count == 1) || op_type == CHANGE)
	{
		if (dollar_vcol)
			must_redraw = 0;		/* don't want a redraw now */
		cursupdate();
		if (!dollar_vcol)
			updateline();
	}
	else if (!global_busy)			/* no need to update screen for :global */
		updateScreen(CURSUPD);

	msgmore(curbuf->b_ml.ml_line_count - old_lcount);

		/* correct op_end for deleted text (for "']" command) */
	if (op_block_mode)
		curbuf->b_op_end.col = curbuf->b_op_start.col;
	else
		curbuf->b_op_end = curbuf->b_op_start;
}

/*
 * do_tilde - handle the (non-standard vi) tilde operator
 */
	void
do_tilde()
{
	FPOS				pos;
	struct block_def	bd;

	if (u_save((linenr_t)(curbuf->b_op_start.lnum - 1),
							   (linenr_t)(curbuf->b_op_end.lnum + 1)) == FAIL)
		return;

	pos = curbuf->b_op_start;
	if (op_block_mode)					/* Visual block mode */
	{
		for (; pos.lnum <= curbuf->b_op_end.lnum; ++pos.lnum)
		{
			block_prep(&bd, pos.lnum, FALSE);
			pos.col = bd.textcol;
			while (--bd.textlen >= 0)
			{
				swapchar(&pos);
				if (inc(&pos) == -1)	/* at end of file */
					break;
			}
		}
	}
	else			/* not block mode */
	{
		if (op_motion_type == MLINE)
		{
				pos.col = 0;
				curbuf->b_op_end.col = STRLEN(ml_get(curbuf->b_op_end.lnum));
				if (curbuf->b_op_end.col)
						--curbuf->b_op_end.col;
		}
		else if (!op_inclusive)
			dec(&(curbuf->b_op_end));

		while (ltoreq(pos, curbuf->b_op_end))
		{
			swapchar(&pos);
			if (inc(&pos) == -1)	/* at end of file */
				break;
		}
	}

	if (op_motion_type == MCHAR && op_line_count == 1 && !op_block_mode)
	{
		cursupdate();
		updateline();
	}
	else
		updateScreen(CURSUPD);

	if (op_line_count > p_report)
			smsg((char_u *)"%ld line%s ~ed",
										op_line_count, plural(op_line_count));
}

/*
 * If op_type == UPPER: make uppercase,
 * if op_type == LOWER: make lowercase,
 * else swap case of character at 'pos'
 */
	void
swapchar(pos)
	FPOS	*pos;
{
	int		c;

	c = gchar(pos);
	if (islower(c) && op_type != LOWER)
	{
		pchar(*pos, toupper(c));
		CHANGED;
	}
	else if (isupper(c) && op_type != UPPER)
	{
		pchar(*pos, tolower(c));
		CHANGED;
	}
}

/*
 * do_change - handle a change operation
 * 
 * return TRUE if edit() returns because of a CTRL-O command
 */
	int
do_change()
{
	register colnr_t 		   l;

	l = curbuf->b_op_start.col;
	if (op_motion_type == MLINE)
	{
		l = 0;
		can_si = TRUE;		/* It's like opening a new line, do si */
	}

	if (!op_empty)
		do_delete();			/* delete the text and take care of undo */

	if ((l > curwin->w_cursor.col) && !lineempty(curwin->w_cursor.lnum))
		inc_cursor();

#ifdef LISPINDENT
	if (op_motion_type == MLINE)
	{
		if (curbuf->b_p_lisp && curbuf->b_p_ai)
			fixthisline(get_lisp_indent);
# ifdef CINDENT
		else if (curbuf->b_p_cin)
			fixthisline(get_c_indent);
# endif
	}
#endif

	op_type = NOP;			/* don't want op_type == CHANGED in Insert mode */
	return edit(NUL, FALSE, (linenr_t)1);
}

/*
 * set all the yank buffers to empty (called from main())
 */
	void
init_yank()
{
	register int i;

	for (i = 0; i < NUM_REGISTERS; ++i)
		y_buf[i].y_array = NULL;
}

/*
 * Free "n" lines from the current yank buffer.
 * Called for normal freeing and in case of error.
 */
	static void
free_yank(n)
	long n;
{
	if (y_current->y_array != NULL)
	{
		register long i;

		for (i = n; --i >= 0; )
		{
			if ((i & 1023) == 1023)					/* this may take a while */
			{
				/*
				 * This message should never cause a hit-return message.
				 * Overwrite this message with any next message.
				 */
				++no_wait_return;
				smsg((char_u *)"freeing %ld lines", i + 1);
				--no_wait_return;
				msg_didout = FALSE;
				msg_col = 0;
			}
			vim_free(y_current->y_array[i]);
		}
		vim_free(y_current->y_array);
		y_current->y_array = NULL;
		if (n >= 1000)
			MSG("");
	}
}

	static void
free_yank_all()
{
		free_yank(y_current->y_size);
}

/*
 * Yank the text between curwin->w_cursor and startpos into a yank buffer.
 * If we are to append ("uppercase), we first yank into a new yank buffer and
 * then concatenate the old and the new one (so we keep the old one in case
 * of out-of-memory).
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_yank(deleting, mess)
	int deleting;
	int mess;
{
	long 				i;				/* index in y_array[] */
	struct yankbuf		*curr;			/* copy of y_current */
	struct yankbuf		newbuf; 		/* new yank buffer when appending */
	char_u				**new_ptr;
	register linenr_t	lnum;			/* current line number */
	long 				j;
	int					yanktype = op_motion_type;
	long				yanklines = op_line_count;
	linenr_t			yankendlnum = curbuf->b_op_end.lnum;

	char_u				*pnew;
	struct block_def	bd;

									/* check for read-only buffer */
	if (yankbuffer != 0 && !is_yank_buffer(yankbuffer, TRUE))
	{
		beep_flush();
		return FAIL;
	}
	if (!deleting)					/* do_delete() already set y_current */
		get_yank_buffer(TRUE);

	curr = y_current;
									/* append to existing contents */
	if (yankappend && y_current->y_array != NULL)
		y_current = &newbuf;
	else
		free_yank_all();		/* free previously yanked lines */

/*
 * If the cursor was in column 1 before and after the movement, and the
 * operator is not inclusive, the yank is always linewise.
 */
	if (op_motion_type == MCHAR && curbuf->b_op_start.col == 0 &&
				  !op_inclusive && curbuf->b_op_end.col == 0 && yanklines > 1)
	{
		yanktype = MLINE;
		--yankendlnum;
		--yanklines;
	}

	y_current->y_size = yanklines;
	y_current->y_type = yanktype;	/* set the yank buffer type */
	y_current->y_array = (char_u **)lalloc((long_u)(sizeof(char_u *) *
															yanklines), TRUE);

	if (y_current->y_array == NULL)
	{
		y_current = curr;
		return FAIL;
	}

	i = 0;
	lnum = curbuf->b_op_start.lnum;

/*
 * Visual block mode
 */
	if (op_block_mode)
	{
		y_current->y_type = MBLOCK;	/* set the yank buffer type */
		for ( ; lnum <= yankendlnum; ++lnum)
		{
			block_prep(&bd, lnum, FALSE);

			if ((pnew = alloc(bd.startspaces + bd.endspaces +
										  bd.textlen + 1)) == NULL)
				goto fail;
			y_current->y_array[i++] = pnew;

			copy_spaces(pnew, (size_t)bd.startspaces);
			pnew += bd.startspaces;

			vim_memmove(pnew, bd.textstart, (size_t)bd.textlen);
			pnew += bd.textlen;

			copy_spaces(pnew, (size_t)bd.endspaces);
			pnew += bd.endspaces;

			*pnew = NUL;
		}
	}
	else
	{
/*
 * there are three parts for non-block mode:
 * 1. if yanktype != MLINE yank last part of the top line
 * 2. yank the lines between op_start and op_end, inclusive when
 *    yanktype == MLINE
 * 3. if yanktype != MLINE yank first part of the bot line
 */
		if (yanktype != MLINE)
		{
			if (yanklines == 1)		/* op_start and op_end on same line */
			{
				j = curbuf->b_op_end.col - curbuf->b_op_start.col +
															1 - !op_inclusive;
				if ((y_current->y_array[0] = strnsave(ml_get(lnum) +
									 curbuf->b_op_start.col, (int)j)) == NULL)
				{
fail:
					free_yank(i);	/* free the allocated lines */
					y_current = curr;
					return FAIL;
				}
				goto success;
			}
			if ((y_current->y_array[0] = strsave(ml_get(lnum++) +
											 curbuf->b_op_start.col)) == NULL)
				goto fail;
			++i;
		}

		while (yanktype == MLINE ? (lnum <= yankendlnum) : (lnum < yankendlnum))
		{
			if ((y_current->y_array[i] = strsave(ml_get(lnum++))) == NULL)
				goto fail;
			++i;
		}
		if (yanktype != MLINE)
		{
			if ((y_current->y_array[i] = strnsave(ml_get(yankendlnum),
						   curbuf->b_op_end.col + 1 - !op_inclusive)) == NULL)
				goto fail;
		}
	}

success:
	if (curr != y_current)		/* append the new block to the old block */
	{
		new_ptr = (char_u **)lalloc((long_u)(sizeof(char_u *) *
								   (curr->y_size + y_current->y_size)), TRUE);
		if (new_ptr == NULL)
			goto fail;
		for (j = 0; j < curr->y_size; ++j)
			new_ptr[j] = curr->y_array[j];
		vim_free(curr->y_array);
		curr->y_array = new_ptr;

		if (yanktype == MLINE) 	/* MLINE overrides MCHAR and MBLOCK */
			curr->y_type = MLINE;

		/* concatenate the last line of the old block with the first line of
		 * the new block */
		if (curr->y_type == MCHAR)
		{
			pnew = lalloc((long_u)(STRLEN(curr->y_array[curr->y_size - 1])
							  + STRLEN(y_current->y_array[0]) + 1), TRUE);
			if (pnew == NULL)
			{
					i = y_current->y_size - 1;
					goto fail;
			}
			STRCPY(pnew, curr->y_array[--j]);
			STRCAT(pnew, y_current->y_array[0]);
			vim_free(curr->y_array[j]);
			vim_free(y_current->y_array[0]);
			curr->y_array[j++] = pnew;
			i = 1;
		}
		else
			i = 0;
		while (i < y_current->y_size)
			curr->y_array[j++] = y_current->y_array[i++];
		curr->y_size = j;
		vim_free(y_current->y_array);
		y_current = curr;
	}
	if (mess)					/* Display message about yank? */
	{
		if (yanktype == MCHAR && !op_block_mode)
			--yanklines;
		if (yanklines > p_report)
		{
			cursupdate();		/* redisplay now, so message is not deleted */
			smsg((char_u *)"%ld line%s yanked", yanklines, plural(yanklines));
		}
	}

	return OK;
}

/*
 * put contents of register into the text
 * For ":put" command count == -1.
 */
	void
do_put(dir, count, fix_indent)
	int		dir;				/* BACKWARD for 'P', FORWARD for 'p' */
	long	count;
	int		fix_indent;			/* make indent look nice */
{
	char_u		*ptr;
	char_u		*newp, *oldp;
	int 		yanklen;
	int			oldlen;
	int			totlen = 0;					/* init for gcc */
	linenr_t	lnum;
	colnr_t		col;
	long 		i;							/* index in y_array[] */
	int 		y_type;
	long 		y_size;
	char_u		**y_array;
	long 		nr_lines = 0;
	colnr_t		vcol;
	int			delcount;
	int			incr = 0;
	long		j;
	FPOS		new_cursor;
	int			indent;
	int			orig_indent = 0;			/* init for gcc */
	int			indent_diff = 0;			/* init for gcc */
	int			first_indent = TRUE;
	FPOS		old_pos;
	struct block_def bd;
	char_u		*insert_string = NULL;

#ifdef USE_GUI
	if (yankbuffer == '*')
		gui_get_selection();
#endif

	if (fix_indent)
		orig_indent = get_indent();

	curbuf->b_op_start = curwin->w_cursor;		/* default for "'[" command */
	if (dir == FORWARD)
		curbuf->b_op_start.col++;
	curbuf->b_op_end = curwin->w_cursor;		/* default for "']" command */

	/*
	 * Using inserted text works differently, because the buffer includes
	 * special characters (newlines, etc.).
	 */
	if (yankbuffer == '.')
	{
		(void)stuff_inserted((dir == FORWARD ? (count == -1 ? 'o' : 'a') :
									(count == -1 ? 'O' : 'i')), count, FALSE);
		return;
	}

	/*
	 * For '%' (file name) and ':' (last command line) we have to create a
	 * fake yank buffer.
	 */
	if (yankbuffer == '%')				/* use file name */
	{
		if (check_fname() == FAIL)
			return;
		insert_string = curbuf->b_xfilename;
	}
	else if (yankbuffer == ':')			/* use last command line */
	{
		if (last_cmdline == NULL)
		{
			EMSG(e_nolastcmd);
			return;
		}
		insert_string = last_cmdline;
	}

	if (insert_string != NULL)
	{
		y_type = MCHAR;					/* use fake one-line yank buffer */
		y_size = 1;
		y_array = &insert_string;
	}
	else
	{
		get_yank_buffer(FALSE);

		y_type = y_current->y_type;
		y_size = y_current->y_size;
		y_array = y_current->y_array;
	}

	if (count == -1)		/* :put command */
	{
		y_type = MLINE;
		count = 1;
	}

	if (y_size == 0 || y_array == NULL)
	{
		EMSG2("Nothing in register %s",
					yankbuffer == 0 ? (char_u *)"\"" : transchar(yankbuffer));
		return;
	}

	if (y_type == MBLOCK)
	{
		lnum = curwin->w_cursor.lnum + y_size + 1;
		if (lnum > curbuf->b_ml.ml_line_count)
			lnum = curbuf->b_ml.ml_line_count + 1;
		if (u_save(curwin->w_cursor.lnum - 1, lnum) == FAIL)
			return;
	}
	else if (u_save_cursor() == FAIL)
		return;

	yanklen = STRLEN(y_array[0]);
	CHANGED;

	lnum = curwin->w_cursor.lnum;
	col = curwin->w_cursor.col;

/*
 * block mode
 */
	if (y_type == MBLOCK)
	{
		if (dir == FORWARD && gchar_cursor() != NUL)
		{
			getvcol(curwin, &curwin->w_cursor, NULL, NULL, &col);
			++col;
			++curwin->w_cursor.col;
		}
		else
			getvcol(curwin, &curwin->w_cursor, &col, NULL, NULL);
		for (i = 0; i < y_size; ++i)
		{
			bd.startspaces = 0;
			bd.endspaces = 0;
			bd.textcol = 0;
			vcol = 0;
			delcount = 0;

		/* add a new line */
			if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
			{
				ml_append(curbuf->b_ml.ml_line_count, (char_u *)"",
														   (colnr_t)1, FALSE);
				++nr_lines;
			}
			oldp = ml_get_curline();
			oldlen = STRLEN(oldp);
			for (ptr = oldp; vcol < col && *ptr; ++ptr)
			{
				/* Count a tab for what it's worth (if list mode not on) */
				incr = lbr_chartabsize(ptr, (colnr_t)vcol);
				vcol += incr;
				++bd.textcol;
			}
			if (vcol < col)	/* line too short, padd with spaces */
			{
				bd.startspaces = col - vcol;
			}
			else if (vcol > col)
			{
				bd.endspaces = vcol - col;
				bd.startspaces = incr - bd.endspaces;
				--bd.textcol;
				delcount = 1;
			}
			yanklen = STRLEN(y_array[i]);
			totlen = count * yanklen + bd.startspaces + bd.endspaces;
			newp = alloc_check((unsigned)totlen + oldlen + 1);
			if (newp == NULL)
				break;
		/* copy part up to cursor to new line */
			ptr = newp;
			vim_memmove(ptr, oldp, (size_t)bd.textcol);
			ptr += bd.textcol;
		/* may insert some spaces before the new text */
			copy_spaces(ptr, (size_t)bd.startspaces);
			ptr += bd.startspaces;
		/* insert the new text */
			for (j = 0; j < count; ++j)
			{
				vim_memmove(ptr, y_array[i], (size_t)yanklen);
				ptr += yanklen;
			}
		/* may insert some spaces after the new text */
			copy_spaces(ptr, (size_t)bd.endspaces);
			ptr += bd.endspaces;
		/* move the text after the cursor to the end of the line. */
			vim_memmove(ptr, oldp + bd.textcol + delcount,
								(size_t)(oldlen - bd.textcol - delcount + 1));
			ml_replace(curwin->w_cursor.lnum, newp, FALSE);

			++curwin->w_cursor.lnum;
			if (i == 0)
				curwin->w_cursor.col += bd.startspaces;
		}
												/* for "']" command */
		curbuf->b_op_end.lnum = curwin->w_cursor.lnum - 1;
		curbuf->b_op_end.col = bd.textcol + totlen - 1;
		curwin->w_cursor.lnum = lnum;
		cursupdate();
		updateScreen(VALID_TO_CURSCHAR);
	}
	else		/* not block mode */
	{
		if (y_type == MCHAR)
		{
	/* if type is MCHAR, FORWARD is the same as BACKWARD on the next char */
			if (dir == FORWARD && gchar_cursor() != NUL)
			{
				++col;
				if (yanklen)
				{
					++curwin->w_cursor.col;
					++curbuf->b_op_end.col;
				}
			}
			new_cursor = curwin->w_cursor;
		}
		else if (dir == BACKWARD)
	/* if type is MLINE, BACKWARD is the same as FORWARD on the previous line */
			--lnum;

/*
 * simple case: insert into current line
 */
		if (y_type == MCHAR && y_size == 1)
		{
			totlen = count * yanklen;
			if (totlen)
			{
				oldp = ml_get(lnum);
				newp = alloc_check((unsigned)(STRLEN(oldp) + totlen + 1));
				if (newp == NULL)
					return; 			/* alloc() will give error message */
				vim_memmove(newp, oldp, (size_t)col);
				ptr = newp + col;
				for (i = 0; i < count; ++i)
				{
					vim_memmove(ptr, y_array[0], (size_t)yanklen);
					ptr += yanklen;
				}
				vim_memmove(ptr, oldp + col, STRLEN(oldp + col) + 1);
				ml_replace(lnum, newp, FALSE);
										  /* put cursor on last putted char */
				curwin->w_cursor.col += (colnr_t)(totlen - 1);
			}
			curbuf->b_op_end = curwin->w_cursor;
			updateline();
		}
		else
		{
			while (--count >= 0)
			{
				i = 0;
				if (y_type == MCHAR)
				{
					/*
					 * Split the current line in two at the insert position.
					 * First insert y_array[size - 1] in front of second line.
					 * Then append y_array[0] to first line.
					 */
					ptr = ml_get(lnum) + col;
					totlen = STRLEN(y_array[y_size - 1]);
					newp = alloc_check((unsigned)(STRLEN(ptr) + totlen + 1));
					if (newp == NULL)
						goto error;
					STRCPY(newp, y_array[y_size - 1]);
					STRCAT(newp, ptr);
						/* insert second line */
					ml_append(lnum, newp, (colnr_t)0, FALSE);
					vim_free(newp);

					oldp = ml_get(lnum);
					newp = alloc_check((unsigned)(col + yanklen + 1));
					if (newp == NULL)
						goto error;
											/* copy first part of line */
					vim_memmove(newp, oldp, (size_t)col);
											/* append to first line */
					vim_memmove(newp + col, y_array[0], (size_t)(yanklen + 1));
					ml_replace(lnum, newp, FALSE);

					curwin->w_cursor.lnum = lnum;
					i = 1;
				}

				while (i < y_size)
				{
					if ((y_type != MCHAR || i < y_size - 1) &&
						ml_append(lnum, y_array[i], (colnr_t)0, FALSE) == FAIL)
							goto error;
					lnum++;
					i++;
					if (fix_indent)
					{
						old_pos = curwin->w_cursor;
						curwin->w_cursor.lnum = lnum;
						ptr = ml_get(lnum);
#if defined(SMARTINDENT) || defined(CINDENT)
						if (*ptr == '#'
# ifdef SMARTINDENT
						   && curbuf->b_p_si
# endif
# ifdef CINDENT
						   && curbuf->b_p_cin && in_cinkeys('#', ' ', TRUE)
# endif
											)
					
							indent = 0;     /* Leave # lines at start */
						else
#endif
						     if (*ptr == NUL)
							indent = 0;     /* Ignore empty lines */
						else if (first_indent)
						{
							indent_diff = orig_indent - get_indent();
							indent = orig_indent;
							first_indent = FALSE;
						}
						else if ((indent = get_indent() + indent_diff) < 0)
							indent = 0;
						set_indent(indent, TRUE);
						curwin->w_cursor = old_pos;
					}
					++nr_lines;
				}
			}

			/* put '] at last inserted character */
			curbuf->b_op_end.lnum = lnum;
			col = STRLEN(y_array[y_size - 1]);
			if (col > 1)
				curbuf->b_op_end.col = col - 1;
			else
				curbuf->b_op_end.col = 0;

			if (y_type == MLINE)
			{
				curwin->w_cursor.col = 0;
				if (dir == FORWARD)
				{
					updateScreen(NOT_VALID);	/* recomp. curwin->w_botline */
					++curwin->w_cursor.lnum;
				}
					/* put cursor on first non-blank in last inserted line */
				beginline(TRUE);
			}
			else		/* put cursor on first inserted character */
			{
				curwin->w_cursor = new_cursor;
			}

error:
			if (y_type == MLINE)		/* for '[ */
			{
				curbuf->b_op_start.col = 0;
				if (dir == FORWARD)
					curbuf->b_op_start.lnum++;
			}
			mark_adjust(curbuf->b_op_start.lnum + (y_type == MCHAR),
													   MAXLNUM, nr_lines, 0L);
			updateScreen(CURSUPD);
		}
	}

	msgmore(nr_lines);
	curwin->w_set_curswant = TRUE;
}

/* Return the character name of the register with the given number */
	int
get_register_name(num)
	int num;
{
	if (num == -1)
		return '"';
	else if (num < 10)
		return num + '0';
	else if (num == DELETION_REGISTER)
		return '-';
#ifdef USE_GUI
	else if (num == GUI_SELECTION_REGISTER)
		return '*';
#endif
	else
		return num + 'a' - 10;
}

/*
 * display the contents of the yank buffers
 */
	void
do_dis(arg)
	char_u *arg;
{
	register int			i, n;
	register long			j;
	register char_u			*p;
	register struct yankbuf *yb;
	char_u name;
  
	if (arg != NULL && *arg == NUL)
		arg = NULL;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\n--- Registers ---");
	stop_highlight();
	for (i = -1; i < NUM_REGISTERS; ++i)
	{
		if (i == -1)
		{
			if (y_previous != NULL)
				yb = y_previous;
			else
				yb = &(y_buf[0]);
		}
		else
			yb = &(y_buf[i]);
		name = get_register_name(i);
		if (yb->y_array != NULL && (arg == NULL ||
											   vim_strchr(arg, name) != NULL))
		{
			msg_outchar('\n');
			msg_outchar('"');
			msg_outchar(name);
			MSG_OUTSTR("   ");

			n = (int)Columns - 6;
			for (j = 0; j < yb->y_size && n > 1; ++j)
			{
				if (j)
				{
					MSG_OUTSTR("^J");
					n -= 2;
				}
				for (p = yb->y_array[j]; *p && (n -= charsize(*p)) >= 0; ++p)
					msg_outtrans_len(p, 1);
			}
			if (n > 1 && yb->y_type == MLINE)
				MSG_OUTSTR("^J");
			flushbuf();				/* show one line at a time */
		}
	}

	/*
	 * display last inserted text
	 */
	if ((p = get_last_insert()) != NULL &&
		(arg == NULL || vim_strchr(arg, '.') != NULL))
	{
		MSG_OUTSTR("\n\".   ");
		dis_msg(p, TRUE);
	}

	/*
	 * display last command line
	 */
	if (last_cmdline != NULL && (arg == NULL || vim_strchr(arg, ':') != NULL))
	{
		MSG_OUTSTR("\n\":   ");
		dis_msg(last_cmdline, FALSE);
	}

	/*
	 * display current file name
	 */
	if (curbuf->b_xfilename != NULL &&
								(arg == NULL || vim_strchr(arg, '%') != NULL))
	{
		MSG_OUTSTR("\n\"%   ");
		dis_msg(curbuf->b_xfilename, FALSE);
	}
}

/*
 * display a string for do_dis()
 * truncate at end of screen line
 */
	void
dis_msg(p, skip_esc)
	char_u		*p;
	int			skip_esc;			/* if TRUE, ignore trailing ESC */
{
	int		n;

	n = (int)Columns - 6;
	while (*p && !(*p == ESC && skip_esc && *(p + 1) == NUL) &&
						(n -= charsize(*p)) >= 0)
		msg_outtrans_len(p++, 1);
}

/*
 * join 'count' lines (minimal 2), including u_save()
 */
	void
do_do_join(count, insert_space, redraw)
	long	count;
	int		insert_space;
	int		redraw;					/* can redraw, curwin->w_col valid */
{
	if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
					(linenr_t)(curwin->w_cursor.lnum + count)) == FAIL)
		return;

	if (count > 10)
		redraw = FALSE;				/* don't redraw each small change */
	while (--count > 0)
	{
		line_breakcheck();
		if (got_int || do_join(insert_space, redraw) == FAIL)
		{
			beep_flush();
			break;
		}
	}
	if (redraw)
		redraw_later(VALID_TO_CURSCHAR);
	else
		redraw_later(NOT_VALID);
	
	/*
	 * Need to update the screen if the line where the cursor is became too
	 * long to fit on the screen.
	 */
	cursupdate();
}

/*
 * Join two lines at the cursor position.
 *
 * return FAIL for failure, OK ohterwise
 */
	int
do_join(insert_space, redraw)
	int			insert_space;
	int			redraw;		/* should only be TRUE when curwin->w_row valid */
{
	char_u		*curr;
	char_u		*next;
	char_u		*newp;
	int			endcurr1, endcurr2;
	int 		currsize;		/* size of the current line */
	int 		nextsize;		/* size of the next line */
	int			spaces;			/* number of spaces to insert */
	int			rows_to_del = 0;/* number of rows on screen to delete */
	linenr_t	t;

	if (curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count)
		return FAIL;			/* can't join on last line */

	if (redraw)
		rows_to_del = plines_m(curwin->w_cursor.lnum,
												   curwin->w_cursor.lnum + 1);

	curr = ml_get_curline();
	currsize = STRLEN(curr);
	endcurr1 = endcurr2 = NUL;
	if (currsize > 0)
	{
		endcurr1 = *(curr + currsize - 1);
		if (currsize > 1)
			endcurr2 = *(curr + currsize - 2);
	}

	next = ml_get((linenr_t)(curwin->w_cursor.lnum + 1));
	spaces = 0;
	if (insert_space)
	{
		next = skipwhite(next);
		spaces = 1;
		if (*next == ')' || currsize == 0)
			spaces = 0;
		else
		{
			if (endcurr1 == ' ' || endcurr1 == TAB)
			{
				spaces = 0;
				if (currsize > 1)
					endcurr1 = endcurr2;
			}
			if (p_js && vim_strchr((char_u *)".!?", endcurr1) != NULL)
				spaces = 2;
		}
	}
	nextsize = STRLEN(next);

	newp = alloc_check((unsigned)(currsize + nextsize + spaces + 1));
	if (newp == NULL)
		return FAIL;

	/*
	 * Insert the next line first, because we already have that pointer.
	 * Curr has to be obtained again, because getting next will have
	 * invalidated it.
	 */
	vim_memmove(newp + currsize + spaces, next, (size_t)(nextsize + 1));

	curr = ml_get_curline();
	vim_memmove(newp, curr, (size_t)currsize);

	copy_spaces(newp + currsize, (size_t)spaces);

	ml_replace(curwin->w_cursor.lnum, newp, FALSE);

	/*
	 * Delete the following line. To do this we move the cursor there
	 * briefly, and then move it back. After dellines() the cursor may
	 * have moved up (last line deleted), so the current lnum is kept in t.
	 */
	t = curwin->w_cursor.lnum;
	++curwin->w_cursor.lnum;
	dellines(1L, FALSE, FALSE);
	curwin->w_cursor.lnum = t;

	/*
	 * the number of rows on the screen is reduced by the difference
	 * in number of rows of the two old lines and the one new line
	 */
	if (redraw)
	{
		rows_to_del -= plines(curwin->w_cursor.lnum);
		if (rows_to_del > 0)
			win_del_lines(curwin, curwin->w_cline_row + curwin->w_cline_height,
													 rows_to_del, TRUE, TRUE);
	}

 	/*
	 * go to first character of the joined line
	 */
	if (currsize == 0)
		curwin->w_cursor.col = 0;
	else
	{
		curwin->w_cursor.col = currsize - 1;
		(void)oneright();
	}
	CHANGED;

	return OK;
}

/*
 * Return TRUE if the two comment leaders given are the same.  The cursor is
 * in the first line.  White-space is ignored.  Note that the whole of
 * 'leader1' must match 'leader2_len' characters from 'leader2' -- webb
 */
	static int
same_leader(leader1_len, leader1_flags, leader2_len, leader2_flags)
	int		leader1_len;
	char_u	*leader1_flags;
	int		leader2_len;
	char_u	*leader2_flags;
{
	int		idx1 = 0, idx2 = 0;
	char_u	*p;
	char_u	*line1;
	char_u	*line2;

	if (leader1_len == 0)
		return (leader2_len == 0);

	/*
	 * If first leader has 'f' flag, the lines can be joined only if the
	 * second line does not have a leader.
	 * If first leader has 'e' flag, the lines can never be joined.
	 * If fist leader has 's' flag, the lines can only be joined if there is
	 * some text after it and the second line has the 'm' flag.
	 */
	if (leader1_flags != NULL)
	{
		for (p = leader1_flags; *p && *p != ':'; ++p)
		{
			if (*p == COM_FIRST)
				return (leader2_len == 0);
			if (*p == COM_END)
				return FALSE;
			if (*p == COM_START)
			{
				if (*(ml_get_curline() + leader1_len) == NUL)
					return FALSE;
				if (leader2_flags == NULL || leader2_len == 0)
					return FALSE;
				for (p = leader2_flags; *p && *p != ':'; ++p)
					if (*p == COM_MIDDLE)
						return TRUE;
				return FALSE;
			}
		}
	}

	/*
	 * Get current line and next line, compare the leaders.
	 * The first line has to be saved, only one line can be locked at a time.
	 */
	line1 = strsave(ml_get_curline());
	if (line1 != NULL)
	{
		for (idx1 = 0; vim_iswhite(line1[idx1]); ++idx1)
			;
		line2 = ml_get(curwin->w_cursor.lnum + 1);
		for (idx2 = 0; idx2 < leader2_len; ++idx2)
		{
			if (!vim_iswhite(line2[idx2]))
			{
				if (line1[idx1++] != line2[idx2])
					break;
			}
			else
				while (vim_iswhite(line1[idx1]))
					++idx1;
		}
		vim_free(line1);
	}
	return (idx2 == leader2_len && idx1 == leader1_len);
}

/*
 * implementation of the format operator 'Q'
 */
	void
do_format()
{
	long		old_line_count = curbuf->b_ml.ml_line_count;
	int			prev_is_blank = FALSE;
	int			is_end_block = TRUE;
	int			next_is_end_block;
	int			leader_len = 0;		/* init for gcc */
	int			next_leader_len;
	char_u		*leader_flags = NULL;
	char_u		*next_leader_flags;
	int			advance = TRUE;
	int			second_indent = -1;
	int			do_second_indent;
	int			first_par_line = TRUE;

	if (u_save((linenr_t)(curwin->w_cursor.lnum - 1),
				   (linenr_t)(curwin->w_cursor.lnum + op_line_count)) == FAIL)
		return;

	/* check for 'q' and '2' in 'formatoptions' */
	fo_do_comments = has_format_option(FO_Q_COMS);
	do_second_indent = has_format_option(FO_Q_SECOND);

	/*
	 * get info about the previous and current line.
	 */
	if (curwin->w_cursor.lnum > 1)
		is_end_block = fmt_end_block(curwin->w_cursor.lnum - 1,
										&next_leader_len, &next_leader_flags);
	next_is_end_block = fmt_end_block(curwin->w_cursor.lnum,
										&next_leader_len, &next_leader_flags);

	curwin->w_cursor.lnum--;
	while (--op_line_count >= 0)
	{
		/*
		 * Advance to next block.
		 */
		if (advance)
		{
			curwin->w_cursor.lnum++;
			prev_is_blank = is_end_block;
			is_end_block = next_is_end_block;
			leader_len = next_leader_len;
			leader_flags = next_leader_flags;
		}

		/*
		 * The last line to be formatted.
		 */
		if (op_line_count == 0)
		{
			next_is_end_block = TRUE;
			next_leader_len = 0;
			next_leader_flags = NULL;
		}
		else
			next_is_end_block = fmt_end_block(curwin->w_cursor.lnum + 1,
										&next_leader_len, &next_leader_flags);
		advance = TRUE;

		/*
		 * For the first line of a paragraph, check indent of second line.
		 * Don't do this for comments and empty lines.
		 */
		if (first_par_line && do_second_indent &&
				prev_is_blank && !is_end_block &&
				curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count &&
				leader_len == 0 && next_leader_len == 0 &&
				!lineempty(curwin->w_cursor.lnum + 1))
			second_indent = get_indent_lnum(curwin->w_cursor.lnum + 1);

		/*
		 * Skip end-of-block (blank) lines
		 */
		if (is_end_block)
		{
		}
		/*
		 * If we have got to the end of a paragraph, format it.
		 */
		else if (next_is_end_block || !same_leader(leader_len, leader_flags,
										  next_leader_len, next_leader_flags))
		{
			/* replace indent in first line with minimal number of tabs and
			 * spaces, according to current options */
			set_indent(get_indent(), TRUE);

			/* put cursor on last non-space */
			coladvance(MAXCOL);
			while (curwin->w_cursor.col && vim_isspace(gchar_cursor()))
				dec_cursor();
			curs_columns(FALSE);			/* update curwin->w_virtcol */

			/* do the formatting */
			State = INSERT;		/* for Opencmd() */
			insertchar(NUL, TRUE, second_indent);
			State = NORMAL;
			first_par_line = TRUE;
			second_indent = -1;
		}
		else
		{
			/*
			 * Still in same paragraph, so join the lines together.
			 * But first delete the comment leader from the second line.
			 */
			advance = FALSE;
			curwin->w_cursor.lnum++;
			curwin->w_cursor.col = 0;
			while (next_leader_len--)
				delchar(FALSE);
			curwin->w_cursor.lnum--;
			if (do_join(TRUE, FALSE) == FAIL)
			{
				beep_flush();
				break;
			}
			first_par_line = FALSE;
		}
	}
	fo_do_comments = FALSE;
	/*
	 * Leave the cursor at the first non-blank of the last formatted line.
	 * If the cursor was move one line back (e.g. with "Q}") go to the next
	 * line, so "." will do the next lines.
	 */
	if (op_end_adjusted && curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
		++curwin->w_cursor.lnum;
	beginline(TRUE);
	updateScreen(NOT_VALID);
	msgmore(curbuf->b_ml.ml_line_count - old_line_count);
}

/*
 * Blank lines, and lines containing only the comment leader, are left
 * untouched by the formatting.  The function returns TRUE in this
 * case.  It also returns TRUE when a line starts with the end of a comment
 * ('e' in comment flags), so that this line is skipped, and not joined to the
 * previous line.  A new paragraph starts after a blank line, or when the
 * comment leader changes -- webb.
 */
	static int
fmt_end_block(lnum, leader_len, leader_flags)
	linenr_t	lnum;
	int			*leader_len;
	char_u		**leader_flags;
{
	char_u		*flags = NULL;		/* init for GCC */
	char_u		*ptr;

	ptr = ml_get(lnum);
	*leader_len = get_leader_len(ptr, leader_flags);

	if (*leader_len > 0)
	{
		/*
		 * Search for 'e' flag in comment leader flags.
		 */
		flags = *leader_flags;
		while (*flags && *flags != ':' && *flags != COM_END)
			++flags;
	}

	return (ptr[*leader_len] == NUL ||
			(*leader_len > 0 && *flags == COM_END) ||
			 startPS(lnum, NUL, FALSE));
}

/*
 * prepare a few things for block mode yank/delete/tilde
 *
 * for delete:
 * - textlen includes the first/last char to be (partly) deleted
 * - start/endspaces is the number of columns that are taken by the
 *	 first/last deleted char minus the number of columns that have to be deleted.
 * for yank and tilde:
 * - textlen includes the first/last char to be wholly yanked
 * - start/endspaces is the number of columns of the first/last yanked char
 *   that are to be yanked.
 */
	static void
block_prep(bd, lnum, is_del)
	struct block_def	*bd;
	linenr_t			lnum;
	int					is_del;
{
	colnr_t		vcol;
	int			incr = 0;
	char_u		*pend;
	char_u		*pstart;

	bd->startspaces = 0;
	bd->endspaces = 0;
	bd->textlen = 0;
	bd->textcol = 0;
	vcol = 0;
	pstart = ml_get(lnum);
	while (vcol < op_start_vcol && *pstart)
	{
		/* Count a tab for what it's worth (if list mode not on) */
		incr = lbr_chartabsize(pstart, (colnr_t)vcol);
		vcol += incr;
		++pstart;
		++bd->textcol;
	}
	if (vcol < op_start_vcol)	/* line too short */
	{
		if (!is_del)
			bd->endspaces = op_end_vcol - op_start_vcol + 1;
	}
	else /* vcol >= op_start_vcol */
	{
		bd->startspaces = vcol - op_start_vcol;
		if (is_del && vcol > op_start_vcol)
			bd->startspaces = incr - bd->startspaces;
		pend = pstart;
		if (vcol > op_end_vcol)		/* it's all in one character */
		{
			bd->startspaces = op_end_vcol - op_start_vcol + 1;
			if (is_del)
				bd->startspaces = incr - bd->startspaces;
		}
		else
		{
			while (vcol <= op_end_vcol && *pend)
			{
				/* Count a tab for what it's worth (if list mode not on) */
				incr = lbr_chartabsize(pend, (colnr_t)vcol);
				vcol += incr;
				++pend;
			}
			if (vcol < op_end_vcol && !is_del)	/* line too short */
			{
				bd->endspaces = op_end_vcol - vcol;
			}
			else if (vcol > op_end_vcol)
			{
				bd->endspaces = vcol - op_end_vcol - 1;
				if (!is_del && pend != pstart && bd->endspaces)
					--pend;
			}
		}
		if (is_del && bd->startspaces)
		{
			--pstart;
			--bd->textcol;
		}
		bd->textlen = (int)(pend - pstart);
	}
	bd->textstart = pstart;
}

#define NUMBUFLEN 30

/*
 * add or subtract 'Prenum1' from a number in a line
 * 'command' is CTRL-A for add, CTRL-X for subtract
 *
 * return FAIL for failure, OK otherwise
 */
	int
do_addsub(command, Prenum1)
	int			command;
	linenr_t	Prenum1;
{
	register int 	col;
	char_u			buf[NUMBUFLEN];
	int				hex;			/* 'X': hexadecimal; '0': octal */
	static int		hexupper = FALSE;	/* 0xABC */
	long			n;
	char_u			*ptr;
	int				i;
	int				c;
	int				zeros = 0;		/* number of leading zeros */
	int				digits = 0;		/* number of digits in the number */

	ptr = ml_get_curline();
	col = curwin->w_cursor.col;

		/* first check if we are on a hexadecimal number */
	while (col > 0 && isxdigit(ptr[col]))
		--col;
	if (col > 0 && (ptr[col] == 'X' || ptr[col] == 'x') &&
						ptr[col - 1] == '0' && isxdigit(ptr[col + 1]))
		--col;		/* found hexadecimal number */
	else
	{
		/* first search forward and then backward for start of number */
		col = curwin->w_cursor.col;

		while (ptr[col] != NUL && !isdigit(ptr[col]))
			++col;

		while (col > 0 && isdigit(ptr[col - 1]))
			--col;
	}

	if (isdigit(ptr[col]) && u_save_cursor() == OK)
	{
		ptr = ml_get_curline();					/* get it again, because
												   u_save may have changed it */
		curwin->w_set_curswant = TRUE;

		hex = 0;								/* default is decimal */
		if (ptr[col] == '0')					/* could be hex or octal */
		{
			hex = TO_UPPER(ptr[col + 1]);		/* assume hexadecimal */
			if (hex != 'X' || !isxdigit(ptr[col + 2]))
			{
				if (isdigit(hex))
					hex = '0';					/* octal */
				else
					hex = 0;					/* 0 by itself is decimal */
			}
		}

		if (!hex && col > 0 && ptr[col - 1] == '-')
			--col;

		ptr += col;
		/*
		 * we copy the number into a buffer because some versions of sscanf
		 * cannot handle characters with the upper bit set, making some special
		 * characters handled like digits.
		 */
		for (i = 0; *ptr && !(*ptr & 0x80) && i < NUMBUFLEN - 1; ++i)
			buf[i] = *ptr++;
		buf[i] = NUL;

		if (hex == '0')
			sscanf((char *)buf, "%lo", &n);
		else if (hex)
			sscanf((char *)buf + 2, "%lx", &n);	/* "%X" doesn't work! */
		else
			n = atol((char *)buf);

		if (command == Ctrl('A'))
			n += Prenum1;
		else
			n -= Prenum1;

		if (hex == 'X')					/* skip the '0x' */
			col += 2;
		else if (hex == '0')
			col++;						/* skip the '0' */
		curwin->w_cursor.col = col;

		c = gchar_cursor();
		do								/* delete the old number */
		{
			if (digits == 0 && c == '0')
				++zeros;				/* count the number of leading zeros */
			else
				++digits;				/* count the number of digits */
			if (isalpha(c))
			{
				if (isupper(c))
					hexupper = TRUE;
				else
					hexupper = FALSE;
			}
			(void)delchar(FALSE);
			c = gchar_cursor();
		}
		while (hex ? (hex == '0' ? c >= '0' && c <= '7' :
										isxdigit(c)) : isdigit(c));

		if (hex == 0)
			sprintf((char *)buf, "%ld", n);
		else
		{
			if (hex == '0')
				sprintf((char *)buf, "%lo", n);
			else if (hex && hexupper)
				sprintf((char *)buf, "%lX", n);
			else if (hex)
				sprintf((char *)buf, "%lx", n);
			/* adjust number of zeros to the new number of digits, so the
			 * total length of the number remains the same */
			if (zeros)
			{
				zeros += digits - STRLEN(buf);
				if (zeros > 0)
				{
					vim_memmove(buf + zeros, buf, STRLEN(buf) + 1);
					for (col = 0; zeros > 0; --zeros)
						buf[col++] = '0';
				}
			}
		}
		ins_str(buf);					/* insert the new number */
		--curwin->w_cursor.col;
		updateline();
		return OK;
	}
	else
	{
		beep_flush();
		return FAIL;
	}
}

#ifdef VIMINFO
	int
read_viminfo_register(line, fp, force)
	char_u	*line;
	FILE	*fp;
	int		force;
{
	int		eof;
	int		do_it = TRUE;
	int		size;
	int		limit;
	int		i;
	int		set_prev = FALSE;
	char_u	*str;
	char_u	**array = NULL;

	/* We only get here (hopefully) if line[0] == '"' */
	str = line + 1;
	if (*str == '"')
	{
		set_prev = TRUE;
		str++;
	}
	if (!isalnum(*str) && *str != '-')
	{
		if (viminfo_error("Illegal register name", line))
			return TRUE;		/* too many errors, pretend end-of-file */
		do_it = FALSE;
	}
	yankbuffer = *str++;
	get_yank_buffer(FALSE);
	yankbuffer = 0;
	if (!force && y_current->y_array != NULL)
		do_it = FALSE;
	size = 0;
	limit = 100;		/* Optimized for registers containing <= 100 lines */
	if (do_it)
	{
		if (set_prev)
			y_previous = y_current;
		vim_free(y_current->y_array);
		array = y_current->y_array =
					   (char_u **)alloc((unsigned)(limit * sizeof(char_u *)));
		str = skipwhite(str);
		if (STRNCMP(str, "CHAR", 4) == 0)
			y_current->y_type = MCHAR;
		else if (STRNCMP(str, "BLOCK", 5) == 0)
			y_current->y_type = MBLOCK;
		else
			y_current->y_type = MLINE;
	}
	while (!(eof = vim_fgets(line, LSIZE, fp)) && line[0] == TAB)
	{
		if (do_it)
		{
			if (size >= limit)
			{
				y_current->y_array = (char_u **)
							  alloc((unsigned)(limit * 2 * sizeof(char_u *)));
				for (i = 0; i < limit; i++)
					y_current->y_array[i] = array[i];
				vim_free(array);
				limit *= 2;
				array = y_current->y_array;
			}
			viminfo_readstring(line);
			str = strsave(line + 1);
			if (str != NULL)
				array[size++] = str;
			else
				do_it = FALSE;
		}
	}
	if (do_it)
	{
		if (size == 0)
		{
			vim_free(array);
			y_current->y_array = NULL;
		}
		else if (size < limit)
		{
			y_current->y_array =
						(char_u **)alloc((unsigned)(size * sizeof(char_u *)));
			for (i = 0; i < size; i++)
				y_current->y_array[i] = array[i];
			vim_free(array);
		}
		y_current->y_size = size;
	}
	return eof;
}

	void
write_viminfo_registers(fp)
	FILE	*fp;
{
	int		i, j;
	char_u	*type;
	char_u	c;
	int		num_lines;
	int		max_num_lines;

	fprintf(fp, "\n# Registers:\n");

	max_num_lines = get_viminfo_parameter('"');
	if (max_num_lines == 0)
		return;
	for (i = 0; i < NUM_REGISTERS; i++)
	{
		if (y_buf[i].y_array == NULL)
			continue;
#ifdef USE_GUI
		/* Skip '*' register, we don't want it back next time */
		if (i == GUI_SELECTION_REGISTER)
			continue;
#endif
		switch (y_buf[i].y_type)
		{
			case MLINE:
				type = (char_u *)"LINE";
				break;
			case MCHAR:
				type = (char_u *)"CHAR";
				break;
			case MBLOCK:
				type = (char_u *)"BLOCK";
				break;
			default:
				sprintf((char *)IObuff, "Unknown register type %d",
					y_buf[i].y_type);
				emsg(IObuff);
				type = (char_u *)"LINE";
				break;
		}
		if (y_previous == &y_buf[i])
			fprintf(fp, "\"");
		if (i == DELETION_REGISTER)
			c = '-';
		else if (i < 10)
			c = '0' + i;
		else
			c = 'a' + i - 10;
		fprintf(fp, "\"%c\t%s\n", c, type);
		num_lines = y_buf[i].y_size;

		/* If max_num_lines < 0, then we save ALL the lines in the register */
		if (max_num_lines > 0 && num_lines > max_num_lines)
			num_lines = max_num_lines;
		for (j = 0; j < num_lines; j++)
		{
			putc('\t', fp);
			viminfo_writestring(fp, y_buf[i].y_array[j]);
		}
	}
}
#endif /* VIMINFO */

#if defined(USE_GUI) || defined(PROTO)
/*
 * Text selection stuff that uses the GUI selection register '*'.  When using a
 * GUI this may be text from another window, otherwise it is the last text we
 * had highlighted with VIsual mode.  With mouse support, clicking the middle
 * button performs the paste, otherwise you will need to do <"*p>.
 */

	void
gui_free_selection()
{
	struct yankbuf *y_ptr = y_current;

	y_current = &y_buf[GUI_SELECTION_REGISTER];		/* '*' register */
	free_yank_all();
	y_current->y_size = 0;
	y_current = y_ptr;
}

/*
 * Get the selected text and put it in the gui text register '*'.
 */
	void
gui_get_selection()
{
	struct yankbuf *old_y_previous, *old_y_current;
	char_u	old_yankbuffer;
	FPOS	old_cursor, old_visual;
	int		old_op_type;

	if (gui.selection.owned)
	{
		if (y_buf[GUI_SELECTION_REGISTER].y_array != NULL)
			return;

		/* Get the text between gui.selection.start & gui.selection.end */
		old_y_previous = y_previous;
		old_y_current = y_current;
		old_yankbuffer = yankbuffer;
		old_cursor = curwin->w_cursor;
		old_visual = VIsual;
		old_op_type = op_type;
		yankbuffer = '*';
		op_type = YANK;
		do_pending_operator('y', NUL, FALSE, NULL, NULL, 0, TRUE, TRUE);
		y_previous = old_y_previous;
		y_current = old_y_current;
		yankbuffer = old_yankbuffer;
		curwin->w_cursor = old_cursor;
		VIsual = old_visual;
		op_type = old_op_type;
	}
	else
	{
		gui_free_selection();

		/* Try to get selected text from another window */
		gui_request_selection();
	}
}

/* Convert from the GUI selection string into the '*' register */
	void
gui_yank_selection(type, str, len)
	int		type;
	char_u	*str;
	long_u	len;
{
	struct yankbuf *y_ptr = &y_buf[GUI_SELECTION_REGISTER];	/* '*' register */
	int		lnum;
	int		start;
	int		i;

	gui_free_selection();

	/* Count the number of lines within the string */
	y_ptr->y_size = 1;
	for (i = 0; i < len; i++)
		if (str[i] == '\n')
			y_ptr->y_size++;

	if (type != MCHAR && i > 0 && str[i - 1] == '\n')
		y_ptr->y_size--;

	y_ptr->y_array = (char_u **)lalloc(y_ptr->y_size * sizeof(char_u *), TRUE);
	if (y_ptr->y_array == NULL)
		return;
	y_ptr->y_type = type;
	lnum = 0;
	start = 0;
	for (i = 0; i < len; i++)
	{
		if (str[i] == NUL)
			str[i] = '\n';
		else if (str[i] == '\n')
		{
			str[i] = NUL;
			if (type == MCHAR || i != len - 1)
			{
				if ((y_ptr->y_array[lnum] = strsave(str + start)) == NULL)
				{
					y_ptr->y_size = lnum;
					return;
				}
				lnum++;
				start = i + 1;
			}
		}
	}
	if ((y_ptr->y_array[lnum] = alloc(i - start + 1)) == NULL)
		return;
	if (i - start > 0)
		STRNCPY(y_ptr->y_array[lnum], str + start, i - start);
	y_ptr->y_array[lnum][i - start] = NUL;
	y_ptr->y_size = lnum + 1;
}

/*
 * Convert the '*' register into a GUI selection string returned in *str with
 * length *len.
 */
	int
gui_convert_selection(str, len)
	char_u	**str;
	long_u	*len;
{
	struct yankbuf *y_ptr = &y_buf[GUI_SELECTION_REGISTER];	/* '*' register */
	char_u	*p;
	int		lnum;
	int		i, j;

	*str = NULL;
	*len = 0;
	if (y_ptr->y_array == NULL)
		return -1;

	for (i = 0; i < y_ptr->y_size; i++)
		*len += STRLEN(y_ptr->y_array[i]) + 1;

	/*
	 * Don't want newline character at end of last line if we're in MCHAR mode.
	 */
	if (y_ptr->y_type == MCHAR && *len > 1)
		(*len)--;

	p = *str = lalloc(*len, TRUE);
	if (p == NULL)
		return -1;
	lnum = 0;
	for (i = 0, j = 0; i < *len; i++, j++)
	{
		if (y_ptr->y_array[lnum][j] == '\n')
			p[i] = NUL;
		else if (y_ptr->y_array[lnum][j] == NUL)
		{
			p[i] = '\n';
			lnum++;
			j = -1;
		}
		else
			p[i] = y_ptr->y_array[lnum][j];
	}
	return y_ptr->y_type;
}
#endif /* USE_GUI || PROTO */

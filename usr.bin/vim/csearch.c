/*	$OpenBSD: csearch.c,v 1.2 1996/09/21 06:22:56 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 *
 * csearch.c: do_sub() and do_glob() for :s, :g and :v
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

/* we use modified Henry Spencer's regular expression routines */
#include "regexp.h"

static int do_sub_msg __ARGS((void));

#ifdef VIMINFO
	static char_u   *old_sub = NULL;
#endif /* VIMINFO */

/* do_sub(lp, up, cmd)
 *
 * Perform a substitution from line 'lp' to line 'up' using the
 * command pointed to by 'cmd' which should be of the form:
 *
 * /pattern/substitution/gc
 *
 * The trailing 'g' is optional and, if present, indicates that multiple
 * substitutions should be performed on each line, if applicable.
 * The trailing 'c' is optional and, if present, indicates that a confirmation
 * will be asked for each replacement.
 * The usual escapes are supported as described in the regexp docs.
 *
 * use_old == 0 for :substitute
 * use_old == 1 for :&
 * use_old == 2 for :~
 */

static long			sub_nsubs;		/* total number of substitutions */
static linenr_t		sub_nlines;		/* total number of lines changed */

	void
do_sub(lp, up, cmd, nextcommand, use_old)
	linenr_t	lp;
	linenr_t	up;
	char_u		*cmd;
	char_u		**nextcommand;
	int			use_old;
{
	linenr_t		lnum;
	long			i;
	char_u		   *ptr;
	char_u		   *old_line;
	regexp		   *prog;
	static int		do_all = FALSE; 	/* do multiple substitutions per line */
	static int		do_ask = FALSE; 	/* ask for confirmation */
	int				do_print = FALSE;	/* print last line with subst. */
	char_u		   *pat, *sub;
#ifndef VIMINFO						/* otherwise it is global */
	static char_u   *old_sub = NULL;
#endif
	int 			delimiter;
	int 			sublen;
	int				got_quit = FALSE;
	int				got_match = FALSE;
	int				temp;
	int				which_pat;
	
	if (!global_busy)
	{
		sub_nsubs = 0;
		sub_nlines = 0;
	}

	if (use_old == 2)
		which_pat = RE_LAST;	/* use last used regexp */
	else
		which_pat = RE_SUBST;	/* use last substitute regexp */

								/* new pattern and substitution */
	if (use_old == 0 && *cmd != NUL &&
					   vim_strchr((char_u *)"0123456789gcr|\"", *cmd) == NULL)
	{
								/* don't accept alphanumeric for separator */
		if (isalpha(*cmd) || isdigit(*cmd))
		{
			EMSG("Regular expressions can't be delimited by letters or digits");
			return;
		}
		/*
		 * undocumented vi feature:
		 *	"\/sub/" and "\?sub?" use last used search pattern (almost like
		 *	//sub/r).  "\&sub&" use last substitute pattern (like //sub/).
		 */
		if (*cmd == '\\')
		{
			++cmd;
			if (vim_strchr((char_u *)"/?&", *cmd) == NULL)
			{
				emsg(e_backslash);
				return;
			}
			if (*cmd != '&')
				which_pat = RE_SEARCH;		/* use last '/' pattern */
			pat = (char_u *)"";				/* empty search pattern */
			delimiter = *cmd++;				/* remember delimiter character */
		}
		else			/* find the end of the regexp */
		{
			which_pat = RE_LAST;			/* use last used regexp */
			delimiter = *cmd++;				/* remember delimiter character */
			pat = cmd;						/* remember start of search pat */
			cmd = skip_regexp(cmd, delimiter);
			if (cmd[0] == delimiter)		/* end delimiter found */
				*cmd++ = NUL;				/* replace it with a NUL */
		}

		/*
		 * Small incompatibility: vi sees '\n' as end of the command, but in
		 * Vim we want to use '\n' to find/substitute a NUL.
		 */
		sub = cmd;			/* remember the start of the substitution */

		while (cmd[0])
		{
			if (cmd[0] == delimiter)			/* end delimiter found */
			{
				*cmd++ = NUL;					/* replace it with a NUL */
				break;
			}
			if (cmd[0] == '\\' && cmd[1] != 0)	/* skip escaped characters */
				++cmd;
			++cmd;
		}

		vim_free(old_sub);
		old_sub = strsave(sub);
	}
	else								/* use previous pattern and substitution */
	{
		if (old_sub == NULL)    /* there is no previous command */
		{
			emsg(e_nopresub);
			return;
		}
		pat = NULL; 			/* myregcomp() will use previous pattern */
		sub = old_sub;
	}

	/*
	 * find trailing options
	 */
	if (!p_ed)
	{
		if (p_gd)				/* default is global on */
			do_all = TRUE;
		else
			do_all = FALSE;
		do_ask = FALSE;
	}
	while (*cmd)
	{
		/*
		 * Note that 'g' and 'c' are always inverted, also when p_ed is off
		 * 'r' is never inverted.
		 */
		if (*cmd == 'g')
			do_all = !do_all;
		else if (*cmd == 'c')
			do_ask = !do_ask;
		else if (*cmd == 'r')		/* use last used regexp */
			which_pat = RE_LAST;
		else if (*cmd == 'p')
			do_print = TRUE;
		else
			break;
		++cmd;
	}

	/*
	 * check for a trailing count
	 */
	cmd = skipwhite(cmd);
	if (isdigit(*cmd))
	{
		i = getdigits(&cmd);
		if (i <= 0)
		{
			emsg(e_zerocount);
			return;
		}
		lp = up;
		up += i - 1;
	}

	/*
	 * check for trailing '|', '"' or '\n'
	 */
	cmd = skipwhite(cmd);
	if (*cmd)
	{
		if (vim_strchr((char_u *)"|\"\n", *cmd) == NULL)
		{
			emsg(e_trailing);
			return;
		}
		else
			*nextcommand = cmd + 1;
	}

	if ((prog = myregcomp(pat, RE_SUBST, which_pat, SEARCH_HIS)) == NULL)
	{
		emsg(e_invcmd);
		return;
	}

	/*
	 * ~ in the substitute pattern is replaced with the old pattern.
	 * We do it here once to avoid it to be replaced over and over again.
	 */
	sub = regtilde(sub, (int)p_magic);

	old_line = NULL;
	for (lnum = lp; lnum <= up && !(got_int || got_quit); ++lnum)
	{
		ptr = ml_get(lnum);
		if (vim_regexec(prog, ptr, TRUE))  /* a match on this line */
		{
			char_u		*new_end, *new_start = NULL;
			char_u		*old_match, *old_copy;
			char_u		*prev_old_match = NULL;
			char_u		*p1;
			int			did_sub = FALSE;
			int			match, lastone;
			unsigned	len, needed_len;
			unsigned	new_start_len = 0;

			/* make a copy of the line, so it won't be taken away when updating
				the screen */
			if ((old_line = strsave(ptr)) == NULL)
				continue;
			vim_regexec(prog, old_line, TRUE);  /* match again on this line to
											 	 * update the pointers. TODO:
												 * remove extra vim_regexec() */
			if (!got_match)
			{
				setpcmark();
				got_match = TRUE;
			}

			old_copy = old_match = old_line;
			for (;;)			/* loop until nothing more to replace */
			{
				/*
				 * Save the position of the last change for the final cursor
				 * position (just like the real vi).
				 */
				curwin->w_cursor.lnum = lnum;
				curwin->w_cursor.col = (int)(prog->startp[0] - old_line);

				/*
				 * Match empty string does not count, except for first match.
				 * This reproduces the strange vi behaviour.
				 * This also catches endless loops.
				 */
				if (old_match == prev_old_match && old_match == prog->endp[0])
				{
					++old_match;
					goto skip;
				}
				old_match = prog->endp[0];
				prev_old_match = old_match;

				while (do_ask)		/* loop until 'y', 'n', 'q', CTRL-E or CTRL-Y typed */
				{
					temp = RedrawingDisabled;
					RedrawingDisabled = FALSE;
					comp_Botline(curwin);
					search_match_len = prog->endp[0] - prog->startp[0];
									/* invert the matched string
									 * remove the inversion afterwards */
					if (search_match_len == 0)
						search_match_len = 1;		/* show something! */
					highlight_match = TRUE;
					updateScreen(CURSUPD);
					highlight_match = FALSE;
					redraw_later(NOT_VALID);
									/* same highlighting as for wait_return */
					(void)set_highlight('r');
					msg_highlight = TRUE;
					smsg((char_u *)"replace with %s (y/n/a/q/^E/^Y)?", sub);
					showruler(TRUE);
					RedrawingDisabled = temp;
					
					++no_mapping;				/* don't map this key */
					i = vgetc();
					--no_mapping;

						/* clear the question */
					msg_didout = FALSE;			/* don't scroll up */
					msg_col = 0;
					gotocmdline(TRUE);
					if (i == 'q' || i == ESC || i == Ctrl('C'))
					{
						got_quit = TRUE;
						break;
					}
					else if (i == 'n')
						goto skip;
					else if (i == 'y')
						break;
					else if (i == 'a')
					{
						do_ask = FALSE;
						break;
					}
					else if (i == Ctrl('E'))
						scrollup_clamp();
					else if (i == Ctrl('Y'))
						scrolldown_clamp();
				}
				if (got_quit)
					break;

						/* get length of substitution part */
				sublen = vim_regsub(prog, sub, old_line, FALSE, (int)p_magic);
				if (new_start == NULL)
				{
					/*
					 * Get some space for a temporary buffer to do the
					 * substitution into (and some extra space to avoid
					 * too many calls to alloc()/free()).
					 */
					new_start_len = STRLEN(old_copy) + sublen + 25;
					if ((new_start = alloc_check(new_start_len)) == NULL)
						goto outofmem;
					*new_start = NUL;
					new_end = new_start;
				}
				else
				{
					/*
					 * Extend the temporary buffer to do the substitution into.
					 * Avoid an alloc()/free(), it takes a lot of time.
					 */
					len = STRLEN(new_start);
					needed_len = len + STRLEN(old_copy) + sublen + 1;
					if (needed_len > new_start_len)
					{
						needed_len += 20;		/* get some extra */
						if ((p1 = alloc_check(needed_len)) == NULL)
							goto outofmem;
						STRCPY(p1, new_start);
						vim_free(new_start);
						new_start = p1;
						new_start_len = needed_len;
					}
					new_end = new_start + len;
				}

				/*
				 * copy the text up to the part that matched
				 */
				i = prog->startp[0] - old_copy;
				vim_memmove(new_end, old_copy, (size_t)i);
				new_end += i;

				vim_regsub(prog, sub, new_end, TRUE, (int)p_magic);
				sub_nsubs++;
				did_sub = TRUE;

				/*
				 * Now the trick is to replace CTRL-Ms with a real line break.
				 * This would make it impossible to insert CTRL-Ms in the text.
				 * That is the way vi works. In Vim the line break can be
				 * avoided by preceding the CTRL-M with a CTRL-V. Now you can't
				 * precede a line break with a CTRL-V, big deal.
				 */
				while ((p1 = vim_strchr(new_end, CR)) != NULL)
				{
					if (p1 == new_end || p1[-1] != Ctrl('V'))
					{
						if (u_inssub(lnum) == OK)	/* prepare for undo */
						{
							*p1 = NUL;				/* truncate up to the CR */
							mark_adjust(lnum, MAXLNUM, 1L, 0L);
							ml_append(lnum - 1, new_start,
										(colnr_t)(p1 - new_start + 1), FALSE);
							++lnum;
							++up;				/* number of lines increases */
							STRCPY(new_start, p1 + 1);	/* copy the rest */
							new_end = new_start;
						}
					}
					else							/* remove CTRL-V */
					{
						STRCPY(p1 - 1, p1);
						new_end = p1;
					}
				}

				old_copy = prog->endp[0];	/* remember next character to be copied */
				/*
				 * continue searching after the match
				 * prevent endless loop with patterns that match empty strings,
				 * e.g. :s/$/pat/g or :s/[a-z]* /(&)/g
				 */
skip:
				match = -1;
				lastone = (*old_match == NUL || got_int || got_quit || !do_all);
				if (lastone || do_ask ||
					  (match = vim_regexec(prog, old_match, (int)FALSE)) == 0)
				{
					if (new_start)
					{
						/*
						 * Copy the rest of the line, that didn't match.
						 * Old_match has to be adjusted, we use the end of the
						 * line as reference, because the substitute may have
						 * changed the number of characters.
						 */
						STRCAT(new_start, old_copy);
						i = old_line + STRLEN(old_line) - old_match;
						if (u_savesub(lnum) == OK)
							ml_replace(lnum, new_start, TRUE);

						vim_free(old_line);			/* free the temp buffer */
						old_line = new_start;
						new_start = NULL;
						old_match = old_line + STRLEN(old_line) - i;
						if (old_match < old_line)		/* safety check */
						{
							EMSG("do_sub internal error: old_match < old_line");
							old_match = old_line;
						}
						old_copy = old_line;
					}
					if (match == -1 && !lastone)
						match = vim_regexec(prog, old_match, (int)FALSE);
					if (match <= 0)	  /* quit loop if there is no more match */
						break;
				}
				line_breakcheck();

			}
			if (did_sub)
				++sub_nlines;
			vim_free(old_line);		/* free the copy of the original line */
			old_line = NULL;
		}
		line_breakcheck();
	}

outofmem:
	vim_free(old_line);		/* may have to free an allocated copy of the line */
	if (sub_nsubs)
	{
		CHANGED;
		if (!global_busy)
		{
			updateScreen(CURSUPD); /* need this to update LineSizes */
			beginline(TRUE);
			if (!do_sub_msg() && do_ask)
				MSG("");
		}
		if (do_print)
			print_line(curwin->w_cursor.lnum, FALSE);
	}
	else if (!global_busy)
	{
		if (got_int)			/* interrupted */
			emsg(e_interr);
		else if (got_match)		/* did find something but nothing substituted */
			MSG("");
		else					/* nothing found */
			emsg(e_nomatch);
	}

	vim_free(prog);
}

/*
 * Give message for number of substitutions.
 * Can also be used after a ":global" command.
 * Return TRUE if a message was given.
 */
	static int
do_sub_msg()
{
	if (sub_nsubs > p_report)
	{
		sprintf((char *)msg_buf, "%s%ld substitution%s on %ld line%s",
				got_int ? "(Interrupted) " : "",
				sub_nsubs, plural(sub_nsubs),
				(long)sub_nlines, plural((long)sub_nlines));
		if (msg(msg_buf))
			keep_msg = msg_buf;
		return TRUE;
	}
	if (got_int)
	{
		emsg(e_interr);
		return TRUE;
	}
	return FALSE;
}

/*
 * do_glob(cmd)
 *
 * Execute a global command of the form:
 *
 * g/pattern/X : execute X on all lines where pattern matches
 * v/pattern/X : execute X on all lines where pattern does not match
 *
 * where 'X' is an EX command
 *
 * The command character (as well as the trailing slash) is optional, and
 * is assumed to be 'p' if missing.
 *
 * This is implemented in two passes: first we scan the file for the pattern and
 * set a mark for each line that (not) matches. secondly we execute the command
 * for each line that has a mark. This is required because after deleting
 * lines we do not know where to search for the next match.
 */

	void
do_glob(type, lp, up, cmd)
	int 		type;
	linenr_t	lp, up;
	char_u		*cmd;
{
	linenr_t		lnum;		/* line number according to old situation */
	linenr_t		old_lcount; /* curbuf->b_ml.ml_line_count before the command */
	int 			ndone;

	char_u			delim;		/* delimiter, normally '/' */
	char_u		   *pat;
	regexp		   *prog;
	int				match;
	int				which_pat;

	if (global_busy)
	{
		EMSG("Cannot do :global recursive");	/* will increment global_busy */
		return;
	}

	which_pat = RE_LAST;			/* default: use last used regexp */
	sub_nsubs = 0;
	sub_nlines = 0;

	/*
	 * undocumented vi feature:
	 *	"\/" and "\?": use previous search pattern.
	 *  	     "\&": use previous substitute pattern.
	 */
	if (*cmd == '\\')
	{
		++cmd;
		if (vim_strchr((char_u *)"/?&", *cmd) == NULL)
		{
			emsg(e_backslash);
			return;
		}
		if (*cmd == '&')
			which_pat = RE_SUBST;		/* use previous substitute pattern */
		else
			which_pat = RE_SEARCH;		/* use previous search pattern */
		++cmd;
		pat = (char_u *)"";
	}
	else if (*cmd == NUL)
	{
		EMSG("Regular expression missing from global");
		return;
	}
	else
	{
		delim = *cmd; 			/* get the delimiter */
		if (delim)
			++cmd;				/* skip delimiter if there is one */
		pat = cmd;				/* remember start of pattern */
		cmd = skip_regexp(cmd, delim);
		if (cmd[0] == delim)				/* end delimiter found */
			*cmd++ = NUL;					/* replace it with a NUL */
	}

	if ((prog = myregcomp(pat, RE_BOTH, which_pat, SEARCH_HIS)) == NULL)
	{
		emsg(e_invcmd);
		return;
	}

/*
 * pass 1: set marks for each (not) matching line
 */
	ndone = 0;
	for (lnum = lp; lnum <= up && !got_int; ++lnum)
	{
		/* a match on this line? */
		match = vim_regexec(prog, ml_get(lnum), (int)TRUE);
		if ((type == 'g' && match) || (type == 'v' && !match))
		{
			ml_setmarked(lnum);
			ndone++;
		}
		line_breakcheck();
	}

/*
 * pass 2: execute the command for each line that has been marked
 */
	if (got_int)
		MSG("Interrupted");
	else if (ndone == 0)
		msg(e_nomatch);
	else
	{
		/*
		 * Set current position only once for a global command.
		 * If global_busy is set, setpcmark() will not do anything.
		 * If there is an error, global_busy will be incremented.
		 */
		setpcmark();

		global_busy = 1;
		old_lcount = curbuf->b_ml.ml_line_count;
		while (!got_int && (lnum = ml_firstmarked()) != 0 && global_busy == 1)
		{
			curwin->w_cursor.lnum = lnum;
			curwin->w_cursor.col = 0;
			if (*cmd == NUL || *cmd == '\n')
				do_cmdline((char_u *)"p", FALSE, TRUE);
			else
				do_cmdline(cmd, FALSE, TRUE);
			mch_breakcheck();
		}

		global_busy = 0;

		/*
		 * Redraw everything.  Could use CLEAR, which is faster in some
		 * situations, but when there are few changes this makes the display
		 * flicker.
		 */
		must_redraw = NOT_VALID;
		cursupdate();

		/* If subsitutes done, report number of substitues, otherwise report
		 * number of extra or deleted lines. */
		if (!do_sub_msg())
			msgmore(curbuf->b_ml.ml_line_count - old_lcount);
	}

	ml_clearmarked();      /* clear rest of the marks */
	vim_free(prog);
}

#ifdef VIMINFO
	int
read_viminfo_sub_string(line, fp, force)
	char_u	*line;
	FILE	*fp;
	int		force;
{
	if (old_sub != NULL && force)
		vim_free(old_sub);
	if (force || old_sub == NULL)
	{
		viminfo_readstring(line);
		old_sub = strsave(line + 1);
	}
	return vim_fgets(line, LSIZE, fp);
}

	void
write_viminfo_sub_string(fp)
	FILE	*fp;
{
	if (get_viminfo_parameter('/') != 0 && old_sub != NULL)
	{
		fprintf(fp, "\n# Last Substitute String:\n$");
		viminfo_writestring(fp, old_sub);
	}
}
#endif /* VIMINFO */

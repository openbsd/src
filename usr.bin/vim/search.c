/*	$OpenBSD: search.c,v 1.1.1.1 1996/09/07 21:40:24 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */
/*
 * search.c: code for normal mode searching commands
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#include "ops.h"		/* for op_inclusive */

/* modified Henry Spencer's regular expression routines */
#include "regexp.h"

static int inmacro __ARGS((char_u *, char_u *));
static int cls __ARGS((void));
static int skip_chars __ARGS((int, int));
static void back_in_line __ARGS((void));
static void find_first_blank __ARGS((FPOS *));
static void show_pat_in_path __ARGS((char_u *, int,
										 int, int, FILE *, linenr_t *, long));
static void give_warning __ARGS((char_u *));

static char_u *top_bot_msg = (char_u *)"search hit TOP, continuing at BOTTOM";
static char_u *bot_top_msg = (char_u *)"search hit BOTTOM, continuing at TOP";

/*
 * This file contains various searching-related routines. These fall into
 * three groups:
 * 1. string searches (for /, ?, n, and N)
 * 2. character searches within a single line (for f, F, t, T, etc)
 * 3. "other" kinds of searches like the '%' command, and 'word' searches.
 */

/*
 * String searches
 *
 * The string search functions are divided into two levels:
 * lowest:	searchit(); called by do_search() and edit().
 * Highest: do_search(); changes curwin->w_cursor, called by normal().
 *
 * The last search pattern is remembered for repeating the same search.
 * This pattern is shared between the :g, :s, ? and / commands.
 * This is in myregcomp().
 *
 * The actual string matching is done using a heavily modified version of
 * Henry Spencer's regular expression library.
 */

/*
 * Two search patterns are remembered: One for the :substitute command and
 * one for other searches. last_pattern points to the one that was
 * used the last time.
 */
static char_u	*search_pattern = NULL;
static int		search_magic = TRUE;
static int		search_no_scs = FALSE;
static char_u	*subst_pattern = NULL;
static int		subst_magic = TRUE;
static int		subst_no_scs = FALSE;
static char_u	*last_pattern = NULL;
static int		last_magic = TRUE;
static int		last_no_scs = FALSE;
static char_u	*mr_pattern = NULL;		/* pattern used by myregcomp() */

static int		want_start;				/* looking for start of line? */

/*
 * Type used by find_pattern_in_path() to remember which included files have
 * been searched already.
 */
typedef struct SearchedFile
{
	FILE		*fp;		/* File pointer */
	char_u		*name;		/* Full name of file */
	linenr_t	lnum;		/* Line we were up to in file */
	int			matched;	/* Found a match in this file */
} SearchedFile;

/*
 * translate search pattern for vim_regcomp()
 *
 * sub_cmd == RE_SEARCH: save pat in search_pattern (normal search command)
 * sub_cmd == RE_SUBST: save pat in subst_pattern (:substitute command)
 * sub_cmd == RE_BOTH: save pat in both patterns (:global command)
 * which_pat == RE_SEARCH: use previous search pattern if "pat" is NULL
 * which_pat == RE_SUBST: use previous sustitute pattern if "pat" is NULL
 * which_pat == RE_LAST: use last used pattern if "pat" is NULL
 * options & SEARCH_HIS: put search string in history
 * options & SEARCH_KEEP: keep previous search pattern
 * 
 */
	regexp *
myregcomp(pat, sub_cmd, which_pat, options)
	char_u	*pat;
	int		sub_cmd;
	int		which_pat;
	int		options;
{
	rc_did_emsg = FALSE;
	reg_magic = p_magic;
	if (pat == NULL || *pat == NUL)		/* use previous search pattern */
	{
		if (which_pat == RE_SEARCH)
		{
			if (search_pattern == NULL)
			{
				emsg(e_noprevre);
				rc_did_emsg = TRUE;
				return (regexp *) NULL;
			}
			pat = search_pattern;
			reg_magic = search_magic;
			no_smartcase = search_no_scs;
		}
		else if (which_pat == RE_SUBST)
		{
			if (subst_pattern == NULL)
			{
				emsg(e_nopresub);
				rc_did_emsg = TRUE;
				return (regexp *) NULL;
			}
			pat = subst_pattern;
			reg_magic = subst_magic;
			no_smartcase = subst_no_scs;
		}
		else	/* which_pat == RE_LAST */
		{
			if (last_pattern == NULL)
			{
				emsg(e_noprevre);
				rc_did_emsg = TRUE;
				return (regexp *) NULL;
			}
			pat = last_pattern;
			reg_magic = last_magic;
			no_smartcase = last_no_scs;
		}
	}
	else if (options & SEARCH_HIS)
		add_to_history(1, pat);			/* put new pattern in history */

	mr_pattern = pat;

	/*
	 * save the currently used pattern in the appropriate place,
	 * unless the pattern should not be remembered
	 */
	if (!(options & SEARCH_KEEP))
	{
		/*
		 * search or global command
		 */
		if (sub_cmd == RE_SEARCH || sub_cmd == RE_BOTH)
		{
			if (search_pattern != pat)
			{
				vim_free(search_pattern);
				search_pattern = strsave(pat);
				last_pattern = search_pattern;
				search_magic = reg_magic;
				last_magic = reg_magic;		/* Magic sticks with the r.e. */
				search_no_scs = no_smartcase;
				last_no_scs = no_smartcase;
			}
		}
		/*
		 * substitute or global command
		 */
		if (sub_cmd == RE_SUBST || sub_cmd == RE_BOTH)
		{
			if (subst_pattern != pat)
			{
				vim_free(subst_pattern);
				subst_pattern = strsave(pat);
				last_pattern = subst_pattern;
				subst_magic = reg_magic;
				last_magic = reg_magic;		/* Magic sticks with the r.e. */
				subst_no_scs = no_smartcase;
				last_no_scs = no_smartcase;
			}
		}
	}

	want_start = (*pat == '^');	/* looking for start of line? */
	set_reg_ic(pat);			/* tell the vim_regexec routine how to search */
	return vim_regcomp(pat);
}

/*
 * Set reg_ic according to p_ic, p_scs and the search pattern.
 */
	void
set_reg_ic(pat)
	char_u	*pat;
{
	char_u *p;

	reg_ic = p_ic;
	if (!no_smartcase && p_scs
#ifdef INSERT_EXPAND
								&& !(ctrl_x_mode && curbuf->b_p_inf)
#endif
																	)
	{
		/* don't ignore case if pattern has uppercase */
		for (p = pat; *p; )
			if (isupper(*p++))
				reg_ic = FALSE;
	}
	no_smartcase = FALSE;
}

/*
 * lowest level search function.
 * Search for 'count'th occurrence of 'str' in direction 'dir'.
 * Start at position 'pos' and return the found position in 'pos'.
 *
 * if (options & SEARCH_MSG) == 0 don't give any messages
 * if (options & SEARCH_MSG) == SEARCH_NFMSG don't give 'notfound' messages
 * if (options & SEARCH_MSG) == SEARCH_MSG give all messages
 * if (options & SEARCH_HIS) put search pattern in history
 * if (options & SEARCH_END) return position at end of match
 * if (options & SEARCH_START) accept match at pos itself
 * if (options & SEARCH_KEEP) keep previous search pattern
 *
 * Return OK for success, FAIL for failure.
 */
	int
searchit(pos, dir, str, count, options, which_pat)
	FPOS	*pos;
	int		dir;
	char_u	*str;
	long	count;
	int		options;
	int		which_pat;
{
	int					found;
	linenr_t			lnum;			/* no init to shut up Apollo cc */
	regexp				*prog;
	char_u				*ptr;
	register char_u		*match = NULL, *matchend = NULL;	/* init for GCC */
	int					loop;
	FPOS				start_pos;
	int					at_first_line;
	int					extra_col;
	int					match_ok;
	char_u				*p;

	if ((prog = myregcomp(str, RE_SEARCH, which_pat,
							 (options & (SEARCH_HIS + SEARCH_KEEP)))) == NULL)
	{
		if ((options & SEARCH_MSG) && !rc_did_emsg)
			emsg2((char_u *)"Invalid search string: %s", mr_pattern);
		return FAIL;
	}

	if (options & SEARCH_START)
		extra_col = 0;
	else
		extra_col = 1;


/*
 * find the string
 */
	do							/* loop for count */
	{
		start_pos = *pos;		/* remember start pos for detecting no match */
		found = 0;				/* default: not found */

		/*
		 * Start searching in current line, unless searching backwards and
		 * we're in column 0 or searching forward and we're past the end of
		 * the line
		 */
		if (dir == BACKWARD && start_pos.col == 0)
		{
			lnum = pos->lnum - 1;
			at_first_line = FALSE;
		}
		else
		{
			lnum = pos->lnum;
			at_first_line = TRUE;
		}

		for (loop = 0; loop <= 1; ++loop)	/* loop twice if 'wrapscan' set */
		{
			for ( ; lnum > 0 && lnum <= curbuf->b_ml.ml_line_count;
										   lnum += dir, at_first_line = FALSE)
			{
				ptr = ml_get(lnum);
											/* forward search, first line */
				if (dir == FORWARD && at_first_line && start_pos.col > 0 &&
																   want_start)
					continue;				/* match not possible */

				/*
				 * Look for a match somewhere in the line.
				 */
				if (vim_regexec(prog, ptr, TRUE))
				{
					match = prog->startp[0];
					matchend = prog->endp[0];
					
					/*
					 * Forward search in the first line: match should be after
					 * the start position. If not, continue at the end of the
					 * match (this is vi compatible).
					 */
					if (dir == FORWARD && at_first_line)
					{
						match_ok = TRUE;
						/*
						 * When *match == NUL the cursor will be put one back
						 * afterwards, compare with that position, otherwise
						 * "/$" will get stuck on end of line.  Same for
						 * matchend.
						 */
						while ((options & SEARCH_END) ?
						((int)(matchend - ptr) - 1 - (int)(*matchend == NUL) <
											 (int)start_pos.col + extra_col) :
								  ((int)(match - ptr) - (int)(*match == NUL) <
											  (int)start_pos.col + extra_col))
						{
							/*
							 * If vi-compatible searching, continue at the end
							 * of the match, otherwise continue one position
							 * forward.
							 */
							if (vim_strchr(p_cpo, CPO_SEARCH) != NULL)
							{
								p = matchend;
								if (match == p && *p != NUL)
									++p;
							}
							else
							{
								p = match;
								if (*p != NUL)
									++p;
							}
							if (*p != NUL && vim_regexec(prog, p, FALSE))
							{
								match = prog->startp[0];
								matchend = prog->endp[0];
							}
							else
							{
								match_ok = FALSE;
								break;
							}
						}
						if (!match_ok)
							continue;
					}
					if (dir == BACKWARD && !want_start)
					{
						/*
						 * Now, if there are multiple matches on this line,
						 * we have to get the last one. Or the last one before
						 * the cursor, if we're on that line.
						 * When putting the new cursor at the end, compare
						 * relative to the end of the match.
						 */
						match_ok = FALSE;
						for (;;)
						{
							if (!at_first_line || ((options & SEARCH_END) ?
										((prog->endp[0] - ptr) - 1 + extra_col
													  <= (int)start_pos.col) :
										  ((prog->startp[0] - ptr) + extra_col
													  <= (int)start_pos.col)))
							{
								match_ok = TRUE;
								match = prog->startp[0];
								matchend = prog->endp[0];
							}
							else
								break;
							/*
							 * If vi-compatible searching, continue at the end
							 * of the match, otherwise continue one position
							 * forward.
							 */
							if (vim_strchr(p_cpo, CPO_SEARCH) != NULL)
							{
								p = matchend;
								if (p == match && *p != NUL)
									++p;
							}
							else
							{
								p = match;
								if (*p != NUL)
									++p;
							}
							if (*p == NUL || !vim_regexec(prog, p, (int)FALSE))
								break;
						}

						/*
						 * If there is only a match after the cursor, skip
						 * this match.
						 */
						if (!match_ok)
							continue;
					}

					pos->lnum = lnum;
					if (options & SEARCH_END && !(options & SEARCH_NOOF))
						pos->col = (int) (matchend - ptr - 1);
					else
						pos->col = (int) (match - ptr);
					found = 1;
					break;
				}
				line_breakcheck();		/* stop if ctrl-C typed */
				if (got_int)
					break;

				if (loop && lnum == start_pos.lnum)
					break;			/* if second loop, stop where started */
			}
			at_first_line = FALSE;

			/*
			 * stop the search if wrapscan isn't set, after an interrupt and
			 * after a match
			 */
			if (!p_ws || got_int || found)
				break;

			/*
			 * If 'wrapscan' is set we continue at the other end of the file.
			 * If 'shortmess' does not contain 's', we give a message.
			 * This message is also remembered in keep_msg for when the screen
			 * is redrawn. The keep_msg is cleared whenever another message is
			 * written.
			 */
			if (dir == BACKWARD)	/* start second loop at the other end */
			{
				lnum = curbuf->b_ml.ml_line_count;
				if (!shortmess(SHM_SEARCH) && (options & SEARCH_MSG))
					give_warning(top_bot_msg);
			}
			else
			{
				lnum = 1;
				if (!shortmess(SHM_SEARCH) && (options & SEARCH_MSG))
					give_warning(bot_top_msg);
			}
		}
		if (got_int)
			break;
	}
	while (--count > 0 && found);	/* stop after count matches or no match */

	vim_free(prog);

	if (!found)				/* did not find it */
	{
		if (got_int)
			emsg(e_interr);
		else if ((options & SEARCH_MSG) == SEARCH_MSG)
		{
			if (p_ws)
				EMSG2("Pattern not found: %s", mr_pattern);
			else if (lnum == 0)
				EMSG2("search hit TOP without match for: %s", mr_pattern);
			else
				EMSG2("search hit BOTTOM without match for: %s", mr_pattern);
		}
		return FAIL;
	}
	search_match_len = matchend - match;

	return OK;
}

/*
 * Highest level string search function.
 * Search for the 'count'th occurence of string 'str' in direction 'dirc'
 *				  If 'dirc' is 0: use previous dir.
 *    If 'str' is NULL or empty : use previous string.
 *	  If 'options & SEARCH_REV' : go in reverse of previous dir.
 *	  If 'options & SEARCH_ECHO': echo the search command and handle options
 *	  If 'options & SEARCH_MSG' : may give error message
 *	  If 'options & SEARCH_OPT' : interpret optional flags
 *	  If 'options & SEARCH_HIS' : put search pattern in history
 *	  If 'options & SEARCH_NOOF': don't add offset to position
 *	  If 'options & SEARCH_MARK': set previous context mark
 *	  If 'options & SEARCH_KEEP': keep previous search pattern
 *
 * Careful: If lastoffline == TRUE and lastoff == 0 this makes the
 * movement linewise without moving the match position.
 *
 * return 0 for failure, 1 for found, 2 for found and line offset added
 */
	int
do_search(dirc, str, count, options)
	int				dirc;
	char_u		   *str;
	long			count;
	int				options;
{
	FPOS			pos;		/* position of the last match */
	char_u			*searchstr;
	static int		lastsdir = '/';	/* previous search direction */
	static int		lastoffline;/* previous/current search has line offset */
	static int		lastend;	/* previous/current search set cursor at end */
	static long		lastoff;	/* previous/current line or char offset */
	int				old_lastsdir;
	int				old_lastoffline;
	int				old_lastend;
	long			old_lastoff;
	int				retval;		/* Return value */
	register char_u	*p;
	register long	c;
	char_u			*dircp;
	int				i = 0;		/* init for GCC */

	/*
	 * A line offset is not remembered, this is vi compatible.
	 */
	if (lastoffline && vim_strchr(p_cpo, CPO_LINEOFF) != NULL)
	{
		lastoffline = FALSE;
		lastoff = 0;
	}

	/*
	 * Save the values for when (options & SEARCH_KEEP) is used.
	 * (there is no "if ()" around this because gcc wants them initialized)
	 */
	old_lastsdir = lastsdir;
	old_lastoffline = lastoffline;
	old_lastend = lastend;
	old_lastoff = lastoff;

	pos = curwin->w_cursor;		/* start searching at the cursor position */

	/*
	 * Find out the direction of the search.
	 */
	if (dirc == 0)
		dirc = lastsdir;
	else
		lastsdir = dirc;
	if (options & SEARCH_REV)
	{
#ifdef WIN32
		/* There is a bug in the Visual C++ 2.2 compiler which means that
		 * dirc always ends up being '/' */
		dirc = (dirc == '/')  ?  '?'  :  '/';
#else
		if (dirc == '/')
			dirc = '?';
		else
			dirc = '/';
#endif
	}

	/*
	 * Repeat the search when pattern followed by ';', e.g. "/foo/;?bar".
	 */
	for (;;)
	{
		searchstr = str;
		dircp = NULL;
											/* use previous pattern */
		if (str == NULL || *str == NUL || *str == dirc)
		{
			if (search_pattern == NULL)		/* no previous pattern */
			{
				emsg(e_noprevre);
				retval = 0;
				goto end_do_search;
			}
			searchstr = (char_u *)"";  /* make myregcomp() use search_pattern */
		}

		if (str != NULL && *str != NUL)	/* look for (new) offset */
		{
			/*
			 * Find end of regular expression.
			 * If there is a matching '/' or '?', toss it.
			 */
			p = skip_regexp(str, dirc);
			if (*p == dirc)
			{
				dircp = p;		/* remember where we put the NUL */
				*p++ = NUL;
			}
			lastoffline = FALSE;
			lastend = FALSE;
			lastoff = 0;
			/*
			 * Check for a line offset or a character offset.
			 * For get_address (echo off) we don't check for a character
			 * offset, because it is meaningless and the 's' could be a
			 * substitute command.
			 */
			if (*p == '+' || *p == '-' || isdigit(*p))
				lastoffline = TRUE;
			else if ((options & SEARCH_OPT) &&
										(*p == 'e' || *p == 's' || *p == 'b'))
			{
				if (*p == 'e')			/* end */
					lastend = SEARCH_END;
				++p;
			}
			if (isdigit(*p) || *p == '+' || *p == '-')	   /* got an offset */
			{
				if (isdigit(*p) || isdigit(*(p + 1)))
					lastoff = atol((char *)p);		/* 'nr' or '+nr' or '-nr' */
				else if (*p == '-')			/* single '-' */
					lastoff = -1;
				else						/* single '+' */
					lastoff = 1;
				++p;
				while (isdigit(*p))			/* skip number */
					++p;
			}
			searchcmdlen = p - str;			/* compute length of search command
															for get_address() */
			str = p;						/* put str after search command */
		}

		if (options & SEARCH_ECHO)
		{
			msg_start();
			msg_outchar(dirc);
			msg_outtrans(*searchstr == NUL ? search_pattern : searchstr);
			if (lastoffline || lastend || lastoff)
			{
				msg_outchar(dirc);
				if (lastend)
					msg_outchar('e');
				else if (!lastoffline)
					msg_outchar('s');
				if (lastoff < 0)
				{
					msg_outchar('-');
					msg_outnum((long)-lastoff);
				}
				else if (lastoff > 0 || lastoffline)
				{
					msg_outchar('+');
					msg_outnum((long)lastoff);
				}
			}
			msg_clr_eos();
			(void)msg_check();

			gotocmdline(FALSE);
			flushbuf();
		}

		/*
		 * If there is a character offset, subtract it from the current
		 * position, so we don't get stuck at "?pat?e+2" or "/pat/s-2".
		 * This is not done for a line offset, because then we would not be vi
		 * compatible.
		 */
		if (!lastoffline && lastoff)
		{
			if (lastoff > 0)
			{
				c = lastoff;
				while (c--)
					if ((i = dec(&pos)) != 0)
						break;
				if (i == -1)				/* at start of buffer */
					goto_endofbuf(&pos);
			}
			else
			{
				c = -lastoff;
				while (c--)
					if ((i = inc(&pos)) != 0)
						break;
				if (i == -1)				/* at end of buffer */
				{
					pos.lnum = 1;
					pos.col = 0;
				}
			}
		}

		c = searchit(&pos, dirc == '/' ? FORWARD : BACKWARD, searchstr, count,
				lastend + (options &
					   (SEARCH_KEEP + SEARCH_HIS + SEARCH_MSG +
						   ((str != NULL && *str == ';') ? 0 : SEARCH_NOOF))),
				2);
		if (dircp != NULL)
			*dircp = dirc;		/* put second '/' or '?' back for normal() */
		if (c == FAIL)
		{
			retval = 0;
			goto end_do_search;
		}
		if (lastend)
			op_inclusive = TRUE;	/* 'e' includes last character */

		retval = 1;					/* pattern found */

		/*
		 * Add character and/or line offset
		 */
		if (!(options & SEARCH_NOOF) || *str == ';')
		{
			if (lastoffline)			/* Add the offset to the line number. */
			{
				c = pos.lnum + lastoff;
				if (c < 1)
					pos.lnum = 1;
				else if (c > curbuf->b_ml.ml_line_count)
					pos.lnum = curbuf->b_ml.ml_line_count;
				else
					pos.lnum = c;
				pos.col = 0;

				retval = 2;				/* pattern found, line offset added */
			}
			else
			{
				if (lastoff > 0)	/* to the right, check for end of line */
				{
					p = ml_get_pos(&pos) + 1;
					c = lastoff;
					while (c-- && *p++ != NUL)
						++pos.col;
				}
				else				/* to the left, check for start of line */
				{
					if ((c = pos.col + lastoff) < 0)
						c = 0;
					pos.col = c;
				}
			}
		}

		/*
		 * The search command can be followed by a ';' to do another search.
		 * For example: "/pat/;/foo/+3;?bar"
		 * This is like doing another search command, except:
		 * - The remembered direction '/' or '?' is from the first search.
		 * - When an error happens the cursor isn't moved at all.
		 * Don't do this when called by get_address() (it handles ';' itself).
		 */
		if (!(options & SEARCH_OPT) || str == NULL || *str != ';')
			break;

		dirc = *++str;
		if (dirc != '?' && dirc != '/')
		{
			retval = 0;
			EMSG("Expected '?' or '/'  after ';'");
			goto end_do_search;
		}
		++str;
	}

	if (options & SEARCH_MARK)
		setpcmark();
	curwin->w_cursor = pos;
	curwin->w_set_curswant = TRUE;

end_do_search:
	if (options & SEARCH_KEEP)
	{
		lastsdir = old_lastsdir;
		lastoffline = old_lastoffline;
		lastend = old_lastend;
		lastoff = old_lastoff;
	}
	return retval;
}

/*
 * search_for_exact_line(pos, dir, pat)
 *
 * Search for a line starting with the given pattern (ignoring leading
 * white-space), starting from pos and going in direction dir.	pos will
 * contain the position of the match found.    Blank lines will never match.
 * Return OK for success, or FAIL if no line found.
 */
	int
search_for_exact_line(pos, dir, pat)
	FPOS		*pos;
	int			dir;
	char_u		*pat;
{
	linenr_t	start = 0;
	char_u		*ptr;
	char_u		*p;

	if (curbuf->b_ml.ml_line_count == 0)
		return FAIL;
	for (;;)
	{
		pos->lnum += dir;
		if (pos->lnum < 1)
		{
			if (p_ws)
			{
				pos->lnum = curbuf->b_ml.ml_line_count;
				if (!shortmess(SHM_SEARCH))
					give_warning(top_bot_msg);
			}
			else
			{
				pos->lnum = 1;
				break;
			}
		}
		else if (pos->lnum > curbuf->b_ml.ml_line_count)
		{
			if (p_ws)
			{
				pos->lnum = 1;
				if (!shortmess(SHM_SEARCH))
					give_warning(bot_top_msg);
			}
			else
			{
				pos->lnum = 1;
				break;
			}
		}
		if (pos->lnum == start)
			break;
		if (start == 0)
			start = pos->lnum;
		ptr = ml_get(pos->lnum);
		p = skipwhite(ptr);
		pos->col = p - ptr;
		if (*p != NUL && STRNCMP(p, pat, STRLEN(pat)) == 0)
			return OK;
		else if (*p != NUL && p_ic)
		{
			ptr = pat;
			while (*p && TO_LOWER(*p) == TO_LOWER(*ptr))
			{
				++p;
				++ptr;
			}
			if (*ptr == NUL)
				return OK;
		}
	}
	return FAIL;
}

/*
 * Character Searches
 */

/*
 * searchc(c, dir, type, count)
 *
 * Search for character 'c', in direction 'dir'. If 'type' is 0, move to the
 * position of the character, otherwise move to just before the char.
 * Repeat this 'count' times.
 */
	int
searchc(c, dir, type, count)
	int				c;
	register int	dir;
	int				type;
	long			count;
{
	static int		lastc = NUL;	/* last character searched for */
	static int		lastcdir;		/* last direction of character search */
	static int		lastctype;		/* last type of search ("find" or "to") */
	register int	col;
	char_u			*p;
	int				len;

	if (c != NUL)		/* normal search: remember args for repeat */
	{
		if (!KeyStuffed)	/* don't remember when redoing */
		{
			lastc = c;
			lastcdir = dir;
			lastctype = type;
		}
	}
	else				/* repeat previous search */
	{
		if (lastc == NUL)
			return FALSE;
		if (dir)		/* repeat in opposite direction */
			dir = -lastcdir;
		else
			dir = lastcdir;
		type = lastctype;
		c = lastc;
	}

	p = ml_get_curline();
	col = curwin->w_cursor.col;
	len = STRLEN(p);

	while (count--)
	{
		for (;;)
		{
			if ((col += dir) < 0 || col >= len)
				return FALSE;
			if (p[col] == c)
				break;
		}
	}
	if (type)
		col -= dir;
	curwin->w_cursor.col = col;
	return TRUE;
}

/*
 * "Other" Searches
 */

/*
 * findmatch - find the matching paren or brace
 *
 * Improvement over vi: Braces inside quotes are ignored.
 */
	FPOS *
findmatch(initc)
	int		initc;
{
	return findmatchlimit(initc, 0, 0);
}

/*
 * findmatchlimit -- find the matching paren or brace, if it exists within
 * maxtravel lines of here.  A maxtravel of 0 means search until you fall off
 * the edge of the file.
 *
 * flags: FM_BACKWARD	search backwards (when initc is '/', '*' or '#')
 * 		  FM_FORWARD	search forwards (when initc is '/', '*' or '#')
 *		  FM_BLOCKSTOP	stop at start/end of block ({ or } in column 0)
 *		  FM_SKIPCOMM	skip comments (not implemented yet!)
 */

	FPOS *
findmatchlimit(initc, flags, maxtravel)
	int		initc;
	int		flags;
	int		maxtravel;
{
	static FPOS		pos;				/* current search position */
	int				findc;				/* matching brace */
	int				c;
	int				count = 0;			/* cumulative number of braces */
	int				idx = 0;			/* init for gcc */
	static char_u	table[6] = {'(', ')', '[', ']', '{', '}'};
#ifdef RIGHTLEFT
	static char_u   table_rl[6] = {')', '(', ']', '[', '}', '{'};
#endif
	int				inquote = FALSE;	/* TRUE when inside quotes */
	register char_u	*linep;				/* pointer to current line */
	char_u			*ptr;
	int				do_quotes;			/* check for quotes in current line */
	int				at_start;			/* do_quotes value at start position */
	int				hash_dir = 0;		/* Direction searched for # things */
	int				comment_dir = 0;	/* Direction searched for comments */
	FPOS			match_pos;			/* Where last slash-star was found */
	int				start_in_quotes;	/* start position is in quotes */
	int				traveled = 0;		/* how far we've searched so far */
	int				ignore_cend = FALSE;	/* ignore comment end */
	int				cpo_match;			/* vi compatible matching */
	int				dir;				/* Direction to search */

	pos = curwin->w_cursor;
	linep = ml_get(pos.lnum); 

	cpo_match = (vim_strchr(p_cpo, CPO_MATCH) != NULL);

	/* Direction to search when initc is '/', '*' or '#' */
	if (flags & FM_BACKWARD)
		dir = BACKWARD;
	else if (flags & FM_FORWARD)
		dir = FORWARD;
	else
		dir = 0;

	/*
	 * if initc given, look in the table for the matching character
	 * '/' and '*' are special cases: look for start or end of comment.
	 * When '/' is used, we ignore running backwards into an star-slash, for
	 * "[*" command, we just want to find any comment.
	 */
	if (initc == '/' || initc == '*')
	{
		comment_dir = dir;
		if (initc == '/')
			ignore_cend = TRUE;
		idx = (dir == FORWARD) ? 0 : 1;
		initc = NUL;
	}
	else if (initc != '#' && initc != NUL)
	{
		for (idx = 0; idx < 6; ++idx)
#ifdef RIGHTLEFT
			if ((curwin->w_p_rl ? table_rl : table)[idx] == initc)
			{
				initc = (curwin->w_p_rl ? table_rl : table)[idx = idx ^ 1];
				
#else
			if (table[idx] == initc)
			{
				initc = table[idx = idx ^ 1];
#endif
				break;
			}
		if (idx == 6)			/* invalid initc! */
			return NULL;
	}
	/*
	 * Either initc is '#', or no initc was given and we need to look under the
	 * cursor.
	 */
	else
	{
		if (initc == '#')
		{
			hash_dir = dir;
		}
		else
		{
			/*
			 * initc was not given, must look for something to match under
			 * or near the cursor.
			 */
			if (linep[0] == '#' && pos.col == 0)
			{
				/* If it's not #if, #else etc, we should look for a brace
				 * instead */
				for (c = 1; vim_iswhite(linep[c]); c++)
					;
				if (STRNCMP(linep + c, "if", (size_t)2) == 0 ||
					STRNCMP(linep + c, "endif", (size_t)5) == 0 ||
					STRNCMP(linep + c, "el", (size_t)2) == 0)
						hash_dir = 1;
			}

			/*
			 * Are we on a comment?
			 */
			else if (linep[pos.col] == '/')
			{
				if (linep[pos.col + 1] == '*')
				{
					comment_dir = 1;
					idx = 0;
					pos.col++;
				}
				else if (pos.col > 0 && linep[pos.col - 1] == '*')
				{
					comment_dir = -1;
					idx = 1;
					pos.col--;
				}
			}
			else if (linep[pos.col] == '*')
			{
				if (linep[pos.col + 1] == '/')
				{
					comment_dir = -1;
					idx = 1;
				}
				else if (pos.col > 0 && linep[pos.col - 1] == '/')
				{
					comment_dir = 1;
					idx = 0;
				}
			}

			/*
			 * If we are not on a comment or the # at the start of a line, then
			 * look for brace anywhere on this line after the cursor.
			 */
			if (!hash_dir && !comment_dir)
			{
				/*
				 * find the brace under or after the cursor
				 */
				linep = ml_get(pos.lnum); 
				idx = 6;					/* error if this line is empty */
				for (;;)
				{
					initc = linep[pos.col];
					if (initc == NUL)
						break;

					for (idx = 0; idx < 6; ++idx)
#ifdef RIGHTLEFT
						if ((curwin->w_p_rl ? table_rl : table)[idx] == initc)
#else
 						if (table[idx] == initc)
#endif
							break;
					if (idx != 6)
						break;
					++pos.col;
				}
				if (idx == 6)
				{
					if (linep[0] == '#')
						hash_dir = 1;
					else
						return NULL;
				}
			}
		}
		if (hash_dir)
		{
			/*
			 * Look for matching #if, #else, #elif, or #endif
			 */
			op_motion_type = MLINE;		/* Linewise for this case only */
			if (initc != '#')
			{
				ptr = skipwhite(linep + 1);
				if (STRNCMP(ptr, "if", (size_t)2) == 0 ||
									   STRNCMP(ptr, "el", (size_t)2) == 0)
					hash_dir = 1;
				else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
					hash_dir = -1;
				else
					return NULL;
			}
			pos.col = 0;
			while (!got_int)
			{
				if (hash_dir > 0)
				{
					if (pos.lnum == curbuf->b_ml.ml_line_count)
						break;
				}
				else if (pos.lnum == 1)
					break;
				pos.lnum += hash_dir;
				linep = ml_get(pos.lnum);
				line_breakcheck();
				if (linep[0] != '#')
					continue;
				ptr = linep + 1;
				while (*ptr == ' ' || *ptr == TAB)
					ptr++;
				if (hash_dir > 0)
				{
					if (STRNCMP(ptr, "if", (size_t)2) == 0)
						count++;
					else if (STRNCMP(ptr, "el", (size_t)2) == 0)
					{
						if (count == 0)
							return &pos;
					}
					else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
					{
						if (count == 0)
							return &pos;
						count--;
					}
				}
				else
				{
					if (STRNCMP(ptr, "if", (size_t)2) == 0)
					{
						if (count == 0)
							return &pos;
						count--;
					}
					else if (initc == '#' && STRNCMP(ptr, "el", (size_t)2) == 0)
					{
						if (count == 0)
							return &pos;
					}
					else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
						count++;
				}
			}
			return NULL;
		}
	}

#ifdef RIGHTLEFT
	findc = (curwin->w_p_rl ? table_rl : table)[idx ^ 1];
#else
 	findc = table[idx ^ 1];		/* get matching brace */
#endif
	idx &= 1;

	do_quotes = -1;
	start_in_quotes = MAYBE;
	while (!got_int)
	{
		/*
		 * Go to the next position, forward or backward. We could use
		 * inc() and dec() here, but that is much slower
		 */
		if (idx)						/* backward search */
		{
			if (pos.col == 0)			/* at start of line, go to prev. one */
			{
				if (pos.lnum == 1)		/* start of file */
					break;
				--pos.lnum;

				if (maxtravel && traveled++ > maxtravel)
					break;

				linep = ml_get(pos.lnum);
				pos.col = STRLEN(linep);	/* put pos.col on trailing NUL */
				do_quotes = -1;
				line_breakcheck();
			}
			else
				--pos.col;
		}
		else							/* forward search */
		{
			if (linep[pos.col] == NUL)	/* at end of line, go to next one */
			{
				if (pos.lnum == curbuf->b_ml.ml_line_count) /* end of file */
					break;
				++pos.lnum;

				if (maxtravel && traveled++ > maxtravel)
					break;

				linep = ml_get(pos.lnum);
				pos.col = 0;
				do_quotes = -1;
				line_breakcheck();
			}
			else
				++pos.col;
		}

		/*
		 * If FM_BLOCKSTOP given, stop at a '{' or '}' in column 0.
		 */
		if (pos.col == 0 && (flags & FM_BLOCKSTOP) &&
										 (linep[0] == '{' || linep[0] == '}'))
		{
			if (linep[0] == findc && count == 0)		/* match! */
				return &pos;
			break;										/* out of scope */
		}

		if (comment_dir)
		{
			/* Note: comments do not nest, and we ignore quotes in them */
			if (comment_dir == 1)
			{
				if (linep[pos.col] == '*' && linep[pos.col + 1] == '/')
				{
					pos.col++;
					return &pos;
				}
			}
			else	/* Searching backwards */
			{
				/*
				 * A comment may contain slash-star, it may also start or end
				 * with slash-star-slash.  I'm not using real examples though
				 * because "gcc -Wall" would complain -- webb
				 */
				if (pos.col == 0)
					continue;
				else if (linep[pos.col - 1] == '/' && linep[pos.col] == '*')
				{
					count++;
					match_pos = pos;
					match_pos.col--;
				}
				else if (linep[pos.col - 1] == '*' && linep[pos.col] == '/')
				{
					if (count > 0)
						pos = match_pos;
					else if (pos.col > 1 && linep[pos.col - 2] == '/')
						pos.col -= 2;
					else if (ignore_cend)
						continue;
					else
						return NULL;
					return &pos;
				}
			}
			continue;
		}

		/*
		 * If smart matching ('cpoptions' does not contain '%'), braces inside
		 * of quotes are ignored, but only if there is an even number of
		 * quotes in the line.
		 */
		if (cpo_match)
			do_quotes = 0;
		else if (do_quotes == -1)
		{
			/*
			 * count the number of quotes in the line, skipping \" and '"'
			 */
			at_start = do_quotes;
			for (ptr = linep; *ptr; ++ptr)
			{
				if (ptr == linep + curwin->w_cursor.col)
					at_start = (do_quotes & 1);
				if (*ptr == '"' && (ptr == linep || ptr[-1] != '\\') &&
							(ptr == linep || ptr[-1] != '\'' || ptr[1] != '\''))
					++do_quotes;
			}
			do_quotes &= 1;			/* result is 1 with even number of quotes */

			/*
			 * If we find an uneven count, check current line and previous
			 * one for a '\' at the end.
			 */
			if (!do_quotes)
			{
				inquote = FALSE;
				if (ptr[-1] == '\\')
				{
					do_quotes = 1;
					if (start_in_quotes == MAYBE)
					{
						inquote = !at_start;
						if (inquote)
							start_in_quotes = TRUE;
					}
					else if (idx)				/* backward search */
						inquote = TRUE;
				}
				if (pos.lnum > 1)
				{
					ptr = ml_get(pos.lnum - 1);
					if (*ptr && *(ptr + STRLEN(ptr) - 1) == '\\')
					{
						do_quotes = 1;
						if (start_in_quotes == MAYBE)
						{
							inquote = at_start;
							if (inquote)
								start_in_quotes = TRUE;
						}
						else if (!idx)				/* forward search */
							inquote = TRUE;
					}
				}
			}
		}
		if (start_in_quotes == MAYBE)
			start_in_quotes = FALSE;

		/*
		 * If 'smartmatch' is set:
		 *	 Things inside quotes are ignored by setting 'inquote'.  If we
		 *	 find a quote without a preceding '\' invert 'inquote'.  At the
		 *	 end of a line not ending in '\' we reset 'inquote'.
		 *
		 *	 In lines with an uneven number of quotes (without preceding '\')
		 *	 we do not know which part to ignore. Therefore we only set
		 *	 inquote if the number of quotes in a line is even, unless this
		 *	 line or the previous one ends in a '\'.  Complicated, isn't it?
		 */
		switch (c = linep[pos.col])
		{
		case NUL:
				/* at end of line without trailing backslash, reset inquote */
			if (pos.col == 0 || linep[pos.col - 1] != '\\')
			{
				inquote = FALSE;
				start_in_quotes = FALSE;
			}
			break;

		case '"':
				/* a quote that is preceded with a backslash is ignored */
			if (do_quotes && (pos.col == 0 || linep[pos.col - 1] != '\\'))
			{
				inquote = !inquote;
				start_in_quotes = FALSE;
			}
			break;

		/*
		 * If smart matching ('cpoptions' does not contain '%'):
		 *	 Skip things in single quotes: 'x' or '\x'.  Be careful for single
		 *	 single quotes, eg jon's.  Things like '\233' or '\x3f' are not
		 *	 skipped, there is never a brace in them.
		 */
		case '\'':
			if (vim_strchr(p_cpo, CPO_MATCH) == NULL)
			{
				if (idx)						/* backward search */
				{
					if (pos.col > 1)
					{
						if (linep[pos.col - 2] == '\'')
							pos.col -= 2;
						else if (linep[pos.col - 2] == '\\' &&
									pos.col > 2 && linep[pos.col - 3] == '\'')
							pos.col -= 3;
					}
				}
				else if (linep[pos.col + 1])	/* forward search */
				{
					if (linep[pos.col + 1] == '\\' &&
							linep[pos.col + 2] && linep[pos.col + 3] == '\'')
						pos.col += 3;
					else if (linep[pos.col + 2] == '\'')
						pos.col += 2;
				}
			}
			break;

		default:
					/* Check for match outside of quotes, and inside of
					 * quotes when the start is also inside of quotes */
			if (!inquote || start_in_quotes == TRUE)
			{
				if (c == initc)
					count++;
				else if (c == findc)
				{
					if (count == 0)
						return &pos;
					count--;
				}
			}
		}
	}

	if (comment_dir == -1 && count > 0)
	{
		pos = match_pos;
		return &pos;
	}
	return (FPOS *) NULL;		/* never found it */
}

/*
 * Move cursor briefly to character matching the one under the cursor.
 * Show the match only if it is visible on the screen.
 */
	void
showmatch()
{
	FPOS		   *lpos, csave;
	colnr_t			vcol;

	if ((lpos = findmatch(NUL)) == NULL)		/* no match, so beep */
		beep_flush();
	else if (lpos->lnum >= curwin->w_topline)
	{
		if (!curwin->w_p_wrap)
			getvcol(curwin, lpos, NULL, &vcol, NULL);
		if (curwin->w_p_wrap || (vcol >= curwin->w_leftcol &&
										  vcol < curwin->w_leftcol + Columns))
		{
			updateScreen(VALID_TO_CURSCHAR); /* show the new char first */
			csave = curwin->w_cursor;
			curwin->w_cursor = *lpos;	/* move to matching char */
			cursupdate();
			showruler(0);
			setcursor();
			cursor_on();				/* make sure that the cursor is shown */
			flushbuf();

			/*
			 * brief pause, unless 'm' is present in 'cpo' and a character is
			 * available.
			 */
			if (vim_strchr(p_cpo, CPO_SHOWMATCH) != NULL)
				mch_delay(500L, TRUE);
			else if (!char_avail())
				mch_delay(500L, FALSE);
			curwin->w_cursor = csave;	/* restore cursor position */
			cursupdate();
		}
	}
}

/*
 * findsent(dir, count) - Find the start of the next sentence in direction
 * 'dir' Sentences are supposed to end in ".", "!" or "?" followed by white
 * space or a line break. Also stop at an empty line.
 * Return OK if the next sentence was found.
 */
	int
findsent(dir, count)
	int		dir;
	long	count;
{
	FPOS			pos, tpos;
	register int	c;
	int				(*func) __PARMS((FPOS *));
	int				startlnum;
	int				noskip = FALSE;			/* do not skip blanks */

	pos = curwin->w_cursor;
	if (dir == FORWARD)
		func = incl;
	else
		func = decl;

	while (count--)
	{
		/*
		 * if on an empty line, skip upto a non-empty line
		 */
		if (gchar(&pos) == NUL)
		{
			do
				if ((*func)(&pos) == -1)
					break;
			while (gchar(&pos) == NUL);
			if (dir == FORWARD)
				goto found;
		}
		/*
		 * if on the start of a paragraph or a section and searching forward,
		 * go to the next line
		 */
		else if (dir == FORWARD && pos.col == 0 &&
												startPS(pos.lnum, NUL, FALSE))
		{
			if (pos.lnum == curbuf->b_ml.ml_line_count)
				return FAIL;
			++pos.lnum;
			goto found;
		}
		else if (dir == BACKWARD)
			decl(&pos);

		/* go back to the previous non-blank char */
		while ((c = gchar(&pos)) == ' ' || c == '\t' ||
			 (dir == BACKWARD && vim_strchr((char_u *)".!?)]\"'", c) != NULL))
		{
			if (decl(&pos) == -1)
				break;
			/* when going forward: Stop in front of empty line */
			if (lineempty(pos.lnum) && dir == FORWARD)
			{
				incl(&pos);
				goto found;
			}
		}

		/* remember the line where the search started */
		startlnum = pos.lnum;

		for (;;)				/* find end of sentence */
		{
			c = gchar(&pos);
			if (c == NUL || (pos.col == 0 && startPS(pos.lnum, NUL, FALSE)))
			{
				if (dir == BACKWARD && pos.lnum != startlnum)
					++pos.lnum;
				break;
			}
			if (c == '.' || c == '!' || c == '?')
			{
				tpos = pos;
				do
					if ((c = inc(&tpos)) == -1)
						break;
				while (vim_strchr((char_u *)")]\"'", c = gchar(&tpos)) != NULL);
				if (c == -1  || c == ' ' || c == '\t' || c == NUL)
				{
					pos = tpos;
					if (gchar(&pos) == NUL) /* skip NUL at EOL */
						inc(&pos);
					break;
				}
			}
			if ((*func)(&pos) == -1)
			{
				if (count)
					return FAIL;
				noskip = TRUE;
				break;
			}
		}
found:
			/* skip white space */
		while (!noskip && ((c = gchar(&pos)) == ' ' || c == '\t'))
			if (incl(&pos) == -1)
				break;
	}

	setpcmark();
	curwin->w_cursor = pos;
	return OK;
}

/*
 * findpar(dir, count, what) - Find the next paragraph in direction 'dir'
 * Paragraphs are currently supposed to be separated by empty lines.
 * Return TRUE if the next paragraph was found.
 * If 'what' is '{' or '}' we go to the next section.
 * If 'both' is TRUE also stop at '}'.
 */
	int
findpar(dir, count, what, both)
	register int	dir;
	long			count;
	int				what;
	int				both;
{
	register linenr_t	curr;
	int					did_skip;		/* TRUE after separating lines have
												been skipped */
	int					first;			/* TRUE on first line */

	curr = curwin->w_cursor.lnum;

	while (count--)
	{
		did_skip = FALSE;
		for (first = TRUE; ; first = FALSE)
		{
			if (*ml_get(curr) != NUL)
				did_skip = TRUE;

			if (!first && did_skip && startPS(curr, what, both))
				break;

			if ((curr += dir) < 1 || curr > curbuf->b_ml.ml_line_count)
			{
				if (count)
					return FALSE;
				curr -= dir;
				break;
			}
		}
	}
	setpcmark();
	if (both && *ml_get(curr) == '}')	/* include line with '}' */
		++curr;
	curwin->w_cursor.lnum = curr;
	if (curr == curbuf->b_ml.ml_line_count)
	{
		if ((curwin->w_cursor.col = STRLEN(ml_get(curr))) != 0)
		{
			--curwin->w_cursor.col;
			op_inclusive = TRUE;
		}
	}
	else
		curwin->w_cursor.col = 0;
	return TRUE;
}

/*
 * check if the string 's' is a nroff macro that is in option 'opt'
 */
	static int
inmacro(opt, s)
	char_u			*opt;
	register char_u *s;
{
	register char_u *macro;

	for (macro = opt; macro[0]; ++macro)
	{
		if (macro[0] == s[0] && (((s[1] == NUL || s[1] == ' ') &&
				   (macro[1] == NUL || macro[1] == ' ')) || macro[1] == s[1]))
			break;
		++macro;
		if (macro[0] == NUL)
			break;
	}
	return (macro[0] != NUL);
}

/*
 * startPS: return TRUE if line 'lnum' is the start of a section or paragraph.
 * If 'para' is '{' or '}' only check for sections.
 * If 'both' is TRUE also stop at '}'
 */
	int
startPS(lnum, para, both)
	linenr_t	lnum;
	int			para;
	int			both;
{
	register char_u *s;

	s = ml_get(lnum);
	if (*s == para || *s == '\f' || (both && *s == '}'))
		return TRUE;
	if (*s == '.' && (inmacro(p_sections, s + 1) ||
										   (!para && inmacro(p_para, s + 1))))
		return TRUE;
	return FALSE;
}

/*
 * The following routines do the word searches performed by the 'w', 'W',
 * 'b', 'B', 'e', and 'E' commands.
 */

/*
 * To perform these searches, characters are placed into one of three
 * classes, and transitions between classes determine word boundaries.
 *
 * The classes are:
 *
 * 0 - white space
 * 1 - keyword charactes (letters, digits and underscore)
 * 2 - everything else
 */

static int		stype;			/* type of the word motion being performed */

/*
 * cls() - returns the class of character at curwin->w_cursor
 *
 * The 'type' of the current search modifies the classes of characters if a
 * 'W', 'B', or 'E' motion is being done. In this case, chars. from class 2
 * are reported as class 1 since only white space boundaries are of interest.
 */
	static int
cls()
{
	register int c;

	c = gchar_cursor();
	if (c == ' ' || c == '\t' || c == NUL)
		return 0;

	if (iswordchar(c))
		return 1;

	/*
	 * If stype is non-zero, report these as class 1.
	 */
	return (stype == 0) ? 2 : 1;
}


/*
 * fwd_word(count, type, eol) - move forward one word
 *
 * Returns FAIL if the cursor was already at the end of the file.
 * If eol is TRUE, last word stops at end of line (for operators).
 */
	int
fwd_word(count, type, eol)
	long		count;
	int			type;
	int			eol;
{
	int			sclass;		/* starting class */
	int			i;
	int			last_line;

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();

		/*
		 * We always move at least one character, unless on the last character
		 * in the buffer.
		 */
		last_line = (curwin->w_cursor.lnum == curbuf->b_ml.ml_line_count);
		i = inc_cursor();
		if (i == -1 || (i == 1 && last_line))
											/* started at last char in file */
			return FAIL;
		if (i == 1 && eol && count == 0)	/* started at last char in line */
			return OK;

		/*
		 * Go one char past end of current word (if any)
		 */
		if (sclass != 0)
			while (cls() == sclass)
			{
				i = inc_cursor();
				if (i == -1 || (i == 1 && eol && count == 0))
					return OK;
			}

		/*
		 * go to next non-white
		 */
		while (cls() == 0)
		{
			/*
			 * We'll stop if we land on a blank line
			 */
			if (curwin->w_cursor.col == 0 && *ml_get_curline() == NUL)
				break;

			i = inc_cursor();
			if (i == -1 || (i == 1 && eol && count == 0))
				return OK;
		}
	}
	return OK;
}

/*
 * bck_word() - move backward 'count' words
 *
 * If stop is TRUE and we are already on the start of a word, move one less.
 *
 * Returns FAIL if top of the file was reached.
 */
	int
bck_word(count, type, stop)
	long		count;
	int			type;
	int			stop;
{
	int			sclass;		/* starting class */

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();
		if (dec_cursor() == -1)		/* started at start of file */
			return FAIL;

		if (!stop || sclass == cls() || sclass == 0)
		{
			/*
			 * Skip white space before the word.
			 * Stop on an empty line.
			 */
			while (cls() == 0)
			{
				if (curwin->w_cursor.col == 0 &&
											 lineempty(curwin->w_cursor.lnum))
					goto finished;

				if (dec_cursor() == -1)		/* hit start of file, stop here */
					return OK;
			}

			/*
			 * Move backward to start of this word.
			 */
			if (skip_chars(cls(), BACKWARD))
				return OK;
		}

		inc_cursor();					 /* overshot - forward one */
finished:
		stop = FALSE;
	}
	return OK;
}

/*
 * end_word() - move to the end of the word
 *
 * There is an apparent bug in the 'e' motion of the real vi. At least on the
 * System V Release 3 version for the 80386. Unlike 'b' and 'w', the 'e'
 * motion crosses blank lines. When the real vi crosses a blank line in an
 * 'e' motion, the cursor is placed on the FIRST character of the next
 * non-blank line. The 'E' command, however, works correctly. Since this
 * appears to be a bug, I have not duplicated it here.
 *
 * Returns FAIL if end of the file was reached.
 *
 * If stop is TRUE and we are already on the end of a word, move one less.
 * If empty is TRUE stop on an empty line.
 */
	int
end_word(count, type, stop, empty)
	long		count;
	int			type;
	int			stop;
	int			empty;
{
	int			sclass;		/* starting class */

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();
		if (inc_cursor() == -1)
			return FAIL;

		/*
		 * If we're in the middle of a word, we just have to move to the end
		 * of it.
		 */
		if (cls() == sclass && sclass != 0)
		{
			/*
			 * Move forward to end of the current word
			 */
			if (skip_chars(sclass, FORWARD))
				return FAIL;
		}
		else if (!stop || sclass == 0)
		{
			/*
			 * We were at the end of a word. Go to the end of the next word.
			 * First skip white space, if 'empty' is TRUE, stop at empty line.
			 */
			while (cls() == 0)
			{
				if (empty && curwin->w_cursor.col == 0 &&
											 lineempty(curwin->w_cursor.lnum))
					goto finished;
				if (inc_cursor() == -1)		/* hit end of file, stop here */
					return FAIL;
			}

			/*
			 * Move forward to the end of this word.
			 */
			if (skip_chars(cls(), FORWARD))
				return FAIL;
		}
		dec_cursor();					/* overshot - one char backward */
finished:
		stop = FALSE;					/* we move only one word less */
	}
	return OK;
}

/*
 * bckend_word(count, type) - move back to the end of the word
 *
 * If 'eol' is TRUE, stop at end of line.
 *
 * Returns FAIL if start of the file was reached.
 */
	int
bckend_word(count, type, eol)
	long		count;
	int			type;
	int			eol;
{
	int			sclass;		/* starting class */
	int			i;

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();
		if ((i = dec_cursor()) == -1)
			return FAIL;
		if (eol && i == 1)
			return OK;

		/*
		 * Move backward to before the start of this word.
		 */
		if (sclass != 0)
		{
			while (cls() == sclass)
				if ((i = dec_cursor()) == -1 || (eol && i == 1))
					return OK;
		}

		/*
		 * Move backward to end of the previous word
		 */
		while (cls() == 0)
		{
			if (curwin->w_cursor.col == 0 && lineempty(curwin->w_cursor.lnum))
				break;
			if ((i = dec_cursor()) == -1 || (eol && i == 1))
				return OK;
		}
	}
	return OK;
}

	static int
skip_chars(cclass, dir)
	int		cclass;
	int		dir;
{
	while (cls() == cclass)
		if ((dir == FORWARD ? inc_cursor() : dec_cursor()) == -1)
			return TRUE;
	return FALSE;
}

/*
 * Go back to the start of the word or the start of white space
 */
	static void
back_in_line()
{
	int			sclass;				/* starting class */

	sclass = cls();
	for (;;)
	{
		if (curwin->w_cursor.col == 0)		/* stop at start of line */
			break;
		--curwin->w_cursor.col;
		if (cls() != sclass)				/* stop at start of word */
		{
			++curwin->w_cursor.col;
			break;
		}
	}
}

/*
 * Find word under cursor, cursor at end
 */
	int
current_word(count, type)
	long		count;
	int			type;
{
	FPOS		start;
	FPOS		pos;
	int			inclusive = TRUE;

	stype = type;

	/*
	 * When visual area is bigger than one character: Extend it.
	 */
	if (VIsual_active && !equal(curwin->w_cursor, VIsual))
	{
		if (lt(curwin->w_cursor, VIsual))
		{
			if (decl(&curwin->w_cursor) == -1)
				return FAIL;
			if (cls() == 0)
			{
				if (bckend_word(count, type, TRUE) == FAIL)
					return FAIL;
				(void)incl(&curwin->w_cursor);
			}
			else
			{
				if (bck_word(count, type, TRUE) == FAIL)
					return FAIL;
			}
		}
		else
		{
			if (incl(&curwin->w_cursor) == -1)
				return FAIL;
			if (cls() == 0)
			{
				if (fwd_word(count, type, TRUE) == FAIL)
					return FAIL;
				(void)oneleft();
			}
			else
			{
				if (end_word(count, type, TRUE, TRUE) == FAIL)
					return FAIL;
			}
		}
		return OK;
	}

	/*
	 * Go to start of current word or white space.
	 */
	back_in_line();
	start = curwin->w_cursor;

	/*
	 * If the start is on white space, find end of word.
	 * Otherwise find start of next word.
	 */
	if (cls() == 0)
	{
		if (end_word(count, type, TRUE, TRUE) == FAIL)
			return FAIL;
	}
	else
	{
		if (fwd_word(count, type, TRUE) == FAIL)
			return FAIL;
		/*
		 * If end is just past a new-line, we don't want to include the first
		 * character on the line
		 */
		if (oneleft() == FAIL)			/* put cursor on last char of area */
			inclusive = FALSE;
		else
		{
			pos = curwin->w_cursor;		/* save cursor position */
			/*
			 * If we don't include white space at the end, move the start to 
			 * include some white space there. This makes "d." work better on
			 * the last word in a sentence. Don't delete white space at start
			 * of line (indent).
			 */
			if (cls() != 0)
			{
				curwin->w_cursor = start;
				if (oneleft() == OK)
				{
					back_in_line();
					if (cls() == 0 && curwin->w_cursor.col > 0)
						start = curwin->w_cursor;
				}
			}
			curwin->w_cursor = pos;		/* put cursor back at end */
		}
	}
	if (VIsual_active)
	{
		/* should do something when inclusive == FALSE ! */
		VIsual = start;
		VIsual_mode = 'v';
		update_curbuf(NOT_VALID);		/* update the inversion */
	}
	else
	{
		curbuf->b_op_start = start;
		op_motion_type = MCHAR;
		op_inclusive = inclusive;
	}
	return OK;
}

/*
 * Find sentence under the cursor, cursor at end.
 */
	int
current_sent(count)
	long	count;
{
	FPOS	start;
	FPOS	pos;
	int		start_blank;
	int		c;

	pos = curwin->w_cursor;
	start = pos;

	/*
	 * When visual area is bigger than one character: Extend it.
	 */
	if (VIsual_active && !equal(curwin->w_cursor, VIsual))
	{
		if (lt(pos, VIsual))
		{
			/*
			 * Do a "sentence backward" on the next character.
			 * If we end up on the same position, we were already at the start
			 * of a sentence
			 */
			if (incl(&curwin->w_cursor) == -1)
				return FAIL;
			findsent(BACKWARD, 1L);
			start = curwin->w_cursor;
			if (count > 1)
				findsent(BACKWARD, count - 1);
			/*
			 * When at start of sentence: Include blanks in front of sentence.
			 * Use current_word() to cross line boundaries.
			 * If we don't end up on a blank or on an empty line, go back to
			 * the start of the previous sentence.
			 */
			if (equal(pos, start))
			{
				current_word(1L, 0);
				c = gchar_cursor();
				if (c != NUL && !vim_iswhite(c))
					findsent(BACKWARD, 1L);
			}

		}
		else
		{
			/*
			 * When one char before start of sentence: Don't include blanks in
			 * front of next sentence.
			 * Else go count sentences forward.
			 */
			findsent(FORWARD, 1L);
			incl(&pos);
			if (equal(pos, curwin->w_cursor))
			{
				findsent(FORWARD, count);
				find_first_blank(&curwin->w_cursor);
			}
			else if (count > 1)
				findsent(FORWARD, count - 1);
			decl(&curwin->w_cursor);
		}
		return OK;
	}

	/*
	 * Find start of next sentence.
	 */
	findsent(FORWARD, 1L);

	/*
	 * If cursor started on blank, check if it is just before the start of the
	 * next sentence.
	 */
	while (vim_iswhite(gchar(&pos)))
		incl(&pos);
	if (equal(pos, curwin->w_cursor))
	{
		start_blank = TRUE;
		/*
		 * go back to first blank
		 */
		while (decl(&start) != -1)
		{
			if (!vim_iswhite(gchar(&start)))
			{
				incl(&start);
				break;
			}
		}
	}
	else
	{
		start_blank = FALSE;
		findsent(BACKWARD, 1L);
		start = curwin->w_cursor;
	}
	findsent(FORWARD, count);

	/*
	 * If the blank in front of the sentence is included, exclude the blanks
	 * at the end of the sentence, go back to the first blank.
	 */
	if (start_blank)
		find_first_blank(&curwin->w_cursor);
	else
	{
		/*
		 * If there are no trailing blanks, try to include leading blanks
		 */
		pos = curwin->w_cursor;
		decl(&pos);
		if (!vim_iswhite(gchar(&pos)))
			find_first_blank(&start);
	}

	if (VIsual_active)
	{
		VIsual = start;
		VIsual_mode = 'v';
		decl(&curwin->w_cursor);		/* don't include the cursor char */
		update_curbuf(NOT_VALID);		/* update the inversion */
	}
	else
	{
		curbuf->b_op_start = start;
		op_motion_type = MCHAR;
		op_inclusive = FALSE;
	}
	return OK;
}

	int
current_block(what, count)
	int		what;			/* '(' or '{' */
	long	count;
{
	FPOS	old_pos;
	FPOS	*pos = NULL;
	FPOS	start_pos;
	FPOS	*end_pos;
	FPOS	old_start, old_end;
	int		inclusive = FALSE;
	int		other;

	old_pos = curwin->w_cursor;
	if (what == '{')
		other = '}';
	else
		other = ')';

	old_end = curwin->w_cursor;				/* remember where we started */
	old_start = old_end;

	/*
	 * If we start on '(', '{', ')' or '}', use the whole block inclusive.
	 */
	if (!VIsual_active || (VIsual.lnum == curwin->w_cursor.lnum &&
										  VIsual.col == curwin->w_cursor.col))
	{
		if (what == '{')					/* ignore indent */
			while (inindent(1))
				if (inc_cursor() != 0)
					break;
		if (gchar_cursor() == what)			/* cursor on '(' or '{' */
		{
			++curwin->w_cursor.col;
			inclusive = TRUE;
		}
		else if (gchar_cursor() == other)	/* cursor on ')' or '}' */
			inclusive = TRUE;
	}
	else if (lt(VIsual, curwin->w_cursor))
	{
		old_start = VIsual;
		curwin->w_cursor = VIsual;			/* cursor at low end of Visual */
	}
	else
		old_end = VIsual;

	/*
	 * Search backwards for unclosed '(' or '{'.
	 * put this position in start_pos.
	 */
	while (count--)
	{
		if ((pos = findmatch(what)) == NULL)
			break;
		curwin->w_cursor = *pos;
		start_pos = *pos;	/* the findmatch for end_pos will overwrite *pos */
	}

	/*
	 * Search for matching ')' or '}'.
	 * Put this position in curwin->w_cursor.
	 */
	if (pos == NULL || (end_pos = findmatch(other)) == NULL)
	{
		curwin->w_cursor = old_pos;
		return FAIL;
	}
	curwin->w_cursor = *end_pos;

	/*
	 * Try to exclude the '(', '{', ')' and '}'.
	 * If the ending '}' is only preceded by indent, skip that indent.
	 * But only if the resulting area is not smaller than what we started with.
	 */
	if (!inclusive)
	{
		incl(&start_pos);
		old_pos = curwin->w_cursor;
		decl(&curwin->w_cursor);
		if (what == '{')
			while (inindent(0))
				if (decl(&curwin->w_cursor) != 0)
					break;
		if (!lt(start_pos, old_start) && !lt(old_end, curwin->w_cursor))
		{
			decl(&start_pos);
			curwin->w_cursor = old_pos;
		}
	}

	if (VIsual_active)
	{
		VIsual = start_pos;
		VIsual_mode = 'v';
		update_curbuf(NOT_VALID);		/* update the inversion */
		showmode();
	}
	else
	{
		curbuf->b_op_start = start_pos;
		op_motion_type = MCHAR;
		op_inclusive = TRUE;
	}

	return OK;
}

	int
current_par(type, count)
	int		type;			/* 'p' for paragraph, 'S' for section */
	long	count;
{
	linenr_t	start;
	linenr_t	end;
	int			white_in_front;
	int			dir;
	int			start_is_white;
	int			retval = OK;

	if (type == 'S')		/* not implemented yet */
		return FAIL;
	
	start = curwin->w_cursor.lnum;

	/*
	 * When visual area is more than one line: extend it.
	 */
	if (VIsual_active && start != VIsual.lnum)
	{
		if (start < VIsual.lnum)
			dir = BACKWARD;
		else
			dir = FORWARD;
		while (count--)
		{
			if (start == (dir == BACKWARD ? 1 : curbuf->b_ml.ml_line_count))
				retval = FAIL;

			start_is_white = -1;
			for (;;)
			{
				if (start == (dir == BACKWARD ? 1 : curbuf->b_ml.ml_line_count))
					break;
				if (start_is_white >= 0 &&
								  (start_is_white != linewhite(start + dir) ||
									startPS(start + (dir > 0 ? 1 : 0), 0, 0)))
					break;
				start += dir;
				if (start_is_white < 0)
					start_is_white = linewhite(start);
			}
		}
		curwin->w_cursor.lnum = start;
		curwin->w_cursor.col = 0;
		return retval;
	}

	/*
	 * First move back to the start of the paragraph or white lines
	 */
	white_in_front = linewhite(start);
	while (start > 1)
	{
		if (white_in_front)			/* stop at first white line */
		{
			if (!linewhite(start - 1))
				break;
		}
		else			/* stop at first non-white line of start of paragraph */
		{
			if (linewhite(start - 1) || startPS(start, 0, 0))
				break;
		}
		--start;
	}

	/*
	 * Move past the end of the white lines.
	 */
	end = start;
	while (linewhite(end) && end < curbuf->b_ml.ml_line_count)
		++end;
	
	--end;
	while (count--)
	{
		if (end == curbuf->b_ml.ml_line_count)
			return FAIL;

		++end;
		/*
		 * skip to end of paragraph
		 */
		while (end < curbuf->b_ml.ml_line_count &&
							   !linewhite(end + 1) && !startPS(end + 1, 0, 0))
			++end;

		if (count == 0 && white_in_front)
			break;

		/*
		 * skip to end of white lines after paragraph
		 */
		while (end < curbuf->b_ml.ml_line_count && linewhite(end + 1))
			++end;
	}

	/*
	 * If there are no empty lines at the end, try to find some empty lines at
	 * the start (unless that has been done already).
	 */
	if (!white_in_front && !linewhite(end))
		while (start > 1 && linewhite(start - 1))
			--start;

	if (VIsual_active)
	{
		VIsual.lnum = start;
		VIsual_mode = 'V';
		update_curbuf(NOT_VALID);		/* update the inversion */
		showmode();
	}
	else
	{
		curbuf->b_op_start.lnum = start;
		op_motion_type = MLINE;
	}
	curwin->w_cursor.lnum = end;
	curwin->w_cursor.col = 0;

	return OK;
}

/*
 * linewhite -- return TRUE if line 'lnum' is empty or has white chars only.
 */
	int
linewhite(lnum)
	linenr_t	lnum;
{
	char_u	*p;

	p = skipwhite(ml_get(lnum));
	return (*p == NUL);
}

	static void
find_first_blank(posp)
	FPOS	*posp;
{
	while (decl(posp) != -1)
	{
		if (!vim_iswhite(gchar(posp)))
		{
			incl(posp);
			break;
		}
	}
}

	void
find_pattern_in_path(ptr, len, whole, skip_comments,
									type, count, action, start_lnum, end_lnum)
	char_u	*ptr;			/* pointer to search pattern */
	int		len;			/* length of search pattern */
	int		whole;			/* match whole words only */
	int		skip_comments;	/* don't match inside comments */
	int		type;			/* Type of search; are we looking for a type?  a
								macro? */
	long	count;
	int		action;			/* What to do when we find it */
	linenr_t	start_lnum;	/* first line to start searching */
	linenr_t	end_lnum;	/* last line for searching */
{
	SearchedFile *files;				/* Stack of included files */
	SearchedFile *bigger;				/* When we need more space */
	int			max_path_depth = 50;
	long		match_count = 1;

	char_u		*pat;
	char_u		*new_fname;
	char_u		*curr_fname = curbuf->b_xfilename;
	char_u		*prev_fname = NULL;
	linenr_t	lnum;
	int			depth;
	int			depth_displayed;		/* For type==CHECK_PATH */
	int			old_files;
	int			already_searched;
	char_u		*file_line;
	char_u		*line;
	char_u		*p;
	char_u		*p2 = NUL;				/* Init for gcc */
	char_u		save_char = NUL;
	int			define_matched;
	struct regexp *prog = NULL;
	struct regexp *include_prog = NULL;
	struct regexp *define_prog = NULL;
	int			matched = FALSE;
	int			did_show = FALSE;
	int			found = FALSE;
	int			i;

	file_line = alloc(LSIZE);
	if (file_line == NULL)
		return;

	reg_magic = p_magic;
	if (type != CHECK_PATH)
	{
		pat = alloc(len + 5);
		if (pat == NULL)
			goto fpip_end;
		sprintf((char *)pat, whole ? "\\<%.*s\\>" : "%.*s", len, ptr);
		set_reg_ic(pat);	/* set reg_ic according to p_ic, p_scs and pat */
		prog = vim_regcomp(pat);
		vim_free(pat);
		if (prog == NULL)
			goto fpip_end;
	}
	reg_ic = FALSE;		/* don't ignore case in include and define patterns */
	if (*p_inc != NUL)
	{
		include_prog = vim_regcomp(p_inc);
		if (include_prog == NULL)
			goto fpip_end;
	}
	if (type == FIND_DEFINE && *p_def != NUL)
	{
		define_prog = vim_regcomp(p_def);
		if (define_prog == NULL)
			goto fpip_end;
	}
	files = (SearchedFile *)lalloc(max_path_depth * sizeof(SearchedFile), TRUE);
	if (files == NULL)
		goto fpip_end;
	for (i = 0; i < max_path_depth; i++)
	{
		files[i].fp = NULL;
		files[i].name = NULL;
		files[i].lnum = 0;
		files[i].matched = FALSE;
	}
	old_files = max_path_depth;
	depth = depth_displayed = -1;

	lnum = start_lnum;
	if (end_lnum > curbuf->b_ml.ml_line_count)
		end_lnum = curbuf->b_ml.ml_line_count;
	if (lnum > end_lnum)				/* do at least one line */
		lnum = end_lnum;
	line = ml_get(lnum);

	for (;;)
	{
		if (include_prog != NULL && vim_regexec(include_prog, line, TRUE))
		{
			new_fname = get_file_name_in_path(include_prog->endp[0] + 1,
																0, FNAME_EXP);
			already_searched = FALSE;
			if (new_fname != NULL)
			{
				/* Check whether we have already searched in this file */
				for (i = 0;; i++)
				{
					if (i == depth + 1)
						i = old_files;
					if (i == max_path_depth)
						break;
					if (STRCMP(new_fname, files[i].name) == 0)
					{
						if (type != CHECK_PATH &&
								action == ACTION_SHOW_ALL && files[i].matched)
						{
							msg_outchar('\n');		/* cursor below last one */
							if (!got_int)			/* don't display if 'q'
													   typed at "--more--"
													   mesage */
							{
								set_highlight('d');	/* Same as for dirs */
								start_highlight();
								msg_home_replace(new_fname);
								stop_highlight();
								MSG_OUTSTR(" (includes previously listed match)");
								prev_fname = NULL;
							}
						}
						vim_free(new_fname);
						new_fname = NULL;
						already_searched = TRUE;
						break;
					}
				}
			}

			if (type == CHECK_PATH && (action == ACTION_SHOW_ALL ||
									(new_fname == NULL && !already_searched)))
			{
				if (did_show)
					msg_outchar('\n');		/* cursor below last one */
				else
				{
					gotocmdline(TRUE);		/* cursor at status line */
					set_highlight('t');		/* Highlight title */
					start_highlight();
					MSG_OUTSTR("--- Included files ");
					if (action != ACTION_SHOW_ALL)
						MSG_OUTSTR("not found ");
					MSG_OUTSTR("in path ---\n");
					stop_highlight();
				}
				did_show = TRUE;
				while (depth_displayed < depth && !got_int)
				{
					++depth_displayed;
					for (i = 0; i < depth_displayed; i++)
						MSG_OUTSTR("  ");
					msg_home_replace(files[depth_displayed].name);
					MSG_OUTSTR(" -->\n");
				}
				if (!got_int)				/* don't display if 'q' typed
											   for "--more--" message */
				{
					for (i = 0; i <= depth_displayed; i++)
						MSG_OUTSTR("  ");
					set_highlight('d');		/* Same as for directories */
					start_highlight();
					/*
					 * Isolate the file name.
					 * Include the surrounding "" or <> if present.
					 */
					for (p = include_prog->endp[0] + 1; !isfilechar(*p); p++)
						;
					for (i = 0; isfilechar(p[i]); i++)
						;
					if (p[-1] == '"' || p[-1] == '<')
					{
						--p;
						++i;
					}
					if (p[i] == '"' || p[i] == '>')
						++i;
					save_char = p[i];
					p[i] = NUL;
					msg_outstr(p);
					p[i] = save_char;
					stop_highlight();
					if (new_fname == NULL && action == ACTION_SHOW_ALL)
					{
						if (already_searched)
							MSG_OUTSTR("  (Already listed)");
						else
							MSG_OUTSTR("  NOT FOUND");
					}
				}
				flushbuf();			/* output each line directly */
			}

			if (new_fname != NULL)
			{
				/* Push the new file onto the file stack */
				if (depth + 1 == old_files)
				{
					bigger = (SearchedFile *)lalloc(max_path_depth * 2
												* sizeof(SearchedFile), TRUE);
					if (bigger != NULL)
					{
						for (i = 0; i <= depth; i++)
							bigger[i] = files[i];
						for (i = depth + 1; i < old_files + max_path_depth; i++)
						{
							bigger[i].fp = NULL;
							bigger[i].name = NULL;
							bigger[i].lnum = 0;
							bigger[i].matched = FALSE;
						}
						for (i = old_files; i < max_path_depth; i++)
							bigger[i + max_path_depth] = files[i];
						old_files += max_path_depth;
						max_path_depth *= 2;
						vim_free(files);
						files = bigger;
					}
				}
				if ((files[depth + 1].fp = fopen((char *)new_fname, "r"))
																	== NULL)
					vim_free(new_fname);
				else
				{
					if (++depth == old_files)
					{
						/*
						 * lalloc() for 'bigger' must have failed above.  We
						 * will forget one of our already visited files now.
						 */
						vim_free(files[old_files].name);
						++old_files;
					}
					files[depth].name = curr_fname = new_fname;
					files[depth].lnum = 0;
					files[depth].matched = FALSE;
				}
			}
		}
		else
		{
			/*
			 * Check if the line is a define (type == FIND_DEFINE)
			 */
			p = line;
			define_matched = FALSE;
			if (define_prog != NULL && vim_regexec(define_prog, line, TRUE))
			{
				/*
				 * Pattern must be first identifier after 'define', so skip
				 * to that position before checking for match of pattern.  Also
				 * don't let it match beyond the end of this identifier.
				 */
				p = define_prog->endp[0] + 1;
				while (*p && !isidchar(*p))
					p++;
				p2 = p;
				while (*p2 && isidchar(*p2))
					p2++;
				save_char = *p2;
				*p2 = NUL;
				define_matched = TRUE;
			}

			/*
			 * Look for a match.  Don't do this if we are looking for a
			 * define and this line didn't match define_prog above.
			 */
			if ((define_prog == NULL || define_matched) &&
							  prog != NULL && vim_regexec(prog, p, p == line))
			{
				matched = TRUE;
				/*
				 * Check if the line is not a comment line (unless we are
				 * looking for a define).  A line starting with "# define" is
				 * not considered to be a comment line.
				 */
				if (!define_matched && skip_comments)
				{
					fo_do_comments = TRUE;
					if ((*line != '#' ||
							STRNCMP(skipwhite(line + 1), "define", 6) != 0) &&
												   get_leader_len(line, NULL))
						matched = FALSE;

					/*
					 * Also check for a "/ *" or "/ /" before the match.
					 * Skips lines like "int idx;  / * normal index * /" when
					 * looking for "normal".
					 */
					else
						for (p = line; *p && p < prog->startp[0]; ++p)
							if (p[0] == '/' && (p[1] == '*' || p[1] == '/'))
							{
								matched = FALSE;
								break;
							}
					fo_do_comments = FALSE;
				}
			}
			if (define_matched)
				*p2 = save_char;
		}
		if (matched)
		{
#ifdef INSERT_EXPAND
			if (action == ACTION_EXPAND)
			{
				if (depth == -1 && lnum == curwin->w_cursor.lnum)
					break;
				found = TRUE;
				p = prog->startp[0];
				while (iswordchar(*p))
					++p;
				if (add_completion_and_infercase(prog->startp[0],
												   (int)(p - prog->startp[0]),
						curr_fname == curbuf->b_xfilename ? NULL : curr_fname,
														FORWARD) == RET_ERROR)
					break;
			}
			else
#endif
				 if (action == ACTION_SHOW_ALL)
			{
				found = TRUE;
				if (!did_show)
					gotocmdline(TRUE);			/* cursor at status line */
				if (curr_fname != prev_fname)
				{
					if (did_show)
						msg_outchar('\n');		/* cursor below last one */
					if (!got_int)				/* don't display if 'q' typed
													at "--more--" mesage */
					{
						set_highlight('d');		/* Same as for directories */
						start_highlight();
						msg_home_replace(curr_fname);
						stop_highlight();
					}
					prev_fname = curr_fname;
				}
				did_show = TRUE;
				if (!got_int)
					show_pat_in_path(line, type, TRUE, action,
							(depth == -1) ? NULL : files[depth].fp,
							(depth == -1) ? &lnum : &files[depth].lnum,
							match_count++);

				/* Set matched flag for this file and all the ones that
				 * include it */
				for (i = 0; i <= depth; ++i)
					files[i].matched = TRUE;
			}
			else if (--count <= 0)
			{
				found = TRUE;
				if (depth == -1 && lnum == curwin->w_cursor.lnum)
					EMSG("Match is on current line");
				else if (action == ACTION_SHOW)
				{
					show_pat_in_path(line, type, did_show, action,
						(depth == -1) ? NULL : files[depth].fp,
						(depth == -1) ? &lnum : &files[depth].lnum, 1L);
					did_show = TRUE;
				}
				else
				{
					if (action == ACTION_SPLIT)
					{
						if (win_split(0, FALSE) == FAIL)
							break;
					}
					if (depth == -1)
					{
						setpcmark();
						curwin->w_cursor.lnum = lnum;
					}
					else
						if (getfile(0, files[depth].name, NULL, TRUE,
														files[depth].lnum) > 0)
							break;		/* failed to jump to file */
				}
				if (action != ACTION_SHOW)
				{
					curwin->w_cursor.col = prog->startp[0] - line;
					curwin->w_set_curswant = TRUE;
				}
				break;
			}
			matched = FALSE;
		}
		line_breakcheck();
		if (got_int)
			break;
		while (depth >= 0)
		{
			if (!vim_fgets(line = file_line, LSIZE, files[depth].fp))
			{
				++files[depth].lnum;
				break;
			}
			fclose(files[depth].fp);
			--old_files;
			files[old_files].name = files[depth].name;
			files[old_files].matched = files[depth].matched;
			--depth;
			curr_fname = (depth == -1) ? curbuf->b_xfilename
									   : files[depth].name;
			if (depth < depth_displayed)
				depth_displayed = depth;
		}
		if (depth < 0)
		{
			if (++lnum > end_lnum)
				break;
			line = ml_get(lnum);
		}
	}
	for (i = 0; i <= depth; i++)
	{
		fclose(files[i].fp);
		vim_free(files[i].name);
	}
	for (i = old_files; i < max_path_depth; i++)
		vim_free(files[i].name);
	vim_free(files);

	if (type == CHECK_PATH)
	{
		if (!did_show)
		{
			if (action != ACTION_SHOW_ALL)
				MSG("All included files were found");
			else
				MSG("No included files");
		}
	}
	else if (!found
#ifdef INSERT_EXPAND
					&& action != ACTION_EXPAND
#endif
												)
	{
		if (got_int)
			emsg(e_interr);
		else if (type == FIND_DEFINE)
			EMSG("Couldn't find definition");
		else
			EMSG("Couldn't find pattern");
	}
	if (action == ACTION_SHOW || action == ACTION_SHOW_ALL)
		msg_end();

fpip_end:
	vim_free(file_line);
	vim_free(prog);
	vim_free(include_prog);
	vim_free(define_prog);
}

	static void
show_pat_in_path(line, type, did_show, action, fp, lnum, count)
	char_u	*line;
	int		type;
	int		did_show;
	int		action;
	FILE	*fp;
	linenr_t *lnum;
	long	count;
{
	char_u	*p;

	if (did_show)
		msg_outchar('\n');		/* cursor below last one */
	else
		gotocmdline(TRUE);		/* cursor at status line */
	if (got_int)				/* 'q' typed at "--more--" message */
		return;
	for (;;)
	{
		p = line + STRLEN(line) - 1;
		if (fp != NULL)
		{
			/* We used fgets(), so get rid of newline at end */
			if (p >= line && *p == '\n')
				--p;
			if (p >= line && *p == '\r')
				--p;
			*(p + 1) = NUL;
		}
		if (action == ACTION_SHOW_ALL)
		{
			sprintf((char *)IObuff, "%3ld: ", count);	/* show match nr */
			msg_outstr(IObuff);
			set_highlight('n');					/* Highlight line numbers */
			start_highlight();
			sprintf((char *)IObuff, "%4ld", *lnum);		/* show line nr */
			msg_outstr(IObuff);
			stop_highlight();
			MSG_OUTSTR(" ");
		}
		msg_prt_line(line);
		flushbuf();						/* show one line at a time */

		/* Definition continues until line that doesn't end with '\' */
		if (got_int || type != FIND_DEFINE || p < line || *p != '\\')
			break;
		
		if (fp != NULL)
		{
			if (vim_fgets(line, LSIZE, fp))	/* end of file */
				break;
			++*lnum;
		}
		else
		{
			if (++*lnum > curbuf->b_ml.ml_line_count)
				break;
			line = ml_get(*lnum);
		}
		msg_outchar('\n');
	}
}

#ifdef VIMINFO
	int
read_viminfo_search_pattern(line, fp, force)
	char_u	*line;
	FILE	*fp;
	int		force;
{
	char_u	*lp;
	char_u	**pattern;

	lp = line;
	if (lp[0] == '~')
		lp++;
	if (lp[0] == '/')
		pattern = &search_pattern;
	else
		pattern = &subst_pattern;
	if (*pattern != NULL && force)
		vim_free(*pattern);
	if (force || *pattern == NULL)
	{
		viminfo_readstring(lp);
		*pattern = strsave(lp + 1);
		if (line[0] == '~')
			last_pattern = *pattern;
	}
	return vim_fgets(line, LSIZE, fp);
}

	void
write_viminfo_search_pattern(fp)
	FILE	*fp;
{
	if (get_viminfo_parameter('/') != 0)
	{
		if (search_pattern != NULL)
		{
			fprintf(fp, "\n# Last Search Pattern:\n");
			fprintf(fp, "%s/", (last_pattern == search_pattern) ? "~" : "");
			viminfo_writestring(fp, search_pattern);
		}
		if (subst_pattern != NULL)
		{
			fprintf(fp, "\n# Last Substitute Search Pattern:\n");
			fprintf(fp, "%s&", (last_pattern == subst_pattern) ? "~" : "");
			viminfo_writestring(fp, subst_pattern);
		}
	}
}
#endif /* VIMINFO */

/*
 * Give a warning message.
 * Use 'w' highlighting and may repeat the message after redrawing
 */
	static void
give_warning(message)
	char_u	*message;
{
	(void)set_highlight('w');
	msg_highlight = TRUE;
	if (msg(message) && !msg_scroll)
	{
		keep_msg = message;
		keep_msg_highlight = 'w';
	}
	msg_didout = FALSE;		/* overwrite this message */
	msg_col = 0;
}

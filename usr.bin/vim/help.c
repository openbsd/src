/*	$OpenBSD: help.c,v 1.2 1996/09/21 06:23:04 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * help.c: open a read-only window on the vim_help.txt file
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

	void
do_help(arg)
	char_u		*arg;
{
	char_u	*fnamep;
	FILE	*helpfd;			/* file descriptor of help file */
	int		n;
	WIN		*wp;
	int		num_matches;
	char_u	**matches;
	int		need_free = FALSE;

	/*
	 * If an argument is given, check if there is a match for it.
	 */
	if (*arg != NUL)
	{
		n = find_help_tags(arg, &num_matches, &matches);
		if (num_matches == 0 || n == FAIL)
		{
			EMSG2("Sorry, no help for %s", arg);
			return;
		}

		/* The first match is the best match */
		arg = strsave(matches[0]);
		need_free = TRUE;
		FreeWild(num_matches, matches);
	}

	/*
	 * If there is already a help window open, use that one.
	 */
	if (!curwin->w_buffer->b_help)
	{
		for (wp = firstwin; wp != NULL; wp = wp->w_next)
			if (wp->w_buffer != NULL && wp->w_buffer->b_help)
				break;
		if (wp != NULL && wp->w_buffer->b_nwindows > 0)
			win_enter(wp, TRUE);
		else
		{
			/*
			 * There is no help buffer yet.
			 * Try to open the file specified by the "helpfile" option.
			 */
			fnamep = p_hf;
			if ((helpfd = fopen((char *)p_hf, READBIN)) == NULL)
			{
#if defined(MSDOS)
			/*
			 * for MSDOS: try the DOS search path
			 */
				fnamep = searchpath("vim_help.txt");
				if (fnamep == NULL ||
							(helpfd = fopen((char *)fnamep, READBIN)) == NULL)
				{
					smsg((char_u *)"Sorry, help file \"%s\" and \"vim_help.txt\" not found", p_hf);
					goto erret;
				}
#else
				smsg((char_u *)"Sorry, help file \"%s\" not found", p_hf);
				goto erret;
#endif
			}
			fclose(helpfd);

			if (win_split(0, FALSE) == FAIL)
				goto erret;
			
			if (curwin->w_height < p_hh)
				win_setheight((int)p_hh);

#ifdef RIGHTLEFT
			curwin->w_p_rl = 0;				/* help window is left-to-right */
#endif
			curwin->w_p_nu = 0;				/* no line numbers */

			/*
			 * open help file (do_ecmd() will set b_help flag, readfile() will
			 * set b_p_ro flag)
			 */
			(void)do_ecmd(0, fnamep, NULL, NULL, (linenr_t)0,
												   ECMD_HIDE + ECMD_SET_HELP);

			/* save the values of the options we change */
			vim_free(help_save_isk);
			help_save_isk = strsave(curbuf->b_p_isk);
			help_save_ts = curbuf->b_p_ts;

			/* accept all chars for keywords, except ' ', '*', '"', '|' */
			set_string_option((char_u *)"isk", -1,
											 (char_u *)"!-~,^*,^|,^\"", TRUE);
			curbuf->b_p_ts = 8;
			check_buf_options(curbuf);
			(void)init_chartab();		/* needed because 'isk' changed */
		}
	}

	restart_edit = 0;		/* don't want insert mode in help file */

	stuffReadbuff((char_u *)":ta ");
	if (arg != NULL && *arg != NUL)
		stuffReadbuff(arg);
	else
		stuffReadbuff((char_u *)"vim_help.txt");		/* go to the index */
	stuffcharReadbuff('\n');

erret:
	if (need_free)
		vim_free(arg);
}

/*
 * Return a heuristic indicating how well the given string matches.  The
 * smaller the number, the better the match.  This is the order of priorities,
 * from best match to worst match:
 *		- Match with least alpha-numeric characters is better.
 *		- Match with least total characters is better.
 *		- Match towards the start is better.
 * Assumption is made that the matched_string passed has already been found to
 * match some string for which help is requested.  webb.
 */
	int
help_heuristic(matched_string, offset)
	char_u	*matched_string;
	int		offset;				/* offset for match */
{
	int		num_letters;
	char_u	*p;

	num_letters = 0;
	for (p = matched_string; *p; p++)
		if (isalnum(*p))
			num_letters++;

	/*
	 * Multiply the number of letters by 100 to give it a much bigger
	 * weighting than the number of characters.
	 * If the match starts in the middle of a word, add 10000 to put it
	 * somewhere in the last half.
	 * If the match is more than 2 chars from the start, multiply by 200 to
	 * put it after matches at the start.
	 */
	if (isalnum(matched_string[offset]) && offset > 0 &&
										  isalnum(matched_string[offset - 1]))
		offset += 10000;
	else if (offset > 2)
		offset *= 200;
	return (int)(100 * num_letters + STRLEN(matched_string) + offset);
}

static int help_compare __ARGS((const void *s1, const void *s2));

/*
 * Compare functions for qsort() below, that checks the help heuristics number
 * that has been put after the tagname by find_tags().
 */
	static int
help_compare(s1, s2)
	const void	*s1;
	const void	*s2;
{
	char	*p1;
	char	*p2;

	p1 = *(char **)s1 + strlen(*(char **)s1) + 1;
	p2 = *(char **)s2 + strlen(*(char **)s2) + 1;
	return strcmp(p1, p2);
}

/*
 * Find all help tags matching "arg", sort them and return in matches[], with
 * the number of matches in num_matches.
 * We try first with case, and then ignoring case.  Then we try to choose the
 * "best" match from the ones found.
 */
	int
find_help_tags(arg, num_matches, matches)
	char_u		*arg;
	int			*num_matches;
	char_u		***matches;
{
	char_u	*s, *d;
	regexp	*prog;
	int		attempt;
	int		retval = FAIL;
	int		i;
	static char *(mtable[]) = {"*", "g*", "[*", "]*",
							   "/*", "/\\*", "/\\(\\)",
							   "?", ":?", "?<CR>"};
	static char *(rtable[]) = {"star", "gstar", "[star", "]star",
							   "/star", "/\\\\star", "/\\\\(\\\\)",
							   "?", ":?", "?<CR>"};

	reg_magic = p_magic;
	d = IObuff;				/* assume IObuff is long enough! */

	/*
	 * Recognize a few exceptions to the rule.  Some strings that contain '*'
	 * with "star".  Otherwise '*' is recognized as a wildcard.
	 */
	for (i = sizeof(mtable) / sizeof(char *); --i >= 0; )
	{
		if (STRCMP(arg, mtable[i]) == 0)
		{
			STRCPY(d, rtable[i]);
			break;
		}
	}

	if (i < 0)		/* no match in table, replace single characters */
	{
		for (s = arg; *s; ++s)
		{
			/*
			 * Replace "|" with "bar" and """ with "quote" to match the name of
			 * the tags for these commands.
			 * Replace "*" with ".*" and "?" with "." to match command line
			 * completion.
			 * Insert a backslash before '~', '$' and '.' to avoid their
			 * special meaning.
			 */
			if (d - IObuff > IOSIZE - 10)		/* getting too long!? */
				break;
			switch (*s)
			{
				case '|':	STRCPY(d, "bar");
							d += 3;
							continue;
				case '\"':	STRCPY(d, "quote");
							d += 5;
							continue;
				case '*':	*d++ = '.';
							break;
				case '?':	*d++ = '.';
							continue;
				case '$':
				case '.':
				case '~':	*d++ = '\\';
							break;
			}

			/*
			 * Replace "^x" by "CTRL-X". Don't do this for "^_" to make
			 * ":help i_^_CTRL-D" work.
			 */
			if (*s < ' ' || (*s == '^' && s[1] && s[1] != '_'))	/* ^x */
			{
				STRCPY(d, "CTRL-");
				d += 5;
				if (*s < ' ')
				{
					*d++ = *s + '@';
					continue;
				}
				++s;
			}
			else if (*s == '^')			/* "^" or "CTRL-^" or "^_" */
				*d++ = '\\';

			/*
			 * Insert a backslash before a backslash after a slash, for search
			 * pattern tags: "/\|" --> "/\\|".
			 */
			else if (s[0] == '\\' && s[1] != '\\' &&
												  *arg == '/' && s == arg + 1)
				*d++ = '\\';

			*d++ = *s;

			/*
			 * If tag starts with ', toss everything after a second '. Fixes
			 * CTRL-] on 'option'. (would include the trailing '.').
			 */
			if (*s == '\'' && s > arg && *arg == '\'')
				break;
		}
		*d = NUL;
	}

	reg_ic = FALSE;
	prog = vim_regcomp(IObuff);
	if (prog == NULL)
		return FAIL;

	/* First try to match with case, then without */
	for (attempt = 0; attempt < 2; ++attempt, reg_ic = TRUE)
	{
		*matches = (char_u **)"";
		*num_matches = 0;
		retval = find_tags(NULL, prog, num_matches, matches, TRUE, FALSE);
		if (retval == FAIL || *num_matches)
			break;
	}
	vim_free(prog);

#ifdef HAVE_QSORT
	/*
	 * Sort the matches found on the heuristic number that is after the
	 * tag name.  If there is no qsort, the output will be messy!
	 */
	qsort((void *)*matches, (size_t)*num_matches,
											  sizeof(char_u *), help_compare);
#endif
	return OK;
}

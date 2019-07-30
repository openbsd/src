/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines to search a file for a pattern.
 */

#include "charset.h"
#include "less.h"
#include "pattern.h"
#include "position.h"

#define	MINPOS(a, b)	(((a) < (b)) ? (a) : (b))
#define	MAXPOS(a, b)	(((a) > (b)) ? (a) : (b))

extern volatile sig_atomic_t sigs;
extern int how_search;
extern int caseless;
extern int linenums;
extern int sc_height;
extern int jump_sline;
extern int bs_mode;
extern int ctldisp;
extern int status_col;
extern void *const ml_search;
extern off_t start_attnpos;
extern off_t end_attnpos;
extern int utf_mode;
extern int screen_trashed;
extern int hilite_search;
extern int size_linebuf;
extern int squished;
extern int can_goto_line;
static int hide_hilite;
static off_t prep_startpos;
static off_t prep_endpos;
static int is_caseless;
static int is_ucase_pattern;

struct hilite {
	struct hilite *hl_next;
	off_t hl_startpos;
	off_t hl_endpos;
};
static struct hilite hilite_anchor = { NULL, -1, -1 };
static struct hilite filter_anchor = { NULL, -1, -1 };
#define	hl_first	hl_next

/*
 * These are the static variables that represent the "remembered"
 * search pattern and filter pattern.
 */
struct pattern_info {
	regex_t *compiled;
	char *text;
	int search_type;
};

#define	info_compiled(info) ((info)->compiled)

static struct pattern_info search_info;
static struct pattern_info filter_info;

/*
 * Are there any uppercase letters in this string?
 */
static int
is_ucase(char *str)
{
	char *str_end = str + strlen(str);
	LWCHAR ch;

	while (str < str_end) {
		ch = step_char(&str, +1, str_end);
		if (isupper(ch))
			return (1);
	}
	return (0);
}

/*
 * Compile and save a search pattern.
 */
static int
set_pattern(struct pattern_info *info, char *pattern, int search_type)
{
	if (pattern == NULL)
		info->compiled = NULL;
	else if (compile_pattern(pattern, search_type, &info->compiled) < 0)
		return (-1);
	/* Pattern compiled successfully; save the text too. */
	free(info->text);
	info->text = NULL;
	if (pattern != NULL)
		info->text = estrdup(pattern);
	info->search_type = search_type;

	/*
	 * Ignore case if -I is set OR
	 * -i is set AND the pattern is all lowercase.
	 */
	is_ucase_pattern = is_ucase(pattern);
	if (is_ucase_pattern && caseless != OPT_ONPLUS)
		is_caseless = 0;
	else
		is_caseless = caseless;
	return (0);
}

/*
 * Discard a saved pattern.
 */
static void
clear_pattern(struct pattern_info *info)
{
	free(info->text);
	info->text = NULL;
	uncompile_pattern(&info->compiled);
}

/*
 * Initialize saved pattern to nothing.
 */
static void
init_pattern(struct pattern_info *info)
{
	info->compiled = NULL;
	info->text = NULL;
	info->search_type = 0;
}

/*
 * Initialize search variables.
 */
void
init_search(void)
{
	init_pattern(&search_info);
	init_pattern(&filter_info);
}

/*
 * Determine which text conversions to perform before pattern matching.
 */
static int
get_cvt_ops(void)
{
	int ops = 0;
	if (is_caseless || bs_mode == BS_SPECIAL) {
		if (is_caseless)
			ops |= CVT_TO_LC;
		if (bs_mode == BS_SPECIAL)
			ops |= CVT_BS;
		if (bs_mode != BS_CONTROL)
			ops |= CVT_CRLF;
	} else if (bs_mode != BS_CONTROL) {
		ops |= CVT_CRLF;
	}
	if (ctldisp == OPT_ONPLUS)
		ops |= CVT_ANSI;
	return (ops);
}

/*
 * Is there a previous (remembered) search pattern?
 */
static int
prev_pattern(struct pattern_info *info)
{
	if ((info->search_type & SRCH_NO_REGEX) == 0)
		return (info->compiled != NULL);
	return (info->text != NULL);
}

/*
 * Repaint the hilites currently displayed on the screen.
 * Repaint each line which contains highlighted text.
 * If on==0, force all hilites off.
 */
void
repaint_hilite(int on)
{
	int slinenum;
	off_t pos;
	int save_hide_hilite;

	if (squished)
		repaint();

	save_hide_hilite = hide_hilite;
	if (!on) {
		if (hide_hilite)
			return;
		hide_hilite = 1;
	}

	if (!can_goto_line) {
		repaint();
		hide_hilite = save_hide_hilite;
		return;
	}

	for (slinenum = TOP; slinenum < TOP + sc_height-1; slinenum++) {
		pos = position(slinenum);
		if (pos == -1)
			continue;
		(void) forw_line(pos);
		goto_line(slinenum);
		put_line();
	}
	lower_left();
	hide_hilite = save_hide_hilite;
}

/*
 * Clear the attn hilite.
 */
void
clear_attn(void)
{
	int slinenum;
	off_t old_start_attnpos;
	off_t old_end_attnpos;
	off_t pos;
	off_t epos;
	int moved = 0;

	if (start_attnpos == -1)
		return;
	old_start_attnpos = start_attnpos;
	old_end_attnpos = end_attnpos;
	start_attnpos = end_attnpos = -1;

	if (!can_goto_line) {
		repaint();
		return;
	}
	if (squished)
		repaint();

	for (slinenum = TOP; slinenum < TOP + sc_height-1; slinenum++) {
		pos = position(slinenum);
		if (pos == -1)
			continue;
		epos = position(slinenum+1);
		if (pos < old_end_attnpos &&
		    (epos == -1 || epos > old_start_attnpos)) {
			(void) forw_line(pos);
			goto_line(slinenum);
			put_line();
			moved = 1;
		}
	}
	if (moved)
		lower_left();
}

/*
 * Hide search string highlighting.
 */
void
undo_search(void)
{
	if (!prev_pattern(&search_info)) {
		error("No previous regular expression", NULL);
		return;
	}
	hide_hilite = !hide_hilite;
	repaint_hilite(1);
}

/*
 * Clear the hilite list.
 */
static void
clr_hlist(struct hilite *anchor)
{
	struct hilite *hl;
	struct hilite *nexthl;

	for (hl = anchor->hl_first; hl != NULL; hl = nexthl) {
		nexthl = hl->hl_next;
		free(hl);
	}
	anchor->hl_first = NULL;
	prep_startpos = prep_endpos = -1;
}

void
clr_hilite(void)
{
	clr_hlist(&hilite_anchor);
}

static void
clr_filter(void)
{
	clr_hlist(&filter_anchor);
}

/*
 * Should any characters in a specified range be highlighted?
 */
	static int
is_hilited_range(off_t pos, off_t epos)
{
	struct hilite *hl;

	/*
	 * Look at each highlight and see if any part of it falls in the range.
	 */
	for (hl = hilite_anchor.hl_first; hl != NULL; hl = hl->hl_next) {
		if (hl->hl_endpos > pos &&
		    (epos == -1 || epos > hl->hl_startpos))
			return (1);
	}
	return (0);
}

/*
 * Is a line "filtered" -- that is, should it be hidden?
 */
int
is_filtered(off_t pos)
{
	struct hilite *hl;

	if (ch_getflags() & CH_HELPFILE)
		return (0);

	/*
	 * Look at each filter and see if the start position
	 * equals the start position of the line.
	 */
	for (hl = filter_anchor.hl_first; hl != NULL; hl = hl->hl_next) {
		if (hl->hl_startpos == pos)
			return (1);
	}
	return (0);
}

/*
 * Should any characters in a specified range be highlighted?
 * If nohide is nonzero, don't consider hide_hilite.
 */
int
is_hilited(off_t pos, off_t epos, int nohide, int *p_matches)
{
	int match;

	if (p_matches != NULL)
		*p_matches = 0;

	if (!status_col &&
	    start_attnpos != -1 &&
	    pos < end_attnpos &&
	    (epos == -1 || epos > start_attnpos))
		/*
		 * The attn line overlaps this range.
		 */
		return (1);

	match = is_hilited_range(pos, epos);
	if (!match)
		return (0);

	if (p_matches != NULL)
		/*
		 * Report matches, even if we're hiding highlights.
		 */
		*p_matches = 1;

	if (hilite_search == 0)
		/*
		 * Not doing highlighting.
		 */
		return (0);

	if (!nohide && hide_hilite)
		/*
		 * Highlighting is hidden.
		 */
		return (0);

	return (1);
}

/*
 * Add a new hilite to a hilite list.
 */
static void
add_hilite(struct hilite *anchor, struct hilite *hl)
{
	struct hilite *ihl;

	/*
	 * Hilites are sorted in the list; find where new one belongs.
	 * Insert new one after ihl.
	 */
	for (ihl = anchor; ihl->hl_next != NULL; ihl = ihl->hl_next)
	{
		if (ihl->hl_next->hl_startpos > hl->hl_startpos)
			break;
	}

	/*
	 * Truncate hilite so it doesn't overlap any existing ones
	 * above and below it.
	 */
	if (ihl != anchor)
		hl->hl_startpos = MAXPOS(hl->hl_startpos, ihl->hl_endpos);
	if (ihl->hl_next != NULL)
		hl->hl_endpos = MINPOS(hl->hl_endpos,
		    ihl->hl_next->hl_startpos);
	if (hl->hl_startpos >= hl->hl_endpos) {
		/*
		 * Hilite was truncated out of existence.
		 */
		free(hl);
		return;
	}
	hl->hl_next = ihl->hl_next;
	ihl->hl_next = hl;
}

/*
 * Hilight every character in a range of displayed characters.
 */
static void
create_hilites(off_t linepos, int start_index, int end_index, int *chpos)
{
	struct hilite *hl;
	int i;

	/* Start the first hilite. */
	hl = ecalloc(1, sizeof (struct hilite));
	hl->hl_startpos = linepos + chpos[start_index];

	/*
	 * Step through the displayed chars.
	 * If the source position (before cvt) of the char is one more
	 * than the source pos of the previous char (the usual case),
	 * just increase the size of the current hilite by one.
	 * Otherwise (there are backspaces or something involved),
	 * finish the current hilite and start a new one.
	 */
	for (i = start_index+1; i <= end_index; i++) {
		if (chpos[i] != chpos[i-1] + 1 || i == end_index) {
			hl->hl_endpos = linepos + chpos[i-1] + 1;
			add_hilite(&hilite_anchor, hl);
			/* Start new hilite unless this is the last char. */
			if (i < end_index) {
				hl = ecalloc(1, sizeof (struct hilite));
				hl->hl_startpos = linepos + chpos[i];
			}
		}
	}
}

/*
 * Make a hilite for each string in a physical line which matches
 * the current pattern.
 * sp,ep delimit the first match already found.
 */
static void
hilite_line(off_t linepos, char *line, int line_len, int *chpos,
    char *sp, char *ep)
{
	char *searchp;
	char *line_end = line + line_len;

	/*
	 * sp and ep delimit the first match in the line.
	 * Mark the corresponding file positions, then
	 * look for further matches and mark them.
	 * {{ This technique, of calling match_pattern on subsequent
	 *    substrings of the line, may mark more than is correct
	 *    if the pattern starts with "^".  This bug is fixed
	 *    for those regex functions that accept a notbol parameter
	 *    (currently POSIX, PCRE and V8-with-regexec2). }}
	 */
	searchp = line;
	do {
		if (sp == NULL || ep == NULL)
			return;

		create_hilites(linepos, (intptr_t)sp - (intptr_t)line,
		    (intptr_t)ep - (intptr_t)line, chpos);
		/*
		 * If we matched more than zero characters,
		 * move to the first char after the string we matched.
		 * If we matched zero, just move to the next char.
		 */
		if (ep > searchp)
			searchp = ep;
		else if (searchp != line_end)
			searchp++;
		else /* end of line */
			break;
	} while (match_pattern(info_compiled(&search_info), search_info.text,
	    searchp, (intptr_t)line_end - (intptr_t)searchp, &sp, &ep, 1,
	    search_info.search_type));
}

/*
 * Change the caseless-ness of searches.
 * Updates the internal search state to reflect a change in the -i flag.
 */
void
chg_caseless(void)
{
	if (!is_ucase_pattern)
		/*
		 * Pattern did not have uppercase.
		 * Just set the search caselessness to the global caselessness.
		 */
		is_caseless = caseless;
	else
		/*
		 * Pattern did have uppercase.
		 * Discard the pattern; we can't change search caselessness now.
		 */
		clear_pattern(&search_info);
}

/*
 * Find matching text which is currently on screen and highlight it.
 */
static void
hilite_screen(void)
{
	struct scrpos scrpos;

	get_scrpos(&scrpos);
	if (scrpos.pos == -1)
		return;
	prep_hilite(scrpos.pos, position(BOTTOM_PLUS_ONE), -1);
	repaint_hilite(1);
}

/*
 * Change highlighting parameters.
 */
void
chg_hilite(void)
{
	/*
	 * Erase any highlights currently on screen.
	 */
	clr_hilite();
	hide_hilite = 0;

	if (hilite_search == OPT_ONPLUS)
		/*
		 * Display highlights.
		 */
		hilite_screen();
}

/*
 * Figure out where to start a search.
 */
static off_t
search_pos(int search_type)
{
	off_t pos;
	int linenum;

	if (empty_screen()) {
		/*
		 * Start at the beginning (or end) of the file.
		 * The empty_screen() case is mainly for
		 * command line initiated searches;
		 * for example, "+/xyz" on the command line.
		 * Also for multi-file (SRCH_PAST_EOF) searches.
		 */
		if (search_type & SRCH_FORW) {
			pos = ch_zero();
		} else {
			pos = ch_length();
			if (pos == -1) {
				(void) ch_end_seek();
				pos = ch_length();
			}
		}
		linenum = 0;
	} else {
		int add_one = 0;

		if (how_search == OPT_ON) {
			/*
			 * Search does not include current screen.
			 */
			if (search_type & SRCH_FORW)
				linenum = BOTTOM_PLUS_ONE;
			else
				linenum = TOP;
		} else if (how_search == OPT_ONPLUS &&
		    !(search_type & SRCH_AFTER_TARGET)) {
			/*
			 * Search includes all of displayed screen.
			 */
			if (search_type & SRCH_FORW)
				linenum = TOP;
			else
				linenum = BOTTOM_PLUS_ONE;
		} else {
			/*
			 * Search includes the part of current screen beyond
			 * the jump target.
			 * It starts at the jump target (if searching
			 * backwards), or at the jump target plus one
			 * (if forwards).
			 */
			linenum = jump_sline;
			if (search_type & SRCH_FORW)
				add_one = 1;
		}
		linenum = adjsline(linenum);
		pos = position(linenum);
		if (add_one)
			pos = forw_raw_line(pos, NULL, NULL);
	}

	/*
	 * If the line is empty, look around for a plausible starting place.
	 */
	if (search_type & SRCH_FORW) {
		while (pos == -1) {
			if (++linenum >= sc_height)
				break;
			pos = position(linenum);
		}
	} else {
		while (pos == -1) {
			if (--linenum < 0)
				break;
			pos = position(linenum);
		}
	}
	return (pos);
}

/*
 * Search a subset of the file, specified by start/end position.
 */
static int
search_range(off_t pos, off_t endpos, int search_type, int matches,
    int maxlines, off_t *plinepos, off_t *pendpos)
{
	char *line;
	char *cline;
	int line_len;
	off_t linenum;
	char *sp, *ep;
	int line_match;
	int cvt_ops;
	int cvt_len;
	int *chpos;
	off_t linepos, oldpos;

	linenum = find_linenum(pos);
	oldpos = pos;
	for (;;) {
		/*
		 * Get lines until we find a matching one or until
		 * we hit end-of-file (or beginning-of-file if we're
		 * going backwards), or until we hit the end position.
		 */
		if (ABORT_SIGS()) {
			/*
			 * A signal aborts the search.
			 */
			return (-1);
		}

		if ((endpos != -1 && pos >= endpos) ||
		    maxlines == 0) {
			/*
			 * Reached end position without a match.
			 */
			if (pendpos != NULL)
				*pendpos = pos;
			return (matches);
		}
		if (maxlines > 0)
			maxlines--;

		if (search_type & SRCH_FORW) {
			/*
			 * Read the next line, and save the
			 * starting position of that line in linepos.
			 */
			linepos = pos;
			pos = forw_raw_line(pos, &line, &line_len);
			if (linenum != 0)
				linenum++;
		} else {
			/*
			 * Read the previous line and save the
			 * starting position of that line in linepos.
			 */
			pos = back_raw_line(pos, &line, &line_len);
			linepos = pos;
			if (linenum != 0)
				linenum--;
		}

		if (pos == -1) {
			/*
			 * Reached EOF/BOF without a match.
			 */
			if (pendpos != NULL)
				*pendpos = oldpos;
			return (matches);
		}

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 * Don't do it for every line because it slows down
		 * the search.  Remember the line number only if
		 * we're "far" from the last place we remembered it.
		 */
		if (linenums && abs((int)(pos - oldpos)) > 2048)
			add_lnum(linenum, pos);
		oldpos = pos;

		if (is_filtered(linepos))
			continue;

		/*
		 * If it's a caseless search, convert the line to lowercase.
		 * If we're doing backspace processing, delete backspaces.
		 */
		cvt_ops = get_cvt_ops();
		cvt_len = cvt_length(line_len);
		cline = ecalloc(1, cvt_len);
		chpos = cvt_alloc_chpos(cvt_len);
		cvt_text(cline, line, chpos, &line_len, cvt_ops);

		/*
		 * Check to see if the line matches the filter pattern.
		 * If so, add an entry to the filter list.
		 */
		if ((search_type & SRCH_FIND_ALL) &&
		    prev_pattern(&filter_info)) {
			int line_filter =
			    match_pattern(info_compiled(&filter_info),
			    filter_info.text, cline, line_len, &sp, &ep, 0,
			    filter_info.search_type);
			if (line_filter) {
				struct hilite *hl =
				    ecalloc(1, sizeof (struct hilite));
				hl->hl_startpos = linepos;
				hl->hl_endpos = pos;
				add_hilite(&filter_anchor, hl);
			}
		}

		/*
		 * Test the next line to see if we have a match.
		 * We are successful if we either want a match and got one,
		 * or if we want a non-match and got one.
		 */
		if (prev_pattern(&search_info)) {
			line_match = match_pattern(info_compiled(&search_info),
			    search_info.text, cline, line_len, &sp, &ep, 0,
			    search_type);
			if (line_match) {
				/*
				 * Got a match.
				 */
				if (search_type & SRCH_FIND_ALL) {
					/*
					 * We are supposed to find all matches
					 * in the range.
					 * Just add the matches in this line
					 * to the hilite list and keep
					 * searching.
					 */
					hilite_line(linepos, cline, line_len,
					    chpos, sp, ep);
				} else if (--matches <= 0) {
					/*
					 * Found the one match we're looking
					 * for.  Return it.
					 */
					if (hilite_search == OPT_ON) {
						/*
						 * Clear the hilite list and
						 * add only
						 * the matches in this one line.
						 */
						clr_hilite();
						hilite_line(linepos, cline,
						    line_len, chpos, sp, ep);
					}
					free(cline);
					free(chpos);
					if (plinepos != NULL)
						*plinepos = linepos;
					return (0);
				}
			}
		}
		free(cline);
		free(chpos);
	}
}

/*
 * search for a pattern in history. If found, compile that pattern.
 */
static int
hist_pattern(int search_type)
{
	char *pattern;

	set_mlist(ml_search, 0);
	pattern = cmd_lastpattern();
	if (pattern == NULL)
		return (0);

	if (set_pattern(&search_info, pattern, search_type) < 0)
		return (0);

	if (hilite_search == OPT_ONPLUS && !hide_hilite)
		hilite_screen();

	return (1);
}

/*
 * Search for the n-th occurrence of a specified pattern,
 * either forward or backward.
 * Return the number of matches not yet found in this file
 * (that is, n minus the number of matches found).
 * Return -1 if the search should be aborted.
 * Caller may continue the search in another file
 * if less than n matches are found in this file.
 */
int
search(int search_type, char *pattern, int n)
{
	off_t pos;

	if (pattern == NULL || *pattern == '\0') {
		/*
		 * A null pattern means use the previously compiled pattern.
		 */
		search_type |= SRCH_AFTER_TARGET;
		if (!prev_pattern(&search_info) && !hist_pattern(search_type)) {
			error("No previous regular expression", NULL);
			return (-1);
		}
		if ((search_type & SRCH_NO_REGEX) !=
		    (search_info.search_type & SRCH_NO_REGEX)) {
			error("Please re-enter search pattern", NULL);
			return (-1);
		}
		if (hilite_search == OPT_ON) {
			/*
			 * Erase the highlights currently on screen.
			 * If the search fails, we'll redisplay them later.
			 */
			repaint_hilite(0);
		}
		if (hilite_search == OPT_ONPLUS && hide_hilite) {
			/*
			 * Highlight any matches currently on screen,
			 * before we actually start the search.
			 */
			hide_hilite = 0;
			hilite_screen();
		}
		hide_hilite = 0;
	} else {
		/*
		 * Compile the pattern.
		 */
		if (set_pattern(&search_info, pattern, search_type) < 0)
			return (-1);
		if (hilite_search) {
			/*
			 * Erase the highlights currently on screen.
			 * Also permanently delete them from the hilite list.
			 */
			repaint_hilite(0);
			hide_hilite = 0;
			clr_hilite();
		}
		if (hilite_search == OPT_ONPLUS) {
			/*
			 * Highlight any matches currently on screen,
			 * before we actually start the search.
			 */
			hilite_screen();
		}
	}

	/*
	 * Figure out where to start the search.
	 */
	pos = search_pos(search_type);
	if (pos == -1) {
		/*
		 * Can't find anyplace to start searching from.
		 */
		if (search_type & SRCH_PAST_EOF)
			return (n);
		/* repaint(); -- why was this here? */
		error("Nothing to search", NULL);
		return (-1);
	}

	n = search_range(pos, -1, search_type, n, -1, &pos, NULL);
	if (n != 0) {
		/*
		 * Search was unsuccessful.
		 */
		if (hilite_search == OPT_ON && n > 0)
			/*
			 * Redisplay old hilites.
			 */
			repaint_hilite(1);
		return (n);
	}

	if (!(search_type & SRCH_NO_MOVE)) {
		/*
		 * Go to the matching line.
		 */
		jump_loc(pos, jump_sline);
	}

	if (hilite_search == OPT_ON)
		/*
		 * Display new hilites in the matching line.
		 */
		repaint_hilite(1);
	return (0);
}


/*
 * Prepare hilites in a given range of the file.
 *
 * The pair (prep_startpos,prep_endpos) delimits a contiguous region
 * of the file that has been "prepared"; that is, scanned for matches for
 * the current search pattern, and hilites have been created for such matches.
 * If prep_startpos == -1, the prep region is empty.
 * If prep_endpos == -1, the prep region extends to EOF.
 * prep_hilite asks that the range (spos,epos) be covered by the prep region.
 */
void
prep_hilite(off_t spos, off_t epos, int maxlines)
{
	off_t nprep_startpos = prep_startpos;
	off_t nprep_endpos = prep_endpos;
	off_t new_epos;
	off_t max_epos;
	int result;
	int i;

/*
 * Search beyond where we're asked to search, so the prep region covers
 * more than we need.  Do one big search instead of a bunch of small ones.
 */
#define	SEARCH_MORE (3*size_linebuf)

	if (!prev_pattern(&search_info) && !is_filtering())
		return;

	/*
	 * If we're limited to a max number of lines, figure out the
	 * file position we should stop at.
	 */
	if (maxlines < 0) {
		max_epos = -1;
	} else {
		max_epos = spos;
		for (i = 0; i < maxlines; i++)
			max_epos = forw_raw_line(max_epos, NULL, NULL);
	}

	/*
	 * Find two ranges:
	 * The range that we need to search (spos,epos); and the range that
	 * the "prep" region will then cover (nprep_startpos,nprep_endpos).
	 */

	if (prep_startpos == -1 ||
	    (epos != -1 && epos < prep_startpos) ||
	    spos > prep_endpos) {
		/*
		 * New range is not contiguous with old prep region.
		 * Discard the old prep region and start a new one.
		 */
		clr_hilite();
		clr_filter();
		if (epos != -1)
			epos += SEARCH_MORE;
		nprep_startpos = spos;
	} else {
		/*
		 * New range partially or completely overlaps old prep region.
		 */
		if (epos != -1) {
			if (epos > prep_endpos) {
				/*
				 * New range ends after old prep region.
				 * Extend prep region to end at end of new
				 * range.
				 */
				epos += SEARCH_MORE;

			} else {
				/*
				 * New range ends within old prep region.
				 * Truncate search to end at start of old prep
				 * region.
				 */
				epos = prep_startpos;
			}
		}

		if (spos < prep_startpos) {
			/*
			 * New range starts before old prep region.
			 * Extend old prep region backwards to start at
			 * start of new range.
			 */
			if (spos < SEARCH_MORE)
				spos = 0;
			else
				spos -= SEARCH_MORE;
			nprep_startpos = spos;
		} else { /* (spos >= prep_startpos) */
			/*
			 * New range starts within or after old prep region.
			 * Trim search to start at end of old prep region.
			 */
			spos = prep_endpos;
		}
	}

	if (epos != -1 && max_epos != -1 &&
	    epos > max_epos)
		/*
		 * Don't go past the max position we're allowed.
		 */
		epos = max_epos;

	if (epos == -1 || epos > spos) {
		int search_type = SRCH_FORW | SRCH_FIND_ALL;
		search_type |= (search_info.search_type & SRCH_NO_REGEX);
		result = search_range(spos, epos, search_type, 0,
		    maxlines, NULL, &new_epos);
		if (result < 0)
			return;
		if (prep_endpos == -1 || new_epos > prep_endpos)
			nprep_endpos = new_epos;
	}
	prep_startpos = nprep_startpos;
	prep_endpos = nprep_endpos;
}

/*
 * Set the pattern to be used for line filtering.
 */
void
set_filter_pattern(char *pattern, int search_type)
{
	clr_filter();
	if (pattern == NULL || *pattern == '\0')
		clear_pattern(&filter_info);
	else
		(void) set_pattern(&filter_info, pattern, search_type);
	screen_trashed = 1;
}

/*
 * Is there a line filter in effect?
 */
int
is_filtering(void)
{
	if (ch_getflags() & CH_HELPFILE)
		return (0);
	return (prev_pattern(&filter_info));
}

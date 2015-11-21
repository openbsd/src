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
 * Routines to do pattern matching.
 */

#include "less.h"
#include "pattern.h"

extern int caseless;
extern int less_is_more;

/*
 * Compile a search pattern, for future use by match_pattern.
 */
static int
compile_pattern2(char *pattern, int search_type, regex_t **comp_pattern)
{
	regex_t *comp;

	if (search_type & SRCH_NO_REGEX)
		return (0);
	comp = ecalloc(1, sizeof (regex_t));
	if (regcomp(comp, pattern, less_is_more ? 0 : REGCOMP_FLAG)) {
		free(comp);
		error("Invalid pattern", NULL);
		return (-1);
	}
	if (*comp_pattern != NULL)
		regfree(*comp_pattern);
	*comp_pattern = comp;
	return (0);
}

/*
 * Like compile_pattern2, but convert the pattern to lowercase if necessary.
 */
int
compile_pattern(char *pattern, int search_type, regex_t **comp_pattern)
{
	char *cvt_pattern;
	int result;

	if (caseless != OPT_ONPLUS) {
		cvt_pattern = pattern;
	} else {
		cvt_pattern = ecalloc(1, cvt_length(strlen(pattern)));
		cvt_text(cvt_pattern, pattern, NULL, NULL, CVT_TO_LC);
	}
	result = compile_pattern2(cvt_pattern, search_type, comp_pattern);
	if (cvt_pattern != pattern)
		free(cvt_pattern);
	return (result);
}

/*
 * Forget that we have a compiled pattern.
 */
void
uncompile_pattern(regex_t **pattern)
{
	if (*pattern != NULL)
		regfree(*pattern);
	*pattern = NULL;
}

/*
 * Simple pattern matching function.
 * It supports no metacharacters like *, etc.
 */
static int
match(char *pattern, int pattern_len, char *buf, int buf_len,
    char **pfound, char **pend)
{
	char *pp, *lp;
	char *pattern_end = pattern + pattern_len;
	char *buf_end = buf + buf_len;

	for (; buf < buf_end; buf++) {
		for (pp = pattern, lp = buf; *pp == *lp; pp++, lp++)
			if (pp == pattern_end || lp == buf_end)
				break;
		if (pp == pattern_end) {
			if (pfound != NULL)
				*pfound = buf;
			if (pend != NULL)
				*pend = lp;
			return (1);
		}
	}
	return (0);
}

/*
 * Perform a pattern match with the previously compiled pattern.
 * Set sp and ep to the start and end of the matched string.
 */
int
match_pattern(void *pattern, char *tpattern, char *line, int line_len,
    char **sp, char **ep, int notbol, int search_type)
{
	int matched;
	regex_t *spattern = (regex_t *)pattern;

	if (search_type & SRCH_NO_REGEX) {
		matched = match(tpattern, strlen(tpattern), line, line_len,
		    sp, ep);
	} else {
		regmatch_t rm;
		int flags = (notbol) ? REG_NOTBOL : 0;
#ifdef	REG_STARTEND
		flags |= REG_STARTEND;
		rm.rm_so = 0;
		rm.rm_eo = line_len;
#endif
		matched = !regexec(spattern, line, 1, &rm, flags);
		if (matched) {
			*sp = line + rm.rm_so;
			*ep = line + rm.rm_eo;
		}
	}
	matched = (!(search_type & SRCH_NO_MATCH) && matched) ||
	    ((search_type & SRCH_NO_MATCH) && !matched);
	return (matched);
}

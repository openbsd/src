/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "less.h"

#define	WHITESP(c)	((c)==' ' || (c)=='\t')

#if TAGS

public char *tagfile;
public char *tags = "tags";

static char *tagpattern;
static int taglinenum;

extern int linenums;
extern int sigs;
extern int jump_sline;

/*
 * Find a tag in the "tags" file.
 * Sets "tagfile" to the name of the file containing the tag,
 * and "tagpattern" to the search pattern which should be used
 * to find the tag.
 */
	public void
findtag(tag)
	register char *tag;
{
	char *p;
	char *q;
	register FILE *f;
	register int taglen;
	int search_char;
	int err;
	static char tline[200];

	if ((f = fopen(tags, "r")) == NULL)
	{
		error("No tags file", NULL_PARG);
		tagfile = NULL;
		return;
	}

	taglen = strlen(tag);

	/*
	 * Search the tags file for the desired tag.
	 */
	while (fgets(tline, sizeof(tline), f) != NULL)
	{
		if (strncmp(tag, tline, taglen) != 0 || !WHITESP(tline[taglen]))
			continue;

		/*
		 * Found it.
		 * The line contains the tag, the filename and the
		 * location in the file, separated by white space.
		 * The location is either a decimal line number, 
		 * or a search pattern surrounded by a pair of delimiters.
		 * Parse the line and extract these parts.
		 */
		tagfile = tagpattern = NULL;
		taglinenum = 0;

		/*
		 * Skip over the whitespace after the tag name.
		 */
		p = skipsp(tline+taglen);
		if (*p == '\0')
			/* File name is missing! */
			continue;

		/*
		 * Save the file name.
		 * Skip over the whitespace after the file name.
		 */
		tagfile = p;
		while (!WHITESP(*p) && *p != '\0')
			p++;
		*p++ = '\0';
		p = skipsp(p);
		if (*p == '\0')
			/* Pattern is missing! */
			continue;

		/*
		 * First see if it is a line number. 
		 */
		taglinenum = getnum(&p, 0, &err);
		if (err)
		{
			/*
			 * No, it must be a pattern.
			 * Delete the initial "^" (if present) and 
			 * the final "$" from the pattern.
			 * Delete any backslash in the pattern.
			 */
			taglinenum = 0;
			search_char = *p++;
			if (*p == '^')
				p++;
			tagpattern = q = p;
			while (*p != search_char && *p != '\0')
			{
				if (*p == '\\')
					p++;
				*q++ = *p++;
			}
			if (q[-1] == '$')
				q--;
			*q = '\0';
		}

		fclose(f);
		return;
	}
	fclose(f);
	error("No such tag in tags file", NULL_PARG);
	tagfile = NULL;
}

/*
 * Search for a tag.
 * This is a stripped-down version of search().
 * We don't use search() for several reasons:
 *   -	We don't want to blow away any search string we may have saved.
 *   -	The various regular-expression functions (from different systems:
 *	regcmp vs. re_comp) behave differently in the presence of 
 *	parentheses (which are almost always found in a tag).
 */
	public POSITION
tagsearch()
{
	POSITION pos, linepos;
	int linenum;
	char *line;

	/*
	 * If we have the line number of the tag instead of the pattern,
	 * just use find_pos.
	 */
	if (taglinenum)
		return (find_pos(taglinenum));

	pos = ch_zero();
	linenum = find_linenum(pos);

	for (;;)
	{
		/*
		 * Get lines until we find a matching one or 
		 * until we hit end-of-file.
		 */
		if (ABORT_SIGS())
			return (NULL_POSITION);

		/*
		 * Read the next line, and save the 
		 * starting position of that line in linepos.
		 */
		linepos = pos;
		pos = forw_raw_line(pos, &line);
		if (linenum != 0)
			linenum++;

		if (pos == NULL_POSITION)
		{
			/*
			 * We hit EOF without a match.
			 */
			error("Tag not found", NULL_PARG);
			return (NULL_POSITION);
		}

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 */
		if (linenums)
			add_lnum(linenum, pos);

		/*
		 * Test the line to see if we have a match.
		 * Use strncmp because the pattern may be
		 * truncated (in the tags file) if it is too long.
		 */
		if (strncmp(tagpattern, line, strlen(tagpattern)) == 0)
			break;
	}

	return (linepos);
}

#endif

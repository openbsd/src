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

#include "less.h"

extern IFILE curr_ifile;
extern int sc_height;
extern int jump_sline;

/*
 * A mark is an ifile (input file) plus a position within the file.
 */
struct mark {
	IFILE m_ifile;
	struct scrpos m_scrpos;
};

/*
 * The table of marks.
 * Each mark is identified by a lowercase or uppercase letter.
 * The final one is lmark, for the "last mark"; addressed by the apostrophe.
 */
#define	NMARKS		((2*26)+1)	/* a-z, A-Z, lastmark */
#define	LASTMARK	(NMARKS-1)
static struct mark marks[NMARKS];

/*
 * Initialize the mark table to show no marks are set.
 */
void
init_mark(void)
{
	int i;

	for (i = 0; i < NMARKS; i++)
		marks[i].m_scrpos.pos = -1;
}

/*
 * See if a mark letter is valid (between a and z).
 */
static struct mark *
getumark(int c)
{
	if (c >= 'a' && c <= 'z')
		return (&marks[c-'a']);

	if (c >= 'A' && c <= 'Z')
		return (&marks[c-'A'+26]);

	error("Invalid mark letter", NULL);
	return (NULL);
}

/*
 * Get the mark structure identified by a character.
 * The mark struct may come either from the mark table
 * or may be constructed on the fly for certain characters like ^, $.
 */
static struct mark *
getmark(int c)
{
	struct mark *m;
	static struct mark sm;

	switch (c) {
	case '^':
		/*
		 * Beginning of the current file.
		 */
		m = &sm;
		m->m_scrpos.pos = ch_zero();
		m->m_scrpos.ln = 0;
		m->m_ifile = curr_ifile;
		break;
	case '$':
		/*
		 * End of the current file.
		 */
		if (ch_end_seek()) {
			error("Cannot seek to end of file", NULL);
			return (NULL);
		}
		m = &sm;
		m->m_scrpos.pos = ch_tell();
		m->m_scrpos.ln = sc_height-1;
		m->m_ifile = curr_ifile;
		break;
	case '.':
		/*
		 * Current position in the current file.
		 */
		m = &sm;
		get_scrpos(&m->m_scrpos);
		m->m_ifile = curr_ifile;
		break;
	case '\'':
		/*
		 * The "last mark".
		 */
		m = &marks[LASTMARK];
		break;
	default:
		/*
		 * Must be a user-defined mark.
		 */
		m = getumark(c);
		if (m == NULL)
			break;
		if (m->m_scrpos.pos == -1) {
			error("Mark not set", NULL);
			return (NULL);
		}
		break;
	}
	return (m);
}

/*
 * Is a mark letter is invalid?
 */
int
badmark(int c)
{
	return (getmark(c) == NULL);
}

/*
 * Set a user-defined mark.
 */
void
setmark(int c)
{
	struct mark *m;
	struct scrpos scrpos;

	m = getumark(c);
	if (m == NULL)
		return;
	get_scrpos(&scrpos);
	m->m_scrpos = scrpos;
	m->m_ifile = curr_ifile;
}

/*
 * Set lmark (the mark named by the apostrophe).
 */
void
lastmark(void)
{
	struct scrpos scrpos;

	if (ch_getflags() & CH_HELPFILE)
		return;
	get_scrpos(&scrpos);
	if (scrpos.pos == -1)
		return;
	marks[LASTMARK].m_scrpos = scrpos;
	marks[LASTMARK].m_ifile = curr_ifile;
}

/*
 * Go to a mark.
 */
void
gomark(int c)
{
	struct mark *m;
	struct scrpos scrpos;

	m = getmark(c);
	if (m == NULL)
		return;

	/*
	 * If we're trying to go to the lastmark and
	 * it has not been set to anything yet,
	 * set it to the beginning of the current file.
	 */
	if (m == &marks[LASTMARK] && m->m_scrpos.pos == -1) {
		m->m_ifile = curr_ifile;
		m->m_scrpos.pos = ch_zero();
		m->m_scrpos.ln = jump_sline;
	}

	/*
	 * If we're using lmark, we must save the screen position now,
	 * because if we call edit_ifile() below, lmark will change.
	 * (We save the screen position even if we're not using lmark.)
	 */
	scrpos = m->m_scrpos;
	if (m->m_ifile != curr_ifile) {
		/*
		 * Not in the current file; edit the correct file.
		 */
		if (edit_ifile(m->m_ifile))
			return;
	}

	jump_loc(scrpos.pos, scrpos.ln);
}

/*
 * Return the position associated with a given mark letter.
 *
 * We don't return which screen line the position
 * is associated with, but this doesn't matter much,
 * because it's always the first non-blank line on the screen.
 */
off_t
markpos(int c)
{
	struct mark *m;

	m = getmark(c);
	if (m == NULL)
		return (-1);

	if (m->m_ifile != curr_ifile) {
		error("Mark not in current file", NULL);
		return (-1);
	}
	return (m->m_scrpos.pos);
}

/*
 * Clear the marks associated with a specified ifile.
 */
void
unmark(IFILE ifile)
{
	int i;

	for (i = 0; i < NMARKS; i++)
		if (marks[i].m_ifile == ifile)
			marks[i].m_scrpos.pos = -1;
}

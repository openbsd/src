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
 * An IFILE represents an input file.
 *
 * It is actually a pointer to an ifile structure,
 * but is opaque outside this module.
 * Ifile structures are kept in a linked list in the order they
 * appear on the command line.
 * Any new file which does not already appear in the list is
 * inserted after the current file.
 */

#include "less.h"

extern IFILE	curr_ifile;

struct ifile {
	struct ifile *h_next;		/* Links for command line list */
	struct ifile *h_prev;
	char *h_filename;		/* Name of the file */
	void *h_filestate;		/* File state (used in ch.c) */
	int h_index;			/* Index within command line list */
	int h_hold;			/* Hold count */
	char h_opened;			/* Has this ifile been opened? */
	struct scrpos h_scrpos;		/* Saved position within the file */
};

/*
 * Convert an IFILE (external representation)
 * to a struct file (internal representation), and vice versa.
 */
#define	int_ifile(h)	((struct ifile *)(h))
#define	ext_ifile(h)	((IFILE)(h))

/*
 * Anchor for linked list.
 */
static struct ifile anchor = { &anchor, &anchor, NULL, NULL, 0, 0, '\0',
				{ -1, 0 } };
static int ifiles = 0;

static void
incr_index(struct ifile *p, int incr)
{
	for (; p != &anchor; p = p->h_next)
		p->h_index += incr;
}

/*
 * Link an ifile into the ifile list.
 */
static void
link_ifile(struct ifile *p, struct ifile *prev)
{
	/*
	 * Link into list.
	 */
	if (prev == NULL)
		prev = &anchor;
	p->h_next = prev->h_next;
	p->h_prev = prev;
	prev->h_next->h_prev = p;
	prev->h_next = p;
	/*
	 * Calculate index for the new one,
	 * and adjust the indexes for subsequent ifiles in the list.
	 */
	p->h_index = prev->h_index + 1;
	incr_index(p->h_next, 1);
	ifiles++;
}

/*
 * Unlink an ifile from the ifile list.
 */
static void
unlink_ifile(struct ifile *p)
{
	p->h_next->h_prev = p->h_prev;
	p->h_prev->h_next = p->h_next;
	incr_index(p->h_next, -1);
	ifiles--;
}

/*
 * Allocate a new ifile structure and stick a filename in it.
 * It should go after "prev" in the list
 * (or at the beginning of the list if "prev" is NULL).
 * Return a pointer to the new ifile structure.
 */
static struct ifile *
new_ifile(char *filename, struct ifile *prev)
{
	struct ifile *p;

	/*
	 * Allocate and initialize structure.
	 */
	p = ecalloc(1, sizeof (struct ifile));
	p->h_filename = estrdup(filename);
	p->h_scrpos.pos = -1;
	p->h_opened = 0;
	p->h_hold = 0;
	p->h_filestate = NULL;
	link_ifile(p, prev);
	return (p);
}

/*
 * Delete an existing ifile structure.
 */
void
del_ifile(IFILE h)
{
	struct ifile *p;

	if (h == NULL)
		return;
	/*
	 * If the ifile we're deleting is the currently open ifile,
	 * move off it.
	 */
	unmark(h);
	if (h == curr_ifile)
		curr_ifile = getoff_ifile(curr_ifile);
	p = int_ifile(h);
	unlink_ifile(p);
	free(p->h_filename);
	free(p);
}

/*
 * Get the ifile after a given one in the list.
 */
IFILE
next_ifile(IFILE h)
{
	struct ifile *p;

	p = (h == NULL) ? &anchor : int_ifile(h);
	if (p->h_next == &anchor)
		return (NULL);
	return (ext_ifile(p->h_next));
}

/*
 * Get the ifile before a given one in the list.
 */
IFILE
prev_ifile(IFILE h)
{
	struct ifile *p;

	p = (h == NULL) ? &anchor : int_ifile(h);
	if (p->h_prev == &anchor)
		return (NULL);
	return (ext_ifile(p->h_prev));
}

/*
 * Return a different ifile from the given one.
 */
IFILE
getoff_ifile(IFILE ifile)
{
	IFILE newifile;

	if ((newifile = prev_ifile(ifile)) != NULL)
		return (newifile);
	if ((newifile = next_ifile(ifile)) != NULL)
		return (newifile);
	return (NULL);
}

/*
 * Return the number of ifiles.
 */
int
nifile(void)
{
	return (ifiles);
}

/*
 * Find an ifile structure, given a filename.
 */
static struct ifile *
find_ifile(const char *filename)
{
	struct ifile *p;

	for (p = anchor.h_next; p != &anchor; p = p->h_next)
		if (strcmp(filename, p->h_filename) == 0)
			return (p);
	return (NULL);
}

/*
 * Get the ifile associated with a filename.
 * If the filename has not been seen before,
 * insert the new ifile after "prev" in the list.
 */
IFILE
get_ifile(char *filename, IFILE prev)
{
	struct ifile *p;

	if ((p = find_ifile(filename)) == NULL)
		p = new_ifile(filename, int_ifile(prev));
	return (ext_ifile(p));
}

/*
 * Get the filename associated with a ifile.
 */
char *
get_filename(IFILE ifile)
{
	if (ifile == NULL)
		return (NULL);
	return (int_ifile(ifile)->h_filename);
}

/*
 * Get the index of the file associated with a ifile.
 */
int
get_index(IFILE ifile)
{
	return (int_ifile(ifile)->h_index);
}

/*
 * Save the file position to be associated with a given file.
 */
void
store_pos(IFILE ifile, struct scrpos *scrpos)
{
	int_ifile(ifile)->h_scrpos = *scrpos;
}

/*
 * Recall the file position associated with a file.
 * If no position has been associated with the file, return -1.
 */
void
get_pos(IFILE ifile, struct scrpos *scrpos)
{
	*scrpos = int_ifile(ifile)->h_scrpos;
}

/*
 * Mark the ifile as "opened".
 */
void
set_open(IFILE ifile)
{
	int_ifile(ifile)->h_opened = 1;
}

/*
 * Return whether the ifile has been opened previously.
 */
int
opened(IFILE ifile)
{
	return (int_ifile(ifile)->h_opened);
}

void
hold_ifile(IFILE ifile, int incr)
{
	int_ifile(ifile)->h_hold += incr;
}

int
held_ifile(IFILE ifile)
{
	return (int_ifile(ifile)->h_hold);
}

void *
get_filestate(IFILE ifile)
{
	return (int_ifile(ifile)->h_filestate);
}

void
set_filestate(IFILE ifile, void *filestate)
{
	int_ifile(ifile)->h_filestate = filestate;
}

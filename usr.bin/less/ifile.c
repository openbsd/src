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
	int h_opened;			/* Only need one bit */
	struct scrpos h_scrpos;		/* Saved position within the file */
};

/*
 * Convert an IFILE (external representation)
 * to a struct file (internal representation), and vice versa.
 */
#define int_ifile(h)	((struct ifile *)(h))
#define ext_ifile(h)	((IFILE)(h))

/*
 * Anchor for linked list.
 */
static struct ifile anchor = { &anchor, &anchor, 0 };
static int ifiles = 0;

	static void
incr_index(p, incr)
	register struct ifile *p;
	int incr;
{
	for (;  p != &anchor;  p = p->h_next)
		p->h_index += incr;
}

	static void
link_ifile(p, prev)
	struct ifile *p;
	struct ifile *prev;
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
	
	static void
unlink_ifile(p)
	struct ifile *p;
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
new_ifile(filename, prev)
	char *filename;
	struct ifile *prev;
{
	register struct ifile *p;

	/*
	 * Allocate and initialize structure.
	 */
	p = (struct ifile *) ecalloc(1, sizeof(struct ifile));
	p->h_filename = save(filename);
	p->h_scrpos.pos = NULL_POSITION;
	p->h_opened = 0;
	link_ifile(p, prev);
	return (p);
}

/*
 * Delete an existing ifile structure.
 */
	public void
del_ifile(h)
	IFILE h;
{
	register struct ifile *p;

	if (h == NULL_IFILE)
		return;
	/*
	 * If the ifile we're deleting is the currently open ifile,
	 * move off it.
	 */
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
	public IFILE
next_ifile(h)
	IFILE h;
{
	register struct ifile *p;

	p = (h == NULL_IFILE) ? &anchor : int_ifile(h);
	if (p->h_next == &anchor)
		return (NULL_IFILE);
	return (ext_ifile(p->h_next));
}

/*
 * Get the ifile before a given one in the list.
 */
	public IFILE
prev_ifile(h)
	IFILE h;
{
	register struct ifile *p;

	p = (h == NULL_IFILE) ? &anchor : int_ifile(h);
	if (p->h_prev == &anchor)
		return (NULL_IFILE);
	return (ext_ifile(p->h_prev));
}

/*
 * Return a different ifile from the given one.
 */
	public IFILE
getoff_ifile(ifile)
	IFILE ifile;
{
	IFILE newifile;
	
	if ((newifile = prev_ifile(ifile)) != NULL_IFILE)
		return (newifile);
	if ((newifile = next_ifile(ifile)) != NULL_IFILE)
		return (newifile);
	return (NULL_IFILE);
}

/*
 * Return the number of ifiles.
 */
	public int
nifile()
{
	return (ifiles);
}

/*
 * Find an ifile structure, given a filename.
 */
	static struct ifile *
find_ifile(filename)
	char *filename;
{
	register struct ifile *p;

	for (p = anchor.h_next;  p != &anchor;  p = p->h_next)
		if (strcmp(filename, p->h_filename) == 0)
			return (p);
	return (NULL);
}

/*
 * Get the ifile associated with a filename.
 * If the filename has not been seen before,
 * insert the new ifile after "prev" in the list.
 */
	public IFILE
get_ifile(filename, prev)
	char *filename;
	IFILE prev;
{
	register struct ifile *p;

	if ((p = find_ifile(filename)) == NULL)
		p = new_ifile(filename, int_ifile(prev));
	return (ext_ifile(p));
}

/*
 * Get the filename associated with a ifile.
 */
	public char *
get_filename(ifile)
	IFILE ifile;
{
	if (ifile == NULL)
		return (NULL);
	return (int_ifile(ifile)->h_filename);
}

/*
 * Get the index of the file associated with a ifile.
 */
	public int
get_index(ifile)
	IFILE ifile;
{
	return (int_ifile(ifile)->h_index); 
}

/*
 * Save the file position to be associated with a given file.
 */
	public void
store_pos(ifile, scrpos)
	IFILE ifile;
	struct scrpos *scrpos;
{
	int_ifile(ifile)->h_scrpos = *scrpos;
}

/*
 * Recall the file position associated with a file.
 * If no position has been associated with the file, return NULL_POSITION.
 */
	public void
get_pos(ifile, scrpos)
	IFILE ifile;
	struct scrpos *scrpos;
{
	*scrpos = int_ifile(ifile)->h_scrpos;
}

/*
 * Mark the ifile as "opened".
 */
	public void
set_open(ifile)
	IFILE ifile;
{
	int_ifile(ifile)->h_opened = 1;
}

/*
 * Return whether the ifile has been opened previously.
 */
	public int
opened(ifile)
	IFILE ifile;
{
	return (int_ifile(ifile)->h_opened);
}

	public void *
get_filestate(ifile)
	IFILE ifile;
{
	return (int_ifile(ifile)->h_filestate);
}

	public void
set_filestate(ifile, filestate)
	IFILE ifile;
	void *filestate;
{
	int_ifile(ifile)->h_filestate = filestate;
}

#if 0
	public void
if_dump()
{
	register struct ifile *p;

	for (p = anchor.h_next;  p != &anchor;  p = p->h_next)
	{
		printf("%x: %d. <%s> pos %d,%x\n", 
			p, p->h_index, p->h_filename, 
			p->h_scrpos.ln, p->h_scrpos.pos);
		ch_dump(p->h_filestate);
	}
}
#endif

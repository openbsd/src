
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


/******************************************************************************

NAME
   hardscroll.c -- hardware-scrolling optimization for ncurses

SYNOPSIS
   void _nc_scroll_optimize(void)

DESCRIPTION
			OVERVIEW

This algorithm for computes optimum hardware scrolling to transform an
old screen (curscr) into a new screen (newscr) via vertical line moves.

Because the screen has a `grain' (there are insert/delete/scroll line
operations but no insert/delete/scroll column operations), it is efficient
break the update algorithm into two pieces: a first stage that does only line
moves, optimizing the end product of user-invoked insertions, deletions, and
scrolls; and a second phase (corresponding to the present doupdate code in
ncurses) that does only line transformations.

The common case we want hardware scrolling for is to handle line insertions
and deletions in screen-oriented text-editors.  This two-stage approach will
accomplish that at a low computation and code-size cost.

			LINE-MOVE COMPUTATION

Now, to a discussion of the line-move computation.

For expository purposes, consider the screen lines to be represented by
integers 0..23 (with the understanding that the value of 23 may vary).
Let a new line introduced by insertion, scrolling, or at the bottom of
the screen following a line delete be given the index -1.

Assume that the real screen starts with lines 0..23.  Now, we have
the following possible line-oriented operations on the screen:

Insertion: inserts a line at a given screen row, forcing all lines below
to scroll forward.  The last screen line is lost.  For example, an insertion
at line 5 would produce: 0..4 -1 5..23.

Deletion: deletes a line at a given screen row, forcing all lines below
to scroll forward.  The last screen line is made new.  For example, a deletion
at line 7 would produce: 0..6 8..23 -1.

Scroll up: move a range of lines up 1.  The bottom line of the range
becomes new.  For example, scrolling up the region from 9 to 14 will
produce 0..8 10..14 -1 15..23.

Scroll down: move a range of lines down 1.  The top line of the range
becomes new.  For example, scrolling down the region from 12 to 16 will produce
0..11 -1 12..15 17..23.

Now, an obvious property of all these operations is that they preserve the
order of old lines, though not their position in the sequence.  

The key trick of this algorithm is that the original line indices described
above are actually maintained as _line[].oldindex fields in the window
structure, and stick to each line through scroll and insert/delete operations.

Thus, it is possible at update time to look at the oldnum fields and compute
an optimal set of il/dl/scroll operations that will take the real screen 
lines to the virtual screen lines.  Once these vertical moves have been done,
we can hand off to the second stage of the update algorithm, which does line
transformations.

Note that the move computation does not need to have the full generality
of a diff algorithm (which it superficially resembles) because lines cannot
be moved out of order.

			THE ALGORITHM

First, mark each line on the real screen that is *not* carried over to the
virtual screen discarded (that is, with a -1 oldnum index).

Second, optionally run through each virtual line with a non -1 oldnum.  If the
line is sufficiently changed, mark it -1 (we don't want to move it).  The exact
test for "sufficiently changed" is not relevant to the control flow of this
algorithm.  Cases the test should detect are those in which rewriting
the line from whatever might be on the real screen would be cheaper than the
move.  Blank lines on a terminal with clear-to-eol probably meet this test.

Here is pseudo-code for the remainder of the algorithm:

  repeat
1:    first = 0;
2:    no_hunk_moved = TRUE;

      # on each pass, try to find a movable hunk
3:    while (first < screen_depth)

          # scan for start of hunk
4:        while (oldnum field of first == -1)
              first++

          # if we have no hunk, quit this pass
5:        if (first >= screen_depth)
              break;

          # we found a hunk
6:        last = (end of longest continues oldnum range starting here)

7:        ofirst = (first line's oldnum, where it was on real screen)
8:        olast = (last line's oldnum, where it was on real screen)

          # figure the hunk's displacement 
9:        disp = first - (first virtual line's oldnum field)

          # does the hunk want to move?
10:       if (disp != 0)
              # is the hunk movable without destroying info?
11:           if (real [ofirst+disp, olast+disp] are all in range or DISCARDED)
12:               if (disp > 0)
13:                   scroll real [ofirst, olast+disp] down by disp
                      (mark [ofirst, olast+disp] DISCARDED)
14:               else if (disp < 0)
15:                   scroll real [ofirst+disp, olast] up by disp
                      (mark [ofirst+disp, olast] DISCARDED)
16:               no_hunk_moved = FALSE

          # done trying to move this hunk
17:       first = last + 1; 
       end while
    until
18:    no_hunk_moved;    # quit when a complete pass finds no movable hunks

HOW TO TEST THIS:

Use the following production:

hardscroll: hardscroll.c
	$(CC) -g -DMAINDEBUG hardscroll.c -o hardscroll

Then just type scramble vectors and watch.  The following test loads are 
a representative sample of cases:

-----------------------------  CUT HERE ------------------------------------
# No lines moved
 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
#
# A scroll up
 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 -1
#
# A scroll down
-1  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22
#
# An insertion (after line 12)
 0  1  2  3  4  5  6  7  8  9 10 11 12 -1 13 14 15 16 17 18 19 20 21 22
#
# A simple deletion (line 10)
 0  1  2  3  4  5  6  7  8  9  11 12 13 14 15 16 17 18 19 20 21 22 23 -1
#
# A more complex case
-1 -1 -1 -1 -1  3  4  5  6  7  -1 -1  8  9 10 11 12 13 14 15 16 17 -1 -1
-----------------------------  CUT HERE ------------------------------------

AUTHOR
    Eric S. Raymond <esr@snark.thyrsus.com>, November 1994

*****************************************************************************/

#include "curses.priv.h"

#include <stdlib.h>
#include <string.h>

#if defined(TRACE) || defined(MAINDEBUG)
static void linedump(void);
#endif /* defined(TRACE) || defined(MAINDEBUG) */

/* if only this number of lines is carried over, nuke the screen and redraw */
#define CLEAR_THRESHOLD		3

#ifdef MAINDEBUG
#define LINES	24
static int oldnums[LINES], reallines[LINES];
#define OLDNUM(n)	oldnums[n]
#define REAL(m)		reallines[m]
#undef T
#define T(x)		(void) printf x ; (void) putchar('\n');
#else
#include <curses.h>
#define OLDNUM(n)	newscr->_line[n].oldindex
#define REAL(m)		curscr->_line[m].oldindex
#ifndef _NEWINDEX
#define _NEWINDEX	-1
#endif /* _NEWINDEX */
#endif /* MAINDEBUG */

static bool all_discarded(int const top, int const bottom, int const disp)
/* has the given range of real lines been marked discarded? */
{
    int		n;

    for (n = top + disp; n <= bottom + disp; n++)
	if (REAL(n) != _NEWINDEX && !(REAL(n) <= bottom && REAL(n) >= top))
	    return(FALSE);

    return(TRUE);
}

void _nc_scroll_optimize(void)
/* scroll optimization to transform curscr to newscr */
{
    bool no_hunk_moved;		/* no hunk moved on this pass? */
    int	n, new_lines;
#if defined(TRACE) || defined(MAINDEBUG)
    int	pass = 0;
#endif /* defined(TRACE) || defined(MAINDEBUG) */

    TR(TRACE_CALLS, ("_nc_scroll_optimize() begins"));

    /* mark any line not carried over with _NEWINDEX */
    for (n = 0; n < LINES; n++)
	REAL(n) += (MAXLINES + 1);
    for (n = 0; n < LINES; n++)
	if (OLDNUM(n) != _NEWINDEX
	 && REAL(OLDNUM(n)) >= MAXLINES)
	    REAL(OLDNUM(n)) -= (MAXLINES + 1);
    for (n = new_lines = 0; n < LINES; n++)
	if (REAL(n) > MAXLINES)
	{
	    REAL(n) = _NEWINDEX;
	    new_lines++;
	}

    /* 
     * ^F in vi (which scrolls forward by LINES-2 in the file) exposes
     * a weakness in this design.  Ideally, vertical motion
     * optimization should cost its actions and then force a
     * ClrUpdate() and complete redraw if that would be faster than
     * the scroll.  Unfortunately, this would be a serious pain to
     * arrange; hence, this hack.  If there are few enough lines
     * carried over, don't bother with the scrolling, we just nuke the
     * screen and redraw the whole thing.  Keith Bostic argues that
     * this will be a win on strictly visual grounds even if the
     * resulting update is theoretically sub-optimal.  Experience 
     * with vi says he's probably right.
     */
    if (LINES - new_lines <= CLEAR_THRESHOLD)
    {
	T(("too few lines carried over, nuking screen"));
#ifndef MAINDEBUG
	clearok(stdscr, TRUE);
#endif /* MAINDEBUG */
	return;
    }

#ifdef TRACE
    TR(TRACE_UPDATE | TRACE_MOVE, ("After real line marking:"));
    if (_nc_tracing & (TRACE_UPDATE | TRACE_MOVE))
	linedump();
#endif /* TRACE */

    /* time to shuffle lines to do scroll optimization */
    do {
	int	first;		/* first line of current hunk */
	int	last;		/* last line of current hunk */
	int	ofirst;		/* oldnum index of first line */
	int	olast;		/* oldnum index of last line */
	int	disp;		/* hunk displacement */

	TR(TRACE_UPDATE | TRACE_MOVE, ("Pass %d:", pass++));

	first = 0;	       	/* start scan at top line */
	no_hunk_moved = TRUE;

	while (first < LINES)
	{
	    /* find the beginning of a hunk */
	    while (first < LINES && OLDNUM(first) == _NEWINDEX)
		first++;
	    if (first >= LINES)
		break;

	    /* find the end of the hunk */
	    for (last = first; last < LINES; last++)
		if (last == LINES - 1 || OLDNUM(last + 1) != OLDNUM(last) + 1)
		    break;

	    /* find the corresponding range on the old screen */
	    ofirst = OLDNUM(first);
	    olast = OLDNUM(last);

	    /* compute the hunk's displacement */
	    disp = first - OLDNUM(first);

	    TR(TRACE_UPDATE | TRACE_MOVE, ("found hunk: first = %2d, last = %2d, ofirst = %2d, olast = %2d, disp = %2d",
			   first, last, ofirst, olast, disp));

	    /* OK, time to try to move the hunk? */
	    if (disp != 0)
		if (all_discarded(ofirst, olast, disp))
		{
		    int	m;

		    if (disp > 0)
			olast += disp;
		    else /* (disp < 0) */
			ofirst += disp;

		    TR(TRACE_UPDATE | TRACE_MOVE, ("scroll [%d, %d] by %d", ofirst, olast, -disp));
#ifndef MAINDEBUG
		    (void) _nc_mvcur_scrolln(-disp, ofirst, olast, LINES - 1);
		    _nc_scroll_window(curscr, -disp, ofirst, olast);
#endif /* MAINDEBUG */		    

		    for (m = ofirst; m <= olast; m++)
		    {
			REAL(m) = _NEWINDEX;
#ifndef MAINDEBUG
			/*
			 * This will tell the second stage of the optimizer
			 * that every line in the hunk on the real screen has
			 * been changed.
			 */
			curscr->_line[m].firstchar = 0;
			curscr->_line[m].lastchar = curscr->_maxx;
#endif /* MAINDEBUG */		    
		    }
		    for (m = first; m <= last; m++)
			OLDNUM(m) = _NEWINDEX;

		    no_hunk_moved = FALSE;
		}

	    /* OK, done with this hunk */
	    first = last + 1;
	}
    } while
	(!no_hunk_moved);
}

#if defined(TRACE) || defined(MAINDEBUG)
static void linedump(void)
/* dump the state of the real and virtual oldnum fields */
{
    int	n;
    char	buf[BUFSIZ];

    (void) strcpy(buf, "real");
    for (n = 0; n < LINES; n++)
	(void) sprintf(buf + strlen(buf), " %02d", REAL(n)); 
    TR(TRACE_UPDATE | TRACE_MOVE, (buf));

    (void) strcpy(buf, "virt");
    for (n = 0; n < LINES; n++)
	(void) sprintf(buf + strlen(buf), " %02d", OLDNUM(n));
    TR(TRACE_UPDATE | TRACE_MOVE, (buf));
}
#endif /* defined(TRACE) || defined(MAINDEBUG) */

#ifdef MAINDEBUG

main()
{
    char	line[BUFSIZ], *st;

    _nc_tracing = TRACE_MOVE;
    for (;;)
    {
	int	n;

	for (n = 0; n < LINES; n++)
	{
	    reallines[n] = n;
	    oldnums[n] = _NEWINDEX;
	}

	/* grab the test vector */
	if (fgets(line, sizeof(line), stdin) == (char *)NULL)
	    exit(0);

	/* parse it */
	n = 0;
	if (line[0] == '#')
	{
	    (void) fputs(line, stderr);
	    continue;
	}
	st = strtok(line, " ");
	do {
	    oldnums[n++] = atoi(st);
	} while
	    (st = strtok((char *)NULL, " "));

	/* display it */
	(void) fputs("Initial input:\n", stderr);
	linedump();

	_nc_scroll_optimize();	
    }
}

#endif /* MAINDEBUG */

/* hardscroll.c ends here */

